# DS10 — Hallazgos del Bring-Up (registro completo)

**Período:** mayo-junio 2026
**Placa:** DS10 (1 de 3 recibidas de JLCPCB; las otras 2 defectuosas por rotación de ICs)
**Estado final:** audio limpio verificado de oído, ADC capturando, EEPROM persistiendo. Pipeline completo funcional con 1 de 4 DACs poblado.

---

## Resumen ejecutivo

El bring-up encontró y resolvió **7 problemas**, de los cuales:
- **1 bug de firmware crítico heredado de DS9** (alineamiento SAI) que era la causa de la distorsión "tipo 50kHz" nunca diagnosticada en DS9
- **3 errores del design review** (AF de I2C4, dirección I2C del AK5538, strapping TDM)
- **1 error de diseño de niveles** (DAC a full-scale clipea el PAM8302A)
- **2 problemas de fabricación JLCPCB** (rotación 180° de PCM1690, TVS equivocado en D8)

---

## Hallazgo #1 — Alineamiento de datos SAI (ROOT CAUSE histórico) 🔥

**Síntoma:** tono de 1kHz audible pero "cuadrado/saturado" en parlantes; osciloscopio mostraba la fundamental con hash HF enorme montado. Idéntico a lo que en DS9 se diagnosticó como "ruido de ~50kHz" (nunca resuelto — DS9 no tenía amps para escucharlo).

**Pista decisiva:** bajar el nivel digital 24dB NO cambió el carácter de la distorsión → la distorsión era proporcional a la señal → digital, no analógica ni clipping.

**Causa:** el SAI del STM32H7 con `DataSize=24` transmite los bits **[23:0]** del registro FIFO (right-aligned, RM0433). El código hacía `out = s << 8` (muestra en [31:8], creyendo que el SAI tomaba los bits altos). El SAI transmitía los 16 bits bajos + 8 ceros → **wrapping** de la forma de onda (64 vueltas por ciclo a -6dBFS) → sierra con armónicos masivos.

**Fix (commit 4db688d):**
- TX: escribir el sample de 24 bits directo, sin `<<8`
- RX→TX (beamforming): `sample >> 8` (ADC 32-bit → DAC 24-bit) ANTES del gain — de paso elimina un overflow latente en `sample * gain`

**Regla permanente:** SAI DataSize=24 → samples right-aligned [23:0]. Nunca `<<8`.

---

## Hallazgo #2 — EEPROM: I2C4 es AF6, no AF11

**Síntoma:** `EEPROM available=0` (no ACK en 0xA0) con hardware verificado correcto (U8 bien soldada, RN5 4.7kΩ, PB5/6/7 = pines 91/92/93).

**Causa:** el design review especificaba **AF11** para I2C4 en PB6/PB7. Con AF11 los pines no quedan conectados al periférico → SCL/SDA mudos. El AF correcto es **AF6** (ya verificado empíricamente en DS9 con el mismo silicio).

**Fix (commit en sesión 2026-05-19):** `EEPROM_I2C_AF = GPIO_AF6_I2C4`. Resultado: `available=1`, load/save funcionando.

---

## Hallazgo #3 — AK5538: dirección I2C real es 0x11, no 0x14

**Síntoma:** probe I2C en 0x28 (8-bit) → NO ACK.

**Causa:** el design review decía "7-bit 0x14". Un scan completo del bus encontró el chip en **7-bit 0x11 (8-bit 0x22)** — consistente con el esquema AKM `0x10 + CAD1·2 + CAD0` con CAD0=1.

**Fix (commit 465add2):** `AK5538_ADDR = 0x22` + scan I2C3 permanente en el boot.

---

## Hallazgo #4 — AK5538: el strapping NO configura TDM256

**Síntoma:** con I2C ya funcionando, el RX seguía misframed: datos solo en slot 0, patrones stuck (0x3FFFxxxx), ceros exactos en ch1-7.

**Causa:** el design review afirmaba que el strapping de hardware dejaba el chip en TDM256/32-bit. El register dump mostró `Control2 (R03) = 0x00` → **TDM deshabilitado, modo estéreo**. Causa probable: los pines de strap CKS0/CKS2 comparten el bus I2C3 y los pull-ups los fuerzan a 1 al liberar PDN.

**Fix (commit 16f8519):** configuración TDM256 por I2C en cada boot:
```
R01 = 0x00  (RSTN=0 — reset interno)
R03 = 0x40  (Control2: TDM[1:0]=10 → TDM256)
R01 = 0x01  (RSTN=1 — normal)
+ readback de verificación
```
Resultado: framing perfecto, 8 canales con datos de 24-bit MSB-aligned en slots de 32.

**Nota:** los registros se resetean con cada ciclo de PDN → la config debe re-aplicarse en cada boot (el firmware lo hace).

---

## Hallazgo #5 — Niveles: el DAC a full-scale clipea el PAM8302A

**Síntoma:** tono audible pero recortado, consumo 5.4W (vs 3.2W base).

**Causa (aritmética, no falla):**
- PCM1690 full-scale: 8Vpp diferencial (1.6×VCC1, VCC1=5V)
- PAM8302A: ganancia fija +24dB (×15.8), rail 5V BTL → salida máx ~10Vpp
- Entrada máxima limpia del PAM: 10Vpp/15.8 ≈ 630mVpp ≈ **-22dBFS digital**
- El tono a -6dBFS (4Vpp) sobrecargaba el amp por ~16dB

**Fix (commit 65428a7):** `test_tone_shift = 4` → tono a -30dBFS ≈ 250mVpp ≈ 0.5W/4Ω limpio.

**Regla permanente:** con PAM8302A directo, nunca mandar más de ~-22dBFS desde el PCM1690. (Ver propuesta de divisor de entrada en [ds11_upgrades.md](ds11_upgrades.md).)

---

## Hallazgo #6 — AK_PDN: polaridad (pre-flash review)

Detectado en revisión de código antes del primer flash: el firmware tenía la polaridad invertida. Datasheet AK5538VN: **"L" = Reset & Power-down, "H" = Normal operation**. Corregido: init LOW (chip off durante boot) → HIGH después de SAI3 listo.

---

## Hallazgo #7 — Fabricación JLCPCB

- **U1/U3/U4 (PCM1690) montados rotados 180°** pese a CPL correcto → corto +3V3→GND (7.8Ω) y +5VA_DAC→GND (13Ω). La placa no encendía hasta desoldarlos. JLCPCB admitió culpa; reclamo por crédito en curso. Solo U2 (DAC0) quedó funcional.
- **D8 con SM6T15A** (TVS de 15V) en vez del SMBJ24A correcto para el rail de 24V.

---

## Otros datos verificados en el bring-up

| Item | Resultado |
|------|-----------|
| MP2315S buck 24V→5VA | OK. Acepta 12V de entrada (rango 4.5-24V) — útil para trabajo portátil |
| 5VA ruido | <100mV — limpio, descartado como fuente de problemas |
| Debug serial | UART7 (PE7/PE8) → J7 pins 7/8 → VCP del ST-Link. Sin contención con ESP32 (UART4) |
| `st-flash --reset` | Necesario para que el H7 arranque solo tras flashear |
| DMA pipeline | 50 Half + 50 Full/s, 0 errores, jitter ~0µs (rango ~100 ciclos @ 480MHz) |
| Fill time | ~127µs por half-buffer (1.3% CPU) con tono; presupuesto 10ms |
| Underruns | Solo durante bursts de print verbose (benignos); 1 línea/s = 0 underruns |
| AK5538 OVF | LOW = OK (confirmado contra datasheet: "Power down status: L") |
| Formato RX del AK5538 | 24-bit MSB-aligned en slots de 32 (LSB byte = 0x00) |

---

## Lecciones para futuras revisiones

1. **No confiar en el design review sin verificar contra datasheet/hardware** — acumuló 5 errores (MCLK 12.288 vs 24.576, AF11, addr 0x14, "strapping TDM", master/slave del AK5538 en §11.7).
2. **El register dump + scan I2C en el boot valen oro** — diagnosticaron en minutos lo que a ciegas eran horas.
3. **Config por I2C > strapping** cuando el chip lo permite: verificable por readback, reproducible, flexible.
4. **Escuchar la señal** (no solo verla en el scope) — el bug de alineamiento sobrevivió todo DS9 porque nunca hubo un parlante conectado.
5. **Mantener el debug serial fuera del UART del ESP32** (UART7 vs UART4) eliminó la contención que complicaba DS9.

---

*Registro de bring-up DS10 — actualizado 2026-06-12*
*Fixes en commits: 4db688d (SAI alignment), 465add2 (AK addr), 16f8519 (TDM256), 65428a7 (tone level), 9e14fd0 (min/max meter)*
