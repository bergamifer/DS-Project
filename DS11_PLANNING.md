# DS11 — Plan de diseño y checklist

> **Objetivo**: corregir todos los errores detectados en DS10 (de diseño propio y de producción) y agregar mejoras de usabilidad para desarrollo.
> **Contexto**: DS10 tanda 1 (3 placas ensambladas, orden SMT026040962250-11787566A) resultó defectuosa por error de JLCPCB (3 de 4 DACs rotados 180°). Reclamo por crédito (~USD 399) en curso — el resultado define el presupuesto de la orden DS11. Detalle: [DS10_BRINGUP.md](DS10_BRINGUP.md) §6.

---

## 1. Correcciones obligatorias (errores DS10)

| # | Problema en DS10 | Origen | Fix DS11 |
|---|---|---|---|
| 1 | **RJ45 ×4 orientados hacia el interior de la placa** (Issue #1) — el conector abre hacia adentro | Error de placement propio | Rotar 180° en layout. En evaluación: conector RJ45 ×4 integrado (footprint `RJ45_Amphenol_RJHSE538X-04` ya creado, datasheet `datasheets/rj45x4.pdf`, 3D `3d/RJ45x4.step`). **Verificar con 3D viewer + impresión 1:1 antes de Gerbers** |
| 2 | **Daisy-chain 24V por barrel jack acumula corriente** (Issue #2) — 2.5A×N placas: el primer conector de una cadena de 18 lleva ~45A | Error de arquitectura | Terminal block enchufable 5.08mm 20A (tipo BXCONN JCB508-ZOMC2P) entrada/salida + verificar sección de pistas/planos de 24V para la corriente de paso |
| 3 | **D8 = SM6T15A (TVS de 15V) en riel de 24V** — conduce a tensión nominal; contribuyó al humo de la tanda 1 | Error de BOM | Cambiar en el **esquemático**: Value=SMBJ24A, LCSC=C726830 (el BOM se regenera del esquemático; corregirlo solo en CSV se pierde) |
| 4 | **USB-only no arranca** — inrush ~9A cargando 1.1mF de +5VA a través de FB2 → OCP del puerto USB corta VBUS | Limitación de diseño | Jumper en serie con FB2: `+5VA → FB2 → [JP_PAM] → +5V`. Dimensionar para 2-3A continuos (doble pin o solder jumper ancho). Ver §2 |
| 5 | 6× ERC `power_pin_not_driven` | Cosmético | Agregar PWR_FLAG (instrucciones: [DS10_PRE_PRODUCTION_CHECKLIST.md](DS10_PRE_PRODUCTION_CHECKLIST.md) §8) |
| 6 | J31/J32 pin 3 (switch del barrel) sin uso | Mejora | Si se mantiene barrel: divisor 100k/10k a GPIO para detección fin-de-cadena. Si se pasa a terminal block: N/A |
| 7 | Sin protección ESD en entradas RJ45 | Mejora | TVS array en los pares del primer conector de la cadena |
| 8 | `netlist.md` desactualizado (no incluye R107-R110 de AMUTEO) | Doc | Regenerar netlist al cerrar el esquemático DS11 + actualizar la Sección 11 del [DS10_DESIGN_REVIEW.md](DS10_DESIGN_REVIEW.md) |
| 9 | 8× `endpoint_off_grid` en sheet root | Cosmético | Alinear wires a la grilla |
| 10 | Silkscreen de K1 dice "Reset ESP32" — es **boot mode** (GPIO8), no reset | Silk engañoso | Renombrar a "BOOT ESP32" en silk |

---

## 2. Programación sin 24V (alimentación desde la PC)

**Decisión: alimentar por el pin 5V del ST-Link → riel +5V. NUNCA por el pin 3V3.**

Razones:
- Entrando por +5V, los dos LDO (U11 → +3V3 digital, U10 → +3.3VA analógico) levantan normalmente → la topología de power es idéntica a la operación normal, todos los rieles quedan vivos y testeables.
- Inyectar 3.3V directo al riel +3V3 back-alimenta la salida del AP1117 (riesgo de corriente inversa en el LDO) y deja +3.3VA muerto (el ADC no se puede probar).
- El pin 3V3 de los ST-Link (sobre todo clones) sale de un LDO interno chico (~100-200mA): insuficiente para STM32+ESP32 (~350mA en picos). El pin 5V es VBUS del USB casi directo (~450mA disponibles).

### Topología propuesta (tres fuentes de 5V OR-eadas por diodo)

```
Buck 24V→5V ──► FB2 ──► [JP_PAM] ──┐
USB-C VBUS ──► D2 (Schottky) ──────┼──► +5V ──► U11 (+3V3) y U10 (+3.3VA)
ST-Link 5V (J7/10) ──► D_nuevo ────┘
```

- **Agregar Schottky** (tipo B5819W, como D2) en serie con el actual JP3 (+5V → J7/10): hoy, con JP3 cerrado y 24V presente, el buck back-alimenta el VBUS del ST-Link. Con el diodo, cualquier combinación de fuentes es segura.
- Con **JP_PAM abierto**: el lado digital se alimenta por USB-C o ST-Link con ~30-60µF de inrush → no dispara OCP. Cerrado: operación completa por 24V.
- Presupuesto de corriente programando: STM32 ~150-200mA + ESP32 idle ~50mA (bursts WiFi 300mA) + lógica DACs/ADC → OK para programar y debuggear; **evitar WiFi activo + pipeline de audio si se alimenta solo por ST-Link**.
- ⚠️ **Verificar en datasheet PCM1690**: con JP_PAM abierto los DACs quedan con VDD=3.3V y AVDD=0V. Los CMOS de TI suelen tolerarlo (sin path de latch-up), pero confirmarlo antes de fijar la topología.

---

## 3. Nuevas features (lecciones del bring-up tanda 1)

- **LED de power hardwired en +3V3** (y opcional en +5VA): R + LED directo al riel, sin firmware. En la tanda 1 no había NINGUNA señal visible de vida (los 5 LEDs son de firmware) y eso complicó el diagnóstico del primer encendido.
- **Test points**: +24V, +5VA, +5V, +3V3, +3.3VA, GND ×2, MCLK (PE2). Hacen el gate de bring-up ([DS10_BRINGUP.md](DS10_BRINGUP.md) §0) mucho más rápido.
- Mantener el **gate de bring-up** como procedimiento estándar para cada tanda nueva: medir rieles a GND + verificar orientación de ICs ANTES de energizar.

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

| Función | Candidato | Material ya en repo |
|---|---|---|
| RJ45 audio | RJHSE538X-04 (bloque ×4 integrado) | footprint en `librerias/DS_local_library.pretty/`, `datasheets/rj45x4.pdf`, `3d/RJ45x4.step` |
| USB-C | G-Switch GT-USB-7051x (vertical) | footprint en `librerias/DS_local_library.pretty/`, `datasheets/usb_vertical.pdf` |
| Power 24V | Terminal block 5.08mm 20A / barrel vertical | `datasheets/barrel vertical.pdf` |

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

1. Cerrar reclamo JLCPCB (define presupuesto).
2. Esquemático: fixes #3 (D8), #4 (JP_PAM), §2 (diodo ST-Link), #5 (PWR_FLAG), #10 (silk K1) + LED power + test points (§3).
3. Layout: RJ45 (#1), conector de potencia (#2), pistas 24V para corriente de paso.
4. Regenerar netlist + ERC/DRC limpios + actualizar docs (#8).
5. Gates de fabricación (§4) — en particular DFM 3D y BOM por C-number.
6. Pedido con el crédito aplicado.

---

**Last updated**: 2026-06-10
**Estado**: planificación — esquemático DS11 no comenzado
