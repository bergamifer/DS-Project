# Medidas de banco para cerrar la revisión de alimentación

> Lo que el papel no puede decidir. Cada fila dice qué medir, qué se espera, y **qué decisión dispara** cada
> resultado. La primera tabla se hace hoy en una **DS11**, que comparte con la DS12 la parte que importa.
> La segunda espera a la primera DS12. De dónde sale cada número: [`../DS12_REVISION_ALIMENTACION.md`](../DS12_REVISION_ALIMENTACION.md)
> §8–§9 y los informes de [`ronda2/`](ronda2/).

## ⚠️ Antes de empezar

- **Ninguna medida de la tabla 1 enciende los amplificadores**, salvo las dos marcadas «🔊 decisión de Fernando».
- Antes de dar los 20 V, **confirmá que la DS11 no tenga el autoarranque activado** (`/status`, bandera de autoarranque).
  Si no lo podés confirmar, **desenchufá los parlantes**: con el autoarranque los amplificadores se encienden solos a los 15 s.
- Micrófonos desenchufados. Con micrófonos, los modos 0, 2–6 y 15 hacen acople (CLAUDE.md).

## 1. En una DS11 (hoy)

| # | qué | cómo | se espera | si da… → hacer |
|---|---|---|---|---|
| **M1** | VBUS de J3 sin cable (N4) | 20 V puestos, nada en J3. Multímetro en el pad de C31 que no es masa | ≈ la tensión de `+5V` de la DS11 (≈4,7–4,8 V) | **≈4,8 V** → confirma N4. **< 0,4 V** → el modelo está mal: avisar antes de dibujar |
| **M2** | ¿quién lo levanta? | lo mismo, con una resistencia de **1 kΩ** apretada entre ese pad y masa | 0,8–1,3 V si el pull-up de D+ del ESP32 entra por U6 | **0,8–1,3 V** → pasar la pata 5 de U6/U9 a `+3V3` es **obligatorio**. **< 0,05 V** → alcanza con el 1 kΩ de descarga |
| **M3** | ¿la Mac reconoce al ESP32 si el cable va **después** de la fuente? | placa encendida → enchufar la Mac por C a C → ¿aparece `/dev/cu.usbmodem…`? 5 veces, en frío y en caliente. Control: cable primero, 20 V después | lo reconoce las 5 veces | **las 5** → el arreglo de J3 baja a 🟢 (el 1 kΩ de J8 va igual). **Falla y el control anda** → el arreglo de J3 es obligatorio. **Falla también el control** → es otro problema |
| **M4** | Continua en las entradas del ADC (C4) | 20 V, firmware andando, sin micrófonos. Multímetro en el extremo **hacia el chip** de 4–5 de las resistencias de 470 Ω de entrada | **1,65 V (1,55–1,75)**, estable; tarda ~25 ms en asentarse | **1,58–1,72 V** → no agregar polarización. **Estable fuera de eso** → decisión tuya. **Va sola hacia 0 V o 3,3 V** → agregar la polarización (AU §3.2) |
| **M5** | ¿los SD de los PAM tienen pull-down interno? (S2) | **placa desenchufada.** Óhmetro entre el extremo de R20 que va a los PAM y masa; esperar a que se quede quieto | **1,033 kΩ** (R20 + R19) | **≈1,03 kΩ** → R20 puede subir a 5,1 kΩ (opcional). **Menos** → hay pull-down interno: R20 no se toca |
| M6 | (opcional) ¿el pin SD tiene diodo hacia VDD? | **desenchufada.** Tester en modo diodo, rojo en ese extremo de R20 y negro en el + de C291 (el lado sin marca). Primero medir la corriente del tester sobre 1 kΩ, y leer **después de 10 s** | sin diodo: I·1033 Ω + 0,6 V (≈1,6 V) | **≈0,4–0,7 V estable** → existe el diodo: la inyección de S2 es real. La lectura inmediata engaña, porque arranca baja y sube |
| **M7** | ¿el AP2125 oscila con cerámicos? (S7) | 20 V. Osciloscopio en **TP2** (`+3.3VA`), acople AC, 20 MHz, resorte de masa | ruido < 20 mVpp, sin una frecuencia fija | **una oscilación sostenida** → hay que agregar ESR en la salida del AP2125 (la DS12 tiene la misma población) |
| **M8** | ¿`-rst` deja SFT? (N6) | `make -C firmware reset` y después `make -C firmware flash-cube`, leyendo la línea `RESET:` por la consola del ST-Link (UART7) | `SFT` en las dos | **SFT** → el arreglo de N6 sirve tal cual. **PIN** → hay que marcar el intento de otra manera |
| M9 | (opcional) ¿cuánto le pide la placa a la PC? (S1) | 20 V y la Mac por J3, con un medidor USB-C en línea | 0–45 mA | sólo confirma S1; no dispara nada |
| M10 | 🔊 **decisión de Fernando** — ruido de los PAM al ADC (S11) | entradas 0–3 vacías, en un modo **sin** lazo (1, 7–14 o 16–23). Espectro del ADC con amps OFF y ON | ningún tono > −140 dBFS por bin; el piso (−113,4 dBFS) no sube más de 0,5 dB | **pasa** → S11 queda 🟢 con evidencia. **No pasa** → revisar el camino `+5VA` → AVDD antes de pedir |
| M11 | 🔊 **decisión de Fernando** — PSRR del PCM1690 | un tono por un canal con los demás amplificadores sonando; buscar productos en 2f y 3f | nada por encima del piso | **productos visibles** → mirar FB3 / `+5VA_DAC` |

## 2. En la primera DS12 (cuando llegue)

| qué | cómo | se espera |
|---|---|---|
| Fuga de D23 con el STM32 en blanco (N1) | 20 V, STM32 todavía sin programar. TP8 (`+5V_AUX`) | 4,8–4,9 V. **Más de 5,1 V** → la fuga de D23 le gana: revisar el drenaje |
| El MAX809S de UMW contra el reset del STM32 | hacer un OTA y ver que la placa se reinicie sola | se reinicia. Si no: la salida push-pull del MAX809S le gana al reset interno del STM32 |
| Golpe de ariete en J8, ya con los capacitores | 20 V. Osciloscopio en TP8. Enchufar en caliente un cable A a C que ya traiga VBUS | pico ≤ 5,8 V |
| Modo sólo-USB con el ESP32 transmitiendo (N-b) | sin 20 V, con el cable real. Osciloscopio en el pad de C319 (`IN`) | **≥ 3,7 V** al arrancar y nunca debajo de 3,1 V con TX. Si no, el flasheo sólo por USB no es confiable |
| PG del AP63356 con EN en bajo | bajar la fuente hasta el UVLO (≈11–14 V) y mirar PG (pad de R159) | bajo (la Fig. 9 de DS41949 lo sugiere; no está especificado) |
| Lazo del buck A | escalón de carga en `+5VA` (TP6) | no más de dos oscilaciones visibles |
| Rampa de `+3V3` y el EN del ESP32 | osciloscopio en TP1 y en EN | EN sube **después** de que `+3V3` pasa 3,0 V |

## Resultados

Anotalos acá, con la fecha y la placa (DS_xxxxxx).

| # | fecha | placa | lectura | decisión tomada |
|---|---|---|---|---|
| | | | | |
