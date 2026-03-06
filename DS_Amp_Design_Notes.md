# DS_Amp — Notas de Diseño

## Arquitectura General
- 32x PAM8302A (mono 2.5W Class-D, entrada diferencial)
- Alimentación: 5V/10A PSU externa (independiente del DS9)
- Speakers: 0.5-1W, 8 Ohm, reciclados de TV
- Escala objetivo: ~500 speakers via múltiples pares DS9+Amp

## PAM8302A — Specs Clave
- Entrada diferencial: A+ (pin 3), A- (pin 4)
- Resistores de ganancia internos: RI=10kOhm, RF=150kOhm
- Ganancia por defecto: ~24dB (sin componentes externos)
- Reducción de ganancia opcional: RIN externo en serie
  - Ganancia = 20 * log(150k / (10k + RIN))
- fosc: 200-300kHz (varía entre chips)
- Eficiencia: ~88% @ 8 Ohm

## Red de Entrada Completa (por línea diferencial)

```
PCM1690 VOUT+ ──[1kOhm]──┬──[0.22uF CI]──> PAM8302A A+ (pin 3)
                          |
                       [4.7nF]
                          |
                         GND

PCM1690 VOUT- ──[1kOhm]──┬──[0.22uF CI]──> PAM8302A A- (pin 4)
                          |
                       [4.7nF]
                          |
                         GND
```

### Filtro Pasa-Bajos (LPF) — Ruido Delta-Sigma
- R serie: 1kOhm 1% 0402
- C shunt: 4.7nF C0G 5% 0402 (C0G obligatorio para coherencia de fase)
- fc_LPF = 1/(2*pi*1k*4.7n) = ~34kHz
- Atenúa ruido delta-sigma del PCM1690 por encima de la banda de audio
- Primer orden (-20dB/década) suficiente porque:
  - PAM8302A es Class-D (genera su propio ruido HF)
  - Speakers de TV ruedan naturalmente por encima de 8-12kHz
  - Operación a baja potencia (0.5-1W)

### Capacitor de Acople (CI) — PAM8302A datasheet pág. 7
- HPF: fc = 1/(2*pi*RI*CI), donde RI = 10kOhm (interno)
- Rango recomendado por datasheet: 0.1uF a 0.22uF
  - 0.1uF  -> fc = 159Hz (corta bajos)
  - 0.16uF -> fc = 100Hz (plano hasta 100Hz)
  - 0.22uF -> fc = 72Hz  (mejor respuesta de bajos, recomendado)

### Conteo de Componentes (red de entrada)
- Por canal: 2x 1kOhm + 2x 4.7nF + 2x 0.22uF = 6 partes
- Total 32 canales: 192 componentes pasivos
- Costo estimado: ~$4

## Filtro EMI en Salida (PAM8302A datasheet pág. 8)
Recomendado si cables a speakers > 20cm:

```
OUT+ ──[Ferrite Bead]──┬──> Speaker+
                       |
                    [220pF]
                       |
OUT- ──[Ferrite Bead]──┴──> Speaker-
```

## Componentes LCSC/JLCPCB

### Red de entrada
| Componente | Valor | Package | LCSC | Notas |
|---|---|---|---|---|
| R serie LPF | 1kOhm 1% | 0402 | C11702 | UniOhm RC0402FR-071KL |
| C shunt LPF | 4.7nF C0G 5% | 0402 | C1604 | SAMSUNG CL05C472JB5NNNC |
| CI acople | 0.22uF X7R | 0402 | C307331 | YAGEO CC0402KRX7R7BB224 |

### Alternativa CI (si se prefiere array compacto)
| Componente | Valor | Package | LCSC | Notas |
|---|---|---|---|---|
| CI array | 4x 100nF X7R | 0402x4 | C1870407 | YAGEO CA0508KRX7R7BB104, fc=160Hz |

### Filtro EMI salida
| Componente | Valor | Package | LCSC | Notas |
|---|---|---|---|---|
| Ferrite bead | 600Ohm@100MHz 1.5A | 0603 | C108648 | Murata BLM18AG601SN1D |
| Ferrite bead (alt) | 600Ohm@100MHz 2A | 0805 | C1543 | Sunlord SDCW2012S601HT |
| Cap EMI | 220pF C0G 50V | 0402 | C1588 | SAMSUNG CL05C221JB5NNNC |

## Coherencia de Fase (Beamforming)

Fuentes de error de fase entre canales:

| Fuente | Error @ 10kHz | Notas |
|---|---|---|
| PCM1690 DAC (mismo MCLK) | < 0.1° | Negligible |
| RC LPF (1kOhm + 4.7nF C0G ±5%) | ~3° | Aceptable |
| PAM8302A fosc (200-300kHz) | ~3° | Tolerable |
| CI acople (0.22uF ±10%) | < 1° | Negligible |
| **Total peor caso** | **~6°** | **Aceptable para audio espacial** |

A 20kHz el error total sube a ~12°.
Para speakers de TV con rango útil < 10kHz: aceptable.
Mitigación: calibración por software en firmware STM32 al inicio.

## Notas de PCB (4 capas)

### Stackup JLCPCB: JLC04161H-7628
```
F.Cu:          0.035mm  (1oz)
PrePreg 7628:  0.2104mm (Er=4.6)
In1.Cu:        0.0152mm (0.5oz) — GND plano sólido
Core FR4:      1.065mm  (Er=4.6)
In2.Cu:        0.0152mm (0.5oz) — Power zones
PrePreg 7628:  0.2104mm (Er=4.6)
B.Cu:          0.035mm  (1oz)
Total:         ~1.6mm
```

### Estrategia de capas
- L1 (Top): Componentes + señales digitales (izq) + pares diferenciales (der)
- L2 (GND): Plano sólido, SIN trazas — sagrado
- L3 (Power): Zonas 3.3V digital, 5V, AVDD analógico
- L4 (Bottom): Componentes pasivos, salidas PAM8302A a speakers

### Decoupling
- 100nF HF: en L1 pegados al IC (STM32, PCM1690 AVDD, ESP32, PCM1802)
- Bulk (4.7uF, 10uF): en L4 con vías, OK

### Pares diferenciales en L1
- Impedancia objetivo: 100 Ohm diferencial
- Sin vías: DACs y conectores ambos en top
- Separación entre pares: mín 3x ancho de traza (~0.6mm)
- Nomenclatura: VP1/VN1, VP2/VN2, ... VP32/VN32
- Wildcard KiCad: VP[1..32] / VN[1..32]
