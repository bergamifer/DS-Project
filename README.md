# Domosonica DS10 — 32-Channel Beamforming Audio System

Sistema de audio espacial de 32 canales con amplificadores Class-D integrados, diseñado para instalaciones artísticas con ~500 speakers distribuidos en ~18 cajas.

## Arquitectura del Sistema

```
STM32H743 (MCU) ──SAI1/SAI2──> 4x PCM1690 (DAC 8ch c/u) ──differential──> 32x PAM8302A (Class-D) ──> Speakers
       |                              |
       ├──UART──> ESP32-C3-Mini      AK5538VN (ADC 8ch) <──differential── Mic inputs
       |          (WiFi/BLE)               |
       ├──I2C1──> DAC control         SAI3 (TDM256)
       ├──I2C4──> EEPROM (CAT24C256)       |
       └──USB-C   (programming)       STM32H743
```

## Componentes Principales

| Componente | IC | Cantidad | Funcion |
|---|---|---|---|
| MCU | STM32H743VIT6 (LQFP100) | 1 | Control central, SAI, I2C, USB |
| DAC | PCM1690 (8ch, differential) | 4 | 32 canales de audio, salida diferencial |
| ADC | AK5538VN (8ch, 32-bit, TDM256) | 1 | 8 entradas de micrófono diferencial |
| Amplificador | PAM8302A (mono 2.5W Class-D) | 32 | Amplificación directa a speakers |
| WiFi/BLE | ESP32-C3-Mini | 1 | Conectividad inalámbrica |
| EEPROM | CAT24C256 | 1 | Almacenamiento de configuración |
| LDO analogico | AP2125K-3.3 | 1 | AVDD limpio para DACs y ADC |

## Alimentacion

```
24V DC externo ──> MP2315S Buck (24V→5V, 3A) ──┬──> 5V_AMP (32x PAM8302A)
                                                 |
                                              [Ferrite]
                                                 |
                                                 ├──> LDO → 3.3V digital
                                                 └──> AP2125K → 3.3VA analogico
```

- Entrada: 24V DC bus (daisy-chain entre cajas, cable 14AWG)
- Buck local: MP2315S en cada placa (SOT-23-8, JLCPCB Basic)
- Aislamiento: Ferrite bead entre 5V_AMP y 5V_CLEAN

## Distribucion de Audio

- 8 canales diferenciales por enlace Cat5e/RJ45 (4 pares trenzados)
- 4 enlaces Cat5e por placa DS10 (32ch / 8 = 4)
- Topologia daisy-chain con passthrough RJ45 en cada caja
- Nomenclatura pares: VP1/VN1 ... VP32/VN32

## PCB — 4 Capas

| Capa | Funcion |
|---|---|
| L1 (Top) | Componentes + señales digitales (izq) + pares diferenciales (der) |
| L2 (GND) | Plano solido — sin trazas |
| L3 (Power) | Zonas 3.3V digital, 5V, AVDD analogico |
| L4 (Bottom) | Pasivos, aux, salidas PAM8302A a speakers |

- Stackup: JLCPCB JLC04161H-7628 (1.6mm, 1oz/0.5oz/0.5oz/1oz)
- Impedancia diferencial objetivo: 100 Ohm en L1

## Red de Entrada DAC → Amp (por canal diferencial)

```
PCM1690 VOUT+ ──[1kOhm]──┬──[0.22uF]──> PAM8302A A+
                          |
                       [4.7nF]
                          |
                         GND
```

- LPF: 1kOhm + 4.7nF C0G (fc=34kHz, atenua ruido delta-sigma)
- Acople: 0.22uF X7R (fc=72Hz con RI=10kOhm interno)
- EMI salida: Ferrite beads + 220pF en OUT+/OUT-

## Fabricacion

- **Fabricante**: JLCPCB
- **KiCad**: 9.0
- **PCB**: 4 capas, FR-4, 1.6mm, HASL
- **Ensamblaje**: SMD ambos lados

## Documentacion

- [DS_Amp_Design_Notes.md](DS_Amp_Design_Notes.md) — Notas detalladas de diseño (amplificadores, fuente, distribucion, componentes LCSC)

## Bugs Conocidos (DS10)

1. **VREF1 swap en AK5538VN**: R8 conecta VREFL1 (pin 61) a +3.3VA en lugar de VREFH1 (pin 62)
2. **AK5538VN I2C flotante**: SDA/SCL del ADC no conectados a ningun bus I2C del STM32
