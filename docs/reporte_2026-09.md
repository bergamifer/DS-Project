# Reporte — El mes en que el instrumento se midió a sí mismo (septiembre 2026)

> Escrito el 2026-09-09. Continúa [reporte_2026-08.md](reporte_2026-08.md). Cubre la
> primera sesión con micrófonos y lo que salió de ella, que fue bastante más de lo que se
> había ido a buscar. Los detalles viven en los docs enlazados; esto es el mapa y el
> registro de por qué se decidió cada cosa.

---

## 1. Dónde estábamos

Agosto cerró con la DS11 andando, 32 canales poblados y el audio limpio, pero con una
limitación de método que empezaba a doler: **casi todo se validaba a oído**. El
sincronismo entre bancos estaba medido, el rendimiento del pipeline también, pero las
preguntas que importaban para la obra —¿el foco es real?, ¿cuánto cae afuera?, ¿llegan
los 32 parlantes en fase?— seguían contestándose caminando la sala.

Fernando armó entonces un **banco de dos micrófonos fijos**, y eso cambió lo que se podía
saber.

---

## 2. Por qué dos micrófonos, y no uno

El problema no era la precisión. Era que **las preguntas abiertas son comparaciones entre
dos lugares al mismo tiempo**, y una persona no puede estar en dos lugares.

Medir primero acá y después allá parece equivalente y no lo es: entre las dos medidas
cambia el instante de disparo, cambia la temperatura —y con ella la velocidad del sonido—
y, sobre todo, alguien entró al campo a mover el micrófono, que es el objeto que más
refleja de la sala.

Con dos entradas las dos medidas comparten trama TDM. No es «casi simultáneo»: es la
**misma** ventana de muestreo, así que el conversor no aporta desfase entre ellas. Lo que
quede de diferencia es acústico y nada más.

Documentación completa: [banco_dos_microfonos.md](banco_dos_microfonos.md).

---

## 3. 🔴 El arnés estaba cruzado de a pares

**El hallazgo que más impacto tuvo, y el que más barato salió de encontrar.**

Fernando avisó a mitad de la sesión que las cajas estaban conectadas de a pares
intercambiados. No se dio por sentado: el AUTOFOCUS mide el tiempo de vuelo de los 32
parlantes hasta un micrófono, y el ajuste por mínimos cuadrados de las cuatro incógnitas
(posición del micrófono y offset de disparo) dice cuánto **no** explica la geometría.

| mapeo | residuo RMS | en camino |
|---|---|---|
| cableado directo | **62,0 µs** | 21,3 mm |
| **pares cruzados** | **3,9 µs** | **1,3 mm** |

**15,8× mejor**, y los **dos micrófonos por separado** dieron el mismo veredicto bajando a
los mismos 3,9 µs. El ajuste par por par confirmó que no faltaba cruzar ninguno más: son
los dieciséis, uniformemente.

**La firma, para reconocerla en otro nodo:** los residuos **alternan de signo dentro de
cada par** y **crecen hacia el borde del muro** —porque la diferencia de camino entre
columnas vecinas crece con la distancia al micrófono—. Con el cableado directo se veía así:

```
ch14 = +128 µs   ch15 = −72 µs        ch22 = +108   ch23 = −76
ch30 = +131 µs   ch31 = −66 µs        ch12 =  +66   ch13 = −78
```

`tools/calfase.py --pares` lo decide con un número. Corregido en el firmware con
`SPK_PAIR_SWAP`, que lo anuncia en el banner del arranque — **es un parche para un error de
cableado**, y cuando el arnés se arregle hay que ponerlo en 0.

### Lo que se oyó

Con `tools/abx.py` —prueba a ciegas, con puntaje y con tiradas trampa donde las dos mitades
son idénticas— la corrección resultó **audible**: **6 de 6, p = 0,016**, y las dos trampas
contestadas «igual». Ese último dato es el que le da peso: no se estaba oyendo el clic del
cambio.

---

## 4. La prueba de aceptación, con respuesta conocida

Con el mapeo corregido, el ajuste recupera **247 mm** de separación entre los dos
micrófonos contra los **250** de la cinta métrica, **sin que se le diga dónde están**.

Con eso quedan validados de una vez la tabla de geometría, los retardos Q8, el mapeo de
canales y el arnés. Y de paso los ubica: **c1 en (+14, −46, 954) es el del centro; c0 en
(−233, −52, 948) el de la izquierda.**

Es la única medición del banco que puede fallar contra un número conocido de antemano, y
por eso es la que vale.

---

## 5. El chirp enfocado (modo 23): medir el foco sin la sala

El barrido del perfil del haz con **tono continuo** en la sala de 3 × 2,5 m salió
inservible: el máximo de un micrófono caía a 400 mm de donde estaba el micrófono. La misma
noche, el chirp del AUTOFOCUS ubicaba los dos micrófonos con 3 mm de error.

La diferencia no es la precisión: **la correlación de un chirp se queda con la primera
llegada y el tono continuo mide la suma de todo lo que hay en la sala.**

De ahí salió el modo 23: el array **entero** emite el chirp **con los retardos del foco**,
el micrófono correlaciona, y se toma el pico de la primera llegada. No hay emisor nuevo —
el chirp se escribe en `beam_line` y el camino de salida de siempre le aplica
`tx_delay_q8[]`.

Mismo muro, misma sala, media hora de diferencia:

| | tono continuo | **chirp enfocado** |
|---|---|---|
| contraste simétrico a 247 mm | +2,8 dB | **+17,7 dB** |
| asimetría entre los dos sentidos | +1,4 dB | **+0,3 dB** |
| ancho −6 dB del lóbulo | 540 mm | **241 y 267 mm** |
| pico de c1 (está en +14 mm) | −32 mm | **+17 mm** |
| pico de c0 (está en −233 mm) | +179 mm ✗ | **−240 mm** ✓ |
| separación recuperada (real 247) | +78 mm ✗ | **+258 mm** ✓ |
| tiempo del barrido | 65 s | **23 s** |

El **+0,3 dB de asimetría** es el número que más dice: enfocar en A leyendo B y al revés
dan lo mismo dentro de tres décimas. Eso sólo pasa si lo que se mide es el haz y no la
sala. Y el ancho de 241–267 mm contra los **286 mm** que predice λz/D a 2 kHz cierra el
círculo: el array hace lo que la geometría dice.

**Con esto, el contraste —el número del que depende la instalación— se puede medir en la
sala que hay, en vez de esperar una mejor.**

---

## 6. 🔴 El foco no es un punto: es un cigarro

`tools/imagen.py` barre el foco por una grilla del espacio y mapea cuánto sonido directo
llega a cada micrófono. Donde el mapa tiene su máximo, está el micrófono — no por cálculo,
sino porque ahí es donde el array concentra la energía.

**Dos métodos independientes, y coinciden:**

| | imagen de haz | ajuste de TOF | diferencia |
|---|---|---|---|
| c0 | (−244, −53, 1007) | (−233, −52, 948) | X 11 · **Y 1** · Z 59 mm |
| c1 | (+12, −46, 1021) | (+14, −46, 954) | **X 2** · **Y 0** · Z 67 mm |

Separación: **256 mm** (imagen) · **247 mm** (TOF) · **250 mm** (cinta). Tres métodos
dentro de 9 mm, dos de ellos completamente independientes —uno geométrico, uno energético—.

Y de eso salió el hallazgo del mes:

| eje | ancho −6 dB |
|---|---|
| X (horizontal) | **200 mm** |
| Y (vertical) | **200 mm** |
| Z (profundidad) | **≥ 900–1100 mm** — el −6 dB toca el borde del barrido |

No es un defecto, es física: la profundidad de foco va con el **cuadrado** de z/D.

```
DOF ≈ 8 · λ · (z/D)²
```

| array | apertura X | z/D a 1 m | DOF a 2 kHz |
|---|---|---|---|
| **DS11 actual 8×4** | 560 mm | 1,79 | **4,4 m** |
| 2×16 entrelazado | 1240 mm | 0,81 | **0,9 m** |

**Tres consecuencias:**

1. **Un nodo solo no puede poner sonido en «un punto» de la sala.** Lo pone en una
   *dirección*, sobre un tubo largo. La discriminación en profundidad la tiene que dar la
   geometría de los quince nodos desde ángulos distintos.
2. **Es un argumento nuevo y fuerte para el 2×16** que no estaba en el análisis previo:
   duplicar la apertura mejora la profundidad **cinco veces**, porque va con el cuadrado.
   La resolución lateral sólo mejora al doble.
3. **Los 200 mm en vertical son exactamente lo que la obra no quiere.** Alguien que mueve
   la cabeza 20 cm pierde el foco.

Análisis de la disposición propuesta: [DS12_PLANNING.md §0.8](../hardware/DS/DS12_PLANNING.md).

---

## 7. 🔴 El riel de 3V3, y por qué se reinicia la placa

El §B de [presupuesto_potencia.md](presupuesto_potencia.md) describía como **hipótesis** que
el riel se hunde al encender los amplificadores y que IC1 (MAX809T, 3,08 V) reinicia el
STM32. Ahora está **medido, desde adentro de la placa y sin agregar un solo componente**.

El STM32H743 puede medir su propia alimentación contra su referencia interna:

```
VDDA [mV] = 3300 × VREFINT_CAL / lectura        (VREFINT_CAL en 0x1FF1E860)
```

| condición | VDDA típico | mínimo retenido |
|---|---|---|
| amps apagados, mute | 3300 mV | 3251–3255 |
| amps **encendidos**, mute | 3300 mV | 3247–3254 |
| amps + volumen −20 dB | 3195–3221 mV | **3068 mV** |
| amps + volumen −18 dB | 3219 mV | **3075 mV → RESET** |

**Encender los amplificadores no cuesta casi nada. Lo que hunde el riel es la corriente de
audio.** Y el reinicio se identifica sin ambigüedad: `/status` informa `reset: PIN`,
`rsr 0x00460000` — el pin NRST. No fue software, ni el brownout del núcleo, ni el watchdog.

**El mínimo retenido es lo que lo hace visible.** Un valor instantáneo leído una vez por
segundo tiene una probabilidad de una en mil de caer dentro de un hundimiento de
milisegundos.

### Y el mecanismo, con la cadena completa (§E)

Faltaba la pregunta incómoda: si los amplificadores están en +5VA, ¿por qué se hunde el
+3V3?

```
+5VA = 0,791 × (1 + 40,2/7,5) = 5,031 V     ← consigna REAL del buck
     − 0,147 V   FB2 (220 mΩ de DCR × 0,67 A)
     − 0,400 V   D9 (SS34)
     − 0,040 V   cobre
     = 4,447 V   en la entrada del LDO
```

Y el LDO **no es el que dice el esquemático**: el campo *Value* dice AP1117-33, pero la
parte montada es **AMS1117-3.3**, con 1,1 V de dropout típico y 1,3 máximo. Necesita 4,30 V
típico —margen +147 mV— y **4,50 V en el peor rincón, donde hay 4,296: margen negativo de
204 mV con los amplificadores en silencio.**

**Ahí está el mecanismo.** Un LDO *en regulación* da 50–70 dB de rechazo de fuente. Un LDO
**en dropout no rechaza nada**: es una resistencia, y transmite la caída de +5VA directo al
riel digital.

### Que el dato sobreviva a la muerte

Cuando el riel colapsa, el STM32 se reinicia y se lleva el mínimo. Los 32 registros de
respaldo del H743 no se enteran —el manual dice textualmente *«System reset: not
affected»*— y habilitarlos son dos líneas, sin LSE ni reloj de RTC.

Verificado de punta a punta: la corrida anterior registró 3236 mV, reinicio, y la corrida
nueva informa `vdda_pre_reset_mv = 3236` con uptime de 5,1 s.

---

## 8. Tres instrumentos que mentían en silencio

El patrón del mes, y el que más conviene recordar: **un número con la forma correcta y el
contenido equivocado es peor que no tener número.**

**a) Dos de los cuatro clamps de salida no contaban el recorte.** `main.c:6715` (modo 14) y
`6781` (modo 15) clampeaban sin incrementar `out_clip_count`, mientras 6824, 6838 y 6865 sí.
Los modos 14 y 15 podían recortar el 100 % de las muestras con `out_clip` en cero — y el 15
es el que suma ocho fuentes y les aplica la ganancia de entrada.

**b) Todo el lado de la entrada estaba fuera del presupuesto de CPU publicado.**
`Process_ADC_Half` se llama en las líneas 1878 y 1883; la primera marca de `DWT->CYCCNT`
está en la 1907. Medido ahora, con `cpu_adc_x10`:

| modo | fills | **adc** | total |
|---|---|---|---|
| MULTIFOCUS 8 entradas | 66,1 % | 1,7 % | **67,8 %** |
| BARRIDO DE FRECUENCIA | 2,3 % | **3,9 %** | 6,2 % |
| FASE por parlante | 2,5 % | **3,8 %** | 6,3 % |

Para los modos de medición, la parte invisible era **la mayor**.

**c) Mis propias herramientas.** El analizador de fase produjo un informe entero
—dispersiones, retardos, polaridades invertidas— a partir de **−138 dBFS de ruido del ADC**,
con los amplificadores apagados. La causa: un umbral relativo siempre deja pasar «algo». Y
el estimador de piso usaba el percentil 10 de la propia señal, que con 32 parlantes
**parejos** —el caso sano— cae adentro de la señal y no sobrevive ninguna medida. Los dos
los encontró la autoprueba sintética, no el uso real.

---

## 9. Dos defectos de audio que se oían

**El barrido zumbaba a 20 Hz.** El modo 16 reapuntaba el foco 20 veces por segundo, y cada
reapuntada mueve el retardo ~0,04 muestras: un escalón de ~1 % de amplitud. El comentario
del código lo daba por inaudible mirando su **tamaño** y olvidando su **repetición**: 1 % es
despreciable una vez y es una nota grave veinte veces por segundo, en fase en los 32
parlantes. Bajado a 200 actualizaciones por segundo: el escalón se achica 10× y sube a
200 Hz.

**Y el foco saltaba hasta 662 mm al entrar al modo.** El triángulo se calculaba con el reloj
de la placa, así que el primer reapunte tiraba el foco donde el triángulo estuviera pasando.
Seis entradas seguidas dieron saltos de 89, 111, 344, 455, 465 y 662 mm. Ahora arranca desde
el instante de entrada, en el centro: **+11 mm, siempre igual**.

La primera hipótesis para el clic —el guardado en EEPROM, que bloquea ~12 ms y está
documentado que causa underruns— **quedó descartada con el contador**: cero underruns antes,
durante y después. Eso dijo que no era un deadline perdido sino una discontinuidad de señal,
y ahí apareció el salto.

---

## 10. Estado del hardware

| | |
|---|---|
| Parlantes que suenan | **29 de 32**. Faltan **ch7** (fila 0, col 6) y la caja de **fila 3, cols 4-5** |
| Apareamiento de los que suenan | **2,1 dB** de dispersión — muy bueno |
| Arnés | cruzado de a pares, compensado en firmware |
| Riel de 3V3 | se hunde a 3068 mV con audio fuerte y reinicia la placa |
| Protocolo | v14 → **v22** |
| Modos | 22 → **24** |

---

## 11. Herramientas nuevas

Ocho scripts en `tools/`, con índice en [tools/README.md](../tools/README.md). Los de este
mes: `perfil.py` (perfil del haz y contraste), `fase.py` (fase por parlante), `imagen.py`
(imagen de haz), `nivel.py` (poner nivel sin acople), `abx.py` (escucha a ciegas con
puntaje), `focos.py` (capturar posiciones y usarlas como focos).

Tres de ellos traen **autoprueba con respuesta conocida** (`--autoprueba`): `calfase.py`
recupera 250 mm con 0,3 mm de error, y `fase.py` pasa 22 comprobaciones —recupera un retardo
de +150 µs con 0,3 µs de error y distingue una inversión de polaridad de un error de
retardo—.

---

## 12. Lo que queda abierto

| | |
|---|---|
| **Tres conos sin conectar** | ch7, y la caja de fila 3 cols 4-5 |
| **El ciclo del chirp enfocado se pierde ~1 de cada 100** | contador `retries` en `GET /foco`; falta la causa de raíz |
| **El mapeo de entradas se corrió un slot, una vez** | de c0/c7 a c0/c1 en un reinicio por OTA; siete reinicios posteriores estables. No se puede decidir desde acá si fue la alineación TDM o el conector |
| **Reproductor de audio (.wav)** | diseño completo, sin implementar. µ-law 8 bits a 48 kHz nativos, clips en flash, 8 voces sobre 4 ranuras. Ver §3 de las notas de diseño |
| **Contraste vs. frecuencia** | el chirp promedia 500 Hz–6 kHz; falta la curva por banda |
| **Profundidad real del foco** | el barrido se quedó corto; hay que ir de 300 a 3000 mm |

---

## 13. Y una nota de método

Tres veces este mes una explicación plausible resultó falsa y la descartó una medición:
el clic que no era la EEPROM, el piso que no era el percentil, y la ventana de la prórroga
del mecenazgo que no estaba cerrada. En los tres casos el costo de verificar fue de minutos
y el de no verificar habría sido de semanas.

El corolario que dejó el mes: **cuando una herramienta produce un número, hay que
preguntarle también qué haría si no hubiera nada que medir.** Si la respuesta es «un número
parecido», la herramienta no sirve todavía.
