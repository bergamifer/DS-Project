# Domosónica — la placa DS

Sistema de audio espacial de 32 canales con capacidad de beamforming, diseñado para instalaciones artísticas a gran escala.

---

## El proyecto

**Domosonica** es una instalación sonora inmersiva basada en un campo de ~500 parlantes distribuidos en el espacio. El objetivo es crear experiencias de escucha donde el sonido se percibe como un fenómeno espacial: fuentes que se mueven, campos que emergen, focos que aparecen y desaparecen en el espacio físico.

El beamforming permite dirigir energía sonora hacia puntos específicos del espacio sumando señales con retardos calculados. Con suficientes parlantes y resolución de fase, es posible construir frentes de onda arbitrarios — lo que convierte el espacio en un instrumento.

### Por qué hardware propio

La escala del proyecto (~500 parlantes) hace inviable el uso de hardware de audio profesional estándar: demasiado costoso, demasiado grande, demasiado dependiente de infraestructura. La placa DS es una solución diseñada específicamente para esta instalación: barato, compacto, reproducible, y capaz de operar en red distribuida.

---

## La placa, por unidad

Cada unidad es un nodo de audio autónomo que distribuye 32 canales amplificados hacia 32 parlantes. La instalación completa usa ~18 unidades conectadas en red.

```
                         DS (por unidad)
┌────────────────────────────────────────────────────────┐
│                                                        │
│  20V DC ──> Buck ──> 5V ──> 32x PAM8302A ──> Speakers │
│              │                                         │
│              └──> LDOs ──> STM32H743 ──SAI──> 4x DAC  │
│                                │                       │
│                            ESP32-C3  (WiFi/BLE)        │
│                                │                       │
│                            AK5538VN  (8ch ADC)         │
│                                                        │
└────────────────────────────────────────────────────────┘
         ↑                              ↑
    20V daisy-chain              Cat5e/RJ45 audio
    entre cajas                  (4 links × 8ch = 32ch)
```

### Capacidades por unidad

- **32 canales de salida** — DAC diferencial 32-bit + amp Class-D 2.5W por canal
- **8 canales de entrada** — ADC diferencial 32-bit (micrófonos o señal de referencia)
- **Sincronización de red** — MCLK 24.576MHz (512xfS) compartido garantiza coherencia de fase entre unidades
- **WiFi/BLE** — configuración y control inalámbrico
- **Alimentación 20V** — bus daisy-chain entre cajas, un solo cable de potencia

### Escala del sistema completo

| | Por unidad | Sistema (18 unidades) |
| --- | --- | --- |
| Canales de salida | 32 | ~576 |
| Canales de entrada | 8 | ~144 |
| Parlantes | 32 | ~500 |
| Alimentación | 20V DC | Bus daisy-chain |

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
| MP2315S | Buck 20V→5V 3A | — |

> **En DS12 la etapa de fuente cambia entera:** el buck pasa a **AP63356** y el LDO digital
> desaparece. Todo el diseño, con valores y con el porqué de cada uno, en
> [DS12_ALIMENTACION.md](DS12_ALIMENTACION.md).
>
> **Entrada de la DS12: 20 V nominal, ventana de 15 a 21 V.** Por debajo de ~14,7 V los
> amplificadores no encienden (el micro sí); por encima de ~22 V conduce el TVS y puede abrirse
> el fusible. Una fuente de 12 V o de 24 V no sirve.

### PCB

- 4 capas, FR-4, 1.6mm — stackup JLC04161H-7628
- Fabricado por JLCPCB
- Diseñado en **KiCad 10.0** (el archivo dice `generator_version "10.0"`)

### Distribución de audio

Señal diferencial sobre Cat5e/RJ45 en topología daisy-chain. 4 pares trenzados por cable = 4 canales diferenciales. 2 cables por placa = 8 canales por enlace × 4 enlaces = 32 canales.

---

## Estado (2026-09-11)

| Versión | Estado |
| --- | --- |
| DS9 | Histórico — sistema base, PCM1802 2ch ADC |
| DS10 | Histórico. Tanda de 3, **defectuosa**: JLCPCB montó 3 de 4 DACs rotados 180°. Reclamo cerrado (USD 355). Ver [DS10_BRINGUP.md](DS10_BRINGUP.md) §6 |
| **DS11** | **En operación — 3 placas**, 32 canales poblados y audio limpio. El KiCad de lo que se fabricó está verificado contra los gerbers en [DS11_RELEASE.md](DS11_RELEASE.md) (tag `ds11-release`) |
| **DS12** | **En diseño — es la tirada de 10 unidades.** Fuente rediseñada, geometría 2×16, auditoría cerrada |

### Mejoras DS10 respecto a DS9

- PCM1802 (2ch) reemplazado por AK5538VN (8ch, 32-bit) — 4× más entradas
- 32× PAM8302A integrados en PCB (antes módulo HAT externo)
- Fuente 20V con buck local por placa (antes fuente externa)
- Supply analógico separado (+3.3VA) para el ADC

---

## Documentación técnica

### DS12 — lo que se está diseñando ahora

- **[DS12_CHECKLIST.md](DS12_CHECKLIST.md) — la lista para tildar mientras dibujás.**
  El índice de todo lo de abajo, en orden de dibujo: qué poner y dónde está el porqué
- [DS12_ALIMENTACION.md](DS12_ALIMENTACION.md) — **La etapa de fuente, lista para dibujar.**
  Qué integrado va en cada lugar, con qué divisor, y por qué. Incluye §6.1: los dos modos
  de falla que *crea* el rediseño y el BAT54 de US$ 0,03 que los cierra
- [DS12_PLANNING.md](DS12_PLANNING.md) — Backlog: geometría 2×16, arnés, polaridad de los
  conectores de parlante (`+ − − +`), correcciones al esquemático
- [DS12_AUDITORIA.md](DS12_AUDITORIA.md) — Auditoría previa a fabricar. Doce dimensiones,
  y **sólo lo que sobrevivió a verificación**: cada ítem dice cómo se comprobó
- [DS12_USB_STM32.md](DS12_USB_STM32.md) — El USB del STM32 (PA11/PA12 hoy al aire)
- [DS12_REVISION_ALIMENTACION.md](DS12_REVISION_ALIMENTACION.md) — **Revisión de la
  alimentación (octubre 2026)**: las fuentes y cómo conviven, contra las hojas de datos, con
  refutación a ciegas. Los planes que la guían son
  [DS12_REVISION_ALIMENTACION_PLAN.md](DS12_REVISION_ALIMENTACION_PLAN.md) y
  [DS12_REVISION_ALIMENTACION_RONDA2.md](DS12_REVISION_ALIMENTACION_RONDA2.md). Las cuentas y
  simulaciones están en [revision_alimentacion/](revision_alimentacion/), todas con autoprueba

### Histórico

- [DS11_RELEASE.md](DS11_RELEASE.md) — Qué se fabricó exactamente, verificado byte a byte
- [DS11_PLANNING.md](DS11_PLANNING.md) — Plan de la revisión DS11
- [DS10_DESIGN_REVIEW.md](DS10_DESIGN_REVIEW.md) — Revisión de diseño y verificación de netlist
- [DS10_BRINGUP.md](DS10_BRINGUP.md) — Bring-up + defectos confirmados de la tanda 1
- [DS10_PRE_PRODUCTION_CHECKLIST.md](DS10_PRE_PRODUCTION_CHECKLIST.md) — Checklist y parámetros JLCPCB
- `Versiones/` — Snapshots del proyecto KiCad tal como se fabricó cada revisión

---

## Cómo está armada la carpeta

| | |
|---|---|
| `DS.kicad_pro/.kicad_sch/.kicad_pcb` | El proyecto activo |
| `DS.kicad_dru` | **Reglas de diseño propias.** Hoy: los 0,1414 mm entre pads del QFN-64 de IC3, que son geometría del footprint y no un ruteo apretado. KiCad lo lee solo por llamarse igual que el proyecto |
| `jlcpcb/production_files/` | **BOM y CPL reales.** La verdad sobre part numbers comprados vive acá, **no** en el netlist: para el 4,7 nF, `DS.txt` dice `C883564` y el BOM dice `C2961271` |
| `DS.txt` | Netlist exportado. Entrada de `tools/netlist_digest.py`, que lo convierte en tres archivos grepables |
| `datasheets/` | Hojas de datos de los componentes de la placa. Las del MCU y los chips de audio están en `../../datasheets/` |
| `librerias/`, `3d/`, `plot/`, `CNC/` | Símbolos y footprints propios, modelos 3D, PDFs del esquemático, corte de la caja |

### Antes de exportar gerbers

```bash
tools/check_pcb.sh --install    # una vez por copia del repo
```

KiCad **no recalcula los rellenos de zona al plotear**: usa el cobre guardado en el
`.kicad_pcb`. Ese mecanismo ya arruinó dos tandas de la DS11
([docs/ds11_errata.md](../../docs/ds11_errata.md)) y apareció una tercera vez en septiembre.
El hook lo verifica solo. Y al exportar: `kicad-cli pcb export gerbers --check-zones`.
