# Ronda 2 — cluster USB (A3, N2, N3, N4, PA5): informe

> Analista USB, 2026-10-07. Sólo esquemático y hojas. Sin refutar todavía.
> Scripts (todos con autoprueba y prueba negativa): `fig2_digit.py`, `usb_vbus.py`, `j3_ariete.py`; salidas en `*_salida.txt`.

## Mensaje final (resumen; el detalle está abajo)

**Veredictos**

| ítem | veredicto | sev. |
|---|---|---|
| A3/N4: el VBUS de J3 queda en ≈ 4,8 V sin cable | **CONFIRMADO**, a toda temperatura. Que la Mac se niegue a conectar: **ABIERTO** (M3) | 🟡 |
| N4 lo domina el pull-up de D+ del ESP32 a través de U6, no la fuga de D2 | **CONFIRMADO** por cálculo (M2 lo verifica) | 🟡 |
| 100 kΩ de descarga en J3 (la propuesta del plan) | **REFUTADO**: pasa vSafe0V a 60 °C | — |
| N2: PA5 ve un VBUS que no está | **CONFIRMADO**: desde 52–60 °C con la fuga típica, siempre con la máxima. Hoy es inofensivo | 🟡 firmware |
| PA5 e inyección | sin riesgo. La explicación del documento está mal | 🟢 doc |
| N3, ESP32 | **CONFIRMADO**: pull-up siempre activo, falla la prueba Back Voltage | 🟡 |
| N3, STM32 | depende del firmware. **Enclavamiento** nuevo | 🟡 |
| Golpe de ariete en J3 con la descarga | cubierto por 3 × C45783 (5,77 V) | 🟢 |

**Normas** (citas en §1):
- **Lo que se exige al dispositivo (nosotros):**
  - USB 2.0 §7.2.1, p.171: «No device shall supply (source) current on VBUS at its upstream facing port at any time» y
    «may not provide power to the pull-up resistor on D+/D- unless VBUS is present».
  - §7.1.5.1, p.141: el pull-up tiene que estar «derived from or controlled by» VBUS.
  - Type-C §4.5.2.2.3.1: «The port shall not drive VBUS».
  - La ECS v1.08 sólo nombra la prueba Back Voltage. El límite está en el procedimiento de USB-IF FS/LS Rev 1.3, Anexo F:
    15 kΩ en VBUS, D+ y D−, y **≤ 400 mV** en cada uno (Keysight 5988-6219 p.9 dice lo mismo).
- **Lo que se exige a la Mac (la fuente):** Type-C R2.5 §4.5.2.2.8.1–2, p.186: «shall apply a load to VBUS to ensure it
  reaches vSafe0V within tVsafe0V» (650 ms, Tabla 4-32). Si no llega, «may exit to ErrorRecovery or Disabled».
  vSafe0V = 0–0,8 V (PD R3.2, Tabla 4.8, p.84). **La norma no fija esa carga.**

**Evidencia**
- **Fig. 2 del SS34**, digitalizada por programa: a 4,8 V la fuga típica es de 1,4 / 4,8 / 24 / 82 / 175 µA a
  25 / 40 / 60 / 75 / 85 °C, y la máxima especificada de 181–448 µA.
  - **J3 hoy**: 4,80 V a toda temperatura, porque U6 sólo consume 10–150 nA.
  - **J8 hoy** (162 kΩ): 0,23 / 0,78 / 3,86 / 4,78 V con la fuga típica, y 4,8 V con la máxima.
- **N2.** PA5 es TT_ha (DS12110 p.69). Con la Tabla 60 (p.137), PA5 pasa 1,22 V (VIL de diseño) a 52 °C, 1,80 V a 57 °C y
  2,31 V a 60 °C. Hoy PA5 está en analógico (`main.c:4575`) y no hay pila USB.
- **PA5 e inyección.** Para un pin TT el límite es VIN ≤ **4,0 V**, no VDD + 0,3 (Tabla 21, p.104). Según la Tabla 22, nota 3
  (p.105), en estos pines «Positive injection is not possible». El «−0/0» de PA4/PA5 agrega que **tampoco se tolera
  inyección negativa**; la hoja no explica por qué, y que se deba al DAC es una inferencia mía. PA5 llega como máximo a
  **3,42 V** con 5,5 V de VBUS. Durante la rampa de `+3V3` pasa VDD + 0,3 V unos ms (τ = 3,8 ms): está permitido. **No hace falta
  otro divisor ni un clamp.** `DS12_USB_STM32.md` atribuye esto a «~40 µA por el diodo de protección», un mecanismo que
  contradice la Tabla 22.
- **N3.** El TRM del ESP32-C3 v1.4 dice en la p.175 «The USB pull-up value of GPIO19 is 1 by default», y en la p.775 el
  registro CONF0 arranca con DP_PULLUP = 1. El chip no tiene entrada de VBUS y el firmware usa la consola por USB.
  El diodo de D+ al pin 5 de U6 (modelo PSpice de ST) hace que, con la fijación de USB-IF, VBUS quede en **2,0–2,3 V** y D+ en
  **2,6–2,9 V**: **falla las dos**. Contra la descarga de la Mac (desconocida): 0,63 V con 0,5 kΩ, 0,99 V con 1 kΩ, 2,1 V con
  10 kΩ. Ese camino entrega 1–2 mA, mil veces más que la fuga de D2.
- **Enclavamiento en J8 (nuevo).** Con el pull-up del STM32 prendido y el host fuera, U9 sostiene el VBUS en 2,73 V. Eso
  deja PA5 = 1,69 V, en la zona indefinida, y el STM32 puede no enterarse nunca de que lo desenchufaron.

**Recomendación de esquemático**
1. **1 kΩ `C11702`** (73 en el BOM) de `/esp32-mini/VBUS` a GND y de `/stm32_custom/VBUS` a GND, del lado del conector.
   R161/R160/C50 quedan.
   - Sin contar a la Mac: 0,17 V a 85 °C con la fuga típica y 0,44 V con la máxima, siempre < 0,8 V. Back Voltage en VBUS:
     ≤ 0,17 V.
   - Consumo con cable: 4,4–5,5 mA y 19–30 mW. Pasa los 500 µA de suspensión, pero eso ya ocurría por S1.
   - Con 5,1 kΩ o 10 kΩ falla a temperatura alta.
2. **Pin 5 de U6 y de U9 a `+3V3`** (no a VBUS), con un `C1525` cada uno. Corta el camino de D+, y la ESD del dato queda
   recortada más bajo. C31 absorbe la ESD del VBUS (0,26 V con 8 kV).
3. PA5 con el divisor de hoy: 4,4 V → 2,70–2,74 V, que pasa el VIH de 2,31 V; 5,5 V → ≤ 3,42 V.
4. **Golpe de ariete en J3** (C31 arranca vacío), con `j3_ariete.py` sobre `a1_ncaps`: con 3 × C45783 el pico es de
   **5,73 V** con clamp y **5,77 V** sin clamp. Con 5,5 V llega a 5,97 V; hoy, sin los capacitores, 8,2 V.
5. Opcional, para cumplir en D+: un divisor de 62 k/100 k (`C37825`/`C25741`) a GPIO3 del ESP32.

**Firmware**
- PA5 como entrada `GPIO_NOPULL` (un pull de 40 kΩ rompe el divisor), sin el DAC ni el ADC.
- USB con VBDEN = 0, arranque en soft-disconnect y conexión sólo con PA5 alto durante ≥ 20 ms; desconexión inmediata cuando baja.
- ESP32, si se agrega el sensado: `PAD_PULL_OVERRIDE = 1` y `DP_PULLUP = 0` sin VBUS.

**Medida en la DS11.** J3 tiene la misma topología, pero D2 es un B5819W. Antes: confirmar que el autoarranque no esté
armado o desenchufar los parlantes.

| medida | lectura | decisión |
|---|---|---|
| M1: pad de C31 sin carga | ≈ TP4 − 0…50 mV | esperado (N4) |
| | < 0,4 V | el modelo está mal: revisar |
| M2: con 1 kΩ a masa | 0,8–1,3 V | camino de D+: el pin 5 a `+3V3` es obligatorio |
| | < 0,05 V | alcanza con el 1 kΩ |
| M3: la Mac con la placa ya encendida (×5, en frío y en caliente; control: cable antes) | la reconoce 5/5 | J3 pasa a 🟢 (el 1 kΩ de J8 va igual) |
| | falla y el control anda | arreglo de J3 obligatorio |
| | falla también el control | otro problema |

**Problemas nuevos:** el enclavamiento de J8; N4 no se arregla con una resistencia sola; cuatro errores en
`DS12_USB_STM32.md` (l.142–145 «sólo puede entrar», l.168–170, «Tabla 50» → 60 y «nunca arriba de VDD»); y el techo de
VBUS es 5,5 V, no 5,25.

---

## Topología verificada en el netlist (DS12, digest del 1-oct)

- `/esp32-mini/VBUS` (7 nodos): C31.1, F2.1, J3.A4/A9/B4/B9, U6.5. **Sin resistencia a GND.** F2.2 → D2.A; D2.K → `+5V_AUX`.
- `/stm32_custom/VBUS` (8 nodos): C55.1, F3.1, J8 ×4, R161.2, U9.5. F3.2 → D8.A; D8.K → `+5V_AUX`.
- `/stm32_custom/PA5` (4 nodos): C50.2 (100 nF), R160.2 (100 k a GND), R161.1 (62 k), U7.29.
- D+ del ESP32: U5.27 → U6.6 (I/O1) ≡ U6.1 → R12 33 Ω → J3.A6/B6 (+ C30 22 pF). D+ del STM32: PA12 (U7.71) → FL1 → U9.6 ≡ U9.1 → J8.A6/B6.
- DS11 (`dig11`): J3 → `/esp32-mini/VBUS` (C31, U6.5, J3) → **D2 = B5819W (`C8598`), sin PTC** → `+5V` (que viene del buck por D9 SS34). Misma topología (sin descarga), otro diodo.

Comando: `grep -E "VBUS|5V_AUX|PA5" .ds12_audit/redes.txt` y lo mismo en `dig11/`.

## 1. Normas (citas textuales)

| norma | texto | dónde |
|---|---|---|
| USB 2.0 §7.1.5.1 | «The voltage source on the pull-up resistor must be derived from or controlled by the power supplied on the USB cable such that when VBUS is removed, the pull-up resistor does not supply current on the data line to which it is attached.» | `usb_20.pdf` p.141 (pág. PDF 169) |
| USB 2.0 §7.2.1 | «No device shall supply (source) current on VBUS at its upstream facing port at any time. From VBUS on its upstream facing port, a device may only draw (sink) current. They may not provide power to the pull-up resistor on D+/D- unless VBUS is present (see Section 7.1.5). When VBUS is removed, the device must remove power from the D+/D- pull-up resistor within 10 seconds.» | p.171 (PDF 199) |
| USB 2.0 §7.2.1.5 | «Self-powered functions: May draw up to one unit load from VBUS to allow the USB interface to function when the remainder of the function is powered down.» | p.171 y p.174 |
| ECS v1.08 (abr-2026) | Sólo lista la prueba: «Back Voltage (for self-powered or battery powered upstream facing ports only)». **No da límite ni carga.** | `usb2_ecs.txt` p.2 |
| USB-IF FS/LS Compliance Test Procedure Rev 1.3, Anexo F | Origen del límite. F: «Section 7.2.1 … requires that no device shall supply (source) current on VBUS…». F.1: fijación con **R1, R2, R3 = 15 kΩ** de VBUS, D− y D+ a GND. F.3: «Measure and record DC voltages on Vbus, D+ and D-. Voltages should all be less than or equal to **400mV**», antes y después de enumerar y desenchufar. F.2: «All USB peripherals must perform the back-voltage test». Fallas reportadas (F): «The PC fails to cold boot due to back-drive voltage…», «Introduction of device/hub knocks out one or more upstream devices». | `hojas/web_usb/USB-IFTestProc1_3.pdf` pp. F-1 a F-3 (usb.org) |
| Keysight 5988-6219EN | «This test measures the DC voltages of VBUS, D+, and D- before and after device enumeration… Any voltage exceeding 400‑mV is considered a failure.» El límite sigue en uso: un informe de 2023 de QualiPHY (LeCroy) muestra «Test Criteria: x <= 400 mV» para VBUS, D+ y D−. | `web_usb/keysight_5988-6219.txt` p.9; `web_usb/fs_dev_alltest.txt` |
| PD R3.2 v1.2 (ago-2026) Tabla 4.8 | «vSafe0V 0 … 0.8 V Safe operating voltage at "zero volts"»; vSafe5V 4,75–5,5 V | `hojas/usbpd/…pdf` p.84 |
| Type-C R2.5 (mar-2026) p.35 | «vSafe0V VBUS "0 volts" as defined by the USB PD specification.» | `hojas/typec/` |
| Type-C §4.5.2.2.8.1 (AttachWait.SRC) | «The port shall apply a load to VBUS to ensure it reaches vSafe0V within tVsafe0V. (Note: The connected Sink port might have charge left in its capacitors at up to vSafe5V that will prevent the Source port from transitioning to Attached.SRC.)» | p.186 |
| Type-C §4.5.2.2.8.2 | «The port shall transition to Attached.SRC when VBUS is at vSafe0V and the SRC.Rd state is detected on exactly one of the CC1 or CC2 pins for at least tCCDebounce…». «The port may exit to ErrorRecovery or Disabled if the VBUS voltage has not reached vSafe0V within tVBUSOFF (Maximum) after entering the AttachWait.SRC state.» | p.186 |
| Type-C Tabla 4-32 | «tVsafe0V 0 / 650 ms: The time the Source is allowed to use for discharging the Sink capacitors to reach vSafe0V in AttachWait.SRC state in order to progress to Attached.SRC.» | p.247 |
| Type-C §4.5.2.2.3.1 (Unattached.SNK) y §4.5.2.2.4.1 (AttachWait.SNK) | «The port shall not drive VBUS or VCONN.» | pp.180–181 |
| Type-C §4.5.2.2.3.2 | «A USB 2.0 only Sink that doesn't support accessories and is self-powered … may transition directly to Attached.SNK when VBUS is detected.» | p.181 |

**Qué se le exige a quién.**
- **Al Sink (la DS12):** no «drive VBUS» sin estar conectado (Type-C), no «source current on VBUS at any time» (USB 2.0 §7.2.1), y no alimentar el pull-up de D+ sin VBUS (§7.1.5.1, §7.2.1). La prueba de certificación que lo mide pone 15 kΩ en VBUS, D+ y D− y exige ≤ 400 mV en cada uno.
- **Al Source (la Mac):** aplicar él mismo una carga a VBUS en AttachWait.SRC y llegar a vSafe0V (≤ 0,8 V) en ≤ 650 ms. Si no llega, *puede* irse a ErrorRecovery/Disabled: no entra en Attached.SRC, no da VBUS y no conecta datos. **La norma no fija el valor de esa carga**, así que si nuestra fuga la vence o no depende de la Mac.

## 2. Tensión de las redes VBUS sin cable, con 20 V (`usb_vbus.py`)

**Fig. 2 del SS34 digitalizada por programa** (`fig2_digit.py`): calibra los ejes con las líneas de la grilla de la imagen
(X: 6 verticales cada 131,6 px; Y: 5 décadas cada 118,4 px), sigue el color de las curvas sólidas y coincide con la lectura
a mano de A2 dentro de ±25 % (autoprueba). Prueba negativa: las curvas punteadas (SS38~) dan menos fuga, como dice la leyenda.
A 12 % de VR (4,8 V): 1,43 µA a 25 °C, 81,8 µA a 75 °C, 548 µA a 100 °C.

**Mecanismo.** D2 y D8 tienen el cátodo en `+5V_AUX` (4,8 V) y el ánodo en la red VBUS del conector. Su fuga entra a esa
red, que sube hasta que lo que entra iguala lo que sale. Al subir la red baja la tensión inversa del diodo, pero la fuga de un
Schottky casi no depende de esa tensión (la curva es chata entre 3 y 30 %). Por eso **la red llega casi a `+5V_AUX` salvo que
algo la cargue con más corriente que la fuga**.

| fuga de D2/D8 a 4,8 V | 25 °C | 40 °C | 60 °C | 75 °C | 85 °C |
|---|---|---|---|---|---|
| típica (Fig. 2) | 1,4 µA | 4,8 | 24 | 82 | 175 |
| máxima especificada (p.1, llevada a 4,8 V con la forma de la curva) | 181 µA | 229 | 314 | 397 | 448 |

| red VBUS sin cable (V), típica / máxima | 25 °C | 40 °C | 60 °C | 75 °C | 85 °C |
|---|---|---|---|---|---|
| **J3 hoy** (cargas: U6 10–150 nA, aislación de C31) | 4,80 / 4,80 | 4,80 / 4,80 | 4,80 / 4,80 | 4,80 / 4,80 | 4,80 / 4,80 |
| **J8 hoy** (162 kΩ) | 0,23 / 4,79 | 0,78 / 4,80 | 3,86 / 4,80 | 4,78 / 4,80 | 4,79 / 4,80 |
| J3 o J8 + **1 kΩ** (propuesta) | 0,00 / 0,18 | 0,00 / 0,23 | 0,02 / 0,31 | 0,08 / 0,39 | 0,17 / 0,44 |
| + 5,1 kΩ | 0,01 / 0,92 | 0,02 / 1,16 | 0,12 / 1,58 | 0,41 / 1,99 | 0,88 / 2,24 |
| + 10 kΩ | 0,01 / 1,81 | 0,05 / 2,27 | 0,24 / 3,08 | 0,80 / 3,88 | 1,72 / 4,35 |
| + 100 kΩ (`C25741`, la candidata del plan) | 0,14 / 4,79 | 0,48 / 4,79 | 2,40 / 4,79 | 4,77 / 4,80 | 4,79 / 4,80 |

**N4: CONFIRMADO** que el VBUS de J3 queda en ≈ `+5V_AUX` (4,8 V) a cualquier temperatura. La resistencia de 100 kΩ que
proponía el plan **no sirve**: pasa vSafe0V a 60 °C con la fuga típica y a cualquier temperatura con la máxima.

## 3. N2 — PA5 «ve» un VBUS que no está

- **PA5 es TT_ha** (pata 29, LQFP100; DS12110 Rev 10 p.69). TT = «3.3 V tolerant I/O»; `_a` = «analog option (supplied by
  VDDA)» (p.63). VDDA = `+3V3` por FB1 (netlist: `/stm32_custom/VREF+` = U7.20/U7.21 + FB1).
- **Umbrales** (Tabla 60, p.137, VDD = 3,3 V): VIL máx 0,3·VDD = 0,99 V (CMOS) y 0,4·VDD−0,1 = 1,22 V (por diseño);
  VIH mín 0,47·VDD+0,25 = 1,80 V (por diseño) y 0,7·VDD = 2,31 V (CMOS). Fuga del pin TT ±250 nA: despreciable.
  Pull-up/pull-down internos 30–50 kΩ (p.138): **si el firmware los activa, rompen el divisor** (con 40 kΩ a GND, 4,4 V → 1,39 V).
- **Resultado** (PA5 = V(J8) × 100/162): con fuga típica PA5 pasa 0,99 V a **50 °C**, 1,22 V a **52 °C**, 1,80 V a **57 °C** y 2,31 V a **60 °C**.
  Con la fuga máxima especificada, PA5 queda en 2,96 V **a cualquier temperatura**. **CONFIRMADO.**
- **Hoy no pasa nada:** el firmware deja PA5 en `GPIO_MODE_ANALOG` (`firmware/Core/Src/main.c:4575-4576`) y no tiene pila USB
  (ningún `PCD`/`OTG` en `main.c`, `.ioc`, `pin_definitions.h`). PA11/PA12 quedan en analógico: el STM32 nunca prende el pull-up de D+.
- **Con 1 kΩ en el VBUS de J8:** PA5 ≤ 0,11 V (típica) / 0,27 V (máxima) a 85 °C. Lee bajo con margen.

## 4. PA5 y la inyección de corriente

- **Qué dice la hoja.** Tabla 21 (p.104): «Input voltage on TT_xx pins: VSS−0.3 … **4.0 V**»: **no** es VDD + 0,3 V.
  Tabla 22 (p.105, y p.211 para rev V): «Injected current on FT_xxx, TT_xx, RST and B pins except PA4, PA5: −5/+0» e
  «Injected current on PA4, PA5: **−0/0**», con la nota 3: «Positive injection is not possible on these I/Os and does not
  occur for input voltages lower than the specified maximum value». Tabla 59 (p.137) agrupa PA4 y PA5 con PA0_C, PA1_C,
  PC2_C, PC3_C (los del switch analógico), PH4, PH5 y BOOT0: 0 mA en los dos sentidos.
- **Por qué −0/0:** la hoja no lo explica. **Inferencia** (no verificada): PA4/PA5 son DAC1_OUT1/OUT2 y tienen la opción
  analógica alimentada por VDDA. Cualquier corriente inyectada caería en esa estructura. La parte positiva no cambia nada
  (en los TT ya era +0); lo nuevo es que **tampoco se tolera inyección negativa**.
- **En esta placa:** la inyección negativa es imposible (el divisor viene de un VBUS ≥ 0; un pulso negativo lo absorben el
  diodo inferior de U9 y C50). La positiva no ocurre mientras PA5 ≤ 4,0 V. PA5 máximo con vSafe5V = 5,5 V (PD, Tabla 4.8) y
  resistencias al 1 %: **3,42 V < 4,0 V**. Pero pasa VDD: VDD = 3,20–3,36 V.
- **Mientras sube `+3V3`** (sólo-USB: U11 se alimenta del mismo USB, 4 ms de arranque suave): PA5 sube con τ = (62k‖100k)·C50 =
  **3,8 ms** y durante unos ms queda > VDD + 0,3 V. Para un TT la hoja lo permite sin condición de VDD, siempre que sea ≤ 4,0 V.
  Si existiera un diodo a VDD, por 62 kΩ pasarían como máximo (5,5 − 0,3)/62k ≈ **84 µA**, contra los 0 mA que admite PA5.
  ⚠️ `DS12_USB_STM32.md` (l.168–170) dice «entran ~40 µA por el diodo de protección del pin: inofensivo». **El mecanismo
  contradice la Tabla 22**, aunque la conclusión se sostiene por otra razón: es un TT y nunca pasa 4,0 V. El golpe de ariete
  de VBUS (S4.f) llega filtrado por τ = 3,8 ms: ΔPA5 ≈ 5 mV.
- **No hace falta otro divisor, otro pin ni un clamp.** Única regla: **no habilitar el DAC1 canal 2 ni el ADC en PA5.**
  Si se quiere salir de la clase PA4/PA5, hay pines FT libres (PE9 pata 39, PE10 pata 40, −5/+0 mA).
- **Otros errores de ese documento:** «Tabla 50» es la **Tabla 60** en la Rev 10. Y la frase «PA5 … nunca arriba de VDD» es
  falsa con 5,5 V de VBUS.

## 5. N3 — back-drive por D+

- **ESP32-C3: el pull-up de D+ está prendido siempre.**
  - El TRM v1.4 lo dice en la p.175: «The USB pull-up value of GPIO19 is 1 by default, therefore, the pin's pull-up resistor
    is enabled».
  - En la p.775, `USB_SERIAL_JTAG_CONF0_REG` arranca del reset con DP_PULLUP (bit 9) = 1 y USB_PAD_ENABLE (bit 14) = 1.
    PULLUP_VALUE vale «0: 2.2 K; 1: 1.1 K».
  - El chip no tiene entrada de VBUS. En la placa, la red `/esp32-mini/VBUS` no llega a ningún GPIO.
  - El firmware usa el USB Serial/JTAG como consola: `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`, en `firmware_esp32c3/sdkconfig`
    l.1391 y en `sdkconfig.defaults` l.8.
  - Conclusión: **no cumple §7.1.5.1 ni §7.2.1, y no hay firmware que pueda arreglarlo con el hardware de hoy.**
- **STM32 (OTG_FS):** el pull-up lo controla el firmware (soft-connect). Hoy no se usa. PA9 (`OTG_FS_VBUS`) está al aire,
  así que el sensado tiene que ser por PA5 y el firmware.
- **El camino.** U6 tiene un diodo de cada I/O al pin 5 («Dhigh» en el modelo PSpice de ST, Doc ID 11265 Rev 5 Fig.16–17;
  la Fig.15 muestra el pin 5 en un +VCC local). El circuito queda así: pull-up de D+ → U5.27 → U6.6 → diodo → U6.5 = VBUS de J3.

| J3 hoy, 25 °C | VBUS | D+ | límite |
|---|---|---|---|
| fijación USB-IF (15 kΩ en VBUS y D+) | **2,0–2,3 V** | **2,6–2,9 V** | 400 mV: **falla las dos** |
| sin cable, 1 kΩ a masa | 0,83–1,26 V | — | — |
| cortocircuito a masa | 1,2–2,4 mA | — | — |
| con la Mac en AttachWait.SRC, R_src 0,5 / 1 / 2 / 10 kΩ | 0,63 / 0,99 / 1,40 / 2,10 V | — | vSafe0V 0,8 V |

Rango = pull-up de 1,1 a 2,2 kΩ. La sensibilidad al IS del diodo (1e-15 a 1e-12 A) es ±0,05 V.

- **Esto domina sobre la fuga de D2** a temperatura ambiente: 1–2 mA contra 1,4 µA. N4 no es sólo la fuga.
- **Host apagado con el cable puesto:** entran hasta ~1,8 mA a 1–2,4 V al VBUS del host, más 0,2 mA por su D+. La norma
  lo prohíbe, y el Anexo F lista las fallas que causa: «PC fails to cold boot…».
- **J8 (N3 × N2), un enclavamiento nuevo.** Si el firmware de la DS12 prende el pull-up y el host se va, el camino por U9
  sostiene el VBUS en 2,73 V. Eso deja PA5 = 1,69 V, en la zona indefinida entre 1,22 y 1,80 V, y el STM32 puede no enterarse
  nunca de que se desenchufó. Con 1 kΩ: VBUS 1,05 V y PA5 = 0,65 V, que se lee bajo, y el firmware apaga el pull-up.
- **Arreglo propuesto: el pin 5 de U6 (y de U9) a `+3V3`, no a VBUS.**
  - El diodo sólo conduce si D+ pasa 3,3 + 0,5 V. Eso no pasa en señalización FS (VOH ≤ 3,6 V da < 50 nA).
  - **ESD:** la descarga positiva de D+ va a `+3V3`, que tiene decenas de µF, y el zener de 6 V queda en paralelo. El dato
    queda recortado más bajo que hoy (≈ 3,3 + VF en vez de 4,8–6 + VF): mejor para los pines de 3,3 V.
  - El VBUS se queda sin el zener de U6, pero C31 (4,7 µF) absorbe un IEC 8 kV de contacto (150 pF × 8 kV = 1,2 µC →
    0,26 V). ST pide 100 nF junto al pin 5 (Fig.18): agregar un `C1525` por chip. Su ubicación **depende del PCB**.
  - El golpe de ariete sin el clamp, con 3 × C45783: **5,766 V** (ver §6).
- **Lo que sigue sin cumplir:** D+ del ESP32 en 2,8 V sin VBUS. Para cumplir hace falta sensar VBUS en el ESP32 (§7).

## 6. Arreglo para N4/N2 y verificación

**Recomendación: 1 kΩ 0402 `C11702` (73 en el BOM) de cada red VBUS a GND, del lado del conector** (en
`/esp32-mini/VBUS` junto a C31, y en `/stm32_custom/VBUS` junto a C55; R161/R160 quedan como están), **más el pin 5 de U6 y U9 a `+3V3`**.

- **vSafe0V** sin contar la carga de la Mac: 0,17 V a 85 °C con la fuga típica y 0,44 V con la máxima: **cumple en todo el
  rango**. Prueba de USB-IF a 25 °C (1 kΩ ‖ 15 kΩ): 0,001 / 0,169 V < 0,4 V.
- **Valores más altos no alcanzan:** 5,1 kΩ falla a 85 °C con la típica (0,88 V) y a 25 °C con la máxima. 10 kΩ falla desde 75 °C.
- **Consumo con VBUS presente:** 4,4–5,5 mA por puerto, 19–30 mW en un 0402 de 62,5 mW. Pasa los 500 µA de suspensión
  (§7.2.3, p.176), pero eso ya lo violaba S1, donde D2/D8 llegan a tomar decenas de mA. Si se quisiera respetarlo: 10 kΩ
  `C25744`, que cumple vSafe0V sólo hasta ~70 °C con la fuga típica.
- **PA5 con el divisor sin cambios:** 4,4 V → **2,70–2,74 V** ≥ 2,31 V (CMOS) ✓. 5,5 V → 3,37–3,42 V ≤ 4,0 V (TT) ✓.
  Queda arriba de VDD, pero no importa en un TT.
- **El golpe de ariete en J3** (C31 arranca vacío) lo cubren los mismos 3 × C45783 de A1. J3 es pieza por pieza igual a J8
  (C31 = C55 `C19666`, F2 = F3, D2 = D8, U6 = U9). Con `j3_ariete.py` (reusa `a1_ncaps.caso`, peor pieza −20 %/85 °C,
  9 cables, C31 efectivo de 2–4 µF), el pico de `+5V_AUX`:

  | VBUS | clamp en VBUS | n = 0 (hoy) | n = 3 |
  |---|---|---|---|
  | 5,25 V | 0,5 Ω | 7,13 V | **5,73 V** |
  | 5,25 V | sin clamp | 8,23 V | **5,77 V** |
  | 5,5 V | 2 Ω | 8,12 V | 5,94 V |
  | 5,5 V | sin clamp | 8,63 V | 5,97 V |

  Con 5,25 V, tres capacitores cumplen el criterio de 5,8 V. Con 5,5 V queda en 5,97 V: debajo de los 6,0 V de operación del
  AP2125, pero arriba del criterio. Es lo mismo que A1 ya encontró para J8 (5,94 V), y si se quiere margen hay que ir a cuatro.

## 7. Medida en una DS11: lectura → decisión

**La topología de J3 es la misma en la DS11** (`dig11`): `/esp32-mini/VBUS` = C31, U6.5 y J3, sin resistencia a masa. Cambian
dos cosas: D2 es un **B5819W** (`C8598`), no un SS34, y va directo a `+5V` (TP4), sin PTC. El ESP32 tiene el mismo firmware,
con la consola por USB, así que el camino de D+ por U6 también existe.

**Seguridad.** No usar la API HTTP, no encender amplificadores, desenchufar los micrófonos. Antes de dar 20 V, confirmar
que esa DS11 **no tenga el autoarranque armado**: si lo tiene, enciende los 32 amplificadores a los 15 s (S2). Si no se puede
confirmar, desenchufar los parlantes. Todas las medidas son con el multímetro en tensión continua, punta negra a masa
(pad de masa de C31).

| # | medida (20 V puestos, J3 sin cable) | lectura | qué significa → decisión |
|---|---|---|---|
| M1 | pad de C31 que no es masa, sin nada más | ≈ V(TP4) − 0…50 mV (≈ 4,6–4,8 V) | **esperado**: la fuga de D2 lleva el conector al riel (N4). Seguir con M2 |
| | | 2,5–3,0 V | la fuga es chica y sostiene el pull-up de D+: N3 sí, N4 por fuga no. Seguir con M2 |
| | | < 0,4 V | no hay ni fuga ni camino de D+: **el modelo está mal**. No dibujar sin revisar (¿ESP32 sin firmware?) |
| M2 | lo mismo, con una resistencia de **1 kΩ** del pad a masa | **0,8–1,3 V** | el pull-up del ESP32 empuja VBUS por U6 (N3). **Pasar el pin 5 de U6/U9 a `+3V3` es necesario**: con el 1 kΩ solo quedaría ~1 V > vSafe0V |
| | | < 0,05 V | no hay camino de D+: alcanza con el 1 kΩ, y lo del pin 5 queda opcional |
| | | 0,05–0,8 V | la fuga del B5819W es alta o el pull-up es débil: anotar y decidir con M3 |
| M3 | con la placa encendida ≥ 1 min, enchufar la Mac por C a C; 5 veces, y otra vez con la placa caliente (≥ 30 min). **Control:** cable primero y 20 V después | la Mac reconoce el ESP32 5/5 y el control funciona | su descarga vence 1–2 mA a 25 °C: N4 **sin consecuencia con esta Mac en el banco**. El arreglo de J3 pasa a 🟢 (norma, otros hosts, calor). **El 1 kΩ de J8 va igual** (N2) |
| | | falla siempre o a veces, y el control funciona | N4/N3 con consecuencia: arreglo de J3 **obligatorio** (1 kΩ + pin 5 a `+3V3`) |
| | | falla también el control | otro problema (cable, puerto, firmware del ESP32): **no dice nada de N4** |

M3 no puede probar el caso de 85 °C ni la fuga máxima: eso lo cubre el cálculo.

## 8. Firmware

**STM32 (DS12):**
1. PA5 como entrada digital **sin pull** (`GPIO_NOPULL`): un pull de 40 kΩ rompe el divisor. Nunca el DAC1 canal 2 ni el ADC en PA5.
2. Si se agrega USB device: el sensado interno de VBUS va deshabilitado (VBDEN = 0, porque PA9 está al aire). Arrancar en
   **soft-disconnect** (`DCTL.SDIS = 1`, `HAL_PCD_DevDisconnect`) y conectar sólo con PA5 alto estable (≥ 20 ms; τ = 3,8 ms).
   Desconectar en cuanto PA5 baje: la norma da 10 s, conviene hacerlo en ms. Sirve la EXTI por los dos flancos o un sondeo
   cada 10–100 ms.
3. En una placa **sin** el 1 kΩ, PA5 no es confiable arriba de ~50 °C. Además, con el pull-up prendido el host «nunca se va» (§5).
4. Si algún día `boot_main.c` hace DFU por USB, la misma regla.

**ESP32:** hoy no se puede hacer nada. Con un sensado agregado, se resuelve así:
- **Hardware:** GPIO3, pata 6, libre y que no es strapping. Divisor 62 k/100 k, `C37825`/`C25741`. Con 4,4 V da 2,72 V
  ≥ 0,75·VDD = 2,475 V; con 5,5 V da 3,40 V ≤ VDD + 0,3 (MINI-1 v1.6, Tabla 10).
- **Firmware:** sin VBUS, `USB_SERIAL_JTAG_CONF0.PAD_PULL_OVERRIDE = 1` y `DP_PULLUP = 0`; con VBUS, restituir (TRM p.775).
  La ROM lo vuelve a prender en cada reset durante una fracción de segundo, dentro de los 10 s de §7.2.1.

## 9. Resumen

| ítem | veredicto | severidad |
|---|---|---|
| **A3/N4**: VBUS de J3 ≈ 4,8 V sin cable | **CONFIRMADO** (a toda temperatura). Que la Mac se niegue: **ABIERTO** → M3 | 🟡 |
| N4 lo domina el pull-up de D+ del ESP32 por U6, no la fuga de D2 | **CONFIRMADO** por cálculo (M2 lo verifica) | 🟡 |
| 100 kΩ de descarga en J3 (plan) | **REFUTADO**: pasa vSafe0V a 60 °C (típica) | — |
| **N2**: PA5 ve VBUS sin cable | **CONFIRMADO**: 52–60 °C con la típica, siempre con la máxima. Hoy inofensivo (PA5 analógico) | 🟡 (firmware DS12) |
| PA5 e inyección | sin riesgo: TT, ≤ 3,42 V < 4,0 V. El mecanismo del doc está equivocado | 🟢 doc |
| **N3** ESP32 | **CONFIRMADO**: pull-up siempre activo; falla Back Voltage en VBUS (2,0–2,3 V) y en D+ (2,6–2,9 V) | 🟡 |
| N3 STM32 | depende del firmware; **enclavamiento** con 162 kΩ (nuevo) | 🟡 |
| Golpe de ariete en J3 con descarga | cubierto por 3 × C45783: 5,77 V | 🟢 |

**Esquemático:** dos `C11702` (1 kΩ) de cada red VBUS a GND; el pin 5 de U6 y de U9 a `+3V3`, con un `C1525` cada uno;
R161/R160/C50 sin cambios. Opcional: 62 k/100 k (`C37825`/`C25741`) de `/esp32-mini/VBUS` a GPIO3.
**Depende del PCB:** la ubicación del 1 kΩ junto al conector, el 100 nF junto al pin 5 y la pista nueva a `+3V3`.

**Problemas nuevos:**
1. El enclavamiento N3 × N2 en J8.
2. N4 no se arregla con una resistencia sola: hay que sacar el pin 5 de VBUS.
3. `DS12_USB_STM32.md`:
   - l.142–145, «la corriente sólo puede entrar»: **refutado** en la escala µA–mA (fuga de D2 y camino por U6);
   - l.168–170, el mecanismo de «~40 µA por el diodo»;
   - «Tabla 50» es la Tabla 60;
   - «nunca arriba de VDD».
4. El techo real de VBUS es **5,5 V** (vSafe5V), no 5,25 V: con ese valor, S4.f con 3 capacitores da 5,97 V.

**Archivos:**
- En `r2/usb/`: `informe.md`, `fig2_digit.py`, `fig2_ss34.json`, `usb_vbus.py`, `j3_ariete.py`, `*_salida.txt` y `fig/fig2_crop.png`.
- En `hojas/`:
  - `web_usb/`: procedimiento USB-IF FS/LS Rev 1.3 (Anexo F), Keysight 5988-6219EN, informe QualiPHY y manual Siglent;
  - `typec/`: Type-C R2.5 y la ECN;
  - `usbpd/`: PD R3.2 v1.2;
  - `esp32c3/`: TRM v1.4.

  Todos con su `.txt`. `USBLC6-2.txt` (ST Doc ID 11265 Rev 5) ya estaba: lo bajó otro analista, sin el PDF.
