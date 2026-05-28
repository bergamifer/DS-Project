# DS10 — Procedimiento de Bring-up

> **Estado**: PCBs recibidos de JLCPCB, primera energización.
> **Regla de oro**: durante el bring-up, **alimentar siempre por 24V**. El USB-only NO arranca la placa (ver §2) y eso es comportamiento esperado, no una falla.

---

## 0. Equipo necesario

- Fuente de banco con límite de corriente ajustable (CV/CC). Usada: 30V / 10A.
- Multímetro.
- Osciloscopio (para verificar MCLK 24.576 MHz — opcional en el primer encendido).
- ST-Link / sonda SWD para el STM32 (conector J7).
- Cable USB-C + laptop **en batería** (desenchufada del cargador) para flashear el ESP32.

---

## 1. Topología de rieles (verificada en netlist)

```
                          ┌──────────────────────────────► +5VA  (salida del buck)
                          │   • L1/2 (inductor)              • 32× PAM8302A (U12-U43 pin 6)
USB VBUS ─► D2 ─► +5V ─────┤   FB2 (ferrite, ~0.3Ω en DC)     • 5× 220µF bulk (C290-293, C71) = 1100µF
                          ├──(ferrite)───────────────────────┘  • FB3 ─► +5VA_DAC (4× PCM1690)
                          │
                          ├─► U11 (AP1117-33) ─► +3V3   (STM32, ESP32, EEPROM)
                          │
                          └─► U10 (AP2125K)   ─► +3.3VA (AK5538 analógico)
                                              +5VA ─► L1 ─► SW ─► MP2315S (buck, entrada +24V)
```

**Puntos clave:**
- El ferrite FB2 **no aísla en DC** (~0.2-0.4Ω). Para la corriente de alimentación, +5V y +5VA son casi el mismo nodo.
- Los dos LDO (U11 digital, U10 analógico) cuelgan de **+5V** (lado USB, antes de FB2).
- Los 1.1 mF de bulk y los 32 PAMs están en **+5VA** (después de FB2).
- D2 (Schottky B5819W) conduce solo VBUS → +5V. Con 24V presente, +5V ≈ 5.0V y D2 queda **en bloqueo**.

---

## 2. Por qué el USB-only no arranca (y por qué da igual)

Enchufar USB con la placa sin 24V obliga al puerto a cargar los 1.1 mF de +5VA a través de D2 + FB2:

| Parámetro | Valor |
|---|---|
| Capacitancia a cargar (+5VA) | ~1.1 mF + decoupling PAMs |
| R serie del camino | ~0.5 Ω |
| **Pico de inrush** | **≈ 9 A** |

9 A contra un puerto que limita a 500 mA → la OCP del puerto corta VBUS en <1 ms → +3V3 nunca llega a 3.3V → los micros no bootean.

**Workflow que lo evita:** alimentar por **24V primero**. Con +5V ya a ~5.0V, D2 queda bloqueado y el USB nunca entrega corriente de potencia — solo aporta D+/D− para flashear el ESP32.

> Corrección para DS11: jumper de aislamiento en serie con FB2 + soft-start en el path D2→+5V. Ver `project_ds11_planning`.

---

## 3. Procedimiento paso a paso

### Paso 0 — Chequeo de cortos (placa SIN alimentar)

Con el multímetro en modo resistencia (Ω):

| Medición | Sano | Falla |
|---|---|---|
| +24V a GND | kΩ y subiendo (caps cargando) | **<1 Ω = corto duro** |
| +5VA a GND | sube lento (1.1mF), termina alto | **~0 Ω = corto** |
| +3V3 a GND | sube, termina en kΩ | **~0 Ω = corto** |
| +3.3VA a GND | sube, termina alto | **~0 Ω = corto** |

Un electrolítico invertido (C290-293, C71), un puente de soldadura o el TVS equivocado (ver §6) se detectan acá sin aplicar tensión.

### Paso 1 — Smoke test (límite 0.2A, V = 24.0V)

Ajustar la fuente a **24.0V** y límite de corriente a **0.2A** ANTES de conectar. Conectar al barrel jack y encender la salida.

| Display de la fuente | Interpretación | Acción |
|---|---|---|
| 24V se mantiene, I clavada en 0.2A | Buck arrancando (normal) **o** corto en +5VA | Pasar al Paso 2 |
| **24V colapsa a ~0**, I = 0.2A | **Corto duro en el riel de 24V** | **APAGAR YA**, inspeccionar |

> **Importante**: 0.2A es demasiado bajo para que el buck complete el soft-start (necesita ~0.9A en la entrada para cargar 1.1mF). La placa NO va a arrancar a 0.2A — eso es normal. 0.2A solo sirve para cazar un corto duro de 24V.

### Paso 2 — Arranque (límite 1.5-2A)

Subir el límite a **1.5-2A**:

| Resultado | Interpretación | Acción |
|---|---|---|
| CV, I cae a **~50-90 mA** | Todo bien, era inrush de arranque | Bajar a 1A y operar |
| CC, rieles abajo, I clavada | Corto en +5VA | Medir +5VA; si ~0V → APAGAR e inspeccionar |

El soft-start del buck necesita cubrir el inrush de carga de 1.1mF:
`I_cap = 1.1mF × 5V/1.5ms ≈ 3.7A (lado 5V) → ≈ 0.9A (lado 24V)`

En idle la placa queda en **<100 mA**, así que con 1A hay margen de sobra y buena protección.

### Paso 3 — Verificación de rieles (24V aplicado)

| Punto | Esperado |
|---|---|
| +24V (barrel jack) | 24.0 V |
| +5VA (salida buck) | ~5.03 V |
| +5V (post-FB2) | ~5.0 V |
| +3V3 (salida U11) | 3.30 V |
| +3.3VA (salida U10) | 3.30 V |
| PE2 (MCLK, con firmware) | 24.576 MHz (osciloscopio) |

---

## 4. Qué esperar en el primer encendido

**Probablemente ningún LED se prende. Eso es normal.**

- No hay LED de power cableado al riel. Los 5 LEDs los maneja firmware:
  - D1 (rojo) ← ESP32 GPIO16
  - D4/D5/D6/D7 ← STM32 TIM3 (PA6/PA7/PB0/PB1)
- STM32 virgen: GPIOs en reset (alta impedancia), TIM3 sin configurar → LEDs apagados.
- ESP32 virgen: firmware de fábrica queda quieto.

**La primera señal de vida es: rieles OK con el multímetro + el STM32 detectable por SWD.** No los LEDs.

---

## 5. Flasheo (los dos micros son independientes)

| Micro | Interfaz | Alimentación |
|---|---|---|
| **STM32H743** | SWD por J7 (SWDIO=PA13, SWCLK=PA14, NRST) | 24V (el USB no levanta +3V3 por el inrush) |
| **ESP32-C3** | USB nativo (USB-Serial-JTAG, D+/D− vía U6/R12/R13) | 24V presente + USB para datos |

**Secuencia:**
1. Alimentar con 24V (1.5-2A para arrancar, 1A en operación).
2. Flashear STM32 por SWD/J7. No necesita USB.
3. Conectar USB-C (laptop **en batería**). Como +5V ya está a 5.0V, D2 bloquea → cero inrush, el USB solo aporta datos.
4. Flashear ESP32 con esptool. El C3 entra en download mode solo por el USB-Serial-JTAG nativo (por eso se sacó el jumper de GPIO9). Fallback: K1 (GPIO8 a GND).

> **K1 NO es un reset.** Es selector de boot mode (GPIO8). El reset del ESP32 es por EN (R5+C25) o software. El silkscreen "Reset ESP32" es engañoso.

---

## 6. ⚠️ Estado de D8 (TVS de entrada)

En el BOM de producción, D8 figuraba como `SM6T15A` (TVS de **15V**) **sin número LCSC**. Implicaciones:

- **Sin LCSC → JLCPCB no lo puebla.** D8 casi con seguridad llegó **sin poblar** (pad vacío). **Verificar visualmente con lupa.**
- Si estuviera poblado con el SM6T15A (15V) sobre el riel de 24V → rompe a ~14.4V y mantiene el riel abajo. El smoke test (Paso 1) lo cazaría (24V colapsa).
- Si está vacío → **esta tanda no tiene protección de transitorios** en la entrada de 24V. Seguro para banco (fuente limpia). Para la instalación real con daisy-chain de 24V conviene poblar el **SMBJ24A** (LCSC C726830, 24V standoff, 600W). BOM corregido a SMBJ24A para futuras tandas.

---

## 7. Masa / ground loop

Plan de bring-up: **laptop en batería** (masa flotante).

- Con la laptop desenchufada del cargador, hay un único camino de masa (Fuente − → GND placa → USB → laptop). No se cierra ningún lazo → no circula corriente de ecualización. Seguro sin aislador.
- **Verificar que la salida de la fuente flota**: con la fuente apagada y desconectada, medir Ω entre el borne **(−)** y el de **tierra (verde)**. Megaohms/abierto → flotante ✓. Si hay un puente metálico (−)↔verde, sacarlo durante el bring-up.
- Para sesiones largas (batería se descarga) o laptop + 24V simultáneos con cargador → usar **aislador USB galvánico** (ADuM3160).

---

## 8. Resumen accionable

1. **Bring-up = 24V siempre.** Nada de USB-only en DS10.
2. **Paso 0**: chequeo de cortos con multímetro (placa apagada).
3. **Smoke test**: 0.2A solo caza cortos duros de 24V; subir a **1.5-2A** para arrancar; idle ~50-90 mA.
4. **Mirar el pad de D8** — probablemente vacío (sin TVS esta tanda; OK para banco).
5. **"Sin LEDs" es normal** sin firmware. Verificar con multímetro + SWD.
6. **Flasheo**: STM32 por SWD/J7 con 24V; ESP32 por USB con 24V ya presente y laptop en batería.
7. **Fuente flotante**: confirmar (−)↔tierra abierto.

---

**Documentos relacionados:**
- [DS10_DESIGN_REVIEW.md](DS10_DESIGN_REVIEW.md) — Sección 11: referencia de configuración de firmware
- [DS10_PRE_PRODUCTION_CHECKLIST.md](DS10_PRE_PRODUCTION_CHECKLIST.md) — verificación de netlist y parámetros JLCPCB
