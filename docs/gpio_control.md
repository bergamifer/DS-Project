# DS Beamforming - GPIO Control y Funciones del Sistema

**Estado:** Planificado (No implementado)
**Fecha:** Enero 2026

---

## 1. GPIOs Reservados para Control de Sistema

### 1.1 GPIOs Disponibles

Los siguientes pines están reservados para funciones de control futuro:

| GPIO | Disponible | Función Propuesta |
|------|------------|-------------------|
| **PC7** | ✅ | Power Button (entrada con pull-up) |
| **PD15** | ✅ | Error LED Red (salida) |
| **PB13** | ✅ | Audio Activity LED (salida) |
| **PB15** | ✅ | Mute Button (entrada con pull-up) |
| **PD9** | ✅ | Calibration/Test Mode (entrada con pull-up) |
| **PD11** | ✅ | Speaker Amplifier Enable (salida) |

### 1.2 LEDs Existentes en el PCB

| LED | Ref | GPIO | Función | Estado |
|-----|-----|------|---------|--------|
| Green | D1 | PB5 | Status LED | ✅ Implementado |
| Power | D5 | — | Conectado a 3.3V directo | ✅ Siempre ON |

---

## 2. Funciones Propuestas (Futuro)

### 2.1 Power Button (PC7)

**Función:** Control de encendido/apagado del sistema de audio

**Configuración:**
```c
GPIO_InitTypeDef GPIO_InitStruct = {0};
GPIO_InitStruct.Pin = GPIO_PIN_7;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_PULLUP;
HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
```

**Comportamiento:**
- Presión corta (<2s): Toggle ON/OFF del sistema de audio
- Presión larga (>2s): Shutdown completo con fade-out

**Secuencia de Power ON:**
```c
void System_PowerOn(void) {
    // 1. Iniciar clocks (SAI1 MCLK)
    Start_Audio_Clocks();
    HAL_Delay(10);

    // 2. Activar ADC (PDWN = HIGH)
    ADC_PowerOn();
    HAL_Delay(100);  // Esperar inicialización PCM1802

    // 3. Configurar DACs via I2C
    DAC_Init();
    HAL_Delay(10);

    // 4. Habilitar amplificadores (si está implementado)
    if (SPEAKER_ENABLE_IMPLEMENTED) {
        GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_SET);
    }

    // 5. Iniciar DMA para SAI
    Start_Audio_Processing();

    // 6. Encender LED de status
    GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
}
```

**Secuencia de Power OFF:**
```c
void System_PowerOff(void) {
    // 1. Fade out en DACs (evitar clicks)
    Audio_FadeOut(500);  // 500ms fade

    // 2. Detener DMA
    Stop_Audio_Processing();

    // 3. Deshabilitar amplificadores
    if (SPEAKER_ENABLE_IMPLEMENTED) {
        GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_RESET);
    }
    HAL_Delay(100);

    // 4. Apagar ADC
    ADC_PowerOff();

    // 5. Detener clocks
    Stop_Audio_Clocks();

    // 6. Apagar LED de status
    GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
}
```

---

### 2.2 Error LED (PD15) - Rojo

**Función:** Indicar errores de sistema

**Configuración:**
```c
GPIO_InitStruct.Pin = GPIO_PIN_15;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
```

**Condiciones de error:**
| Error | Código | LED Patrón |
|-------|--------|-----------|
| DMA Underrun/Overrun | 1 | Parpadeo rápido (5Hz) |
| SAI Clock Lost | 2 | ON continuo |
| I2C DAC Timeout | 3 | Parpadeo lento (1Hz) |
| ADC No Data | 4 | Doble parpadeo |

```c
void Error_Handler_Audio(uint8_t error_code) {
    system_error = error_code;

    switch(error_code) {
        case ERROR_DMA_UNDERRUN:
            // Parpadeo rápido
            while(1) {
                GPIO_TogglePin(GPIOD, GPIO_PIN_15);
                HAL_Delay(100);
            }
            break;

        case ERROR_CLOCK_LOST:
            // ON continuo
            GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_SET);
            System_PowerOff();
            break;

        // ... otros casos
    }
}
```

---

### 2.3 Audio Activity LED (PB13)

**Función:** Indicar actividad de audio (alternativa: VU meter)

**Opción A: Simple ON cuando hay señal**
```c
void Audio_Activity_Update(void) {
    // Calcular RMS de la señal de entrada
    float rms = Calculate_RMS(adc_buffer, BUFFER_SIZE);

    if (rms > THRESHOLD_SILENCE) {
        GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
    } else {
        GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
    }
}
```

**Opción B: PWM para VU meter**
```c
// Usar TIM para PWM en PB13
void VU_Meter_Update(float level_db) {
    // level_db: -60 dB a 0 dB
    uint16_t pwm_duty = (uint16_t)((level_db + 60) / 60.0 * 1000);
    __HAL_TIM_SET_COMPARE(&htim_vu, TIM_CHANNEL_1, pwm_duty);
}
```

---

### 2.4 Mute Button (PB15)

**Función:** Silenciar salida sin detener procesamiento

**Configuración:**
```c
GPIO_InitStruct.Pin = GPIO_PIN_15;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_PULLUP;
HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
```

**Implementación:**
```c
void Mute_Toggle(void) {
    mute_active = !mute_active;

    if (mute_active) {
        // Fade out suave
        for (uint8_t atten = 0; atten <= 255; atten++) {
            PCM1690_SetAttenuation_All(atten);  // I2C command
            HAL_Delay(2);  // 512ms total fade
        }
        // Parpadear LED de status para indicar mute
        start_blink_status_led();
    } else {
        // Fade in
        for (int16_t atten = 255; atten >= 0; atten--) {
            PCM1690_SetAttenuation_All((uint8_t)atten);
            HAL_Delay(2);
        }
        stop_blink_status_led();
    }
}
```

---

### 2.5 Calibration/Test Mode (PD9)

**Función:** Activar modo de prueba/calibración

**Modos de test:**
1. **Tone Generator:** Genera tono de 1kHz en todos los canales
2. **Channel Test:** Reproduce tono rotando por cada canal
3. **Beamforming Test:** Genera patrón de delays conocido

```c
typedef enum {
    TEST_MODE_OFF = 0,
    TEST_MODE_TONE_1KHZ,
    TEST_MODE_CHANNEL_SCAN,
    TEST_MODE_BEAMFORM_PATTERN
} TestMode_t;

void Enter_TestMode(TestMode_t mode) {
    switch(mode) {
        case TEST_MODE_TONE_1KHZ:
            // Generar 1kHz a -20dB en todos los canales
            Generate_SineWave(1000.0f, -20.0f);
            break;

        case TEST_MODE_CHANNEL_SCAN:
            // Cada 1 segundo, encender un canal diferente
            for (uint8_t ch = 0; ch < 16; ch++) {
                Enable_Only_Channel(ch);
                HAL_Delay(1000);
            }
            break;

        case TEST_MODE_BEAMFORM_PATTERN:
            // Aplicar patrón de delays conocido
            Apply_Test_Delays();
            break;
    }
}
```

---

### 2.6 Speaker Amplifier Enable (PD11)

**Función:** Control de relé o enable de amplificadores

**Propósito:** Prevenir clicks/pops al encender/apagar

**Configuración:**
```c
GPIO_InitStruct.Pin = GPIO_PIN_11;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
```

**Secuencia segura:**
```c
void Audio_Start_Safe(void) {
    // 1. Asegurar amplificadores deshabilitados
    GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_RESET);

    // 2. Iniciar clocks y DACs
    Start_Audio_Clocks();
    DAC_Init();
    HAL_Delay(100);

    // 3. Iniciar audio con volumen mínimo
    PCM1690_SetAttenuation_All(255);  // -127.5 dB
    Start_Audio_Processing();
    HAL_Delay(50);

    // 4. Habilitar amplificadores
    GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_SET);
    HAL_Delay(100);  // Esperar estabilización

    // 5. Fade in a volumen normal
    Audio_FadeIn(1000);  // 1 segundo
}

void Audio_Stop_Safe(void) {
    // 1. Fade out
    Audio_FadeOut(500);

    // 2. Deshabilitar amplificadores ANTES de detener audio
    GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_RESET);
    HAL_Delay(100);

    // 3. Detener procesamiento
    Stop_Audio_Processing();
    DAC_PowerDown();
}
```

**Esquema de conexión externa:**
```
PD11 ────► Optoacoplador ────► Relé 12V ────► Amplifiers Power
           (4N35)              (G5V-2)
```

---

## 3. Integración con Status LED Existente (PB5)

### 3.1 Patrones de LED de Status

El LED verde en PB5 puede indicar diferentes estados:

| Estado | Patrón | Descripción |
|--------|--------|-------------|
| OFF | Apagado | Sistema apagado |
| ON continuo | Encendido fijo | Audio activo normal |
| Parpadeo lento (1Hz) | 500ms ON / 500ms OFF | Modo de espera |
| Parpadeo rápido (5Hz) | 100ms ON / 100ms OFF | Modo calibración |
| Doble parpadeo | 2 pulsos cada 2s | Mute activo |

```c
typedef enum {
    LED_STATUS_OFF,
    LED_STATUS_ON,
    LED_STATUS_SLOW_BLINK,
    LED_STATUS_FAST_BLINK,
    LED_STATUS_DOUBLE_BLINK
} LED_Pattern_t;

void LED_Status_Set(LED_Pattern_t pattern) {
    switch(pattern) {
        case LED_STATUS_OFF:
            HAL_TIM_Base_Stop_IT(&htim_led);
            GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
            break;

        case LED_STATUS_ON:
            HAL_TIM_Base_Stop_IT(&htim_led);
            GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
            break;

        case LED_STATUS_SLOW_BLINK:
            led_period = 500;
            HAL_TIM_Base_Start_IT(&htim_led);
            break;

        // ... otros patrones
    }
}
```

---

## 4. Máquina de Estados del Sistema

```
    Power OFF
        │
        │ [Power Button Press]
        ▼
    ┌─────────────────┐
    │ INITIALIZING    │
    │ - Start clocks  │
    │ - Init ADC/DAC  │
    │ - LED: Slow blink│
    └────────┬────────┘
             │ (Success)
             ▼
    ┌─────────────────┐
    │ ACTIVE          │◄──────────────────┐
    │ - Audio running │                   │
    │ - LED: ON       │ [Unmute]          │
    └────────┬────────┘                   │
             │                            │
        [Mute Button]                     │
             │                            │
             ▼                            │
    ┌─────────────────┐                   │
    │ MUTED           │───────────────────┘
    │ - Output = 0    │
    │ - LED: Double   │
    └────────┬────────┘
             │
        [Calib Button]
             │
             ▼
    ┌─────────────────┐
    │ CALIBRATION     │
    │ - Test patterns │
    │ - LED: Fast blink│
    └────────┬────────┘
             │
        [Error Detected]
             │
             ▼
    ┌─────────────────┐
    │ ERROR           │
    │ - Safe shutdown │
    │ - Red LED ON    │
    └─────────────────┘
```

---

## 5. Implementación Recomendada

### 5.1 Inicialización de GPIOs

```c
void System_GPIO_Init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Status LED (PB5) - Ya implementado
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // Future implementations:
    // - Power Button (PC7)
    // - Error LED (PD15)
    // - Activity LED (PB13)
    // - Mute Button (PB15)
    // - Calibration (PD9)
    // - Speaker Enable (PD11)
}
```

### 5.2 Button Scanning con Timer

```c
// Timer cada 20ms para escanear botones (anti-debounce)
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim == &htim_button_scan) {
        Button_Scan_All();
    }

    if (htim == &htim_led_blink) {
        LED_Update_Pattern();
    }
}
```

---

## 6. Resumen de Recursos Necesarios

| Recurso | Uso | Cantidad |
|---------|-----|----------|
| GPIOs entrada | Botones | 3 (PC7, PB15, PD9) |
| GPIOs salida | LEDs y control | 3 (PD15, PB13, PD11) |
| GPIOs ya usados | Status LED | 1 (PB5) |
| Timers | Button scan + LED blink | 2 |
| Interrupciones | Opcional para botones | 0-3 (EXTI) |

---

## 7. Próximos Pasos (Cuando se Implemente)

- [ ] Definir configuración de GPIOs en STM32CubeMX
- [ ] Implementar debounce de botones
- [ ] Crear FSM (Finite State Machine) del sistema
- [ ] Implementar patrones de LED
- [ ] Diseñar circuito de relé para speaker enable (si es necesario)
- [ ] Testing de secuencias de power on/off
- [ ] Implementar modo calibración

---

*Documento de planificación - No implementado*
*Creado: Enero 2026*
