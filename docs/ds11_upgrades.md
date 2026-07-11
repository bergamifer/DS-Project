# DS11 — Recomendaciones de Upgrade (placa de producción)

**Base:** hallazgos del bring-up DS10 ([ds10_bringup_findings.md](ds10_bringup_findings.md))
**Criterio:** DS11 es candidata a placa de producción — debe salir funcionando sin bodges ni sorpresas.
**Última revisión:** 2026-07-02 (agregada revisión de netlist DS11 con Fable — ver sección siguiente)

---

## Revisión de circuito DS11 vía netlist (Fable, 2026-07-02)

**Fuente:** análisis del netlist KiCad exportado ([hardware/DS/DS.txt](../hardware/DS/DS.txt), export 2026-07-02, 606 componentes / 610 nets, sheet "Master DS11"), asistido por IA (modelo Fable).

> **Actualización 2026-07-03 — estado verificado contra el esquemático committeado** (`hardware/DS/AK5538.kicad_sch`, commit `e415806`):
> - ✅ **Acople DC implementado**: Cc 2.2µF ×16 (C294-C309), uno por línea de entrada
> - ✅ **R serie implementada**: Rs 470Ω ×16 (R115-R130) — límite <10mA del AKM cubierto
> - ✅ **R de amortiguación MCLK→AK5538 implementada**: R131 = 33Ω (verificar en netlist fresco que quede en serie con IC3.24)
> - ❌ **Cap diferencial 1nF P↔N: pendiente** (única pieza del front-end que falta, prioridad media)
> - El checklist pre-fabricación unificado vive en [hardware/DS/DS11_PLANNING.md](../hardware/DS/DS11_PLANNING.md) — este documento queda como archivo de hallazgos/racional.

### ~~🔴 Crítico~~ ✅ RESUELTO POR DECISIÓN (2026-07-02) — Bias/alimentación para micrófonos en J9

Los 4 RJ45 (J9) no tienen ningún rail hacia el conector. Con el acople DC (Cc), el auto-bias interno del AK5538 (~1.25V) queda del lado del chip y no puede polarizar un electret pasivo. **Decisión del usuario: las entradas son de NIVEL DE LÍNEA (fuentes activas/autoalimentadas — mezcladora, PC, pods con alimentación propia), no cápsulas pasivas → no hace falta bias y no se agrega.** Si a futuro se quisiera soportar electret pasivo directo: R ~2.2kΩ a 3.3VA del lado J9 del cap. Nota práctica para fuentes desbalanceadas: referenciar la pata N del par a masa local por el mismo camino Cc+Rs (no dejarla flotando).

### ~~🔴 Crítico~~ ✅ RESUELTO POR DISEÑO (2026-07-03) — Polaridad alternada entre canales pares/impares

Verificado en los 64 nets DAC→PAM→JSPK: el amp del canal impar de cada banco recibe VOUT+ en IN− (inversión), y su OUT− termina en el pin 1 del conector JSPK (doble inversión → en fase); el canal par entra derecho y su OUT+ va al pin 4. Si el arnés de parlantes asume polaridad uniforme (1=+, 2=−, 3=+, 4=−), la mitad de los parlantes queda en contrafase y destruye el patrón de beamforming. **Aclaración del usuario (2026-07-03): es una doble inversión deliberada — al JST del parlante llegan +/− en la polaridad correcta. El pinout JST "+ − − +" lo definen los parlantes reciclados usados. No es un bug.** El channel walker / AUTOFOCUS lo verificará empíricamente de todas formas.

### ~~🔴 Crítico~~ ✅ ACLARADO (2026-07-03) — Mapeo canal lógico → conector físico

VC (U1/DAC2, SAI2_A ch16-23) tiene un swap: ch1-2→JSPK10 y ch3-4→JSPK09 (invertidos respecto al resto). El usuario confirma que el orden de los JSPK es parte del diseño del arnés (junto con la polaridad de arriba). El firmware valida el mapeo real con el channel walker, y el AUTOFOCUS mide delays/polaridad por parlante directamente — la tabla física se carga desde esa medición, no desde suposiciones. Direcciones I2C de los 4 PCM1690 confirmadas por strapping y coinciden con firmware (U2=0x98, U3=0x9C, U1=0x9A, U4=0x9E) ✓.

### 🔴 Crítico — MP2315S (buck 24V→5VA) en su límite absoluto de entrada

24V nominal contra 26V máx operativo del MP2315S — margen cero ante cualquier transitorio de cable largo. Los 32 PAM8302A en fase (justo lo que hace un beam) pueden pedir más de los 3A/15W del buck, y un brown-out arrastra todo el sistema (MCU/ADC comparten el mismo rail derivado). Mitigación sin costo para DS11: alimentar a 19-20V en vez de 24V, y/o limitador de potencia global en firmware. Para DS12: buck de 36V/5-6A (MP2338, MPQ8633).

### 🟡 Importante — Front-end de entrada: falta capacitor diferencial

Solo hay 100pF de modo común por pata (a GND); el mismatch entre esos dos caps convierte modo común→diferencial en RF, degradando CMRR. Agregar ~1nF diferencial (P↔N) pegado al ADC, además del acople DC y R serie ya planeados (ver sección de nivel de salida más abajo — la parte de acople DC + R serie ya fue confirmada para agregar en cada placa).

### ~~🟡 Importante~~ ✅ IMPLEMENTADO (2026-07-02) — Rama MCLK del ADC sin terminación serie

Las 4 ramas de MCLK a los DACs tienen 62Ω de amortiguación; la rama al AK5538 (IC3.24) colgaba directa. **Implementado: R131 = 33Ω** (verificado en `AK5538.kicad_sch`). Regla de colocación acordada: la R va pegada al punto donde la rama se separa del tronco de MCLK (no pegada al chip receptor) — verificar el placement en layout, y el valor final se ajusta con scope una vez armada la placa.

### 🟡 Importante — PLL3 fraccional mete jitter en el MCLK compartido

`FRACN=6660` genera spurs que son el techo de rendimiento real del ADC (107dB nominal). Aceptable para voz/PA; no para hi-fi. La placa ya prevé el escape: JP2 permite desconectar PE2 e inyectar MCLK externo desde J5.7 — un oscilador XO de 24.576MHz ahí sería el upgrade limpio para DS12 (requiere firmware en modo MCLK externo).

### 🟡 Importante — +5VA_DAC comparte rail con los 32 amplificadores class-D

Solo separado por ferrite (FB3). El ripple de conmutación de los PAMs a volumen alto puede modular la referencia analógica de los 4 PCM1690. Acción para DS11 física: medir ripple en +5VA_DAC bajo carga; si aparece en el ruido de salida, subir bulk local (100µF) cerca de FB3.

### 🟡 Importante — SNR: el PAM8302A sigue siendo el cuello de botella

Reconfirma el hallazgo ya documentado abajo (sección "Nivel de salida"): -24dB digital para no clipear cuesta ~20dB de rango dinámico del DAC. Un divisor resistivo post-acople (aprovechando los 1kΩ serie ya existentes) recuperaría ~20dB sin re-spin — ítem B4 más abajo.

### 🟢 Confirmado correcto (no tocar)

- ESD PRTR5V0U2X con riel a 3.3VA (no 5V) ✓, filtro RF 470Ω+100pF → 3.4MHz ✓ (más el acople DC + R serie ya decididos)
- Dominio único de MCLK (mejora estructural real vs DS10): todo frequency-locked a PLL3, la preocupación cross-domain SAI1/SAI3 queda resuelta por diseño
- VREF del AK5538 (20Ω + 100µF∥0.1µF) sigue el reference design AKM exacto
- BOOT0: botón K3 + pulldown R14 correctamente wireado
- Buses de control (AMUTEI/RST/SD) sin conflictos de pin — los errores de DS9/DS10 no se repiten
- Anti-pop de boot: `/ALL_AMPS/SD` con pulldown → los 32 amps arrancan apagados, firmware los habilita al final (correcto)
- Filtro de reconstrucción DAC (1kΩ+4.7nF, ~34kHz) y snubber de salida (ferrite+220pF) — sin cambios necesarios

### 🟡 A revisar en layout (no visible en netlist)

- Shield de J9 a GND de señal directo — con pods remotos podría armar lazo de tierra audible; si aparece hum, cortar el shield en el pod o pasar a RC a chasis en DS12
- Retornos de los 32 PAMs no deben cruzar la isla analógica del ADC/DACs
- AMUTEO wired-OR (1kΩ por DAC a PC10): solo válido si los 4 PCM1690 quedan en open-drain; si no, PC10 ve un divisor ambiguo — confirmar configuración o leer estado por I2C

---

## Decisión clave: strapping del AK5538 — DEJARLO COMO ESTÁ ✅

**Pregunta:** ¿re-cablear los straps del AK5538 para que arranque en TDM256 sin I2C?

**Recomendación: NO tocar el hardware. Mantener la configuración por I2C.**

Razones:
1. **Los pines de strap CKS0/CKS2 comparten físicamente el bus I2C3** (CKS2/SCL y CKS0/SDA son el mismo pin). Los pull-ups del bus (necesarios para I2C) los fuerzan a 1 en el momento del release de PDN. "Arreglarlo" por hardware exigiría aislar los straps del bus durante el power-up (buffers, secuenciamiento RC, o mover el control port a SPI) — complejidad y riesgo de un nuevo error de spin, para ganar nada.
2. **La config por I2C ya funciona y es superior**: verificable por readback en cada boot, reproducible, y deja la puerta abierta a usar ganancias digitales, HPF y filtros del chip (que de todos modos se programan por I2C).
3. **El chip resetea sus registros con cada ciclo de PDN** → el estado es siempre conocido. El firmware re-aplica TDM256 en cada boot (3 writes, <1ms).
4. Un strap mal interpretado fue justamente uno de los errores del design review — no repetir el patrón.

**Lo único para DS11:** documentar en el esquemático que el control port I2C es **obligatorio** para la operación (no opcional), y mantener los pull-ups de I2C3 en 4.7kΩ.

---

## Nivel de salida DAC→PAM — RESUELTO POR FIRMWARE (atenuación digital del PCM1690)

**Problema (hallazgo #5):** el PAM8302A (+24dB fijo, rail 5V) clipea con entradas
> ~630mVpp ≈ **-22dBFS del DAC**.

**Solución adoptada (sin componentes):** la **atenuación digital integrada del
PCM1690** (registros 72-79, per-canal, pasos de 0.5dB). El firmware setea −24dB
en el init (`PCM1690_ATTEN_DEFAULT`), dejando que una señal digital full-scale
quede justo bajo el clipping del amp. **No requiere cambios de hardware en DS11.**

**Sobre el SNR (corrección de una afirmación previa exagerada):** la atenuación
digital baja el SNR del DAC ~1:1 (el piso de ruido analógico del PCM1690 es ~fijo),
de 113dB a ~89dB con −24dB. Un divisor *analógico* después del DAC preservaría el
SNR del DAC. **PERO** el cuello de botella del sistema es el PAM8302A (~90dB SNR),
así que el divisor analógico solo ganaría **~6dB reales** de SNR de sistema (no 20dB).

**Recomendación:** mantener la atenuación digital (gratis, per-canal, sirve como
control de volumen/balance futuro desde el ESP32). El divisor analógico pasa a
**opcional/baja prioridad** (ver B4) — solo vale la pena si DS11+ migra a un
amplificador de mejor SNR que el PAM8302A.

## PRIORIDAD ALTA

### A2. D8 — TVS correcto (SMBJ24A)

Error de fabricación en DS10 (vino SM6T15A, 15V, sobre el rail de 24V). Para DS11: **fijar SMBJ24A en BOM con nota de no-sustitución** y verificación visual post-ensamble. (Mientras se use 12V de entrada, el SM6T15A actual no es problema.)

### A3. Silkscreen anti-rotación para los PCM1690

Las 3 placas DS10 vinieron con U1/U3/U4 rotados 180° (error JLCPCB pese a CPL correcto). Para DS11:
- Marca de pin-1 **grande y asimétrica** en silkscreen (no solo un punto)
- Contorno del chip con esquina biselada dibujada
- Nota en la orden de ensamble pidiendo verificación fotográfica de orientación de los 4 HTSSOP-48 antes del reflow

### A4. LED de diagnóstico en GPIO directo

DS10 no tiene ningún LED operable desde un HardFault handler (los 4 están en TIM3 PWM — un crash no puede togglearlos). Para DS11: **1 LED en un GPIO simple** (cualquier pin libre, p.ej. PC13) para indicación de crash/estado de bajo nivel.

---

## PRIORIDAD MEDIA

### M1. Front-end de entradas del ADC

Con entradas flotantes, los 8 canales saturan de hum (alta impedancia + EMI). Para producción:
- Definir el front-end real según la fuente (micrófonos → bias/phantom según cápsula; línea → acople AC + divisor)
- **Terminación a bias para canales no usados** (resistencias a VCOM) para que no saturen
- Documentar el camino conector→AINxP/N en el esquemático (hoy solo constan los 100pF de RF)

### M2. C47: THT 1000µF → SMD polymer

Ya identificado en las known limitations del design review de DS10. Elimina el único componente de soldado manual.

### M3. Punto de inyección/medición para el bus MCLK

Un test point en el bus MCLK (post-R21) y otro en BICK/LRCK del ADC simplifican el debug de clocks — en DS10 hubo que medir sobre pines de IC.

### M4. Revisión de la cadena de alimentación para portabilidad

El MP2315S acepta 4.5-24V — funcionó con 12V sin cambios. Si la portabilidad con baterías es un caso de uso real (3S-6S LiPo = 11-25V), documentarlo como rango de entrada soportado oficial y dimensionar el conector/TVS acorde.

---

## PRIORIDAD BAJA / OPCIONALES

### B1. Alimentación de banco por USB (opcional, con cuidado)

Hoy el USB-C del ESP32 no alimenta la placa (correcto para producción). Para desarrollo sería cómodo poder programar/probar la parte digital sin la fuente de 24V. Si se desea: diodo Schottky de VBUS→5V con limitación, **solo si se analiza el backfeed** al MP2315S y la carga digital total (<500mA). Alternativa: no tocar nada y usar fuente de 12V compacta (ya soportado).

### B2. Pads para divisor de strap CAD del AK5538

La dirección I2C quedó en 0x11 (CAD0=1). Si alguna vez se quisieran 2 ADCs en el mismo bus, pads de 0Ω para CAD0/CAD1 darían las 4 direcciones. Hoy innecesario.

### B4. Divisor analógico DAC→PAM (solo si se mejora el amplificador)

Reclasificado desde "prioridad alta" — ver la sección de nivel de salida arriba.
Con el PAM8302A actual (~90dB SNR), la atenuación digital del PCM1690 ya resuelve
el clipping y el divisor analógico solo agregaría ~6dB de SNR de sistema, que no
justifica los componentes. Si en una revisión futura se reemplaza el PAM8302A por
un amplificador de mejor SNR (donde el DAC dejaría de estar sub-utilizado),
entonces sí conviene: ~-20dB por pata (R serie 1k + shunt ~110Ω), recalcular el
cap del LPF a ~47nF para mantener fc ≈ 34kHz, y subir la atenuación digital del
DAC de nuevo a 0dB.

### B3. Cambios que NO hay que hacer

- **EEPROM PB6/PB7**: el ruteo está perfecto — el problema era de firmware/doc (AF6, no AF11). No tocar.
- **Filtro RC de entrada del PAM**: correcto como está (la "señal sucia" era el bug de alineamiento SAI, no el filtro). Solo se recalcula si se implementa A1.
- **UART7 en J7**: funcionó perfecto como debug port vía ST-Link VCP. Mantener idéntico.
- **Control compartido DAC RST/MUTE (PC10/11/12)**: simplificación exitosa de DS10. Mantener.
- **Sincronización SAI1↔SAI2** (skew entre ch0-15 y ch16-31): es tema de **firmware** (SAI_GCR SYNCIN/SYNCOUT), no de PCB. Issue #14.

---

## Checklist de verificación pre-producción DS11

> **MOVIDO (2026-07-03):** el checklist pre-fabricación unificado y vigente vive en
> **[hardware/DS/DS11_PLANNING.md](../hardware/DS/DS11_PLANNING.md)** (§ "Checklist final
> pre-JLCPCB") — ahí se consolidaron los ítems de este documento, los gates anti-error de
> fabricación (lecciones de la tanda 1 de DS10), y las decisiones de la revisión de netlist.
> Este documento queda como **archivo de hallazgos y racional** — no mantener dos checklists.

---

*Recomendaciones DS11 v1.0 — derivadas del bring-up DS10 (mayo-junio 2026)*
*Ver [ds10_bringup_findings.md](ds10_bringup_findings.md) para el detalle de cada hallazgo*
