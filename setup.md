# Beamforming Array - Project Setup

## Hardware
- MCU: STM32H743VIT6 (WeAct Development Board)
- DACs: 2× PCM1690 (16 canales salida total)
- ADC: 1× PCM1802 (2 canales entrada)
- Control futuro: ESP32 via UART

## Especificaciones Audio
- Sample Rate: 48 kHz
- Bit depth: 24 bits
- Formato: I2S/TDM
- Delay máximo: 100ms por canal
- Latencia objetivo: <10ms

## Pinout
[DS_Hardware_Map.md]

## Power Supply
[PowerSpecs.md]