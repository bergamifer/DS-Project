# DS12 — cámara DVP/MIPI como segunda utilidad (idea archivada)

**Estado (2026-11-20): NO se hace en la DS12.** Se investigó, se concluyó que con el STM32H743 en
LQFP100 no entra sin romper el audio, y se deja anotado para una revisión futura (DS13 o una placa
hija). Por ahora tampoco interesa una pantalla en la placa.

Pregunta original: ¿se puede dejar en la DS12 una interfaz de cámara (DVP o MIPI), como DNP, para darle
una segunda vida como placa de desarrollo? La placa WeAct que tiene display trae un ejemplo en su
esquemático de cómo se conecta.

## Veredicto en una línea

- **MIPI CSI-2: imposible** con este chip. **DVP (DCMI): posible en teoría, inviable en la DS12 tal
  como está**, porque necesita 4 de sus 8 pines de datos justo donde la DS12 tiene audio.

## MIPI CSI-2: el STM32H743 no tiene el periférico

El H743 sólo trae **DCMI**, que es una interfaz paralela. El receptor MIPI CSI-2 existe en otras
familias de STM32 (la N6, por ejemplo), no en ésta. Para MIPI haría falta un chip puente
MIPI→paralelo: más plata, más placa y más riesgo. Descartado.

## DVP por DCMI: lo que pide

Una cámara tipo OV2640 / OV5640 necesita:

| Señal | Cantidad |
|---|---|
| Datos D0–D7 | 8 |
| PCLK, HSYNC, VSYNC | 3 |
| XCLK (reloj que el STM32 le da a la cámara, por un pin MCO) | 1 |
| SCCB (es un I²C: SCL/SDA) | 2 |
| PWDN y RESET | 2 |

Total: **unos 16 pines**. El mecanismo que complica todo: DCMI sólo existe en **pines fijos** (función
alternativa AF13). No se puede elegir cuáles usar.

## El cruce contra la DS12 (hecho el 2026-11-20)

**Cómo se verificó:** la tabla de funciones alternativas del datasheet del H743 (`datasheets/
stm32h743vi.pdf`, versión LQFP100), contra el uso de pines de `docs/pin_map.md` y la lista de patas
libres de [DS12_MICROFONO_DNP.md](DS12_MICROFONO_DNP.md) (esquemático guardado el 2026-10-28).
`pin_map.md` es de la DS10/DS11: el uso de PB6–PB9, PE4–PE6, PD3 y PC11 se mantiene en la DS12 según la
documentación existente, **pero no se corrió el netlist de la DS12 pata por pata**. Antes de decidir
algo en serio, repetirlo con `tools/netlist_digest.py`.

Opciones de pin DCMI en LQFP100 y lo que hay ahí hoy:

| Señal | Pines posibles | Quién los usa hoy | ¿Libre? |
|---|---|---|---|
| D0 | PA9, PC6 | libres | ✅ |
| D1 | PA10, PC7 | PA10 «sin usar» en DS10/11 (no figura en la lista de libres de la DS12) | ⚠️ |
| D2 | PE0, PC8 | PE0 libre; PC8 es del AK5538 | ✅ (PE0) |
| D3 | PE1, PC9 | PE1 libre; PC9 es del AK5538 | ✅ (PE1) |
| **D4** | PE4, PC11 | PE4 = SAI1_FS_A (LRCK de los PCM1690); PC11 = DAC_RST | ❌ |
| **D5** | PB6, PD3 | PB6 = I²C4 (EEPROM); PD3 = PAM_SD (apaga los 32 amplificadores) | ❌ |
| **D6** | PB8, PE5 | PB8 = I²C1 (bus de los DAC); PE5 = SAI1_SCK_A (BCK) | ❌ |
| **D7** | PB9, PE6 | PB9 = I²C1 (bus de los DAC); PE6 = SAI1_SD_A (datos) | ❌ |
| HSYNC | PA4 | botón K2 en DS10/11 | ⚠️ |
| PIXCLK | PA6 | LED D4 en DS10/11 | ⚠️ |
| VSYNC | PB7 | I²C4 (EEPROM) | ❌ |

(En LQFP100 el resto de las opciones de HSYNC/VSYNC —PH8, PG9, PI5— no existen.)

**Conclusión del cruce:** de los 8 bits de datos, sólo **D0 a D3** tienen pines libres. D4 a D7 caen
encima del reloj y los datos del audio, del bus de los DAC, de la EEPROM y del shutdown de los
amplificadores. DCMI funciona en 8, 10, 12 o 14 bits, **no en 4**, así que no hay versión reducida.

## Por qué tampoco conviene como «DNP gratis»

1. **DNP no libera pines ni ruteo.** El conector y la cámara pueden quedar sin soldar, pero las pistas y
   las patas ya están comprometidas.
2. **Ruido.** PCLK de 24 a 48 MHz con 8 bits paralelos, al lado de 32 amplificadores clase D y de un
   ADC de bajo ruido (AK5538).
3. **Riesgo para la obra.** La DS12 es una tirada de 10 unidades que tiene que funcionar sola durante
   una muestra. La cámara es un objetivo secundario que compite por lo escaso con el objetivo principal.

## Caminos si algún día se quiere

| Opción | Qué implica | Costo |
|---|---|---|
| **A. Placa hija** (recomendada) | Un conector de expansión DNP (2×10) con los GPIO libres, I²C y 3V3. La cámara vive en otra placa, con su propio ruteo | barato, sin riesgo para el audio |
| **B. H743 en LQFP176** | Más pines (PH8–PH12, PI4–PI7, PG9, etc.), y el plan ya contempla LQFP176 si hace falta USB HS por ULPI. Con el cambio de paquete, rehacer el cruce | alto: es otra placa |
| **C. Puente MIPI→paralelo** | Sólo si se quisiera una cámara MIPI; se suma a B | alto y sin sentido para la obra |
| **D. Cámara por el ESP32** | Un ESP32-S3 (tiene interfaz de cámara) en otro módulo, hablando con la DS12 por UART/WiFi | medio; evita tocar el STM32 |

La opción D es la más coherente con cómo está armada la obra: el STM32 hace audio, y la cámara es
otro nodo.

## Si se retoma

1. Correr `tools/netlist_digest.py` sobre el esquemático vigente y repetir la tabla de arriba.
2. Decidir el destino: placa hija (A) o nodo aparte (D). B y C sólo con una razón fuerte.
3. Para ver cómo se conecta el display de la WeAct: **no se miró su esquemático** en esta
   investigación; es un buen punto de partida para el conector (tensiones, pull-ups, XCLK).
4. XCLK por MCO: MCO1 es PA8 y MCO2 es PC9 (PC9 es del AK5538); **no se verificó** si PA8 está libre.

## Fuentes

- Datasheet del STM32H743VI, tabla de funciones alternativas, AF13 (`datasheets/stm32h743vi.pdf`).
- `docs/pin_map.md` (uso de pines DS10/DS11) y [DS12_MICROFONO_DNP.md](DS12_MICROFONO_DNP.md) (patas libres de la DS12).
