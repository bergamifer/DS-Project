# Revisión para Claude: bypass digital USB y cadena multinodo coherente en fase sobre DS11

> Fecha de contexto: 2026-08-19  
> Uso: entregar este documento completo a Claude junto con el repositorio DS-Firmware.  
> Esta primera revisión es de viabilidad. No implementar firmware ni modificar PCB todavía.

## Rol y criterio de revisión

Actuá como ingeniero senior de audio digital embebido, clocking, STM32H7 y diseño de
interconexiones. Antes de concluir, verificá el firmware, el esquemático/netlist y los
manuales oficiales. No confirmes la propuesta por inercia: intentá refutarla.

Necesito un veredicto explícito y separado para:

1. Un prototipo de laboratorio con **dos DS11 ya fabricadas** y modificaciones manuales.
2. Una instalación con aproximadamente **10 a 18 DS11**.
3. Una revisión futura de PCB o una daughterboard de producción.

Si una afirmación de este documento está equivocada, corregila con evidencia. Si hay
contradicciones entre documentos históricos y el hardware o firmware actual, priorizá:

1. Netlist y esquemático actuales.
2. Código actualmente ejecutado y mediciones reales.
3. Manuales/datasheets oficiales.
4. Documentos históricos del proyecto.

No alcanza con decir que dos señales tienen la misma frecuencia. Para este instrumento
hay que demostrar coherencia de **frecuencia, frame, epoch y latencia de pipeline**.

## Qué es DS11

DS11 es un instrumento de beamforming con esta arquitectura por placa:

```text
8 entradas analógicas
    -> AK5538VN, ADC TDM256
    -> STM32H743VIT6 a 480 MHz
    -> 8 delay lines + delays focales Q8
    -> cuatro streams TDM8
    -> 4 x PCM1690
    -> 32 amplificadores PAM8302A
    -> 32 parlantes
```

El modo 15 toma ocho fuentes independientes y proyecta cada una hacia un foco diferente
usando los 32 parlantes. El hot path correspondiente está en
`firmware/Core/Src/main.c`, alrededor de `mfocus_in_enabled`.

Estado medido relevante:

- `fs = 48 kHz`.
- Buffers DMA de 240 frames, es decir, 5 ms por half-buffer.
- MCLK actual: 24,576 MHz.
- BCLK TDM8/TDM256: 12,288 MHz.
- Cada frame TDM contiene 8 slots de 32 bits.
- El modo 15 usa 64,8 % de CPU y fue medido con cero underruns.
- Los dos masters SAI de salida actuales comparten PLL3 pero no fase. Su arranque quedó
  medido con aproximadamente 1 us de skew estable entre bancos.
- La resolución temporal de los delays Q8 es:

```text
1 / (48.000 x 256) = 81,38 ns
```

Referencias internas: `docs/audio_architecture.md`, `docs/ds11_perf.md` y
`firmware/Core/Src/main.c`.

## Objetivo propuesto

Quiero evaluar si es viable reutilizar las DS11 fabricadas con algunas modificaciones
y eventualmente construir esta arquitectura:

```text
Computadora / DAW
        |
        | USB Audio Class, 8 fuentes digitales
        v
DS11 raiz o bridge USB
        |
        +--> beamforming local -> 32 salidas de la raiz
        |
        +--> bus digital sincrono de 8 fuentes
                  |
                  v
             DS11 follower 1 -> beamforming local -> 32 salidas
                  |
                  v
             DS11 follower 2 -> beamforming local -> 32 salidas
                  |
                 ...
```

Las ocho pistas USB reemplazarían a las ocho muestras del AK5538 como productoras de
`delay_line[0..7]`. Así se evitarían:

- El DAC de una interfaz de audio externa.
- Los cables analógicos entre placas.
- El front-end analógico y la conversión del AK5538 para el contenido musical.

Se mantienen el procesamiento digital local, los PCM1690, amplificadores y parlantes.

No quiero mandar 32 canales diferentes a cada placa. Quiero distribuir las mismas ocho
fuentes y que cada STM32 calcule localmente sus 32 salidas usando su geometría y delays.

## Requisito no negociable: coherencia temporal entre placas

Como es beamforming, no se acepta ningún desfase desconocido, variable o acumulativo
entre placas. Interpretar el requisito de esta manera:

- **Cero diferencia de muestras enteras** entre placas después de cualquier boot o
  recuperación del enlace.
- **Cero sample slips** durante operación continua.
- El skew submuestra debe estar cuantificado contra una tolerancia acústica explícita.
- Un offset fijo por placa o por hop sólo sería aceptable si es estable, medible y se
  compensa explícitamente en los delays. No debe quedar oculto en un buffer.
- Los cambios de foco, ganancia, mute o modo deben aplicarse en el mismo `frame_id`.
- Una pérdida de clock, frame o datos debe producir mute y relock controlado; nunca la
  repetición silenciosa de un bloque viejo.

Pedimos que calcules un presupuesto de fase para dos bandas:

1. Banda espacial útil documentada del instrumento, hasta aproximadamente 2,1 kHz.
2. Banda de audio completa hasta 20 kHz, para saber qué margen real queda.

Usar:

```text
error_temporal_max = error_de_fase_grados / (360 x frecuencia_maxima)
```

No uses expresiones como “cero latencia” o “sin fase” sin convertirlas en nanosegundos,
grados, muestras y variación entre boots/temperatura.

## El USB de la computadora

La hipótesis de trabajo para la raíz es USB Audio Class class-compliant, no un driver
propietario de macOS.

La DS11 fabricada no tiene el USB del STM32 ruteado:

- PA11/PA12 sólo llegan a las patas U7/70 y U7/71.
- El USB-C J3 pertenece al ESP32-C3.
- Para un prototipo habría que reutilizar J3 mediante cortes/bodges o agregar un
  breakout soldado a PA11/PA12.

MVP ya estimado:

| Stream USB a 48 kHz | Peor paquete aproximado | USB Full-Speed |
|---|---:|---|
| 8 x 16 bit | 784 bytes/ms | viable |
| 10 x 16 bit | 980 bytes/ms | entra, muy cerca de 1023 |
| 8 x 24 bit packed | 1176 bytes/ms | no entra en un único endpoint FS |

Para el primer prototipo se propone `8 x 16-bit / 48 kHz`. Para preservar 24 bits reales
habría que evaluar USB High-Speed mediante un bridge externo, por ejemplo XMOS, o una
nueva revisión de hardware.

El reloj maestro debe ser el de audio de la raíz. El endpoint USB OUT debe ser
asíncrono y devolver feedback para que la computadora siga al reloj DS; no queremos
que cada DS haga ASRC o descarte/duplique muestras para seguir al host.

Evaluá también la calidad efectiva de 16 bits frente al sistema real, incluyendo:

- Headroom necesario al sumar ocho fuentes, cuyo peor caso coherente es +18,06 dB.
- Atenuación digital actual antes del PAM8302A.
- Ruido/SNR del PCM1690 y PAM.
- Diferencia entre “evitar el ADC” y “conservar 24 bits end-to-end”.

## Conectores de expansión disponibles

La DS11 fabricada tiene dos JST-SH de 10 pines, pitch 1 mm.

### J5 — Exp_port_A

| Pin | Señal | MCU | Uso potencial |
|---:|---|---|---|
| 1 | I2C2_SCL | PB10 | Configuración de daughterboard |
| 2 | I2C2_SDA | PB11 | Configuración de daughterboard |
| 3 | SAI3_SCK_A | PD0 | BCLK TDM |
| 4 | SAI3_SD_A | PD1 | DATA TDM, RX o TX según rol |
| 5 | I2S2_SDI | PB14 | Entrada I2S2, no asumir TDM8 |
| 6 | SAI3_FS_A | PD4 | Frame sync TDM |
| 7 | MCLK mediante JP2 | PE2/trunk | Salida en raíz o entrada en follower |
| 8 | GND | - | Tierra local |
| 9 | +3V3 | - | Alimentación local daughterboard |
| 10 | +5V | - | Alimentación local daughterboard |

### J6 — Exp_port_B

| Pin | Señal | MCU | Uso potencial |
|---:|---|---|---|
| 1 | I2S2_WS | PB12 | I2S2 |
| 2 | I2S2_CK | PB13 | I2S2 |
| 3 | SPDIFRX_IN3 | PC5 | Entrada S/PDIF alternativa |
| 4 | SPDIFRX_IN2 | PC4 | Entrada S/PDIF alternativa |
| 5 | SPDIFRX_IN0 | PD7 | Entrada S/PDIF alternativa |
| 6 | USART2_RX | PA3 | Control, trigger o diagnóstico |
| 7 | USART2_TX | PA2 | Control, trigger o diagnóstico |
| 8 | GND | - | Tierra local |
| 9 | +3V3 | - | Alimentación local daughterboard |
| 10 | +5V | - | Alimentación local daughterboard |

Fuentes: `hardware/DS/librerias/netlist.md`, `hardware/DS/stm32_custom.kicad_sch`,
`hardware/DS/DS10_DESIGN_REVIEW.md` y el fragmento KiCad adjunto al pedido original.

### Advertencias que hay que verificar

- J5/J6 son adecuados como conexión corta a una daughterboard montada junto a la DS11.
  No asumir que señales CMOS de 12-25 MHz pueden salir directamente por cables largos.
- No conectar J5 con J5 pin-a-pin entre placas: los pines +3V3/+5V unirían reguladores.
- JP2 no selecciona mágicamente una dirección. Al cerrarlo conecta J5.7 al trunk
  `SAI1_MCLK`, que también incluye PE2 y los codecs. En followers PE2 debe quedar en
  alta impedancia; dos drivers activos producirían contención.
- El periférico SPDIFRX selecciona **una** de sus entradas mediante `INSEL`; los tres
  pines de J6 no equivalen a tres receptores simultáneos. S/PDIF tampoco transporta
  ocho canales en un solo link.
- I2S2 ofrece CK/WS/SDI pero no debe darse por hecho que implementa TDM8.
- SAI3_A no está inicializado por el firmware actual. SAI3_B está ocupado por el ADC.

## Candidato A: TDM8 transparente sobre enlace diferencial propio

Formato propuesto:

| Señal | Frecuencia/función |
|---|---|
| MCLK | 24,576 MHz |
| BCLK | 12,288 MHz, 256 ciclos por frame |
| FS | 48 kHz |
| DATA | 8 slots x 32 bits; PCM de 16 o 24 bits |

El enlace físico tendría cuatro pares diferenciales. La daughterboard de cada nodo
tendría un conector IN y otro OUT:

```text
IN diferencial
    -> receiver de cuatro canales
       -> tap CMOS corto hacia J5/DS11
       -> driver diferencial de cuatro canales
          -> OUT hacia el siguiente nodo
```

El STM32 escucha el tap pero **no reenvía** el stream. No debe existir un buffer de
audio por hop. Un reenvío RX DMA -> RAM -> TX DMA agregaría al menos 5 ms por placa con
los buffers actuales, y además crearía nuevos dominios de scheduling.

Hay una ambigüedad concreta que debés resolver en `hardware/DS/DS11_PLANNING.md`:

- La línea 96 describe un re-driver de hardware con latencia de nanosegundos.
- La línea 102 dice que cada caja escucha y “re-transmite buffereado”.

Esas dos arquitecturas no son equivalentes. Para este requisito, la hipótesis válida es
forwarding eléctrico continuo; si proponés store-and-forward, debés justificar y
compensar cuantitativamente cada frame de latencia.

Como referencia de factibilidad física, un receptor como TI DS90LV048A declara
400 Mbps, 150 ps típicos de skew canal-a-canal y 2,7 ns máximos de propagación. No lo
tomes todavía como selección definitiva: hay que analizar skew máximo garantizado del
par driver+receiver, terminación, cable, common-mode, EMI, ground offsets y disponibilidad.

Comparar dos topologías eléctricas diferentes, sin mezclarlas:

- **Estrella LVDS punto-a-punto:** un fanout independiente por placa. Como referencia,
  `DS90LV110T` es un fanout 1:10 pensado para datos/clock y declara 91 ps máximos de
  skew entre outputs. Esta opción ofrece la simetría más simple.
- **Daisy LVDS activo:** `receiver -> tap local -> driver siguiente`. Es viable para
  pocas placas, pero acumula propagación/skew y una placa apagada corta el resto.
- **Multidrop pasivo:** no poner 100 ohm en cada tap. Si se considera un bus multipunto
  real, evaluar M-LVDS y stubs; no asumir que un driver LVDS punto-a-punto tolera 18 taps.

Evaluar conectores/cables compactos y con traba que ofrezcan cuatro pares reales. No usar
USB-C con un protocolo propietario sin analizar orientación, confusión con USB y riesgo
de daño. Comparar al menos:

- Un conector de pares diferenciales dedicado.
- Mini-SAS u otra solución de cuatro pares con latch.
- HDMI/mini-HDMI usado como cableado propietario, incluyendo sus riesgos mecánicos.
- Una versión digital de un único cable Cat5/etherCON, aunque se quiera abandonar el
  sistema actual de cuatro RJ45 analógicos.
- Hirose ix Industrial Type B, diseñado para cuatro pares y usos non-Ethernet, como
  candidato compacto/robusto para la opción LVDS.

## Gate crítico: hacer llegar el mismo clock a todos los DAC

J5 alcanza para introducir o extraer TDM por SAI3_A, pero **no expone directamente los
clocks de los cuatro DAC**:

- SAI1_A usa PE5=BCLK y PE4=FS para los DAC 0-1.
- SAI2_A usa PD13=BCLK y PD12=FS para los DAC 2-3.
- Ambos son masters asíncronos en el firmware actual.

No basta con recibir DATA en SAI3_A y dejar SAI1/SAI2 con PLL3 local en cada placa. Aun
si todos dicen 48 kHz, la diferencia de ppm llena o vacía buffers y obliga a un slip.

Hipótesis de prototipo que hay que validar:

### Raíz

1. SAI1_A queda como único master de todo el sistema y genera MCLK+BCLK+FS.
2. Se puentean físicamente BCLK/FS de SAI1_A hacia:
   - J5.3/J5.6, para clockear SAI3_A.
   - Los nets de SAI2_A, que pasa a slave TX.
3. SAI3_A funciona como slave TX y pone las ocho fuentes en J5.4.
4. PE2 entrega MCLK al trunk local y, mediante JP2/J5.7, al transmisor del enlace.

### Followers

1. J5.3/J5.6 clockean SAI3_A slave RX.
2. Esos mismos BCLK/FS se puentean a PE5/PE4 y PD13/PD12.
3. SAI1_A y SAI2_A pasan a slave TX; sus bloques B siguen síncronos con A.
4. MCLK entra por J5.7/JP2 y PE2 queda en alta impedancia.
5. El AK5538 se mantiene en power-down durante el modo digital.

Esto daría a todos los PCM1690 de todas las placas el mismo MCLK, BCLK y FS físicos.
Revisá contra RM0433 y el esquemático si los cuatro SAIs pueden operar exactamente así,
qué pines quedan como entradas/salidas y en qué orden deben habilitarse.

Compará esa opción contra una alternativa con `I2S_CKIN`:

- En el H743, PC9 puede ser `I2S_CKIN` y alimentar los kernel clocks SAI.
- En DS11 PC9 está conectado directamente a SDA/CDTI del AK5538 y al pull-up, por lo
  que usarlo exige aislar esa red y sacrifica o complica el ADC/I2C3.
- Un MCLK común podría entrar por J5.7 con JP2 abierto y un bodge hacia PC9.
- Habría que demostrar que los divisores/frame phases locales arrancan con epoch
  determinístico o que su offset se mide y compensa en cada boot.

No aceptar la alternativa `I2S_CKIN` sólo porque elimina cables: debe demostrar la fase
de FS/BCLK y la latencia analógica final entre placas.

También revisá la sincronización cross-SAI de RM0433 sección 51.4.4. El manual dice que
un bloque configurado síncrono con otro SAI libera sus pines SCK/FS/MCLK como GPIO. Eso
puede impedir usar la sincronización interna como solución directa, porque los PCM1690
necesitan que los clocks sigan apareciendo físicamente en sus nets.

## Candidato B: A2B como backbone

Evaluar seriamente A2B, no sólo como nota marginal. La familia AD242x/AD243x está hecha
para audio TDM multicanal en topología lineal. Analog Devices declara:

- Audio I2S/TDM síncrono.
- Clock y fase alineados en todos los nodos.
- Latencia determinística menor a 50 us.
- Hasta 32 canales downstream y 32 upstream, según dispositivo/configuración.
- Daisy-chain sobre un único par trenzado y control I2C remoto.

El datasheet AD2426/7/8 Rev. F actual parece permitir un main más hasta 16 subnodes,
15 m por tramo y hasta 80 m totales bajo sus condiciones. Esto difiere de páginas y
artículos antiguos que hablan de 10 subnodes/40 m. Confirmá los límites exactos para la
mezcla de componentes elegida. Si el objetivo son 18 followers, evaluar dos cadenas
con mains que compartan el mismo TDM/clock, y presupuestar el skew entre ramas.

J5 parece especialmente compatible con una daughterboard A2B:

- I2C2 para configurar el transceiver.
- SAI3_A para TDM.
- 3V3/5V local.
- MCLK por JP2 si el transceiver dispone del clock adecuado.

Pero no asumir que “A2B es phase aligned” resuelve automáticamente la salida analógica
de DS11. Verificar específicamente:

1. Qué `BCLK`, `SYNC/FS`, `MCLK` o `CLKOUT` entrega el transceiver en cada subnodo.
2. Si puede entregar 24,576 MHz o una referencia apta para los PCM1690/SAI.
3. Cómo se gobiernan físicamente los clocks SAI1/SAI2 de salida de la DS11 actual.
4. Si la misma muestra downstream aparece con la misma latencia en todos los nodos o
   si existe offset dependiente de la posición.
5. Cantidad máxima real de nodos para 10-18 DS11 y longitud total/tramo.
6. Comportamiento ante un nodo sin alimentación y ante hot-plug.
7. Acceso a documentación completa, NDA/licencia, SigmaStudio, herramientas, BOM y
   disponibilidad a largo plazo.
8. Si A2B 1.0, AD243x o A2B 2.0 es la generación adecuada para un diseño nuevo.

Verificar en particular la hipótesis de que `CLKOUT1/2`, derivados del PLL A2B de
98,304 MHz a 48 kHz, pueden dividirse a 24,576 MHz. También cuantificar el crecimiento
de jitter al final de la cadena: documentación de ADI reporta valores típicos de SYNC
que crecen desde aproximadamente 1,57 ns en el primer nodo hasta 3,33 ns en el nodo 15,
con máximos cercanos a 6 ns. Puede ser despreciable para steering acústico y aun así
degradar THD+N del DAC; EE-459 debe revisarse contra PCM1690, no extrapolarse sin medir.

Fuentes oficiales iniciales:

- https://www.analog.com/en/products/ad2428.html
- https://www.analog.com/en/solutions/a2b-automotive-audio-bus/a2b-audio-bus-solutions.html
- https://www.analog.com/en/resources/technical-articles/software-over-the-a2b.html
- https://www.analog.com/media/en/technical-documentation/user-guides/ad242x-trm.pdf
- https://www.analog.com/media/en/technical-documentation/application-notes/ee459v01.pdf
- https://www.analog.com/en/resources/evaluation-hardware-and-software/embedded-development-software/a2b-software.html

Una ventaja mecánica potencial es grande: A2B podría ofrecer IN+OUT mediante pequeños
conectores locking de un par, en lugar de cuatro RJ45 analógicos por placa. Las EVM de
ADI usan como referencia Molex DuraClik de dos pines y cable de un par. Pero pedimos
comparación técnica y de riesgo, no una elección por estética.

## Daisy-chain frente a estrella

No des por sentada la cadena sólo porque es cómoda físicamente. Comparar:

### Daisy-chain activo

- Menos cable desde el centro.
- Cada hop agrega cable y propagación del transceiver.
- Una falla intermedia puede cortar todos los nodos posteriores.
- El hop count debe formar parte de la calibración si el delay no es igual para todos.

### Estrella diferencial

- Cada placa atraviesa un solo receptor.
- Con cables de longitud comparable, el skew es más simple de acotar.
- Necesita un hub/fanout y más cable central.
- Puede ser preferible si la coherencia domina sobre la comodidad.

### Híbrido

- Backbone lineal con taps/re-drivers de hardware, sin store-and-forward.
- O pequeños hubs por grupos de paneles.

Para cada opción, calcular delay min/típico/máximo, skew entre ramas, deriva térmica y
posibilidad de calibración. La propagación fija en nanosegundos es compensable; el jitter,
un frame slip o una latencia dependiente del estado no lo son.

## Arquitectura de firmware a evaluar

```text
ROOT
USB isochronous OUT cada 1 ms
       -> ring SPSC
       -> bloque comun de 240 frames
       -> delay_line[8] local
       -> mode15 -> 32 salidas locales
       -> SAI3_A TX -> enlace de 8 fuentes

FOLLOWER
SAI3_A RX -> DMA de 240 frames
       -> validar frame_id/CRC
       -> delay_line[8]
       -> mode15 -> 32 salidas locales
```

Root y followers deben tener el mismo número lógico de frames de latencia. La raíz no
puede tomar un atajo de buffer mientras los followers esperan 240 frames. Evaluar:

- Procesar la raíz según el mismo epoch del SAI3_A TX.
- Agregar delay fijo a la raíz equivalente al camino RX follower.
- O hacer loopback para que la raíz atraviese el mismo camino, si el hardware lo permite.

Como los slots son de 32 bits y la fuente puede tener 16 o 24 bits, evaluar usar bits
no audibles/no utilizados para metadata. Posible contenido:

- Contador absoluto de frame.
- `audio_valid`.
- Marcador de inicio/epoch.
- CRC o checksum.
- Versión/formato de stream.

Definir cómo evitar que esos bits entren al audio y cómo detectar:

- Rotación de slots.
- Frame perdido o repetido.
- Link sin clock.
- Error de datos.
- Root duplicada o dos drivers activos.

Los comandos por Wi-Fi/UART no necesitan ser eléctricamente simultáneos si transportan
`apply_at_frame=N` y llegan con anticipación. Cada nodo debe aplicar el cambio al mismo
contador compartido.

## ADC y AUTOFOCUS

El bypass musical puede apagar el AK5538, pero el ADC sigue siendo útil para AUTOFOCUS,
medición acústica y diagnóstico. Analizar tres políticas:

1. Modo digital y modo calibración mutuamente excluyentes: se calibra, se guardan delays
   y luego se apaga el ADC para la ejecución digital.
2. SAI3_A digital y SAI3_B ADC funcionando simultáneamente con clocks comunes.
3. Entrada de medición separada o ADC externo en una revisión futura.

No asumir que SAI3_A y SAI3_B pueden tener cualquier combinación master/slave y clock.
Verificar RM0433, DMA disponible, pins y la necesidad de que el AK5538 reciba MCLK/BCLK/FS
síncronos. Si la opción simultánea requiere más bodges, listarlos.

## Secuencia de arranque/relock propuesta

Revisá esta máquina de estados:

1. Todos los DAC/PAM permanecen muteados.
2. La raíz inicia MCLK+BCLK+FS y transmite silencio con metadata válida.
3. Cada follower valida clocks y varios frame IDs consecutivos.
4. Se anuncia `ARM_AT_FRAME=N`.
5. Antes de N se limpian delay lines, índices y buffers.
6. En N se fija un epoch común para `delay_write_pos` y parámetros DSP.
7. Se hace prefill con una latencia declarada idéntica.
8. Todos desmutean en `N + LATENCY_FRAMES`.
9. Ante pérdida de clock, CRC o secuencia: mute inmediato, limpieza y relock completo.

Indicar qué partes pueden ser puramente firmware y cuáles necesitan un trigger o clock
físico adicional.

## Hipótesis que tenés que intentar refutar

- “Digital” implica automáticamente coherencia temporal.
- Distribuir sólo DATA y FS es suficiente.
- Distribuir sólo MCLK es suficiente.
- Dos PLL locales a 48 kHz no derivan de manera relevante.
- El buffer elástico o ASRC no afecta la fase del beamforming.
- Se puede reenviar el audio por DMA/firmware en cada hop sin costo temporal.
- Los conectores J5/J6 bastan sin bodges adicionales.
- El jumper JP2 aísla PE2.
- Las tres entradas S/PDIF de J6 reciben seis canales simultáneamente.
- Un re-driver tiene literalmente cero delay.
- A2B phase-aligned garantiza por sí solo que los DAC de DS11 lo estén.
- La raíz y los followers pueden tener pipelines distintos porque la frecuencia es igual.
- Wi-Fi puede usarse como pulso de sincronismo de muestra.
- El skew actual de aproximadamente 1 us entre SAI1 y SAI2 es necesariamente aceptable
  para la especificación multinodo.

## Mediciones y criterios de aceptación mínimos

Proponer un plan escalonado que incluya al menos:

1. **Una DS11:** bypass ADC, ocho canales digitales, orden de slots y calidad.
2. **Dos DS11:** common clock, impulso idéntico y medición de skew analógico real.
3. **Cuatro DS11:** propagación por varios hops y power-up en distinto orden.
4. **Máxima cadena:** peor cable, temperatura y tráfico de control.

Pruebas mínimas:

- PRBS/contador diferente por canal y comparación bit a bit entre primera y última placa.
- Impulso común y correlación de salidas analógicas de placas distintas.
- GPIO asociado al mismo `frame_id`, medido con osciloscopio/analizador lógico.
- Captura simultánea multicanal después de los PCM1690 para incluir group delay real.
- Al menos 1000 cold boots con orden de encendido aleatorio.
- Soak mínimo de 24 horas sin slips, gaps, underruns ni cambio de offset.
- Temperatura y tensión de alimentación extremas razonables.
- Desconexión/reconexión USB y del enlace intermedio.
- Reboot o pérdida de alimentación de un nodo intermedio.
- Eye diagram, jitter y skew en el último receiver.
- Modo 15 al 64,8 % de CPU mientras funcionan USB, link, Wi-Fi y telemetría.

Criterios sugeridos que debés validar o corregir:

- Cero diferencia de muestras enteras tras boot/relock.
- Cero sample slips en 24 horas.
- Todos los parámetros aplicados al mismo `frame_id`.
- Skew analógico estable por debajo del presupuesto de fase acordado.
- Latencia root/follower conocida e idéntica o explícitamente compensada.
- Cualquier error de link es detectado y produce mute; no audio corrupto silencioso.

## Archivos que tenés que leer

En este orden:

1. `codex_onboarding.md`
2. `docs/audio_architecture.md`
3. `docs/manual_modos.md`
4. `docs/ds11_perf.md`
5. `docs/multinode.md`
6. `docs/ds11_upgrades.md`
7. `docs/pin_map.md`
8. `hardware/DS/DS11_PLANNING.md`, especialmente la sección 6
9. `hardware/DS/DS10_DESIGN_REVIEW.md`, tomando sus frecuencias antiguas con cautela
10. `hardware/DS/librerias/netlist.md`
11. `hardware/DS/stm32_custom.kicad_sch`
12. `firmware/Core/Src/main.c`, completo
13. `firmware/Core/Src/stm32h7xx_hal_msp.c`
14. `datasheets/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf`, en particular RCC/I2S_CKIN, SAI y SPDIFRX

Documentación oficial externa inicial:

- STM32H743 RM0433: https://www.st.com/resource/en/reference_manual/rm0433-stm32h743-753-and-stm32h750-value-line-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf
- STM32H743 datasheet: https://www.st.com/resource/en/datasheet/stm32h743bg.pdf
- USB 2.0: https://www.usb.org/document-library/usb-20-specification
- Apple USB Audio: https://developer.apple.com/documentation/technotes/tn3190-usb-audio-device-design-considerations
- TI DS90LV048A: https://www.ti.com/product/DS90LV048A
- Analog Devices AD2428/A2B: https://www.analog.com/en/products/ad2428.html

## Formato de respuesta solicitado

Entregá:

1. **Veredicto ejecutivo:** viable/no viable/viable con condiciones para los tres
   escenarios: dos DS11, 10-18 DS11 y diseño de producción.
2. **Arquitectura recomendada**, con un diagrama de clock y datos desde USB hasta el
   último PCM1690.
3. **Segunda mejor alternativa** y por qué pierde.
4. **Comparación cuantitativa:** LVDS TDM propio vs A2B vs estrella vs Ethernet/PTP u
   otra opción que consideres superior.
5. **Clock tree exacto** para raíz y follower. Enumerá quién maneja cada pin y net.
6. **Modificaciones de la DS11 actual**, separadas en raíz/follower, indicando bodges,
   cortes, jumpers, componentes y riesgos.
7. **Diseño conceptual de daughterboard**, con bloques, conectores, terminaciones,
   transceivers, alimentación, aislamiento y señales hacia J5/J6.
8. **Cambios de firmware** por periférico, DMA, buffers, startup, metadata y telemetría.
9. **Presupuesto de ancho de banda, latencia, jitter y fase**, con min/típico/máximo.
10. **Tratamiento de root vs followers**, garantizando que no haya un half-buffer de
    diferencia inadvertido.
11. **Compatibilidad con AUTOFOCUS/ADC** o una decisión explícita de exclusión mutua.
12. **Matriz de riesgos** priorizada por severidad y probabilidad.
13. **Plan de prototipo con gates go/no-go**, empezando por dos DS11 y mediciones antes
    de diseñar una PCB definitiva.
14. **Preguntas abiertas** que requieran una medición o decisión del autor.

Terminá respondiendo sin ambigüedad:

> ¿Puedo aprovechar las DS11 fabricadas para demostrar un bypass digital y una cadena
> phase-coherent, o los conectores expuestos no alcanzan? ¿Qué mínimo hardware adicional
> y qué modificaciones hacen falta para que la demostración sea técnicamente válida?
