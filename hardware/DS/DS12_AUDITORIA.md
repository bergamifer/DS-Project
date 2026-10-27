# DS12 — Auditoría de hardware previa a fabricar

> **11 de septiembre de 2026.** Doce dimensiones sobre el netlist de producción de la DS11, el
> PCB, los datasheets y el firmware. 37 hallazgos. Lo que sigue es sólo lo que sobrevivió a
> verificación: cada ítem dice cómo se comprobó.
>
> Método: un agente por dimensión + un refutador adversarial con instrucción de descartar ante
> la duda. Los hallazgos de `dfm`, `test` y `connectors` **no pasaron por refutación automática**
> (se agotó el presupuesto); los marcados ✅ los verifiqué a mano, el resto está marcado ⚠️.

---

## El veredicto en tres líneas

1. ~~Stock~~ — **no aplica**: los componentes ya están comprados (Fernando, 2026-09-11).
2. **El archivo del que vas a partir tiene un corto latente**: `DS.kicad_pcb` está guardado con
   los rellenos viejos y SCL/SDA tocan el plano de masa. Verificado con DRC. Son 5 minutos.
3. **La arquitectura está sana.** No apareció nada que obligue a repensar el diseño: todo lo
   grave es stock, proceso, o correcciones a documentos.

---

## ~~1. Bloqueantes de fabricación — stock~~ ✅ NO APLICA

> **Fernando, 2026-09-11: los componentes críticos ya están comprados, y los genéricos
> (capacitores, resistencias) también.** Toda esta sección queda sin efecto — el stock de
> JLCPCB no es una restricción para esta tirada.
>
> Se conserva sólo lo que sigue siendo útil como método:
>
> **El stock se verifica contra `hardware/DS/jlcpcb/production_files/BOM-DS.csv`, no contra el
> netlist.** Los dos no coinciden: para el 4,7 nF, `DS.txt` dice `C883564` y el BOM de
> producción dice `C2961271`. Yo mismo verifiqué el equivocado y di por refutado un hallazgo
> que era correcto.

---

## 🔴 2. Bloqueante de proceso — el archivo tiene un corto guardado

**Verificado con kicad-cli, no inferido.** Sobre `hardware/DS/DS.kicad_pcb` tal como está en el repo:

| DRC | violaciones |
|---|---|
| con los rellenos **guardados** (lo que se plotearía hoy) | **32** |
| con `--refill-zones` (cobre recalculado) | **12** |

Las **20 de diferencia son todas de I2C1**, en dos puntos:

| net | posición | clearance al plano GND | clearance de agujero |
|---|---|---|---|
| `I2C1_SDA` | (166,10 · 108,26) | **0,0000 mm** | 0,1307 mm |
| `I2C1_SCL` | (167,01 · 110,05) | **0,0000 mm** | **0,0000 mm** |

SCL y SDA tocando masa. Ese bus habla con los cuatro PCM1690 y la EEPROM: ploteado así, ningún
DAC responde. Con refill las veinte desaparecen → **el diseño está bien, el cobre guardado no.**

Es el **mismo mecanismo de las dos fallas de fabricación de la DS11** (`docs/ds11_errata.md`), con
la firma idéntica: *"DRC con rellenos almacenados → 0.0000 mm; con --refill-zones → desaparecen"*.

**Arreglo: abrir en pcbnew, `B`, guardar, commitear.** Cinco minutos.

> **Y el corolario, que vale más que el arreglo:** la regla del §2c existe desde la DS11 y aun así
> el archivo llegó hasta hoy sin refillear. Una regla que depende de que alguien se acuerde ya
> falló dos veces. Es automatizable con los dos comandos de arriba en un hook de pre-commit:
> si `drc_stored` tiene más violaciones que `drc_refill`, no deja commitear.

---

## 🟢 3. Alto valor, bajo costo

Ordenados por relación valor/costo. Todos verificados contra el digest del netlist.

### 3.1 J5, J6 y J7 son el mismo conector físico ✅

Los tres son `JST_SH_BM10B-SRSS-TB_1x10` con el **mismo LCSC `C5343037`**. El cable del ST-Link
entra perfecto en los puertos de expansión — y **J7.9 = GND mientras J5.9 = +3V3**, así que
enchufarlo en el lugar equivocado pone masa contra el riel digital.

**Arreglo de costo cero: pasar J7 a un JST SH de 8 o 9 pines.** El de 8 no entra en el molde de
10 y el error se vuelve mecánicamente imposible. J7 usa 8 señales reales (VTref, SWDIO, SWCLK,
GND, SWO, NRST, UART7_RX, UART7_TX) más el pin 10 de alimentación.

### 3.2 Las 8 entradas del ADC no tienen bias DC ⚠️

Los 16 pines `AINnP/N` van al conector y al capacitor de acople, y **a nada más**: no hay red que
los ponga en la ventana de 0,48–0,52×AVDD que pide el datasheet. Sin micrófono enchufado flotan.

**Arreglo: un nodo VCM compartido** (divisor 10k/10k desde `+3,3VA` → 1,65 V, con 10 µF + 100 nF)
y una resistencia por pin. **Usar 22 kΩ, no 100 kΩ**: el ruido térmico de dos resistencias de
100 k (~40 nV/√Hz) baja la DR a ~104 dB y se come los 111 dB del chip. ≈US$0,30 y 20 pasivos.

> 🔧 **2026-10-27 — revisión.** El argumento de ruido vale sólo con la entrada **abierta**: con un micrófono
> enchufado, la fuente (470 Ω + preamp, a través de 2,2 µF) cortocircuita el ruido de la R de bias, que queda
> por debajo de 2 nV/√Hz incluso con 100 kΩ, contra ≈13 nV/√Hz del propio ADC. **Se usa 10 kΩ** (`C25744`, el
> mismo valor del divisor): pasa-altos de ≈7 Hz con los 2,2 µF y una sola pieza. Lo que **no** está resuelto
> es contra qué tensión está la resistencia de entrada de ≈10,4 kΩ (hoja, «Input Resistance»): si es contra
> ≈0 V, ninguna red pasiva lleva la pata a 1,65 V. Lo decide la medida **M4b** de
> [`revision_alimentacion/medidas_ds11.md`](revision_alimentacion/medidas_ds11.md).

### 3.3 Cuatro PTC en la expansión, no dos ✅

`J5.9` y `J6.9` llevan `+3V3` directo (verificado en `pines_por_ic.txt`). El §6 de
`DS12_ALIMENTACION.md` afirma lo contrario. Hacen falta **cuatro** PTC —`BSMD1206-050-30V`
(`C883126`)— uno por cada pin de alimentación, no uno por conector. US$ 0,12.

### 3.4 Pull-up en AMUTEO ✅

Cuatro salidas open-drain del PCM1690 en wired-OR contra PC10, **sin ningún pull-up externo**:
hoy funciona sólo porque el firmware habilita el interno. Una resistencia de 10 kΩ a `+3V3`,
US$ 0,001. (Confirmado también por la dimensión `dac`.)

### 3.5 Pads de prueba para el SAI ✅

No hay un solo punto donde pinchar el bus que causó el bug más caro del proyecto. Una fila de
**8 pads a paso 2,54 mm** —la huella de una pinza de analizador lógico— con MCLK, SCK, FS y los
cuatro SD. **US$ 0.** Sacar el stub del lado del STM32, no del DAC, para no cargar la entrada.

### 3.6 Los 4 LEDs de los jacks ya están cableados y el firmware los ignora ✅

R111-R114 (470 Ω) existen y van a **PE12 compartido**, que `main.c` declara como pin sin usar en
modo analógico. Llevando cada uno a su propio GPIO libre (PE9, PE10, PE13, PE14) hay indicación
de señal por canal **sin agregar un solo componente**.

### 3.7 Partir el `~SD` de los amplificadores en cuatro ⚠️

Los 32 PAM comparten una sola línea de shutdown: no hay forma de aislar un banco ni de detectar
un parlante abierto. Cuatro GPIO libres (PD14, PD15, PD5, PD6) + 3 pares de R ≈ **US$ 0,01**.

---

## ✏️ 4. Correcciones a lo ya documentado

Lo más valioso del informe: dónde los documentos que vas a usar como guía dicen algo equivocado.

| documento | dice | realidad |
|---|---|---|
| `DS12_ALIMENTACION.md:41,:164` y `DS12_PLANNING.md:109` | C56 es cerámico, "≈9 µF reales por DC bias" | ✅ **es un electrolítico de aluminio** 22 µF/50 V (`C72505`, footprint `CP_Elec_6.3x5.4`). La derating por DC bias **no aplica**: 22 µF son 22 µF, y la DS11 **ya tiene** amortiguación en la entrada |
| `DS12_ALIMENTACION.md:482` (§6) | un corto en la expansión "no toca el riel digital" | ✅ **es al revés**: J5.9/J6.9 llevan `+3V3` |
| `DS12_ALIMENTACION.md:262` | "los 6,8 µH son del datasheet, renglón de 3,3 V" | ⚠️ en DS42002 los 6,8 µH son el renglón de **5,0 V**; el de 3,3 V pide 4,7 µH y 56 pF. Y el "196 k / 100 pF del datasheet" que se cita **no existe** en esa tabla |
| `DS12_ALIMENTACION.md:270` | Cff = 220 pF | ⚠️ pone el par cero-polo **2,6× por debajo** del diseño de referencia. Con el divisor 62k/20k, **100 pF** reproduce el cero y el polo de Diodes |
| `DS12_ALIMENTACION.md:375` (§3.4) | margen USB de 290 mV con 0,5 A | ⚠️ la tabla del §3.3 dice **900 mA de pico**; el margen real es 70-130 mV, y F2 (750 mA de hold) queda **por debajo del consumo** |
| `DS12_PLANNING.md:744` | series de MCLK = 75 Ω | ⚠️ valor elegido por stock, y asimétrico (75 Ω a los DAC, 33 Ω al ADC). Unificar en 33 Ω **y ponerlas junto al STM32** |
| `DS12_PLANNING.md` §3 | usar PC13 como línea de timing | ✅ **choca con la errata es0392 §2.2.30**: conmutar PC13 en LQFP corrompe el LSE, workaround "ninguno" |
| `DS12_PLANNING.md` §4 | el bead de salida "filtra bien" | ⚠️ el datasheet dice que actúa **de ~1 MHz para arriba** y `fOSC` está en 200-300 kHz |
| backlog varios | el conflicto PE3/PE11 sigue pendiente | ⚠️ **ya no existe en DS11**: es herencia de DS9. Sacarlo del backlog |
| `DS12_PLANNING.md` §2 | el tap de VOUT cierra el lazo DAC→ADC | ⚠️ **no cierra**: las 8 entradas del AK5538 ya están ocupadas por J9. Hay que llevarlo a un ADC del STM32 (PC0 está libre) |

---

## ⚠️ 5. Para considerar

- **El UVLO del buck A crea un modo que la DS11 no tenía**: los rieles analógicos hacen un ciclo
  completo mientras el digital nunca se cae, y el AK5538 queda con TVDD vivo, AVDD en cero y PDN
  en alto — **nunca recibe su pulso de reset**. Arreglo: `PDN_AK = PC7 AND PWR_GOOD_5V`, dos
  componentes. *(Mismo mecanismo que el riesgo sobre los pines SD de los PAM.)*
- **MCLK es un árbol de 112,5 mm con 11 nodos y cero terminación de fuente.** Funciona porque el
  SCKI del PCM1690 es Schmitt trigger, pero el margen es cero. Un buffer de fanout (74LVC125A,
  ~US$0,10) con 5 salidas punto a punto lo resuelve.
- **In2 está partido al 35 %** mientras In1 es un plano impecable: media cara B rutea sin
  referencia continua, incluidos relojes del SAI, el I2C y una entrada de micrófono.
- **Siete PAM8302A tienen su único pin de masa colgando por un solo radio térmico de 0,2 mm** —
  es el retorno *y* el disipador por el mismo cuello.
- **Los 32 PAM son osciladores libres de 200-300 kHz sin sincronizar**, y de las cuatro barreras
  que escribí contra el batido en `DS12_ALIMENTACION.md` §3.3, **tres no aplican a este caso**.
- **El encapsulado de los PAM puede ser SO-8 y no MSOP-8** (θJA 115 vs 180 °C/W). §0.3 decide una
  migración cara sobre los 180: hay que resolver cuál es antes de gastar.
- **El pulsador K1 rotulado "BOOT ESP32" está cableado a EN**: es un reset, y la placa no tiene
  forma de forzar el download boot.
- **38 de 71 líneas del BOM son Extended** (~US$114 de setup por pedido); tres tienen equivalente
  Basic sin costo eléctrico.

---

## ❌ 6. Descartado explícitamente

- **El 4,7 nF `C883564` con 4920 unidades** — no es el que usa producción. Ver §1.2.
- **Subir R_I de 1 k a 20 k** (§0.7): ya estaba retirado y la auditoría lo confirma.
- **Igualar largos de pista para corregir el skew entre bancos**: el skew de layout es de **67 ps**
  contra los 4 µs medidos — tres órdenes de magnitud. Los 4 µs son arranque de periféricos, no
  layout. El arreglo real es llevar BCK/LRCK de SAI1 a los cuatro DAC.
- **Un segundo TVS en `+24V` sin fusible aguas arriba**: el refutador señaló que cada placa de la
  cadena lleva su propio F1+D8′, así que un surge ve N clamps en paralelo; y un TVS sin impedancia
  serie suma fuga y ~1 nF de capacitancia por nodo. La discusión correcta es F1 como PTC de 30 V.

---

## 🔍 7. Lo que no se pudo verificar

- Los hallazgos de `dfm`, `test` y `connectors` **no pasaron por refutación adversarial**. Los
  marcados ✅ los verifiqué a mano; los ⚠️ están sin contrastar.
- **El crítico de completitud nunca corrió**: puede haber dimensiones enteras sin mirar.
- El encapsulado real de los PAM8302A (el BOM dice una cosa y el footprint otra).
- Si `C7205134` está realmente de baja o es un hueco del índice — **verificar en jlcpcb.com/parts**.
- La geometría real de la estructura de madera y el ruteo físico del arnés.
- El dieléctrico real de los 2,2 µF de VCAP (`C23630`): el netlist no trae el campo.

---

## Orden sugerido para esta semana

1. **Refillear zonas y commitear** — 5 minutos, y desbloquea todo lo demás. *(§2)*
2. **Hook de pre-commit** que impida que el §2 vuelva a pasar. *(§2)*
3. Aplicar las correcciones del §4 a los documentos **antes** de dibujar desde ellos. ✅ hecho
4. Los ítems del §3 mientras dibujás: son todos baratos y ninguno cambia la arquitectura.

---

## ✅ Aplicado el 2026-09-11

Verificado, no sólo escrito. Cada ítem dice cómo se comprobó.

| Qué | Cómo se verificó | Estado |
|---|---|---|
| **Rellenos de zona refilleados** en `DS.kicad_pcb` | DRC **32 → 12** violaciones; **0** de I2C1. El diff toca **sólo líneas `(xy ...)`** — ninguna pista, vía ni footprint | ✅ |
| **`pad_to_mask_clearance` 0,1 → 0,05 mm** (estándar JLCPCB) | DRC **12 → 5**; los **7 puentes de máscara desaparecen**, incluidos los tres del ADC y los dos del USB-C | ✅ |
| **`tools/check_pcb.sh`** + hook de pre-commit | Probado en las dos direcciones: bloquea con el archivo viejo (detecta las 20, 11 en `I2C1_SCL` y 9 en `I2C1_SDA`) y pasa con el nuevo | ✅ |
| **`TX_MAX_DELAY_SAMPLES` 128 → 192** + `_Static_assert` | Compila limpio. Con `SPK_LAYOUT_2X16=1` y 128, **el assert frena la compilación** — forzando el rebuild, porque `make` puede no rehacer `main.o` y dar un falso OK | ✅ |
| **Interruptor `SPK_LAYOUT_2X16`** (0 = DS11, 1 = DS12) | Las dos variantes compilan. El entrelazado ya estaba implementado vía `SPK_ROW_DX_MM` | ✅ |
| **Banner sin números a mano** | Los `7 ×` y `3 ×` hardcodeados ahora se derivan de `SPK_ROWS`/`SPK_COLS`, y el banner imprime diagonal y retardo máximo | ✅ |
| **11 correcciones a los documentos** | 8 en `DS12_ALIMENTACION.md` (+ §6.1 nuevo), 5 en `DS12_PLANNING.md`, cada reemplazo verificado | ✅ |
| **`tools/netlist_digest.py`** | Regenera el digest; reproduce los hechos ya verificados | ✅ |

### Las 5 violaciones que quedaban — y por qué NO las toqué

> **Decididas por vos el mismo día.** 5 → 1. Ver **§ Las tres decisiones del 11-sep** más abajo.

| Qué | Por qué es tu decisión, no mía |
|---|---|
| 3× separación de pads en IC3 (0,1414 mm contra la regla propia de 0,18) | Es **inherente al QFN-64 de paso 0,5 mm**, y 0,1414 mm está **por encima del mínimo de JLCPCB** (0,127 mm). O sea que sobra la regla, no el footprint. El arreglo correcto es un *override* local de clearance en IC3 — relajar una regla de diseño, que es una decisión tuya |
| Código de barras contra el texto `JLCJLCJLCJLC` | Es el ítem §7 del backlog (el serial salió literal en la DS11). Hay que decidir qué serial va, no moverlo |
| Código de barras sobre una vía de `SAI_FS` | Mover el barcode o la vía: decisión de layout |

### Lo que sigue pendiente

- **El hook sólo existe en tu máquina** (`.git/hooks/` no se versiona). En otra copia del repo hay que correr `tools/check_pcb.sh --install`.
- **Los ítems del §3** (bias del ADC, J7 de 8 pines, 4 PTC, pull-up de AMUTEO, pads del SAI, LEDs, `~SD` en cuatro) son cambios de esquemático: van mientras dibujás.
- **`dfm`, `test` y `connectors` sin refutación adversarial**, y el **crítico de completitud nunca corrió**.

---

## Las tres decisiones del 11-sep (tarde)

De las 5 violaciones que quedaban, **4 están cerradas**. Queda 1, y es la que se
va sola cuando muevas el texto del serial.

### 1. IC3 a 0,1414 mm — regla propia, no *override* de footprint

Hay dos formas de aceptar esa distancia y **no son equivalentes**:

| | Alcance | Riesgo |
|---|---|---|
| *Override* de clearance en el footprint (Footprint Properties) | Baja el piso de **IC3 contra todo**: pads, pistas ajenas, vías, plano | Mañana ruteás una pista a 0,13 mm del QFN y el DRC no dice nada |
| **Regla propia con condición** ← lo que se hizo | Baja el piso **sólo entre dos pads que sean los dos de IC3** | Ninguno: todo lo demás sigue exigiendo 0,18 mm |

Está en **`hardware/DS/DS.kicad_dru`** (nuevo). KiCad lo lee solo por llamarse
igual que el proyecto; no hay que importarlo. Se ve y se edita desde
*Board Setup → Design Rules → Custom Rules*, y ahora **viaja en el repo**: si
mañana abrís el proyecto en otra máquina, la regla está.

Verificado: DRC **5 → 2** violaciones, y las tres que desaparecieron son
exactamente las de IC3 (pads 1/64, 16/17 y 32/33).

### 2. El QR: la regla no se puede apagar — probado de cuatro maneras

Esto no es una opinión, es lo que hace KiCad 10.0.3 en esta placa:

| Intento | Resultado |
|---|---|
| Regla propia con `(min 0mm)` sobre el barcode | **la ignora** |
| Regla propia **global**, sin condición, con `(min 0mm)` | **la ignora** — y esa misma regla sí apagó los 3 errores de IC3, o sea que el archivo se está leyendo |
| `(margins 0 0)` en el objeto barcode | no cambia nada |
| Severidad `ignore` en el Board Setup | **no existe la entrada**: el error se reporta con el tipo genérico `clearance`, así que ponerlo en ignore apagaría **todos** los clearance de cobre de la placa |

Los 1,0 mm son un piso interno del programa ("barcode visual separation
default"). Una regla propia puede **subirlo**, nunca bajarlo.

Lo único que funciona es excluir violación por violación — y ahí está la
explicación de tus "muchos errores": **el proyecto ya tenía 24 exclusiones
guardadas, las 24 contra este mismo QR**. Cada vez que movés algo cerca,
aparecen nuevas.

Se agregó la 25ª (QR contra la vía de `SAI_FS`), verificada: DRC **2 → 1**.

La que queda es **QR contra el texto `JLCJLCJLCJLC`** — y esa la resuelve el
punto siguiente, porque ese texto se va a mover igual.

### 3. `JLCJLCJLCJLC` — cómo hacer que JLCPCB lo tome como número de orden

**Qué es.** JLCPCB imprime su número de orden en **toda** placa que fabrica: es
cómo rastrean el lote. La única elección que te dan es *dónde*. `JLCJLCJLCJLC`
es el marcador que buscan en la serigrafía: donde encuentran esas 12 letras,
imprimen el número en su lugar.

**Por qué en la DS11 salió el texto literal.** Poner el marcador no alcanza:
hay que **pedirlo en el pedido**. En la página de orden, en *Remove Order
Number*, la opción es **"Specify a location"** (no "No", que es el default y
hace que lo pongan donde ellos quieran). Si no la marcás, nadie busca el
marcador y las 12 letras se imprimen tal cual — que es exactamente lo que pasó.

**Lo que ya está bien en la placa** (verificado en `DS.kicad_pcb`):

- el texto es exactamente `JLCJLCJLCJLC`, sin espacios ni sufijos;
- está en **F.SilkS**, no espejado;
- tamaño 1 × 1 mm, negrita — dentro de lo que imprimen sin problemas;
- hay **una sola** instancia en toda la placa.

**Lo único a corregir: dónde está.** Hoy está a 0,80 mm del QR, rotado 90°.
Dos razones para moverlo:

1. es la última violación de DRC que queda, y se va sola;
2. el número de orden **tiene más dígitos que el marcador**, así que crece:
   dejale aire alrededor, no lo encajes contra otra cosa.

Ponelo en un área libre de serigrafía —cerca del borde, lejos del QR— y el DRC
queda en **0**.

---

## Dos cosas para reportarle a KiCad

Las dos salieron de esta auditoría y las dos son de KiCad, no de la placa.
El repo de KiCad **no está en GitHub**: es GitLab, `gitlab.com/kicad/code/kicad`.
Hace falta cuenta de GitLab (no sirve la de GitHub).

### A. `kicad-cli` plotea cobre viejo sin avisar

Es el mecanismo que ya arruinó dos tandas de la DS11 y que hoy encontramos
latente por tercera vez.

Estado real de KiCad 10.0.3 — hay que decirlo con precisión, porque **ya está
medio resuelto**:

- la **GUI sí avisa** antes de plotear ("Check zone fills before plotting",
  con un "Continue without Refill");
- **`kicad-cli pcb export gerbers` tiene `--check-zones`** ("Check and refill
  zones if required")… **apagado por default**;
- **`kicad-cli pcb drc` no dice una palabra** sobre rellenos desactualizados, ni
  siquiera como *warning*: corre el DRC sobre el cobre viejo y sale 0.

O sea que el pedido no es "agreguen la función" sino algo mucho más chico y más
difícil de rechazar: **que el silencio no sea el default en la línea de
comandos**. Un *warning* —no un error, no un refill automático— cuando el cobre
guardado difiere del recalculado. Justo lo que hace `tools/check_pcb.sh`, que es
la prueba de concepto: dos corridas de DRC y una comparación.

### B. La regla del barcode no se puede bajar ni apagar

Con la evidencia de la tabla del punto 2. El pedido concreto: **una entrada de
severidad propia** (hoy se reporta como `clearance` genérico, así que la única
manera de apagarla es apagar todos los clearance de cobre), **o** que la regla
implícita respete una regla del usuario, como hace cualquier otro clearance.

Caso de uso real y aburrido: un QR decorativo con el link al repo, que se lee
con un teléfono, en una placa donde nadie va a pasar un lector óptico.
