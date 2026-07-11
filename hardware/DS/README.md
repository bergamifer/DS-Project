# Domosonica DS10

Sistema de audio espacial de 32 canales con capacidad de beamforming, diseñado para instalaciones artísticas a gran escala.

---

## El proyecto

**Domosonica** es una instalación sonora inmersiva basada en un campo de ~500 parlantes distribuidos en el espacio. El objetivo es crear experiencias de escucha donde el sonido se percibe como un fenómeno espacial: fuentes que se mueven, campos que emergen, focos que aparecen y desaparecen en el espacio físico.

El beamforming permite dirigir energía sonora hacia puntos específicos del espacio sumando señales con retardos calculados. Con suficientes parlantes y resolución de fase, es posible construir frentes de onda arbitrarios — lo que convierte el espacio en un instrumento.

### Por qué hardware propio

La escala del proyecto (~500 parlantes) hace inviable el uso de hardware de audio profesional estándar: demasiado costoso, demasiado grande, demasiado dependiente de infraestructura. El DS10 es una solución diseñada específicamente para esta instalación: barato, compacto, reproducible, y capaz de operar en red distribuida.

---

## DS10 — Placa de producción

Cada unidad DS10 es un nodo de audio autónomo que distribuye 32 canales amplificados hacia 32 parlantes. La instalación completa usa ~18 unidades conectadas en red.

```
                        DS10 (por unidad)
┌────────────────────────────────────────────────────────┐
│                                                        │
│  24V DC ──> Buck ──> 5V ──> 32x PAM8302A ──> Speakers │
│              │                                         │
│              └──> LDOs ──> STM32H743 ──SAI──> 4x DAC  │
│                                │                       │
│                            ESP32-C3  (WiFi/BLE)        │
│                                │                       │
│                            AK5538VN  (8ch ADC)         │
│                                                        │
└────────────────────────────────────────────────────────┘
         ↑                              ↑
    24V daisy-chain              Cat5e/RJ45 audio
    entre cajas                  (4 links × 8ch = 32ch)
```

### Capacidades por unidad

- **32 canales de salida** — DAC diferencial 32-bit + amp Class-D 2.5W por canal
- **8 canales de entrada** — ADC diferencial 32-bit (micrófonos o señal de referencia)
- **Sincronización de red** — MCLK 24.576MHz (512xfS) compartido garantiza coherencia de fase entre unidades
- **WiFi/BLE** — configuración y control inalámbrico
- **Alimentación 24V** — bus daisy-chain entre cajas, un solo cable de potencia

### Escala del sistema completo

| | Por unidad | Sistema (18 unidades) |
| --- | --- | --- |
| Canales de salida | 32 | ~576 |
| Canales de entrada | 8 | ~144 |
| Parlantes | 32 | ~500 |
| Alimentación | 24V DC | Bus daisy-chain |

---

## Beamforming

El beamforming en este contexto no es telecomunicaciones — es síntesis de campo sonoro. La idea central es que si conocés la posición de cada parlante y podés controlar la fase y amplitud de cada canal de forma independiente, podés construir patrones de interferencia específicos en el espacio.

**Aplicaciones concretas en la instalación:**

- Focos de sonido: energía concentrada en un punto del espacio
- Fuentes virtuales: la percepción de que el sonido viene de un lugar donde no hay parlante
- Zonas silenciosas: cancelación activa por interferencia destructiva
- Movimiento: barrido de un frente de onda a través del espacio

El STM32H743 corre a 480MHz con DMA audio pipeline y opera los 5 puertos SAI simultáneamente sin underruns, lo que permite actualizar los 32 canales en tiempo real a 48kHz.

---

## Hardware

### Componentes principales

| IC | Función | Bus |
| --- | --- | --- |
| STM32H743VIT6 | MCU central, 480MHz, LQFP100 | — |
| 4× PCM1690 | DAC 8ch diferencial 32-bit | SAI1/SAI2 |
| AK5538VN | ADC 8ch diferencial 32-bit | SAI3_B (TDM256) |
| 32× PAM8302A | Amplificador Class-D 2.5W/ch | — |
| ESP32-C3-Mini | WiFi / BLE | UART |
| MP2315S | Buck 24V→5V 3A | — |

### PCB

- 4 capas, FR-4, 1.6mm — stackup JLC04161H-7628
- Fabricado por JLCPCB
- Diseñado en KiCad 9.0

### Distribución de audio

Señal diferencial sobre Cat5e/RJ45 en topología daisy-chain. 4 pares trenzados por cable = 4 canales diferenciales. 2 cables por placa = 8 canales por enlace × 4 enlaces = 32 canales.

---

## Estado

| Versión | Estado |
| --- | --- |
| DS9 | En producción — sistema base, PCM1802 2ch ADC |
| DS10 | Tanda 1 (3 ensambladas) **defectuosa** — JLCPCB montó 3 de 4 DACs rotados 180°; reclamo por crédito en curso. Ver [DS10_BRINGUP.md](DS10_BRINGUP.md) §6 |
| **DS11** | **En diseño** — correcciones DS10 + mejoras de usabilidad. Ver [DS11_PLANNING.md](DS11_PLANNING.md) |

### Mejoras DS10 respecto a DS9

- PCM1802 (2ch) reemplazado por AK5538VN (8ch, 32-bit) — 4× más entradas
- 32× PAM8302A integrados en PCB (antes módulo HAT externo)
- Fuente 24V con buck local por placa (antes fuente externa)
- Supply analógico separado (+3.3VA) para el ADC

---

## Documentación técnica

- [DS11_PLANNING.md](DS11_PLANNING.md) — Plan de la revisión DS11: correcciones, nuevas features, gates de fabricación, análisis DS12
- [DS10_DESIGN_REVIEW.md](DS10_DESIGN_REVIEW.md) — Revisión de diseño, verificación de netlist, referencia de firmware (Sección 11)
- [DS10_BRINGUP.md](DS10_BRINGUP.md) — Procedimiento de bring-up + defectos confirmados de la tanda 1
- [DS10_PRE_PRODUCTION_CHECKLIST.md](DS10_PRE_PRODUCTION_CHECKLIST.md) — Checklist pre-producción tanda 1 (histórico), parámetros JLCPCB
- `versiones/DS10/` — Snapshot del proyecto KiCad tal como se fabricó (tag git `DS10`)
