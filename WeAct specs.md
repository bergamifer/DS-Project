# STM32H7xx Core Board
> `0.96''` ST7735 TFT, TF Card, 8MB SPI FLASH(SPI1)， 8MB QSPI FLASH(QSPI1), 8Bit DVP Port

## STM32H743VIT6 2048KB ROM, 1MB RAM
* ARM Cortex M7
* 480Mhz Max. Freq
* 2048KB ROM, 1MB RAM
* 8MB SPI Flash, 8MB QSPI Flash

![](Images/STM32H750VB_1.jpg)

### Interface and Keys
* 2*22 Pin 2.54mm I/O x 2
* 4 Pin 2.54mm SW x 1
* USB C (type C)  x 1
* MicroSD TF x 1
* 8Bit DCMI x 1
* User Key K1 (PC13) x 1
* NRST Key x 1
* BOOT0 Key x 1

### Design and Quality
* `TG155` plate with gold sinking process is designed with four layers
* Use `Lead-Free` welding process
* Use the button to set BOOT
* Use high quality crystal vibration, metal shell, all can be good vibration
* Always use the original ST chip
> We do not provide the factory test firmware to prevent the appearance of duplicates, affecting the quality

### Chip Date Code
STM32H743VIT6

### AliExpress
1. [WeAct Studio Official Store](https://weactstudio.aliexpress.com/)

## Begin to use

### How to enter ISP mode
* Method 1: When the power is on, press the BOOT0 key and the reset key, then release the reset key, and release the BOOT0 key after 0.5 seconds
* Method 2: When the power is off, hold down the BOOT0 key, and release the BOOT0 at 0.5s after the power is on
* DFU Mode: Use the data line to connect to the computer.
* Serial Port Mode: Connect PA9 and PA10 of core board with USB serial port
* Soft: STM32CubeProg。

### Power supply and Board Shape
* Input Voltage: 3.3V-5.5V
* DC-DC Output Current: 1A Max.
* Size: 40.64mm * 66.88mm