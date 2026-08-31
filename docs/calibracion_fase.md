# Calibración de fase, parlante por parlante

> Escrito 2026-08-31, a pedido de Fernando: *"hacer una calibración de fase parlante por
> parlante, ¿es posible?"*
>
> **Sí.** La medición ya existía desde el AUTOFOCUS; lo que faltaba era la aritmética que
> separa el retardo propio del parlante del camino geométrico hasta el micrófono, y la
> prueba que decide si esa separación es real.
>
> Complementa [`protocolo_medicion.md`](protocolo_medicion.md), que resuelve **cómo medir el
> haz sin medir la sala**. Esto responde **cómo medir el parlante sin medir la geometría**.

---

## 1. Los tres niveles, y en cuál estamos

| nivel | qué separa | estado |
|---|---|---|
| **eléctrico** | el skew entre canales del pipeline digital | ✅ **medido y cerrado** |
| **acústico por parlante** | el retardo propio del driver + su montaje | 🔨 esto |
| **relevamiento completo** | también las posiciones reales de los parlantes | idea (v4 de `app_ios.md`) |

El nivel eléctrico ya está resuelto y conviene tenerlo presente antes de medir el acústico,
porque **fija la escala de lo que sería interesante encontrar**:

> El peor par de los 32 canales tiene **2,25 µs de skew = 0,77 mm** de camino acústico,
> contra 80 mm de paso de la grilla. Tres órdenes de magnitud por debajo de la geometría
> que el array tiene que resolver. — `loopback_debug.md` §22.1, issue #14.

O sea: si el retardo propio de los parlantes resulta ser del mismo orden, **no hay nada que
corregir**, y eso también es un resultado.

---

## 2. Por qué el AUTOFOCUS solo no alcanza

El modo 11 recorre los 32 parlantes, emite un chirp de 10 ms (500 Hz → 6 kHz) por cada uno,
captura 100 ms en el micrófono de la entrada c0 y correlaciona. Por parlante devuelve el
pico de la correlación en **muestras Q8** — 1/256 de muestra = **81 ns = 28 µm** de camino.

La resolución no es el problema. El problema es que lo medido mezcla tres cosas:

```
TOF_k  =  t₀  +  τ_k  +  |p_k − m| / c
          │      │        │
          │      │        camino geométrico (calculable: p_k se conoce)
          │      retardo PROPIO del parlante  ← lo que buscamos
          instante de disparo (común a todos)
```

El firmware guarda esa suma en `cal_delay_q8[]` y el propio código lo advierte
(`main.c:139-159`): *"es UN foco, el del punto donde estaba el micrófono. NO es una
corrección general"*. Aplicarlo por defecto pisaría el foco de cualquier otro modo con el de
un micrófono que ya no está ahí.

Las **polaridades** sí se aplican siempre, y con razón: son una propiedad del cableado, no
del punto de medición.

---

## 3. La separación

Las posiciones `p_k` de los 32 parlantes son conocidas (`Beam_InitTxGeometry`: columnas cada
80 mm, filas cada 160 mm, filas 2 y 4 corridas 40 mm en patrón de ladrillo).

Con 32 TOF medidos se ajustan por mínimos cuadrados **cuatro incógnitas** — la posición del
micrófono `m` y el offset de disparo `t₀`. **El residuo de ese ajuste es `τ_k`.**

32 ecuaciones, 4 incógnitas: **28 grados de libertad**.

Dos consecuencias que vale entender:

- **No hace falta saber dónde se puso el micrófono.** Lo resuelve el ajuste, y de paso lo
  informa — sirve como control de que la medición tiene sentido.
- El resultado **vale para cualquier foco**, no sólo para el punto del micrófono. Ésa es
  toda la diferencia con `cal_delay_q8`.

---

## 4. 🔴 La prueba que decide si la calibración es real

Un ajuste con 28 grados de libertad **siempre** devuelve residuos. Que existan no prueba
nada: pueden ser el parlante, o pueden ser una reflexión de la sala y un error de geometría
que el ajuste no supo absorber.

**La única forma de distinguirlos es medir desde dos posiciones de micrófono y comparar.**

| resultado | qué significa |
|---|---|
| `τ_k` **coinciden** | es una propiedad del parlante, y **generaliza**. La calibración vale. |
| `τ_k` **difieren** | es sala, geometría, o el micrófono moviéndose. **No hay calibración.** |

Sin esta prueba no se puede distinguir una calibración de un ajuste que absorbió el ruido.
Es la misma disciplina que cerró §27 y §28 de `loopback_debug.md`: medir **dentro del rango
fiel** y **contra una referencia tomada en la misma configuración**.

El discriminante concreto es el cociente entre la diferencia de las dos corridas y la
dispersión de cada una. Verificado contra datos sintéticos, donde `τ` se conoce:

| escenario | dispersión | dif. entre corridas | cociente | veredicto |
|---|---|---|---|---|
| τ real de 6 µs, sin ruido | 4,29 µs | 0,26 | **0,06** | coinciden ✅ |
| τ real de 6 µs, ruido 0,3 µs | 4,23 µs | 0,48 | **0,11** | coinciden ✅ |
| τ real de 2 µs, ruido 0,3 µs | 1,38 µs | 0,36 | **0,26** | coinciden ✅ |
| sin τ, sólo ruido de 1 µs | 0,91 µs | 1,10 | **1,21** | no coinciden ✅ |
| sin τ, sólo ruido de 3 µs | 2,73 µs | 3,28 | **1,20** | no coinciden ✅ |
| sin τ y sin ruido | 0,02 µs | 0,03 | — | despreciable ✅ |

**El cociente vale 0,1-0,3 cuando `τ` es real y ~1,2 cuando es ruido** — dos ruidos
independientes dan √2 veces la dispersión. El umbral de 0,5 cae limpio en el medio.

Esa tabla se reproduce con `python3 tools/calfase.py --autoprueba`. Está adentro del script
a propósito: un umbral sin prueba es una opinión, y una prueba que vive en un scratchpad se
pudre sin que nadie se entere.

---

## 5. Cómo se corre

**Hace falta**: un micrófono en la entrada 1 del ADC (c0), la placa alimentada, y el
espacio más absorbente que haya. Cerca mejor que lejos: el campo directo domina las
reflexiones.

```bash
# 1. Micrófono en la posición A. Disparar el scan del AUTOFOCUS (modo 11):
curl -X POST http://<nodo>/mode -d '{"mode":11}'
#    Esperar a que termine — /status informa cal_progress y cal_ok_count.

# 2. Bajar el resultado y guardarlo:
python3 tools/calfase.py --host <nodo> -o A.json

# 3. MOVER el micrófono a la posición B (al menos 300 mm, mejor 1 m y del otro
#    lado del eje). Repetir el scan y bajar:
curl -X POST http://<nodo>/mode -d '{"mode":11}'
python3 tools/calfase.py --host <nodo> -o B.json

# 4. LA PRUEBA:
python3 tools/calfase.py --comparar A.json B.json
```

El paso 3 es el que no se puede saltear. Dos corridas desde el mismo lugar no prueban nada:
ven las mismas reflexiones y coincidirían aunque `τ` fuera puro artefacto de la sala.

---

## 6. Cómo escala a los 15 nodos

El procedimiento es **por nodo**. Cada uno tiene su propia entrada de micrófono, su propia
EEPROM y su propio muro de 32 parlantes, así que la calibración es local y **viaja con la
placa**.

Quince corridas de ~2 minutos, una vez, en fabricación o en instalación. No es un problema
de 480 parlantes: son 15 problemas de 32, y cada uno se resuelve solo.

Lo que **no** resuelve es la alineación *entre* nodos, que depende de compartir MCLK (JP2) y
está tratada en [`multinode.md`](multinode.md). Son dos capas distintas y conviene no
confundirlas: `τ_k` corrige el parlante dentro de su muro; el reloj compartido alinea los
muros entre sí.

### Dónde vive el resultado

Cuando la prueba del §4 dé positiva, el `τ_k` promediado va a un campo nuevo de la
configuración persistida:

```c
int16_t spk_tau_q8[BEAM_NUM_CHANNELS];   // retardo propio, muestras Q8
```

**Aplicado siempre**, igual que `cal_sign[]` y a diferencia de `cal_delay_q8[]` — porque a
diferencia de aquél, éste sí es una corrección general. Requiere subir la versión del layout
de EEPROM (hoy v5).

---

## 7. Los límites, dichos de entrada

- **Es retardo de grupo, no fase contra frecuencia.** La correlación de un chirp da **un
  escalar por parlante**. Si los drivers difieren en la *forma* de su fase —probable cerca
  de la resonancia de 125-180 Hz que apareció en `loopback_debug.md` §28— un retardo único
  no lo arregla. Y el pipeline (delay entero + Q8, ganancia entera) no puede sostener un FIR
  por parlante: corregir eso sería un cambio de arquitectura, no un ajuste.
- **Vale en la banda del chirp**: 500 Hz – 6 kHz.
- **El ajuste encoge `τ`.** Parte del patrón se absorbe en los cuatro parámetros libres: en
  la prueba sintética, 6,0 µs de dispersión real se leen como 4,3. El `τ` recuperado es un
  piso, no el valor exacto. No cambia el veredicto —lo que decide es si se *repite*— pero sí
  el número.
- **La velocidad del sonido se toma fija** en 343 m/s (20 °C), y sube ~0,6 m/s por grado. Un
  error de 10 °C es 1,7 % de escala, que el ajuste absorbe casi todo en la distancia al
  micrófono. Se puede pasar con `--c`. Es la misma razón por la que `DS12_PLANNING` §6c
  propone un sensor de temperatura ambiente.
- **La sala sesga el pico de la correlación.** Es el límite dominante, y el único que se
  combate con el arreglo físico y no con más cuentas.

---

## 8. Lo que el mismo scan regala

El pico de la correlación también da **nivel por parlante**. Como `gains[]` ya existe en la
configuración, un trim de ganancia por canal sale del mismo barrido sin medir nada más.

Vale la pena porque §28 ya midió que cada parlante imprime su propia curva de impedancia
sobre su canal, hasta ±0,5 dB. Es chico y común a todos mientras los 480 drivers sean del
mismo modelo, pero es **la primera cantidad medida que depende de la dispersión de
fabricación del driver** — y ahora hay cómo medirla canal por canal.

---

## 9. Estado

| | |
|---|---|
| `GET /cal` en el ESP32 | ✅ implementado, compila |
| `CMD_GET_CAL` / `CMD_CAL_DATA` (protocolo v11) | ✅ implementado, compila |
| `tools/calfase.py` — ajuste y veredicto | ✅ escrito, autoprueba pasa |
| **Verificación contra hardware** | ⬜ **pendiente** — la placa estaba apagada el 31-ago |
| `spk_tau_q8[]` en EEPROM | ⬜ pendiente, y a propósito: no se guarda nada hasta que la prueba del §4 dé positiva |

Seguimiento en el issue #21.
