# DS12 — micrófono MEMS I2S de banco (DNP)

**Decisión (2026-11-07):** un micrófono MEMS I2S de 3,3 V, soldado después a mano, conectado a las
tres redes I2S2 que ya existen. Sin patas nuevas del STM32 y sin circuito analógico.

No tiene aplicación directa en la obra. Sirve para probar el STM32 con micrófonos en otros proyectos.

> **2026-11-18:** se suma **un segundo micrófono igual, en el ESP32**, para chequear los parlantes desde
> la central sin tocar el audio del STM32. Ver la sección «El segundo micrófono», al final.

> 🔧 **2026-11-10 — corrección.** La primera versión de este documento proponía tres modelos como si
> fueran equivalentes y hablaba de un «LGA de unos 3,5 × 2,65 mm». Estaba mal:
>
> - **ICS-43434** y **SPH0645LM4H-B** son de **puerto inferior**: escuchan por un agujero del lado de
>   los pads y necesitan un agujero en la placa. Además figuran como discontinuados (TDK marca el
>   ICS-43434 como EOL; DigiKey lista el SPH0645LM4H-B como obsoleto).
> - El único de los tres que escucha por arriba es el **MSM261S4030H0R**, y mide **4 × 3 mm**.
> - Faltaban tres conexiones que pide su hoja: la habilitación, el pull-down de datos y el 10 µF.
>
> Cómo se verificó: la hoja del fabricante del MSM261S4030H0R (MEMSensing, DS-050 v1.0, abril de 2017),
> leída entera; para los otros dos, la página de producto de TDK y la ficha de DigiKey.

## Por qué I2S y no PDM ni analógico

| Opción | Patas nuevas | Circuito extra | Veredicto |
|---|---|---|---|
| **I2S MEMS (elegida)** | 0 (usa I2S2) | ninguno | la más simple |
| PDM (MP34DT06J, etc.) | 0 a 2 | ninguno, pero el firmware tiene que filtrar la señal (DFSDM, SAI-PDM o por software) | más firmware |
| Analógico al ADC interno | 1 (ej. PC0) | acople + polarización | ensucia el esquemático |

Mecanismo: el micrófono I2S ya entrega audio digital de 24 bits. No hay ADC, capacitor de acople ni
polarización de por medio.

## El modelo: MSM261S4030H0R (MEMSensing)

- **Puerto superior** (Ø 0,65 mm, en la tapa metálica): no lleva agujero en la placa.
- Cuerpo de **4,0 × 3,0 mm**, 1,25 mm de alto (la tabla de la hoja; el texto dice 1,0).
- Alimentación de 1,6 a 3,6 V (máximo absoluto 4,0 V): va con `+3V3`.
- Reloj SCK de **1,0 a 4,0 MHz** en modo normal. Con 64 bits por trama, eso es una frecuencia de
  muestreo de 15,6 a 62,5 kHz. A 48 kHz son 3,072 MHz, el valor nominal de la hoja.
- Relación señal-ruido 61 dB; sensibilidad −26 dBFS a 94 dB SPL.
- **Stock no verificado.** Como va DNP y se suelda a mano, no hace falta que esté en LCSC.

### Las ocho patas

| Pata | Nombre | A dónde va |
|---|---|---|
| 1 | GND | GND |
| 2 | N/C | **sin conectar** |
| 3 | WS | `/stm32_custom/I2S2_WS` (PB12) |
| 4 | CHIPEN | **`+3V3`**. En bajo el micrófono queda apagado |
| 5 | L/R | GND (canal izquierdo). A `+3V3` sale por el derecho |
| 6 | SCK | `/stm32_custom/I2S2_CK` (PB13) |
| 7 | SD | `/stm32_custom/I2S2_SDI` (PB14), con **100 kΩ a GND** |
| 8 | VDD | `+3V3`, con **100 nF + 10 µF** pegados a la pata |

Por qué el 100 kΩ: el micrófono maneja la línea de datos sólo durante la mitad de la trama que le
toca (izquierda o derecha). La otra mitad suelta la línea, y sin la resistencia queda flotando.

### La huella

Ocho pads de **0,9 × 0,6 mm**, en dos columnas de cuatro:

- paso vertical de 1,0 mm (0,6 de pad + 0,4 de separación);
- 1,0 mm libre entre las dos columnas.

Numeración vista desde abajo: 1–4 en la columna izquierda, de abajo hacia arriba, y 5–8 en la
derecha, de arriba hacia abajo. **Copiar la huella del plano de la hoja**, no de este texto.

## Qué dibujar

- El micrófono, **DNP** en el BOM.
- Las tres piezas que sí se ensamblan, porque sin el micrófono no molestan:
  - 100 nF (`C1525`) y 10 µF (`C1691`) entre la pata 8 y GND;
  - 100 kΩ (`C25741`) de la pata 7 a GND.
- Las redes I2S2 ya existen y van también a J6.1 (WS), J6.2 (CK) y J5.5 (SDI). **Reconfirmado el
  2026-11-10** contra el esquemático guardado el 28-oct, con `tools/sch2net.py`.
- Para estéreo: un segundo micrófono igual, con la pata 5 a `+3V3`, sobre las mismas tres redes.

## Ubicación (layout)

- Lejos de los PAM8302A, del inductor del buck y de los cables de parlantes.
- Con lugar alrededor para soldar con pasta y aire caliente.

## Al soldarlo (de la hoja)

- **No lavar la placa** ni pasarle cepillo o solvente después de soldar.
- No soplar aire a presión ni meter nada en el agujero de la tapa.
- Tres pasadas de horno o aire caliente como mucho.
- Es sensible a la humedad (MSL 2a): dejarlo en su bolsa hasta usarlo.

## Limitación

Las líneas I2S2 también salen a J5/J6. Con el micrófono soldado, **no conectar ahí otro dispositivo
I2S** a la vez: quedarían dos fuentes sobre la misma línea de datos.

## Firmware

**El firmware no usa I2S2** (verificado el 2026-11-10): no hay ninguna referencia a SPI2 ni a I2S2
en `main.c` ni en `stm32h7xx_hal_msp.c`. Para usar el micrófono hay que:

- habilitar el módulo I2S del HAL (`HAL_I2S_MODULE_ENABLED` está comentado en
  `stm32h7xx_hal_conf.h`) y sumar su fuente al `Makefile`;
- configurar I2S2 como **maestro receptor**: el micrófono es esclavo y necesita que el STM32 le dé
  SCK y WS.

Con el 100 kΩ puesto no hace falta el pull-down interno de PB14.

## Patas libres del STM32 (esquemático guardado el 2026-10-28)

Libres: PC0, PC2_C, PC3_C, PB2, PE9, PE10, PE14, PB15, PD14, PD15, PC6, PA9, PD5, PD6, PB4, PE0, PE1.
PC13 también está al aire, pero **no se puede conmutar** (errata ES0392 §2.2.30: corrompe el LSE).
PE12 y PE13 quedan para los LEDs de J9. Las funciones alternativas de cada pata **no se
verificaron**: chequear en CubeMX o en la hoja de datos del H743 antes de fijar pines.

## Otros periféricos considerados

- **Sensores I2C (IMU, temperatura, luz):** ya hay I2C2 con pull-ups (RN6) en J5.1/J5.2. Alcanza con
  un conector estándar.
- **Conector de 6–8 GPIO libres:** lo más barato para dejar la placa abierta.
- **microSD: descartada.** SDMMC1 usa PC8–PC12 y PD2; PC8–PC12 los ocupa el AK5538.

## El segundo micrófono: en el ESP32 (decidido el 2026-11-18)

**Para qué.** Un oído que no depende del STM32. El ESP32 lo lee, lo analiza y lo publica por WiFi,
sin tocar los buses de audio del STM32 (SAI y el ADC de J9). Sirve para un chequeo de los parlantes
desde la central: el STM32 emite un tono por canal y el ESP32 dice si llegó y con cuánto nivel. También
sirve para escuchar la sala de forma remota.

**Por qué el mismo modelo.** Una sola huella y una sola línea en el BOM. El ESP32-C3 lo lee por I2S
estándar, y el ESP32 es el maestro: genera SCK y WS. Las alternativas no sirven:

- **PDM: no.** Según la documentación de ESP-IDF el C3 no tiene PDM de entrada, sólo de salida
  ⚠️ (no se verificó contra la hoja del chip).
- **Analógico: no.** El ADC del C3 es ruidoso y el ADC2 comparte recursos con el WiFi.

**Las patas del ESP32.** Elegidas sobre el netlist derivado del esquemático del commit `b31521d`
con `tools/sch2net.py`:

| Micrófono | ESP32 (U5) | Por qué esa |
|---|---|---|
| 6 SCK | **GPIO6** (pata 20) | hoy va sólo a J1.3 |
| 3 WS | **GPIO7** (pata 21) | hoy va sólo a J1.4 |
| 7 SD | **GPIO3** (pata 6), con **100 kΩ a GND** | hoy al aire; no es de arranque |
| 4 CHIPEN, 8 VDD | `+3V3`, con **100 nF + 10 µF** | igual que el del STM32 |
| 5 L/R, 1 GND | GND | canal izquierdo |
| 2 | sin conectar | |

- **GPIO2 y GPIO8 no se usan:** están libres, pero son de arranque. Con el 100 kΩ a GND de SD pueden
  cambiar cómo arranca el chip.
- **GPIO6 y GPIO7 son dos patas del JTAG externo de J1** (J1 es un JST-XH de 4 pines con GPIO4–7).
  Con el micrófono montado no se puede usar ese JTAG. Se pierde poco: el C3 también se depura por su
  USB (GPIO18/19), que no cambia. Sin el micrófono, J1 queda como está.
- El firmware del ESP32 no usa GPIO3, 6 ni 7 (verificado: sólo usa GPIO0, 1 y 10).

**Qué dibujar.** Un segundo MSM261S4030H0R, **DNP**, con sus tres piezas, que sí se ensamblan: 100 nF
(`C1525`), 10 µF (`C1691`) y 100 kΩ (`C25741`). Son las mismas del micrófono del STM32 y el
costo es de centavos. Las tres redes son nuevas: `ESP_MIC_SCK`, `ESP_MIC_WS`, `ESP_MIC_SD`.
Ubicarlo lejos de los PAM y del buck. **Ojo:** mirá dónde queda la placa respecto de los conos. Si
queda detrás del panel, el micrófono va a oír la caja más que a los parlantes.

**El firmware (sin escribir todavía).**

- Driver `i2s_std` de ESP-IDF, modo Philips, mono izquierdo, palabras de 32 bits, a **24 kHz**. Eso da
  SCK = 1,536 MHz, dentro del rango de 1–4 MHz de la hoja.
- Cálculo en enteros, porque el C3 no tiene FPU: nivel RMS y Goertzel para los tonos de prueba.
- `GET /mic/nivel`: el nivel en vivo.
- `GET /mic/escuchar`: un WAV de 16 kHz (unos 32 kB/s) para oír desde la central.
- `GET /mic/chequeo`:
  - **primero mide el piso con silencio** (la regla del repo: un umbral sin esa medida es una opinión);
  - después pide al STM32, por UART, un tono por canal, de a uno;
  - devuelve los dB sobre el piso y un veredicto por canal;
  - **no enciende amplificadores**: eso lo sigue decidiendo una persona;
  - si el STM32 no tiene un comando de «tono sólo en el canal k», hay que agregarlo (no se revisó).

**Límites.**

- Con la obra en beamforming, el chequeo por parlante necesita tonos de prueba, o sea, interrumpe la obra.
  Sin interrumpirla, el micrófono sólo puede decir «hay sonido o no» y mostrar el espectro.
- Este micrófono no entra al ADC del STM32, así que **no crea el acople** de los modos 0, 2–6 y 15. Tiene
  que seguir así: el audio del ESP32 no se manda al camino de los parlantes.

## Fuentes

- [Hoja del MSM261S4030H0R](https://www.waveshare.com/w/upload/0/01/MSM261S4030H0R.pdf) (MEMSensing DS-050)
- [TDK, ICS-43434](https://invensense.tdk.com/en-us/products/ics-43434)
- [DigiKey, SPH0645LM4H-B](https://www.digikey.com/en/products/detail/syntiant/SPH0645LM4H-B/5332440)
