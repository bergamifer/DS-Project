# DS Hardware Map - STM32H743VIT6 Pinout

**Proyecto:** Domosonica DS (Master Audio Processor)  
**MCU:** STM32H743VIT6 (WeAct MiniSTM32H7xx)  
**Fecha:** Enero 2026  
**Estado:** En desarrollo

---

## 📋 Índice

- [Tabla de Pines por Puerto](#-tabla-de-pines-por-puerto)
  - [Port A (PA0-PA15)](#port-a-pa0-pa15)
  - [Port B (PB0-PB15)](#port-b-pb0-pb15)
  - [Port C (PC0-PC13)](#port-c-pc0-pc13)
  - [Port D (PD0-PD15)](#port-d-pd0-pd15)
  - [Port E (PE0-PE15)](#port-e-pe0-pe15)
- [Mapeo de Audio (SAI - Serial Audio Interface)](#-mapeo-de-audio-sai---serial-audio-interface)
  - [SAI1 - Digital Audio Interface (DACs)](#sai1---digital-audio-interface-dacs)
  - [SAI4 - Digital Audio Interface (ADC)](#sai4---digital-audio-interface-adc)
- [Conectividad Externa](#-conectividad-externa)
  - [UART7 (Comunicación STM32 ↔ ESP32)](#uart7-comunicación-stm32--esp32)
  - [I2C1 (Control de DACs/ADC)](#i2c1-control-de-dacsadc)
  - [MicroSD (Almacenamiento)](#microsd-almacenamiento)
- [Observaciones y Recomendaciones](#️-observaciones-y-recomendaciones)
- [Resumen de Utilización](#-resumen-de-utilización)
- [Checklist Pre-Fabricación](#-checklist-pre-fabricación)

---

## 📋 Tabla de Pines por Puerto

### Port A (PA0-PA15)

| Pin | Func | GPIO | Asignación | Periférico | Estado | Notas |
|-----|------|------|-----------|-----------|--------|-------|
| PA0 | B17 | - | **LIBRE** | - | ⊘ | Disponible para futura expansión |
| PA1 | B18 | ADC_PDWN | ADC Control (salida) | GPIO | ✓ | Controla power-down del ADC PCM1802 |
| PA2 | B19 | - | **LIBRE** | - | ⊘ | Disponible |
| PA3 | B20 | - | **LIBRE** | - | ⊘ | Disponible |
| PA4 | B21 | - | **LIBRE** | - | ⊘ | Disponible |
| PA5 | B22 | - | **LIBRE** | - | ⊘ | Disponible |
| PA6 | B23 | DAC0_RST | DAC0 Reset (salida) | GPIO | ✓ | Reset activo alto del PCM1690 #0 |
| PA7 | B24 | DAC1_RST | DAC1 Reset (salida) | GPIO | ✓ | Reset activo alto del PCM1690 #1 |
| PA8 | A28 | UART7_RX | UART7 RX (entrada) | UART7 | ✓ | Con R220 serie hacia ESP32. Nivel 3.3V |
| PA9 | A27 | J1_Pin1 | Header Opcional | GPIO | ✓ | Pinout expandible (Header J1) |
| PA10 | A26 | - | **LIBRE** | - | ⊘ | Disponible |
| PA11 | A25 | - | **LIBRE** | - | ⊘ | Disponible (potencial USB, UART) |
| PA12 | A24 | - | **LIBRE** | - | ⊘ | Disponible (potencial USB, UART) |
| PA15 | A23 | UART7_TX | UART7 TX (salida) | UART7 | ✓ | Con R220 serie hacia ESP32. Nivel 3.3V |

### Port B (PB0-PB15)

| Pin | Func | GPIO | Asignación | Periférico | Estado | Notas |
|-----|------|------|-----------|-----------|--------|-------|
| PB0 | B27 | ADC_OSR | ADC OSR (salida) | GPIO | ✓ | Selecciona Over-Sampling Rate del PCM1802 |
| PB1 | B28 | DAC1_AMUTEI | DAC1 Mute Input (salida) | GPIO | ✓ | Entrada de mute activo bajo del PCM1690 #1 |
| PB2 | B29 | - | **LIBRE** | - | ⊘ | Disponible |
| PB3 | A11 | - | **LIBRE** | - | ⊘ | Disponible |
| PB4 | A10 | - | **LIBRE** | - | ⊘ | Disponible |
| PB5 | A9 | LED_STATUS | LED Status (salida) | GPIO | ✓ | LED de estado del sistema |
| PB6 | A8 | - | **LIBRE** | - | ⊘ | Alt: I2C1_SCL (por defecto sin usar) |
| PB7 | A7 | - | **LIBRE** | - | ⊘ | Alt: I2C1_SDA (por defecto sin usar) |
| PB8 | A6 | I2C1_SCL | I2C1 SCL (open-drain) | I2C1 | ✓ | Con pullup 5.1k a 3V3. DACs config |
| PB9 | A5 | I2C1_SDA | I2C1 SDA (open-drain) | I2C1 | ✓ | Con pullup 5.1k a 3V3. DACs config |
| PB10 | B39 | - | **LIBRE** | - | ⊘ | Disponible (Alt: I2C2) |
| PB11 | B40 | ADC_SAI4_SD | SAI4 Serial Data Out (entrada) | SAI4 | ✓ | Audio input del ADC PCM1802. Clock del STM |
| PB12 | A44 | - | **LIBRE** | - | ⊘ | Disponible |
| PB13 | A43 | J1_Pin6 | Header Opcional | GPIO | ✓ | Pinout expandible |
| PB14 | A42 | - | **LIBRE** | - | ⊘ | Disponible |
| PB15 | A41 | J1_Pin5 | Header Opcional | GPIO | ✓ | Pinout expandible |

### Port C (PC0-PC13)

| Pin | Func | GPIO | Asignación | Periférico | Estado | Notas |
|-----|------|------|-----------|-----------|--------|-------|
| PC0 | B11 | ADC_SAI4_FS | SAI4 Frame Sync (entrada) | SAI4 | ✓ | LRCK del ADC. Frecuencia de muestreo |
| PC1 | B12 | ADC_SAI4_SCK | SAI4 Bit Clock (entrada) | SAI4 | ✓ | BCK del ADC. Sincroniza datos seriales |
| PC2 | B13 | - | **LIBRE** | - | ⊘ | Disponible |
| PC3 | B14 | ADC_BYPASS | ADC Bypass Filter (salida) | GPIO | ✓ | Control del filtro bypass del PCM1802 |
| PC4 | B25 | - | **LIBRE** | - | ⊘ | Disponible (Alt: SPI3) |
| PC5 | B26 | DAC1_AMUTEO | DAC1 Mute Output (entrada) | GPIO | ✓ | Confirmación de mute del PCM1690 #1 |
| PC6 | A32 | - | **LIBRE** | - | ⊘ | Disponible |
| PC7 | A31 | J1_Pin3 | Header Opcional | GPIO | ✓ | Pinout expandible |
| PC8 | A30 | MicroSD_D0 | SDMMC D0 | SDMMC | ✓ | Línea de datos MicroSD |
| PC9 | A29 | MicroSD_D1 | SDMMC D1 | SDMMC | ✓ | Línea de datos MicroSD |
| PC10 | A22 | MicroSD_D2 | SDMMC D2 | SDMMC | ✓ | Línea de datos MicroSD |
| PC11 | A21 | MicroSD_D3 | SDMMC D3 | SDMMC | ✓ | Línea de datos MicroSD |
| PC12 | A20 | MicroSD_CK | SDMMC Clock | SDMMC | ✓ | Clock del MicroSD. Velocidad variable |
| PC13 | B9 | - | **LIBRE** | - | ⊘ | Disponible (es RTC en algunos STM32) |

### Port D (PD0-PD15)

| Pin | Func | GPIO | Asignación | Periférico | Estado | Notas |
|-----|------|------|-----------|-----------|--------|-------|
| PD0 | A19 | - | **LIBRE** | - | ⊘ | Disponible |
| PD1 | A18 | DAC0_AMUTEI | DAC0 Mute Input (salida) | GPIO | ✓ | Entrada de mute activo bajo del PCM1690 #0 |
| PD2 | A17 | MicroSD_CMD | SDMMC Command | SDMMC | ✓ | Línea de comando MicroSD |
| PD3 | A16 | DAC0_AMUTEO | DAC0 Mute Output (entrada) | GPIO | ✓ | Confirmación de mute del PCM1690 #0 |
| PD4 | A15 | - | **LIBRE** | - | ⊘ | Disponible |
| PD5 | A14 | - | **LIBRE** | - | ⊘ | Disponible |
| PD6 | A13 | - | **LIBRE** | - | ⊘ | Disponible |
| PD7 | A12 | - | **LIBRE** | - | ⊘ | Disponible |
| PD8 | A40 | - | **LIBRE** | - | ⊘ | Disponible |
| PD9 | A39 | J1_Pin4 | Header Opcional | GPIO | ✓ | Pinout expandible |
| PD10 | A38 | - | **LIBRE** | - | ⊘ | Disponible |
| PD11 | A37 | - | **LIBRE** | - | ⊘ | Disponible |
| PD12 | A36 | - | **LIBRE** | - | ⊘ | Disponible |
| PD13 | A35 | - | **LIBRE** | - | ⊘ | Disponible |
| PD14 | A34 | - | **LIBRE** | - | ⊘ | Disponible |
| PD15 | A33 | J1_Pin2 | Header Opcional | GPIO | ✓ | Pinout expandible |

### Port E (PE0-PE15)

| Pin | Func | GPIO | Asignación | Periférico | Estado | Notas |
|-----|------|------|-----------|-----------|--------|-------|
| PE0 | A4 | - | **LIBRE** | - | ⊘ | Disponible |
| PE1 | A3 | - | **LIBRE** | - | ⊘ | Disponible |
| PE2 | B3 | SAI1_MCLK | SAI1 Master Clock (salida) | SAI1 | ✓ | **CRÍTICO**: MCLK para DAC0, DAC1, ADC. Con R33 serie |
| PE3 | B4 | - | **LIBRE** | - | ⊘ | Disponible |
| PE4 | B5 | DAC0_SAI_FS | SAI1 Frame Sync DAC0 (salida) | SAI1 | ✓ | LRCK del DAC0. Frecuencia muestreo |
| PE5 | B6 | DAC0_SAI_SCK | SAI1 Bit Clock DAC0 (salida) | SAI1 | ✓ | BCK del DAC0. Con R33 serie |
| PE6 | B7 | DAC0_SAI_SD | SAI1 Serial Data DAC0 (salida) | SAI1 | ✓ | Datos de audio hacia DAC0 |
| PE7 | B30 | DAC1_SAI_SD | SAI1 Serial Data DAC1 (salida) | SAI1 | ✓ | Datos de audio hacia DAC1 |
| PE8 | B31 | DAC1_SAI_SCK | SAI1 Bit Clock DAC1 (salida) | SAI1 | ✓ | BCK del DAC1. Con R33 serie |
| PE9 | B32 | DAC1_SAI_FS | SAI1 Frame Sync DAC1 (salida) | SAI1 | ✓ | LRCK del DAC1. Frecuencia muestreo |
| PE10 | B33 | LCD_LED | LCD Backlight (salida) | GPIO | ✓ | Control del LED de fondo LCD |
| PE11 | B34 | LCD_CS | LCD Chip Select (salida) | GPIO | ✓ | Selección del chip LCD |
| PE12 | B35 | LCD_SCL | LCD Serial Clock (salida) | GPIO | ✓ | Clock para interfaz LCD |
| PE13 | B36 | LCD_WR_RS | LCD Write/RS (salida) | GPIO | ✓ | Escritura/Register Select LCD |
| PE14 | B37 | LCD_SDA | LCD Serial Data (salida) | GPIO | ✓ | Datos para interfaz LCD |
| PE15 | B38 | - | **LIBRE** | - | ⊘ | Disponible |

---

## 🎵 Mapeo de Audio (SAI - Serial Audio Interface)

### SAI1 - Digital Audio Interface (DACs)

```
STM32H743            DAC0 (PCM1690)       DAC1 (PCM1690)
┌───────────────┐    ┌──────────────┐    ┌──────────────┐
│               │    │              │    │              │
│ PE2 ──R33──► MCLK ─────────────────────► MCLK         │
│ (SAI1_MCLK)  │    │              │    │              │
│               │    │              │    │              │
│ PE4 ───────► LRCK  (DAC0 LRCK)        │              │
│ (FS)         │    │              │    │              │
│               │    │              │    │              │
│ PE5 ──R33──► BCK   (DAC0 BCK)        │              │
│ (SCK)        │    │              │    │              │
│               │    │              │    │              │
│ PE6 ───────► DIN1  (Datos DAC0)      │              │
│ (SD)         │    │              │    │              │
│               │    │              │    │              │
│ PE9 ───────────────────────────► LRCK (DAC1 LRCK)  │
│ (FS)         │    │              │    │              │
│               │    │              │    │              │
│ PE8 ──R33──────────────────────► BCK  (DAC1 BCK)   │
│ (SCK)        │    │              │    │              │
│               │    │              │    │              │
│ PE7 ───────────────────────────► DIN1 (Datos DAC1) │
│ (SD)         │    │              │    │              │
└───────────────┘    └──────────────┘    └──────────────┘
                     I2C Control:        I2C Control:
                     PB8 (SCL) ◄──────────► SCL
                     PB9 (SDA) ◄──────────► SDA
                     
                     Mute Control:       Mute Control:
                     PD1 (AMUTEI) ──────► AMUTEI
                     PD3 (AMUTEO) ◄────── AMUTEO
                     
                     PB1 (AMUTEI) ──────► AMUTEI
                     PC5 (AMUTEO) ◄────── AMUTEO
```

**Observación:** Ambos DACs comparten MCLK, FS (LRCK) y SCK del SAI1. Datos (DIN1) van en paralelo desde PE6 y PE7. Config vía I2C1.

### SAI4 - Digital Audio Interface (ADC)

```
STM32H743            ADC (PCM1802)
┌───────────────┐    ┌──────────────┐
│               │    │              │
│ PC0 ◄────── LRCK   (ADC LRCK)     │
│ (FS input)   │    │              │
│               │    │              │
│ PC1 ◄────── BCK    (ADC BCK)      │
│ (SCK input)  │    │              │
│               │    │              │
│ PB11 ◄────── DOUT  (Datos ADC)    │
│ (SD input)   │    │              │
│               │    │              │
│ PC3 ───────► BYPASS (Filtro)      │
│ (GPIO out)   │    │              │
│               │    │              │
│ PA1 ───────► PDWN  (Power Down)   │
│ (GPIO out)   │    │              │
│               │    │              │
│ PB0 ───────► OSR   (Over-Sample)  │
│ (GPIO out)   │    │              │
└───────────────┘    └──────────────┘
                     I2C Control:
                     PB8 (SCL) ◄──── SCL
                     PB9 (SDA) ◄──── SDA
```

**Observación:** ADC PCM1802 configurado como ESCLAVO. Recibe MCLK (PE2), BCK y LRCK desde el STM32 (SAI4 o wired desde DAC). El STM32 recibe datos de audio vía DOUT (PB11). Sincronización garantizada por MCLK común.

---

## 🔌 Conectividad Externa

### UART7 (Comunicación STM32 ↔ ESP32)

| Pin | Dirección | Función | Nivel | Resistor | Notas |
|-----|-----------|---------|-------|----------|-------|
| PA8 | ← | RX (entrada) | 3.3V | R220 | Recibe datos del ESP32 |
| PA15 | → | TX (salida) | 3.3V | R220 | Envía datos al ESP32 |

**Advertencia:** Los resistores 220Ω en serie son atenuadores, no conversores de nivel. Si el ESP32 genera 3.3V, está OK. Si genera 5V, necesitarías un divisor de tensión real en RX (220Ω + 330Ω a GND).

### I2C1 (Control de DACs/ADC)

| Pin | Función | Nivel | Pullup | Velocidad | Dispositivos |
|-----|---------|-------|--------|-----------|--------------|
| PB8 | SCL | 3.3V (open-drain) | 5.1k a 3V3 | 100/400 kHz | PCM1690 #0, #1, PCM1802 |
| PB9 | SDA | 3.3V (open-drain) | 5.1k a 3V3 | 100/400 kHz | PCM1690 #0, #1, PCM1802 |

### MicroSD (Almacenamiento)

| Pin | Función | Velocidad | Notas |
|-----|---------|-----------|-------|
| PC12 | CLK | Configurable | Controlador SDMMC |
| PD2 | CMD | Configurable | Comando/Respuesta |
| PC8-11 | D0-D3 | Configurable | 4-bit mode |

**OK:** Estándar SDMMC. Velocidades típicas 25-50 MHz en modo regular.

---

## ⚠️ Observaciones y Recomendaciones

### 1. **CRITICAL: MCLK - Sincronización Master**
- ✓ PE2 es el MCLK que alimenta todos los chips de audio
- **Acción:** Verificá que las 3 pistas hacia DAC0, DAC1 y ADC tengan **equal length matching** (±0.05-0.1mm)
- **Resistor:** R17 (33Ω) está correcto como impedancia serie
- **Frecuencia típica:** 12-24 MHz. ¿A qué frecuencia estás corriendo?

### 2. **SAI1 - Configuración Dual para DACs (SAI1_A y SAI1_B)**
- ✓ SAI1_A → DAC0 (PE4=FS, PE5=SCK, PE6=SD)
- ✓ SAI1_B → DAC1 (PE9=FS, PE8=SCK, PE7=SD)
- ✓ **MCLK compartido:** PE2 (SAI1_MCLK_A) alimenta ambos DACs
- **Configuración:**
  - SAI1_A: Master TX, genera su propio FS y SCK
  - SAI1_B: Master TX, sincronizado con SAI1_A (SYNCEN[1:0]=01)
  - Resultado: Dos buses I2S/TDM independientes con sync perfecto
- **Ventaja:** 8 canales por DAC = 16 canales totales con timing idéntico

### 3. **SAI4 - ADC en Modo Esclavo (Configuración Confirmada)**
- ✓ PC0, PC1, PB11 configurados como **entradas** en el STM32
- **Configuración ADC PCM1802:**
  - MODE0=GND, MODE1=GND → **Slave mode (256/384/512 fS)**
  - Recibe MCLK desde PE2 (compartido con DACs vía hardware)
  - **El ADC NO genera clocks**, los recibe del sistema
- **Arquitectura de clock:**
  1. STM32 genera MCLK (PE2) → alimenta DACs y ADC
  2. **Opción A:** SAI4 genera BCK/LRCK en modo Master y los envía al ADC
  3. **Opción B:** ADC recibe BCK/LRCK wired desde DAC0 (PE5, PE4)
  4. ADC sincroniza conversión y envía datos (DOUT) al STM32 vía PB11
- **Recomendación:** Usar Opción A (SAI4 Master) para control total del timing

### 4. **I2C1 - Direcciones de los Chips (formato 7-bit)**
- PB8 (SCL) y PB9 (SDA) controlan 3 chips:
  - PCM1690 #0 (DAC0) → **0x4C** (AD1=GND, AD0=GND)
  - PCM1690 #1 (DAC1) → **0x4E** (AD1=HIGH, AD0=GND)
  - PCM1802 (ADC) → **0x4D** (dirección fija)
- **Strapping Pins PCM1690:**
  - TEST/ADR1 y MS/ADR0/RSV se conectan a GND o nivel alto
  - **IMPORTANTE:** Verificar en datasheet si son **5V tolerant** o requieren **3.3V máximo**
  - DAC0: TEST=GND, MS=GND → addr 0x4C
  - DAC1: TEST=HIGH, MS=GND → addr 0x4E
  - Conectar a **3.3V** (seguro) o 5V (verificar tolerancia)
- **Nota:** Todas las direcciones en formato 7-bit. HAL_I2C usa este formato


### 6. **GPIO de Control - Timing**
- PA1 (ADC_PDWN), PB0 (ADC_OSR), PC3 (ADC_BYPASS): OK, son salidas de control simple
- PD1, PD3, PB1, PC5 (MUTE): Son entradas/salidas de control. Verifica en el datasheet PCM1690 si requieren debounce o timing específico

### 7. **Pines Libres - Sugerencias**
Con **28 pines libres**, podrías considerar:
- **PA10, PA11** (USB si necesitas reprogramar sin UART)
- **PC2, PC13** (GPIO adicionales para futuras funciones)
- **PD4-11** (Puerto completo disponible para expansión SPI, otros periféricos)
- **PB2, PB3, PB4** (Expansión GPIO)

### 8. **MicroSD - Velocidad**
- Configuración estándar SDMMC
- ✓ OK para almacenamiento de configuración
- **Nota:** Si necesitas grabación de audio en tiempo real, considera una SD de alta velocidad (Clase 10 UHS-II)

### 9. **LCD - Interfaz GPIO**
- PE10-14 están asignados a LCD
- Son GPIO bitbanged (software SPI/I2C)
- ✓ OK, pero con velocidades limitadas (típicamente < 10 MHz)

### 10. **Layout Recomendaciones**
- Separa analógico (ADC input) de digital (DAC output, clock)
- MCLK (PE2) debe ir en pista de baja impedancia y length-matched
- Agrupa capacitores de decoupling cerca de los chips
- SAI4 (ADC inputs) con buena integridad: bajos 90° corners, evita ruido digital

---

## 📊 Resumen de Utilización

- **Pines Utilizados:** 44 - 28 = **16 pines** (36% del STM32)
- **Pines Libres:** **28 pines** (64% disponible)
- **Periféricos Usados:**
  - SAI1 (audio DACs) ✓
  - SAI4 (audio ADC) ✓
  - I2C1 (config audio) ✓
  - UART7 (comunicación STM32-ESP32) ✓
  - SDMMC (MicroSD) ✓
  - GPIO (control y status) ✓

---

## 🔧 Checklist Pre-Fabricación

- [ ] Verificar direcciones I2C de los 3 chips (strapping pins)
- [ ] Confirmar frecuencia de MCLK (¿12 MHz? ¿24 MHz?)
- [ ] Length matching de MCLK a ±0.05mm
- [ ] Nivel de voltaje UART7 (¿ESP32 es 3.3V o 5V?)
- [ ] Decoupling capacitors cerca de cada chip (100nF + 10µF)
- [ ] Ground planes sólido bajo SAI1/SAI4
- [ ] Ferrite bead en 5V_analog (FB1) funcionando correctamente
- [ ] Cronometría SAI: ADC y DACs sincronizados al mismo MCLK
- [ ] Layout: no pasar datos SAI sobre zonas de ruido digital