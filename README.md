# Line Follower con STM32

Robot seguidor de línea con una matriz analógica de 8 sensores y dos motores DC. El firmware calibra los sensores al encender y luego usa un controlador PID para mantener el robot sobre la línea.

## Hardware

- Placa STM32 compatible con STM32duino. El código está preparado principalmente para **WeAct STM32H503CB** y también contempla **Blackpill STM32F401CC** y **STM32F411CE**.
- Arreglo de 8 sensores analógicos de línea, por ejemplo QTR-8A o módulos TCRT5000 con salidas analógicas.
- Dos motores DC y un driver de motores con entradas de dirección y PWM.
- Batería y chasis del robot.

El chasis está basado en este modelo: [Line Follower en MakerWorld](https://makerworld.com/en/models/1547362-line-follower?from=search#profileId-1624897).

## Conexiones

| Función | Pines STM32 |
| --- | --- |
| Sensores S0–S7, de izquierda a derecha | PA0–PA7 |
| Motor A (izquierdo), IN1 / IN2 / PWM | PB0 / PB1 / PB7 |
| Motor B (derecho), IN1 / IN2 / PWM | PB2 / PB10 / PB8 |
| LED integrado | PC13 |

Conectá las salidas del driver a los motores y sus entradas a los pines indicados. Alimentá los motores desde una fuente adecuada y asegurá una **masa común** entre la placa, el driver y los sensores. No alimentes los motores desde los pines de la placa.

### Esquema

![Esquema de conexiones del robot](Schematic%20Picture.png)

El diagrama editable está en [Schematics.fzz](Schematics.fzz) (formato Fritzing). La imagen muestra también la alimentación propuesta para la batería; verificá la tensión y polaridad de tus componentes antes de conectar la placa.

## Código

- `main_code/SeguidorLineas/SeguidorLineas.ino`: firmware principal con calibración y control PID.
- `wiring_test/PruebaMotores.ino`: prueba de motores y lectura de sensores para verificar el cableado.
- `Schematic Picture.png` y `Schematics.fzz`: esquema exportado e editable.

Los directorios `build/` contienen binarios y mapas generados al compilar; no hace falta subirlos porque se pueden volver a generar.

## Compilar y cargar

1. Instalá Arduino IDE y el core **STM32 MCU based boards (STM32duino)**.
2. Abrí `main_code/SeguidorLineas/SeguidorLineas.ino`.
3. Seleccioná la placa STM32 correspondiente, el método de carga y el puerto en **Herramientas**.
4. Compilá y cargá el programa.

El firmware usa lecturas analógicas de 12 bits y PWM de 8 bits. En el STM32H503CB, PB8 genera PWM por software; las demás salidas PWM usan `analogWrite`.

## Calibración y puesta en marcha

1. Colocá el robot de modo que, durante el encendido, los sensores puedan pasar tanto sobre la superficie clara como sobre la línea oscura.
2. Durante los primeros **5 segundos**, el LED parpadea mientras se calibran los sensores. Deslizá el robot de lado a lado para que cada sensor detecte ambos colores.
3. Cuando el LED queda encendido, ubicá el robot centrado sobre la línea. Después de **2 segundos**, empieza a moverse.
4. Si la lectura del sensor es mayor sobre la línea, dejá `LINEA_DA_VALOR_ALTO = true`; si es menor, cambialo a `false`.
5. Si un motor gira en sentido contrario, cambiá el valor correspondiente de `INVERTIR_MOTOR_A` o `INVERTIR_MOTOR_B`.

## Ajustes

Los parámetros principales están al comienzo del sketch:

- `VEL_BASE`, `VEL_MAX` y `VEL_BUSQUEDA`: velocidad de marcha, límite y búsqueda cuando se pierde la línea.
- `KP`, `KI` y `KD`: constantes del controlador PID. Ajustalas gradualmente según el comportamiento del robot.
- `CALIB_MS`: duración de la calibración.
- `DEBUG_SERIAL`: cambialo a `1` para enviar lecturas, posición y velocidades por USB Serial a 115200 baudios.

## Licencia

No se especificó una licencia para este proyecto. Si querés permitir que otras personas reutilicen o modifiquen el código, agregá un archivo `LICENSE` con la licencia que elijas.


