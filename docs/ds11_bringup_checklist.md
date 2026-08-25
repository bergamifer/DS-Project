# DS11 — Checklist de primer encendido (32 canales)

> Para usar parado frente a la placa. Primera vez que los 32 parlantes suenan:
> todo lo anterior se validó sobre DS10 con 8. Orden pensado para que cada paso
> valide una cosa y falle barato.

---

## 0. Antes de dar tensión

- [ ] **⚠️ REPARAR LOS DOS CORTOS DE FÁBRICA** — afectan a **todas** las placas
      de la tirada. Ver `ds11_errata.md`: via de GND cortocircuitando +3V3 (se
      taladra) y pista de +5VA sobre GND bajo C290 (se abre con bisturí).
      **Verificar antes de alimentar**: TP1↔TP3 (`+3V3`) y pad `+` de C290 ↔ GND
      deben dar cientos de Ω o más, nunca sub-ohm.
- [ ] **Cortar el pin 1 del cable del ST-Link** (3.3 V sin protección → colapsa
      el dongle). Detalle en `ds11_errata.md`.

- [ ] **Alimentar a 19-20 V, NO a 24 V.** El buck MP2315S tiene 26 V de máximo
      absoluto y a 24 V el margen ante transitorios es cero. Además los 32
      PAM8302A **en fase** (justo lo que hace un beam enfocado) pueden pedir
      más de los 3 A / 15 W del buck → brown-out que se lleva puesto al MCU y
      al ADC. Detalle en `ds11_upgrades.md`.
- [ ] **Volumen bajo.** Con 32 parlantes hay **+6 dB** de salida acústica
      respecto a DS10 al mismo ajuste. El default persistido es −24 dB; si
      venís de mediciones, verificá que no quedó más alto.
- [ ] Parlantes conectados y, si es posible, la primera prueba lejos de
      superficies duras.

## 1. Flashear (los dos micros)

El protocolo binario es v2 en ambos lados: si flasheás uno solo, la telemetría
no parsea (los comandos igual funcionan, pero `/status` da error).

```bash
cd firmware && make -j4 && make flash-stlink
# desconectar el ST-Link y reiniciar la placa  <-- IMPORTANTE
cd ../firmware_esp32c3 && idf.py -p /dev/cu.usbmodemXXX flash
```

- [ ] **Con el ST-Link conectado la placa se cuelga en el boot** (verificado
      2026-07-12). Flashear, desconectar, reiniciar.

## 2. Log de arranque — qué tiene que decir

Por UART7 (J7, 115200 8N1) o por el monitor del ESP32, que lo reenvía con
prefijo `[STM32]`.

- [ ] `=== DS11 Beamforming Audio (32ch) ===`
- [ ] 🔴 **`Silicio: rev X` o `rev V`** — ES0392 §2.2.21 dice que **VOS0 no existe
      en las revisiones Y y W**, que quedan limitadas a 400 MHz. Este firmware
      configura **VOS0 + 480 MHz**: sobre un chip Y o W eso corre **fuera de
      especificación** (la escritura de VOS0 no toma efecto y el core queda a
      480 MHz con el voltaje de VOS1). Si el banner dice **Y, W o Z**, parar y
      bajar a 400 MHz antes de seguir. La placa de banco aguantó 19,6 h con
      `Err=0`, o sea que su silicio está bien — pero **JLCPCB puede entregar otro
      date code en la próxima tirada sin avisar**, y por eso se verifica en cada
      arranque.
- [ ] `SAI DMA: 1B=OK 2B=OK 1A=OK 2A=OK` — los cuatro bloques arrancaron.
- [ ] `PCM1690: 4/4 DACs initialized OK` ← **el número clave de DS11.** En
      DS10 era 4/4 también, pero sólo U2 sacaba audio (rework fallado). Acá
      los 4 tienen que estar de verdad.
- [ ] Los 4 bloques SAI con `SAIEN=1 WCKCFG=0 OVRUDR=0`.
- [ ] `AK5538VN: readback R01=0x01 R02=0x31 R03=0x40` — clock mode y TDM256.
- [ ] `EEPROM: I2C4 init=0, available=1`
- [ ] `Memoria: buffers DMA en RAM_D2, tablas calientes en DTCM — OK`. Silencioso
      cuando está bien; si aparece un `!!` con una dirección, **parar**: las dos
      clases de bug más caras del proyecto fueron de *ubicación* (un buffer de DMA
      en DTCM no sale por el SAI; una tabla del lazo de fill en FLASH cuesta wait
      states por muestra). Ninguna da error de compilación.
- [ ] `INGAIN: +N dB al boot` — 0 = línea. Si dice 30, lo que entre por J9 va a
      recortar; ver "Las DOS ganancias" en `manual_modos.md`.
- [ ] `PAM8302A: SD HIGH — 32 amplifiers enabled`

## 3. Heartbeat — las dos métricas nuevas

```
[Ns] Half: 50/s, Full: 50/s, Err: 0, Undr: 0, ...
     SYNC bancos: <n> us | CPU fills: <n>%
```

- [ ] **`Half: 50/s` y `Full: 50/s`**, `Err: 0`.
- [x] **`SYNC bancos`** — ✅ **medido 2026-08-13: 4 µs (2038 ciclos)**, estable
      arranque tras arranque. Son 1,4 mm de camino acústico: irrelevante frente
      al pitch de 80 mm. La duda de julio sobre el desfase entre bancos **queda
      cerrada**. Si alguna vez da **milisegundos**, es una regresión — revisar
      que los cuatro `HAL_SAI_Transmit_DMA` sigan consecutivos y sin prints en
      el medio. (Ver `ds11_perf.md` §1 y "Sincronismo entre bancos" en
      `manual_modos.md`.)
- [x] **`CPU fills`** — ✅ **medido en soak de 19,6 h (2026-08-14): 26,6% en modo 7**
      (`Fill avg = 668 µs`), contra 59,2% antes de los cambios de I-cache/`-O2`.
      Si volvés a ver ~60%, algo revirtió: chequeá `SCB_EnableICache()` en `main()`
      y `OPT = -O2` en el Makefile. Falta anotar el valor en **modo 15**.

## 3.5 Sesión de escucha del 2026-08-19 — qué falta validar POR OÍDO

El 2026-08-18 entró mucho cambio y **todo se verificó por telemetría, ninguno por
oído**. Los 16 modos dan 0 underruns, 0 errores SAI y 0 clip; eso descarta
deadlines perdidos y recorte digital, pero **no** ve artefactos de fase,
polaridad ni nivel. Esta lista está ordenada: si algo falla, parar ahí.

```bash
IP=domosonica.local
curl -X POST http://$IP/reset -d ''            # arrancar de un estado conocido
curl -X POST http://$IP/amps  -d '{"on":true}'
curl -X POST http://$IP/volume -d '{"db":-40}' # y subir de a poco
```

1. **Modo 13 (tono 1k) — ¿pops?**
   Un tono puro delata cualquier discontinuidad. Escuchar 2 minutos: tiene que
   cruzar al menos dos resúmenes de 60 s **sin un tick**. Ese pop por minuto era
   real (el resumen descartaba fills pendientes, `ds11_perf.md` §8) y este es el
   test que lo confirma muerto.

2. **Modo 14 (MULTIFOCUS 4 tonos) — el que peor sonaba.**
   Tenía dos bugs, los dos arreglados: la tabla de senos en FLASH y la fase entre
   bancos. Si sigue con pops, **es distinto de lo diagnosticado** y hay que
   volver a mirar — probablemente el buffer de 240.

3. **Modos 8 y 9 — ¿el spot cae donde dice?**
   `8` enfoca 0,5 m a la **izquierda**, `9` a la **derecha**, del oyente mirando
   los conos, con un tono de **2 kHz** (spot ±143 mm a 1 m).

   ✅ **Verificado a oído el 2026-08-19: funciona y NO está espejado.**

   ⚠️ **No se distinguen por el timbre: es el mismo tono ondulante en los dos.** Lo que
   cambia es dónde sube el nivel. El test correcto es un **A/B desde un punto
   fijo**: pararse a 50 cm a la izquierda del centro, a 1 m del array, y alternar
   8 ↔ 9. Ahí el 8 tiene que sonar **claramente más fuerte** que el 9. Después
   repetir 50 cm a la derecha y esperar lo contrario. Escuchar parado en el
   centro no prueba nada — es justo el punto donde los dos suenan igual.
   - Si están **espejados**, es una línea: el signo de `spk_x_mm[]` en
     `Beam_InitTxGeometry()`. **Conviene saberlo antes de la DS12.**
   - Si no se nota ningún spot, sospechar de la polaridad (`speaker_sign`) o del
     mapeo canal→parlante: correr el walker (§4.1).
   - El modo `10` enfoca a 0,3 m: el spot tiene que ser **más chico y más
     marcado** que el del `7` a 1 m. Es un test auto-validante.

4. **Modo 0 con la Mac al 100 %** — no tiene que distorsionar con `ingain` en 0.
   Si distorsiona, mirar si aparece la línea `SATURACION` y cuál de los dos
   contadores creció (`manual_modos.md`, "Las DOS ganancias").

5. **Cambiar de modo mientras suena** — un tick al cambiar es esperable
   (se recalculan focos y se guarda en EEPROM); un pop fuerte no.

6. **Modo 15 con las 8 entradas** — el objetivo artístico. Arrastrar los puntos
   del pad de la web y verificar que cada entrada se mueve en el espacio.

> Si algo suena mal, lo más útil que se puede capturar es el **log serie del
> minuto en cuestión** más `curl -s http://$IP/status`: entre `underruns`
> (acumulado desde v5), `out_clip`, `adc_ovf`, `cpu_fills_pct` (v6) y
> `stm32_uptime_ms` casi siempre alcanza para separar deadline / recorte /
> presupuesto de CPU / reinicio **sin volver a escuchar**.
>
> **Presupuesto de CPU esperado** (`cpu_fills_pct`, medido 2026-08-18): RX beam
> 1,3 % · walker 2,2 % · monitor 4,9 % · AUTOFOCUS 11,1 % · TX con tono 27,0 % ·
> MULTIFOCUS 4 tonos 63,4 % · MULTIFOCUS 8 entradas 64,8 %. Si alguno aparece
> muy por encima de eso, hay una regresión de rendimiento — y esa se ve sin oído.

## 4. Pruebas, en orden de valor diagnóstico

### 4.1 Walker (modo 1) — el test que más informa

```bash
curl -X POST http://<IP>/mode -d '{"mode":1}'
```

Un parlante por vez, 1 s cada uno, **32 s por vuelta completa**.

- [ ] **Suenan los 32**, uno por uno. Cualquier silencio = ese canal no llega
      (soldadura, JSPK, DAC).
- [ ] **El orden es coherente con la geometría**: ch0-7 = fila 1 de izquierda
      a derecha, ch8-15 = fila 2, ch16-23 = fila 3, ch24-31 = fila 4. Esto
      valida el mapeo `ch = fila*8 + col` que asume TODO el steering.
- [ ] Si el orden no coincide, **anotá el mapeo real** — es más barato
      corregir la tabla de geometría que recablear.

Este paso valida la fundación de todo lo demás. No sigas si falla.

### 4.2 Monitor ADC (modo 0)

```bash
curl -X POST http://<IP>/mode -d '{"mode":0}'
```

- [ ] Las 8 entradas se repiten en cada fila (entrada N en la columna N de las
      4 filas). Tocar una entrada debe oírse en 4 parlantes, uno por fila.
- [ ] `ADCpk` en `/status` sube al tocar la entrada correspondiente.

### 4.3 Tono enfocado (modo 12) — primera vez con apertura vertical

```bash
curl -X POST http://<IP>/mode -d '{"mode":12}'
curl -X POST http://<IP>/volume -d '{"db":-20}'
```

- [ ] Suena limpio (sin distorsión = no estás clipeando los PAM).
- [ ] **Novedad de DS11**: ahora `fy` hace algo. Con las 4 filas pobladas la
      apertura vertical (480 mm) también enfoca — probá mover el foco en Y:
      ```bash
      curl -X POST http://<IP>/beam -d '{"type":"tx","fx":0,"fy":-300,"fz":1000}'
      curl -X POST http://<IP>/beam -d '{"type":"tx","fx":0,"fy":300,"fz":1000}'
      ```
      En DS10 esto no cambiaba nada. Si ahora tampoco, sospechar del SYNC.
- [ ] Medir a **0.5–1 m**, no a 2 m ni 4 m (a más distancia el foco es más
      débil: N de Fresnel 0.91 @0.5m vs 0.23 @2m).

### 4.4 MULTIFOCUS tonos (modo 14) — repetir la validación de DS10

```bash
curl -X POST http://<IP>/mode -d '{"mode":14}'
```

- [ ] Caminando de izquierda a derecha, los 4 tonos aparecen de grave a agudo.
      Ya funcionó con 8 parlantes; con 32 el contraste debería ser **mejor**
      (+6 dB de ganancia coherente y lóbulos laterales más bajos).

### 4.5 MULTIFOCUS entradas (modo 15) — la intervención

```bash
curl -X POST http://<IP>/mode -d '{"mode":15}'
```

- [ ] **Anotar `CPU fills`** — es el fill más caro (8 fuentes × 32 salidas).
      ⚠️ **Este modo NO entraba en el presupuesto de CPU hasta el 2026-08-13**:
      perdía el 55% de los deadlines de fill. Se arregló con I-cache + `delay_line`
      en DTCM + `-O2` (43× de mejora). Ver `ds11_perf.md`. Si el modo 15 vuelve a
      sonar entrecortado, ese es el primer lugar donde mirar.
- [ ] `Undr` estable y bajo. **No va a dar 0**: los prints bloqueantes del
      heartbeat cuestan ~4/s por sí solos (`ds11_perf.md` §2, causa A). Lo que
      importa es que no crezca de a cientos por segundo.
- [ ] Con una sola fuente conectada ya se prueba: una voz situada en un punto
      del aire. Las entradas libres sólo aportan piso de ruido.

## 4.6 Modo 17 — BARRIDO VERTICAL: la medición que decide la DS12

**No es una demo, es un experimento.** Parado quieto a ~1 m, escuchá el ciclo de
5 s y anotá cuál de las dos cosas pasa:

- [ ] **Se percibe el foco subiendo y bajando** → el problema del eje vertical
      era el *aliasing* (filas a 0,93 λ a 2 kHz). Bajar el pitch de filas a
      ≤86 mm en la DS12 se justifica.
- [ ] **No se percibe nada, o sólo sube y baja el volumen** → el limitante es la
      *apertura* (sólo 4 filas). Bajar el pitch no alcanza: hay que agregar filas.

Referencia de "así se siente cuando funciona": el modo 16, mismo barrido pero
horizontal, donde el array sí tiene resolución. Correr los dos seguidos.

Fundamento y consecuencias: `hardware/DS/DS12_PLANNING.md` §0.5.

---

## 5. Cuando llegue el preamp (ECM8000 + Focusrite 2i2)

- [ ] **Antes de escanear, mirar la tarjeta AUTOFOCUS** en
      `http://domosonica.local`: las cinco filas tienen que estar en verde hasta
      "nivel del micrófono". Si el micrófono no da señal o los amps están
      apagados, el scan mide ruido — y desde el 2026-08-20 lo **descarta** en vez
      de aplicar 32 retardos de basura (verificado: 4/32 en el peor caso).
- [ ] AUTOFOCUS (modo 11) — ahora escanea **los 32 parlantes, ~8 s**. El mic
      no se mueve durante todo el barrido. Guía completa en
      `autofocus_setup.md`.
- [ ] La fila "resultado" tiene que decir **≥16/32 parlantes medidos con
      confianza**. Con menos, el scan queda descartado: revisar nivel del preamp
      y posición del micrófono antes de repetir (el botón *Iniciar scan*
      relanza).
- [ ] Revisar los `sign=` del log: cualquier `-` es un parlante con polaridad
      invertida. La doble inversión de los canales impares es **deliberada**
      y debería dar `+` en todos; un `-` inesperado es un dato de hardware.

---

## Notas

- **Los delays medidos por AUTOFOCUS no se persisten** y se pisan al cambiar
  de modo; las polaridades sobreviven hasta el reboot. ~~Si el SYNC entre bancos
  resultara variable por arranque, una calibración dejaría de valer tras un ciclo
  de alimentación.~~ **Descartado 2026-08-13**: el SYNC mide 4 µs y es reproducible
  arranque tras arranque, así que una calibración sí sobreviviría un ciclo de
  alimentación. Persistirla es trabajo pendiente, no un imposible.
- **El protocolo remoto sigue limitado a 16 canales** (`CFG_NUM_CHANNELS`):
  los canales 16-31 suenan con sus defaults pero no se pueden ajustar ni
  mutear individualmente desde la web. No bloquea nada del primer encendido;
  está en el backlog.

---

## 6. Segunda DS11 — estado y qué le falta (2026-08-25)

La segunda placa entró en servicio el 2026-08-25. Es la **primera con los cuatro
PCM1690 poblados y respondiendo**. `192.168.100.70`, ESP32 MAC `08:92:72:1e:1f:30`.

### Lo que ya está

| | |
|---|---|
| STM32 | bootloader en `0x08000000` + app en `0x08020000`, con la instrumentación de `NDTR` (§9 de `loopback_debug.md`) |
| ESP32 | build del 2026-08-23, proto v9, OTA con rollback |
| `/health` | los 7 chequeos en verde, los cuatro DAC incluidos |
| Loopback | J13 soldado, **entrado por la entrada 4**, polaridad **+** |
| Estado seguro | −40 dB, modo 0, amplificadores apagados |

### ⚠️ Lo que le falta, en orden

**1. ~~`device_id` y mDNS colisionan~~ ✅ ARREGLADO 2026-08-25.** Los dos salen
ahora del MAC del ESP32, que ya es único y no hay que administrar: esta placa es
**`DS_1E1F30` en `ds-1e1f30.local`**.

> ⚠️ **`domosonica.local` ya no resuelve.** Es deliberado: con dos placas
> encendidas resolvía a cualquiera de las dos. Se prefirió romperlo con dos
> placas y las herramientas a mano antes que con dieciséis en una sala. La IP
> funciona siempre, y `tools/captura.py` acepta `--host` o la variable `DS_HOST`.

**2. No tiene 24 V.** Hoy corre de USB. Sin 24 V no hay amplificadores, así que no
hay escucha, y **no se puede medir el skew entre bancos**: los adaptadores de §4
toman de la salida de parlante, que sin 24 V no entrega nada. Es lo que bloquea
la única medición pendiente de valor en una sola placa.

**3. Trampa operativa del volumen.** El loopback (modo 19) **necesita 0 dB** — a
−40 dB el chirp no supera el umbral y las ocho entradas informan `pico=1`. Pero
0 dB con amplificadores encendidos mete al PAM8302A unos 40 dB adentro del
recorte. **Son incompatibles**: nunca dejar 0 dB si se van a encender los amps.
Volver a −40 dB al terminar de medir.

**4. ~~El nivel 1 de `multinode.md` no está implementado~~ ✅ HECHO 2026-08-25.**
`loopback_debug.md` §10. La dispersión entre arranques pasó de **29,8 mm a
0,21 mm (140×)** y la fase quedó en 0 en 41 de 41 arranques. El heartbeat ahora
publica `fase RX-TX`, que se lee **sin cable de loopback** — es el número con el
que cada nodo de una instalación sabe en qué estado arrancó.

### Cómo se flashea de cero

`st-flash` **no sirve en esta Mac**: escribe una vez y después el ST-Link queda
enumerado pero no reclamable (libusb pide entitlements o root). Usar
CubeProgrammer, que trae driver propio — y sirve además para loops de reinicio:

```bash
CLI="/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI"
"$CLI" -c port=SWD mode=UR -w firmware/build_boot/ds11_boot.bin 0x08000000 -v
"$CLI" -c port=SWD mode=UR -w firmware/build/ds11_beamforming.bin 0x08020000 -v
"$CLI" -c port=SWD mode=UR -rst
```

ESP32 de cero, con los binarios de `firmware_esp32c3/build` (esptool v4.9 usa
comandos con guion bajo):

```bash
ESPTOOL=~/.espressif/python_env/idf6.0_py3.13_env/bin/esptool.py
$ESPTOOL -p /dev/cu.usbmodem21301 --chip esp32c3 --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x0 bootloader/bootloader.bin 0x8000 partition_table/partition-table.bin \
  0xd000 ota_data_initial.bin 0x10000 ds11_beamform_c3.bin
```

### Dónde sale el debug (DS11 ≠ DS9)

- **UART7** (PE7/PE8) → J7 pines 7-8 = **VCP del ST-Link**. Ahí sale *todo*: el
  bootloader y la app. Es la consola.
- **UART4** (PA0/PA1) → ESP32, y sólo lleva el protocolo binario.
- **No hay contención.** La regla de DS9 de desconectar el serie del ST-Link
  antes de usar el ESP32 **no aplica** a DS11.
