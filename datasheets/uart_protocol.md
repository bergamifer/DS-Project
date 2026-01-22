# UART Protocol - ESP32 ↔ STM32

## Message Format
Header: "DELAY" (5 bytes)
Data: 16× uint16_t delays
Checksum: uint16_t

## Example
"DELAY" + [100, 150, 200, ...] + 0x1A2B