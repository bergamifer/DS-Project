# DS12 — Plan de firmware

> 2026-10-08. Todo lo que el firmware tiene que cambiar para la DS12, en el orden en que conviene hacerlo.
> Sale del esquemático cerrado el 8-oct (commit `c37e9f9`), de la revisión de alimentación
> ([`hardware/DS/DS12_REVISION_ALIMENTACION.md`](../hardware/DS/DS12_REVISION_ALIMENTACION.md) §6.1 y §9.2) y de
> [`hardware/DS/DS12_USB_STM32.md`](../hardware/DS/DS12_USB_STM32.md).
> **Es un plan: no hay nada implementado.** Lo lee Fernando, así que cada ítem dice el porqué, no sólo el qué.

## 0. Reglas que no se negocian

- **Las tres DS11 que andan no se rompen.** Todo cambio entra primero con `BOARD=ds11` por defecto. La prueba es
  mecánica: después de la fase 1, el `.bin` de la DS11 tiene que ser **idéntico** al de antes, salvo `fw_id` y `build`.
- **Una sola base, dos placas.** Nada de `#ifdef DS12` sueltos: el código pregunta por **capacidades**
  (`BOARD_HAS_5VA_SENSE`, …). En la DS11 esos pines están al aire: si una capacidad de la DS12 se prende en el binario
  de la DS11, la DS11 lee basura y apaga los amplificadores.
- **Encender amplificadores es una decisión de una persona.** Ninguna prueba de este plan los enciende sola, y el
  autoarranque se prueba con los parlantes desenchufados.
- Lo del CLAUDE.md:
  - la app vive en `0x08020000`;
  - los buffers de DMA van en `RAM_D2`;
  - `nano.specs` no imprime `%f`;
  - **si un test pasa raro, borrar `build/main.o` y recompilar**, porque `make` ya dio un falso OK.

## 1. Inventario

| # | qué | por qué | pines | cuándo tiene que estar |
|---|---|---|---|---|
| **F0** | Partir `pin_definitions.h` por placa; `make BOARD=ds11/ds12` | todo lo demás se apoya en esto | — | fase 1 |
| **F1** | Bloque de identidad en la imagen; `/status` dice `"board"`; chequeos en `make ota`, el ESP32 y `OTA_End` | que no se pueda subir el firmware de una placa a la otra | — | fase 2 |
| **F2** | Leer el jumper de identidad al arrancar; si no coincide, modo seguro | protege contra flashear el equivocado por ST-Link | PE15 (a GND por JP3 y R36 en la DS12, al aire en la DS11) | fase 2 |
| **F3** | **Lectura de `+5VA` con watchdog analógico** sobre PC1 → SD en bajo en microsegundos | S2 y S10: los 32 SD no pueden quedar arriba con `+5VA` en 0; con señal el riel cae en 0,78 ms | PC1 = `+5VA`/2 (R163/R162/C312) | **antes de la primera DS12 por USB** |
| **F4** | **Guarda de SD** en los cuatro caminos (autoarranque, `POST /amps`, K2 largo, `POST /autostart`); el autoarranque **espera** en AS_WAIT | S2 | PD3 (SD), PC1 | **antes de la primera DS12 por USB** |
| **F5** | Secuencia de recuperación cuando vuelve `+5VA`: reset de los PCM1690 y del AK5538, reconfiguración, y recién después SD | AL §6.1.b: el riel analógico hace un ciclo completo mientras el digital sigue vivo | PA10 (PG), PC11 (`~RST`), PC7 (`PDN_AK`) | primera DS12 |
| **F6** | Autoarranque con **marca propia** en `RTC->BKP3R`; un reset SFT (OTA o reflasheo) no cuenta como falla | N6. Además, con errores esporádicos la ventana de 60 s se reinicia sin fin | — | fase 2 (sirve también en la DS11) |
| **F7** | PA5 como **detector de VBUS**: entrada sin pull, 20 ms de antirrebote | N2; lo necesita F9 | PA5 | antes de F9 |
| **F8** | ESP32: arreglar `reset_name()` (bits 23 y 28) y publicar los campos nuevos de `/status` | los bits están mal y aciertan por casualidad | — | fase 2 |
| **F9** | USB del STM32 (TinyUSB, `dwc2`, HSI48 + CRS): primero **CDC** (consola), después **DFU**, después **UAC2** | DS12_USB_STM32 §5 y §8; conectar sólo con PA5 alto | PA11, PA12, PA5 | después del bring-up |
| **F10** | Línea de timing del ESP32 a PD2 (EXTI con sello de tiempo) | PL §3, sincronismo entre nodos | PD2 ← GPIO4 (R132) | cuando se trabaje el multi-nodo |
| **F11** | `SPK_LAYOUT_2X16 = 1` atado a `BOARD_ds12` | hoy es un `#define` a mano (`main.c:94`) | — | fase 1 |

Lo que **no** necesita firmware: C28, TDMIN, los capacitores de `+5V_AUX` y los LED de tensión. El reset del
AK5538 al volver `+5VA` lo garantiza el hardware (D3 junto con PG), pero la *reconfiguración* es de F5.

## 2. Fases

### Fase 1 — Partir por placa, sin cambiar nada (se prueba en una DS11) ✅ hecha el 2026-10-09

> **Resultado:**
> - `make` (DS11) da un `.bin` **byte a byte idéntico** al de antes, y el bootloader también. Se compiló con
>   `GIT_ID=00000000 BUILD_EPOCH=0` y se comparó con `cmp`; antes se probó que el build es reproducible.
> - `make BOARD=ds12` compila sin warnings. Sólo cambia la geometría (`SPK_LAYOUT_2X16 = 1`): las capacidades
>   están declaradas, pero todavía no las usa nadie.
> - Pruebas negativas: `BOARD=ds13` lo frena el Makefile; un build sin placa lo frena `#error`.
> - La DS11 sigue saliendo en `build/ds11_beamforming.bin`, y la DS12 sale en `build_ds12/`.
> - El `_Static_assert` de pines duplicados no hizo falta: GCC ya avisa si un `#define` se redefine con otro valor.

1. Crear `firmware/Core/Inc/board/`:
   - `board.h`: el único que se incluye. Elige por `-DBOARD_DS11` / `-DBOARD_DS12`, y si no hay ninguno no compila;
   - `board_common.h`: lo que hoy es `pin_definitions.h`;
   - `board_ds11.h` y `board_ds12.h`.
2. `pin_definitions.h` queda como `#include "board/board.h"`: no se toca ninguno de los includes de hoy (`main.h`, `gpio.c`,
   `pcm1690.c`, `eeprom_config.c` y `main.c`).
3. En el Makefile:
   - `BOARD ?= ds11`, con `TARGET = $(BOARD)_beamforming`;
   - carpetas de compilación separadas (`build/ds11/`, `build/ds12/`);
   - `ota`, `flash-cube` y `todo-cube` tienen que usar la carpeta de la placa.
4. Capacidades en `board_ds12.h`: `BOARD_HAS_5VA_SENSE`, `BOARD_HAS_PG`, `BOARD_HAS_VBUS_SENSE`, `BOARD_HAS_USB_FS`,
   `BOARD_HAS_TIMING_LINE`, `BOARD_HAS_ID_JUMPER`, todas en 1, y `SPK_LAYOUT_2X16 = 1`. En `board_ds11.h`, todas en 0.
5. Un `_Static_assert`: ningún pin definido a la vez en `common` y en la placa.

**Criterio:**
- `make BOARD=ds11` da un `.bin` idéntico al de `main` salvo `fw_id`/`build`; se verifica con `cmp` después de enmascarar esos bytes;
- `make BOARD=ds12` compila sin warnings nuevos.

### Fase 2 — Identidad y autoarranque (se prueba en una DS11, con los parlantes desenchufados) 🟡 código hecho el 2026-10-09, falta el banco

> **Resultado (compilado, sin probar en placa):**
> - `make`, `make BOARD=ds12`, el bootloader y el ESP32 (`idf.py build`) compilan sin warnings nuevos.
> - **Bloque de identidad.** `fw_ident` quedó en `0x08020400` (se verificó en el `.map`). Los vectores terminan en
>   `0x298`, y el linker tiene un `ASSERT` por si algún día crecen hasta pisarlo.
> - **`make ota` se niega, no elige.** Se probó contra un servidor falso en la Mac, sin tocar ninguna placa, con seis
>   casos:
>   - ESP32 viejo sin `board` → cuenta como DS11, y deja pasar sólo `BOARD=ds11`;
>   - DS12 con `BOARD=ds11` → rechazo, y dice el comando correcto;
>   - DS12 con `BOARD=ds12` → sube;
>   - STM32 que no contesta → no sube;
>   - ESP32 que no contesta → no sube.
>
>   Compilar "la placa que corresponde" sola, como decía el plan, se descartó: un comando que cambia de binario según
>   lo que contesta la red es justo lo que no se quiere.
> - **`/status`** suma `board`, `board_mismatch` y `v5a_mv` (en 0 hasta la fase 3). `PROTO_VERSION` pasa a **23** y
>   `ds_status_t` crece de 101 a 105 bytes, agregando campos al final.
>   - El ESP32 nuevo pregunta campo por campo (`ST_HAS`) si la trama los trae: con un STM32 viejo no pierde nada.
>   - El ESP32 viejo con un STM32 nuevo copia los 101 bytes que conoce, y la página avisa que los protocolos no
>     coinciden.
>   - **Anda en los dos órdenes de despliegue.**
> - **Autoarranque.** La "ventana acotada" quedó así: si los amplificadores llevan **10 minutos** encendidos sin un
>   reinicio, el intento se da por bueno aunque sigan apareciendo errores esporádicos. No se cuentan minutos sanos:
>   lo que el contador protege es la alimentación, y la prueba de que aguanta es que la placa no se reinició.
> - **Qué cuenta como "reinicio nuestro".** Sólo SFT sin POR.
>   - En la app, los únicos resets por software son el de la OTA y el del ST-Link (`make reset`, `flash-cube`).
>   - Un HardFault deja el chip colgado: no reinicia.
>   - En el H7 un reset por software prende también PIN, así que **no** se puede exigir "sin PIN". Lo confirma M8 con
>     el `reset_rsr` crudo.
> - **El jumper lo leen los dos firmwares, no sólo el de la DS12.** El plan lo ataba a `BOARD_HAS_ID_JUMPER`, y así
>   protegía en una sola dirección: el firmware de la DS11 grabado por ST-Link en una DS12 no se daba cuenta.
>   - En la DS11 fabricada, PE15 está al aire (netlist del tag `ds11-release`: `unconnected-(U7-PE15-Pad45)`) y
>     con el pull-up lee 1.
>   - Cada placa declara `ID_JUMPER_ESPERADO`: 1 la DS11, 0 la DS12.
>   - Costo para las DS11 en servicio: si PE15 leyera 0 por un corto de fábrica, la placa no encendería los
>     amplificadores, y lo diría en el banner y en `/status`. Es una falla segura y a la vista.
> - El `board_id` también se escribe en `rsv[0]` de la cabecera de OTA.

1. **F1, bloque de identidad.**
   - 32 bytes en una sección del linker a `app + 0x400`: `magic "DSIM"`, versión, `board_id` (11/12), `proto_ver`, `fw_id`, `build`.
   - Una palabra reservada de la cabecera de OTA (`ota.h`) lleva el `board_id`. El bootloader instalado no las lee, así que no rompe nada.
2. **Chequeos de F1, en el orden en que se despliegan:**
   - `make ota` pregunta `/status`, compila la placa que corresponde y se niega a subir otra;
   - el ESP32 responde **409** a un `POST /firmware` de otra placa;
   - `OTA_End` del STM32 no escribe la cabecera si el `board_id` no es el suyo.

   El bootloader nuevo es la última barrera, y sólo existe en placas flasheadas por ST-Link.
3. **F2, jumper de identidad.**
   - Leer PE15 con pull-up **antes** de que la inicialización de GPIO la ponga en analógico (`main.c` ≈4558).
   - Lee 0 en la DS12 y 1 en la DS11. Si no coincide con `BOARD_ID`: amplificadores bloqueados, WiFi y OTA andando, y `/status` con `"board_mismatch": 1`.
4. **F6, autoarranque.**
   - `BKP3R` = marca de «intento en curso». Se pone al subir SD y se borra en AS_DONE.
   - Al arrancar, si está la marca y RSR tiene **SFT sin POR**, el intento no cuenta. **PIN y POR siguen contando**: así llega la caída del riel, porque el MAX809 baja NRST.
   - Para los errores esporádicos: contar **minutos sanos acumulados** dentro de una ventana acotada, en vez de reiniciar los 60 s con cada error.
5. **F8, ESP32.** En `firmware_esp32c3/main/main.c`, `reset_name()`:
   - el bit 19 pasa al **23** (PORRSTF);
   - el bit 25 pasa al **28** (WWDG1RSTF);
   - verificar los demás contra `stm32h743xx.h`, `RCC_RSR_*_Pos`.

   `/status` suma `board` y, si la placa lo informa, `v5va_mv`, `pg`, `vbus` y `board_mismatch`.

**Despliegue a las DS11:** el orden de siempre, el del Makefile: **`make ota` (STM32) primero, `make ota-esp` después**.
Mientras tanto, la página muestra el aviso de protocolos distintos.

**Pruebas (parlantes desenchufados, sin encender amplificadores a mano):**
- `make reset` y `make flash-cube` leyendo la línea `RESET:` (medida M8): **SFT**. Anotar también `reset_rsr`;
- un OTA a mitad de un autoarranque no suma una falla: aparece `AUTOARRANQUE` con el contador descontado;
- subir el firmware de la DS12 a una DS11 tiene que ser rechazado en las tres capas, probando cada una por separado:
  - `make ota BOARD=ds12` → lo frena el Makefile;
  - `curl` directo con el `.bin` de la DS12 → 409 del ESP32;
  - el STM32 sólo se puede probar con el ESP32 viejo, que no chequea → el error `la imagen es para DS12` en `ota_err`;
- `make flash-cube BOARD=ds12` en una DS11: la línea `PLACA: … NO COINCIDE`, los amplificadores no encienden y la
  OTA sigue andando para volver a la DS11;
- **primero que todo**, con el firmware normal: la línea `PLACA: firmware DS11, jumper PE15 = 1 (espera 1) (ok)` en
  las tres DS11.

### Fase 3 — La guarda de `+5VA` (código listo antes de que llegue la DS12; se prueba en ella) 🟡 código y test de host hechos el 2026-10-09

> **Resultado (compilado y probado en la Mac; falta la DS12):**
> - **El binario de la DS11 no tiene la guarda.** En su ELF, `ADC_IRQHandler` y `EXTI15_10_IRQHandler` son el handler
>   por defecto, y no aparece ningún símbolo de `power_guard`.
> - **Test de host** (`make test`): 14 escenarios simulados de a 1 ms. La propiedad de seguridad se revisa en cada
>   milisegundo de cada escenario: el pin SD nunca está arriba con `+5VA` bajo el corte o con PG bajo. Escenarios:
>   - sólo USB;
>   - caída y vuelta;
>   - riel que rebota;
>   - histéresis;
>   - amplificadores apagados al caer;
>   - una persona que los apaga durante la caída;
>   - fuente que tartamudea;
>   - ventana de 10 min;
>   - codecs mudos;
>   - caída durante la reconfiguración;
>   - PG solo;
>   - corte sólo por nivel;
>   - vuelta del tick a los 49,7 días.
>
>   **Tres mutantes** (sin los 50 ms, sin límite de restauraciones y con una ventana de 1 ms): el test detecta los
>   tres. Un "ok" que no puede fallar no prueba nada.
> - **Decisiones que el plan no traía:**
>   - **Límite de restauraciones.** Los amplificadores vuelven solos como mucho 3 veces cada 10 minutos. Una fuente
>     que no da abasto se cae cada vez que suenan, y sin límite la obra sería un tartamudeo de golpes. Pasado el
>     límite quedan apagados hasta que una persona los encienda, y `/status` lo muestra en `v5a.bloqueo`.
>   - **Codec que no contesta** después de la caída: la placa queda permitida (el riel está bien), pero no se
>     restaura sola. Lo decide una persona, igual que en el arranque.
>   - **387,5 ciclos de muestreo** en el ADC1, unas 10.000 conversiones por segundo. Convirtiendo más seguido, los
>     50 kΩ del divisor falsean la lectura, porque C312 no llega a recargarse.
>   - **Los umbrales se calibran una vez, al arrancar.** La HAL exige el ADC parado para cambiarlos. Si `+3V3` se
>     moviera un 3 % durante la obra, el corte se correría 0,13 V de `+5VA`. `+3V3` sale de otro buck.
>   - **Después de una caída, el ADC de audio queda apagado (PDN) hasta que vuelva `+5VA`**, aunque en sólo-USB
>     podría andar.
>   - **Las esperas del driver del PCM1690 ceden el CPU** (`PCM1690_Yield`) sólo durante una recuperación. Así el
>     audio no pierde fills en los ~120 ms que tarda, y el arranque queda como era.
>   - **`/status` lleva `"v5a": {mv, ok, pg, bloqueo, caidas}` en la DS12, y `null` en la DS11.** En la DS11 un
>     cero se leería como «`+5VA` en 0 V».
> - **Lo que cambia en la DS11** (no se probó en placa, entra con el banco de la fase 2):
>   - la configuración del AK5538 pasó a `AK5538_EscribirModo()`, que usan el arranque y la recuperación, con los
>     mismos cinco registros;
>   - `Set_Amps` se partió en la decisión y la escritura del pin. La escritura pregunta otra vez con las
>     interrupciones apagadas, ~10 ciclos;
>   - en el banner del arranque tiene que seguir apareciendo el readback `R02=0x31 R03=0x40 R04=0x02`.

Va todo detrás de `BOARD_HAS_5VA_SENSE`.

1. **F3, medición.**
   - **ADC1 en continuo sobre INP11 (PC1)**, sin tocar el ADC3, que ya mide temperatura y VREFINT por polling.
   - **Analog watchdog 1:** umbral bajo de **2,25 V** en el pin (`+5VA` = 4,5 V) y umbral de rearme de 2,35 V (4,7 V).
   - Calibrar el código del umbral con VREFINT al arrancar, porque VDDA = `+3V3` varía ±2,5 %.
   - La interrupción hace `GPIOD->BSRR = PD3 << 16` (SD en bajo) y baja PC7 (`PDN_AK`), en microsegundos.
   - **No** se hace por polling: con señal, entre 4,58 V y el UVLO de los PAM pasan sólo 0,78 ms.
2. **F4, la guarda.**
   - Una sola función, `amps_permitidos()`: `PC1 > 2,25 V` **y** `!board_mismatch` (y PG alto, como aviso).
   - La consultan `Set_Amps(1)`, el autoarranque, K2 largo y `POST /autostart`. Si es falsa, el pedido se rechaza con su motivo, por UART y en `/status`.
   - El autoarranque **se queda en AS_WAIT** mientras no haya `+5VA`. No cuenta el intento.
3. **F5, recuperación.** Cuando `+5VA` vuelve (watchdog rearmado y PG alto durante ≥ 50 ms):
   1. pulso de `~RST` a los PCM1690 (PC11);
   2. reconfiguración I²C de los cuatro;
   3. PC7 alto, que suelta el PDN del AK5538, y reconfiguración;
   4. **recién ahí** se vuelve al estado de amplificadores que había, **si había una persona o el autoarranque que lo había pedido**.
4. **Conviene separar la máquina de estados en un módulo puro** (`power_guard.c`, sin HAL), para compilarlo y probarlo
   en la Mac con un test de host: caída, vuelta, rebotes, pedidos durante la caída. Cumple la regla del repo de que
   todo umbral lleva su prueba.

**Pruebas en la primera DS12**, de la segunda tabla de `medidas_ds11.md`:
- sólo USB: SD nunca sube, ni con autoarranque armado;
- bajar la fuente hasta el UVLO y volver: los amplificadores vuelven solos sólo si estaban pedidos, y el ADC no queda con basura;
- osciloscopio en SD y en `+5VA` al cortar la fuente con señal: SD baja antes de que `+5VA` pase de 4,5 V.

### Fase 4 — Bring-up de la DS12

1. Primero el firmware de la DS11 tal cual, por ST-Link, **con los 20 V puestos** y para ver si suena. Después, `BOARD=ds12`.
2. Verificar el MAX809S de UMW: **que un OTA reinicie la placa**. Su salida es push-pull y el reset por software del STM32 tiene que poder ganarle.
3. **F7:** PA5 lee 0 sin cable, también con la placa caliente; lee 1 con la Mac enchufada en J8.

### Fase 5 — USB del STM32 (F9) y línea de timing (F10)

1. **TinyUSB con `dwc2`** sobre OTG_FS (AF10, PA11/PA12), con reloj **HSI48 + CRS** sincronizado al SOF: no toca PLL3, que es el del audio.
   - Buffers en `RAM_D2`.
   - **El pull-up de D+ se conecta sólo con PA5 alto durante ≥ 20 ms, y se desconecta apenas baja.**
2. Orden:
   1. **CDC-ACM**, la consola, con el mismo protocolo binario que ya va por UART4;
   2. **DFU**, para actualizar el STM32 con un cable;
   3. **UAC2 4×4 a 24 bit/48 kHz** con endpoint de feedback (DS12_USB_STM32 §7);
   4. **USB-MIDI**, si se quiere.

   Medir CPU contra `docs/ds11_perf.md` en cada paso.
3. **F10:** PD2 como EXTI con sello de tiempo (contador DWT). El ESP32 maneja GPIO4 como salida activa en bajo. El
   protocolo de sincronismo es parte del trabajo de multi-nodo (`docs/multinodo` / `esp32_roles`), no de este plan.

## 3. Qué se prueba dónde

| | en una DS11 | en la DS12 |
|---|---|---|
| F0 binario idéntico, F11 | ✅ | — |
| F1, F2 (jumper al aire = DS11), F6, F8 | ✅ | ✅ |
| F3, F4, F5 (PC1, PG) | sólo el test de host de `power_guard.c` | ✅ |
| F7, F9 (USB, PA5) | — | ✅ |
| F10 | — | ✅ |

## 4. Riesgos

- **Partir `pin_definitions.h` toca cinco archivos que incluyen.** El binario idéntico es la red de seguridad: si no
  da idéntico, no se sigue.
- **El bloque de identidad corre la imagen 32 bytes:** verificar que `OTA_Begin` y el bootloader no asuman offsets fijos más allá de la cabecera.
- **El watchdog del ADC no puede quedar activo en la DS11** (PC1 al aire): por eso va detrás de la capacidad, no de un `#ifdef`.
- **TinyUSB suma interrupciones al lado del SAI:** prioridad del OTG_FS por debajo de la del DMA de audio, y medir los fills del ADC (la invariante 200/0/0 por segundo).
