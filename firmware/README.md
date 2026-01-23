# Domosonica DS Firmware

Firmware base para STM32H743VIT6 (WeAct MiniSTM32H7xx).

## Estructura

```
firmware/
├── Core/
│   ├── Inc/
│   │   ├── main.h              # Header principal
│   │   ├── pin_definitions.h   # Definiciones de pines
│   │   └── stm32h7xx_it.h      # Prototipos de interrupciones
│   └── Src/
│       ├── main.c              # Punto de entrada
│       ├── system_clock.c      # Configuración de clocks
│       ├── gpio.c              # Configuración de GPIO
│       ├── i2c.c               # I2C1 para DACs/ADC
│       ├── uart_debug.c        # UART7 debug
│       └── stm32h7xx_it.c      # Handlers de interrupciones
└── README.md
```

## Uso en STM32CubeIDE

### Opción 1: Crear proyecto nuevo e importar código

1. **Crear proyecto STM32CubeIDE:**
   - File → New → STM32 Project
   - Seleccionar STM32H743VIT6
   - Nombre: `DS-Firmware`
   - Targeted Language: C
   - Targeted Binary Type: Executable

2. **Configurar en .ioc (CubeMX):**
   - RCC → HSE: Crystal/Ceramic Resonator
   - Clock Configuration: ver `stm32/system_init_sequence.md`
   - No configurar periféricos (lo hacemos en código)

3. **Copiar archivos:**
   - Reemplazar `Core/Src/*.c` con los de este directorio
   - Reemplazar `Core/Inc/*.h` con los de este directorio

4. **Build:**
   - Project → Build Project

### Opción 2: Solo usar como referencia

Los archivos `.c` contienen código de inicialización que puede
copiarse a un proyecto existente generado por CubeMX.

## Clocks configurados

| Clock | Frecuencia | Uso |
|-------|------------|-----|
| HSE | 25 MHz | Crystal de la placa WeAct |
| SYSCLK | 480 MHz | CPU |
| HCLK | 240 MHz | AXI/AHB buses |
| APB1/APB2 | 120 MHz | Periféricos |
| PLL3P | 12.288 MHz | Audio MCLK (256 × 48 kHz) |

## Pines utilizados

Ver `pin_definitions.h` para la lista completa.

### Mínimos para pruebas sin hardware de audio:

| Pin | Función | Notas |
|-----|---------|-------|
| PB5 | LED_STATUS | Heartbeat 1 Hz |
| PB8 | I2C1_SCL | Bus I2C (sin dispositivos) |
| PB9 | I2C1_SDA | Bus I2C (sin dispositivos) |
| PA8 | UART7_RX | Debug input |
| PA15 | UART7_TX | Debug output @ 115200 |

## Output esperado (UART @ 115200)

```
====================================
  Domosonica DS - Audio Processor
  STM32H743VIT6 @ 480 MHz
====================================

=== Clock Configuration ===
SYSCLK: 480 MHz
HCLK:   240 MHz
PCLK1:  120 MHz
PCLK2:  120 MHz
PLL3:   12.288 MHz (audio)
===========================

I2C1 initialized @ 100 kHz

--- I2C Bus Scan ---
No I2C devices found.
(This is expected without DAC/ADC hardware)
--------------------

Heartbeat LED initialized (PB5)

System ready. Entering main loop.
LED will blink at 1 Hz.

Uptime: 10 seconds
Uptime: 20 seconds
...
```

## Próximos pasos

Con hardware de audio (PCM1690 + PCM1802):

1. Implementar drivers de DAC/ADC
2. Configurar SAI1 (TDM 8 canales × 2)
3. Configurar SAI4 (I2S 2 canales)
4. Configurar DMA/BDMA para audio streaming
5. Implementar delay lines para beamforming
