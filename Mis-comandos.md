# Comandos y Referencias - DS9 Beamforming

## Checklist al iniciar sesion

- [ ] Conectar ST-Link (SWD)
- [ ] **Si vas a usar ESP32 UART: desconectar ST-Link serial** (contención en PA0/PA1)
- [ ] Abrir minicom en terminal separada

---

## Build y Flash (STM32)

```bash
cd /Users/bergamifer/STM32CubeIDE/DS-Firmware/firmware

# Solo compilar
make -j4

# Compilar + Flash via ST-Link (recomendado)
make flash-stlink # No olvidar luego resetar con boton K4

# Limpiar y recompilar
make clean && make -j4
```

**Nota DFU:** Para entrar en modo DFU, mantener BOOT0, presionar RESET, soltar BOOT0.

- Flashear en DFU:
```bash
cd firmware && make flash
```

---

## Debug Serial (UART)

```bash
# Monitor serial ST-Link (UART7, PA8/PA15)
minicom -D /dev/tty.usbmodem1102 -b 115200

# Buscar puertos serie
ls /dev/tty.usb*
ls /dev/cu.usb*

# Para salir de minicom: Ctrl+A luego X
```

---

## Conexiones ST-Link → DS9

```
ST-Link          DS9 Board
--------         ---------
  GND    ─────   GND
  SWCLK  ─────   PA14 (SWCLK)
  SWDIO  ─────   PA13 (SWDIO)
  3.3V   ─────   3.3V  ⚠️ DS9 sin fuente externa (ver issue #1)
  RST    ─────   NRST (opcional, recomendado)
  TX     ─────   PA8  (UART7_RX) ← debug serial STM32 → host
  RX     ─────   PA15 (UART7_TX) ← debug serial host → STM32
```

```
⚠️ PA0/PA1 compartidos: ST-Link serial TX/RX vs ESP32 UART4
   Desconectar ST-Link serial ANTES de usar ESP32 (contención eléctrica)
```

```bash
# Verificar conexion ST-Link
st-info --probe
```

---

## VS Code Tasks (Cmd+Shift+B)

| Task | Descripcion |
|------|-------------|
| Build | Compilar (default) |
| Clean Build | Limpiar + compilar |
| Flash (ST-Link) | Flash via SWD |

---

## Webserver (control DS9)

```bash
cd /Users/bergamifer/STM32CubeIDE/DS-Firmware/webserver
./run.sh

# Acceso: http://localhost:5001
# Detener: Ctrl+C
```

---

## ESP32-C3 (IDF)

```bash
# Cargar entorno ESP-IDF
. $HOME/esp/esp-idf/export.sh

# Build + flash + monitor
idf.py build
idf.py flash -p /dev/cu.usbmodem1101
idf.py monitor -p /dev/cu.usbmodem1101

# Flash sin rebootear
idf.py flash -p /dev/cu.usbmodem1101 && idf.py monitor -p /dev/cu.usbmodem1101 --no-reset

# Listar puertos
ls /dev/cu.usb*
```

**UART DS9 wiring confirmado (trazas cruzadas en PCB, 2026-02-19):**
```
ESP32 GPIO0 (TX) ──── PA1 (STM32 UART4 RX)   ← firmware: STM32_UART_TX = GPIO_NUM_0
ESP32 GPIO1 (RX) ──── PA0 (STM32 UART4 TX)   ← firmware: STM32_UART_RX = GPIO_NUM_1
```

---

## Archivos Importantes

| Archivo | Descripcion |
|---------|-------------|
| `firmware/Core/Src/main.c` | Init, clocks 480MHz, DMA, callbacks, main loop |
| `firmware/Core/Src/uart_protocol.c` | Parser binario ESP32→STM32 |
| `firmware/Core/Inc/uart_protocol.h` | Protocol defines, ds_config_t |
| `firmware/Core/Src/pcm1690.c` | Driver PCM1690 DAC |
| `firmware/Core/Src/epaper.c` | Driver SSD1681 e-paper |
| `firmware/STM32H743XX_FLASH.ld` | Linker script (.dma_buffer → RAM_D2) |
| `firmware/Makefile` | Build config (TARGET=ds9_beamforming) |
| `docs/ds9_pin_audit.md` | Errores SAI en PCB DS9 y workarounds |
| `docs/DS_Hardware_Map_v2.md` | Mapa completo de pines DS9 |
