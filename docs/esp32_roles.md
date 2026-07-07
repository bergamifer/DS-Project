# ESP32-C3-MINI-1 (U5): roles, radio y enlace con el STM32

> Escrito 2026-07-07. Qué capacidades del módulo aprovechar más allá del rol actual
> (WiFi STA + servidor HTTP + puente UART binario al STM32). Aterrizado sobre el
> pinout REAL de la DS11 (netlist `hardware/DS/DS.txt`).

---

## 1. Qué tiene el chip (y qué no)

- **Núcleo RISC-V 160 MHz + 400 KB RAM** — hoy casi ocioso (el HTTP no lo usa).
- **WiFi 802.11 b/g/n** 2.4 GHz — en uso (STA + HTTP).
- **Bluetooth 5 (LE)** — **solo Low Energy, NO Bluetooth clásico** → nada de A2DP /
  streaming de audio por BT. Sí perfiles BLE, incluido **BLE-MIDI**.
- **ESP-NOW** — protocolo Espressif sin router, ESP32↔ESP32 directo, latencia ~1-5 ms,
  jitter bajo. Convive con WiFi STA (misma pila; *ojo*: los peers ESP-NOW deben estar
  en el mismo canal WiFi que el AP cuando el nodo está también asociado como STA).
- **Caveat de coexistencia WiFi/BLE**: una sola radio de 2.4 GHz compartida por
  time-slicing. Coexisten, pero un nodo haciendo BLE-MIDI *y* HTTP a full tiene jitter.
  → Conviene asignar rol por nodo (ver §4).

---

## 2. Aplicaciones que valen para Domosonica

### Tier 1 — alineadas con el proyecto

1. **ESP-NOW para coordinación multi-nodo.** Resuelve la "conmutación a las T"
   (docs/multinode.md §2): un broadcast en vez de 15 requests HTTP con jitter. El nodo
   cabecera puede ser el **director** (guarda la partitura, reparte escenas por ESP-NOW)
   → la instalación se vuelve autónoma, sin laptop/RPi externa.
2. **BLE-MIDI — el nodo como instrumento inalámbrico.** El C3 se presenta como
   dispositivo BLE-MIDI, que **macOS/iOS reconocen nativamente** (sin drivers). Un
   compositor mapea notas/CCs desde su DAW → foco, modo, trayectoria. Es el puente
   directo a la convocatoria de músicos.
3. **WiFi provisioning por BLE.** Configurar la red de cada uno de los 15 nodos
   acercándose con el teléfono, sin SSID hardcodeado ni reflasheo.

### Tier 2 — situacionales
- BLE como control directo teléfono↔nodo (sin infraestructura WiFi, para demos).
- Ableton Link (sync de tempo con las herramientas del músico) — evaluar si entra en el C3.
- OTA de firmware por WiFi a la flota.

### Tier 3 — descartadas
- Posicionamiento por RSSI de BLE (precisión de metros — inútil para colocar focos).
- Sensor de temperatura local para corregir c (el AUTOFOCUS ya lo absorbe; GPIOs justos).
- WiFi mesh (ESP-MESH) — el daisy-chain + WiFi normal alcanzan.

---

## 3. Impacto en hardware DS11: casi nulo

BLE-MIDI y ESP-NOW son **firmware sobre la radio existente** — cero componentes nuevos.
El único ítem hardware es de **layout** (no de esquemático):

- 🔴 **Keepout de antena.** U5 en (126, 40), a 5.4 mm del borde superior, antena hacia
  el borde (bien ubicado). Verificar que **no haya cobre en la zona de antena en ninguna
  capa**, en particular que el plano **In1 (GND sólido) esté recortado** debajo. Plano de
  masa entero bajo antena de PCB = alcance WiFi/BLE destruido. Se respeta solo si el pour
  cumple el keepout del footprint — solo hay que confirmarlo visualmente.

---

## 4. Enlace con el STM32 (pinout real DS11)

Enlace actual (netlist): **ESP32 GPIO0 (TX, pin 12) / GPIO1 (RX, pin 13) ↔ STM32 (U7)
pines 22/23** — net "esp32-mini/UART1", 115200 8N1.

**El UART compartido alcanza de sobra.** El tráfico es control-rate (modos, beam, status)
y MIDI (3 bytes/mensaje). A 115200 hay ~10× de margen. La "eficiencia" es firmware:
- subir el baud (921600+) — cero cambio hardware;
- para MIDI, *fire-and-forget* (sacar el ACK por mensaje) → latencia mínima.

**GPIOs libres del ESP32** (ruteados a net pero sin nada del otro lado, según netlist):
**GPIO4 (pin 18), GPIO5 (pin 19), GPIO6 (pin 20), GPIO7 (pin 21)**; además GPIO20/21
(UART0 nativo, pines 30/31). GPIO8/9 son strapping — no usar como I/O general.

- Pines ya comprometidos: GPIO0/1 (UART al STM32), GPIO10→D1 (LED), GPIO9→R106 (boot
  strap), GPIO18/19 (USB D-/D+ para programación), EN (reset).

### Línea GPIO4→PC13: opcional, y NO hace falta la inversa

**Aclaración importante (2026-07-07): el UART ya es interrupt-driven en ambos sentidos.**
Cuando el STM32 manda un byte, el UART del ESP32 lo recibe por IRQ/DMA (no pollea), y
viceversa. O sea: el STM32 avisa cualquier evento al ESP32 con solo mandar un mensaje
(~87 µs/byte) — **no hace falta línea IRQ inversa (STM32→ESP32)**. Y la línea *forward*
(GPIO4→PC13) también es redundante para "notificar un evento".

Lo único que un GPIO dedicado hace mejor que el UART: (1) **pulso de timing sin jitter**
(un flanco es instantáneo; un mensaje UART tiene latencia de parseo + jitter de cola)
para un futuro disparo sample-accurate entre nodos; (2) **wake de sleep** (PC13 es WKUP).
Para BLE-MIDI el UART sobra.

**Si igual se rutea (hedge de timing, verificado en netlist 2026-07-07):**
- **GPIO4 (U5 pin 18, push-pull, no strapping) → PC13 (U7 pad 7)**. PC13 libre; VBAT
  atado a +3V3 → PC13 usable como GPIO normal; sin otro `Px13` en uso → EXTI13 libre.
  PC13 como **entrada/EXTI** (el drive limitado de PC13/14/15 no aplica: recibe, no maneja).
- **R serie ~220 Ω** (100-330), cerca del ESP32 — protección de contención en el boot
  asíncrono de los dos procesadores. Idle definido con **pull interno del STM32** en PC13
  (activa-baja: pull-up + pulso a bajo), sin pull externo. Ambos 3.3V → directo.
- **No rutear la inversa** — el UART cubre STM32→ESP32.

---

## 5. TODOs de firmware (cuando toque)

1. ESP-NOW: rol cabecera/miembro, broadcast de escena, integración con "delay de
   ejecución a las T" del protocolo (docs/multinode.md §2 TODO 2).
2. BLE-MIDI: perfil peripheral, mapeo MIDI→(modo/beam), fire-and-forget al STM32.
3. Gestión de coexistencia: elegir por nodo si corre BLE o WiFi caliente.
4. WiFi provisioning por BLE (componente `wifi_provisioning` de ESP-IDF).
5. mDNS `domosonica-N.local` para descubrimiento (ya hay `DEVICE_ID`).

Ver [multinode.md](multinode.md) (coordinación) y [app_ios.md](app_ios.md) (control).
