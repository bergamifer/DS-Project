# Herramientas

Dos familias, y conviene no confundirlas:

- **Medición acústica** (las ocho de abajo + `sweep_focus.sh`): hablan con el ESP32 por
  HTTP, con la placa encendida y sonando.
- **Placa y repositorio** (`check_pcb.sh`, `netlist_digest.py`): trabajan sobre los
  archivos de KiCad, sin tocar hardware. Están al final.

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

## Placa y repositorio

Estas dos no hablan con la placa: leen los archivos de KiCad.

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

### `netlist_digest.py` — el netlist, en tres archivos grepables
`hardware/DS/DS.txt` son 70.454 líneas. Cualquier pregunta de conectividad obliga a
parsearlo entero; esto lo hace una vez y deja tres archivos en `.ds12_audit/` (derivado,
fuera de git).

El dato útil: `redes.txt` trae la **cantidad de nodos** por red, así que buscar pines al
aire es un `awk`, no una investigación.
```
tools/netlist_digest.py
awk -F'\t' '$2==1' .ds12_audit/redes.txt     # pines que no van a ningun lado
```
Así aparecieron PA11 y PA12 —`USB_DM`/`USB_DP`— flotando.

⚠️ **El netlist no es la fuente de verdad del stock ni de los part numbers comprados.**
Para eso es `hardware/DS/jlcpcb/production_files/BOM-DS.csv`. Los dos no coinciden: para el
4,7 nF, `DS.txt` dice `C883564` y el BOM dice `C2961271`.

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
