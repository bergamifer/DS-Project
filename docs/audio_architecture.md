# DS Beamforming — Arquitectura de Audio v4.0

**Placa:** DS10 (STM32H743VIT6 + AK5538VN + 4×PCM1690 + 32×PAM8302A)
**Estado:** DS10 firmware listo — pendiente de hardware
**Última revisión:** Abril 2026 — actualizado desde DS10_DESIGN_REVIEW.md netlist 2026-04-02

---

## 1. Resumen del Sistema

```
                        ┌───────────────────────────────────────────────────────────────────┐
                        │                       STM32H743VIT6                               │
                        │                                                                   │
  8× Micrófonos ───────►│ SAI3_B RX TDM256: PD8(SCK) PD9(SD) PD10(FS) AF6                 │
  (vía AK5538VN IC3)    │   AK5538VN → 8ch 32-bit, STM32=Master, AK5538=Slave (MSN=GND)   │
                        │   I2C3: PA8(SCL)/PC9(SDA) AF4 → AK5538 config                   │
                        │                                                                   │
                        │ PE2 (SAI1_MCLK AF6, 24.576 MHz) ─── MCLK Bus → todos los ICs    │
                        │                                                                   │
                        │ SAI1_A: PE4(FS)/PE5(SCK)/PE6(SD) AF6 → PCM1690 U2 (DAC0) ch0-7 │──► 8× PAM8302A
                        │ SAI1_B: PE3(SD) AF6               → PCM1690 U3 (DAC1) ch8-15   │──► 8× PAM8302A
                        │ SAI2_A: PD12(FS)/PD13(SCK)/PD11(SD) AF10 → PCM1690 U1 ch16-23  │──► 8× PAM8302A
                        │ SAI2_B: PE11(SD) AF10             → PCM1690 U4 (DAC3) ch24-31  │──► 8× PAM8302A
                        │                                                                   │
                        │ I2C1 (PB8/PB9 AF4) ──── DAC config (4× PCM1690)                 │
                        │ I2C4 (PB6/PB7 AF11) ─── EEPROM CAT24C256 (U8, 32KB)             │
                        │ UART4 (PA0/PA1 AF8) ──── ESP32-C3-Mini U5 (WiFi control)         │
                        └───────────────────────────────────────────────────────────────────┘
```

---

## 2. Componentes de Audio

| Componente | Ref | Función | SAI Block | I2C Addr (8-bit) | Canales |
|------------|-----|---------|-----------|-------------------|---------|
| AK5538VN   | IC3 | ADC 8ch TDM256 | SAI3_B (Master RX) | 0x28 (I2C3) | 8 entrada |
| PCM1690    | U2  | DAC 8ch TDM8   | SAI1_A (Master TX) | 0x98 (I2C1) | ch 0-7 |
| PCM1690    | U3  | DAC 8ch TDM8   | SAI1_B (Slave TX)  | 0x9C (I2C1) | ch 8-15 |
| PCM1690    | U1  | DAC 8ch TDM8   | SAI2_A (Master TX) | 0x9A (I2C1) | ch 16-23 |
| PCM1690    | U4  | DAC 8ch TDM8   | SAI2_B (Slave TX)  | 0x9E (I2C1) | ch 24-31 |
| PAM8302A   | —   | Class-D amp 2.5W | GPIO (PAM_SD PD3) | —          | 32 canales |
| **Total**  |     |                |                    |             | **8 in / 32 out** |

> **DS9→DS10 cambio de ADC:** PCM1802 (2ch estéreo) reemplazado por AK5538VN (8ch TDM256).
> **DS9→DS10 cambio designadores PCM1690:** U3→U2, U4→U3, U2→U1, U8→U4.

---

## 3. SAI Pin Mapping (DS10 — verificado contra datasheet STM32H743 Tables 13-14)

| SAI Block | Modo | FS | SCK | SD | AF | DMA Stream | DMAMUX |
|-----------|------|-----|------|------|------|------------|--------|
| SAI1_A | Master TX TDM8   | PE4  | PE5  | PE6  | AF6  | DMA1_Stream0 | 87  |
| SAI1_B | Slave TX sync    | —    | —    | PE3  | AF6  | DMA1_Stream3 | 88  |
| SAI2_A | Master TX TDM8   | PD12 | PD13 | PD11 | AF10 | DMA1_Stream1 | 89  |
| SAI2_B | Slave TX sync    | —    | —    | PE11 | AF10 | DMA1_Stream2 | 90  |
| SAI3_B | Master RX TDM256 | PD10 | PD8  | PD9  | AF6  | DMA1_Stream4 | 114 |

**MCLK:** PE2 (SAI1_MCLK_A, AF6, 24.576 MHz) — bus compartido por los 4 PCM1690 y AK5538VN.

**SAI Sync:** SAI1_B y SAI2_B usan SCK/FS internamente desde su bloque A (sync mode). Solo necesitan pin SD. Arrancar slave **antes** que master.

---

## 4. Clocks

```
HSE 25 MHz ──►  PLL1: M=5 N=192 P=2  ──►  SYSCLK = 480 MHz
                                           HCLK = 240 MHz (AHB/2)
                                           APBx  = 120 MHz

HSE 25 MHz ──►  PLL3: M=5 N=68 P=14 FRACN=6660  ──►  VCO = 344.0625 MHz
                                                        PLL3P = 24.576 MHz
                                                              │
                                                         SAI1_MCLK (PE2)
                                                         MckOverSampling=ENABLE
                                                              │
                                             24.576 MHz MCLK Bus (512 × fS)
                                      ┌──────┬────────┬────────┬────────┬────────┐
                                      ▼      ▼        ▼        ▼        ▼        ▼
                                  AK5538  PCM1690  PCM1690  PCM1690  PCM1690
                                   IC3      U2       U3       U1       U4
                                   ADC     DAC0     DAC1     DAC2     DAC3
```

| Señal | Frecuencia | Relación |
|-------|------------|----------|
| MCLK  | 24.576 MHz | 512 × fS (MckOverSampling=ENABLE en SAI1_A/SAI2_A) |
| BCK (TDM8) | 12.288 MHz | 256 × fS (8 slots × 32 bits × 48 kHz) |
| LRCK / FS | 48 kHz | fS |

**Por qué MCLK = 512×fS:** Los PCM1690 auto-detectan la relación SCKI/fS. Con 256×fS entran en "dual-rate mode" (ch5-8 van a DIN2 que no está conectado). Con 512×fS usan "single-rate mode" y los 8 canales van al mismo DIN1.

---

## 5. AK5538VN — ADC 8 Canales

| Parámetro | Valor |
|-----------|-------|
| Interfaz audio | TDM256 slave (32-bit, MSN=GND hardware-strapped) |
| SAI STM32 | SAI3_B Master RX — STM32 genera BICK y LRCK |
| Interfaz control | I2C3 (PA8 SCL, PC9 SDA, AF4) |
| Dirección I2C | 0x28 (8-bit write), 7-bit 0x14 |
| Control PDN | PC7 — HIGH=activo, LOW=power-down |
| DIF1 | PC8 — strapped LOW en PCB (TDM mode) |
| OVF | PA15 — output overflow flag (input al STM32) |

**Secuencia de bring-up:**
1. PC7 (AK_PDN) = LOW durante init (power-down)
2. Arrancar SAI3_B DMA antes de liberar PDN
3. PC7 = HIGH → esperar ≥10ms → AK5538 listo
4. No se requieren writes I2C para operación básica (hardware strapping configura TDM256/slave/32-bit)

---

## 6. PCM1690 DAC — Configuración TDM8

| Parámetro | Valor |
|-----------|-------|
| Modo audio | TDM8 I2S (Reg65 = 0x06, FSPolarity = ACTIVE_LOW) |
| FrameLength | 256 bits |
| DataSize | 24-bit |
| SlotSize | 32-bit |
| SlotNumber | 8 |
| SlotActive | 0xFF (todos) |
| FSDefinition | STARTFRAME |
| FSOffset | BEFOREFIRSTBIT |
| MckOverSampling | ENABLE (en SAI1_A y SAI2_A) |

**Control compartido DS10 (simplificado vs DS9):**
- `DAC_RST` (PC11): bus ~RST a los 4 chips. LOW durante init → HIGH → esperar 12ms.
- `DAC_MUTE_IN` (PC12): AMUTEI compartido. LOW=silenciado, HIGH=activo.
- `DAC_MUTE_OUT` (PC10): AMUTEO wire-OR desde los 4 chips (input al STM32). LOW indica problema.
- DS10 elimina los 12 pines GPIO por-chip del DS9.

**I2C addresses (8-bit, DS10 netlist verificado):**

| DAC | Ref | I2C Addr | SAI | Canales |
|-----|-----|----------|-----|---------|
| DAC0 | U2 | 0x98 | SAI1_A | ch 0-7 |
| DAC1 | U3 | 0x9C | SAI1_B | ch 8-15 |
| DAC2 | U1 | 0x9A | SAI2_A | ch 16-23 |
| DAC3 | U4 | 0x9E | SAI2_B | ch 24-31 |

---

## 7. PAM8302A — Amplificadores

32× PAM8302A Class-D (2.5W/canal). Control compartido:
- `PAM_SD` (PD3): shutdown activo LOW. LOW=off (seguro durante reset), HIGH=activo.
- Pull-down R19 + resistencia serie R20 en PCB.

Secuencia: DACs inicializados y silenciados → PAM_SD = HIGH.

---

## 8. Beamforming Pipeline

```
┌──────────┐   ┌──────────┐   ┌──────────────────────────────┐   ┌─────────┐   ┌──────────┐
│8 Micros  │──►│AK5538VN  │──►│        STM32H743             │──►│4×PCM1690│──►│32 PAM8302│
│          │   │ADC 8ch   │   │                              │   │DAC 32ch │   │2.5W/ch   │
└──────────┘   └──────────┘   │ ADC RX (SAI3_B DMA)          │   └─────────┘   └──────────┘
                              │   ↓ Process_ADC_Half()        │
                              │ delay_line[8][1024]           │
                              │ (ch0=main beam, ch1-7 stubs)  │
                              │   ↓ Fill_Audio_Beamform()     │
                              │ Per canal:                    │
                              │   delay → gain → mute         │
                              │   src_ch = global_ch mod 8    │
                              │   ↓                           │
                              │ 4× SAI TX DMA (32 canales)    │
                              └──────────────────────────────┘
```

### Buffer Configuration

| Buffer | Tamaño | Memoria | Notas |
|--------|--------|---------|-------|
| `audio_buffer[]` | SAI1_A (1b) | RAM_D2 section .dma_buffer | DMA ping-pong, 240 frames half |
| `audio_buffer_1b[]` | SAI1_B | RAM_D2 | Slave sync de audio_buffer |
| `audio_buffer_2a[]` | SAI2_A | RAM_D2 | |
| `audio_buffer_2b[]` | SAI2_B | RAM_D2 | Slave sync de audio_buffer_2a |
| `adc_buffer[]` | SAI3_B (RX) | RAM_D2 | 8ch interleaved |
| `delay_line[8][1024]` | 8 canales | RAM_D2 | Ring buffers, aligned(32) |

**DMA accede solo a RAM_D2** — DMA1/DMA2 NO pueden acceder DTCMRAM.

### Latencia del Sistema

| Etapa | Latencia | Notas |
|-------|----------|-------|
| AK5538VN decimation | ~350 µs | ~17/fS @ 48kHz |
| STM32 DMA input buffer | 10 ms | 240 frames half-buffer |
| STM32 beamforming | ~321 µs avg | fill time medido en DS9 |
| STM32 DMA output buffer | 10 ms | 240 frames half-buffer |
| PCM1690 interpolation | ~21 µs | ~1/fS |
| **Total** | ~20.7 ms | Dominado por buffers DMA |

### Delay Line

- Ring buffer por canal: 1024 samples (power-of-2, `& 0x3FF`)
- `MAX_DELAY_SAMPLES`: 64 samples = ~1.33 ms = ~45.7 cm a 343 m/s
- `delay_line[0]` = canal ADC 0 (main beam source)
- `delay_line[1..7]` = stubs para expansión futura
- `src_ch = (global_ch < 8) ? global_ch : 0` en Fill_Audio_Beamform

---

## 9. Secuencia de Inicialización

```c
// 1. SystemClock_Config() — PLL1 480MHz CPU, HCLK 240MHz
// 2. PeriphCommonClock_Config() — PLL3 24.576MHz MCLK
// 3. MX_GPIO_Init() — GPIO init: DAC_RST LOW, DAC_MUTE_IN LOW, PAM_SD LOW, AK_PDN LOW
// 4. MX_DMA_Init() — DMA1 channels + DMAMUX
// 5. MX_I2C1_Init() + MX_I2C3_Init() + MX_I2C4_Init()
// 6. MX_UART4_Init() — ESP32 link (115200 8N1)
// 7. MX_SAI1_Init() + MX_SAI2_Init() + MX_SAI3_Init()
// 8. MX_TIM3_Init() — LED dimmer 4ch PWM

// 9. EEPROM: load beam_config via I2C4

// 10. Beamforming init
memset(delay_line, 0, sizeof(delay_line));
beam_config = { .master_gain=255, .gains={0xFF…}, .delays={0…}, .mutes=0 };

// 11. Pre-fill TX buffers with silence

// 12. Start DMA — slave ANTES que master
HAL_SAI_Transmit_DMA(&hsai_BlockB1, …);  // SAI1_B slave
HAL_SAI_Transmit_DMA(&hsai_BlockA1, …);  // SAI1_A master
HAL_SAI_Transmit_DMA(&hsai_BlockB2, …);  // SAI2_B slave
HAL_SAI_Transmit_DMA(&hsai_BlockA2, …);  // SAI2_A master
HAL_SAI_Receive_DMA(&hsai_BlockB3, …);   // SAI3_B RX

// 13. AK5538VN: AK_PDN HIGH (liberar power-down), esperar 10ms
// 14. Shared DAC RST: PC11 HIGH, esperar 12ms
// 15. PCM1690_Init() × 4 via I2C1 (NULL pins = sin control por-chip)
// 16. DAC_MUTE_IN (PC12) HIGH — unmute
// 17. PAM_SD (PD3) HIGH — amps on
// 18. UART4 protocol init (ESP32 comms)
// 19. LED dimmer start, banner print, DWT counter reset
```

---

## 10. ESP32 / WiFi Control

| Señal | Pin STM32 | Pin ESP32 | Dirección |
|-------|-----------|-----------|-----------|
| UART4_TX | PA0 AF8 | GPIO0 (RX) | STM32 → ESP32 |
| UART4_RX | PA1 AF8 | GPIO1 (TX) | ESP32 → STM32 |

- Protocolo binario: `uart_protocol.c/h`, 115200 8N1
- ESP32 envía tramas de configuración de beamforming (delays[32], gains[32], mutes, master_gain)
- STM32 responde con status ASCII
- Heartbeat cada 1s via DWT->CYCCNT (SysTick-independiente a 480MHz)

---

## 11. Test Tone Mode

Variable global `test_tone_enabled` (toggle via ESP32 o debug). Cuando activo, `Fill_Audio_Beamform()` genera un tono 1kHz -6dBFS en todos los 8 canales usando la tabla `sine_table_1khz[48]`, bypaseando el beamforming.

---

## 12. Referencias

- [ds10_pin_map.md](ds10_pin_map.md) — Mapa completo de pines DS10
- [pcm1690_ref.md](pcm1690_ref.md) — Referencia DAC PCM1690
- [ak5538vn_ref.md](ak5538vn_ref.md) — Referencia ADC AK5538VN
- [ds9_pin_audit.md](ds9_pin_audit.md) — Errores PCB DS9 (histórico)
- [pin_definitions.h](../firmware/Core/Inc/pin_definitions.h) — Fuente de verdad de pines

---

*DS Beamforming Audio Architecture v4.0 — 8ch in / 32ch out*
*DS10 board — STM32H743VIT6 + AK5538VN + 4×PCM1690 + 32×PAM8302A*
