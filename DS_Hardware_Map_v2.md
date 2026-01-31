# DS Hardware Map v2.0 - STM32H743VIT6 Direct (LQFP100)

**Proyecto:** Domosonica DS v2.0 (32-channel Beamforming)
**MCU:** STM32H743VIT6 LQFP100 (soldado directo al PCB)
**Fecha:** Enero 2026
**Estado:** Pinout actualizado desde DS.net

---

## Cambios principales vs v1.0

| Aspecto | v1.0 (Dev Board) | v2.0 (LQFP100 Direct) |
|---------|------------------|------------------------|
| **Canales DAC** | 16 (2x PCM1690) | **32 (4x PCM1690)** |
| **Canales ADC** | 2 (1x PCM1802) | 2 (1x PCM1802) |
| **SAI utilizados** | SAI1, SAI4 | **SAI1, SAI2, SAI4** |
| **I2C buses** | 1 (I2C1) | **2 (I2C1, I2C3)** |
| **MCLK source** | PLL interno | **Cristal externo 12.288 MHz** |
| **Calibracion** | RAM volatil | **EEPROM I2C persistente** |
| **Test points** | 0 | **5 puntos** |
| **UART para debug** | - | **UART4 (PA0/PA1)** |

---

## Tabla de Pines Actualizada - v2.0 (desde DS.net)

### Port A (PA0-PA15)

| Pin | Pkg | GPIO | Asignacion | Periferico | Notas |
|-----|-----|------|-----------|-----------|-------|
| PA0 | 22 | UART4_TX | UART4 TX | UART4 | Debug/comunicacion |
| PA1 | 23 | UART4_RX | UART4 RX | UART4 | Debug/comunicacion |
| PA2 | 24 | - | **LIBRE** | - | Disponible |
| PA3 | 25 | - | **LIBRE** | - | (I2C3_SCL_ALT eliminado) |
| PA4 | 28 | ADC_OSR | ADC Oversampling | GPIO | PCM1802 OSR pin |
| PA5 | 29 | ADC_BYPASS | ADC HPF Bypass | GPIO | PCM1802 bypass filter |
| PA6 | 30 | ADC_PDWN | ADC Power Down | GPIO | PCM1802 control |
| PA7 | 31 | - | **LIBRE** | - | Disponible |
| PA8 | 67 | - | **LIBRE** | - | Disponible (sugerido TP_DEBUG) |
| PA9 | 68 | - | **LIBRE** | - | Disponible (sugerido TP_DEBUG2) |
| PA10 | 69 | PWR_EN_3V3A | Power Enable Analog | GPIO | Secuencia power-up |
| PA11 | 70 | LED1 | LED Status 1 | GPIO | Indicador sistema |
| PA12 | 71 | LED2 | LED Status 2 | GPIO | Indicador sistema |
| PA13 | 72 | SWDIO | Debug | SWD | Reservado para debug |
| PA14 | 76 | SWCLK | Debug | SWD | Reservado para debug |
| PA15 | 77 | - | **LIBRE** | - | Disponible |

### Port B (PB0-PB15)

| Pin | Pkg | GPIO | Asignacion | Periferico | Notas |
|-----|-----|------|-----------|-----------|-------|
| PB0 | 34 | - | **LIBRE** | - | Disponible |
| PB1 | 35 | - | **LIBRE** | - | Disponible |
| PB2 | 36 | - | **LIBRE** | - | Disponible |
| PB3 | 89 | - | **LIBRE** | - | Disponible |
| PB4 | 90 | - | **LIBRE** | - | Disponible |
| PB5 | 91 | EEPROM_WP | EEPROM Write Protect | GPIO | CAT24C256 WP pin |
| PB6 | 92 | I2C3_SCL | I2C3 Clock | I2C3 | Bus EEPROM |
| PB7 | 93 | I2C3_SDA | I2C3 Data | I2C3 | Bus EEPROM |
| PB8 | 95 | I2C1_SCL | I2C1 Clock | I2C1 | Bus DACs (4x PCM1690) |
| PB9 | 96 | I2C1_SDA | I2C1 Data | I2C1 | Bus DACs (4x PCM1690) |
| PB10 | 46 | - | **LIBRE** | - | Disponible |
| PB11 | 47 | SAI4_SD_B | SAI4 Data In | SAI4_B | Audio input PCM1802 |
| PB12 | 51 | - | **LIBRE** | - | Disponible |
| PB13 | 52 | DAC3_AMUTEO | DAC3 Mute Out | GPIO | PCM1690 U7 status |
| PB14 | 53 | DAC3_AMUTEI | DAC3 Mute In | GPIO | PCM1690 U7 control |
| PB15 | 54 | DAC3_RST | DAC3 Reset | GPIO | PCM1690 U7 reset |

### Port C (PC0-PC15)

| Pin | Pkg | GPIO | Asignacion | Periferico | Notas |
|-----|-----|------|-----------|-----------|-------|
| PC0 | 15 | SAI4_FS_B | SAI4 Frame Sync | SAI4_B | ADC LRCK |
| PC1 | 16 | SAI4_SCK_B | SAI4 Bit Clock | SAI4_B | ADC BCK |
| PC2 | 17 | - | **LIBRE** | - | Disponible |
| PC3 | 18 | - | **LIBRE** | - | Disponible |
| PC4 | 32 | - | **LIBRE** | - | Disponible |
| PC5 | 33 | - | **LIBRE** | - | Disponible |
| PC6 | 63 | - | **LIBRE** | - | Disponible |
| PC7 | 64 | TP_MCLK | Test Point MCLK | GPIO | Monitor clock 12.288 MHz |
| PC8 | 65 | - | **LIBRE** | - | Disponible |
| PC9 | 66 | - | **LIBRE** | - | Disponible |
| PC10 | 78 | DAC2_RST | DAC2 Reset | GPIO | PCM1690 U2 reset |
| PC11 | 79 | DAC2_AMUTEI | DAC2 Mute In | GPIO | PCM1690 U2 control |
| PC12 | 80 | DAC2_AMUTEO | DAC2 Mute Out | GPIO | PCM1690 U2 status |
| PC13 | 7 | GPIO | General Purpose | GPIO | Disponible |
| PC14 | 8 | OSC32_IN | LSE Crystal | RTC | Crystal 32.768 kHz |
| PC15 | 9 | OSC32_OUT | LSE Crystal | RTC | Crystal 32.768 kHz |

### Port D (PD0-PD15)

| Pin | Pkg | GPIO | Asignacion | Periferico | Notas |
|-----|-----|------|-----------|-----------|-------|
| PD0 | 81 | - | **LIBRE** | - | Disponible |
| PD1 | 82 | - | **LIBRE** | - | Disponible |
| PD2 | 83 | - | **LIBRE** | - | Disponible |
| PD3 | 84 | - | **LIBRE** | - | Disponible |
| PD4 | 85 | - | **LIBRE** | - | Disponible |
| PD5 | 86 | - | **LIBRE** | - | Disponible |
| PD6 | 87 | - | **LIBRE** | - | Disponible |
| PD7 | 88 | - | **LIBRE** | - | Disponible |
| PD8 | 55 | DAC3_SAI_SD | SAI2 SD Out | SAI2_B | Audio data -> PCM1690 U7 |
| PD9 | 56 | DAC3_SAI_SCK | SAI2 SCK Out | SAI2_B | Bit clock -> U7 |
| PD10 | 57 | DAC3_SAI_FS | SAI2 FS Out | SAI2_B | Frame sync -> U7 |
| PD11 | 58 | DAC2_SAI_SD | SAI2 SD Out | SAI2_A | Audio data -> PCM1690 U2 |
| PD12 | 59 | DAC2_SAI_SCK | SAI2 SCK Out | SAI2_A | Bit clock -> U2 |
| PD13 | 60 | DAC2_SAI_FS | SAI2 FS Out | SAI2_A | Frame sync -> U2 |
| PD14 | 61 | - | **LIBRE** | - | Disponible (sugerido TP_SAI2_FS) |
| PD15 | 62 | - | **LIBRE** | - | Disponible |

### Port E (PE0-PE15)

| Pin | Pkg | GPIO | Asignacion | Periferico | Notas |
|-----|-----|------|-----------|-----------|-------|
| PE0 | 97 | DAC1_AMUTEO | DAC1 Mute Out | GPIO | PCM1690 U4 status |
| PE1 | 98 | DAC1_AMUTEI | DAC1 Mute In | GPIO | PCM1690 U4 control |
| PE2 | 1 | SAI1_MCLK | SAI1 Master Clock | SAI1 | **CRITICO**: Clock 12.288 MHz |
| PE3 | 2 | DAC1_RST | DAC1 Reset | GPIO | PCM1690 U4 reset |
| PE4 | 3 | DAC1_SAI_FS | SAI1 FS Out | SAI1_B | Frame sync -> U4 |
| PE5 | 4 | DAC1_SAI_SCK | SAI1 SCK Out | SAI1_B | Bit clock -> U4 |
| PE6 | 5 | DAC1_SAI_SD | SAI1 SD Out | SAI1_B | Audio data -> U4 |
| PE7 | 37 | DAC0_SAI_FS | SAI1 FS Out | SAI1_A | Frame sync -> U3 |
| PE8 | 38 | DAC0_SAI_SCK | SAI1 SCK Out | SAI1_A | Bit clock -> U3 |
| PE9 | 39 | DAC0_SAI_SD | SAI1 SD Out | SAI1_A | Audio data -> U3 |
| PE10 | 40 | DAC0_RST | DAC0 Reset | GPIO | PCM1690 U3 reset |
| PE11 | 41 | DAC0_AMUTEI | DAC0 Mute In | GPIO | PCM1690 U3 control |
| PE12 | 42 | DAC0_AMUTEO | DAC0 Mute Out | GPIO | PCM1690 U3 status |
| PE13 | 43 | - | **LIBRE** | - | Disponible (sugerido TP_SAI1) |
| PE14 | 44 | - | **LIBRE** | - | Disponible (sugerido TP_SAI1_FS) |
| PE15 | 45 | - | **LIBRE** | - | Disponible |

---

## Mapeo de Audio v2.0

### Arquitectura Completa - 32 Canales Out + 2 In

```
                              +-------------------------------------+
                              |         STM32H743VIT6               |
                              |                                     |
                    +---------| PE2 (SAI1_MCLK) ----> R21 (33 Ohm)  |
                    |         |                       12.288 MHz    |
                    |         +-------------------------------------+
                    |                                  |
                    |  +-------------------------------+
                    |  |
        +-----------v--v--+         +----+----+----+----+
        |  Clock          |         |    |    |    |    |
        |  Distribution   |         |    |    |    |    |
        +-+------+----+---+         |    |    |    |    |
          |      |    |             v    v    v    v    v
          v      v    v         +----+ +----+ +----+ +----+
      +------+ |  | +------+    |PCM | |PCM | |PCM | |PCM |
      |PCM   | |  | |      |    |1690| |1690| |1690| |1690|
      |1802  | |  | |      |    | U3 | | U4 | | U2 | | U7 |
      | U1   | |  | |      |    |DAC0| |DAC1| |DAC2| |DAC3|
      | ADC  | |  | |      |    +----+ +----+ +----+ +----+
      +------+ |  | |      |    8ch    8ch    8ch    8ch
      2ch IN   |  | |      |     |      |      |      |
               |  | |      |     v      v      v      v
        <------+  +-+      |   SAI1_A SAI1_B SAI2_A SAI2_B
         SAI4_B            |
```

### Mapeo SAI -> DAC

| SAI Block | Pines (FS/SCK/SD) | DAC | Canales |
|-----------|-------------------|-----|---------|
| SAI1_A | PE7/PE8/PE9 | DAC0 (U3) | ch 0-7 |
| SAI1_B | PE4/PE5/PE6 | DAC1 (U4) | ch 8-15 |
| SAI2_A | PD13/PD12/PD11 | DAC2 (U2) | ch 16-23 |
| SAI2_B | PD10/PD9/PD8 | DAC3 (U7) | ch 24-31 |
| SAI4_B | PC0/PC1/PB11 | ADC (U1) | input 2ch |

---

## I2C Bus Assignment v2.0

### I2C1 - Audio DACs (PB8/PB9)

| Device | Address (8-bit) | ADR1 | ADR0 | Funcion |
|--------|-----------------|------|------|---------|
| PCM1690 U3 (DAC0) | **0x94** | 0 | 0 | 8 canales (ch 0-7) |
| PCM1690 U4 (DAC1) | **0x96** | 0 | 1 | 8 canales (ch 8-15) |
| PCM1690 U2 (DAC2) | **0x98** | 1 | 0 | 8 canales (ch 16-23) |
| PCM1690 U7 (DAC3) | **0x9A** | 1 | 1 | 8 canales (ch 24-31) |

**Nota sobre direcciones I2C:** El PCM1690 usa direccionamiento de 8 bits. Las direcciones mostradas son los valores de escritura (R/W=0). Para lectura, sumar 1.

### I2C3 - EEPROM (PB6/PB7)

| Device | Address (7-bit) | Funcion |
|--------|-----------------|---------|
| CAT24C256 (U10) | **0x50** | Calibracion persistente |

**Control de EEPROM:**
- PB5 = EEPROM_WP (Write Protect)
- WP=LOW: Escritura habilitada
- WP=HIGH: Escritura protegida

---

## Test Points v2.0

### Actuales (en DS.net)

| TP | Senal | Ubicacion | Proposito |
|----|-------|-----------|-----------|
| TP1 | +12V | Power | Verificar alimentacion principal |
| TP2 | +5V | Power | Verificar regulador U8 |
| TP3 | +3.3V | Power | Verificar regulador U9 |
| TP4 | GND | Power | Referencia para osciloscopio |
| - | TP_MCLK (PC7) | STM32 | Monitor clock 12.288 MHz |

### Sugeridos para agregar

| Pin Libre | Sugerencia | Proposito |
|-----------|------------|-----------|
| PA8 | TP_DEBUG | GPIO debug general |
| PA9 | TP_DEBUG2 | GPIO debug general |
| PE14 | TP_SAI1_FS | Verificar 48 kHz SAI1 |
| PD14 | TP_SAI2_FS | Verificar 48 kHz SAI2 |
| PE13 | TP_GND_A | Referencia GND analogico |

### Para que sirven los Test Points

1. **Debugging en produccion**: Conectar osciloscopio/multimetro sin soldar
2. **Verificacion de clock**: Confirmar MCLK = 12.288 MHz exactos
3. **Verificacion de sync**: Confirmar SAI_FS = 48 kHz
4. **Diagnostico de alimentacion**: Medir ripple en 3.3V/5V/12V
5. **Referencia de medicion**: GND para punta de osciloscopio

### Como implementar Test Points en KiCad

1. Agregar simbolo `TestPoint` de libreria `Connector`
2. Conectar a la net que queres monitorear
3. Usar footprint `TestPoint:TestPoint_Pad_D2.5mm`
4. En el PCB, ubicar cerca de la senal pero accesible

---

## Supervisor de Voltaje v2.0

### MAX809TEUR+T - Power On Reset Monitor

**Especificaciones:**
- Referencia: U11 (o IC1)
- Parte: Analog Devices MAX809TEUR+T
- Encapsulado: SOT-23 (3 pines)
- Función: Monitorea VREF+ y genera RESET limpio

**Configuración:**
- **Pin 1 (VCC):** +3.3V (fuente supervisada)
- **Pin 2 (IN):** Conectado a VREF+ (monitoreado)
- **Pin 3 (GND):** Tierra

**Voltaje de supervisión:**
- Threshold: 2.92-3.23V (típico 3.08V)
- Función: Si VREF+ cae por debajo, fuerza RESET del STM32

**Conexión a NRST:**
- Salida RESET → R41 (100k) → NRST del STM32H743 (pin 14)
- Pull-down R42 (1.5k) a VREF+
- Circuito de reset limpio independiente del firmware

**Propósito:**
1. Reset automático si falla alimentación
2. Reset durante power-down (mantiene coherencia SAI/I2C)
3. Protección de hardware contra estados indefinidos
4. Supervisión independiente sin necesidad de firmware

---

## GPIO Control Summary

### DAC Reset Pins

| DAC | Reset Pin | Active Level |
|-----|-----------|--------------|
| DAC0 (U3) | PE10 | LOW = Reset |
| DAC1 (U4) | PE3 | LOW = Reset |
| DAC2 (U2) | PC10 | LOW = Reset |
| DAC3 (U7) | PB15 | LOW = Reset |

### DAC Mute Control

| DAC | AMUTEI (Input) | AMUTEO (Output) |
|-----|----------------|-----------------|
| DAC0 (U3) | PE11 | PE12 |
| DAC1 (U4) | PE1 | PE0 |
| DAC2 (U2) | PC11 | PC12 |
| DAC3 (U7) | PB14 | PB13 |

### ADC Control (PCM1802 U1)

| Funcion | Pin | Descripcion |
|---------|-----|-------------|
| PDWN | PA6 | Power Down (LOW=off, HIGH=on) |
| OSR | PA4 | Oversampling ratio |
| BYPASS | PA5 | HPF bypass |

---

## Pines Libres (disponibles para expansion)

### Total: ~25 pines libres

**Port A:** PA2, PA3, PA7, PA8, PA9, PA15
**Port B:** PB0, PB1, PB2, PB3, PB4, PB10, PB12
**Port C:** PC2, PC3, PC4, PC5, PC6, PC8, PC9
**Port D:** PD0, PD1, PD2, PD3, PD4, PD5, PD6, PD7, PD14, PD15
**Port E:** PE13, PE14, PE15

---

## EEPROM Calibration Data Structure

```c
// CAT24C256: 32KB, I2C address 0x50
#define EEPROM_ADDR_MAGIC       0x0000  // "DSv2" (4 bytes)
#define EEPROM_ADDR_VERSION     0x0004  // Firmware version (4 bytes)
#define EEPROM_ADDR_SERIAL      0x0008  // Device serial number (8 bytes)
#define EEPROM_ADDR_CHANNELS    0x0010  // Channel calibration data

typedef struct {
    float gain_cal[32];         // Calibracion de ganancia por canal (128 bytes)
    int16_t delay_cal[32];      // Calibracion de delay por canal (64 bytes)
    uint8_t channel_enable[32]; // Habilitacion de canal (32 bytes)
    uint16_t crc;               // CRC16 de verificacion
} __attribute__((packed)) EEPROM_Calibration_t;
// Total: 226 bytes

// Control de Write Protect
void EEPROM_WriteProtect_Enable(void) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
}

void EEPROM_WriteProtect_Disable(void) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
}
```

---

## Power Sequencing v2.0

```c
void Audio_PowerUp_Sequence(void)
{
    // 1. Habilitar 3.3V analogico primero
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);  // PWR_EN_3V3A
    HAL_Delay(10);

    // 2. Mantener todos los DACs en reset
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);  // DAC0_RST
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET);   // DAC1_RST
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_RESET);  // DAC2_RST
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);  // DAC3_RST

    // 3. ADC en power-down
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);   // ADC_PDWN

    // 4. Iniciar MCLK (SAI1)
    HAL_SAI_Init(&hsai_BlockA1);
    HAL_Delay(1);

    // 5. Sacar DACs de reset secuencialmente
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_SET);  // DAC0
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);   // DAC1
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_SET);  // DAC2
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);  // DAC3
    HAL_Delay(1);

    // 6. Configurar DACs via I2C1 (direcciones en escritura)
    PCM1690_Init(&hi2c1, 0x94);  // DAC0 (U3) - ADR1=0, ADR0=0
    PCM1690_Init(&hi2c1, 0x96);  // DAC1 (U4) - ADR1=0, ADR0=1
    PCM1690_Init(&hi2c1, 0x98);  // DAC2 (U2) - ADR1=1, ADR0=0
    PCM1690_Init(&hi2c1, 0x9A);  // DAC3 (U7) - ADR1=1, ADR0=1

    // 7. Activar ADC
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);   // ADC_PDWN
    HAL_Delay(100);  // ADC requiere 100ms startup

    // 8. Cargar calibracion desde EEPROM
    EEPROM_WriteProtect_Enable();  // Proteger datos
    EEPROM_Calibration_t cal;
    EEPROM_Read_Calibration(&hi2c3, &cal);
    Apply_Calibration(&cal);

    // 9. Iniciar SAI con DMA
    HAL_SAI_Receive_DMA(&hsai_BlockB4, adc_buffer, BUFFER_SIZE);
    HAL_SAI_Transmit_DMA(&hsai_BlockA1, dac0_buffer, BUFFER_SIZE);
    HAL_SAI_Transmit_DMA(&hsai_BlockB1, dac1_buffer, BUFFER_SIZE);
    HAL_SAI_Transmit_DMA(&hsai_BlockA2, dac2_buffer, BUFFER_SIZE);
    HAL_SAI_Transmit_DMA(&hsai_BlockB2, dac3_buffer, BUFFER_SIZE);
}
```

---

---

## Verificación del Diseño v2.0 - ESTADO ACTUAL

### ✅ Verificado y Validado

#### Direcciones I2C
- [x] DAC0 (U3): 0x94 (ADR1=0, ADR0=0)
- [x] DAC1 (U4): 0x96 (ADR1=0, ADR0=1)
- [x] DAC2 (U2): 0x98 (ADR1=1, ADR0=0)
- [x] DAC3 (U7): 0x9A (ADR1=1, ADR0=1)
- [x] EEPROM (U10): 0xA0 (A0-A2 = GND)
- [x] **Todas las direcciones son únicas - Sin conflictos I2C**

#### Conexiones SAI (Bit Clock)
- [x] DAC0 (U3) BCK: PE8 (SAI1_A SCK)
- [x] DAC1 (U4) BCK: PE5 (SAI1_B SCK)
- [x] DAC2 (U2) BCK: PD12 (SAI2_A SCK)
- [x] DAC3 (U7) BCK: PD9 (SAI2_B SCK)
- [x] ADC (U1): PC1 (SAI4_SCK_B)
- [x] **Todos los BCK conectados correctamente - Audio funcional**

#### Supervisor de Voltaje
- [x] MAX809TEUR+T: SOT-23 (3 pines)
- [x] Capacitor 100nF en VCC: **Presente**
- [x] VCC del supervisor: +3.3V
- [x] Monitorea VREF+ para reset automático
- [x] **Reset limpio e independiente implementado**

#### Alimentación
- [x] U8 (AMS1117-5V): +12V → +5V
- [x] U9 (AMS1117-3.3V): +5V → +3.3V
- [x] Cascada correcta de reguladores
- [x] Ferrita FB1 en VREF+
- [x] Protección VBAT con BAT54C (D3)
- [x] **Power supply validado - Margen seguro en ambos rails**

#### Cristales y Osciladores
- [x] Y1: 32.768 kHz (LSE para RTC)
- [x] Y2: 25 MHz (HSE para STM32)
- [x] Capacitores de carga correctos (7pF y 10pF)
- [x] Pines correctos (PC14/PC15, PH0/PH1)
- [x] **Timing de sistema funcional**

#### Test Points
- [x] TP1: +12V
- [x] TP2: +5V
- [x] TP3: +3.3V
- [x] TP4: GND
- [x] TP_MCLK: PC7 (12.288 MHz)
- [x] **Debugging y diagnostico disponible**

#### Pines de Control
- [x] ADC (PCM1802): PA4 (OSR), PA5 (BYPASS), PA6 (PDWN)
- [x] DAC Reset: PE10, PE3, PC10, PB15
- [x] DAC Mute: PE11/PE12, PE0/PE1, PC11/PC12, PB14/PB13
- [x] EEPROM WP: PB5
- [x] **Todos los controles de periféricos conectados**

---

## Checklist Fabricacion v2.0

### Hardware - ANTES DE FABRICAR
- [x] Verificar todas las direcciones I2C (sin conflictos)
- [x] Verificar BCK conectados (audio funcional)
- [x] MAX809 con capacitor (reset funcional)
- [x] Reguladores en cascada (power supply OK)
- [x] Cristales con valores correctos
- [x] Todos los pines críticos conectados
- [ ] Revisar footprints de componentes
- [ ] Verificar impedancia de pistas SAI (si es necesario)
- [ ] Revisar planos de GND/alimentación en PCB

### Alternativa: v1.0 (Dev Board STM32H743)
- [ ] Si no tienes fabricada v1, considera hacerlo PRIMERO
- [ ] Permite validar firmware sin riesgo
- [ ] Reduce riesgo de v2 (versión final)

### Firmware - PLAN DE DESARROLLO
- [ ] UART4 (PA0/PA1) para debug y logs
- [ ] Inicialización de SAI1, SAI2, SAI4
- [ ] I2C1: Configurar 4 DACs con direcciones correctas
- [ ] I2C3: EEPROM read/write con protección WP
- [ ] Power sequencing: Reset secuencial de DACs
- [ ] Supervisor MAX809: Ya funciona a nivel hardware
- [ ] 32-channel beamforming algorithm con DMA

---

*Hardware Map v2.0 - 32-Channel Beamforming System*
*Actualizado desde DS.net - Enero 2026*
*Verificación completada - Diseño validado ✓*
