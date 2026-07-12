# AUTOFOCUS (modo 11) — Setup de medición con ECM8000 + Focusrite 2i2 3rd gen

> Guía para el día de la prueba. El firmware ya está listo (revisado 2026-07-12);
> esto es solo cableado, gain staging y procedimiento.

## Cadena de señal

```
ECM8000 ──XLR──> 2i2 IN 1 (+48V ON) ──[Direct Monitor]──> 2i2 OUT L (TRS)
                                                              │
                                              cable TRS→TS (o TS común)
                                                              │
                                            DS10 J9 entrada 1 (= ADC c0)
```

## Configuración de la 2i2 (standalone, sin computadora)

La 3rd gen funciona como preamp autónomo alimentada por USB-C (cargador de
pared o la Mac, da igual — no hace falta software).

1. **Alimentación**: USB-C a un cargador 5V.
2. **48V ON** (botón phantom) — el ECM8000 es condenser, sin phantom no da señal.
3. **AIR OFF** en el canal 1 (colorea la respuesta; para medición, plano).
4. **INST OFF** (es mic, no instrumento).
5. **Direct Monitor: MONO** (el botón cicla off/mono/stereo). Esto rutea la
   entrada 1 directo a las salidas de línea sin computadora.
6. **Perilla Monitor** (la grande): arrancar a ~50%; es el volumen de la salida
   de línea hacia el DS10.
7. Salida **OUT L** (TRS 1/4") → entrada 1 del J9 del DS10. Cable TRS→TS está
   bien (desbalancear en tramo corto no mete ruido apreciable).

## Gain staging (antes de escanear)

El objetivo: chirp bien por encima del piso de ruido, sin clipear ni la 2i2
ni el AK5538.

1. Encender todo, `GET /status` → verificar `stm32:ok`.
2. **Verificar que el mic entra por c0**: golpecitos suaves al mic → `adc_pk[0]`
   tiene que saltar en `/status`. Si salta otro índice, mover el plug de J9 a
   la entrada correcta (CAL_MIC_CH = 0). Piso de ruido esperado: ~100 cuentas.
3. **Nivel de referencia**: modo 12 (tono 2k) con foco cerca del mic:
   ```bash
   curl -X POST http://<IP>/mode -d '{"mode":12}'
   curl -X POST http://<IP>/volume -d '{"db":-20}'
   ```
   Ajustar GAIN del canal 1 de la 2i2: el halo debe quedar VERDE (picos ámbar
   OK, nunca rojo), y `adc_pk[0]` en `/status` en el orden de **10^5–10^6**
   (1–10% de fondo de escala 24-bit ≈ 8.3M).
4. **OVF debe ser 0** en `/status` durante todo el proceso. OVF=1 = el AK5538
   está clipeando → bajar la perilla Monitor de la 2i2.

## Procedimiento del scan

1. **Posicionar el mic EN el punto focal deseado** (≥30 cm del array; ideal
   0.5–1 m sobre el eje). El mic no se mueve durante el scan (~2 s).
2. Volumen moderado (el chirp sale a -6dBFS + atenuación DAC):
   `curl -X POST http://<IP>/volume -d '{"db":-20}'`
3. Disparar: `curl -X POST http://<IP>/mode -d '{"mode":11}'`
   ⚠️ El scan arranca INMEDIATAMENTE al entrar al modo — mic primero, POST después.
4. Secuencia audible: silencio (100 ms) → chirp corto por el parlante 1 →
   silencio → chirp parlante 2 → … × 8. Total ~2 s.
5. Al terminar (`cal_state:5` en `/status`) suena un **tono de 1 kHz enfocado
   en el mic** usando los delays y polaridades MEDIDOS. `adc_pk[0]` es ahora el
   nivel en el foco: mover el foco NO se puede en este modo, pero mover el mic
   sí — alejarlo del punto medido debería bajar `c0` (mapa del spot real).
6. Para re-escanear (otro punto, u otro nivel): reposicionar mic y re-mandar
   `{"mode":11}`.

## Lectura de resultados

- **Por telemetría** (`GET /status`): `cal_state` progresa 1(SETTLE)→2(ARM)→
  3(CAPT)→4(CORR) por parlante → 5(DONE). `adc_pk[0]` = nivel en el foco.
- **Por UART7** (header J7, 115200 8N1 — ST-Link VCP u otro USB-serial):
  una línea por parlante: `CAL chN: tof=X.XX smp sign=± q=NN`.
  - **q ≥ 50** = medición sólida. q < 50 = señal de mic pobre → subir volumen
    o gain, o acercar el mic.
  - **sign=-** = ese parlante está con polaridad invertida (¡dato de oro para
    el hardware!). El firmware ya lo compensa en los modos TX hasta el reboot.
- ⚠️ El ST-Link conectado colgó la placa en una prueba (2026-07-12) — si pasa,
  leer solo por telemetría, o usar un adaptador USB-serial genérico en J7.

## Qué NO persiste

- Los **delays medidos** se pierden al cambiar de modo (los pisan los
  geométricos del preset). Las **polaridades** medidas quedan hasta el reboot.
- Nada va a EEPROM. Si la calibración medida resulta valiosa → backlog:
  persistir `speaker_sign[]` + exponer resultados via `CMD_GET_CAL_RESULT`.

## Troubleshooting

| Síntoma | Causa probable |
|---|---|
| q bajo en TODOS los canales | Gain insuficiente, mic lejos, o mic en otra entrada (ver paso 2 de gain staging) |
| q bajo en UN canal | Ese parlante suena poco/nada — revisar cableado JSPK |
| OVF=1 durante el scan | 2i2 Monitor muy alto → bajar perilla |
| sign=- en algún canal | Polaridad invertida real del parlante — anotar para hardware |
| tof muy dispares (>30 smp) | Mic demasiado cerca del array, o reflexión fuerte más intensa que el directo |
