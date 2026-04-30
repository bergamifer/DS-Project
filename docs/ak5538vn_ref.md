# AK5538VN — Referencia ADC 8 Canales

**Dispositivo:** AKM AK5538VN
**Proyecto:** Domosonica DS10 (reemplaza PCM1802 del DS9)
**Ref en PCB:** IC3

---

## Especificaciones Clave

| Parámetro | Valor |
|-----------|-------|
| Canales | 8 (4 pares estéreo) |
| Resolución | 32 bits |
| Sampling rate | 8–192 kHz |
| SNR | 123 dB (A-weighted, 48kHz) |
| THD+N | −111 dB |
| Rango dinámico | 123 dB |
| Interfaz digital | TDM / I2S / Left-Justified |
| Interfaz control | I2C (slave) |
| Alimentación VDD | 3.3V |
| LDO interno | Sí (AVDD = 1.8V generado internamente con VDDA = 3.3V) |

---

## Configuración en DS10

### Hardware Strapping (PCB)

| Pin | Estado PCB | Efecto |
|-----|-----------|--------|
| MSN (pin 32) | GND | Modo **esclavo** — STM32 SAI3_B genera BICK y LRCK |
| Otros strapping | N/C o GND | Defaults del datasheet |

El AK5538VN en DS10 es **esclavo de audio**: el STM32 SAI3_B actúa como Master RX generando BICK (PD8) y LRCK (PD10), y el AK5538 responde con datos en SDTO1 (PD9).

### Conexiones STM32

| Señal AK5538 | Pin AK5538 | Pin STM32 | Descripción |
|-------------|-----------|-----------|-------------|
| BICK | 33 | PD8 AF6 (SAI3_SCK_B) | Bit clock — STM32 genera |
| SDTO1 | 36 | PD9 AF6 (SAI3_SD_B) | Audio data — AK5538 envía al STM32 |
| LRCK | 34 | PD10 AF6 (SAI3_FS_B) | Frame sync — STM32 genera |
| SCKI / MCLK | — | PE2 (SAI1_MCLK) | 24.576 MHz via bus MCLK |
| PDN | 44 | PC7 (AK_PDN) | Power-down: HIGH=activo, LOW=PDN |
| DIF1 | — | PC8 (AK_DIF1) | Strapped LOW en PCB (TDM mode) |
| OVF | — | PA15 (AK_OVF) | Overflow flag → input al STM32 |
| SCL | 45 | PA8 AF4 (I2C3_SCL) | I2C3 control |
| SDA | 43 | PC9 AF4 (I2C3_SDA) | I2C3 control |
| VDDA | — | 3.3VA | Analógico (separado del VDD digital) |
| VDD | — | 3.3V | Digital |

### Dirección I2C

| Parámetro | Valor |
|-----------|-------|
| 7-bit addr | 0x14 |
| 8-bit write | 0x28 |
| Pin I2CEN | +3.3V (I2C habilitado) |

---

## SAI3_B Configuration (STM32 — Master RX TDM256)

```c
hsai_BlockB3.Instance = SAI3_Block_B;
hsai_BlockB3.Init.AudioMode = SAI_MODEMASTER_RX;
hsai_BlockB3.Init.Synchro = SAI_ASYNCHRONOUS;
hsai_BlockB3.Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
hsai_BlockB3.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
hsai_BlockB3.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_48K;
hsai_BlockB3.Init.DataSize = SAI_DATASIZE_32;
hsai_BlockB3.Init.FirstBit = SAI_FIRSTBIT_MSB;
hsai_BlockB3.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
hsai_BlockB3.Init.Protocol = SAI_FREE_PROTOCOL;
hsai_BlockB3.Init.MonoStereoMode = SAI_STEREOMODE;
hsai_BlockB3.Init.CompandingMode = SAI_NOCOMPANDING;
hsai_BlockB3.FrameInit.FrameLength = 256;
hsai_BlockB3.FrameInit.ActiveFrameLength = 1;
hsai_BlockB3.FrameInit.FSDefinition = SAI_FS_STARTFRAME;
hsai_BlockB3.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
hsai_BlockB3.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;
hsai_BlockB3.SlotInit.FirstBitOffset = 0;
hsai_BlockB3.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
hsai_BlockB3.SlotInit.SlotNumber = 8;
hsai_BlockB3.SlotInit.SlotActive = 0x000000FF;  // slots 0-7
```

**Resultado:** 8 canales 32-bit a 48 kHz, FrameLength=256 bits (= 8 slots × 32 bits).

---

## Secuencia de Inicialización

No se requieren writes I2C para la operación básica. El hardware strapping configura todo lo crítico. La secuencia mínima:

1. `AK_PDN (PC7) = LOW` — mantener power-down durante boot
2. Configurar e iniciar SAI3_B DMA (Master RX)
3. Asegurar MCLK (PE2, 24.576 MHz) corriendo
4. `AK_PDN (PC7) = HIGH` — liberar power-down
5. Esperar ≥10 ms para POR interno del AK5538VN
6. Verificar `AK_OVF (PA15)` — debe ser LOW (sin overflow)

Para configuración avanzada (filtros, ganancia, modo HPF), usar I2C3:

```c
// Ejemplo write I2C3
uint8_t data[2] = { reg_addr, value };
HAL_I2C_Master_Transmit(&hi2c3, AK5538_ADDR, data, 2, HAL_MAX_DELAY);
```

---

## Registros Clave (para configuración avanzada)

| Registro | Addr | Descripción | Default |
|----------|------|-------------|---------|
| Control 1 | 0x00 | HPF, reset, mode | 0x0C |
| Control 2 | 0x01 | DIF1/DIF0, TDM mode | 0x22 |
| Control 3 | 0x02 | Loop, mute | 0x00 |
| Channel Mute | 0x03 | Per-channel mute (bit per ch) | 0x00 |
| Gain CH1/2 | 0x04–0x05 | Digital gain ±18dB | 0x00 |
| Gain CH3/4 | 0x06–0x07 | | 0x00 |
| Gain CH5/6 | 0x08–0x09 | | 0x00 |
| Gain CH7/8 | 0x0A–0x0B | | 0x00 |

**Reg 0x01 (Control 2) — DIF1:DIF0 modo interfaz:**
| DIF1 | DIF0 | Modo |
|------|------|------|
| 0 | 0 | I2S Left-Justified |
| 0 | 1 | I2S Right-Justified |
| 1 | 0 | I2S standard (Philips) |
| 1 | 1 | **TDM256** ← DS10 config |

Con `DIF1=1, DIF0=1` (TDM256), los 8 canales se transmiten en TDM en la misma señal SDTO1, con FrameLength=256 bits (8 slots × 32 bits).

---

## Datos de Buffer (STM32)

El DMA de SAI3_B llena `adc_buffer` con muestras TDM interleaved:

```
adc_buffer[0]  = ch0 frame N
adc_buffer[1]  = ch1 frame N
adc_buffer[2]  = ch2 frame N
...
adc_buffer[7]  = ch7 frame N
adc_buffer[8]  = ch0 frame N+1
...
```

Procesamiento en `Process_ADC_Half()`:
```c
for (uint32_t i = 0; i < samples; i++) {
    for (uint32_t ch = 0; ch < ADC_CHANNELS; ch++) {  // ADC_CHANNELS = 8
        delay_line[ch][(wp + i) & DELAY_LINE_MASK] = rx_buf[i * ADC_CHANNELS + ch];
    }
}
```

---

## Comparación PCM1802 (DS9) vs AK5538VN (DS10)

| Parámetro | PCM1802 (DS9) | AK5538VN (DS10) |
|-----------|---------------|-----------------|
| Canales | 2 (estéreo) | **8** |
| Bits | 24 | **32** |
| SNR | 105 dB | **123 dB** |
| Control | 8 GPIO pines | **I2C3 + 3 GPIO** |
| SAI mode | I2S 2ch | **TDM256 8ch** |
| SAI config | Master RX 2ch | **Master RX 8ch FrameLen=256** |
| Delay lines fw | `delay_line_L/R[1024]` | **`delay_line[8][1024]`** |

---

*AK5538VN Reference — DS10 — Abril 2026*
*Ver [audio_architecture.md](audio_architecture.md) para integración completa*
