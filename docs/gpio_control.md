# DS11 — GPIO Control y Señales de Sistema

**Placa:** DS11 (STM32H743VIT6) — GPIO idéntico a DS10
**Última revisión:** Abril 2026 — reescrito para DS10

> **Cambio mayor DS9→DS10:** DS9 tenía 12 pines GPIO de control por-chip para los 4 PCM1690 (RST×4, AMUTEI×4, AMUTEO×4). DS10 simplifica a un bus compartido de 3 señales. PCM1802 (ADC 2ch) reemplazado por AK5538VN (ADC 8ch) con su propia señal PDN.

---

## 1. Señales de Control Implementadas

### 1.1 Control DAC Compartido (4× PCM1690)

DS10 conecta RST y AMUTEI de los 4 PCM1690 en bus — un solo GPIO controla todos.

| Señal | Pin | Dirección | Nivel activo | Función |
|-------|-----|-----------|-------------|---------|
| `DAC_RST` | PC11 | Output | LOW | ~RST a los 4 chips simultáneamente |
| `DAC_MUTE_IN` | PC12 | Output | LOW | AMUTEI — silencia todos los DACs |
| `DAC_MUTE_OUT` | PC10 | Input | LOW | AMUTEO wire-OR — cualquier DAC auto-silenciado |

```c
// Secuencia de init DAC (DS10)
HAL_GPIO_WritePin(DAC_RST_PORT, DAC_RST_PIN, GPIO_PIN_RESET);   // PC11 LOW — en reset
HAL_GPIO_WritePin(DAC_MUTE_IN_PORT, DAC_MUTE_IN_PIN, GPIO_PIN_RESET); // PC12 LOW — muted

// ... SAI DMA running, MCLK corriendo ...
HAL_Delay(10);  // MCLK time before RST release (PCM1690 POR requirement)

HAL_GPIO_WritePin(DAC_RST_PORT, DAC_RST_PIN, GPIO_PIN_SET);     // PC11 HIGH — release reset
HAL_Delay(12);                                                   // Wait POR complete

for (uint32_t d = 0; d < 4; d++) {
    PCM1690_Init(&hi2c1, dac_addrs[d], NULL);  // NULL = DS10 no per-chip GPIO
}

HAL_GPIO_WritePin(DAC_MUTE_IN_PORT, DAC_MUTE_IN_PIN, GPIO_PIN_SET); // PC12 HIGH — unmute

// Diagnóstico: PC10 debe ser HIGH tras RST (si algún DAC sigue LOW → falla de MCLK/alimentación)
if (HAL_GPIO_ReadPin(DAC_MUTE_OUT_PORT, DAC_MUTE_OUT_PIN) == GPIO_PIN_RESET) {
    Debug_Print("WARN: DAC_MUTE_OUT LOW — check MCLK / supply\r\n");
}
```

### 1.2 PAM8302A Amplificadores (PD3)

32× PAM8302A Class-D amps con shutdown compartido:

| Señal | Pin | Dirección | Activo | Función |
|-------|-----|-----------|--------|---------|
| `PAM_SD` | PD3 | Output | LOW | Shutdown todos los amps (pull-down R19 en PCB) |

```c
// Encender amps — DESPUÉS de inicializar DACs y unmute
HAL_GPIO_WritePin(PAM_SD_PORT, PAM_SD_PIN, GPIO_PIN_SET);   // PD3 HIGH — amps on

// Apagar amps — ANTES de reset del sistema
HAL_GPIO_WritePin(PAM_SD_PORT, PAM_SD_PIN, GPIO_PIN_RESET); // PD3 LOW — amps off
```

**Secuencia segura anti-pop:**
1. `PAM_SD = LOW` (amps off)
2. DACs inicializados y silenciados vía `DAC_MUTE_IN`
3. `DAC_MUTE_IN = HIGH` (unmute)
4. `PAM_SD = HIGH` (amps on)

### 1.3 AK5538VN ADC Control

| Señal | Pin | Dirección | Activo | Función |
|-------|-----|-----------|--------|---------|
| `AK_PDN` | PC7 | Output | LOW | Power-down AK5538VN — LOW=off, HIGH=running |
| `AK_DIF1` | PC8 | Output | — | Interface mode (strapped LOW en PCB = TDM mode) |
| `AK_OVF` | PA15 | Input | HIGH | Overflow flag output del AK5538VN |

```c
// Init ADC — PDN LOW durante init, liberar después de SAI3_B DMA running
HAL_GPIO_WritePin(AK_PDN_PORT, AK_PDN_PIN, GPIO_PIN_RESET);  // PC7 LOW — power-down

// ... SAI3_B DMA start ...
HAL_Delay(5);

HAL_GPIO_WritePin(AK_PDN_PORT, AK_PDN_PIN, GPIO_PIN_SET);    // PC7 HIGH — active
HAL_Delay(10);   // AK5538 startup time

// Verificar overflow (debería ser LOW normalmente)
if (HAL_GPIO_ReadPin(AK_OVF_PORT, AK_OVF_PIN) == GPIO_PIN_SET) {
    Debug_Print("WARN: AK5538 OVF flag set\r\n");
}
```

**Nota (corregida tras bring-up 2026-06):** El strapping de hardware solo configura el modo esclavo (MSN=GND). **El modo TDM256 NO queda strapped y DEBE escribirse por I2C3 (addr 0x22)** en cada boot: `R01=0x00 → R03=0x40 → R01=0x01`. Sin esto el chip queda en modo estéreo y el SAI recibe datos misframed. Ver [ak5538vn_ref.md](ak5538vn_ref.md).

### 1.4 EEPROM Write-Protect (PB5)

| Señal | Pin | Dirección | Activo | Función |
|-------|-----|-----------|--------|---------|
| `EEPROM_WP` | PB5 | Output | HIGH | Write-protect CAT24C256 (U8, 32KB) |

```c
// Guardar config beam — secuencia
HAL_GPIO_WritePin(EEPROM_WP_PORT, EEPROM_WP_PIN, GPIO_PIN_RESET);  // WP off
EEPROM_Save_Config(&hi2c4, &beam_config);  // bloquea ~12ms (2 page writes)
HAL_GPIO_WritePin(EEPROM_WP_PORT, EEPROM_WP_PIN, GPIO_PIN_SET);    // WP on

// NOTA: save bloquea ~12ms y causa ~6 underruns DMA — usar save diferido via flag
```

---

## 2. LEDs de Estado — TIM3 PWM

4 LEDs con PWM 1kHz, 300 pasos de intensidad via TIM3.

| LED | Ref | Pin | Canal TIM3 | Color | Alias |
|-----|-----|-----|-----------|-------|-------|
| D4 | — | PA6 | CH1 AF2 | Amarillo | `LED_POWER` |
| D5 | — | PA7 | CH2 AF2 | Rojo | `LED_AUDIO` |
| D6 | — | PB0 | CH3 AF2 | Verde | `LED_COMM` |
| D7 | — | PB1 | CH4 AF2 | Blanco | `LED_STATUS` |

```c
// Control de brillo (0-299)
__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, brightness);  // D4 Amarillo
__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, brightness);  // D5 Rojo
__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, brightness);  // D6 Verde
__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, brightness);  // D7 Blanco
```

**Patrones de estado sugeridos:**

| Estado del sistema | D4 Amarillo | D5 Rojo | D6 Verde | D7 Blanco |
|-------------------|-------------|---------|----------|-----------|
| Boot / init | Rampa | OFF | OFF | OFF |
| Audio activo | ON | VU meter | OK comm | Heartbeat |
| Error DMA | OFF | Parpadeo rápido | OFF | OFF |
| Mute global | Dim | OFF | OFF | Parpadeo lento |
| ESP32 desconectado | ON | OFF | Parpadeo | ON |

**Dimmer sweep (loop principal actual):**
```c
// Animación sweep 0..299..0 en todos los LEDs (implementado en main.c)
static int dimmer = 0, dir = 1;
if ((DWT->CYCCNT - last_dim) >= DIM_PERIOD) {
    last_dim = DWT->CYCCNT;
    dimmer += dir;
    if (dimmer >= 299 || dimmer <= 0) dir = -dir;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, dimmer);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, dimmer);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, dimmer);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, dimmer);
}
```

---

## 3. Botón de Usuario (PA4)

| Señal | Pin | Configuración | Función |
|-------|-----|--------------|---------|
| `OPT_SW_K2` | PA4 | Input, pull-up externo | Botón usuario K2 |

Uso actual: no implementado en firmware (disponible para futuro).

---

## 4. UART7 Debug Header (J7)

| Señal | Pin | Función |
|-------|-----|---------|
| UART7_TX | PE8 AF7 | STM32 → J7/pin8 |
| UART7_RX | PE7 AF7 | J7/pin7 → STM32 |

Disponible para debug adicional o protocolo de expansión. No activo por defecto.

---

## 5. Pines Configurados como GPIO_ANALOG (sin usar)

Para minimizar consumo y EMI, los pines sin función se configuran como Analog:

```c
// Pines sin función — GPIO_ANALOG para reducir EMI
// PE0,PE1,PE9,PE10,PE12-PE15 (DS9: DAC RST/MUTE per-chip — obsoletos en DS10)
// PB2, PB4, PB15
// PC0, PC1, PC2, PC3, PC6
// PD2, PD5, PD6, PD14, PD15
// PA5, PA9, PA10
```

---

## 6. Resumen de Recursos de Control

| Recurso | Pines | Estado |
|---------|-------|--------|
| DAC control compartido | PC10, PC11, PC12 | Implementado |
| Amp enable | PD3 | Implementado |
| ADC control | PC7, PC8, PA15 | Implementado |
| EEPROM WP | PB5 | Implementado |
| LEDs PWM (4ch) | PA6, PA7, PB0, PB1 | Implementado |
| Botón usuario | PA4 | Disponible |
| UART7 debug | PE7, PE8 | Disponible |
| I2C2 expansión | PB10, PB11 | Disponible |

---

*DS10 GPIO Control — Abril 2026*
*Ver [pin_map.md](pin_map.md) para mapa completo de todos los pines*
