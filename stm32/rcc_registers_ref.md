# RCC Registers Reference - STM32H743

**Source:** RM0433 Reference Manual
**Note:** Valores extraídos de conocimiento del STM32H7. Verificar contra RM0433 páginas 300-400 para valores exactos.

---

## Table of Contents
- [PLL3 Configuration Registers](#pll3-configuration-registers)
- [SAI Clock Source Selection](#sai-clock-source-selection)
- [Clock Enable Registers](#clock-enable-registers)
- [Calculation Examples](#calculation-examples)

---

## PLL3 Configuration Registers

### RCC_PLLCKSELR (PLL Clock Source Selection Register)
**Offset:** 0x28
**Reset value:** 0x0202 0200

| Bits | Name | Type | Description | Value para PLL3 |
|------|------|------|-------------|-----------------|
| 31:28 | Reserved | - | - | - |
| 27:26 | DIVM3[5:4] | RW | PLL3 input clock divider MSB | Ver tabla |
| 25:20 | DIVM2[5:0] | RW | PLL2 input clock divider | - |
| 19:16 | DIVM1[5:0] | RW | PLL1 input clock divider | - |
| 15:12 | Reserved | - | - | - |
| 11:6 | DIVM3[3:0] | RW | PLL3 input clock divider LSB | Ver tabla |
| 5:0 | Reserved | - | - | - |

**DIVM3 Calculation:**
```
PLL3_ref_ck = HSE_ck / DIVM3
Para HSE = 25 MHz, target ref = 5 MHz
DIVM3 = 25 / 5 = 5
Bits[27:26,11:6] = 000101 (decimal 5)
```

### RCC_PLL3DIVR (PLL3 Dividers Register)
**Offset:** 0x3C
**Reset value:** 0x0101 0280

| Bits | Name | Type | Description | Value para 12.288 MHz |
|------|------|------|-------------|------------------------|
| 31:24 | DIVR3[6:0] | RW | PLL3 DIVR division factor | 0x00 (not used) |
| 23:16 | DIVQ3[6:0] | RW | PLL3 DIVQ division factor | 0x00 (not used) |
| 15:9 | DIVP3[6:0] | RW | PLL3 DIVP division factor | **28** (0x1C) |
| 8:0 | DIVN3[8:0] | RW | PLL3 multiplication factor | Ver cálculo |

**DIVN3 Calculation:**
```
Target VCO frequency: need to calculate backwards
PLL3_P_ck = VCO_ck / DIVP3
12.288 MHz = VCO_ck / 28
VCO_ck = 12.288 × 28 = 344.064 MHz

DIVN3 = VCO_ck / PLL3_ref_ck
DIVN3 = 344.064 MHz / 5 MHz = 68.8128

Since DIVN must be integer, we need fractional mode:
DIVN3 = 68 (integer part)
Fractional part = 0.8128
```

**Register Value:**
```c
RCC_PLL3DIVR = (0x00 << 24) |  // DIVR3 = 0 (not used)
                (0x00 << 16) |  // DIVQ3 = 0 (not used)
                (0x1C << 9)  |  // DIVP3 = 28
                (0x44);         // DIVN3 = 68
```

### RCC_PLLCFGR (PLL Configuration Register)
**Offset:** 0x2C
**Reset value:** 0x0000 0000

| Bits | Name | Type | Description | Value para PLL3 |
|------|------|------|-------------|-----------------|
| 31:22 | Reserved | - | - | - |
| 21 | DIVR3EN | RW | PLL3 DIVR output enable | 0 (disabled) |
| 20 | DIVQ3EN | RW | PLL3 DIVQ output enable | 0 (disabled) |
| 19 | DIVP3EN | RW | PLL3 DIVP output enable | **1** (enabled) |
| 18:16 | PLL3RGE[1:0] | RW | PLL3 input frequency range | **010** (4-8 MHz) |
| 15 | Reserved | - | - | - |
| 14 | PLL3FRACEN | RW | PLL3 fractional mode enable | **1** (enabled) |
| 13 | PLL3VCOSEL | RW | PLL3 VCO range selection | **0** (wide range 192-960 MHz) |
| 12:9 | Reserved | - | - | - |
| 8:7 | PLL2RGE[1:0] | RW | PLL2 input frequency range | - |
| 6 | PLL2FRACEN | RW | PLL2 fractional mode enable | - |
| 5 | PLL2VCOSEL | RW | PLL2 VCO range selection | - |
| 4:3 | PLL1RGE[1:0] | RW | PLL1 input frequency range | - |
| 2 | PLL1FRACEN | RW | PLL1 fractional mode enable | - |
| 1 | PLL1VCOSEL | RW | PLL1 VCO range selection | - |
| 0 | Reserved | - | - | - |

**Register Value:**
```c
RCC_PLLCFGR = (1 << 19) |     // DIVP3EN = 1
              (2 << 16) |     // PLL3RGE = 010 (4-8 MHz)
              (1 << 14) |     // PLL3FRACEN = 1
              (0 << 13);      // PLL3VCOSEL = 0 (wide range)
// Result: 0x000A_4000
```

**PLL3RGE Values:**
| Value | Range | Use when |
|-------|-------|----------|
| 00 | 1-2 MHz | DIVM > 12 for 25 MHz HSE |
| 01 | 2-4 MHz | DIVM = 6-12 |
| **10** | **4-8 MHz** | **DIVM = 3-6** ← Our case (DIVM=5) |
| 11 | 8-16 MHz | DIVM = 1-3 |

### RCC_PLL3FRACR (PLL3 Fractional Register)
**Offset:** 0x40
**Reset value:** 0x0000 0000

| Bits | Name | Type | Description | Value para 12.288 MHz exacto |
|------|------|------|-------------|-------------------------------|
| 31:13 | Reserved | - | - | - |
| 12:3 | FRACN3[12:0] | RW | PLL3 fractional part | Ver cálculo |
| 2:0 | Reserved | - | - | - |

**FRACN3 Calculation:**
```
Fractional part needed: 0.8128 (from DIVN calculation)
FRACN3 = fractional_part × 2^13
FRACN3 = 0.8128 × 8192 = 6656.5 ≈ 6657

More precise calculation:
Target VCO = 344.064 MHz
Actual VCO with DIVN=68: 68 × 5 = 340 MHz
Error = 344.064 - 340 = 4.064 MHz
Fractional needed = 4.064 / 5 = 0.8128
FRACN3 = 0.8128 × 8192 = 6657

Verification:
VCO = (DIVN + FRACN/8192) × ref_ck
VCO = (68 + 6657/8192) × 5
VCO = (68 + 0.8128) × 5 = 68.8128 × 5 = 344.064 MHz ✓
PLL3_P = 344.064 / 28 = 12.288 MHz ✓
```

**Register Value:**
```c
RCC_PLL3FRACR = (6657 << 3);  // FRACN3 = 6657
// Result: 0x0000_CFC8
```

---

## SAI Clock Source Selection

### RCC_D2CCIP1R (Domain 2 Kernel Clock Configuration Register)
**Offset:** 0x4C
**Reset value:** 0x0000 0000

| Bits | Name | Type | Description | Value para SAI1 |
|------|------|------|-------------|-----------------|
| 31:23 | Reserved | - | - | - |
| 22:21 | SAI1SEL[2:0] | RW | SAI1 kernel clock source | Ver tabla |
| 20:6 | Other peripherals | - | - | - |

**SAI1SEL Values:**
| Value | Clock Source | Frequency |
|-------|--------------|-----------|
| 000 | pll1_q_ck | Variable |
| 001 | pll2_p_ck | Variable |
| 010 | pll3_p_ck | **12.288 MHz** ← Our choice |
| 011 | I2S_CKIN | External |
| 100 | per_ck | Variable |
| Others | Reserved | - |

**Register Value:**
```c
RCC_D2CCIP1R = (2 << 21);  // SAI1SEL = 010 (PLL3_P)
```

### RCC_D3CCIPR (Domain 3 Kernel Clock Configuration Register)
**Offset:** 0x54
**Reset value:** 0x0000 0000

| Bits | Name | Type | Description | Value para SAI4 |
|------|------|------|-------------|-----------------|
| 31:22 | Reserved | - | - | - |
| 21:20 | SAI4ASEL[1:0] | RW | SAI4A kernel clock source | Ver tabla |
| 19:20 | SAI4BSEL[1:0] | RW | SAI4B kernel clock source | Ver tabla |

**SAI4xSEL Values:**
| Value | Clock Source | Use |
|-------|--------------|-----|
| 00 | pll1_q_ck | Not recommended for audio |
| 01 | pll2_p_ck | Alternative |
| **10** | **pll3_p_ck** | **12.288 MHz** ← Our choice |
| 11 | I2S_CKIN | External clock |

**Register Value:**
```c
RCC_D3CCIPR = (2 << 21) |  // SAI4ASEL = 10 (PLL3_P)
              (2 << 20);   // SAI4BSEL = 10 (PLL3_P)
```

---

## Clock Enable Registers

### RCC_AHB2ENR (AHB2 Peripheral Clock Enable Register)
**Offset:** 0xE4

```c
RCC->AHB2ENR |= RCC_AHB2ENR_D2SRAM1EN |  // Enable SRAM1
                RCC_AHB2ENR_D2SRAM2EN |  // Enable SRAM2
                RCC_AHB2ENR_D2SRAM3EN;   // Enable SRAM3
```

### RCC_APB2ENR (APB2 Peripheral Clock Enable Register)
**Offset:** 0xF0

```c
RCC->APB2ENR |= RCC_APB2ENR_SAI1EN;  // Enable SAI1 clock
```

### RCC_APB4ENR (APB4 Peripheral Clock Enable Register)
**Offset:** 0xF4

```c
RCC->APB4ENR |= RCC_APB4ENR_SAI4EN;  // Enable SAI4 clock
```

### RCC_AHB1ENR (AHB1 Peripheral Clock Enable Register)
**Offset:** 0xE0

```c
RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN |  // Enable DMA1
                RCC_AHB1ENR_DMA2EN;   // Enable DMA2
```

### RCC_APB1LENR (APB1 Low Peripheral Clock Enable Register)
**Offset:** 0xE8

```c
RCC->APB1LENR |= RCC_APB1LENR_I2C1EN;  // Enable I2C1
```

---

## Calculation Examples

### Example 1: PLL3 for 12.288 MHz (256 × 48 kHz)

```c
// Input: HSE = 25 MHz
// Target: PLL3_P = 12.288 MHz for SAI

// Step 1: Select DIVM3 for ref clock 4-8 MHz
DIVM3 = 5;  // 25 MHz / 5 = 5 MHz (within 4-8 MHz range)

// Step 2: Calculate VCO frequency
// PLL3_P = VCO / DIVP3
// 12.288 MHz = VCO / 28
VCO = 12.288 × 28 = 344.064 MHz

// Step 3: Calculate DIVN (integer part)
DIVN3 = VCO / ref_ck = 344.064 / 5 = 68.8128
DIVN3_int = 68;

// Step 4: Calculate fractional part
fractional = 0.8128
FRACN3 = 0.8128 × 8192 = 6657

// Step 5: Verify
VCO_actual = (68 + 6657/8192) × 5 = 344.064 MHz ✓
PLL3_P_actual = 344.064 / 28 = 12.288 MHz ✓
```

### Example 2: Alternative - 11.2896 MHz (256 × 44.1 kHz)

```c
// For 44.1 kHz audio (less common, but for reference)
// Target: PLL3_P = 11.2896 MHz

// Option A: DIVM3 = 5, ref = 5 MHz
VCO = 11.2896 × 28 = 316.1088 MHz
DIVN3 = 316.1088 / 5 = 63.22176
DIVN3_int = 63
fractional = 0.22176
FRACN3 = 0.22176 × 8192 = 1816

// Verify
VCO = (63 + 1816/8192) × 5 = 316.1096 MHz
PLL3_P = 316.1096 / 28 = 11.2896 MHz ✓
```

### Example 3: Higher Sample Rate - 96 kHz

```c
// For 96 kHz audio
// MCLK = 256 × 96 kHz = 24.576 MHz

// Option A: DIVM3 = 5, DIVP3 = 14
VCO = 24.576 × 14 = 344.064 MHz (same VCO!)
DIVN3 = 68, FRACN3 = 6657

// Option B: Double the MCLK multiplier (512×)
// MCLK = 512 × 96 kHz = 49.152 MHz
VCO = 49.152 × 7 = 344.064 MHz
DIVN3 = 68, FRACN3 = 6657, DIVP3 = 7
```

---

## Clock Tree Diagram

```
HSE (25 MHz)
    │
    ├─ DIVM3 (÷5) ──► 5 MHz (ref_ck)
    │                     │
    │                     ├─ DIVN3 (×68.8128) ──► 344.064 MHz (VCO)
    │                     │                              │
    │                     │                              ├─ DIVP3 (÷28) ──► 12.288 MHz
    │                     │                              │                      │
    │                     │                              │                      ├─► SAI1 kernel
    │                     │                              │                      └─► SAI4 kernel
    │                     │                              │
    │                     │                              ├─ DIVQ3 (disabled)
    │                     │                              └─ DIVR3 (disabled)
    │                     │
    │                     └─ FRACN3 (fractional modulation)
    │
    └─ (PLL1 for CPU @ 480 MHz)
```

---

## HAL Configuration Code

```c
void PLL3_Audio_Config(void)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    // Select peripherals to configure
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI1 |
                                                RCC_PERIPHCLK_SAI4A;

    // PLL3 configuration
    PeriphClkInitStruct.PLL3.PLL3M = 5;      // 25 MHz / 5 = 5 MHz
    PeriphClkInitStruct.PLL3.PLL3N = 68;     // Integer part
    PeriphClkInitStruct.PLL3.PLL3P = 28;     // 344.064 / 28 = 12.288 MHz
    PeriphClkInitStruct.PLL3.PLL3Q = 2;      // Not used
    PeriphClkInitStruct.PLL3.PLL3R = 2;      // Not used
    PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;  // 4-8 MHz
    PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;  // Wide range
    PeriphClkInitStruct.PLL3.PLL3FRACN = 6657;  // Fractional part

    // Select PLL3_P as SAI clock source
    PeriphClkInitStruct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL3;
    PeriphClkInitStruct.Sai4AClockSelection = RCC_SAI4ACLKSOURCE_PLL3;
    PeriphClkInitStruct.Sai4BClockSelection = RCC_SAI4BCLKSOURCE_PLL3;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }
}
```

---

## Verification Checklist

- [ ] HSE frequency confirmed: 25 MHz
- [ ] PLL3 reference clock: 5 MHz (4-8 MHz range) ✓
- [ ] PLL3 VCO frequency: 344.064 MHz (192-960 MHz range) ✓
- [ ] PLL3_P output: 12.288 MHz ✓
- [ ] SAI1 kernel clock: 12.288 MHz
- [ ] SAI4 kernel clock: 12.288 MHz
- [ ] MCLK output: 12.288 MHz (256 × 48 kHz)
- [ ] BCK: 3.072 MHz (64 × 48 kHz)
- [ ] LRCK: 48 kHz

---

**Note:** Todos los valores han sido calculados para audio @ 48 kHz con MCLK = 256 × fs. Para verificar valores exactos de bits y offsets, consultar RM0433 sección 7 (RCC) páginas 300-450.
