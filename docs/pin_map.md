# DS10/DS11 Board Pin Map — STM32H743VIT6 LQFP100

> **Hardware:** DS10 y DS11 PCB (pinout idéntico, verificado en el netlist DS11) (STM32H743VIT6 + AK5538VN + 4×PCM1690 + 32×PAM8302A + ESP32-C3-Mini)
> **Netlist verificado:** 2026-04-02 (574 nets — DS10_DESIGN_REVIEW.md)
> **Fuente de verdad en código:** `firmware/Core/Inc/pin_definitions.h`

---

## Port A (PA0–PA15)

| Pin | GPIO | Función | Periférico | AF | Notas |
|-----|------|---------|------------|-----|-------|
| PA0  | UART4_TX  | ESP32 link TX | UART4 | AF8 | STM32→ESP32 GPIO0(RX) |
| PA1  | UART4_RX  | ESP32 link RX | UART4 | AF8 | ESP32 GPIO1(TX)→STM32 |
| PA2  | USART2_TX | (disponible)  | — | — | Libre |
| PA3  | USART2_RX | (disponible)  | — | — | Libre |
| PA4  | OPT_SW_K2 | Botón usuario K2 | GPIO_INPUT | — | Pull-up externo |
| PA5  | —         | Sin usar | GPIO_ANALOG | — | |
| PA6  | LED_D4    | D4 Amarillo — PWM | TIM3_CH1 | AF2 | LED_POWER |
| PA7  | LED_D5    | D5 Rojo — PWM | TIM3_CH2 | AF2 | LED_AUDIO |
| PA8  | I2C3_SCL  | AK5538VN SCL | I2C3 | AF4 | pull-up 4.7kΩ en PCB |
| PA9  | —         | Sin usar | GPIO_ANALOG | — | |
| PA10 | —         | Sin usar | GPIO_ANALOG | — | |
| PA11 | USB_DM    | USB D- | USB_OTG_FS | — | STM32 USB Device |
| PA12 | USB_DP    | USB D+ | USB_OTG_FS | — | STM32 USB Device |
| PA13 | SWDIO     | SWD data | SWD | AF0 | J7/pin2 |
| PA14 | SWCLK     | SWD clock | SWD | AF0 | J7/pin3 |
| PA15 | AK_OVF    | AK5538VN overflow flag | GPIO_INPUT | — | Output AK5538 → input STM32 |

---

## Port B (PB0–PB15)

| Pin | GPIO | Función | Periférico | AF | Notas |
|-----|------|---------|------------|-----|-------|
| PB0  | LED_D6   | D6 Verde — PWM | TIM3_CH3 | AF2 | LED_COMM |
| PB1  | LED_D7   | D7 Blanco — PWM | TIM3_CH4 | AF2 | LED_STATUS |
| PB2  | —        | Sin usar | GPIO_ANALOG | — | |
| PB3  | SWO      | SWD trace output | SWO | AF0 | J7/pin5 |
| PB4  | —        | Sin usar | GPIO_ANALOG | — | |
| PB5  | EEPROM_WP | Write-protect EEPROM | GPIO_OUTPUT | — | HIGH=protegido, LOW=escritura |
| PB6  | I2C4_SCL | EEPROM CAT24C256 SCL | I2C4 | **AF6** | pull-up en PCB (RN5 4.7k) |
| PB7  | I2C4_SDA | EEPROM CAT24C256 SDA | I2C4 | **AF6** | pull-up en PCB (RN5 4.7k) |
| PB8  | I2C1_SCL | DAC bus SCL (4×PCM1690) | I2C1 | AF4 | RN5 4.7kΩ pull-up |
| PB9  | I2C1_SDA | DAC bus SDA (4×PCM1690) | I2C1 | AF4 | RN5 4.7kΩ pull-up |
| PB10 | I2C2_SCL | Expansión J5 | I2C2 | AF4 | RN6 4.7kΩ pull-up |
| PB11 | I2C2_SDA | Expansión J5 | I2C2 | AF4 | RN6 4.7kΩ pull-up |
| PB12 | —        | Sin usar | — | — | Libre |
| PB13 | —        | Sin usar | — | — | Libre |
| PB14 | —        | Sin usar | — | — | Libre |
| PB15 | —        | Sin usar | GPIO_ANALOG | — | |

> **Nota AF6 I2C4 (verificado en hardware 2026-05):** PB6/PB7 AF4 = I2C1 (ya usado en PB8/PB9). El AF correcto para I2C4 en estos pines es **AF6** — con AF11 (que decía el design review) la EEPROM no hace ACK. Confirmado empíricamente en DS9 y DS10: con AF6, `EEPROM available=1`.

---

## Port C (PC0–PC15)

| Pin | GPIO | Función | Periférico | AF | Notas |
|-----|------|---------|------------|-----|-------|
| PC0  | — | Sin usar | GPIO_ANALOG | — | |
| PC1  | — | Sin usar | GPIO_ANALOG | — | |
| PC2  | — | Sin usar | GPIO_ANALOG | — | |
| PC3  | — | Sin usar | GPIO_ANALOG | — | |
| PC4  | — | Sin usar | — | — | Libre (DS9: LED D6) |
| PC5  | — | Sin usar | — | — | Libre (DS9: LED D7) |
| PC6  | — | Sin usar | GPIO_ANALOG | — | |
| PC7  | AK_PDN   | AK5538VN power-down | GPIO_OUTPUT | — | HIGH=activo, LOW=PDN |
| PC8  | AK_DIF1  | AK5538VN DIF1 | GPIO_OUTPUT | — | Strapped LOW en PCB (TDM mode) |
| PC9  | I2C3_SDA | AK5538VN SDA | I2C3 | AF4 | |
| PC10 | DAC_MUTE_OUT | AMUTEO wire-OR (4 DACs) | GPIO_INPUT | — | LOW = algún DAC auto-silenciado |
| PC11 | DAC_RST  | ~RST compartido 4×PCM1690 | GPIO_OUTPUT | — | LOW=reset, HIGH=operación |
| PC12 | DAC_MUTE_IN | AMUTEI compartido 4×PCM1690 | GPIO_OUTPUT | — | LOW=muted, HIGH=activo |
| PC13 | — | Sin usar | GPIO | — | Libre |
| PC14 | OSC32_IN  | Crystal LSE 32.768 kHz | RCC | — | Y1 |
| PC15 | OSC32_OUT | Crystal LSE 32.768 kHz | RCC | — | Y1 |

---

## Port D (PD0–PD15)

| Pin | GPIO | Función | Periférico | AF | Notas |
|-----|------|---------|------------|-----|-------|
| PD0  | — | Sin usar | — | — | Libre |
| PD1  | — | Sin usar | — | — | Libre |
| PD2  | — | Sin usar | GPIO_ANALOG | — | |
| PD3  | PAM_SD  | Shutdown 32×PAM8302A | GPIO_OUTPUT | — | LOW=off (safe), HIGH=amps activos |
| PD4  | — | Sin usar | — | — | Libre |
| PD5  | — | Sin usar | GPIO_ANALOG | — | |
| PD6  | — | Sin usar | GPIO_ANALOG | — | |
| PD7  | — | Sin usar | — | — | Libre |
| PD8  | SAI3_SCK_B | AK5538VN BICK | SAI3_B | AF6 | STM32→AK5538 (Master genera BCK) |
| PD9  | SAI3_SD_B  | AK5538VN SDTO1 | SAI3_B | AF6 | AK5538→STM32 (audio data in) |
| PD10 | SAI3_FS_B  | AK5538VN LRCK | SAI3_B | AF6 | STM32→AK5538 (Master genera FS) |
| PD11 | SAI2_SD_A  | PCM1690 U1 DIN | SAI2_A | AF10 | ch 16-23 audio data |
| PD12 | SAI2_FS_A  | PCM1690 U1+U4 LRCK | SAI2_A | AF10 | |
| PD13 | SAI2_SCK_A | PCM1690 U1+U4 BCK | SAI2_A | AF10 | |
| PD14 | — | Sin usar | GPIO_ANALOG | — | |
| PD15 | — | Sin usar | GPIO_ANALOG | — | |

---

## Port E (PE0–PE15)

| Pin | GPIO | Función | Periférico | AF | Notas |
|-----|------|---------|------------|-----|-------|
| PE0  | — | Sin usar | GPIO_ANALOG | — | DS9: DAC1_AMUTEO |
| PE1  | — | Sin usar | GPIO_ANALOG | — | DS9: DAC1_AMUTEI |
| PE2  | SAI1_MCLK | MCLK 24.576 MHz | SAI1_A | AF6 | Bus MCLK a todos los ICs |
| PE3  | SAI1_SD_B | PCM1690 U3 DIN | SAI1_B | AF6 | ch 8-15 audio data (slave sync) |
| PE4  | SAI1_FS_A | PCM1690 U2+U3 LRCK | SAI1_A | AF6 | |
| PE5  | SAI1_SCK_A | PCM1690 U2+U3 BCK | SAI1_A | AF6 | |
| PE6  | SAI1_SD_A | PCM1690 U2 DIN | SAI1_A | AF6 | ch 0-7 audio data |
| PE7  | UART7_RX  | Debug header J7 | UART7 | AF7 | J7/pin7 |
| PE8  | UART7_TX  | Debug header J7 | UART7 | AF7 | J7/pin8 |
| PE9  | — | Sin usar | GPIO_ANALOG | — | DS9: DAC1_SAI_SD (ERROR pin) |
| PE10 | — | Sin usar | GPIO_ANALOG | — | DS9: DAC0_RST |
| PE11 | SAI2_SD_B | PCM1690 U4 DIN | SAI2_B | AF10 | ch 24-31 audio data (slave sync) |
| PE12 | — | Sin usar | GPIO_ANALOG | — | DS9: DAC0_AMUTEO |
| PE13 | — | Sin usar | GPIO_ANALOG | — | |
| PE14 | — | Sin usar | GPIO_ANALOG | — | |
| PE15 | — | Sin usar | GPIO_ANALOG | — | |

---

## Port H (PH0–PH1)

| Pin | GPIO | Función | Periférico | Notas |
|-----|------|---------|------------|-------|
| PH0 | OSC_IN  | Crystal HSE 25 MHz | RCC | Y2 — PLL1+PLL3 source |
| PH1 | OSC_OUT | Crystal HSE 25 MHz | RCC | Y2 |

---

## I2C Bus Summary

| Bus | SCL | SDA | AF | Dispositivos | Velocidad |
|-----|-----|-----|----|-------------|-----------|
| I2C1 | PB8  | PB9  | AF4  | 4× PCM1690 DAC (0x98/0x9A/0x9C/0x9E) | 100 kHz SM |
| I2C2 | PB10 | PB11 | AF4  | Expansión J5 | — |
| I2C3 | PA8  | PC9  | AF4  | AK5538VN ADC (**0x22** = 7-bit 0x11, verificado por scan) | 100 kHz SM |
| I2C4 | PB6  | PB7  | **AF6** | EEPROM CAT24C256 (0xA0) | 100 kHz SM |

---

## Audio Signal Routing

| SAI Block | DMA | DAC | Ref | I2C Addr | Canales |
|-----------|-----|-----|-----|----------|---------|
| SAI1_A (Master TX) | DMA1_S0 req87 | DAC0 | U2 | 0x98 | ch 0-7 |
| SAI1_B (Slave TX)  | DMA1_S3 req88 | DAC1 | U3 | 0x9C | ch 8-15 |
| SAI2_A (Master TX) | DMA1_S1 req89 | DAC2 | U1 | 0x9A | ch 16-23 |
| SAI2_B (Slave TX)  | DMA1_S2 req90 | DAC3 | U4 | 0x9E | ch 24-31 |
| SAI3_B (Master RX) | DMA1_S4 req114 | ADC | IC3 | 0x22 (I2C3) | 8 ch in |

---

## Control GPIO Quick Reference

| Signal | Pin | Dirección | Activo | Función |
|--------|-----|-----------|--------|---------|
| DAC_RST | PC11 | Output | LOW | ~RST bus a los 4 PCM1690 |
| DAC_MUTE_IN | PC12 | Output | LOW | AMUTEI bus — silencia todos los DAC |
| DAC_MUTE_OUT | PC10 | Input | LOW | AMUTEO wire-OR — detecta auto-mute en algún DAC |
| PAM_SD | PD3 | Output | LOW | Shutdown 32 amplificadores PAM8302A |
| AK_PDN | PC7 | Output | LOW | Power-down AK5538VN ADC |
| AK_DIF1 | PC8 | Output | — | Interface mode (strapped LOW = TDM) |
| AK_OVF | PA15 | Input | HIGH | Overflow flag desde AK5538VN |
| EEPROM_WP | PB5 | Output | HIGH | Write-protect CAT24C256 |

---

## LEDs (TIM3 PWM, 1kHz, 300 steps)

| LED | Ref | Pin | TIM3 CH | Color | Alias firmware |
|-----|-----|-----|---------|-------|---------------|
| D4 | — | PA6 | CH1 AF2 | Amarillo | LED_POWER |
| D5 | — | PA7 | CH2 AF2 | Rojo | LED_AUDIO |
| D6 | — | PB0 | CH3 AF2 | Verde | LED_COMM |
| D7 | — | PB1 | CH4 AF2 | Blanco | LED_STATUS |

---

## ESP32-C3-Mini (U5) GPIO

| GPIO ESP32 | Conexión | Pin STM32 | Función |
|-----------|----------|-----------|---------|
| GPIO0 | STM32 PA0 (UART4_TX) | PA0 | ESP32 RX ← STM32 TX |
| GPIO1 | STM32 PA1 (UART4_RX) | PA1 | ESP32 TX → STM32 RX |
| GPIO8 | Pull-up 10kΩ → +3.3V | — | Strapping — boot normal |
| GPIO9 | Pull-up 10kΩ → +3.3V | PC6 (opcional) | Download mode opcional |
| GPIO10 | LED D12 (blanco, 120Ω) | — | ESP32 status LED |
| GPIO18 | USB D- | — | USB-C programación |
| GPIO19 | USB D+ | — | USB-C programación |

> **Nota wiring DS10 UART:** Las trazas del PCB cruzan GPIO0↔PA1 y GPIO1↔PA0 (igual que DS9). TX del ESP32 (GPIO0) va al RX del STM32 (PA1) y viceversa. El firmware usa UART4 con PA0=TX / PA1=RX.

---

## Power Supply DS10

| Rail | Fuente | Regulador | Consumidores |
|------|--------|-----------|--------------|
| 24V  | Externo | — | Entrada principal |
| 5VA  | Buck 24V→5VA | U_BUCK | 32× PAM8302A alimentación analógica |
| 3.3V | LDO 5VA→3.3V | — | STM32, ESP32, PCM1690 VDD |
| 3.3VA | LDO 5VA→3.3VA | — | PCM1690 analógico, AK5538VN |

---

## Pines Libres / Disponibles

**Para expansión:** PB12, PB13, PB14, PC4, PC5, PC13, PD0, PD1, PD4, PD7, PA2, PA3

**GPIO_ANALOG (EMI reducida):** PA5, PA9, PA10, PB2, PB4, PB15, PC0-PC3, PC6, PD2, PD5, PD6, PD14, PD15, PE0, PE1, PE9, PE10, PE12-PE15

---

*DS10 Pin Map — Verificado contra netlist DS10_DESIGN_REVIEW.md 2026-04-02*
*Ver [audio_architecture.md](audio_architecture.md) para detalle de audio y beamforming*
