# DS Beamforming - Arquitectura de Audio v3.0

**Estado:** Firmware con beamforming pipeline funcionando en WeAct (pinout DS10)
**Fecha:** Febrero 2026

---

## 1. Resumen del Sistema

```
                        ┌──────────────────────────────────────────────────────────────┐
                        │                      STM32H743VIT6                           │
                        │                                                              │
   Microfono ──────────►│ SAI3_B RX: PD9(SD) PD8(SCK) PD10(FS) AF6                   │
                        │           PCM1802 U1 (ADC stereo)                           │
                        │                                                              │
                        │ PE2 (SAI1_MCLK) ── R21 (33Ohm) ── Clock Bus 12.288 MHz     │
                        │                                    ↓    ↓    ↓    ↓    ↓    │
                        │ SAI1_A: PE4(FS)/PE5(SCK)/PE6(SD)  → PCM1690 U3 (DAC0) ch 0-7 │──► 8 Speakers
                        │ SAI1_B: PE3(SD sync)              → PCM1690 U4 (DAC1) ch 8-15│──► 8 Speakers
                        │ SAI2_A: PD12(FS)/PD13(SCK)/PD11(SD) → PCM1690 U2 (DAC2) ch16-23│─► 8 Speakers
                        │ SAI2_B: PE11(SD sync)             → PCM1690 U8 (DAC3) ch24-31│──► 8 Speakers
                        │                                                              │
                        │ I2C1 (PB8/PB9) ──── DAC config (4x PCM1690)                │
                        │ UART4 (PA0/PA1) ──── ESP32 (WiFi control)                   │
                        └──────────────────────────────────────────────────────────────┘
```

---

## 2. Componentes de Audio

| Componente | Ref | Funcion | SAI Block | I2C Addr (8-bit) | Canales |
|------------|-----|---------|-----------|-------------------|---------|
| PCM1802 | U1 | ADC Stereo | SAI3_B (RX) | - (GPIO ctrl) | 2 entrada |
| PCM1690 | U3 | DAC 8-ch | SAI1_A (Master TX) | 0x94 | 0-7 |
| PCM1690 | U4 | DAC 8-ch | SAI1_B (Slave TX sync) | 0x96 | 8-15 |
| PCM1690 | U2 | DAC 8-ch | SAI2_A (Master TX) | 0x98 | 16-23 |
| PCM1690 | U8 | DAC 8-ch | SAI2_B (Slave TX sync) | 0x9A | 24-31 |
| **Total** | | | | | **2 in / 32 out** |

---

## 3. SAI Pin Mapping (DS10 Correcto — verificado contra datasheet Tables 13-14)

| SAI Block | Mode | FS | SCK | SD | AF | DMA Stream | DMAMUX |
|-----------|------|-----|------|------|------|------------|--------|
| SAI1_A | Master TX TDM8 | PE4 | PE5 | PE6 | AF6 | DMA1_Stream0 | 87 |
| SAI1_B | Slave TX sync | — | — | PE3 | AF6 | DMA1_Stream3 | 88 |
| SAI2_A | Master TX TDM8 | PD12 | PD13 | PD11 | AF10 | DMA1_Stream1 | 89 |
| SAI2_B | Slave TX sync | — | — | PE11 | AF10 | DMA1_Stream2 | 90 |
| SAI3_B | Master RX I2S | PD10 | PD8 | PD9 | AF6 | DMA1_Stream4 | 114 |

**MCLK:** PE2 (SAI1_MCLK_A, AF6) → R21 33Ohm → bus compartido por todos los chips

**SAI Sync Mode:** SAI1_B y SAI2_B comparten SCK/FS internamente con su bloque A. Solo necesitan pin SD. Start slave ANTES que master.

### DS9 Pin Errors (ver ds9_pin_audit.md)

El PCB DS9 v2.0 tiene 6 errores de mapeo. Los mas criticos:
- PE7/PE8/PE9 no tienen funcion SAI en ninguna AF
- PD8/PD9/PD10 son SAI3_B (AF6), no SAI2_B
- SAI4_B SCK/FS no estan disponibles en LQFP100

---

## 4. Clocks

```
PE2 (SAI1_MCLK) = 12.288 MHz (PLL3: M=5, N=68, P=28, FRACN=6660)
                         │
                    R21 (33Ohm)
                         │
               12.288 MHz Clock Bus
          ┌──────┬───────┼───────┬──────┐
          ▼      ▼       ▼       ▼      ▼
     PCM1802  PCM1690  PCM1690  PCM1690  PCM1690
      U1       U3       U4       U2       U8
      ADC     DAC0     DAC1     DAC2     DAC3
```

| Senal | Frecuencia | Relacion |
|-------|------------|----------|
| MCLK | 12.288 MHz | 256 x fS |
| BCK (TDM8) | 12.288 MHz | 256 x fS (= MCLK) |
| FS | 48 kHz | fS |

---

## 5. Beamforming Pipeline

```
┌──────────┐   ┌──────────┐   ┌─────────────────────────────┐   ┌──────────┐   ┌───────────┐
│Microfono │──►│ PCM1802  │──►│         STM32H743           │──►│4xPCM1690 │──►│32 Speakers│
│          │   │   ADC    │   │                             │   │   DACs   │   │           │
└──────────┘   └──────────┘   │  ADC RX (SAI3_B)           │   └──────────┘   └───────────┘
                              │    ↓ Process_ADC_Half()     │
                              │  delay_line_L[1024]         │
                              │  delay_line_R[1024]         │
                              │    ↓ Fill_Audio_Beamform()  │
                              │  Per channel:               │
                              │    delay → gain → mute      │
                              │    ↓                        │
                              │  4x SAI TX (32 channels)    │
                              └─────────────────────────────┘
```

### Latencia del Sistema

| Etapa | Latencia | Notas |
|-------|----------|-------|
| PCM1802 decimation filter | ~362 us | 17.4 / fS @ 48kHz |
| STM32 DMA input buffer | 10 ms | 480 samples half-buffer |
| STM32 beamforming | < 200 us | Fill + cache clean |
| STM32 DMA output buffer | 10 ms | 480 samples half-buffer |
| PCM1690 interpolation | ~21 us | ~1 / fS |
| **Total** | ~20.6 ms | Dominado por buffers DMA |

### Delay Line

- Ring buffer 1024 samples (power-of-2 para `& 0x3FF` modulo)
- MAX_DELAY: 64 samples = ~1.33 ms = ~45.7 cm a 343 m/s
- Dual beam ready: L y R delay lines, solo L usado actualmente

---

## 6. Secuencia de Inicializacion

```c
// 1. Clocks: PLL1 (480MHz CPU), PLL3 (12.288MHz MCLK)
// 2. GPIO, DMA, I2C1, UART7, SPI2, UART4
// 3. SAI1_Init (A master + B slave), SAI2_Init (A + B), SAI3_Init (B RX)

// 4. Beamforming init
memset(delay_line_L, 0, sizeof(delay_line_L));
memset(delay_line_R, 0, sizeof(delay_line_R));
beam_config = { .master_gain=255, .gains={0xFF...}, .delays={0...}, .mutes=0 };

// 5. Pre-fill TX buffers with silence, cache clean

// 6. Start DMA (slave before master!)
HAL_SAI_Transmit_DMA(&hsai_BlockB1, ...);  // SAI1_B slave
HAL_SAI_Transmit_DMA(&hsai_BlockA1, ...);  // SAI1_A master
HAL_SAI_Transmit_DMA(&hsai_BlockB2, ...);  // SAI2_B slave
HAL_SAI_Transmit_DMA(&hsai_BlockA2, ...);  // SAI2_A master
HAL_SAI_Receive_DMA(&hsai_BlockB3, ...);   // SAI3_B RX

// 7. E-Paper init (non-fatal, audio already running)
// 8. UART4 protocol init (ESP32 comms)
// 9. Reset counters + DWT cycle counter
```

---

## 7. PCM1690 DAC Config (para DS9/DS10 board)

Configuracion via I2C1:
1. Hardware reset (RST pin LOW → HIGH, wait 20ms)
2. Reg 65 = 0x06 (TDM I2S 24-bit mode)
3. Reg 66 = 0x00 (all DACs enabled, sharp rolloff)
4. Reg 72-79 = 0xFF (0dB attenuation per channel)

I2C addresses: U3=0x94, U4=0x96, U2=0x98, U8=0x9A

---

## 8. Calibracion Automatica (Futuro)

Concepto: usar el microfono para medir delay real de cada canal via cross-correlacion.
- Emitir chirp secuencial por cada canal → capturar con ADC
- Calcular lag maximo → compensar en delay_compensated[N]
- Guardar en EEPROM (CAT24C256 via I2C4 PB6/PB7)

Ver seccion completa en versiones anteriores de este documento.

---

## 9. Referencias

- [ds9_pin_audit.md](ds9_pin_audit.md) - DS9 pin errors y bodge fixes
- [pin_definitions_ds10.h](../firmware/Core/Inc/pin_definitions_ds10.h) - Corrected pins
- [pcm1690_ref.md](pcm1690_ref.md) - DAC reference
- [pcm1802_registers.md](pcm1802_registers.md) - ADC reference
- [uart-stm32-protocol.md](../uart-stm32-protocol.md) - ESP32 protocol spec

---

*DS Beamforming Audio Architecture v3.0 — 32ch output / 2ch input*
*Pin mapping corrected for DS10 (verified from STM32H743 datasheet)*
