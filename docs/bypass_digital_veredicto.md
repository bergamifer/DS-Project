# Veredicto: bypass digital y cadena coherente en fase sobre DS11

> Respuesta a `docs/claude_review_bypass_digital_multinodo_ds11.md` (2026-08-19).
> Escrito 2026-08-24. Todo lo afirmado acá está verificado contra el netlist, el
> firmware que corre hoy, RM0433 Rev 8 y la errata ES0392 — las referencias están
> en línea para que se puedan volver a chequear.

---

## 0. Veredicto corto

**El camino es viable y las DS11 fabricadas alcanzan para demostrarlo.** El costo
de la demostración es **más bajo** de lo que el pedido supone: ~6 cables finos y un
jumper entre dos placas, sin cortes de pista, sin quitar componentes y **sin USB**.

Pero el documento tiene tres cosas que cambian la respuesta, y una omisión que es
más importante que todo lo demás:

1. El número de skew que usa como línea de base **no mide lo que dice medir**.
2. Nunca pregunta *por qué* digital. El motivo bueno está en otro documento del
   repo y es mucho más fuerte que los que enumera.
3. **No cita ni refuta `docs/multinode.md`**, que argumenta lo contrario con
   números — a pesar de listarlo como lectura obligatoria (#5).
4. Mete USB en el camino crítico del primer gate, cuando es un riesgo
   independiente y separable.

Escenario por escenario:

| Escenario | Veredicto |
|---|---|
| **2 DS11 en banco** | **Viable.** 4 cables en el follower, 2 en la raíz, JP2 cerrado. Reversible. |
| **10–18 DS11** | **Viable con condiciones.** En estrella, no en cadena. La cadena convierte el interruptor de la caja 7 en una falla de las cajas 8 a 18. |
| **Producción** | **Viable.** Acá A2B se gana el lugar, o una revisión de PCB que exponga los clocks. Hoy no hay dato para elegir; el gate 2 lo genera. |

---

## 1. Correcciones al documento

### 1.1 El skew de ~1 µs no es una medición de fase

El pedido dice: *"Su arranque quedó medido con aproximadamente 1 us de skew estable
entre bancos"*, y lo usa como línea de base (hipótesis #14).

`firmware/Core/Src/main.c:706-711` dice exactamente lo contrario, y lo dice el
firmware sobre sí mismo:

> *"El número de 4 µs que publica el heartbeat sale de comparar marcas de
> DWT->CYCCNT en los callbacks de DMA: mide cuándo se entera el SOFTWARE, no
> cuándo sale la señal."*

Es la latencia de servicio de una interrupción, no el skew de la salida analógica.
Además no es estable entre builds: `docs/ds11_perf.md:51` lo publica en 4 µs y el
`/health` de hoy en 1 µs — se movió porque cambió el scheduling, no porque cambiara
el hardware.

**El skew analógico real entre SAI1_A y SAI2_A nunca se midió.** El modo 19
(LOOPBACK) existe para eso y está bloqueado por el puente de J13. Así que la
hipótesis #14 no se puede ni confirmar ni refutar: no hay número.

Esto no invalida la propuesta. La cambia de lugar: **la línea de base hay que
medirla antes de comparar contra ella**, y es el mismo experimento que ya estaba
pendiente.

### 1.2 El motivo bueno para ir a digital está en otro documento

El pedido justifica el bypass con: evitar el DAC de la interfaz, evitar los cables
analógicos, evitar el front-end y la conversión del AK5538. Los tres son débiles.
El AK5538 mide **−113,4 dBFS RMS** de piso de ruido (captura del 2026-08-23,
entradas 0-3) — está 20 dB por debajo de lo que puede entregar una fuente de 16
bits, y muy por debajo de lo que hace el PAM8302A.

`hardware/DS/DS11_PLANNING.md` §6 tiene el motivo de verdad, en la primera línea:

> *"El ruido captado por el cable se acumula a lo largo de la cadena (18 cajas)."*

Ese sí es un problema físico que lo digital resuelve y lo analógico no, y es el
único que justifica el trabajo. **Debería ser la tesis del documento.** Cambia
también el criterio de éxito: el experimento que importa no es "¿suena igual?"
sino "¿cuánto ruido tiene la caja 18 comparada con la caja 1?".

### 1.3 El requisito de fase se lo autoimpone la elección digital

`docs/multinode.md` §2 ya argumenta el caso contrario, con números:

> Δτ entre nodos = latencia × Δppm = 20 ms × 40 ppm ≈ 0,8 µs ≈ 0,27 mm

Y concluye: *"para obras alimentadas por la señal analógica del daisy-chain NO hace
falta ningún cable de clock."*

El razonamiento es correcto y hay que enfrentarlo, no ignorarlo: **cada nodo es una
línea de retardo analógica.** Muestrear y reconstruir con el mismo reloj reproduce
x(t−τ) exacto; el error de fase no se acumula porque la señal solo vive ~20 ms
dentro del nodo. La cadena analógica **no tiene concepto de alineación de muestra**,
así que no puede desalinearse.

En digital sí lo tiene, y por eso aparecen "cero diferencia de muestras enteras",
"cero sample slips", buffers elásticos y ASRC. **Ese requisito no viene del
beamforming: lo crea la decisión de distribuir digital.** Es un costo legítimo si
lo que se compra es la no-acumulación de ruido. Pero hay que decir que es un costo,
y hay una opción intermedia que el documento no considera (§4.3).

### 1.4 PC9 / I2S_CKIN: el dato es correcto, la conclusión es demasiado fuerte

Confirmado en el netlist línea 478:
`/ADC_AK5538/CKS0{slash}SDA{slash}CDTI: IC3/43, U7/66, RN6/2` — U7/66 = **PC9**,
con pull-up en RN6/2. El SCL es PA8 (U7/67, línea 481). El conflicto es real.

Pero I2C3 solo hace falta para **configurar** el AK5538, y en modo digital el ADC
está apagado igual. Se configura al arrancar y después se reasigna PC9 a
`I2S_CKIN`: es exclusión mutua en el tiempo, no un sacrificio permanente.

**Aun así hay que descartar la opción `I2S_CKIN`, por otro motivo.** Repartir solo
MCLK da error de frecuencia cero pero deja la **época de FS arbitraria**: cada SAI
master divide MCLK por 512, y la fase de esa división depende de cuándo se escribió
`SAIEN`. Eso es hasta 512 ciclos de MCLK = 20,8 µs = **una muestra entera de offset
desconocido en cada boot**, en cada placa. Viola directamente el primer requisito no
negociable. La hipótesis #3 del documento queda refutada, y con ella la alternativa
`I2S_CKIN` entera. No hay que compararla: hay que sacarla.

### 1.5 Lo que falta: los packs de resistencias RN1–RN4

El documento no los menciona y son el hecho práctico más importante de todo el
asunto. Cada PCM1690 tiene **su propio pack de 4 resistencias en serie** con FS,
SCK, SD y MCLK (netlist líneas 10, 17, 23, 24, 28, 42, 43, 47, 57):

| Pack | DAC | FS | SCK | SD | MCLK |
|---|---|---|---|---|---|
| RN2 | U2 = DAC0 | PE4 | PE5 | PE6 | PE2 |
| RN3 | U3 = DAC1 | PE4 | PE5 | PE3 | PE2 |
| RN1 | U? = DAC2 | PD12 | PD13 | PD11 | PE2 |
| RN4 | U4 = DAC3 | PD12 | PD13 | PE11 | PE2 |

Esto convierte la modificación del follower en algo **inyectable y reversible**:
hay pads grandes, accesibles y eléctricamente correctos donde soldar el clock que
llega del cable, sin tocar una sola pista. Y si algo sale mal, se desuelda el
cable y la placa vuelve a ser la de fábrica.

---

## 2. Lo que confirmé contra RM0433 y la errata

Todo esto lo pedía el documento. Sale bien, con una salvedad.

| Afirmación | Fuente | Resultado |
|---|---|---|
| En slave, MCLK no se genera | RM0433 §51.4.3: *"In slave mode, MCLK_x pin is not used and can be assigned to another function"* | ✅ El follower necesita MCLK externo. La ruta JP2 → J5.7 es exactamente la correcta, y PE2 queda libre para ponerlo en entrada. |
| Un bloque síncrono libera SCK/FS/MCLK | RM0433 §51.4.4: *"sees its own SCK_x, FS_x, and MCLK_x pins released back as GPIOs"* | ✅ La advertencia del documento es literal. **Sincronizar no ahorra los bodges**: los PCM1690 necesitan los clocks físicamente en sus nets. |
| Se puede sincronizar SAI3 y SAI2 con SAI1 | RM0433 Tabla 426 | ✅ SAI2 SYNCIN=0 → SAI1. SAI3 SYNCIN=0 → SAI1. Ambas existen. |
| Restricción de frecuencia en slave/sync | RM0433 §51.4.4: *"PCLK APB frequency must be higher than twice the bit rate clock frequency"* | ✅ 120 MHz > 2 × 12,288 MHz = 24,576. Pasa con 5× de margen. **Es la única restricción que encontré; no hay blocker.** |
| Orden de arranque | RM0433: *"recommended to enable the slave device before enabling the master"* | ✅ Ya es la práctica del firmware. |
| SAI3_A sin inicializar | `grep SAI3_A firmware/` → 0 resultados | ✅ Libre. |
| USB no ruteado | Netlist: `/stm32_custom/PA11: U7/70` y `/stm32_custom/PA12: U7/71`, sin nada más en la net | ✅ Confirmado. Y ojo: `unconnected-(U7-PB15-Pad54)` — PB15 (OTG_HS_DP) es un **pad pelado**, ni siquiera hay net. PB14 (OTG_HS_DM) sí sale a J5.5. |
| Presupuesto USB FS | 8×16 bit → 784 B/ms; 8×24 bit → 1176 B/ms > 1023 | ✅ La tabla del documento está bien. |
| Modo 15 al 64,8 % | `docs/ds11_perf.md:13` | ✅ |

### ⚠️ La salvedad: errata ES0392

Hay exactamente dos erratas de SAI, las dos sobre truncar el último pulso de clock
al deshabilitar un master. Con una placa sola son cosméticas. **Con 18 placas
colgando del clock de la raíz dejan de serlo**: un pulso de SCK truncado cuando la
raíz para su SAI corre la alineación de slots de todos los followers a la vez.

Dos consecuencias, las dos obligatorias:

- Poner **`OUTDRIV`** en `SAI_xCR1` (workaround de la segunda errata).
- La máquina de relock debe asumir que **cualquier disable del SAI de la raíz
  invalida a todos los followers** y dispara re-lock completo. No hay recuperación
  parcial.

---

## 3. La reestructuración que propongo

Cuatro cambios sobre el plan del documento. El primero es el que más importa.

### 3.1 Separar USB de la coherencia de fase

El documento pone USB en el gate 1. Son dos riesgos independientes y USB es el más
caro: hay que bodgear PA11/PA12, que no tienen conector, ni testpoint, ni
—probablemente— cobre cómodo donde soldar; y encima hay que escribir una clase UAC2
con endpoint asíncrono y feedback.

**La pregunta que importa es si dos DS11 pueden quedar sample-locked, y no necesita
USB para contestarse.** La raíz ya sabe generar 8 fuentes internamente: tiene
generador de tonos, walker y chirp. El gate 1 es:

> raíz genera 8 fuentes internamente → SAI3_A TX → cable → follower → modo 15

Cero trabajo de USB, cero bodges de PA11/PA12. Si eso funciona, USB es un problema
de software con un riesgo acotado. Si no funciona, no hay USB que valga.

### 3.2 La raíz no debería hacer beamforming

El documento dedica una sección entera (§"Tratamiento de root vs followers") a
garantizar que la raíz y los followers no queden con medio buffer de diferencia. Es
un problema real —5 ms son 1,7 m de camino acústico, un disparate para
beamforming— y la solución que propone es compensarlo.

**Es más barato borrarlo.** Si la raíz es un puente puro —USB o generador → SAI3_A
TX, sin salida local— entonces *todas* las placas son followers, con el mismo
firmware y la misma latencia por construcción. No hay nada que compensar porque no
hay dos caminos.

Y resuelve un segundo problema que el documento no relaciona: el modo 15 ya está al
**64,8 % de CPU**. Meterle encima la clase USB isócrona (una interrupción por ms),
el RX del link, validación de CRC y la máquina de relock, sobre el 35 % que queda,
no es cómodo. Separando roles, la raíz tiene el 100 % para USB y los followers el
100 % para beamforming.

Con 18 cajas, dedicar una es nada. Y para el banco es gratis: la DS11 #1 hace de
puente sin que le sobre ni le falte hardware.

### 3.3 Estrella, no cadena

Coincido con el documento en que la estrella es superior, y agrego el número que
cierra la discusión: con un fanout tipo DS90LV110T (91 ps de skew máximo entre
salidas), la diferencia de largo de cable domina. **1 metro de diferencia ≈ 5 ns ≈
1,7 mm de camino acústico.** Estático, medible, compensable — y despreciable frente
a la separación entre parlantes.

La cadena, en cambio, hace que el interruptor de la caja 7 sea una falla de las
cajas 8 a 18. En una instalación de arte eso no es un riesgo técnico, es un riesgo
de función.

### 3.4 Resistencia en serie en cada bodge — la cláusula de "sin riesgo para el micro"

Este es el punto que conecta con la condición no negociable del proyecto.

En el follower, los nets de PE5/PE4 quedan atados al BCLK/FS que **maneja la raíz**.
Si un follower arranca alguna vez con firmware que se cree master, hay dos salidas
CMOS peleando: contención dura, la clase de cosa que sí daña un pin. Y con OTA en el
sistema, subir el firmware equivocado a la placa equivocada es un evento probable,
no hipotético.

Tres mitigaciones, en orden de importancia:

1. **100–330 Ω en serie en cada cable de bodge.** Convierte la contención de un
   camino de daño en unos pocos mA. Cuesta nada y es lo único que hace la
   afirmación "sin riesgo" defendible.
2. Los pines liberados (PD12/PD13 en el follower si SAI2 va sincronizado) hay que
   ponerlos en **analógico/entrada explícitamente**. Dejarlos en AF es contención.
3. El rol (raíz/follower) debería salir de un **strap de hardware**, no de una
   constante de compilación. Una placa bodgeada no debería poder ejecutar el rol de
   raíz aunque le subas el binario equivocado.

---

## 4. La arquitectura que recomiendo

### 4.1 Árbol de clock

```
RAIZ (puente puro, sin salida local)
  SAI1_A  master TX  -> genera MCLK 24,576 / BCLK 12,288 / FS 48 k
  SAI3_A  slave TX SINCRONO CON SAI1 (SAI3 GCR SYNCIN=0)
          -> comparte FS/SCK de SAI1 internamente
          -> PD0/PD4 quedan liberados como GPIO
          -> datos de las 8 fuentes salen por PD1 = J5.4     [sin bodge]
  MCLK sale por PE2 -> JP2 cerrado -> J5.7                   [sin bodge]
  BCLK/FS hay que tomarlos de los pads de RN2/RN3            [2 taps]

        |  MCLK + BCLK + FS + DATA
        v  (4 pares; en banco, 4 hilos CMOS + retornos)

FOLLOWER  (x N, firmware identico)
  MCLK entra por J5.7 -> JP2 cerrado -> trunk; PE2 en ENTRADA [sin bodge]
  SAI3_A  slave RX asincrono, SCK/FS en PD0/PD4 = J5.3/J5.6   [sin bodge]
  SAI1_A  slave TX, SCK/FS en PE5/PE4                         [2 bodges]
  SAI2    SINCRONO CON SAI1 (SAI2 GCR SYNCIN=0)
          -> PD12/PD13 liberados; los clocks fisicos de
             DAC2/DAC3 se inyectan en RN1/RN4                 [2 bodges]
  AK5538 en power-down durante modo digital
```

Detalle que vale la pena: sincronizar SAI2 con SAI1 en vez de hacerlo slave
independiente deja **un solo punto de muestreo de FS en toda la placa**. Los cuatro
DAC quedan bit-idénticos por construcción, y de paso desaparece el skew entre
bancos que hoy no sabemos cuánto vale (§1.1) — también en operación standalone.

### 4.2 Modificaciones físicas, contadas

| Placa | Qué | Cuánto |
|---|---|---|
| Raíz | Tap BCLK y FS desde pads de RN2/RN3 al cable | 2 hilos + 2 R serie |
| Raíz | JP2 cerrado | jumper |
| Follower | J5.3 → net PE5, J5.6 → net PE4 | 2 hilos + 2 R serie |
| Follower | net PE5 → net PD13, net PE4 → net PD12 (en RN1/RN4) | 2 hilos + 2 R serie |
| Follower | JP2 cerrado | jumper |

**Total: 6 hilos, 6 resistencias, 2 jumpers. Cero cortes. Cero componentes
removidos. Cien por ciento reversible.** No hace falta tocar J3 ni el USB.

Lo que J5 **sí** da nativo: datos (PD1), MCLK (JP2/J5.7) y —en el follower—
BCLK/FS para SAI3_A (PD0/PD4). Lo único que no da es BCLK/FS **en los cuatro
PCM1690**. Ese es todo el hueco, y son esos 6 hilos.

### 4.3 La opción que el documento no considera

Antes de comprometerse con digital conviene poner sobre la mesa la opción
intermedia, porque ataca el problema real (§1.2) sin crear el problema de fase
(§1.3):

**Distribución analógica en estrella, con un splitter buffereado activo.** El ruido
deja de acumularse porque cada caja recibe una copia fresca y regenerada; cada tramo
capta su propio ruido, sin sumarse al del anterior. Cero trabajo de firmware, cero
riesgo de fase, cero bodges.

Pierde por cableado: 8 canales × 18 cajas desde un punto central son muchos más
metros que la cadena. Un híbrido (3 grupos de 6, con un repetidor buffereado en la
cabeza de cada grupo) probablemente sea el óptimo real si el ruido resulta ser el
único problema.

**No la recomiendo por encima de digital, pero sí digo que hay que medir el ruido
acumulado antes de elegir.** Es el dato que decide, y hoy no existe.

### 4.4 LVDS vs A2B

| | LVDS + fanout | A2B (AD242x/243x) |
|---|---|---|
| Cable | 4 pares (Cat5 / ix Industrial) | 1 par trenzado, con alimentación |
| Topología | Estrella natural | Cadena natural |
| Latencia por nodo | ~ns, sin acumulación en estrella | Determinística <50 µs, pero con offset por posición **que hay que verificar** |
| Clock a los PCM1690 | Directo: es el mismo BCLK/FS físico | **Riesgo abierto**: hay que confirmar que CLKOUT entrega 512×fS = 24,576 MHz |
| Herramientas | Ninguna | SigmaStudio, NDA, licencia |
| Costo/nodo | ~USD 3–5 | ~USD 10–15 |
| Disponible hoy | Sí | Sí, con fricción |

**Para el prototipo y para 10–18 nodos: LVDS en estrella.** Es más simple, usa
partes que se compran hoy, y el clock llega a los DAC sin intermediarios — que es
precisamente donde A2B tiene su riesgo no resuelto (el punto 2 de la lista de
verificaciones del documento).

**A2B recién se gana el lugar en producción**, y con una condición previa: confirmar
en el datasheet que CLKOUT1/2 puede entregar 24,576 MHz. Si no puede, A2B necesita
un PLL/generador extra por nodo y pierde buena parte de su ventaja.

---

## 5. Sobre los 16 bits

El documento pregunta por la calidad efectiva de 16 bits y la respuesta es
incómoda para la propuesta:

- Piso de ruido medido del AK5538: **−113,4 dBFS RMS** (entradas 0-3, captura del
  2026-08-23, `docs/protocolo_medicion.md` §D).
- Piso de una fuente de 16 bits con dither: **≈ −93 dBFS**.

**El camino "digital limpio" a 16 bits es 20 dB peor que el camino analógico que
reemplaza.** El argumento de calidad no solo no se sostiene: apunta al revés.

Eso no lo mata —lo que compra el digital es la no-acumulación de ruido en 18 cajas,
no la calidad de la primera conversión— pero sí obliga a medir antes de decidir.
La pregunta concreta es dónde está el piso de ruido del PAM8302A + parlante. Si
está por encima de −93 dBFS, los 16 bits son gratis y la discusión se termina. Si
está por debajo, hay que ir a USB High-Speed o a 24 bits por otro camino.

**Ese experimento ya es posible**: micrófono a una entrada, `python3
tools/captura.py`, silencio digital, leer el RMS. Es media hora.

---

## 6. Plan de gates

Cada gate produce un número que decide el siguiente. Ninguno necesita PCB nueva.

| Gate | Qué se prueba | Necesita | Criterio |
|---|---|---|---|
| **0** | Skew analógico real entre bancos, hoy, una placa | puente J13 (pendiente) + modo 19 | Es la línea de base. Sin esto no hay contra qué comparar. |
| **0b** | Piso de ruido del amplificador+parlante | mic + `captura.py` | Decide si 16 bits alcanzan (§5). |
| **1** | SAI3_A sincronizado con SAI1 funciona | solo firmware, una placa | ¿Puede un bloque sincronizarse a un SAI que a su vez es *slave*? RM0433 no lo prohíbe pero tampoco lo afirma. **20 minutos de banco, y es el único desconocido de papel que queda.** |
| **2** | Dos DS11 sample-locked | 6 hilos + JP2 | Impulso simultáneo, mic, `captura.py` → correlación cruzada. **La herramienta ya existe y reporta en muestras / µs / mm.** |
| **3** | Ruido acumulado, cadena vs estrella | 2–4 placas | El número que justifica todo el proyecto (§1.2). |
| **4** | USB UAC2 en la raíz | bodge PA11/PA12 | Recién acá, y sobre un problema ya resuelto. |

El gate 2 es el que contesta la pregunta del documento, y su instrumento de medida
ya está construido y verificado: `tools/captura.py` baja 8 canales crudos y calcula
retardo entre canales por correlación cruzada, en muestras, µs y mm. No hace falta
osciloscopio.

---

## 7. Respuesta a la pregunta final del documento

> *¿Puedo aprovechar las DS11 fabricadas para demostrar un bypass digital y una
> cadena phase-coherent, o los conectores expuestos no alcanzan?*

**Sí, alcanzan casi enteros.** J5 entrega nativamente los datos (PD1), el MCLK
(JP2/J5.7) y, en el follower, el BCLK/FS para SAI3_A (PD0/PD4). Lo único que J5 no
entrega es BCLK/FS **en los cuatro PCM1690** — y los packs RN1–RN4 dan pads limpios
para inyectarlos.

> *¿Qué mínimo hardware adicional y qué modificaciones hacen falta?*

**Seis hilos, seis resistencias de 100–330 Ω, dos jumpers JP2, un cable de 10 vías
entre J5 y J5 con los pines 9 y 10 (3V3/5V) cortados.** Nada más. Sin USB, sin
LVDS, sin daughterboard: a 30 cm de distancia, 12,288 MHz en CMOS con retorno de
masa y resistencia serie es perfectamente sano. LVDS recién hace falta cuando el
cable pase de ~1 m.

Y hay una condición de método: **medir el gate 0 antes que nada.** El documento
razona contra una línea de base que no existe.
