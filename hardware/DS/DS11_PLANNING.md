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

**Last updated**: 2026-06-22
**Estado**: esquemático DS11 prácticamente cerrado; pendiente layout (placement TVS, pistas 24V), limpieza ERC y gates de fabricación. Reclamo JLCPCB cerrado en USD 355.
