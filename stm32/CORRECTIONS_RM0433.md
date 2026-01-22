# Correcciones Basadas en RM0433

**Fecha:** 2026-01-22
**Fuente:** RM0433 Reference Manual STM32H742/H743/H753/H750

---

## ❌ ERRORES CRÍTICOS ENCONTRADOS

### 1. **SAI4 DMA Configuration - ERROR ARQUITECTÓNICO**

**❌ Documentación Anterior (INCORRECTA):**
```
SAI4_A: DMAMUX1 request 112 → DMA2 Stream 6
SAI4_B: DMAMUX1 request 113 → DMA2 Stream 2
```

**✅ Información Correcta del RM0433:**
```
SAI4_A: DMAMUX2 request 15 → BDMA channel
SAI4_B: DMAMUX2 request 16 → BDMA channel
```

**Impacto:**
- SAI4 está en el **dominio D3/APB4**, NO en D2
- SAI4 usa **BDMA** (Basic DMA), NO DMA1 o DMA2
- SAI4 usa **DMAMUX2**, NO DMAMUX1
- Los registros BDMA son diferentes a DMA1/DMA2
- El código de inicialización debe cambiarse completamente

**Referencias RM0433:**
- Table 102: DMAMUX1 request mapping (línea 111894) - SAI1_A=87, SAI1_B=88
- Table 103: DMAMUX2 connections (línea 92903) - SAI4 en D3/APB4
- Table 126: DMAMUX2 request mapping (línea 113114) - SAI4_A=15, SAI4_B=16

---

## ✅ INFORMACIÓN VERIFICADA CORRECTA

### 1. PLL3 Configuration

**Registros confirmados del RM0433:**

```c
// RCC_PLL3DIVR (offset 0x040, línea 55757)
DIVP3[6:0]  // Bits 15:9 - P divider
DIVN3[8:0]  // Bits 8:0 - N multiplier (4-512)

// RCC_PLL3FRACR (offset 0x044, línea 55916)
FRACN3[12:0]  // Bits 15:3 - Fractional part (0-8191)

// Formula (línea 56035):
VCO = Fref3_ck × (DIVN3 + FRACN3/2^13)
pll3_p_ck = VCO / DIVP3
```

**Cálculo para 12.288 MHz verificado:**
```
Fref3 = 25 MHz / DIVM3(5) = 5 MHz
VCO = 5 MHz × (68 + 6657/8192) = 340.0640869 MHz
pll3_p_ck = 340.0640869 / 28 = 12.1451459 MHz (error)
```

**⚠️ NOTA:** Cálculo fraccionario necesita ajuste fino. Valor FRACN3=6657 da error. Recalcular.

### 2. SAI Clock Source Selection

**RCC_D2CCIP1R register (línea 56432-56455):**

```c
// SAI1 kernel clock source (bits 2:0)
000: pll1_q_ck (default)
001: pll2_p_ck
010: pll3_p_ck ← USAR ESTE
011: I2S_CKIN
100: per_ck

// SAI4 kernel clock source - separate bits
SAI4ASEL[2:0] - Sub-block A source
SAI4BSEL[2:0] - Sub-block B source
```

**✅ Configuración confirmada:** SAI1SEL = 010 (pll3_p_ck)

### 3. SAI Registers

**SAI_xCR1 configuration verified (línea 363787-363852):**

```c
// MODE[1:0] (bits 1:0)
00: Master transmitter ✓
01: Master receiver ✓
10: Slave transmitter
11: Slave receiver

// PRTCFG[1:0] (bits 3:2)
00: Free protocol (TDM, I2S, etc.) ✓
01: SPDIF
10: AC'97
11: Reserved

// DS[2:0] (bits 7:5) - Data size
100: 16 bits
110: 24 bits ✓
111: 32 bits

// SYNCEN[1:0] (bits 11:10)
00: Asynchronous mode ✓
01: Synchronous with other internal audio subblock ✓ (must be slave)
10: Synchronous with external SAI (must be slave)
11: Reserved
```

**SAI_xFRCR configuration verified (línea 364788-364795):**

```c
// FRL[7:0] (bits 7:0) - Frame length
Frame length in SCK cycles = FRL + 1
TDM8: FRL = 255 → 256 SCK cycles ✓

// FSALL[6:0] (bits 14:8) - FS active level length
FS length in SCK cycles = FSALL + 1
TDM8: FSALL = 0 → 1 SCK cycle ✓
```

### 4. DMAMUX1 Mapping (Verified)

**Table 122 (línea 111894) - DMAMUX1 assignments:**

```
Request 87 → sai1a_dma ✓
Request 88 → sai1b_dma ✓
Request 113 → sai3_a_dma (NOT SAI4!)
```

**✅ SAI1 DMA configuration correcta:**
- SAI1_A: DMAMUX1 request 87 → DMA1 Stream 0
- SAI1_B: DMAMUX1 request 88 → DMA1 Stream 5

---

## 🔧 CORRECCIONES NECESARIAS

### Archivos a Actualizar

1. **stm32/dma_mapping_ref.md** ❌ CRÍTICO
   - Eliminar referencias a SAI4 con DMA2
   - Añadir sección BDMA para SAI4
   - Documentar DMAMUX2 configuration
   - Crear ejemplos de BDMA init

2. **stm32/audio_architecture.md** ⚠️ IMPORTANTE
   - Actualizar diagrama de bloques
   - Cambiar "DMA2 Stream 2" → "BDMA"
   - Añadir nota sobre dominio D3

3. **stm32/system_init_sequence.md** ⚠️ IMPORTANTE
   - Actualizar secuencia de init DMA
   - Añadir init de BDMA para SAI4

4. **stm32/rcc_registers_ref.md** ⚠️ MENOR
   - Verificar cálculo de FRACN3
   - Recalcular para exactamente 12.288 MHz

---

## 📚 Referencias RM0433

### Capítulos Clave

- **Chapter 8:** Reset and Clock Control (RCC) - página 325
  - Section 8.7.17: RCC_PLL3DIVR (línea 55757)
  - Section 8.7.18: RCC_PLL3FRACR (línea 55916)
  - Section 8.7.19: RCC_D1CCIPR (línea 56062) - SAI1 clock source
  - Section 8.7.20: RCC_D3CCIPR (línea 56812) - SAI4 clock source

- **Chapter 15:** DMA Controllers
  - Section 15.4: DMAMUX - DMA request multiplexer
  - Table 102: DMAMUX1 connections (línea 91827)
  - Table 103: DMAMUX2 connections (línea 92903)
  - Table 122: DMAMUX1 request mapping (línea 111894)
  - Table 126: DMAMUX2 request mapping (línea 113114)

- **Chapter 16:** BDMA Controller
  - (Buscar información específica de BDMA para SAI4)

- **Chapter 51:** Serial Audio Interface (SAI) - página 2237
  - Section 51.6.2: SAI_ACR1/BCR1 (línea 363787)
  - Section 51.6.4: SAI_AFRCR/BFRCR (línea 364788)
  - Section 51.6.6: SAI_ASLOTR/BSLOTR

---

## ✅ PLAN DE ACCIÓN

1. ✅ Extraer información de RM0433
2. ✅ Identificar errores en documentación
3. ⬜ Buscar información sobre BDMA en RM0433
4. ⬜ Corregir dma_mapping_ref.md con BDMA
5. ⬜ Actualizar audio_architecture.md
6. ⬜ Verificar cálculo PLL3 FRACN3
7. ⬜ Crear commit con todas las correcciones

---

## 💡 NOTAS ADICIONALES

### BDMA vs DMA1/DMA2

**BDMA (Basic DMA):**
- Ubicado en dominio D3
- Menor complejidad que DMA1/DMA2
- Optimizado para periféricos de bajo consumo
- SAI4 está en D3 → debe usar BDMA

**DMA1/DMA2:**
- Ubicados en dominio D2
- Mayor capacidad y features
- SAI1/SAI2/SAI3 están en D2 → usan DMA1/DMA2

### Implicaciones para el Proyecto

**Para el ADC (SAI4_B):**
- Cambiar todo el código de DMA2 → BDMA
- Usar diferentes registros y estructuras
- Considerar si el rendimiento de BDMA es suficiente para 2 canales @ 48kHz
  - Bandwidth: 2ch × 32bit × 48kHz = 384 KB/sec
  - BDMA debería ser más que suficiente

**Alternativa:**
- Considerar usar SAI1 o SAI3 para el ADC en lugar de SAI4
- Esto permitiría usar DMA1/DMA2 estándar
- Pero requeriría verificar disponibilidad de pines

---

**Documento creado:** 2026-01-22 19:30
**Última actualización:** 2026-01-22 19:30
**Estado:** En proceso de corrección
