# DS11 — Plan de diseño y checklist

> **Objetivo**: corregir todos los errores detectados en DS10 (de diseño propio y de producción) y agregar mejoras de usabilidad para desarrollo.
> **Contexto**: DS10 tanda 1 (3 placas ensambladas, orden SMT026040962250-11787566A) resultó defectuosa por error de JLCPCB (3 de 4 DACs rotados 180°). Reclamo por crédito (~USD 399) en curso — el resultado define el presupuesto de la orden DS11. Detalle: [DS10_BRINGUP.md](DS10_BRINGUP.md) §6.

---

## 1. Correcciones obligatorias (errores DS10)

> **Estado al 2026-06-22**: el grueso del esquemático DS11 está implementado. ✅ = hecho, ⬜ = pendiente (mayormente layout / pre-fab).

| # | Problema en DS10 | Origen | Fix DS11 | Estado |
|---|---|---|---|---|
| 1 | **RJ45 ×4 orientados hacia el interior de la placa** (Issue #1) | Error de placement propio | Conector RJ45 ×4 integrado `RJ45_Amphenol_RJHSE538X-04` (datasheet `datasheets/rj45x4.pdf` = RJ-052-4-2, 3D `3d/RJ45x4.step`) orientado hacia afuera. **Verificar con 3D viewer + impresión 1:1 antes de Gerbers** | ✅ diseñado |
| 2 | **Daisy-chain 24V por barrel jack acumula corriente** (Issue #2) — 2.5A×N placas | Error de arquitectura | Terminal block enchufable **MSTBVA 2,5/2-G-5,08** (Phoenix / clon KF2EDGK 5.08 2P) entrada (J31) + salida (J32). **Clusters de 4 cajas por fuente** → máx 10A < 12A rating del MSTB. Machos enchufables se compran aparte (LCSC/AliExpress `KF2EDGK 5.08 2P`) — JLCPCB no los ensambla (no se sueldan) | ✅ encargado / ⬜ pistas 24V ≥2.5mm en layout |
| 3 | **D8 = SM6T15A (TVS de 15V) en riel de 24V** | Error de BOM | Value=SMBJ24A, LCSC=C726830 en el esquemático | ✅ |
| 4 | **USB-only no arranca** — inrush ~9A cargando 1.1mF de +5VA a través de FB2 | Limitación de diseño | **Schottky SS34 (C8678) en serie con FB2**: `+5VA → FB2 → D(SS34, ánodo +5VA) → +5V`. Bloquea USB→+5VA automáticamente. FB3/+5VA_DAC cuelga de +5VA → mismo corte aísla los DACs. Solo lleva rama LDOs (~0.5A). Ver §2 | ✅ |
| 5 | 6× ERC `power_pin_not_driven` | Cosmético | Agregar PWR_FLAG | ✅ |
| 6 | J31/J32 pin 3 (switch del barrel) sin uso | Mejora | N/A — se pasó a terminal block | ✅ N/A |
| 7 | Sin protección ESD en entradas RJ45 | Mejora | **PRTR5V0U2X (SOT-143, C5158049)**, 1 por par diferencial. Pinout: IO1(pin3)→IN+, IO2(pin2)→IN-, VCC(pin4)→+3V3, GND(pin1)→GND. **En AMBOS conectores RJ45** (cualquiera puede ser IN según cableado del daisy-chain) → 8 pares × 2 = **16 ICs**. **Colocar cerca de cada conector RJ45**, no del AK5538 (el TVS debe interceptar el pico antes del trace) | ✅ en esquemático / ⬜ placement |
| 8 | `netlist.md` desactualizado (no incluye R107-R110 de AMUTEO) | Doc | Regenerar netlist al cerrar el esquemático DS11 + actualizar Sección 11 del [DS10_DESIGN_REVIEW.md](DS10_DESIGN_REVIEW.md) | ⬜ al cerrar esquemático |
| 9 | 8× `endpoint_off_grid` en sheet root | Cosmético | Alinear wires a la grilla (ERC → click error → snap a grilla 50mil; o `Edit → Cleanup Schematic`) | ⬜ |
| 10 | Silkscreen de K1 dice "Reset ESP32" — antes era boot mode | Silk | **K1 rediseñado: ahora ES el reset real del ESP32** (pin EN + RC 10k+100nF, ver §3). El silk "Reset ESP32" quedó correcto. **Botón de boot (GPIO8) eliminado** — GPIO8 queda fijo HIGH con pull-up, USB CDC bootea siempre | ✅ |

---

## 2. Programación sin 24V (alimentación desde la PC)

**Decisión: alimentar por el pin 5V del ST-Link → riel +5V. NUNCA por el pin 3V3.**

Razones:
- Entrando por +5V, los dos LDO (U11 → +3V3 digital, U10 → +3.3VA analógico) levantan normalmente → la topología de power es idéntica a la operación normal, todos los rieles quedan vivos y testeables.
- Inyectar 3.3V directo al riel +3V3 back-alimenta la salida del AP1117 (riesgo de corriente inversa en el LDO) y deja +3.3VA muerto (el ADC no se puede probar).
- El pin 3V3 de los ST-Link (sobre todo clones) sale de un LDO interno chico (~100-200mA): insuficiente para STM32+ESP32 (~350mA en picos). El pin 5V es VBUS del USB casi directo (~450mA disponibles).

### Topología propuesta (tres fuentes de 5V OR-eadas por diodo, aislamiento automático)

```
Buck 24V→5V (+5VA) ──► FB2 ──► D_buck (SS34) ──┐
USB-C VBUS ──────────► D2 (Schottky) ──────────┼──► +5V ──► U11 (+3V3) y U10 (+3.3VA)
ST-Link 5V (J7/10) ──► D_stlink (B5819W) ──────┘
```

- **D_buck (Schottky en serie con FB2, ánodo en +5VA)**: deja pasar buck→LDOs en operación normal y bloquea USB/ST-Link→+5VA. El aislamiento de los PAMs es **automático, sin jumper**. Como FB3/+5VA_DAC cuelga de +5VA, el mismo diodo aísla también el AVDD de los DACs y el back-feed al buck. Corriente: solo la rama de LDOs (~0.3-0.5A) → SS34 sobra. Caída ~0.4V → +5V queda en ~4.6V con 24V (AP1117-33 necesita ~4.3V: margen OK; los pines +5V de J5/J6 quedan en ~4.6V — aceptable). Opcional: solder jumper en paralelo a D_buck como bypass.
- **D_stlink (B5819W) en serie con el actual JP3** (+5V → J7/10): hoy, con JP3 cerrado y 24V presente, el buck back-alimenta el VBUS del ST-Link. Con el diodo, cualquier combinación de fuentes es segura. ✅ implementado.
- **VTREF del ST-Link (J7/1)**: el antiguo JP4 (jumper abierto) → conectar **directo a +3V3**, sin jumper. VTREF es solo referencia de tensión (alta impedancia, sin back-feed, sin consumo); siempre quiere estar conectado. ✅ implementado.
- Alimentando por USB-C o ST-Link: el lado digital carga solo ~30-60µF de inrush → no dispara OCP.
- Presupuesto de corriente programando: STM32 ~150-200mA + ESP32 idle ~50mA (bursts WiFi 300mA) + lógica DACs/ADC → OK para programar y debuggear; **evitar WiFi activo + pipeline de audio si se alimenta solo por ST-Link**.
- ⚠️ **Verificar en datasheet PCM1690**: alimentando por USB los DACs quedan con VDD=3.3V y AVDD=0V. Los CMOS de TI suelen tolerarlo (sin path de latch-up), pero confirmarlo antes de fijar la topología.
- **Bodge equivalente en DS10**: desoldar FB2 y soldar un B5819W sobre sus pads — ánodo al pad 1 (+5VA), cátodo al pad 2 (+5V). Se pierde el ferrite (OK en desarrollo).

---

## 3. Nuevas features (lecciones del bring-up tanda 1)

- **LED de power hardwired en +3V3**: R + LED directo al riel, sin firmware. **R = 470Ω** (LED verde Vf≈2.0V → If≈2.8mA; bajar a 330Ω si se quiere más brillo). En la tanda 1 no había NINGUNA señal visible de vida (los 5 LEDs son de firmware) y eso complicó el diagnóstico del primer encendido. ✅ implementado.
- **Test points**: +24V, +5VA, +5V, +3V3, +3.3VA, GND ×2, MCLK (PE2), BICK (PD8). Footprint `TestPoint:TestPoint_Pad_1.0x1.0mm` o 1.5×1.5mm. ✅ implementado.
- **LEDs verdes del conector RJ45×4** (2 LEDs por puerto, 8 en total): los 4 verdes encendidos como indicador de "puerto vivo", controlados por **STM32 PE12** (no PB2). **4× R 470Ω individuales** (una por LED → distribución uniforme; If≈2.8mA c/u, 11mA total < 25mA del GPIO). ✅ implementado.
- **Reset hardware del ESP32 en K1**: pin EN (CHIP_EN) con red **RC 10kΩ a +3V3 + 100nF a GND** + botón K1 a GND. El RC filtra glitches y garantiza reset limpio al soltar. (DS10 no tenía reset para el ESP32). ✅ implementado.
- Mantener el **gate de bring-up** como procedimiento estándar para cada tanda nueva: medir rieles a GND + verificar orientación de ICs ANTES de energizar.
- **(Idea, no implementada) Link al repo en silk**: QR en `F.Silkscreen` (KiCad 9: `Edit → Add → QR Code`, ~15×15mm) apuntando a un redirect que se controla (ej. `bergamifer.github.io/ds` → repo real). Así el link es actualizable sin rediseñar la placa.

---

## 4. Proceso de fabricación — gates anti-error (lecciones tanda 1)

1. **DFM Analysis 3D de JLCPCB** (botón "DFM Analysis" en la orden, NO el preview de compra): verificar orientación de pin 1 de **todos** los ICs (especialmente U1-U4 PCM1690, IC3, U7) **antes de aprobar producción**. JLCPCB admitió que el preview "no es 100% preciso"; el DFM 3D muestra los archivos de producción reales que genera su equipo de ingeniería — ahí fue donde se introdujo la rotación de los DACs.
2. **BOM por C-number**: verificar que cada LCSC part number corresponde al Value del esquemático. El SM6T15A (C118275) donde iba un SMBJ24A pasó todos los filtros anteriores.
3. **Rotation corrections del P&P**: revalidar para los footprints **nuevos** (RJ45×4, USB-C vertical, terminal block) — los footprints nuevos son los de mayor riesgo.
4. Mantener: Confirm Production File = Yes, Confirm Parts Placement = Yes, Photo Confirmation = Yes.
5. Al recibir: gate §0 del bring-up antes de energizar cualquier placa.
6. Si hay defecto: **fotos + reclamo ANTES de modificar las placas**.

---

## 5. Conectores en evaluación (trabajo en curso)

| Función | Candidato | Estado | Material ya en repo |
|---|---|---|---|
| RJ45 audio | RJHSE538X-04 (bloque ×4 integrado) | ✅ elegido e implementado | footprint en `librerias/DS_local_library.pretty/`, `datasheets/rj45x4.pdf`, `3d/RJ45x4.step` |
| USB-C | G-Switch GT-USB-7051x (vertical) | en evaluación | footprint en `librerias/DS_local_library.pretty/`, `datasheets/usb_vertical.pdf` |
| Power 24V | **MSTBVA 2,5/2-G-5,08** (Phoenix / clon KF2EDGK) — hembra PCB; macho KF2EDGK 5.08 2P aparte | ✅ elegido y encargado | `3d/.../PhoenixContact_MSTBVA_2,5_2-G-5,08_1x02_P5.08mm_Vertical.step`, `datasheets/barrel vertical.pdf` |

Para cada footprint nuevo: verificar pin 1 y dimensiones contra datasheet, agregar a la tabla de rotation corrections, y probar con impresión 1:1 del layout.

---

## 6. DS12 — Interconexión digital entre placas (análisis preliminar)

**Problema actual**: los 8 canales viajan analógicos diferenciales por Cat5e en daisy-chain pasivo. El ruido captado por el cable se acumula a lo largo de la cadena (18 cajas).

**Pregunta**: ¿se pueden tirar los 8 canales digitales con S/PDIF o similar?

| Opción | Canales/link | Hardware extra por caja | Veredicto |
|---|---|---|---|
| **A. TDM re-driven sobre LVDS/RS-485** | 8 (TDM256 = 12.288 Mbps en 1 par); Cat5e lleva MCLK + BICK + LRCK + DATA en sus 4 pares | Drivers/receivers LVDS (~USD 2-3) + re-driver | **Recomendada.** Cada caja regenera la señal en hardware (repeater, el MCU no está en el data path) → **cero acumulación de ruido**: lo que llega a la caja 18 es idéntico a lo que salió de la 1. MCLK viaja en su propio par → se preserva la filosofía de clock compartido y la coherencia de fase del beamforming |
| B. S/PDIF | 2 por link → 8ch = 4 links | 4× TX + 4× RX, clock recovery por link | Descartada como backbone: 4 enlaces por tramo, jitter por hop, 4× el trabajo de firmware. (El SPDIFRX del H743 queda útil como entrada auxiliar de fuentes externas — ya está en J6) |
| C. Ethernet 100M + switch 3 puertos (ej. KSZ8863) | Ilimitados (8ch = 12.3 Mbps de 100M) | PHY/switch + magnetics; firmware UDP/RTP + sincronización tipo PTP | La opción "pro" y más escalable (audio + control + más canales en un solo cable estándar), pero el esfuerzo de firmware es grande y la coherencia de fase pasa a depender de PTP — se pierde la simplicidad del MCLK compartido. Candidata a largo plazo |
| D. ADAT (8ch por TOSLINK) | 8 | FPGA/CPLD (el H7 no tiene periférico ADAT) | Descartada: complejidad sin beneficio frente a A |

**Notas de la opción A:**
- El stream TDM que ya genera el sistema (misma trama que el AK5538) se transmite tal cual; cada caja lo escucha con un SAI en slave RX y lo re-transmite buffereado a la siguiente.
- Budget de pines: SAI3_A ya está expuesto en J5 (PD0/PD1/PD4) y puede ser el TX hacia la caja siguiente; el RX requiere un SAI slave libre (evaluar disponibilidad de SAI4 en LQFP100, o mux con SAI3_B/AK5538).
- El AK5538 queda para entradas analógicas locales (micrófonos), independiente del bus digital.
- Latencia por hop: ~ns (buffer LVDS); si se re-timea con flip-flop, 1 ciclo de BICK por caja — determinístico y compensable por índice de caja en firmware.
- Costo aproximado: 2× transceiver LVDS/RS-485 + conectores = pocos dólares por caja.

---

## 7. Orden sugerido de trabajo hacia DS11

1. ✅ Cerrar reclamo JLCPCB — **resuelto: USD 355 a JLC Balance** (ver `memory/project_ds10_production_defect.md`).
2. ✅ Esquemático: D8→SMBJ24A, SS34 en FB2, B5819W en ST-Link, VTREF directo, PWR_FLAG, K1=reset ESP32 (boot eliminado), LED power, test points, LEDs RJ45 en PE12, TVS PRTR5V0U2X en pares diferenciales.
3. ⬜ **Layout (lo que queda)**: placement de los 16 TVS pegados a cada RJ45; pistas/planos 24V ≥2.5mm entre J31-J32; verificar RJ45 abre hacia afuera (3D + impresión 1:1).
4. ⬜ Limpiar `endpoint_off_grid` (#9), regenerar netlist + ERC/DRC limpios + actualizar docs (#8).
5. ⬜ Gates de fabricación (§4) — tabla de rotation corrections para footprints nuevos, DFM 3D, BOM por C-number.
6. ⬜ Pedido con el crédito de USD 355 aplicado.

---

## 8. Checklist final pre-JLCPCB (consolidado 2026-07-03)

> Unifica: este plan (§1-§5), la revisión de netlist asistida (hallazgos y racional en
> [docs/ds11_upgrades.md](../../docs/ds11_upgrades.md)), y las decisiones tomadas en las
> sesiones de firmware de julio 2026. Verificado contra el esquemático committeado
> (commit `e415806`) donde se indica. **Este es EL checklist — no mantener otros.**

### 8.1 Decisiones de esquemático — YA APLICADAS ✅ (verificadas en `AK5538.kicad_sch`)

- [x] Acople DC por línea de entrada: **Cc 2.2µF ×16** (C294-C309)
- [x] R serie limitadora por línea: **Rs 470Ω ×16** (R115-R130) — cumple <10mA AKM
- [x] R amortiguación rama MCLK→AK5538: **R131 = 33Ω**
- [x] ESD **PRTR5V0U2X ×16** (ambos RJ45 de cada par) + filtro RF 100pF ×16 (C274-C289)
- [x] D8 = SMBJ24A (C726830) en esquemático — reconfirmar en BOM final (§8.4)
- [x] SS34 en serie con FB2, B5819W en ST-Link 5V, VTREF directo (topología 3× 5V OR-ead)
- [x] BOOT0: botón K3 + pulldown — SE MANTIENE (decisión 2026-07-02, "por las dudas")
- [x] LED power hardwired, test points, LEDs RJ45 en PE12, reset ESP32 en K1

### 8.2 Decisiones de esquemático — PENDIENTES antes de cerrar ⬜

- [ ] **Cap diferencial 1nF P↔N por par de entrada** (×8, pegado al ADC) — único faltante
      del front-end; mejora CMRR en RF. Prioridad media: decidir agregar o aceptar el riesgo.
- [x] ~~🔴 Polaridad alternada pares/impares + swap JSPK09/10~~ **RESUELTO POR DISEÑO
      (aclaración del usuario, 2026-07-03)**: la "inversión" del canal impar es una **doble
      inversión deliberada** — el JST del parlante recibe +/− en la polaridad correcta. El
      pinout del JST está definido por los parlantes reciclados que se usan: **"+ − − +"**.
      No es un bug: es la convención del arnés. El firmware queda con `speaker_sign[32]`=+1
      y el AUTOFOCUS lo verificará empíricamente de todas formas (mide polaridad real).
- [ ] **Footprint/pads para bulk extra (100µF) cerca de FB3** (+5VA_DAC): seguro barato
      contra el ripple de los 32 class-D sobre el rail analógico de los DACs — se puebla
      solo si la medición post-armado lo pide.
- [ ] Consistencia de docs: §1.7 de este plan dice PRTR VCC→+3V3, el netlist los tiene en
      +3.3VA — confirmar cuál es el conectado real y corregir el texto (funcionalmente ambos
      sirven; definir a qué rail se quiere derivar el surge).
- [ ] Nota en esquemático: "AK5538 requiere config por I2C en cada boot: CKS=512fs (R02=0x31)
      + TDM256 (R03=0x40), addr 7-bit 0x11" — lección del bring-up DS10 (root cause CKS).
- [ ] **Tap de línea VOUT (nuevo 2026-07-05, multi-nodo)**: header 2 pines **DNP** desde
      VOUT ch0 del PCM1690 (antes del PAM) vía **R serie ~1 kΩ** + GND. Tres usos con un
      cable patch a una entrada de J9: (1) **self-test** de toda la cadena DAC→ADC con el
      chirp/correlación del AUTOFOCUS; (2) verificación empírica de la latencia del nodo;
      (3) **"oscilador en la cabecera"**: el nodo 1 genera contenido y lo inyecta al
      daisy-chain analógico → los 15 nodos lo beamformean coherente (resuelve la deriva
      del contenido generado internamente). Ver docs/multinode.md §2. Costo: 2 pads + 1 R.
- [ ] 🔴 **Keepout de antena del ESP32-C3-MINI-1 (U5) — VERIFICAR EN LAYOUT** (habilita
      BLE-MIDI + ESP-NOW, que son solo firmware sobre la radio existente). U5 en (126, 40),
      a 5.4 mm del borde superior con la antena hacia el borde (bien ubicado). Confirmar que
      **NO hay cobre en la zona de antena del módulo en NINGUNA capa** — en particular que el
      plano **In1 (GND sólido) esté recortado** debajo de la antena. Un plano de masa entero
      bajo una antena de PCB destruye el alcance de WiFi *y* BLE. Cero costo si el pour ya
      respeta el keepout del footprint; solo hay que mirarlo. Ver docs/esp32_roles.md.
- [ ] **Opcional — línea de timing ESP32→STM32** (hedge gratis a nivel fab): rutear
      **GPIO4 (U5 pin 18) → PC13 (U7 pad 7)** con **R serie ~220 Ω** cerca del ESP32.
      Verificado en netlist (2026-07-07): PC13 libre, VBAT@+3V3 (PC13 usable como GPIO),
      EXTI13 sin conflicto; PC13 como entrada/EXTI con pull interno (activa-baja). **No es
      necesaria**: el UART GPIO0/1 ya es interrupt-driven en ambos sentidos, así que sirve
      solo para un futuro disparo de timing sin jitter. **NO rutear la inversa** (STM32→ESP32):
      el UART ya cubre esa notificación. Ver docs/esp32_roles.md §"Enlace con el STM32".

### 8.3 Decisión de sistema — alimentación (sin cambio de PCB)

- [ ] **MP2315S a 24V = margen cero** (26V abs max; el SMBJ24A clampea recién a ~26.7V).
      Mitigación elegida para DS11: **alimentar el sistema a 19-20V** (el buck y todo el
      diseño lo aceptan sin cambios) y/o limitador de potencia global en firmware.
      Documentar la tensión nominal elegida en la serigrafía/manual. Buck más grande = DS12.

### 8.4 Gates de fabricación (lecciones tanda 1 — NO SALTEAR)

- [ ] **DFM Analysis 3D de JLCPCB** (no el preview de compra): orientación pin-1 de TODOS los
      ICs — especialmente U1-U4 (PCM1690), IC3 (AK5538), U7. Acá se introdujo la rotación 180°.
- [ ] **BOM por C-number**: cada LCSC part number vs Value del esquemático (así entró el
      SM6T15A donde iba SMBJ24A).
- [ ] **Rotation corrections del P&P** para footprints nuevos: RJ45×4, USB-C vertical,
      terminal block MSTBVA.
- [ ] **RN1-4 (arrays 62Ω 4x0603) → 16 resistencias discretas de 75Ω** (decisión 2026-07-07:
      los arrays no están en stock). Cada RN es un array de 4 elementos **aislados** (no bus)
      que amortigua las 4 líneas SAI de un DAC → mapea 1:1 a 4 discretas (R1=SAI_FS, R2=SAI_SCK,
      R3=SAI_SD, R4=MCLK; ver ruteo en netlist). 62→75Ω OK en las 4 líneas: el RC extra en MCLK
      (24.576MHz) es ~0.75ns « 40.7ns de período. Plus: las discretas 75Ω 0402 suelen ser
      *basic part* (sin fee) y tienen menos riesgo de rotación que los arrays. **A cuidar en
      layout**: cabe en la zona densa de los DACs, y mantener las 4 líneas de cada DAC de
      largo/skew parecido (el array las igualaba por construcción). Verificar footprint pin-1.
- [ ] Silkscreen pin-1 **grande y asimétrico** en los 4 PCM1690 + AK5538.
- [ ] Confirm Production File / Parts Placement / Photo Confirmation = Yes.
- [x] Layout — **VERIFICADO por revisión de plots 600dpi (2026-07-03,
      `plots_png_600/`)**: In1 = plano GND sólido sin cortes ✓; In2 = islas de power bien
      particionadas (3V3 digital / analógica / +5VA amps) ✓; 24V daisy-chain J12↔J11 en
      pour ancho (≥8mm, sobra vs los 2.5mm requeridos) ✓; RJ45 abre hacia afuera en el borde
      inferior ✓; TVS (D10/D13...) pegados al conector ✓; R131 colocada en el tronco MCLK
      (regla de bifurcación) ✓; floorplan separa amps (derecha) de la analógica de entrada
      (abajo-izq) ✓; QR + labels en silk ✓. Queda para el 3D/impresión 1:1: verificación
      mecánica del RJ45×4 y los conectores nuevos.
- [x] **Netlist regenerado** (export 2026-07-03 18:02, committeado como `DS.txt`) — R131
      verificado en su net: pin 1 → `/ADC_AK5538/MCLK` (solo IC3.24), pin 2 → tronco
      `SAI1_MCLK` (JP2 + RN1-4). Topología exacta a lo acordado.
- [ ] ERC limpio (endpoint_off_grid §1.9) + DRC limpio.
- [ ] Al recibir: gate de bring-up §0 (rieles a GND + orientación de ICs) ANTES de energizar.
      Si hay defecto: fotos + reclamo ANTES de tocar las placas.

### 8.5 Sincronización con firmware (lado STM32/ESP32, rama `feature/beamforming`)

- [ ] **Geometría del array**: el firmware asume 4×8 patrón ladrillo, X_pitch=80mm,
      Y_pitch=160mm, filas 2&4 +40mm a la derecha (marco del oyente). Si el layout físico
      difiere, actualizar `mic_pos/spk` en `firmware/Core/Src/main.c` (es una tabla).
- [ ] Al llegar la DS11: `WALKER_NUM_CH` y `CAL_NUM_CH` de 8 → 32; correr **channel walker**
      para validar mapeo+polaridad real; **AUTOFOCUS** mide TOF y polaridad por parlante.
- [ ] PE12 pasa de ANALOG a salida GPIO (driver de los LEDs RJ45) — cambio de una línea en
      el init, anotado en [docs/beamforming_roadmap.md](../../docs/beamforming_roadmap.md).

---

**Last updated**: 2026-07-03 (checklist §8 consolidado post-revisión de netlist + decisiones de julio)
**Estado**: esquemático DS11 con front-end de entrada completo (Cc+Rs+R131 verificados); pendiente: decisión polaridad/swap (§8.2), layout (placement TVS, pistas 24V), limpieza ERC, gates de fabricación. Reclamo JLCPCB cerrado en USD 355.
