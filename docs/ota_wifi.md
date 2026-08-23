# Actualizar el firmware por WiFi

Subir un firmware nuevo desde el navegador, sin cable, sin desarmar nada. Vale
para **los dos chips**: el STM32 (~11 s) y el ESP32 (~5 s). Se elige el `.bin` en
la página de `domosonica.local`, se aprieta *Actualizar* y el equipo se reinicia
con el firmware nuevo.

Este documento explica cómo funciona, **por qué no puede romper el
microprocesador**, y qué es lo único que sigue necesitando el cable.

---

## 1. Lo primero: qué hay que hacer una sola vez

El OTA necesita un **bootloader** instalado en la placa. Es un programa chiquito
(10 kB) que vive en los primeros 128 kB de la flash y cuyo único trabajo es
aplicar las actualizaciones. Se graba con el ST-Link, una vez, y no se toca
nunca más.

Con la placa conectada al ST-Link:

```bash
cd firmware
make flash-todo        # bootloader + aplicación, en ese orden
```

El ESP32 también necesita **un** flasheo por cable (USB-C, J3) para instalar la
tabla de particiones con dos ranuras — ver §9:

```bash
cd ../firmware_esp32c3
idf.py -p /dev/cu.usbmodemXXXX erase-flash    # cambió la tabla de particiones
idf.py -p /dev/cu.usbmodemXXXX flash
```

Eso es todo, y es la última vez que hace falta un cable. A partir de ahí:

```bash
cd firmware
make ota        # STM32   (por WiFi)
make ota-esp    # ESP32   (por WiFi)
```

o desde el navegador: al final de `http://domosonica.local` hay una tarjeta
**Firmware** con un selector de archivo para cada chip, y un aviso que dice cuál
hay que actualizar cuando los dos no coinciden (§10).

> **Ojo con las direcciones.** Desde ahora la aplicación **no** empieza en
> `0x08000000` sino en **`0x08020000`**. `make flash-stlink` ya lo hace solo.
> Si alguna vez grabás a mano, la dirección es ésa. Grabar la aplicación en
> `0x08000000` no rompe nada —arranca igual, porque se pisa el bootloader— pero
> te quedás sin OTA hasta volver a hacer `make boot-flash`.

---

## 2. Cómo está repartida la flash

El STM32H743 tiene **2 MB de flash en dos bancos independientes de 1 MB**, y el
firmware entero ocupa 83 kB. Sobra tanto lugar que se puede tener una copia
completa del firmware nuevo esperando sin molestar a la que está corriendo.

```
banco 1  ┌─ 0x08000000 ─────────── 128 kB ─┐  BOOTLOADER
         │                                  │  se graba con ST-Link, nunca por WiFi
         ├─ 0x08020000 ─────────── 896 kB ─┤  APLICACIÓN
         │                                  │  acá corre el firmware (usa 83 kB)
banco 2  ├─ 0x08100000 ────────── 1024 kB ─┤  STAGING
         │                                  │  acá aterriza la imagen que llega por WiFi
         └──────────────────────────────────┘
```

El recorrido de una actualización:

```
navegador ──POST /firmware──> ESP32 ──UART, de a 126 bytes──> STM32
                                                                │
                                             escribe en el BANCO 2 ─┘
                                             verifica CRC32
                                             escribe la cabecera
                                             se reinicia
                                                                │
   bootloader: ¿hay cabecera? ¿cierra el CRC? ¿está linkeada acá? ─┘
               → borra la app, copia banco 2 → banco 1, verifica
               → borra la cabecera y salta al firmware nuevo
```

---

## 3. Por qué esto no puede romper el micro

Ésa fue la condición de diseño, así que vale la pena ser explícito. Hay cuatro
cosas que lo garantizan, y cada una tapa un modo de falla distinto.

### 3.1 No se tocan los option bytes

Es lo único de este chip que puede quedar **irreversiblemente** mal: un RDP mal
escrito deja el micro cerrado para siempre, sin ST-Link que valga.

El camino "de manual" para un OTA en un STM32 dual-bank es `SWAP_BANK`, que es
justamente un option byte. **Acá no se usa.** No se escribe un solo option byte
en ningún momento, ni en la aplicación ni en el bootloader.

*(El otro camino de manual, el bootloader ROM de ST, tampoco sirve: se entra
moviendo BOOT0 y NRST, y en la DS11 ninguno de los dos llega al ESP32 — BOOT0 va
a C49/K3/R14/R21/U7-94 y NRST a C36/C52/IC1/J7-6/K4/U7-14. Sólo los botones y el
conector del ST-Link.)*

### 3.2 La aplicación no puede escribir donde corre

El firmware que recibe la imagen **sólo sabe escribir el banco 2**. El chequeo
está en un único lugar (`stage_range_ok` en `firmware/Core/Src/ota.c`) y el
número de banco es una constante, no una variable.

Aunque el ESP32 mandara basura, aunque la imagen viniera corrupta, aunque el
tamaño anunciado fuera mentira: lo peor que puede pasar es que quede basura en un
banco que nadie ejecuta. **El firmware que está corriendo es inalcanzable desde
ahí.**

### 3.3 La copia es idempotente

El único momento en que se escribe sobre el firmware que anda es dentro del
bootloader, y ese paso es **repetible**:

- La cabecera del staging se borra **después** de que la copia se verificó.
- Si se corta la luz en el medio, al arrancar de nuevo el bootloader encuentra la
  cabecera todavía puesta y **vuelve a copiar desde cero**.
- Si la copia no verifica, se reinicia y reintenta en vez de saltar a una
  aplicación a medio escribir.

No hay ningún estado intermedio del que no se salga solo.

### 3.4 Se verifica antes de tocar nada

Antes de borrar un solo byte del banco 1, el bootloader comprueba dos cosas
sobre la imagen que está en el banco 2:

1. **CRC32** de la imagen entera contra el que declaró el emisor. Si no cierra,
   no toca nada y arranca la aplicación que ya estaba.
2. **Que esté compilada para correr en `0x08020000`.** Las dos primeras palabras
   de un firmware ARM son el puntero de pila y el `Reset_Handler`; si el segundo
   no cae dentro del hueco de la aplicación, la imagen no es para esta
   dirección.

   Esto ataja el error más probable de toda la transición: subir un `.bin` viejo,
   de cuando la aplicación arrancaba en `0x08000000`. Ese archivo tiene un CRC
   perfectamente válido y se copiaría sin una queja, pero todas sus direcciones
   absolutas apuntarían 128 kB más abajo y el equipo quedaría arrancando en un
   HardFault sin ninguna pista.

Además, si la cabecera quedó escrita a medias (por ejemplo si se cortó la
alimentación en los ~100 µs que tarda grabarla), leerla produce un error de ECC
que en el H7 es un *bus fault*. El bootloader **atrapa ese fault** y arranca
normalmente ignorando la actualización.

---

## 4. Lo único que esto NO cubre

**Un firmware que compila, se sube, verifica, copia — y después no arranca.**

Si el binario tiene un error que lo cuelga antes de llegar al lazo principal, el
equipo se queda sin red y hay que enchufar el ST-Link:

```bash
cd firmware && make flash-stlink
```

Dos cosas para poner esto en perspectiva:

- **No es peor que hoy.** Flashear por cable un firmware que se cuelga deja el
  equipo exactamente igual. El OTA no agrega este riesgo, simplemente no lo
  elimina.
- **Si arranca, se puede volver a actualizar.** El único caso sin salida remota
  es "no llega al lazo principal", que es raro y suele notarse compilando.

Si alguna vez el equipo va a una instalación permanente sin acceso físico, la
respuesta es una **vuelta atrás automática**: el bootloader guarda la imagen
anterior y la restituye si el firmware nuevo no confirma que arrancó bien dentro
de N segundos. Está pensado pero no implementado para el STM32, porque hoy el
ST-Link está siempre a mano.

**El ESP32 sí la tiene** — ver §9. La diferencia no es capricho: en el ESP32
viene hecha con ESP-IDF, y además ese chip es el que tiene la red, así que
perderlo cuesta mucho más.

---

## 5. Durante la actualización

**El equipo se queda mudo.** No es un efecto secundario: es a propósito.

Borrar un sector de flash bloquea el CPU varios cientos de milisegundos, y eso
son decenas de deadlines de audio perdidos. En vez de dejar que eso salga por
los parlantes como una ráfaga de pops, el firmware **apaga los amplificadores**
apenas empieza la carga. Vuelven apagados después del reinicio (arrancan siempre
así) y se prenden a mano como siempre.

Los contadores de *underruns* van a subir durante la carga. Es esperado.

En la terminal serie se ve todo el recorrido:

```
OTA: recibiendo 83276 bytes (id 4a7c1e02) — amps apagados
OTA: imagen verificada. Reiniciando para aplicarla...
OTA: reiniciando.

=== DS11 boot ===
BOOT: actualizacion pendiente, 83276 bytes, crc 0x971F8A82
BOOT: verificando staging... ok
BOOT: aplicando (no cortar la alimentacion)...
BOOT: actualizacion aplicada, 83276 bytes
BOOT: -> app

=== DS11 Beamforming Audio (32ch) ===
firmware 4A7C1E02  build 1755950000  @ 0x08020000
```

El LED **D4** queda encendido mientras el bootloader copia (aproximadamente un
segundo). Si alguna vez lo ves parpadear rápido y quedarse así, el bootloader no
encontró una aplicación válida: hay que grabarla con el ST-Link.

En un arranque normal, sin actualización pendiente, todo esto son microsegundos:

```
=== DS11 boot ===
BOOT: sin actualizacion pendiente
BOOT: -> app
```

---

## 6. Qué firmware está corriendo

Con OTA, "el último que flasheé" deja de ser una respuesta: la imagen pudo
haberla subido otra máquina, o vos mismo hace tres semanas. Por eso cada
compilación lleva su identidad adentro:

- **`fw_id`** — el hash git corto del commit.
- **`fw_build`** — el momento de la compilación.

Salen por `/status`, se ven en la página (`corriendo: 4a7c1e02 · compilado …`) y
en el banner de arranque. Los genera el Makefile en cada `make`, recompilando
`Core/Src/fw_version.c` sin condiciones — si fueran macros dentro de `main.c`
quedarían congeladas hasta la próxima vez que ese archivo cambiara, y un número
de versión que a veces miente es peor que no tener ninguno.

Un `fw_id` en `00000000` significa que se compiló sin git a mano.

---

## 7. El protocolo, para cuando haya que tocarlo

Cuatro comandos nuevos sobre el enlace UART que ya existía (`PROTO_VERSION 8`):

| comando | payload | quién contesta |
|---|---|---|
| `CMD_FW_BEGIN` 0x20 | `ds_fw_begin_t` (12 B: len, fw_id, build) | el **main loop**, tras borrar |
| `CMD_FW_DATA` 0x21 | `[nº de tramo: 2][datos: 1..126]` | el **main loop**, tras grabar |
| `CMD_FW_END` 0x22 | `uint32` CRC32 de la imagen | el **main loop**, tras verificar |
| `CMD_FW_ABORT` 0x23 | — | el parser, en el acto |

Los tres primeros **no se contestan desde el parser**, y eso no es un detalle de
implementación: **el ACK diferido es el control de flujo**. Mientras el ESP32
espera el ACK de un tramo no manda el siguiente, así que la transferencia va
exactamente al ritmo al que el STM32 puede grabar. Sin eso habría que inventar
una ventana y un buffer, y el borrado de un sector los desbordaría igual.

De ahí que los tiempos de espera sean tan distintos entre sí: `FW_BEGIN` puede
tardar segundos porque borra, `FW_DATA` milisegundos, `FW_END` otra vez segundos
porque relee la imagen entera para el CRC.

El número de tramo tampoco es paranoia: sin él, un ACK que se pierda hace que el
ESP32 reenvíe el mismo tramo y el STM32 lo escriba **dos veces**, corriendo todo
lo que sigue medio tramo — con el CRC fallando al final y sin ninguna pista de
dónde. Con él, un reenvío se reconoce y se vuelve a confirmar sin escribir nada.

### El abrazo mortal del primer día

Las tres primeras cargas murieron en un tramo distinto cada vez —51, 55, 33— con
el ESP32 informando *"el STM32 dejó de contestar"*. El STM32 no tenía la culpa:
los contadores de diagnóstico dieron `tx_fail=0`, `busy_nack=0` y `ack_sent`
congelado, y el contador de bytes recibidos dejaba de subir. **No es que no
contestara: es que dejaban de llegarle tramos.**

Eran dos problemas del ESP32, los dos invisibles hasta que existió el OTA:

1. **`stm32_monitor_task` es el único que lee el UART**, y por lo tanto el único
   que puede ver un ACK. Cada 10 s publica su IP al STM32, y para eso tomaba el
   lock de transmisión **esperando indefinidamente**. Pero
   `uart_send_and_wait_ack` retiene ese lock durante *toda* la transacción,
   incluida la espera del ACK. Si el push caía ahí, el lector se quedaba
   esperando el lock, dejaba de leer, y el ACK del STM32 llegaba sin que nadie
   lo viera. Plazo vencido, reintento, y a veces otra vez lo mismo.

   Arreglo: el push toma el lock **sin esperar** y si está ocupado se saltea. Es
   periódico y sólo informativo — perderse uno no cuesta nada; bloquear al
   lector, sí.

2. **`uart_read_bytes` no vuelve hasta juntar los bytes que le pediste** o agotar
   el plazo. El lector pedía 512 bytes con 100 ms de plazo, y una respuesta del
   STM32 son 5 bytes: **se comía los 100 ms enteros, siempre**. Con una consulta
   de estado por segundo eso no se notaba. Con ~660 tramos que esperan cada uno
   su ACK, son más de un minuto de puro plazo vencido. Medido: **9 tramos/s
   cuando el cable da 80**.

   Arreglo: esperar el **primer** byte y después vaciar lo que ya esté en el
   buffer sin esperar más.

Con los dos arreglos, una carga de 83 kB tarda **10,6 s** y la flash queda byte a
byte igual al archivo subido. La moraleja, que vale para todo el enlace: *un
patrón de lectura pensado para una consulta por segundo no sobrevive a
seiscientas transacciones seguidas.*

**CRC-32 de zlib** (poly reflejado `0xEDB88320`), implementado por separado en
los dos lados a propósito, en vez de usar el de la ROM del ESP32: las variantes
de la ROM difieren en si el valor entra y sale invertido, y un desacuerdo
silencioso ahí haría fallar *todas* las cargas con "el CRC no coincide" y
ninguna pista de por qué. Es el mismo que calcula `zlib.crc32` en Python, así
que una imagen se puede verificar a mano desde la PC.



---

## 9. El otro chip: OTA del ESP32

Es un mecanismo **distinto y más simple**, porque ESP-IDF ya lo trae hecho: su
bootloader de segunda etapa elige entre dos ranuras de aplicación según lo que
diga la partición `otadata`, y `esp_ota_*` escribe la que no se está ejecutando.
No hubo que escribir ningún bootloader.

```
0x000000  bootloader de ESP-IDF
0x008000  tabla de particiones
0x00d000  otadata            ← dice cuál de las dos ranuras arranca
0x010000  ota_0   1,95 MB    ┐ la aplicación (890 kB) usa menos de la mitad
0x200000  ota_1   1,95 MB    ┘ de la suya
```

> **El módulo siempre tuvo 4 MB de flash y estaba configurado como si tuviera 2.**
> La mitad estaba sin usar y era invisible para el sistema. Se descubrió el
> 2026-08-23 justo al ir a hacer lugar para la segunda ranura — con 2 MB entraba
> raspando, con 4 MB entra holgado.

### Vuelta atrás automática, y por qué importa acá

El ESP32 es el chip que **tiene la red**. Si se rompe, se pierde el acceso remoto
a los dos. Por eso este OTA lleva `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`: la
imagen nueva arranca *a prueba* y tiene que declararse buena. El criterio es el
único que importa para este chip — **se conectó a la red**:

- se conectó → se confirma y queda como la buena;
- no se conectó en 90 s → se marca inválida y **se reinicia a la anterior**, sin
  esperar a que nadie vaya a apretar un botón.

Esto dejó de ser teórico en la primera prueba: la versión inicial lanzaba la
tarea de confirmación *antes* de `wifi_init()`, o sea esperando sobre un grupo de
eventos que todavía era `NULL`. Entró en pánico, reinició, y **el bootloader
restauró la imagen anterior sola**. El equipo nunca salió de la red.

Lo revelador es que ese bug **no aparecía al flashear por cable**: una imagen
grabada por cable no está *a prueba*, así que la tarea salía antes de tocar nada.
Sólo se manifestó en la primera actualización por WiFi de verdad — que es
precisamente el escenario para el que existe la vuelta atrás.

### Qué firmware corre

`esp_sha` — los primeros 9 caracteres del SHA-256 del ELF. **No** la fecha de
compilación: `esp_app_desc.date`/`.time` salen de `__DATE__`/`__TIME__` de un
archivo de ESP-IDF que no se recompila salvo que cambie él, así que quedan
congelados en la primera vez. Medido: dos binarios distintos reportaban la misma
hora al segundo.

El SHA del ELF cambia con cada binario distinto y **no** cambia si el binario es
el mismo — que es exactamente la propiedad que se le pide a un identificador.
Junto a él va `esp_fw`, el `git describe` del árbol.

---

## 10. Cuál actualizar, y en qué orden

**No hace falta acordarse: la página lo dice.** Los dos chips publican con qué
versión de protocolo fueron compilados —`proto_ver` el STM32, `proto_esp` el
ESP32— y la tarjeta *Firmware* muestra un aviso según el caso:

| situación | qué dice |
|---|---|
| las dos versiones coinciden | ✓ protocolo vN en los dos |
| el ESP32 está más nuevo | ⚠ **falta actualizar el STM32** — botón de arriba |
| el STM32 está más nuevo | ⚠ **falta actualizar el ESP32** — botón de abajo |
| el STM32 no contesta | ⚠ revisá alimentación, o cargalo por ST-Link |

Para que eso funcione hubo que arreglar algo que estaba de antes: el ESP32
**descartaba** una trama de estado cuyo largo no fuera exactamente el suyo, y la
página mostraba `stm32: timeout` — o sea **lo mismo que muestra si la placa está
muerta**. Un desajuste de firmwares, que es el estado normal mientras se
actualiza uno de los dos, era indistinguible de una falla de hardware. Ahora se
acepta cualquier largo razonable, se copia el prefijo común y se compara: el
primer byte del status es `proto_ver`, así que una trama de otro largo igual dice
con qué versión fue hecha.

### Cuando hay que actualizar los dos

**Primero el STM32, después el ESP32.** Dos razones:

1. El ESP32 es el que **transporta** la carga hasta el STM32. Conviene tenerlo
   estable y conocido hasta el último paso: si lo actualizás primero y la imagen
   nueva tiene un problema sutil con el enlace UART, te quedaste sin camino al
   STM32.
2. El del ESP32 es el que **se deshace solo** si sale mal. Es el mejor candidato
   para dejar al final.

Desde la consola es lo mismo, en ese orden:

```bash
cd firmware
make ota        # STM32 primero
make ota-esp    # ESP32 después
```

---

## 11. Archivos

| archivo | qué es |
|---|---|
| `firmware/Core/Inc/ota.h` | mapa de flash, cabecera del staging, API. El razonamiento largo está acá. |
| `firmware/Core/Src/ota.c` | lado aplicación: recibe y escribe el banco 2 |
| `firmware/Core/Src/boot_main.c` | el bootloader completo |
| `firmware/STM32H743XX_BOOT.ld` | linker del bootloader (0x08000000, 128 kB) |
| `firmware/STM32H743XX_FLASH.ld` | linker de la aplicación (**0x08020000**, 896 kB) |
| `firmware/Core/Src/fw_version.c` | identidad del binario; se recompila siempre |
| `firmware_esp32c3/main/main.c` | endpoints `POST /firmware` y `/firmware-esp`, la tarjeta de la página y el aviso de desajuste |
| `firmware_esp32c3/partitions.csv` | las dos ranuras del ESP32 (4 MB) |

| comando de make | qué hace |
|---|---|
| `make boot` | compila el bootloader |
| `make boot-flash` | lo graba con ST-Link en 0x08000000 |
| `make flash-stlink` | graba la aplicación con ST-Link en **0x08020000** |
| `make flash-todo` | los dos, en orden — instalación desde cero o rescate |
| `make ota` | sube el firmware del **STM32** por WiFi (`OTA_HOST=domosonica.local`) |
| `make ota-esp` | sube el firmware del **ESP32** por WiFi (compilarlo antes con `idf.py build`) |
