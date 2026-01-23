# Domosonica DS - Master Audio Processor

Sistema de audio beamforming con STM32H743VIT6, 16 canales de salida y 2 de entrada.

---

## 📁 Estructura de Documentación

### 🎯 Hardware Overview

| Archivo | Descripción |
|---------|-------------|
| [DS_Hardware_Map.md](DS_Hardware_Map.md) | **Mapa completo del hardware** - Pines, chips, conexiones |
| [PowerSpecs.md](PowerSpecs.md) | Especificaciones de alimentación (8 sistemas @ 12V) |
| [WeAct specs.md](WeAct%20specs.md) | Especificaciones del módulo STM32H743 WeAct |

### 🎵 Audio Components

| Archivo | Descripción |
|---------|-------------|
| [pcm1690_quick_ref.md](pcm1690_quick_ref.md) | DAC PCM1690 - Quick reference (I2C, strapping) |
| [pcm1690_registers.md](pcm1690_registers.md) | DAC PCM1690 - Registros I2C completos |
| [pcm1802_registers.md](pcm1802_registers.md) | ADC PCM1802 - Registros I2C completos |

### 🧮 Audio Processing

| Archivo | Descripción |
|---------|-------------|
| [delays_calculation.md](delays_calculation.md) | Cálculo de delays para beamforming |
| [memory_map.md](memory_map.md) | Mapa de memoria RAM del STM32H743 |

### 📡 Communication

| Archivo | Descripción |
|---------|-------------|
| [datasheets/uart_protocol.md](datasheets/uart_protocol.md) | Protocolo UART con ESP32 |

---

## 🔧 STM32H743 Reference Documentation

### Architecture & System

| Archivo | Descripción |
|---------|-------------|
| [stm32/audio_architecture.md](stm32/audio_architecture.md) | **⭐ Diagrama arquitectura completa** - DMA, SAI, clocks |
| [stm32/system_init_sequence.md](stm32/system_init_sequence.md) | Secuencia completa de inicialización |
| [stm32/CORRECTIONS_RM0433.md](stm32/CORRECTIONS_RM0433.md) | Correcciones verificadas vs RM0433 |

### Clock Configuration

| Archivo | Descripción |
|---------|-------------|
| [stm32/clock_config.md](stm32/clock_config.md) | Referencia completa RCC y PLLs del RM0433 |
| [stm32/rcc_registers_ref.md](stm32/rcc_registers_ref.md) | Registros RCC - PLL3 para 12.288 MHz |

### SAI (Serial Audio Interface)

| Archivo | Descripción |
|---------|-------------|
| [stm32/stm32_sai_config.md](stm32/stm32_sai_config.md) | **⭐ Configuración SAI para proyecto** - TDM8, I2S |
| [stm32/sai_registers_ref.md](stm32/sai_registers_ref.md) | Referencia completa registros SAI |

### DMA & BDMA

| Archivo | Descripción |
|---------|-------------|
| [stm32/dma_mapping_ref.md](stm32/dma_mapping_ref.md) | **⭐ DMA/BDMA completo** - SAI1→DMA1, SAI4→BDMA |

### GPIO & Peripherals

| Archivo | Descripción |
|---------|-------------|
| [stm32/pin_alternate_functions.md](stm32/pin_alternate_functions.md) | Tabla pines proyecto con código HAL |
| [stm32/gpio_af_mapping_ref.md](stm32/gpio_af_mapping_ref.md) | Referencia GPIO registers y AF mapping |
| [stm32/i2c_timing_ref.md](stm32/i2c_timing_ref.md) | I2C timing calculations (100 kHz @ 120 MHz) |

---

## 🎛️ Quick Reference

### Hardware Configuration

**Audio Chips:**
- 2× PCM1690 DACs (8ch each) → I2C addresses: 0x4C, 0x4E
- 1× PCM1802 ADC (2ch) → I2C address: 0x4D

**Clock:**
- HSE: 25 MHz crystal
- PLL3: 12.288 MHz (256 × 48 kHz)
- Sample Rate: 48 kHz
- Bit Depth: 24-bit (stored as 32-bit)

**SAI Configuration:**
- SAI1_A → DAC0 (8ch TDM) - PE2, PE4, PE5, PE6
- SAI1_B → DAC1 (8ch TDM) - PE2, PE7, PE8, PE9
- SAI4_B → ADC (2ch I2S) - PE2, PC0, PC1, PB11

**DMA Configuration:**
- SAI1_A: DMA1 Stream 0 (DMAMUX1 request 87)
- SAI1_B: DMA1 Stream 5 (DMAMUX1 request 88)
- SAI4_B: **BDMA Channel 7** (DMAMUX2 request 16) ⚠️

### Memory Budget

- **Delay buffers:** 256 KB (SRAM1/2) - 16 canales × 16 KB
- **DMA buffers:** AXI SRAM - Ping-pong double buffer
- **Stack/vars:** DTCM (128 KB) - Fastest access

### Power

- Single board: ~6W @ 12V (500 mA)
- 8 systems: 48W @ 12V (4A)
- Power supply: Mean Well LRS-100-12 (102W, 8.5A)

---

## 📝 Development Notes

### Files Updated (2026-01-22)

**Critical Corrections:**
- ✅ Verified all SAI pins against hardware netlist
- ✅ Corrected SAI4 to use BDMA instead of DMA2
- ✅ All register values verified against RM0433

**Cleaned Up:**
- ❌ Deleted DS.net (in context)
- ❌ Deleted obsolete DMA mapping files
- ❌ Deleted duplicate SAI register files
- ❌ Deleted rm0433.txt (6.5MB - can regenerate)

### Important Reminders

1. **SAI4 uses BDMA, not DMA!** - It's in D3 domain, must use BDMA with DMAMUX2
2. **I2C addresses are 7-bit** - 0x4C, 0x4E, 0x4D (not 8-bit 0x98, 0x9C, 0x9A)
3. **MCLK is shared** - PE2 drives all three audio chips via resistor network
4. **Double buffer required** - For continuous audio streaming without gaps

---

## 🔗 External References

**Datasheets (in datasheets/ folder):**
- rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
- stm32h743vi.pdf
- es0392-stm32h742xig-stm32h743xig-stm32h750xb-stm32h753xi-device-errata-stmicroelectronics.pdf
- an4838-introduction-to-memory-protection-unit-management-on-stm32-mcus-stmicroelectronics.pdf

**To regenerate rm0433.txt if needed:**
```bash
pdftotext datasheets/rm0433-*.pdf datasheets/rm0433.txt
```

---

## 📊 Project Status

**Hardware:** ✅ PCB verified - All connections correct
**Documentation:** ✅ Complete and verified against RM0433
**Firmware:** 🔄 Ready to implement

**Next Steps:**
1. Configure STM32CubeIDE project
2. Initialize PLL3 for 12.288 MHz
3. Configure SAI1_A, SAI1_B (DMA), SAI4_B (BDMA)
4. Implement audio buffers and processing
5. Configure PCM1690/PCM1802 via I2C

---

**Last Updated:** 2026-01-22
**Verified Against:** RM0433 Rev 8, DS.net (hardware netlist)
