# Prompt de onboarding para Codex — proyecto DS (Domosonica)

> Pegá el bloque de abajo como primer mensaje en Codex, parado en la raíz del repo.
> Después de eso ya podés preguntarle cosas concretas (abajo hay 3 pedidos listos).

---

## BLOQUE 1 — Onboarding (pegar tal cual)

```
Sos un ingeniero de firmware embebido revisando un proyecto real que ya está
corriendo en hardware. Antes de opinar, leé el repo. NO escribas código ni
propongas cambios en este primer turno: quiero que primero cargues contexto y
me devuelvas un resumen para verificar que entendiste bien.

CONTEXTO QUE NO PODÉS DEDUCIR DEL CÓDIGO (dado por el autor):

- El proyecto es un ARRAY DE PARLANTES CON BEAMFORMING ("Domosonica"). Es un
  instrumento artístico, no un producto: proyecta sonido enfocado en puntos
  del espacio. La placa se llama DS; la revisión actual es DS11.
- Hardware DS11: STM32H743 (480 MHz) + AK5538VN (ADC 8 canales, TDM256) +
  4× PCM1690 (DACs, TDM8, 32 canales) + 32× PAM8302A (amplificadores class-D)
  + ESP32-C3-MINI-1 (WiFi/HTTP, puente por UART4 con protocolo binario).
- Estado: la DS11 ACABA DE LLEGAR y es la primera placa con los 32 canales
  poblados. Todo lo anterior (tag `ds10-release`) se validó sobre DS10, que
  solo tenía 8 parlantes (un solo DAC, U2).
- El beamforming YA FUE VALIDADO EN EL AIRE sobre DS10, de oído: con 4 tonos
  simultáneos enfocados en puntos distintos, caminando de izquierda a derecha
  los tonos aparecían de grave a agudo, coincidiendo con la geometría.
- El commit `3d7d6d6` ("migrar firmware a la placa DS11 — 32 canales activos")
  es el más reciente y NO ESTÁ PROBADO EN HARDWARE TODAVÍA. Es el que más me
  interesa que mires.

ARQUITECTURA EN UNA LÍNEA:
ADC (8 entradas) → delay lines circulares → cálculo de delays focales por
canal (punto fijo Q8) → 32 salidas TDM8 por 4 bloques SAI con DMA circular
ping-pong → 4 DACs → 32 amplificadores → 32 parlantes en grilla 4×8.

LEÉ ESTOS ARCHIVOS, EN ESTE ORDEN:
1. docs/manual_modos.md        — los 16 modos de operación, el mapa mental del firmware
2. docs/ds11_upgrades.md       — revisión de hardware DS11, riesgos conocidos
3. firmware/Core/Src/main.c    — TODO el firmware STM32 (~3200 líneas)
4. firmware/Core/Inc/uart_protocol.h — protocolo binario con el ESP32
5. docs/protocolo_medicion.md — metodología de medición acústica (mic fijo, foco móvil)
6. git show 3d7d6d6            — el cambio a DS11 sin probar

RESTRICCIONES REALES DEL SISTEMA (violarlas rompe el audio):
- Presupuesto duro: el pipeline llena buffers de 480 muestras cada 10 ms, en
  4 bloques SAI. Si un fill no termina a tiempo hay underrun (audible).
- Sin FPU en el camino crítico: la aritmética por muestra es entera/punto fijo.
  `printf` con nano.specs no soporta %f. Float solo en cambios de estado.
- DMA1/DMA2 NO acceden a DTCMRAM en el STM32H7: los buffers viven en RAM_D2
  (sección .dma_buffer, aligned 32).
- D-cache NO está habilitada: se usa __DSB() como barrera, no SCB_CleanDCache.
- SAI con DataSize=24 transmite bits [23:0]: las muestras van right-aligned,
  NUNCA <<8. El camino ADC (32-bit) → DAC (24-bit) requiere >>8.
- Las escrituras I2C (volumen del DAC, EEPROM) jamás desde ISR.

TAREA DE ESTE TURNO: leé todo lo de arriba y devolveme
(a) un resumen de la arquitectura del pipeline de audio en tus palabras,
(b) qué hace cada uno de los modos 11 a 15,
(c) las 3 partes del código que te parecen más frágiles o riesgosas, con
    archivo y línea, y por qué.
Si algo del código contradice lo que te dije arriba, decímelo — puedo estar
equivocado yo.
```

---

## BLOQUE 2 — Pedidos concretos (uno por vez, después del onboarding)

### A. Revisión del commit DS11 sin probar (el de mayor valor)

```
Revisá el commit 3d7d6d6 buscando errores que solo aparecerían al pasar de 8 a
32 canales: índices fuera de rango, arrays dimensionados para 8, loops que
asumen un solo banco de DACs, overflow de acumuladores, y cualquier lugar del
código que todavía asuma que solo la primera fila de parlantes está poblada.
Este código todavía no se flasheó: un bug acá cuesta un ciclo completo de
hardware. Priorizá por severidad y citá archivo:línea. Si no encontrás nada
serio, decilo claramente en vez de inventar hallazgos menores.
```

### B. Análisis de overflow de punto fijo (verificación independiente)

```
En firmware/Core/Src/main.c, el fill del modo 15 (MULTIFOCUS de 8 entradas,
buscá `mfocus_in_enabled`) suma 8 fuentes de audio, cada una interpolada con
delay fraccional Q8, aplica un boost por desplazamiento y después un gain por
canal. Hacé el análisis de rangos numéricos vos mismo, desde cero, sin asumir
que lo que está es correcto: ¿puede desbordar el int32 en algún punto? ¿El
clamp está en el lugar correcto respecto de la multiplicación por el gain?
¿Los shifts intermedios pierden precisión innecesariamente? Mostrame los
números (bits) de cada paso. Hacé lo mismo con el fill del modo 14 (buscá
`mfocus_enabled`), que usa offsets de fase en vez de delays temporales.
```

### C. Presupuesto de CPU con 32 canales

```
Estimá el costo de CPU del fill del modo 15 con los 32 canales de la DS11.
Datos: CPU Cortex-M7 a 480 MHz, 48 kHz de sample rate, buffers de 480 muestras
(10 ms), 4 bloques SAI de 8 canales cada uno, 8 fuentes de entrada. Cada
muestra de salida suma las 8 fuentes con interpolación lineal Q8 sobre una
línea de retardo circular. Calculá iteraciones por ventana de 10 ms y ciclos
estimados por iteración (tené en cuenta que el M7 tiene SMULL de un ciclo para
multiplicaciones 32×32→64). Decime qué porcentaje del presupuesto de 10 ms se
consume y si ves riesgo de underrun. Si lo ves ajustado, proponé optimizaciones
concretas ordenadas por relación beneficio/riesgo.
```

---

## Notas para vos (no para Codex)

- El pedido A es el de mayor valor: es código real, sin probar, y un bug
  cuesta un viaje a la placa.
- En B y C pedí deliberadamente que calcule **desde cero** en vez de revisar mi
  razonamiento: si le mostrás mi conclusión primero, es probable que la
  confirme. Que llegue solo al número y después comparás.
- Si Codex y yo diferimos en algo, ese desacuerdo es la señal más valiosa —
  pasámelo y lo miramos.
