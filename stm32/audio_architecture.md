# Audio Architecture - Domosonica DS

## Block Diagram

```
                    ╔═══════════════════════════════════════════════════════════╗
                    ║              STM32H743VIT6 (480 MHz)                      ║
                    ║                                                           ║
┌─────────┐         ║   ┌──────────────────────────────────────────┐           ║
│ HSE     │ 25MHz   ║   │         PLL3 Configuration               │           ║
│ Crystal ├─────────╫───┤ DIVM3=5, DIVN3=68, DIVP3=28             │           ║
└─────────┘         ║   │ VCO=340MHz → Output=12.288MHz            │           ║
                    ║   └────────┬───────────────┬──────────────┬──┘           ║
                    ║            │               │              │              ║
                    ║            ▼               ▼              ▼              ║
                    ║   ┌────────────┐  ┌────────────┐  ┌────────────┐        ║
                    ║   │  SAI1_A    │  │  SAI1_B    │  │  SAI4_B    │        ║
                    ║   │  Master TX │  │  Master TX │  │  Master RX │        ║
                    ║   │  TDM8      │  │  TDM8      │  │  I2S 2ch   │        ║
                    ║   │            │  │  (sync)    │  │            │        ║
                    ║   └──┬──┬──┬───┘  └──┬──┬──┬───┘  └──┬──┬──┬───┘        ║
                    ║      │  │  │         │  │  │         │  │  │            ║
                    ║      │  │  └MCLK────┼──┼──┼─────────┼──┼──┼──► PE2      ║
                    ║      │  │    12.288 │  │  │    12.288 MHz│              ║
                    ║      │  └FS─────────┘  │  │            │  │             ║
                    ║      │    (48kHz)      │  │            │  │             ║
                    ║      └SCK───────────────┘  │            │  │             ║
                    ║         (3.072MHz)         │            │  │             ║
                    ║                            │            │  │             ║
                    ║   PE4 ◄────────────────────┘            │  │             ║
                    ║   PE5 ◄─────────────────────────────────┘  │             ║
                    ║   PE6 ◄────────────────────────────────────┘             ║
                    ║   PE9, PE8, PE7 ◄── SAI1_B signals                       ║
                    ║   PC0, PC1, PB11 ◄── SAI4_B signals                      ║
                    ║                                                           ║
                    ║   ┌──────────┐  ┌──────────┐  ┌──────────┐              ║
                    ║   │  DMA1    │  │  DMA1    │  │  DMA2    │              ║
                    ║   │ Stream 0 │  │ Stream 5 │  │ Stream 2 │              ║
                    ║   │ (SAI1_A) │  │ (SAI1_B) │  │ (SAI4_B) │              ║
                    ║   └────┬─────┘  └────┬─────┘  └────┬─────┘              ║
                    ║        │             │             │                     ║
                    ╚════════╪═════════════╪═════════════╪═════════════════════╝
                             │             │             │
                             ▼             ▼             ▼
                    ┌────────────┐  ┌────────────┐  ┌────────────┐
                    │  DAC0      │  │  DAC1      │  │  ADC       │
                    │  PCM1690   │  │  PCM1690   │  │  PCM1802   │
                    │  I2C: 0x4C │  │  I2C: 0x4E │  │  I2C: 0x4D │
                    │            │  │            │  │            │
                    │  MCLK ◄────┼──┼── PE2 ─────┼──┼───────────►│
                    │  BCK  ◄────┼  │  12.288MHz │  │            │
                    │  LRCK ◄────┼  │            │  │            │
                    │  DIN1 ◄ PE6│  │            │  │            │
                    │            │  │  BCK  ◄ PE8│  │  BCK  ◄────┼── PC1
                    │  8ch OUT   │  │  LRCK ◄ PE9│  │  LRCK ◄────┼── PC0
                    │  ┌─┬─┬─┬─┐ │  │  DIN1 ◄ PE7│  │  DOUT ─────┼─► PB11
                    │  │1│2│3│4│ │  │            │  │            │
                    │  └─┴─┴─┴─┘ │  │  8ch OUT   │  │  2ch IN    │
                    │  ┌─┬─┬─┬─┐ │  │  ┌─┬─┬─┬─┐ │  │  ┌───┬───┐ │
                    │  │5│6│7│8│ │  │  │9│A│B│C│ │  │  │ L │ R │ │
                    └──┴─┴─┴─┴─┴──┘  └──┴─┴─┴─┴─┴──┘  └──┴───┴───┴──┘

                    ┌───────────────────────────────────────────────────┐
                    │        Control Interface (I2C1)                   │
                    │  PB8 (SCL) ──┬────────┬────────┬─────────         │
                    │  PB9 (SDA) ──┼────────┼────────┼─────────         │
                    │              │        │        │                  │
                    │              ▼        ▼        ▼                  │
                    │           DAC0     DAC1     ADC                   │
                    │          (0x4C)   (0x4E)   (0x4D)                 │
                    └───────────────────────────────────────────────────┘
```

## Signal Flow

### Output Path (STM32 → Speakers)
1. **Audio Processing** (CPU/DSP)
   - Read input from ADC via DMA
   - Apply beamforming delays
   - Process audio (EQ, compression, etc.)
   - Write to output buffers

2. **DMA Transfer**
   - DMA1 Stream 0: 8 channels → SAI1_A FIFO
   - DMA1 Stream 5: 8 channels → SAI1_B FIFO

3. **SAI Transmission**
   - SAI1_A generates MCLK (12.288 MHz)
   - SAI1_A generates BCK (3.072 MHz) and LRCK (48 kHz)
   - SAI1_B synchronized with SAI1_A
   - Data sent in TDM format (8 slots × 32 bits)

4. **DAC Conversion**
   - PCM1690 receives I2C config
   - Converts 24-bit digital → analog
   - Outputs 8 channels per DAC

### Input Path (Mic → STM32)
1. **ADC Capture**
   - PCM1802 samples @ 48 kHz
   - Uses MCLK from PE2
   - Receives BCK/LRCK from SAI4 (master mode)

2. **SAI Reception**
   - SAI4_B in master RX mode
   - Generates BCK/LRCK for ADC
   - Receives data on PB11

3. **DMA Transfer**
   - DMA2 Stream 2: ADC data → memory buffer

4. **Audio Processing**
   - CPU reads from input buffer
   - Processes for beamforming

## Clock Tree

```
HSE (25 MHz)
    │
    ├─► PLL1 → 480 MHz (CPU clock)
    │
    └─► PLL3 (Audio)
         ├─ DIVM3 = 5  → 5 MHz reference
         ├─ DIVN3 = 68 → 340 MHz VCO
         └─ DIVP3 = 28 → 12.288 MHz
              │
              ├─► SAI1_A kernel clock
              │    └─► MCLK = 12.288 MHz (MCKDIV=0)
              │         ├─► BCK = 3.072 MHz (256 bits/frame)
              │         └─► LRCK = 48 kHz
              │
              ├─► SAI1_B kernel clock (synchronized with SAI1_A)
              │
              └─► SAI4_B kernel clock
                   └─► Generates BCK/LRCK for ADC
```

## Memory Map

### DMA Buffers (AXI SRAM - 512 KB)
```
Address Range          | Usage
-----------------------|--------------------------------
0x24000000-0x24001FFF  | SAI1_A TX Buffer (8 KB, ping-pong)
0x24002000-0x24003FFF  | SAI1_B TX Buffer (8 KB, ping-pong)
0x24004000-0x24005FFF  | SAI4_B RX Buffer (8 KB, ping-pong)
```

### Delay Buffers (SRAM1/2 - 256 KB)
```
Address Range          | Usage
-----------------------|--------------------------------
0x30000000-0x3003FFFF  | Delay lines para 16 canales
                       | 16 canales × 16 KB = 256 KB
                       | Max delay: ~4800 samples @ 48kHz = 100ms
```

### Critical Variables (DTCM - 128 KB)
```
Address Range          | Usage
-----------------------|--------------------------------
0x20000000-0x2001FFFF  | Stack, variables globales, flags
```

## Synchronization Strategy

### Timing Master: SAI1_A
- Generates MCLK que alimenta todos los chips
- Genera BCK y LRCK para DAC0
- SAI1_B se sincroniza con SAI1_A (SYNCEN=01)
- SAI4_B usa el mismo MCLK pero genera sus propios BCK/LRCK

### Critical Constraints
1. **MCLK Distribution**
   - PE2 debe tener length-matched traces (±0.05mm)
   - R33 series resistor (33Ω) en cada línea de clock

2. **SAI Timing**
   - SAI1_A: Master, asynchronous
   - SAI1_B: Master, synchronous with SAI1_A
   - SAI4_B: Master RX, asynchronous (genera clocks para ADC)

3. **DMA Configuration**
   - Circular mode para continuous streaming
   - Double buffer (ping-pong) para evitar underruns
   - Priority: SAI1_A=High, SAI1_B=High, SAI4_B=Medium

## Power Distribution

```
12V Input
  │
  ├─► AMS1117-5.0 → 5V
  │    ├─► PCM1690 #0 AVDD, DVDD
  │    ├─► PCM1690 #1 AVDD, DVDD
  │    ├─► PCM1802 VA
  │    └─► AMS1117-3.3 → 3.3V
  │         ├─► PCM1690 #0 VDD
  │         ├─► PCM1690 #1 VDD
  │         └─► PCM1802 VDD
  │
  └─► STM32 WeAct Board (usa su LDO interno)
```

## I2C Configuration

### Bus: I2C1 @ 100 kHz
- SCL: PB8 (5.1kΩ pull-up to 3V3)
- SDA: PB9 (5.1kΩ pull-up to 3V3)

### Devices
| Device  | Address (7-bit) | Strapping Pins      |
|---------|-----------------|---------------------|
| DAC0    | 0x4C           | AD1=GND, AD0=GND    |
| DAC1    | 0x4E           | AD1=HIGH, AD0=GND   |
| ADC     | 0x4D           | Fixed address       |

**Note:** Strapping pins HIGH conectados a 3.3V (verificar tolerancia 5V en datasheet)

## GPIO Control Signals

### DAC Control
| Signal       | GPIO | Direction | Device | Function          |
|--------------|------|-----------|--------|-------------------|
| DAC0_RST     | PA6  | Output    | DAC0   | Reset (active high)|
| DAC1_RST     | PA7  | Output    | DAC1   | Reset (active high)|
| DAC0_AMUTEI  | PD1  | Output    | DAC0   | Mute input (active low)|
| DAC0_AMUTEO  | PD3  | Input     | DAC0   | Mute status output|
| DAC1_AMUTEI  | PB1  | Output    | DAC1   | Mute input (active low)|
| DAC1_AMUTEO  | PC5  | Input     | DAC1   | Mute status output|

### ADC Control
| Signal       | GPIO | Direction | Device | Function          |
|--------------|------|-----------|--------|-------------------|
| ADC_PDWN     | PA1  | Output    | ADC    | Power down (active low)|
| ADC_OSR      | PB0  | Output    | ADC    | Oversample ratio  |
| ADC_BYPASS   | PC3  | Output    | ADC    | HPF bypass        |

## Performance Targets

- **Sample Rate:** 48 kHz
- **Bit Depth:** 24-bit
- **Latency:** < 10 ms (target)
- **Delay Range:** 0-100 ms per channel
- **THD+N:** < 0.01% (verificar con test)
- **Channels:** 16 out + 2 in
