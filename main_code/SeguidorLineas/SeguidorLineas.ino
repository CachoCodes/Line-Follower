/*
 * Seguidor de lineas - WeAct STM32H503CB (tambien compila para Blackpill F401/F411)
 * Framework: Arduino (STM32duino, core "STM32 MCU based boards")
 *
 * Conexiones:
 *   Sensor de linea (8 canales analogicos): PA0..PA7  (S0 = PA0 izquierda ... S7 = PA7 derecha)
 *   Motor A (izquierdo):  IN1 = PB0, IN2 = PB1,  PWM = PB7  (en H503: TIM1_CH1)
 *   Motor B (derecho):    IN1 = PB2, IN2 = PB10, PWM = PB8  (en H503: PWM por software con TIM3)
 *   LED de la placa: PC13 (activo en bajo) - indica calibracion
 *
 * Funcionamiento:
 *   1. Al encender, el LED parpadea rapido durante 5 s: desliza el robot
 *      de lado a lado sobre la linea para que todos los sensores vean
 *      blanco y negro (calibracion).
 *   2. LED fijo 2 s -> pon el robot centrado sobre la linea.
 *   3. Arranca el control PID.
 */

// ===================== CONFIGURACION =====================
// Ajusta estos valores segun tu robot.

#define NUM_SENSORES 8

// true  -> el sensor da valor ALTO sobre la linea (ej. QTR-8A sobre linea negra)
// false -> el sensor da valor BAJO sobre la linea (muchos modulos TCRT5000)
const bool LINEA_DA_VALOR_ALTO = true;

// Si un motor gira al reves, cambia su valor a true
const bool INVERTIR_MOTOR_A = true;
const bool INVERTIR_MOTOR_B = true;

// Velocidades (0..255)
const int VEL_BASE = 75;      // velocidad en recta
const int VEL_MAX  = 140;     // limite de cada motor
const int VEL_BUSQUEDA = 70;  // giro cuando se pierde la linea

// Constantes PID (empieza con KD = 0 y KI = 0, sube KP hasta que oscile, luego sube KD)
float KP = 0.03f;
float KI = 0.0f;
float KD = 0.3f;

const int CALIB_MS = 5000;       // duracion de la calibracion
const int UMBRAL_LINEA = 200;    // valor normalizado (0..1000) para considerar "veo linea"

#define DEBUG_SERIAL 0           // pon 1 para ver datos por USB Serial

// ===================== PINES =====================
const uint8_t pinesSensor[NUM_SENSORES] = {PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7};

const uint8_t A_IN1 = PB0;
const uint8_t A_IN2 = PB1;
const uint8_t A_PWM = PB7;

const uint8_t B_IN1 = PB2;
const uint8_t B_IN2 = PB10;
const uint8_t B_PWM = PB8;

const uint8_t LED = PC13;

// ===================== VARIABLES =====================
uint16_t minimo[NUM_SENSORES];
uint16_t maximo[NUM_SENSORES];
uint16_t valor[NUM_SENSORES];   // normalizado 0..1000 (1000 = linea)

const int32_t CENTRO = (NUM_SENSORES - 1) * 1000 / 2;  // 3500
int32_t ultimaPosicion = CENTRO;
float errorAnterior = 0;
float integral = 0;

// ===================== PWM POR SOFTWARE (STM32H503) =====================
// En el STM32H503 el pin PB8 no tiene timer de PWM, asi que se genera
// con interrupciones de TIM3 a 20 kHz. PB7 usa PWM por hardware (TIM1_CH1).
#if defined(STM32H5xx)
#define PWM_B_SOFTWARE 1
HardwareTimer *timerPwmB = nullptr;
volatile uint8_t dutyB = 0;

void pwmB_inicio() {            // inicio de periodo -> PB8 en alto
  if (dutyB > 0) digitalWriteFast(PB_8, HIGH);
}
void pwmB_fin() {               // fin del ciclo util -> PB8 en bajo
  if (dutyB < 255) digitalWriteFast(PB_8, LOW);
}

void pwmB_setup() {
  timerPwmB = new HardwareTimer(TIM3);
  timerPwmB->setOverflow(20000, HERTZ_FORMAT);
  timerPwmB->setMode(1, TIMER_OUTPUT_COMPARE, NC);
  timerPwmB->setCaptureCompare(1, 0, RESOLUTION_8B_COMPARE_FORMAT);
  timerPwmB->attachInterrupt(pwmB_inicio);
  timerPwmB->attachInterrupt(1, pwmB_fin);
  timerPwmB->resume();
}

void pwmB_write(uint8_t d) {
  dutyB = d;
  timerPwmB->setCaptureCompare(1, d, RESOLUTION_8B_COMPARE_FORMAT);
  if (d == 0)   digitalWriteFast(PB_8, LOW);
  if (d == 255) digitalWriteFast(PB_8, HIGH);
}
#else
#define PWM_B_SOFTWARE 0
#endif

// ===================== MOTORES =====================
void escribirPWM(uint8_t pin, int duty) {
#if PWM_B_SOFTWARE
  if (pin == B_PWM) { pwmB_write(duty); return; }
#endif
  analogWrite(pin, duty);
}

void moverMotor(uint8_t in1, uint8_t in2, uint8_t pwm, int vel, bool invertir) {
  if (invertir) vel = -vel;
  vel = constrain(vel, -255, 255);
  if (vel > 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } else if (vel < 0) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  } else {
    digitalWrite(in1, LOW);   // motor libre
    digitalWrite(in2, LOW);
  }
  escribirPWM(pwm, abs(vel));
}

void motores(int velA, int velB) {
  moverMotor(A_IN1, A_IN2, A_PWM, velA, INVERTIR_MOTOR_A);
  moverMotor(B_IN1, B_IN2, B_PWM, velB, INVERTIR_MOTOR_B);
}

// ===================== SENSORES =====================
void leerCrudo(uint16_t *crudo) {
  for (int i = 0; i < NUM_SENSORES; i++) {
    crudo[i] = analogRead(pinesSensor[i]);
  }
}

void calibrar() {
  for (int i = 0; i < NUM_SENSORES; i++) {
    minimo[i] = 4095;
    maximo[i] = 0;
  }
  uint16_t crudo[NUM_SENSORES];
  uint32_t inicio = millis();
  while (millis() - inicio < (uint32_t)CALIB_MS) {
    leerCrudo(crudo);
    for (int i = 0; i < NUM_SENSORES; i++) {
      if (crudo[i] < minimo[i]) minimo[i] = crudo[i];
      if (crudo[i] > maximo[i]) maximo[i] = crudo[i];
    }
    digitalWrite(LED, (millis() / 100) % 2);  // parpadeo rapido
    delay(2);
  }
}

// Devuelve la posicion de la linea: 0 (S0) .. 7000 (S7). Centro = 3500.
// Pone 'veoLinea' en false si ningun sensor detecta la linea.
int32_t leerPosicion(bool &veoLinea) {
  uint16_t crudo[NUM_SENSORES];
  leerCrudo(crudo);

  uint32_t suma = 0, sumaPonderada = 0;
  veoLinea = false;

  for (int i = 0; i < NUM_SENSORES; i++) {
    int32_t rango = (int32_t)maximo[i] - minimo[i];
    int32_t v = 0;
    if (rango > 50) {
      v = ((int32_t)crudo[i] - minimo[i]) * 1000 / rango;
      v = constrain(v, 0, 1000);
    }
    if (!LINEA_DA_VALOR_ALTO) v = 1000 - v;
    valor[i] = v;

    if (v > UMBRAL_LINEA) veoLinea = true;
    if (v > 50) {                       // ignora ruido
      sumaPonderada += (uint32_t)v * (i * 1000);
      suma += v;
    }
  }

  if (!veoLinea || suma == 0) {
    return ultimaPosicion;
  }
  ultimaPosicion = sumaPonderada / suma;
  return ultimaPosicion;
}

// ===================== SETUP =====================
void setup() {
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);  // apagado

  pinMode(A_IN1, OUTPUT); pinMode(A_IN2, OUTPUT);
  pinMode(B_IN1, OUTPUT); pinMode(B_IN2, OUTPUT);
  pinMode(A_PWM, OUTPUT); pinMode(B_PWM, OUTPUT);

  for (int i = 0; i < NUM_SENSORES; i++) pinMode(pinesSensor[i], INPUT_ANALOG);

  analogReadResolution(12);     // 0..4095
  analogWriteResolution(8);     // 0..255
  analogWriteFrequency(20000);  // 20 kHz: motores silenciosos
#if PWM_B_SOFTWARE
  pwmB_setup();
#endif

  motores(0, 0);

#if DEBUG_SERIAL
  Serial.begin(115200);
#endif

  delay(500);
  calibrar();

  digitalWrite(LED, LOW);   // LED fijo: coloca el robot sobre la linea
  delay(2000);
  digitalWrite(LED, HIGH);
}

// ===================== LOOP =====================
void loop() {
  bool veoLinea;
  int32_t posicion = leerPosicion(veoLinea);

  if (!veoLinea) {
    // Linea perdida: girar hacia el lado donde se vio por ultima vez
    integral = 0;
    if (ultimaPosicion < CENTRO) motores(-VEL_BUSQUEDA, VEL_BUSQUEDA);  // linea a la izquierda
    else                         motores(VEL_BUSQUEDA, -VEL_BUSQUEDA);  // linea a la derecha
    return;
  }

  // error > 0 -> linea a la derecha
  float error = (float)(posicion - CENTRO);
  integral = constrain(integral + error, -50000.0f, 50000.0f);
  float derivada = error - errorAnterior;
  errorAnterior = error;

  float correccion = KP * error + KI * integral + KD * derivada;

  int velA = VEL_BASE + (int)correccion;  // motor A = izquierdo
  int velB = VEL_BASE - (int)correccion;  // motor B = derecho
  velA = constrain(velA, -VEL_MAX, VEL_MAX);
  velB = constrain(velB, -VEL_MAX, VEL_MAX);

  motores(velA, velB);

#if DEBUG_SERIAL
  static uint32_t t = 0;
  if (millis() - t > 100) {
    t = millis();
    for (int i = 0; i < NUM_SENSORES; i++) { Serial.print(valor[i]); Serial.print('\t'); }
    Serial.print("pos="); Serial.print(posicion);
    Serial.print(" A="); Serial.print(velA);
    Serial.print(" B="); Serial.println(velB);
  }
#endif

  delay(2);  // ~500 Hz de lazo de control
}
