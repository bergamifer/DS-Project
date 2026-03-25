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
| Buck converter | MP2315S (24V→5V, 3A) | 1 | Fuente principal desde 24V DC |
| LDO digital | AP1117-33 | 1 | +3V3 para STM32, ESP32, digital |
| LDO analogico | AP2125K-3.3 | 1 | +3.3VA limpio para ADC AVDD y VREF |

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

## Estado de Verificacion (2026-03-25)

### Fixes completados
- VREFL1-4 conectados a GND (antes VREFL1 iba a +3.3VA)
- AK5538VN I2C conectado via I2C3 (U7/66=SDA, U7/67=SCL, AF4)
- FB67 eliminado (bridgeaba +3V3 con +3.3VA)
- PCM1802 sheet eliminado del schematic
- TDM0/TDM1 corregidos: TDM256 mode (TDM1=H, TDM0=L)
- VBAT (U7/6) conectado a +3V3
- GND unificado (no mas GNDA/GNDD/GND2)
- Netlist script corregido (null check + auto-save a netlist.md)

### Verificado por netlist (570 nets, completo)
- SAI1 → DAC0(U2)+DAC1(U3), SAI2 → DAC2(U1)+DAC3(U4)
- SAI3_B → AK5538VN (BICK=U7/55, SDTO1=U7/56, LRCK=U7/57)
- MCLK 12.288MHz compartido: DACs + ADC + extension header
- 4 DACs con I2C addresses unicos (0x4C-0x4F) en I2C1
- +3.3VA: U10(AP2125K) output → IC3 AVDD + VREFH network
- +24V → IC2(MP2315S) → L1 → +5VA → FB2 → +5V → LDOs

### DRC restante (19 violations)
- 5x vias con drill 0.2mm (min 0.3mm) — resize pendiente
- 2x unconnected pads (+5V a C59, zonas +3V3)
- 2x dangling tracks (AIN7P/AIN7N en In2.Cu)
- Resto: warnings cosmeticos (silk text, lib mismatch)

### ERC restante (10 errores)
- 6x power_pin_not_driven — agregar PWR_FLAG
- 3x AMUTEO output-output — intencional, suprimir
- Warnings de labels duplicados — cleanup pendiente
