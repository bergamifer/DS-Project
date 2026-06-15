# DS11 — Recomendaciones de Upgrade (placa de producción)

**Base:** hallazgos del bring-up DS10 ([ds10_bringup_findings.md](ds10_bringup_findings.md))
**Criterio:** DS11 es candidata a placa de producción — debe salir funcionando sin bodges ni sorpresas.
**Última revisión:** 2026-06-12

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

## PRIORIDAD ALTA

### A1. Atenuador de nivel entre PCM1690 y PAM8302A (+20dB de SNR efectivo)

**Problema (hallazgo #5):** el PAM8302A (+24dB fijo, rail 5V) clipea con entradas > ~630mVpp ≈ **-22dBFS del DAC**. Hoy el DAC solo puede usar ~1/13 de su rango → se desperdician ~3.7 bits de resolución y el SNR de la cadena queda limitado por el tramo digital bajo.

**Propuesta:** atenuador resistivo de ~-20dB en cada entrada (por pata, manteniendo el diferencial):
- Mantener R98/R99 = 1kΩ serie, agregar **R shunt ~110Ω** después del serie (÷10 ≈ -20dB)
- Recalcular el cap del LPF para conservar fc ≈ 34kHz con la nueva impedancia (~100Ω paralelo): **C ≈ 47nF**
- El acople C250/C251 220nF queda igual

**Resultado:** el DAC opera cerca de full-scale (0..-6dBFS), el PAM recibe el mismo nivel máximo de hoy, y la relación señal/ruido de pista mejora ~20dB. Para beamforming con 32 canales sumando, ese headroom digital es valioso.

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

### B3. Cambios que NO hay que hacer

- **EEPROM PB6/PB7**: el ruteo está perfecto — el problema era de firmware/doc (AF6, no AF11). No tocar.
- **Filtro RC de entrada del PAM**: correcto como está (la "señal sucia" era el bug de alineamiento SAI, no el filtro). Solo se recalcula si se implementa A1.
- **UART7 en J7**: funcionó perfecto como debug port vía ST-Link VCP. Mantener idéntico.
- **Control compartido DAC RST/MUTE (PC10/11/12)**: simplificación exitosa de DS10. Mantener.
- **Sincronización SAI1↔SAI2** (skew entre ch0-15 y ch16-31): es tema de **firmware** (SAI_GCR SYNCIN/SYNCOUT), no de PCB. Issue #14.

---

## Checklist de verificación pre-producción DS11

- [ ] BOM: SMBJ24A en D8 (no-sustitución)
- [ ] BOM: LEDs con part numbers individuales por color (lección DS9)
- [ ] Silkscreen pin-1 reforzado en los 4 PCM1690 + AK5538
- [ ] Esquemático: nota "AK5538 requiere config TDM por I2C (addr 7-bit 0x11)"
- [ ] Esquemático: corregir las constantes erradas del design review DS10 (MCLK=24.576MHz, I2C4=AF6, AK addr=0x11)
- [ ] Atenuador DAC→PAM dimensionado y simulado (A1)
- [ ] Front-end de entrada ADC definido según transductor final (M1)
- [ ] LED de diagnóstico en GPIO directo (A4)
- [ ] Firmware de validación: el actual `main` ya bootea, auto-diagnostica (scans I2C + register dumps + heartbeat) y genera test tone — sirve como firmware de test de fábrica

---

*Recomendaciones DS11 v1.0 — derivadas del bring-up DS10 (mayo-junio 2026)*
*Ver [ds10_bringup_findings.md](ds10_bringup_findings.md) para el detalle de cada hallazgo*
