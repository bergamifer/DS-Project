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

## 5. Cuando llegue el preamp (ECM8000 + Focusrite 2i2)

- [ ] AUTOFOCUS (modo 11) — ahora escanea **los 32 parlantes, ~8 s**. El mic
      no se mueve durante todo el barrido. Guía completa en
      `autofocus_setup.md`.
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
