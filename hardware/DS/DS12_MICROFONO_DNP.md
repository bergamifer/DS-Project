# DS12 — micrófono MEMS I2S de banco (DNP)

**Decisión (2026-11-07):** un micrófono MEMS I2S de 3,3 V, soldado después a mano, conectado a las
tres redes I2S2 que ya existen. Sin patas nuevas del STM32 y sin circuito analógico.

No tiene aplicación directa en la obra. Sirve para probar el STM32 con micrófonos en otros proyectos.

## Por qué I2S y no PDM ni analógico

| Opción | Patas nuevas | Circuito extra | Veredicto |
|---|---|---|---|
| **I2S MEMS (elegida)** | 0 (usa I2S2) | ninguno | la más simple |
| PDM (MP34DT06J, etc.) | 2 | ninguno, pero el firmware usa DFSDM o SAI-PDM | más patas y más firmware |
| Analógico al ADC interno | 1 (ej. PC0) | acople + polarización | ensucia el esquemático |

Mecanismo: el micrófono I2S ya entrega audio digital de 24 bits. No hay ADC, capacitor de acople ni
polarización de por medio.

## Qué dibujar

- 1 micrófono **top-port** (sin agujero en la placa), **DNP** en el BOM.
- Alimentación: `+3V3`, con **100 nF** pegado a la pata (este sí se ensambla).
- Redes: `/stm32_custom/I2S2_WS` (PB12), `/stm32_custom/I2S2_CK` (PB13) y `/stm32_custom/I2S2_SDI`
  (PB14). Estas redes ya van a J6.1, J6.2 y J5.5. Verificado en `.ds12_audit/redes.txt`
  (generado el 2026-10-08; reconfirmar contra el esquemático actual).
- Pata L/R: a GND (canal izquierdo) o a `+3V3` (derecho). Para estéreo se suma un segundo micrófono
  con L/R al otro nivel, sobre las mismas tres redes.
- Modelo: familia I2S de 3,3 V en LGA de 8 patas (ICS-43434, SPH0645LM4H, MSM261S4030H0R).
  Como va DNP y se compra aparte, no hace falta que esté en LCSC. **Stock en LCSC no verificado:** no
  se encontró el listado de `MSM261S4030H0R`.
- **Antes de dibujar el símbolo:** bajar la hoja de datos del modelo comprado y confirmar su disposición
  de patas y que el rango de reloj de CK sea compatible con el I2S2 del STM32. No verificado.

## Ubicación (layout)

- Lejos de los PAM8302A, del inductor del buck y de los cables de parlantes.
- Footprint con pads accesibles para soldar con pasta y aire caliente (LGA de unos 3,5 × 2,65 mm).

## Limitación

Las líneas I2S2 también salen a J5/J6. Con el micrófono soldado, **no conectar ahí otro dispositivo
I2S** a la vez: quedarían dos fuentes sobre la misma línea de datos. Tampoco se revisó si el firmware
ya usa I2S2.

## Patas libres del STM32 (netlist del 2026-10-08)

Libres: PE9, PE10, PE14, PB15, PD14, PD15, PC6, PA9, PB2, PB4, PD5, PD6, PE0, PE1, PC0, PC13.
PE13 queda para el LED2 de J9. Las funciones alternativas de cada pata **no se verificaron**:
chequear en CubeMX o en la hoja de datos del H743 antes de fijar pines.

## Otros periféricos considerados

- **Sensores I2C (IMU, temperatura, luz):** ya hay I2C2 con pull-ups (RN6) en J5.1/J5.2. Alcanza con
  un conector estándar.
- **Conector de 6–8 GPIO libres:** lo más barato para dejar la placa abierta.
- **microSD: descartada.** SDMMC1 usa PC8–PC12 y PD2; PC8–PC12 los ocupa el AK5538.
