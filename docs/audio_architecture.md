# DS Beamforming - Arquitectura de Audio

**Estado:** Verificado contra hardware (DS.net)
**Fecha:** Enero 2026

---

## 1. Resumen del Sistema

```
                    ┌─────────────────────────────────────────────────────────┐
                    │                    STM32H743                            │
                    │                                                         │
   Micrófono ──────►│ SAI4_SD_B (PB11) ◄── PCM1802 (ADC)                     │
                    │ SAI4_SCK_B (PC1) ──► │                                 │
                    │ SAI4_FS_B (PC0) ───► │                                 │
                    │                      │                                 │
                    │ SAI1_MCLK (PE2) ─────┼──────────────────────────┐      │
                    │                      │                          │      │
                    │ SAI1_SD (PE6) ──────►│ PCM1690 U2 (8ch DAC) ◄──┤      │──► 8 Speakers
                    │ SAI1_SCK (PE5) ─────►│                          │      │
                    │ SAI1_FS (PE4) ──────►│                          │      │
                    │                      │                          │      │
                    │ SAI2_SD (PE7) ──────►│ PCM1690 U3 (8ch DAC) ◄──┘      │──► 8 Speakers
                    │ SAI2_SCK (PE8) ─────►│                                 │
                    │ SAI2_FS (PE9) ──────►│                                 │
                    │                                                         │
                    │ I2C1_SCL (PB8) ─────► DACs config                      │
                    │ I2C1_SDA (PB9) ◄────►                                   │
                    └─────────────────────────────────────────────────────────┘
```

---

## 2. Componentes de Audio

| Componente | Ref | Función | Canales |
|------------|-----|---------|---------|
| PCM1802 | U8 | ADC Stereo | 2 entrada |
| PCM1690 | U2 | DAC 8-ch | 8 salida |
| PCM1690 | U3 | DAC 8-ch | 8 salida |
| **Total** | | | **2 in / 16 out** |

---

## 3. Sincronización de Clocks - CRÍTICO

### 3.1 Arquitectura Verificada ✅

```
                         PE2 (SAI1_MCLK_A)
                               │
                          R17 (33Ω)
                               │
                         SAI1_MCLK (12.288 MHz @ 48kHz)
                               │
              ┌────────────────┼────────────────┐
              │                │                │
              ▼                ▼                ▼
         ┌────────┐       ┌────────┐       ┌────────┐
         │PCM1802 │       │PCM1690 │       │PCM1690 │
         │  U8    │       │  U2    │       │  U3    │
         │  ADC   │       │ DAC0   │       │ DAC1   │
         └────────┘       └────────┘       └────────┘
            SCKI             SCKI             SCKI
           pin 15           pin 14           pin 14
```

**PUNTO CLAVE:** Los 3 chips comparten el mismo MCLK. Esto es lo que garantiza:
- Cero drift entre ADC y DACs
- Muestras perfectamente alineadas en tiempo
- Latencia determinística para beamforming

### 3.2 Frecuencias de Clock

| Señal | Frecuencia | Relación | Origen |
|-------|------------|----------|--------|
| MCLK (SCKI) | 12.288 MHz | 256 × fS | PE2 (SAI1_MCLK) |
| BCK | 3.072 MHz | 64 × fS | SAI_SCK |
| LRCK/FS | 48 kHz | fS | SAI_FS |

### 3.3 Configuración de PLL Recomendada

Para obtener 12.288 MHz exactos desde HSE = 25 MHz:

```
PLL2 para SAI:
  DIVM2 = 25    → 1 MHz de referencia
  MULN2 = 344   → 344 MHz VCO
  DIVP2 = 28    → 12.2857 MHz (error: 0.019% - aceptable)

Alternativa con cristal externo:
  - Usar cristal de 12.288 MHz en entrada externa SAI
  - Error: 0%
```

---

## 4. Configuración de Hardware Verificada

### 4.1 PCM1802 (ADC) - U8

| Configuración | Pines | Valor | Resultado |
|---------------|-------|-------|-----------|
| Modo | MODE1, MODE0 | GND, GND | **Slave** |
| Formato | FMT1, FMT0 | GND, 3V3 | **I2S 24-bit** |
| Oversampling | OSR | GPIO | ×64 o ×128 |
| HPF | BYPASS | GPIO | Configurable |
| Power | PDWN | GPIO | Configurable |

### 4.2 PCM1690 (DACs) - U2, U3

| Configuración | Método | Valor |
|---------------|--------|-------|
| Modo | I2C registro | Slave I2S |
| Formato | I2C registro | I2S 24-bit |
| Atenuación | I2C registro | Por canal |
| Reset | GPIO (PA6, PE0) | Active LOW |

### 4.3 Mapeo Completo de Pines

```c
// === ADC PCM1802 (U8) ===
#define ADC_MCLK_PIN        GPIO_PIN_2   // PE2 - SAI1_MCLK
#define ADC_BCK_PIN         GPIO_PIN_1   // PC1 - SAI4_SCK_B
#define ADC_LRCK_PIN        GPIO_PIN_0   // PC0 - SAI4_FS_B
#define ADC_DOUT_PIN        GPIO_PIN_11  // PB11 - SAI4_SD_B
#define ADC_PDWN_PIN        // Verificar en esquemático
#define ADC_BYPASS_PIN      // Verificar en esquemático
#define ADC_OSR_PIN         // Verificar en esquemático

// === DAC0 PCM1690 (U2) ===
#define DAC0_SCK_PIN        GPIO_PIN_5   // PE5 - SAI1_SCK
#define DAC0_FS_PIN         GPIO_PIN_4   // PE4 - SAI1_FS
#define DAC0_SD_PIN         GPIO_PIN_6   // PE6 - SAI1_SD
#define DAC0_RST_PIN        GPIO_PIN_6   // PA6

// === DAC1 PCM1690 (U3) ===
#define DAC1_SCK_PIN        GPIO_PIN_8   // PE8 - SAI2_SCK
#define DAC1_FS_PIN         GPIO_PIN_9   // PE9 - SAI2_FS
#define DAC1_SD_PIN         GPIO_PIN_7   // PE7 - SAI2_SD

// === I2C para DACs ===
#define I2C_SCL_PIN         GPIO_PIN_8   // PB8 - I2C1_SCL
#define I2C_SDA_PIN         GPIO_PIN_9   // PB9 - I2C1_SDA
```

---

## 5. Flujo de Audio para Beamforming

```
┌──────────┐    ┌──────────┐    ┌──────────────┐    ┌──────────┐    ┌──────────┐
│Micrófono │───►│ PCM1802  │───►│   STM32H7    │───►│ PCM1690  │───►│ Speakers │
│          │    │   ADC    │    │  Procesado   │    │   DACs   │    │  (×16)   │
└──────────┘    └──────────┘    │              │    └──────────┘    └──────────┘
                                │ - Delay/ch   │
                                │ - Gain/ch    │
                                │ - Mute/ch    │
                                │ - Beamform   │
                                └──────────────┘
```

### 5.1 Latencia del Sistema

| Etapa | Latencia | Notas |
|-------|----------|-------|
| PCM1802 decimation filter | 17.4 / fS | ~362 µs @ 48kHz |
| STM32 DMA buffer | N samples / fS | Configurable |
| PCM1690 interpolation | ~1 / fS | ~21 µs @ 48kHz |
| **Total mínimo** | ~400 µs | Sin procesamiento |

### 5.2 Requisitos de Timing para Beamforming

Para audio direccional, el delay máximo entre canales depende de:
- Distancia entre speakers
- Velocidad del sonido (~343 m/s)

Ejemplo: speakers separados 5cm
- Delay máximo = 0.05m / 343m/s = **146 µs** = **7 samples @ 48kHz**

---

## 6. Secuencia de Inicialización

```c
void Audio_Init(void)
{
    // 1. Configurar clocks
    //    - Habilitar PLL2 para SAI (12.288 MHz)
    //    - Configurar SAI1, SAI2, SAI4 clock source

    // 2. Inicializar GPIOs de control
    //    - ADC: PDWN, BYPASS, OSR
    //    - DACs: RST

    // 3. Mantener DACs en reset, ADC en power-down
    HAL_GPIO_WritePin(DAC0_RST_PORT, DAC0_RST_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ADC_PDWN_PORT, ADC_PDWN_PIN, GPIO_PIN_RESET);

    // 4. Iniciar MCLK (SAI1)
    HAL_SAI_Init(&hsai_BlockA1);  // Esto activa MCLK en PE2

    // 5. Esperar estabilización de clock (1ms)
    HAL_Delay(1);

    // 6. Sacar DACs de reset
    HAL_GPIO_WritePin(DAC0_RST_PORT, DAC0_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(1);

    // 7. Configurar DACs via I2C
    PCM1690_Init(&hdac0);
    PCM1690_Init(&hdac1);

    // 8. Activar ADC
    HAL_GPIO_WritePin(ADC_PDWN_PORT, ADC_PDWN_PIN, GPIO_PIN_SET);

    // 9. Esperar inicialización ADC (100ms)
    HAL_Delay(100);

    // 10. Iniciar SAI con DMA
    HAL_SAI_Receive_DMA(&hsai_BlockB4, adc_buffer, BUFFER_SIZE);
    HAL_SAI_Transmit_DMA(&hsai_BlockA1, dac0_buffer, BUFFER_SIZE);
    HAL_SAI_Transmit_DMA(&hsai_BlockA2, dac1_buffer, BUFFER_SIZE);
}
```

---

## 7. Checklist de Verificación

### Hardware ✅
- [x] MCLK compartido entre ADC y DACs
- [x] PCM1802 en modo Slave
- [x] PCM1802 en formato I2S 24-bit
- [x] PCM1690 conectados a I2C para config
- [x] Bypass capacitors en VREF
- [x] AGND y DGND conectados
- [x] Alimentación 5V/3.3V correcta

### Firmware (Pendiente)
- [ ] PLL configurado para 12.288 MHz
- [ ] SAI1 MCLK habilitado en PE2
- [ ] SAI4 configurado para recepción ADC
- [ ] SAI1/SAI2 configurados para transmisión DACs
- [ ] DMA configurado para transferencia circular
- [ ] I2C inicializa PCM1690s
- [ ] Secuencia de power-on implementada
- [ ] Procesamiento de beamforming

---

## 8. Referencias

- [pcm1802_registers.md](pcm1802_registers.md) - Detalles del ADC
- [pcm1690_ref.md](pcm1690_ref.md) - Detalles de los DACs
- DS.net - Netlist del PCB (verificación de hardware)

---

*Documento de arquitectura de audio - DS Beamforming*
*Verificado: Enero 2026*
