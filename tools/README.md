# Herramientas

Dos familias, y conviene no confundirlas:

- **Medición acústica** (las ocho de abajo + `sweep_focus.sh`): hablan con el ESP32 por
  HTTP, con la placa encendida y sonando.
- **Banco del firmware** (`banco_fase2.py`): prueba un firmware nuevo en una placa, sin
  sonido y con los parlantes desenchufados.
- **Placa, madera y repositorio** (`check_pcb.sh`, `check_layout.py`, `sch2net.py`, `pcb_status.py`,
  `netlist_digest.py`, `netlist_diff.py`, `geometria_array.py`): trabajan sobre los
  archivos del proyecto —el KiCad y el firmware—, sin tocar hardware. Están al final.

Las de medición no necesitan el cable serie, y **ninguna enciende los amplificadores por su
cuenta** — encender 32 amplificadores es una acción física y la decide una persona.

```
curl -X POST http://192.168.100.77/amps -d '{"on":true}'
```

## ⚠️ Antes de nada: los modos que hacen acople

Con micrófonos enchufados en J9, los modos **0, 2–6 y 15** llevan la entrada del ADC a los
32 parlantes. Con los micrófonos a 1 m eso es acople inmediato, y **no hay limitador ni AGC
en ninguna parte del firmware**.

Modos seguros con amplificadores encendidos: **1, 7–14, 16–23** — todos emiten un tono o un
chirp interno y el micrófono sólo entra al detector.

`nivel.py` y `abx.py` traen la lista y se niegan a entrar. Cualquier herramienta nueva tiene
que hacer lo mismo.

Y una trampa de la API: **`POST /reset` no reinicia el chip.** Manda `CMD_FACTORY_RESET` —
devuelve a fábrica el volumen, la ganancia, el modo y los gains/mutes— y deja la placa en
**modo 0**, que es el del acople. Para reiniciar de verdad: `make -C firmware ota OTA_HOST=…`.

---

## El orden de una sesión

| | herramienta | qué decide |
|---|---|---|
| **0** | `nivel.py` | si vale la pena seguir. Con menos de 20 dB de margen sobre el piso, todo lo demás mide ruido |
| **1** | `perfil.py --chirp` | si la cadena entera está bien. Tiene **respuesta conocida**: los dos micrófonos tienen que salir separados lo que dice la cinta |
| **2** | `focos.py --medir` | dónde están los puntos, para usarlos después como focos |
| **3** | `fase.py` | si los 32 llegan en fase, y si un retardo por parlante alcanza |
| **4** | `imagen.py` | la forma del foco en el espacio: ancho, alto y profundidad |

El paso 1 va antes que el 3 por un motivo: si el foco no se está moviendo —si lo que cambia
con la posición es el volumen y no la dirección— entonces todo lo que diga el análisis de
fase es sobre un array que no está formando nada.

---

## Las de medición, una por una

### `nivel.py` — poner nivel sin acople
Mide el piso con el volumen en mute, sube el volumen **de a poco** desde ahí, y dice qué
ganancia poner. Nunca entra a un modo con lazo.
```
python3 tools/nivel.py --host 192.168.100.77 --canales 0 1
```

### `perfil.py` — perfil del haz, ancho y contraste
Mueve el foco y mide los dos micrófonos a la vez. Con `--chirp` usa el modo 23 y mide el
sonido **directo**, sin la sala: en una sala chica es la diferencia entre 2,8 y 17,7 dB de
contraste.
```
python3 tools/perfil.py --host … --chirp --mics 0 1 --desde -600 --hasta 400 --sep 247 --grafico
```
**Su prueba de aceptación tiene respuesta conocida:** los dos micrófonos ven el mismo
barrido, así que sus perfiles son la misma curva corrida por la separación física. Si da
cero, el foco no se mueve y lo que cambia es el volumen.

### `imagen.py` — imagen de haz, como un radar
Barre el foco por una grilla del espacio y mapea el nivel. Donde el mapa tiene su máximo
está el micrófono — no por cálculo, sino porque ahí es donde el array concentra.
```
python3 tools/imagen.py --host … --plano xz --x -500 300 50 --z 500 1600 100
python3 tools/imagen.py --host … --plano xy --fijo 1000
```

### `calfase.py` — el retardo propio de cada parlante
Separa `τ_k` del camino geométrico ajustando por mínimos cuadrados la posición del
micrófono y el offset de disparo sobre los 32 tiempos de vuelo del AUTOFOCUS.
```
python3 tools/calfase.py --host … --pares      # ¿está cruzado el arnés?
python3 tools/calfase.py cal.json --pares-fijo --sep 250
python3 tools/calfase.py --autoprueba          # recupera 250 mm con 0,3 mm de error
```

### `fase.py` — fase de cada parlante, frecuencia por frecuencia
Analiza el modo 22. Dice cuánto se desalinean los 32 en cada frecuencia, si un retardo por
parlante alcanza para corregirlo, y **distingue una polaridad invertida de un error de
retardo** — una es constante y el otro crece con f.
```
python3 tools/fase.py --host … --pares --ch 1 --mic 14 -46 954 --mapa
python3 tools/fase.py --autoprueba             # 22 comprobaciones de respuesta conocida
```

### `focos.py` — capturar posiciones y usarlas como focos
Poner los micrófonos donde se quieren los focos, medir, sacarlos, y poner cada fuente en su
punto. Guarda **posiciones**, no retardos: una posición vale para cualquier fuente y
cualquier frecuencia.
```
python3 tools/focos.py --host … --medir izquierda centro
python3 tools/focos.py --host … --aplicar izquierda centro
```

### `abx.py` — escucha a ciegas, con puntaje
Cuando uno sabe cuál espera que sea mejor, lo escucha mejor. Acá la máquina elige al azar y
cuenta, y al final sale un p-valor. **Con `--trampa`, algunas tiradas ponen la misma
configuración en las dos mitades**: si igual se percibe una diferencia, lo que se está
oyendo es el cambio y no la configuración.
```
python3 tools/abx.py --host … --que arnes --trampa
python3 tools/abx.py --host … --que foco --a 14 -46 954 --b -233 -52 948 --trampa
```

### `captura.py` — las ocho entradas crudas
Baja las muestras a un `.wav` de 8 canales para mirar cualquier otra cosa.
```
python3 tools/captura.py --host … -n 8192
```
El `.wav` es un dato de una medición puntual y **está fuera de git**. Si una captura vale
la pena, va a `docs/datos/` con nombre propio y con el `.md` que dice qué se estaba midiendo.

### `sweep_focus.sh` — el barrido a oído (el abuelo de `perfil.py`)
Mueve el foco en X y uno escucha, parado y quieto, si el nivel sube al pasar por su
posición. Es anterior al banco de micrófonos y no mide nada: sirve para un chequeo rápido
sin instrumental.
```
ESP=http://192.168.100.77 STEP=100 ./tools/sweep_focus.sh 800
```
⚠️ Su IP por default (`192.168.100.143`) es vieja: pasale `ESP=` siempre.

---

## Banco del firmware

### `banco_fase2.py` — la prueba de la fase 2 del plan DS12, en una DS11
La fase 2 (identidad de placa y autoarranque) está escrita pero nunca corrió en una placa.
Su prueba (`docs/ds12_firmware_plan.md`) es una lista de cosas para mirar en el log y en
`/status`. El script hace esa lista entero:
- revisa `/status`: protocolo 23 en los dos chips, DS11, sin `board_mismatch`, y `v5a` y
  `usb_vbus` en null;
- con `--stlink`, `make reset` tiene que volver con `RESET: SFT`;
- con `--ota`, reinstala el mismo firmware y tiene que volver con SFT y el `fw_id` del repo;
- con `--barreras`, el firmware de la DS12 tiene que rebotar en el Makefile y con un 409
  del ESP32, sin que la placa se reinicie;
- con `--serie`, lee el log del ST-Link y verifica en cada arranque las líneas `PLACA:` y
  `RESET:` y el readback del AK5538.

**No toca los amplificadores ni el autoarranque**, y no llama a `POST /reset`. Igual, se
corre con los parlantes desenchufados. Todo lo crudo queda en `.ds12_audit/banco/`,
incluido el `reset_rsr` de la medida M8.
```
tools/banco_fase2.py --host 192.168.100.77 --serie /dev/cu.usbmodemXXXX --stlink --ota --barreras
tools/banco_fase2.py --autoprueba        # sin placa, contra una placa falsa en localhost
```
**Su autoprueba:** placas falsas que contestan como el ESP32.
- La sana da ✅.
- Dan ❌: el jumper equivocado, un protocolo viejo, una DS12, el STM32 mudo y los
  amplificadores encendidos.
- También da ❌ un ESP32 que acepta el firmware de la DS12, o que contesta 409 pero igual
  se reinicia.
- Un banner vacío da ❌, nunca ✅.

No cubre `make`, el puerto serie real ni el ST-Link: eso se ve en la primera corrida.

La prueba "OTA a mitad de un autoarranque" necesita los amplificadores encendidos por el
autoarranque. Lo arma una persona: el script sólo detecta que está en curso y entonces
exige la línea de descuento.

---

## Placa, madera y repositorio

Estas no hablan con la placa: leen los archivos del proyecto. Y **ninguna necesita
KiCad instalado salvo `check_pcb.sh` y `check_layout.py`**, así que la revisión de lo que
se está dibujando se puede hacer desde cualquier máquina —incluida la nube— sin exportar
nada a mano.

### `check_pcb.sh` — que no salgan gerbers con el cobre viejo
KiCad **no recalcula los rellenos de zona al plotear**: usa el cobre que está guardado
dentro del `.kicad_pcb`. Si ese cobre es anterior a las últimas pistas, los gerbers salen
con planos que nunca abrieron el clearance, y **no se ve en pantalla**, porque pcbnew te
muestra lo que acaba de recalcular.

Ese mecanismo ya arruinó dos tandas de la DS11 y en septiembre apareció una tercera vez,
latente: las vías de `I2C1_SCL`/`I2C1_SDA` con 0,0000 mm contra el plano de masa.

Corre el DRC dos veces sobre el mismo archivo —tal como está guardado y con
`--refill-zones`— y si el primero tiene más violaciones que el segundo, el cobre guardado
está viejo.
```
tools/check_pcb.sh              # verifica y explica
tools/check_pcb.sh --fix        # ademas refillea y guarda
tools/check_pcb.sh --install    # lo deja como hook de pre-commit
```
**`--install` hay que correrlo una vez por copia del repo**: `.git/hooks/` no se versiona.

Y al exportar: `kicad-cli pcb export gerbers --check-zones`, que viene apagado por default.

### `check_layout.py` — las reglas de layout que el DRC no mira
El DRC contesta "¿se tocan cosas que no deberían?". No contesta "¿este buck va a
irradiar?": una placa puede tener el DRC en cero y el lazo caliente de un buck dando la
vuelta por la otra cara. Esto mide las reglas de `hardware/DS/DS12_ALIMENTACION.md` §7:

1. **Lazo caliente.** Los cerámicos de entrada de cada buck, ¿están pegados a VIN y a
   GND del integrado (≤ 2 mm borde a borde) y en la misma cara?
2. **Nodo SW.** ¿El inductor está pegado al pin SW y en la misma cara? ¿Hay vías en SW?
3. **Vías térmicas** bajo el pad de GND del AP63356, y ninguna en el pad de SW.
4. **FB lejos de SW**, en la misma capa (entre F.Cu y B.Cu está el plano de In1).
5. **Separación** de cada buck al AK5538 y a los PCM1690.
6. **Un punto de prueba por riel**, y que el TP esté conectado (en la DS11, seis de
   siete no lo estaban).
7. **+5VA y +3V3 como planos.**
8. **Antena del ESP32:** cero cobre, en ninguna capa, dentro del keepout de la huella, y
   el keepout contra el borde.
9. **Desacople:** para cada pin de alimentación de cada integrado, el capacitor más cercano.

Trabaja sobre una copia en memoria con los rellenos recalculados, y no toca el archivo.
Encuentra los componentes **por valor y por función de pin**, no por designador: si IC4
pasa a llamarse IC7, lo sigue encontrando. Los umbrales son **criterio, no física**
(están juntos al principio del script, para discutirlos en un solo lugar).
```
tools/check_layout.py                          # hardware/DS/DS.kicad_pcb
tools/check_layout.py otra/placa.kicad_pcb
tools/check_layout.py --autoprueba             # ~45 s: prueba la herramienta, no la placa
```
**Su autoprueba:** geometría contra respuestas conocidas. Una placa vacía tiene que dar
❌, nunca ✅. Y 22 mutantes sobre la placa real, en las dos direcciones: los capacitores y
el inductor **bien** puestos tienen que dar ✅, y alejados tienen que dar ❌. También: una
vía en SW, las vías térmicas, una pista de SW junto a FB (y la misma detrás del plano,
que no cuenta), una pista y un plano bajo la antena, un TP desconectado y el desacople
del STM32. Sale 1 si hay alguna ❌.

**Lo que no mide**, y hay que mirar a mano:
- las islas de GND (§7.4);
- dónde toma FB la muestra;
- los retornos por vías.

### `sch2net.py` — el netlist derivado del esquemático, sin KiCad
El netlist exportado (`.net`) está en el `.gitignore`, y sin él no hay forma de
preguntarle nada al esquemático que se está dibujando. Esto lee los `.kicad_sch` —hojas
con instancias múltiples, buses, etiquetas de los tres tipos, símbolos de alimentación,
las doce orientaciones de símbolo— y escribe un netlist en el formato de eeschema, el
que leen las dos herramientas de abajo.

**Su autoprueba es contra KiCad mismo**: corrido sobre el esquemático de la DS11 y
comparado con el netlist que exportó KiCad de esa placa, **0 diferencias de conexión en
2049 pines**. Antes de eso, el primer intento creía que `~{SD}` era un bus.
```
tools/sch2net.py hardware/DS/DS.kicad_sch hardware/DS/DS.net      # DS.net queda fuera de git
tools/netlist_diff.py hardware/DS/DS.txt hardware/DS/DS.net        # DS11 → lo dibujado
tools/sch2net.py hardware/DS/DS.kicad_sch /tmp/x.net --erc         # pines sueltos, PWR_FLAG que faltan, alias
```
No reemplaza al netlist exportado para fabricar: sirve para revisar.

### `pcb_status.py` — qué hay puesto en el PCB y qué falta
Lee el `.kicad_pcb` y dice: qué huellas están fuera del contorno (las nuevas caen ahí al
importar), qué redes no tienen un solo tramo de cobre, cuánto cobre quedó sin red
(huérfano de componentes que se movieron), qué pads ubicados tocan una pista de **otra**
red, y —con el netlist de `sch2net.py`— qué designadores faltan importar al PCB.
```
tools/pcb_status.py hardware/DS/DS.kicad_pcb hardware/DS/DS.net
tools/netlist_diff.py hardware/DS/DS.net hardware/DS/DS.pcb.net    # esquemático ↔ PCB, red por red
```
**Calibrado sobre la DS11 fabricada** (DRC en 0): 0 huellas afuera, 0 redes sin cobre,
0 cobre huérfano. Sus falsos positivos en ese archivo están anotados en el docstring, y
hay que descontarlos. No es un DRC: los rellenos los toma como están guardados.

### `netlist_digest.py` — el netlist, en tres archivos grepables
`hardware/DS/DS.txt` son 70.454 líneas. Cualquier pregunta de conectividad obliga a
parsearlo entero; esto lo hace una vez y deja tres archivos en `.ds12_audit/` (derivado,
fuera de git).

El dato útil: `redes.txt` trae la **cantidad de nodos** por red, así que buscar pines al
aire es un `awk`, no una investigación.
```
tools/netlist_digest.py                        # usa DS.net si existe, si no DS.txt (la DS11)
awk -F'\t' '$2==1' .ds12_audit/redes.txt     # pines que no van a ningun lado
```
Así aparecieron PA11 y PA12 —`USB_DM`/`USB_DP`— flotando.

⚠️ **El netlist no es la fuente de verdad del stock ni de los part numbers comprados.**
Para eso es `hardware/DS/jlcpcb/production_files/BOM-DS.csv`. Los dos no coinciden: para el
4,7 nF, `DS.txt` dice `C883564` y el BOM dice `C2961271`.

### `geometria_array.py` — dónde va cada parlante, para cortar la madera
El beamforming supone que cada cono está en un lugar exacto. **Si la madera y el firmware
no coinciden no hay ningún error**: el haz se forma igual pero apunta a otro lado, y el
síntoma es «suena raro».

Por eso el script no tiene las medidas adentro: **las lee de los `#define` de `main.c`**,
los mismos que usa el STM32 para calcular los retardos. Imprime la tabla de las 32
posiciones, un mapa en ASCII donde se ve la traba, y exporta el panel en DXF —en
milímetros, con capas separadas para el contorno, los agujeros, las cruces de centrado y
los números.
```
tools/geometria_array.py --layout 2x16 --dxf panel.dxf --diametro 66 --marco 30
```
Avisa de dos cosas que sólo se ven cuando ya está cortado: si el retardo compilado **no
cubre la diagonal** (sale con código 1) y si con ese diámetro **los agujeros se superponen**
o queda poca madera entre ellos.

⚠️ El diámetro por default (66 mm) es un ejemplo, no la medida de tu parlante: el script lo
imprime siempre para que no se cuele sin querer. Y los números que pone en el DXF son
**posiciones físicas**, no canales: hoy `SPK_PAIR_SWAP = 1` y el cono de la posición p lo
mueve el canal p^1.

---

## Una regla que costó aprender

**Cuando una herramienta produce un número, hay que preguntarle también qué haría si no
hubiera nada que medir.** El analizador de fase produjo un informe entero —dispersiones,
retardos, polaridades invertidas— a partir de −138 dBFS de ruido del ADC con los
amplificadores apagados, porque un umbral relativo siempre deja pasar «algo».

Por eso `fase.py` y `perfil.py` tienen un piso **absoluto** además del relativo, y cortan
diciendo qué revisar. Y por eso tres de las herramientas traen `--autoprueba`: el veredicto
de un umbral sin prueba es una opinión.

Ver [docs/banco_dos_microfonos.md](../docs/banco_dos_microfonos.md) y
[docs/reporte_2026-09.md](../docs/reporte_2026-09.md).
