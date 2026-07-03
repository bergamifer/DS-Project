# DS_Amp — Notas de Diseño

## Arquitectura General
- 32x PAM8302A (mono 2.5W Class-D, entrada diferencial) integrados en PCB DS10
- Alimentación: 24V DC externo → Buck 24V→5V en PCB
- Speakers: 0.5-1W, 8 Ohm, reciclados de TV
- Escala objetivo: ~500 speakers via ~18 cajas DS10
- Distribución audio: Cat5e con pares trenzados (8 canales diferenciales por cable)
- Distribución power: Bus 24V daisy-chain + buck local por placa

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

## Distribución de Audio (Cat5e/RJ45)

### Problema
~500 speakers distribuidos en ~18 cajas a lo largo de una instalación.
Cada placa DS10 tiene 32 canales de salida diferencial desde 4x PCM1690.

### Solución: Cat5e con pares trenzados
- Cat5e tiene 4 pares trenzados → 4 canales diferenciales por cable
- 2x Cat5e por placa → 8 canales diferenciales por enlace
- 32 canales / 8 = 4 enlaces Cat5e por placa DS10
- Conector: RJ45 estándar (barato, robusto, fácil de cablear)

### Topología
```
DS10 [RJ45 x4] ──Cat5e──> Caja 1 [RJ45 in/out] ──Cat5e──> Caja 2 ...
```
- Daisy-chain con passthrough RJ45 en cada caja
- Cada caja recibe los 8 pares y toma los que necesita localmente
- Señal diferencial tolera bien cables largos (>10m) con pares trenzados

### Asignación de pines RJ45 (T568B)
| Par Cat5e | Pines RJ45 | Canal |
|---|---|---|
| Par 1 (naranja) | 1, 2 | VP1/VN1 |
| Par 2 (verde) | 3, 6 | VP2/VN2 |
| Par 3 (azul) | 4, 5 | VP3/VN3 |
| Par 4 (marrón) | 7, 8 | VP4/VN4 |

## Distribución de Alimentación (24V Bus)

### Problema
A 5V, 18 cajas con 32 speakers cada una requieren ~77A total — imposible distribuir.

### Solución: Bus 24V + Buck local
- Fuente externa: 24V DC (ej. 24V/10A = 240W para el sistema completo)
- Distribución: Cable 14AWG (~16A max) en daisy-chain entre cajas
- Conversión local: 1x Buck 24V→5V por placa DS10
- Ventajas:
  - Corriente 5x menor que a 5V (I = P/V)
  - Cables más delgados, menos pérdidas
  - Cada placa regula su propio 5V independientemente

## Fuente de Alimentación en PCB — Diseño Definitivo

### Arquitectura de Power
```
24V DC (externo) ──> Buck 24V→5V (1 solo, en PCB) ──┬──> 5V_AMP (32× PAM8302A)
                                                      |
                                                   [Ferrite bead]
                                                      |
                                                      ├──> LDO 5V→3.3V digital (STM32, ESP32)
                                                      |
                                                      └──> LDO 5V→3.3VA (AP2125K-3.3)
                                                              └──> AVDD (PCM1690, AK5538)
```

### Por qué 1 solo buck (no 2)
- PAM8302A tiene 77dB PSRR — rechaza ruido de su alimentación
- AVDD pasa por AP2125K-3.3 LDO (75dB PSRR adicional)
- 3.3V digital pasa por otro LDO
- 2 etapas de filtrado entre buck y circuitos sensibles = suficiente
- Un segundo buck agrega complejidad sin beneficio audible

### Buck recomendado: MP2315S
- Rango entrada: 4.5V-24V
- Salida: 5V, 3A continuo (margen: amps ~2A promedio + ~1A digital)
- Package: SOT-23-8
- JLCPCB: Basic (sin fee de Extended)
- Switching: 500kHz (inductor chico)
- Componentes externos mínimos

### Componentes del buck
| Parte | Valor | Requisito | Notas |
|---|---|---|---|
| Inductor | 10uH blindado | Blindado obligatorio | Reduce radiación EMI |
| Cap entrada | 22uF X5R 50V | Cerca del IC | Cerámico |
| Cap salida | 22uF X5R 16V x2 | ESR bajo | Cerámico, ripple mínimo |
| Ferrite bead | 600Ohm@100MHz >=2A | Entre 5V_AMP y 5V_CLEAN | Aislamiento amp/digital |
| Cap post-ferrite | 47uF | Después del ferrite | Antes de los LDOs |

### Layout del buck en PCB
- Ubicación: extremo izquierdo del PCB (lejos de señales analógicas)
- Todo en L1, GND return por L2 con vías anchas
- Polígono de cobre debajo del IC para disipación térmica
- 5V_AMP sale hacia la derecha (zona PAM8302A)
- 5V_CLEAN sale después del ferrite hacia LDOs (zona centro-izquierda)

### Lo que NO se necesita
- Segundo buck — los LDOs limpian suficiente
- Filtro LC de segundo orden — ferrite + cap basta
- Regulador externo — todo integrado en PCB
