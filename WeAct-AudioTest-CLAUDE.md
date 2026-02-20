# WeAct STM32H743 - Audio I2S Test Project

**Objetivo:** Validar configuración SAI1 en modo I2S Master antes de implementar en DS v9 (16 canales TDM8)

---

## 1. Hardware

### WeAct Core Board STM32H743VIT6
- MCU: STM32H743VIT6 (ARM Cortex-M7, 480MHz)
- Flash: 2MB interno + 8MB SPI + 8MB QSPI
- RAM: 1MB
- HSE: **25 MHz** (cristal externo)
- User LED: **PE3** (activo LOW)
- User Button: **PC13** (activo LOW)

### DAC Externo para Pruebas
Usar cualquier DAC I2S compatible (PCM5102, UDA1334, MAX98357A, etc.)

Conexiones:
| Señal | Pin STM32 | Pin DAC |
|-------|-----------|---------|
| MCLK | PE2 | MCLK (si aplica) |
| LRCK/FS | PE4 | LRCK/WS |
| BCK/SCK | PE5 | BCK/BCLK |
| DIN/SD | PE6 | DIN/SDIN |
| GND | GND | GND |

---

## 2. Configuración de Clocks

### Target: MCLK = 12.288 MHz (256 x 48kHz)

```
HSE = 25 MHz

PLL3 Configuration:
  DIVM3 = 5       -> 5 MHz (VCO input)
  DIVN3 = 68      -> Base multiplier
  FRACN3 = 6329   -> Fine tuning para 12.288 MHz exactos
  DIVP3 = 28      -> Output divider

Calculo:
  VCO3 = (25MHz / 5) x (68 + 6329/8192) = 343.8624 MHz
  DIVP3_out = 343.8624 / 28 = 12.2808 MHz (~0.06% error)

SAI1 Clock Source: PLL3_P
```

### En CubeMX - Clock Configuration:
1. HSE = 25 MHz (External Oscillator)
2. PLL Source = HSE
3. Habilitar PLL3 con FRACN
4. SAI1 Clock Mux -> PLL3_P

---

## 3. Configuracion SAI1

### Modo: Master Transmitter con MCLK Output

En CubeMX -> Multimedia -> SAI1:

| Parametro | Valor |
|-----------|-------|
| Mode | Master with Master Clock Out |
| Audio Mode | Master Transmit |
| Protocol | Free Protocol |
| Data Size | 24 bits |
| Frame Length | 64 bits |
| Frame Synchro Active Level Length | 32 bits |
| Frame Synchro Polarity | Active Low (FS Low = Left channel) |
| Frame Synchro Definition | Channel Identification |
| Slot Size | 32 bits |
| Slot Number | 2 |
| Slot Active | 0x03 (slots 0 y 1) |
| First Bit | MSB First |
| Clock Strobing | Falling Edge |
| MCLK Output | Enable |
| MCLK/FS Ratio | 256 |

### Pines SAI1_A:
| Funcion | Pin | AF |
|---------|-----|-----|
| SAI1_MCLK_A | PE2 | AF6 |
| SAI1_FS_A | PE4 | AF6 |
| SAI1_SCK_A | PE5 | AF6 |
| SAI1_SD_A | PE6 | AF6 |

---

## 4. DMA Configuration

Para audio sin glitches, usar DMA circular:

| Stream | Periferico | Direccion | Mode |
|--------|------------|-----------|------|
| DMA1 Stream 0 | SAI1_A | Memory -> Peripheral | Circular |

Configuracion DMA:
- Data Width: Word (32 bits)
- Memory Increment: Enable
- Peripheral Increment: Disable
- Priority: High
- FIFO: Enable, Threshold Full

---

## 5. Estructura de Buffer I2S

```c
#define AUDIO_SAMPLE_RATE    48000
#define AUDIO_BUFFER_SAMPLES 480      // 10ms @ 48kHz
#define AUDIO_CHANNELS       2        // Stereo

// Double buffer para DMA circular
// Formato: [L0][R0][L1][R1]...[L479][R479] x 2 buffers
int32_t audio_buffer[2][AUDIO_BUFFER_SAMPLES * AUDIO_CHANNELS];

// Callbacks DMA
void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
    // Llenar primera mitad del buffer
    Fill_Audio_Buffer(audio_buffer[0], AUDIO_BUFFER_SAMPLES);
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
    // Llenar segunda mitad del buffer
    Fill_Audio_Buffer(audio_buffer[1], AUDIO_BUFFER_SAMPLES);
}
```

---

## 6. Test Signals

### 6.1 Sine Wave (verificar audio basico)
```c
void Generate_Sine(int32_t *buffer, uint32_t samples, float freq_hz) {
    for (uint32_t i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_SAMPLE_RATE;
        int32_t sample = (int32_t)(sinf(2.0f * M_PI * freq_hz * t) * 0x7FFFFF);
        buffer[i * 2]     = sample;  // Left
        buffer[i * 2 + 1] = sample;  // Right
    }
}
```

### 6.2 Square Wave (verificar timing/clocks)
```c
void Generate_Square(int32_t *buffer, uint32_t samples, float freq_hz) {
    uint32_t period = AUDIO_SAMPLE_RATE / freq_hz;
    for (uint32_t i = 0; i < samples; i++) {
        int32_t sample = ((i % period) < (period / 2)) ? 0x7FFFFF : -0x7FFFFF;
        buffer[i * 2]     = sample;
        buffer[i * 2 + 1] = sample;
    }
}
```

---

## 7. Checklist de Verificacion

### Hardware
- [ ] WeAct conectada y programable via ST-Link/USB
- [ ] DAC I2S conectado (MCLK, BCK, LRCK, DIN, GND)
- [ ] Amplificador/auriculares conectados a salida del DAC

### Software
- [ ] PLL3 configurado con FRACN para 12.288 MHz
- [ ] SAI1 clock source = PLL3_P
- [ ] SAI1_A en modo Master TX con MCLK
- [ ] DMA1 configurado para SAI1_A TX
- [ ] Interrupts habilitados (SAI1, DMA1)

### Mediciones con Osciloscopio
- [ ] MCLK (PE2): 12.288 MHz
- [ ] BCK (PE5): 3.072 MHz (MCLK/4)
- [ ] LRCK (PE4): 48 kHz
- [ ] Frame = 64 bits (32 Left + 32 Right)

### Audio
- [ ] Sine 1kHz audible y limpio
- [ ] Sin clicks/pops (DMA funcionando)
- [ ] Canales L/R correctos

---

## 8. Troubleshooting

### No hay salida de audio
1. Verificar MCLK con osciloscopio
2. Verificar que SAI1 este habilitado (`__HAL_SAI_ENABLE`)
3. Verificar DMA iniciado (`HAL_SAI_Transmit_DMA`)

### Audio distorsionado
1. Verificar data size (24 bits en slot de 32)
2. Verificar alineacion de datos (MSB justified)
3. Verificar clock polarity (falling edge para I2S Phillips)

### Clicks/Pops periodicos
1. Buffer underrun - aumentar tamano de buffer
2. DMA no en modo circular
3. Callbacks no llenan buffer a tiempo

### Frecuencia incorrecta
1. Verificar FRACN de PLL3
2. Verificar Frame Length = 64
3. Verificar MCLK ratio = 256

---

## 9. Progresion de Pruebas

1. **Basico:** Generar MCLK y verificar con osciloscopio
2. **Clocks:** Verificar BCK y LRCK con relaciones correctas
3. **Data:** Transmitir patron conocido, verificar con analizador logico
4. **Audio:** Generar sine wave, escuchar en auriculares
5. **Stress:** Audio continuo por varios minutos sin glitches

---

## 10. Calibracion por Microfono (Array Aleatorio)

### 10.1 Concepto

Para beamforming con array de speakers distribuidos aleatoriamente (sin grating lobes),
se usa el microfono del sistema para medir el delay real de cada canal.
Esto elimina la necesidad de conocer la posicion exacta de cada speaker.

```
Procedimiento por canal:
  1. Mute todos los canales
  2. Emitir chirp (1kHz->10kHz, 10ms) por canal N
  3. Capturar con microfono en el punto focal
  4. Cross-correlacion -> delay_real[N]
  5. Repetir para los 16 canales
  6. Compensar: delay_comp[N] = max(delay_real) - delay_real[N]
  7. Guardar en EEPROM
```

### 10.2 Calibracion de Ganancia

**Estado:** Por investigar

Medir amplitud por canal y nivelar automaticamente:
- Emitir senal de referencia (sine de amplitud conocida) por cada canal
- Medir amplitud RMS capturada por el microfono
- Calcular factor de correccion: gain_cal[N] = amplitud_referencia / amplitud_medida[N]
- Aplicar en runtime para que todos los canales lleguen con igual energia al punto focal

Variaciones a considerar:
- Diferencias entre amplificadores
- Diferencias entre speakers (sensibilidad, impedancia)
- Distancia al punto focal (atenuacion por distancia)
- Respuesta en frecuencia por canal (calibrar con sweep en vez de tono unico)
- Implementar directamente sobre los registros ATDA (atenuación por canal) del PCM1690. No hacer las atenuaciones por MCU sino que enviar los valores de atenuación deseados al PCM1690 via I2C

### 10.3 Calibracion Multi-Punto

**Estado:** Por investigar

Calibrar desde varias posiciones de microfono para cambiar el foco sin recalibrar:

```
Concepto:
  1. Definir grilla de puntos focales (ej: 3x3, 5x5)
  2. Colocar microfono en cada punto y calibrar delay + ganancia
  3. Almacenar tabla de delays[punto][canal] en EEPROM
  4. En runtime, seleccionar punto focal -> cargar delays correspondientes
  5. Interpolar entre puntos para posiciones intermedias

Almacenamiento estimado:
  - 9 puntos (3x3) x 16 canales x (4 bytes delay + 4 bytes gain) = 1.15 KB
  - 25 puntos (5x5) x 16 canales x 8 bytes = 3.2 KB
  - EEPROM CAT24C256 = 32 KB -> sobra espacio

Ventaja: cambiar direccion del beam en tiempo real sin recalibrar
Desafio: interpolacion de delays entre puntos discretos
```

### 10.4 Recalibracion Rapida (Canal Individual)

**Estado:** Por investigar

Si se mueve un speaker, recalibrar solo ese canal en vez de los 16:

```
Procedimiento:
  1. Usuario indica canal a recalibrar (o deteccion automatica)
  2. Emitir chirp solo por ese canal
  3. Medir nuevo delay y ganancia
  4. Actualizar solo delay_comp[N] y gain_cal[N]
  5. Guardar en EEPROM

Deteccion automatica de speaker movido:
  - Periodicamente emitir pulso de test (inaudible, baja amplitud)
  - Comparar delay medido vs delay calibrado
  - Si |delta| > threshold -> marcar canal para recalibracion
  - Notificar al usuario via webserver/ESP32
```

### 10.5 Roadmap de Calibracion

| Fase | Funcionalidad | Dependencias |
|------|---------------|--------------|
| 1 | Audio I2S basico funcionando | SAI1, DMA, PLL3 |
| 2 | Calibracion de delay (single point) | ADC funcionando, cross-correlacion |
| 3 | Calibracion de ganancia | Fase 2 completa |
| 4 | Recalibracion rapida por canal | Fase 2 completa |
| 5 | Calibracion multi-punto | Fase 3 completa, webserver para control |
| 6 | Deteccion automatica de cambios | Fase 4 completa |

---

## 11. Referencias

### Datasheets
- PCM1690: DAC 8 canales TDM (para DS v9)
- PCM1802: ADC stereo (para DS v9)
- PCM5102/UDA1334: DAC I2S simples (para pruebas)

### Application Notes
- AN5543: SAI and I2S configuration on STM32H7
- RM0433: STM32H743 Reference Manual (SAI chapter)

### Documentos del Proyecto DS
- `docs/audio_architecture.md` - Arquitectura completa 16 canales
- `docs/pcm1690_ref.md` - Registros y configuracion DAC
- `docs/pcm1802_registers.md` - Registros y configuracion ADC
- `stm32/sai_config.md` - Configuracion SAI detallada
- `stm32/clock_config.md` - Calculos PLL

---

*Proyecto de prueba para validar audio I2S en WeAct STM32H743*
*Una vez funcionando, migrar configuracion a DS v9 con TDM8*
