/*
 * Prueba de hardware - STM32H503CB
 * Ciclo de 8 s, repetido:
 *   0-2 s  Motor A adelante (100%)
 *   2-4 s  Motor A atras    (100%)
 *   4-6 s  Motor B adelante (100%)
 *   6-8 s  Motor B atras    (100%)
 * Los pines PWM (PB7, PB8) se ponen en ALTO fijo: sin PWM, para descartarlo.
 * Por USB Serial (CDC) imprime la fase actual y los 8 sensores crudos.
 */

const uint8_t pinesSensor[8] = {PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7};
const uint8_t A_IN1 = PB0, A_IN2 = PB1, A_PWM = PB7;
const uint8_t B_IN1 = PB2, B_IN2 = PB10, B_PWM = PB8;
const uint8_t LED = PC13;

void setPins(int a1, int a2, int b1, int b2) {
  digitalWrite(A_IN1, a1); digitalWrite(A_IN2, a2);
  digitalWrite(B_IN1, b1); digitalWrite(B_IN2, b2);
}

void setup() {
  uint8_t outs[] = {A_IN1, A_IN2, A_PWM, B_IN1, B_IN2, B_PWM, LED};
  for (uint8_t p : outs) pinMode(p, OUTPUT);
  digitalWrite(A_PWM, HIGH);
  digitalWrite(B_PWM, HIGH);
  for (int i = 0; i < 8; i++) pinMode(pinesSensor[i], INPUT_ANALOG);
  analogReadResolution(12);
  Serial.begin(115200);
}

void loop() {
  uint32_t fase = (millis() / 2000) % 4;
  const char *nombre;
  switch (fase) {
    case 0: setPins(HIGH, LOW, LOW, LOW);  nombre = "A adelante"; break;
    case 1: setPins(LOW, HIGH, LOW, LOW);  nombre = "A atras";    break;
    case 2: setPins(LOW, LOW, HIGH, LOW);  nombre = "B adelante"; break;
    default: setPins(LOW, LOW, LOW, HIGH); nombre = "B atras";    break;
  }
  digitalWrite(LED, (millis() / 250) % 2);

  static uint32_t t = 0;
  if (millis() - t >= 250) {
    t = millis();
    Serial.print(nombre); Serial.print(" | S:");
    for (int i = 0; i < 8; i++) { Serial.print(' '); Serial.print(analogRead(pinesSensor[i])); }
    Serial.println();
  }
}
