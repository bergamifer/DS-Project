# DS12 — Puerto USB del STM32: interfaz de audio 8×8

> Escrito el 2026-09-03, a pedido de Fernando: *"evaluá agregar un puerto USB al STM32 …
> para tener una interfaz de audio potencialmente controlable, tipo Focusrite, que maneje
> 8 entradas y 8 salidas"*.
>
> Datos verificados ✅ contra `DS.txt` (netlist de producción DS11) y contra
> `datasheets/stm32h743vi.pdf` (DS12110 Rev 11). Los ⚠️ hay que confirmarlos.

---

## 0. La respuesta corta

| pregunta | respuesta |
|---|---|
| ¿Hay pines libres para USB en el STM32? | **Sí, y no hay que migrar nada.** PA11/PA12 (D−/D+) y PA9/PA10 están **físicamente sin conectar** en la DS11 ✅ |
| ¿Alcanza para 8 in / 8 out a 24 bits / 48 kHz? | **No.** Hacen falta 1152 B por trama y el máximo de un endpoint isócrono Full-Speed es **1023 B**. No es cuestión de optimizar: no entra. |
| ¿Qué hace falta para 8×8? | **USB High-Speed**, que en el H743 exige un **PHY ULPI externo** de 12 señales |
| ¿Entra el ULPI en el encapsulado actual? | **No.** `ULPI_DIR` y `ULPI_NXT` sólo existen en LQFP176 o mayor ✅ |
| ¿Entonces? | Dos caminos, §5 y §6. El de FS cuesta 4 pines muertos y tres componentes; el de HS cuesta cambiar de encapsulado. |
| ¿Y el hardware de las 8 entradas? | **Ya existe**: el AK5538 es de 8 canales y las 8 entradas diferenciales están cableadas a J9 con protección ESD ✅ |

---

## 1. El cálculo que decide todo

Un endpoint isócrono es la única forma de transportar audio por USB, y el estándar le pone
un techo duro al tamaño de paquete:

| | Full-Speed (12 Mbit/s) | High-Speed (480 Mbit/s) |
|---|---|---|
| período de servicio | 1 trama = **1 ms** | 1 microtrama = **125 µs** |
| máximo por endpoint y período | **1023 bytes** | 1024 bytes (hasta 3072 con alta banda) |
| presupuesto periódico total | 90 % de 1500 B = **1350 B/ms** | 80 % de 7500 B = **6000 B/µtrama** |

Y lo que hace falta para audio (bytes por milisegundo = `canales × bytes/muestra × kHz`):

| formato | B/ms | ¿entra en un endpoint FS? | ¿entra en FS ida y vuelta? |
|---|---|---|---|
| 8 ch × 24 bit × 48 kHz | **1152** | ❌ (>1023) | ❌ |
| 8 ch × 24 bit × 44,1 kHz | **1058** | ❌ (>1023) | ❌ |
| 8 ch × 16 bit × 48 kHz | 768 | ✅ | ❌ (2×768+overhead ≈ 1562 > 1350) |
| **6 ch × 24 bit × 48 kHz** | 864 | ✅ | ❌ (1754 > 1350) |
| **4 ch × 24 bit × 48 kHz** | 576 | ✅ | ✅ (2×576 + overhead ≈ 1183 < 1350) |
| 2 ch × 24 bit × 96 kHz | 576 | ✅ | ✅ |

> **8 canales a 24 bits ni siquiera entran en UN endpoint Full-Speed, en una sola
> dirección.** El límite de 1023 B es del estándar USB 2.0, no del STM32: no hay firmware,
> compilador ni truco que lo mueva.

En High-Speed el mismo caso son **144 bytes por microtrama** — el 2,4 % del presupuesto.
De hecho en HS entran **los 32 canales**: 32 × 3 × 48 = 4608 B/ms = 576 B/µtrama, ~10 % del
presupuesto periódico. Con HS el USB deja de ser el cuello de botella de esta placa por
completo.

**Techo práctico de Full-Speed: 4 in + 4 out a 24 bit/48 kHz.** Techo de High-Speed: la
placa entera.

---

## 2. Qué tiene el STM32H743 (verificado en el datasheet) ✅

Sección 3.40 del DS12110: el chip trae **dos** periféricos USB OTG.

| | controlador | pines FS | HS |
|---|---|---|---|
| **OTG_HS1** (`USB1_OTG_HS`) | FS **y** HS | PB14 (DM) / PB15 (DP), **AF12** | **sí, con PHY ULPI externo** |
| **OTG_HS2** (`USB2_OTG_FS`) | **sólo FS** | PA11 (DM) / PA12 (DP), **AF10** | no, nunca |

Ambos: FIFO combinado de **4 KB** con dimensionamiento dinámico, **9 endpoints
bidireccionales**, **DMA interno**, y —cita textual del datasheet— **"HNP/SNP/IP inside
(no need for any external resistor)"**: el pull-up de 1,5 kΩ en D+ y las resistencias serie
están integradas. **No hay que poner los 33 Ω que sí lleva el ESP32 en R12/R13.**

Detalle importante del encapsulado, también textual: *"Dedicated supply inputs for USB
(OTG_FS and OTG_HS) are available on all packages **except LQFP100**"* ✅. O sea que en la
placa actual **el transceptor USB se alimenta de `VDD`**, y el datasheet exige
`VDD33USB = 3,0 a 3,6 V` cuando se usa USB. Nuestro riel nuevo de 3,30 V ±2 % (3,23-3,37 V)
cumple ✅ — y es una razón más para el **MAX809S** en vez del T
(ver [DS12_ALIMENTACION.md](DS12_ALIMENTACION.md) §3.6): el supervisor debe disparar
**antes** de que el riel deje de ser válido para USB.

---

## 3. Qué pines están libres hoy (verificado en `DS.txt`) ✅

De los 100 pines de U7, estos aparecen como nets de **un solo nodo o `unconnected-`**:

```
PC13(7)  PC0(15) PC1(16) PC2_C(17) PC3_C(18) PA5(29) PB2(36) PE9(39) PE10(40)
PE13(43) PE14(44) PE15(45) PB15(54) PD14(61) PD15(62) PC6(63) PA9(68) PA10(69)
PA11(70) PA12(71) PD2(83) PD5(86) PD6(87) PB4(90) PE0(97) PE1(98)
```

**PA11 y PA12 existen en el esquemático como nets `/stm32_custom/PA11` y
`/stm32_custom/PA12` con un único nodo: no van a ningún lado.** Son literalmente los pines
del USB, ya ruteados a nada. Y PA9 (que además tiene `OTG_FS_VBUS` como función adicional)
y PA10 (`OTG_FS_ID`) también están libres.

**El puerto USB Full-Speed cuesta cero migraciones de pines.**

---

## 4. Protección del puerto de la PC

Hay dos cosas distintas que se llaman "protección USB" y conviene no mezclarlas:

### 4.1 Protección eléctrica de las líneas de datos
- **ESD**: un **USBLC6-2P6** (SOT-666, LCSC C15999 — el mismo que ya usa el ESP32 en U6) en
  D+/D−, **entre el conector y el STM32**, lo más cerca posible del conector.
- **Choque de modo común** (p. ej. Murata DLW21SN900HQ2, 90 Ω @100 MHz) en el par, **entre
  el conector y el USBLC6**. En una placa con 32 amplificadores clase D esto no es
  cosmética: es lo que impide que el cable USB se convierta en la antena de salida de la
  conmutación. ⚠️ Verificar que no degrade el ojo a FS (a 12 Mbit/s no molesta).
- **Nada de resistencias serie** en D+/D−: el datasheet dice explícitamente que están
  integradas.
- **Impedancia diferencial 90 Ω**, par apareado, sin vías si se puede, sin stubs.
- **Malla del conector**: a GND por **1 MΩ ∥ 4,7 nF**. (En J3 hoy hay sólo R9 = 1 MΩ, sin el
  capacitor — el capacitor es el que da el camino de RF que descarga la malla.)

### 4.2 Protección de la *alimentación* de la PC — la que importa y la que hoy falta

Un dispositivo **autoalimentado** tiene dos obligaciones frente al host:

1. **Nunca inyectar corriente en VBUS.** Es requisito del estándar, y es lo que hoy hacen
   bien D2 y D11: son Schottky con el cátodo del lado de la placa, así que la corriente sólo
   puede entrar. Eso se conserva sin cambios en la topología nueva.
2. **No presentarse como conectado si VBUS no está.** Si el pull-up de D+ está activo sin
   VBUS, el host ve un dispositivo fantasma. Por eso hace falta **sensar VBUS**.

**Recomendación para el puerto nuevo del STM32: que VBUS no alimente absolutamente nada.**

```
VBUS ──┬── 62 kΩ ───┬── PA9  (GPIO de entrada: "hay host")
       │            │
       │          100 kΩ            4,75-5,25 V → 2,93-3,24 V
       │            │   ∥ 100 nF
      GND          GND
```

> 🔧 **Corrección 2026-09-28.** Decía 100 kΩ / 56 kΩ → 1,79 V «nivel alto seguro». **No lo es**:
> el VIH del STM32H743 es **0,7 × VDD = 2,30 V** (DS12110, Tabla 50, testeado en producción; el
> 0,47 × VDD + 0,25 = 1,79 V de la otra fila está sólo garantizado por diseño). Y con VBUS en su
> mínimo de USB, 4,75 V, el divisor viejo daba 1,71 V. Con **62 k arriba / 100 k abajo** —dos
> valores que ya están en el BOM (`C37825`, `C25741`)— PA9 queda entre **2,93 y 3,24 V** en todo el
> rango de VBUS: arriba del VIH y **nunca arriba de VDD**, así que ni siquiera hace falta que el
> pin sea tolerante a 5 V. Con la placa apagada drena 5 V / 162 kΩ ≈ 31 µA.

Ventajas de que ese puerto no tome corriente:
- Es **incondicionalmente seguro para la PC**: no hay ningún camino, ni de falla, por el que
  la placa pueda cargar ese puerto.
- Con la placa apagada, el divisor drena 5 V / 156 kΩ ≈ **32 µA**, clampeados por el diodo
  de ESD del pin: no alcanza para alimentar nada ni para dañar nada.
- Firmware: habilitar el pull-up de D+ (soft-connect del core) **sólo** con VBUS presente.
  Es la conducta que pide el estándar y la que evita el dispositivo fantasma.

> Alternativa de ST: conectar VBUS directo al pin `OTG_FS_VBUS` (PA9 es 5 V tolerante) y
> usar el comparador interno del core. Funciona, pero deja 5 V permanentes en un pin del
> MCU y no aporta nada acá. El divisor + GPIO es más conservador.

**El puerto del ESP32 (J3) se queda como el puerto "que puede alimentar"**, ahora con un
**PTC de 0,75 A** en serie con VBUS (ver DS12_ALIMENTACION §3.4). No es redundante con el
limitador del puerto de la PC: el del host protege a la PC de un consumo excesivo, el PTC
protege a la PC de un **cortocircuito nuestro**, que es la dirección que el host no
controla bien.

> Nota de arrastre buena: con el buck nuevo, en modo "sólo USB" la placa le pide a la PC
> **0,38 A en vez de 0,6 A**, porque el camino pasa de un LDO al 66 % a un buck al 90 %.

### 4.3 Lo que hay que decidir a conciencia: el lazo de masa

Conectar la placa a una PC por USB une la masa de audio de la placa con la masa de la PC, y
por ahí con la tierra de la instalación eléctrica. Con 32 amplificadores y cables de parlante
repartidos por una sala, eso es un lazo de masa real.

- A **Full-Speed** existe una salida limpia: **aislar galvánicamente el puerto** con un
  ADuM4160/ADuM3160. Es un argumento no trivial a favor de FS para un equipo de audio.
- A **High-Speed no hay aislador práctico**. Si el puerto se hace HS, el lazo hay que
  gestionarlo por instalación (misma toma, o interfaz sólo para banco/medición).

---

## 5. Camino A — Full-Speed en PA11/PA12 (recomendado para DS12)

**Costo: un conector, tres componentes, cuatro pines que hoy están muertos. Riesgo: nulo.**

| qué | detalle |
|---|---|
| Controlador | `USB2_OTG_FS` (OTG_HS2), **AF10** |
| Pines | PA11 = D−, PA12 = D+, PA9 = VBUS-sense (GPIO) |
| Conector | segundo USB-C receptáculo, **CC1 y CC2 con 5,1 kΩ a GND** (somos dispositivo) |
| Protección | CM choke → USBLC6-2P6 → STM32; VBUS sólo al divisor |
| Reloj | **HSI48 + CRS** sincronizado al SOF del host ✅ — **no toca PLL3** |
| Alimentación del transceptor | `VDD` (LQFP100 no tiene VDD33USB) → el riel nuevo de 3,30 V ✅ |

**Qué entrega:**
- **UAC2, 4 in / 4 out, 24 bit / 48 kHz** (o 2×2 a 96 kHz, o 8 out a 16 bit). Con
  endpoint de *feedback* explícito, que es lo que hay que hacer (§7).
- **CDC-ACM**: la consola por USB, sin ST-Link y sin WiFi.
- **DFU**: actualizar el firmware del STM32 enchufando un cable. Comparado con el
  bootloader propio en 0x08020000 y el baile del ST-Link, esto solo ya paga el puerto.
- **USB-MIDI**: control por cualquier DAW, complementario al BLE-MIDI del ESP32.
- Todo eso junto, como **dispositivo compuesto** (los 9 endpoints alcanzan de sobra).

**Qué NO entrega: los 8×8 a 24 bits.** Es un techo del estándar, no del diseño.

---

## 6. Camino B — High-Speed con PHY ULPI (exige cambiar de encapsulado)

### 6.1 El bloqueo, verificado pin por pin

El ULPI son 12 señales. Contra la tabla de pines del datasheet, y contra el uso actual:

| señal ULPI | pin | ¿existe en LQFP100? | ¿libre hoy? |
|---|---|---|---|
| ULPI_CK | PA5 (29) | ✅ | ✅ libre |
| ULPI_D0 | PA3 (25) | ✅ | ❌ USART2_RX → J6.6 (**se puede mover a PD6, libre**) |
| ULPI_D1 | PB0 (34) | ✅ | ❌ LED D6 (mover, hay pines de sobra) |
| ULPI_D2 | PB1 (35) | ✅ | ❌ LED D7 (mover) |
| ULPI_D3 | PB10 (46) | ✅ | ❌ I2C2_SCL → J5.1 |
| ULPI_D4 | PB11 (47) | ✅ | ❌ I2C2_SDA → J5.2 |
| ULPI_D5 | PB12 (51) | ✅ | ❌ I2S2_WS → J6.1 |
| ULPI_D6 | PB13 (52) | ✅ | ❌ I2S2_CK → J6.2 |
| ULPI_D7 | PB5 (91) | ✅ | ❌ EEPROM_WP (mover, trivial) |
| ULPI_STP | PC0 (15) | ✅ | ✅ libre |
| **ULPI_DIR** | PC2 **o** PI11 | ❌ **ninguno de los dos** | — |
| **ULPI_NXT** | PC3 **o** PH4 | ❌ **ninguno de los dos** | — |

Los dos últimos son el muro. Verificado en la tabla 9 del datasheet ✅:

- **PC2 y PC3 digitales no existen en LQFP100 ni en LQFP144.** Lo que sí existe son
  `PC2_C` (pin 17) y `PC3_C` (pin 18), que son pads **analógicos** separados.
- La nota 4 de la tabla dice que las funciones alternas de PCx *sí* están disponibles en
  `PCx_C` **cerrando el switch analógico interno**. Técnicamente se podría. **No hay que
  hacerlo**: la tabla 103 del datasheet especifica para ULPI `tSC = 0,5 ns` de setup y
  `tHC = 6,5 ns` de hold sobre un reloj de 60 MHz (16,67 ns de período). Meter un switch
  analógico no caracterizado en el camino de DIR y NXT es exactamente el tipo de riesgo que
  no se toma en una placa definitiva.
- `PI11` y `PH4` (las alternativas) **aparecen recién en LQFP176 / LQFP208 / UFBGA169**.

### 6.2 Qué implica entonces

| | |
|---|---|
| MCU | **STM32H743IIT6** — sufijo `I` = 176 pines, `T` = LQFP ✅ (24×24 mm, paso 0,5 mm) |
| PHY | USB3300 (QFN-32) o USB3343 (QFN-24), + cristal de 24 MHz ⚠️ verificar stock JLCPCB |
| Pines | los 12 del ULPI + reset del PHY |
| Trabajo | footprint nuevo, **pin map completo desde cero**, re-ruteo total, `pin_map.md` y todo el firmware de inicialización de GPIO a revisar |
| Riesgo | alto: es un bring-up nuevo, no una revisión |
| Lo que además se gana | FMC/SDRAM, más SAI, PC2/PC3, decenas de pines libres para el multinodo |
| Lo que se pierde | la posibilidad de aislar galvánicamente el USB (§4.3) |

### 6.3 Recomendación

**Separar las dos decisiones y no mezclarlas en la misma tirada.**

La DS12 tiene un objetivo que hoy bloquea todo lo demás: **que la placa funcione sola**,
que es lo que arregla [DS12_ALIMENTACION.md](DS12_ALIMENTACION.md). Meter en la misma vuelta
un cambio de encapsulado del MCU convierte una revisión acotada y verificable en un bring-up
completo, y el historial de este proyecto dice que los bring-ups completos consumen meses.

Concretamente:

1. **DS12 = arreglo de alimentación + USB Full-Speed en PA11/PA12.** Cuesta casi nada,
   no toca ningún pin en uso, y trae DFU + consola + control + 4×4 de audio. Además es la
   forma de **validar toda la pila de USB Audio con hardware real** (descriptores, feedback,
   drivers en macOS/Windows, latencia) antes de comprometerse con la migración.
2. **La 8×8 real es una decisión de encapsulado**, y conviene tomarla cuando la pila UAC2
   ya esté funcionando en FS. Si ahí sigue siendo un requisito, la migración a LQFP176 se
   hace sabiendo exactamente qué se está comprando.

Si aun así se decide hacer HS ahora, entonces **hay que hacerlo completo**: LQFP176, PHY,
pin map nuevo — y no intentar el atajo del switch analógico en PC2_C/PC3_C.

---

## 7. Reloj y sincronismo: el punto fino de una interfaz de audio

El reloj de audio de la placa es **PLL3 desde el cristal HSE, corriendo libre** (24,576 MHz
= 512×fS), y el SAI es el maestro. El host USB tiene su propio reloj. Los dos no están
relacionados, así que la clase de audio hay que implementarla en **modo asíncrono con
endpoint de feedback explícito**: el dispositivo le informa al host, cada trama, la tasa
real a la que consume muestras, y el host ajusta el tamaño de los paquetes.

- Es exactamente lo que hace una interfaz profesional. La alternativa (modo *adaptive*, o
  sincronizar PLL3 al SOF) **degradaría el jitter del reloj de audio**, que es justo lo que
  no se quiere tocar en esta placa.
- Cuesta un endpoint IN isócrono extra de 3 bytes por trama (FS) — despreciable.
- Implica paquetes OUT de tamaño variable (p. ej. 4 o 5 muestras de más/menos por trama):
  el buffer de recepción tiene que tolerarlo.

**Para el reloj del propio USB FS: HSI48 + CRS**, disciplinado por el SOF del host ✅. Es la
opción correcta acá justamente porque **no obliga a tocar la configuración de PLL3**, que
está calibrada y verificada (`PLL3P=14`, FRACN=6660, y el descubrimiento del single/dual
rate del PCM1690 detrás). La otra opción del datasheet —48 MHz desde un PLL sobre HSE—
funciona pero mete una restricción nueva en un árbol de relojes que ya costó cerrar.

---

## 8. Firmware: por dónde se hace

- **TinyUSB** sobre el driver `dwc2`, no la librería USB de ST. La de ST sólo trae UAC1
  (estéreo, sin feedback decente); TinyUSB tiene clase UAC2 con feedback y soporta
  dispositivo compuesto. ⚠️ Los descriptores multicanal hay que escribirlos a mano.
- **UAC2 exige Windows 10 1703 o superior** (driver nativo). macOS, iOS y Linux: nativo
  desde siempre. UAC1 no sirve para 8 canales.
- **Buffers en RAM_D2**, como los del SAI: el DMA no llega a DTCM (descubrimiento #1 del
  proyecto, sigue aplicando).
- **Control**: la mitad "controlable" de la idea se resuelve en dos capas —
  *Feature Units* de UAC2 para volumen/mute por canal (que el sistema operativo ya sabe
  manejar), y una interfaz **CDC o MIDI** en el mismo dispositivo compuesto para lo
  específico (ángulo del haz, modo, foco). El protocolo binario que ya existe sobre UART4
  se puede reusar tal cual sobre CDC.
- El pipeline de beamforming ya tiene el presupuesto de CPU resuelto (`ds11_perf.md`: 43×
  de mejora con I-cache + `-O2`), así que sumar copias de USB no debería moverlo. ⚠️ Medir.

---

## 9. Cambios de hardware concretos para el Camino A

1. Conector **USB-C receptáculo #2** (mismo modelo que J3 para no sumar footprints), con
   CC1/CC2 → 5,1 kΩ a GND, malla → 1 MΩ ∥ 4,7 nF a GND.
2. **Choque de modo común** en D+/D−, pegado al conector.
3. **USBLC6-2P6** (C15999) después del choque.
4. D+ → **PA12**, D− → **PA11**. Sin resistencias serie. Par de 90 Ω, apareado, corto.
5. VBUS → **divisor 62 k / 100 k + 100 nF** → **PA9** (🔧 no 100 k / 56 k: ver §5). Sin camino de alimentación.
6. Dejar **PB14/PB15 sin usar** si es posible (hoy PB14 = I2S2_SDI hacia J5.5): mantiene
   `USB1_OTG_HS` entero y libre por si alguna vez se hace el Camino B.
7. Serigrafía que distinga los dos puertos: **`AUDIO/DFU` (STM32)** y **`ESP32`**.
