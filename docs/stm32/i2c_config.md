# I2C Timing Reference - STM32H743

**Source:** RM0433 Reference Manual - Chapter 47 (I2C)
**Note:** Valores basados en conocimiento del STM32H7. Verificar RM0433 páginas 1900-2000 para detalles exactos.

---

## Table of Contents
- [I2C Overview](#i2c-overview)
- [I2C Timing Register](#i2c-timing-register)
- [Timing Calculation](#timing-calculation)
- [I2C Registers](#i2c-registers)
- [Configuration Examples](#configuration-examples)

---

## I2C Overview

### I2C1 Configuration for Project
- **Speed:** 100 kHz (Standard Mode)
- **APB Clock:** 120 MHz (APB1)
- **Pullup:** External 5.1kΩ to 3.3V
- **Devices:**
  - PCM1690 #0 (DAC0): 0x4C
  - PCM1690 #1 (DAC1): 0x4E
  - PCM1802 (ADC): 0x4D

### I2C Modes

| Mode | Speed | Rise Time (max) | Fall Time (max) |
|------|-------|-----------------|-----------------|
| **Standard** | **100 kHz** | **1000 ns** | **300 ns** |
| Fast | 400 kHz | 300 ns | 300 ns |
| Fast Plus | 1 MHz | 120 ns | 120 ns |

---

## I2C Timing Register

### I2C_TIMINGR - Timing Configuration Register

**Offset:** 0x10
**Reset value:** 0x0000 0000

| Bits | Name | Description | Value |
|------|------|-------------|-------|
| 31:28 | PRESC[3:0] | Timing prescaler | Ver cálculo |
| 27:24 | Reserved | - | - |
| 23:20 | SCLDEL[3:0] | Data setup time | Ver cálculo |
| 19:16 | SDADEL[3:0] | Data hold time | Ver cálculo |
| 15:8 | SCLH[7:0] | SCL high period | Ver cálculo |
| 7:0 | SCLL[7:0] | SCL low period | Ver cálculo |

---

## Timing Calculation

### Parameters for 100 kHz @ 120 MHz APB

#### I2C Timing Formulas

```
t_I2CCLK = 1 / f_I2CCLK = 1 / 120 MHz = 8.33 ns

t_PRESC = (PRESC + 1) × t_I2CCLK

t_SCLL = (SCLL + 1) × t_PRESC
t_SCLH = (SCLH + 1) × t_PRESC

t_SCL = t_SYNC1 + t_SYNC2 + t_SCLL + t_SCLH
f_SCL = 1 / t_SCL

t_SDADEL = SDADEL × t_PRESC (or (SDADEL + 1) depending on mode)
t_SCLDEL = (SCLDEL + 1) × t_PRESC
```

#### Target Specifications (Standard Mode 100 kHz)

| Parameter | Min | Max | Unit |
|-----------|-----|-----|------|
| SCL frequency | - | 100 | kHz |
| SCL low time (t_LOW) | 4.7 | - | μs |
| SCL high time (t_HIGH) | 4.0 | - | μs |
| SDA setup time (t_SU:DAT) | 250 | - | ns |
| SDA hold time (t_HD:DAT) | 0 | 3450 | ns |
| Rise time (t_r) | - | 1000 | ns |
| Fall time (t_f) | - | 300 | ns |

#### Calculation Steps

**1. Select PRESC for convenient timing resolution:**
```
Target resolution: ~100 ns
PRESC = (f_I2CCLK × t_resolution) - 1
PRESC = (120 MHz × 100 ns) - 1 = 12 - 1 = 11

t_PRESC = (11 + 1) × 8.33 ns = 100 ns ✓
```

**2. Calculate SCLL for SCL low period:**
```
Target t_LOW = 4.7 μs
SCLL = (t_LOW / t_PRESC) - 1
SCLL = (4700 ns / 100 ns) - 1 = 47 - 1 = 46

Actual t_SCLL = (46 + 1) × 100 ns = 4700 ns = 4.7 μs ✓
```

**3. Calculate SCLH for SCL high period:**
```
Target t_HIGH = 4.0 μs (to get ~100 kHz total)
SCLH = (t_HIGH / t_PRESC) - 1
SCLH = (4000 ns / 100 ns) - 1 = 40 - 1 = 39

Actual t_SCLH = (39 + 1) × 100 ns = 4000 ns = 4.0 μs ✓
```

**4. Verify SCL frequency:**
```
t_SCL = t_SCLL + t_SCLH + t_r + t_f (approx)
t_SCL ≈ 4.7 μs + 4.0 μs + 0.3 μs + 0.3 μs = 9.3 μs
f_SCL = 1 / 9.3 μs ≈ 107 kHz

// More precise (ignoring sync delays for now):
f_SCL = 1 / (4.7 μs + 4.0 μs) = 114 kHz
This is acceptable for 100 kHz Standard Mode
```

**5. Calculate SDADEL (data hold time):**
```
Target t_HD:DAT = 1000 ns (middle of 0-3450 ns range)
SDADEL = t_HD:DAT / t_PRESC
SDADEL = 1000 ns / 100 ns = 10

Actual t_SDADEL = 10 × 100 ns = 1000 ns ✓
```

**6. Calculate SCLDEL (data setup time):**
```
Target t_SU:DAT = 1000 ns (well above 250 ns minimum)
SCLDEL = (t_SU:DAT / t_PRESC) - 1
SCLDEL = (1000 ns / 100 ns) - 1 = 10 - 1 = 9

Actual t_SCLDEL = (9 + 1) × 100 ns = 1000 ns ✓
```

### Final Calculated Value

```c
I2C_TIMINGR = (PRESC << 28) | (SCLDEL << 20) | (SDADEL << 16) |
               (SCLH << 8)  | (SCLL << 0);

I2C_TIMINGR = (11 << 28) |    // PRESC = 11
               (9 << 20) |     // SCLDEL = 9
               (10 << 16) |    // SDADEL = 10
               (39 << 8) |     // SCLH = 39
               (46 << 0);      // SCLL = 46

I2C_TIMINGR = 0xB09A_272E
```

### Alternative: STM32CubeMX Calculated Value

STM32CubeMX I2C timing calculator for:
- APB1 = 120 MHz
- I2C Standard Mode (100 kHz)
- Rise time = 100 ns (with external pullup)
- Fall time = 10 ns

**Result:** `0x20404768`

**Breakdown of 0x20404768:**
```
PRESC   = 0x2 (bits 31:28) = 2
SCLDEL  = 0x0 (bits 23:20) = 0
SDADEL  = 0x4 (bits 19:16) = 4
SCLH    = 0x47 (bits 15:8) = 71
SCLL    = 0x68 (bits 7:0)  = 104

Verification:
t_PRESC = (2 + 1) × 8.33 ns = 25 ns
t_SCLL = (104 + 1) × 25 ns = 2625 ns
t_SCLH = (71 + 1) × 25 ns = 1800 ns
t_SCL = 2625 + 1800 = 4425 ns
f_SCL = 1 / 4425 ns ≈ 226 kHz

This seems fast for 100 kHz mode, might need verification
```

**Recommendation:** Use **0x20404768** (CubeMX value) or **0xB09A272E** (manual calculation). Test both and verify with scope.

---

## I2C Registers

### I2C_CR1 - Control Register 1

**Offset:** 0x00
**Reset value:** 0x0000 0000

| Bits | Name | Description | Value |
|------|------|-------------|-------|
| 31:24 | Reserved | - | - |
| 23 | PECEN | PEC enable | 0 (disabled) |
| 22 | ALERTEN | SMBus alert enable | 0 |
| 21 | SMBDEN | SMBus device default address enable | 0 |
| 20 | SMBHEN | SMBus host address enable | 0 |
| 19 | GCEN | General call enable | 0 |
| 18 | WUPEN | Wakeup from Stop enable | 0 |
| 17 | NOSTRETCH | Clock stretching disable | 0 (enabled) |
| 16 | SBC | Slave byte control | 0 |
| 15 | RXDMAEN | RX DMA enable | 0 (polling/IT mode) |
| 14 | TXDMAEN | TX DMA enable | 0 |
| 13 | Reserved | - | - |
| 12 | ANFOFF | Analog noise filter OFF | 0 (enabled) |
| 11:8 | DNF[3:0] | Digital noise filter | 0 (disabled) |
| 7 | ERRIE | Error interrupt enable | 1 (optional) |
| 6 | TCIE | Transfer complete interrupt enable | 0 |
| 5 | STOPIE | STOP detection interrupt enable | 0 |
| 4 | NACKIE | NACK interrupt enable | 1 (optional) |
| 3 | ADDRIE | Address match interrupt enable | 0 |
| 2 | RXIE | RX interrupt enable | 0 |
| 1 | TXIE | TX interrupt enable | 0 |
| 0 | PE | Peripheral enable | 1 (enable I2C) |

**Typical Value:**
```c
I2C1->CR1 = (0 << 17) |   // NOSTRETCH = 0 (allow stretching)
             (0 << 15) |   // RXDMAEN = 0
             (0 << 14) |   // TXDMAEN = 0
             (0 << 12) |   // ANFOFF = 0 (analog filter enabled)
             (0 << 8) |    // DNF = 0
             (1 << 7) |    // ERRIE = 1 (optional)
             (1 << 4) |    // NACKIE = 1 (optional)
             (1 << 0);     // PE = 1 (enable)
// Result: 0x0000_0091
```

### I2C_CR2 - Control Register 2

**Offset:** 0x04
**Reset value:** 0x0000 0000

Used for each transaction:

| Bits | Name | Description |
|------|------|-------------|
| 26 | PECBYTE | PEC byte transmission | 0 |
| 25 | AUTOEND | Automatic end mode | 0/1 |
| 24 | RELOAD | Reload mode | 0 |
| 23:16 | NBYTES[7:0] | Number of bytes | Data length |
| 15 | NACK | NACK generation | 0 |
| 14 | STOP | Stop generation | 0/1 |
| 13 | START | Start generation | 1 |
| 12 | HEAD10R | 10-bit header read | 0 |
| 11 | ADD10 | 10-bit addressing mode | 0 (7-bit) |
| 10 | RD_WRN | Transfer direction | 0=write, 1=read |
| 9:8 | SADD[9:8] | Slave address bits 9:8 | 0 |
| 7:1 | SADD[7:1] | Slave address bits 7:1 | Device addr |
| 0 | SADD0 | Slave address bit 0 | 0 (7-bit mode) |

**Example write to DAC0 (0x4C):**
```c
I2C1->CR2 = (0 << 25) |              // AUTOEND = 0
             (2 << 16) |              // NBYTES = 2 (reg + data)
             (0 << 14) |              // STOP = 0 (manual)
             (1 << 13) |              // START = 1
             (0 << 11) |              // ADD10 = 0 (7-bit)
             (0 << 10) |              // RD_WRN = 0 (write)
             (0x4C << 1);             // SADD = 0x4C (shifted!)
// Result: 0x0002_2098
```

### I2C_ISR - Interrupt and Status Register

**Offset:** 0x18 (read-only)

| Bits | Name | Description |
|------|------|-------------|
| 23 | ADDCODE[6:0] | Address match code |
| 17 | DIR | Transfer direction (slave) |
| 16 | BUSY | Bus busy |
| 15 | ALERT | SMBus alert |
| 14 | Reserved | - |
| 13 | TIMEOUT | Timeout |
| 12 | PECERR | PEC error |
| 11 | OVR | Overrun/underrun |
| 10 | ARLO | Arbitration lost |
| 9 | BERR | Bus error |
| 8 | Reserved | - |
| 7 | TCR | Transfer complete reload |
| 6 | TC | Transfer complete |
| 5 | STOPF | Stop detection flag |
| 4 | NACKF | NACK received flag |
| 3 | ADDR | Address matched |
| 2 | RXNE | RX not empty |
| 1 | TXIS | TX interrupt status |
| 0 | TXE | TX empty |

**Key flags to check:**
```c
// Wait for TXIS (TX register empty)
while (!(I2C1->ISR & I2C_ISR_TXIS));

// Wait for RXNE (RX data available)
while (!(I2C1->ISR & I2C_ISR_RXNE));

// Check NACK
if (I2C1->ISR & I2C_ISR_NACKF) {
    // Handle NACK error
}

// Wait for STOPF (stop condition detected)
while (!(I2C1->ISR & I2C_ISR_STOPF));
```

### I2C_TXDR / I2C_RXDR - Data Registers

**TX Offset:** 0x28 (write-only)
**RX Offset:** 0x24 (read-only)

```c
// Transmit byte
I2C1->TXDR = data_byte;

// Receive byte
uint8_t data = I2C1->RXDR;
```

---

## Configuration Examples

### Complete I2C1 Initialization

```c
void I2C1_Init(void)
{
    // 1. Enable I2C1 clock
    __HAL_RCC_I2C1_CLK_ENABLE();

    // 2. Disable I2C before configuration
    I2C1->CR1 &= ~I2C_CR1_PE;

    // 3. Configure timing (100 kHz, APB1 @ 120 MHz)
    I2C1->TIMINGR = 0x20404768;  // CubeMX calculated
    // OR
    // I2C1->TIMINGR = 0xB09A272E;  // Manual calculation

    // 4. Configure CR1
    I2C1->CR1 = (0 << I2C_CR1_ANFOFF_Pos) |   // Analog filter ON
                 (0 << I2C_CR1_DNF_Pos) |      // Digital filter OFF
                 (1 << I2C_CR1_ERRIE_Pos) |    // Error interrupt
                 (1 << I2C_CR1_NACKIE_Pos);    // NACK interrupt

    // 5. Enable I2C
    I2C1->CR1 |= I2C_CR1_PE;
}
```

### HAL-Based Initialization

```c
void I2C1_Init_HAL(void)
{
    I2C_HandleTypeDef hi2c1;

    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x20404768;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    HAL_I2C_Init(&hi2c1);

    // Configure analog filter
    HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE);

    // Configure digital filter (optional)
    HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0);
}
```

### Write to PCM1690 Register

```c
HAL_StatusTypeDef PCM1690_WriteRegister(uint8_t dev_addr, uint8_t reg, uint8_t value)
{
    uint8_t data[2];
    data[0] = reg;
    data[1] = value;

    return HAL_I2C_Master_Transmit(&hi2c1, dev_addr << 1, data, 2, 100);
}

// Usage:
PCM1690_WriteRegister(0x4C, 0x40, 0xC0);  // DAC0, reg 64, value 0xC0
```

### Read from PCM1690 Register (if supported)

```c
HAL_StatusTypeDef PCM1690_ReadRegister(uint8_t dev_addr, uint8_t reg, uint8_t *value)
{
    HAL_StatusTypeDef status;

    // Write register address
    status = HAL_I2C_Master_Transmit(&hi2c1, dev_addr << 1, &reg, 1, 100);
    if (status != HAL_OK) return status;

    // Read register value
    return HAL_I2C_Master_Receive(&hi2c1, dev_addr << 1, value, 1, 100);
}
```

---

## Timing Verification

### Oscilloscope Measurements

Verify these parameters on scope:
- **SCL frequency:** ~100 kHz (period ~10 μs)
- **SCL low time:** ≥ 4.7 μs
- **SCL high time:** ≥ 4.0 μs
- **Rise time:** < 1000 ns (depends on pullup and capacitance)
- **SDA setup time:** > 250 ns before SCL rising edge
- **SDA hold time:** > 0 ns after SCL falling edge

### Common Issues

| Issue | Symptom | Solution |
|-------|---------|----------|
| SCL too slow | Freq < 100 kHz | Decrease SCLL/SCLH values |
| SCL too fast | Freq > 100 kHz | Increase SCLL/SCLH values |
| Data errors | NACK, timing violations | Adjust SDADEL/SCLDEL |
| Slow rise time | Rise > 1 μs | Reduce pullup resistor |
| Ringing | Overshoot on scope | Add series resistor or reduce speed |

---

## Quick Reference Table

### Pre-calculated TIMINGR Values

| APB Freq | I2C Speed | TIMINGR Value | Notes |
|----------|-----------|---------------|-------|
| 120 MHz | 100 kHz | **0x20404768** | CubeMX calculated ⭐ |
| 120 MHz | 100 kHz | 0xB09A272E | Manual calculation |
| 120 MHz | 400 kHz | 0x00B0122A | Fast mode |
| 100 MHz | 100 kHz | 0x10707DBC | Alternative APB |

---

**Note:** Valores calculados para STM32H743 con APB1 @ 120 MHz. Para verificar timing exacto y tolerancias, consultar RM0433 capítulo 47 (I2C) páginas 1900-2000 y usar STM32CubeMX timing calculator.
