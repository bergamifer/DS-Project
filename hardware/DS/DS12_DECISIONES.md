# DS12 — registro de decisiones

> **2026-11-10.** Una sola página con lo que está decidido, para no tener que reconstruirlo leyendo
> diez documentos. **No reemplaza a ninguno:** cada fila dice dónde está el porqué completo.
> Si esta página y un documento se contradicen, gana el documento — y hay que corregir esta página.
>
> **Cómo se armó:** leyendo los documentos `DS12_*.md` y cruzándolos entre sí. **No se volvió a
> verificar contra el esquemático ni el PCB**; el estado de dibujo de cada ítem es el que declaran
> `DS12_REVISION_DECISIONES.md` §7 (netlist guardada el 28-oct) y el checklist. Lo que no pude
> confirmar va marcado ⚠️.
>
> Siglas: **AL** = `DS12_ALIMENTACION` · **24V** = `DS12_ENTRADA_24V` · **REV** =
> `DS12_REVISION_ALIMENTACION` · **DEC** = `DS12_REVISION_DECISIONES` · **PL** = `DS12_PLANNING` ·
> **AU** = `DS12_AUDITORIA` · **USB** = `DS12_USB_STM32` · **MIC** = `DS12_MICROFONO_DNP`.

---

## 1. Lo decidido

### Alimentación

| Decisión | Por qué, en una línea | Detalle |
|---|---|---|
| **Entrada a 24 V** (2026-10-23), **nunca más de 25,5 V**. Net `+24V` | El vatio de 24 V es mucho más barato que el de 19–20 V. Los dos bucks aguantan 35 V; el TVS de 24 V empieza a conducir en ~26,7 V, de ahí el tope | 24V §0 |
| **D9 = SMBJ24A** (`C908801`) | El SMBJ20A conduce o no según la unidad entre 22,2 y 24,5 V: a 24 V, algunas placas lo quemarían al enchufar | 24V §1 |
| **F1 = 3 A** (`C136347`) | El pico de carga de los capacitores crece con el cuadrado de la tensión: con 2 A gastaba el 87 % del fusible | 24V §3.3 |
| **Dos bucks:** AP63356DV-7 → `+5VA` (amplificadores) y AP63301WU-7 → `+3V3` (digital) | El LDO de la DS11 vivía sin margen de dropout y reseteaba al encender los amplificadores. PWM puro (sin modo ráfaga, que mete ruido en audio) | AL §0, §3 |
| **OR de USB en la entrada del buck B** (D20/D21) | Con el LDO fuera, el OR ya no puede estar a la salida | AL §2 |
| **MAX809S**, no MAX809T · **J7 pin 1 con resistencia en serie** | Umbral peor caso 2,93 V contra 3,23 V · ese pin sensa, no alimenta | AL §3.6, §3.7 |
| **Cinco `C45783` (22 µF) en `+5V_AUX`** | Enchufar un USB vivo con 24 V puesto dispara un golpe de ariete: 6,09 V con 3 capacitores, 5,73 V con 5 (límite absoluto del AP2125: 6,5 V) | REV §3, §9.1 |
| **1 kΩ de drenaje en `+5V_AUX`** · **1 kΩ de VBUS a GND en J3 y J8** · **pin 5 de U6/U9 a `+3V3` + 100 nF** | Fugas de los diodos dejan VBUS en 4,8 V sin cable. Los tres cambios van **juntos**: separados no alcanzan | REV §9.1 |
| **C28 (EN del ESP32) a 1 µF** · **TDMIN a GND** · **PE15 a GND por 1 kΩ** | Rampa de `+3V3` más lenta que el RC del EN · lo pide la hoja del AK5538 · no hay 0 Ω en el BOM | REV §9.1 |
| **`PDN_AK = PC7 AND PWR_GOOD`** (BAT54C) y **`+5VA` leído en PC1** | Si cae `+5VA` y el digital no, el ADC nunca recibe su pulso de reset; y el PG del buck no puede indicar falla si el buck no tiene entrada | AL §6.1 |
| **Guarda de firmware:** amplificadores sólo con PC1 > 2,25 V (*analog watchdog*) | Con `+5VA` en cero, los 32 pines SD quedan a 3,3 V y por los diodos de protección entra corriente al PAM | REV §2, §9.2 |

### Entradas, salidas y jacks

| Decisión | Por qué, en una línea | Detalle |
|---|---|---|
| **Bias del ADC: 10 kΩ por pata (16) a un nodo de 1,65 V** | Sin red, las entradas caen a 0 V (medido) y el AK5538 pide 1,58–1,72 V. Medido que su entrada no carga en continua, así que una red pasiva alcanza | DEC §5 |
| **LEDs de J9 invertidos** | En la DS11 el ánodo estaba a GND: nunca podían prender (medido en banco) | DEC §5 |
| **Un solo LED por jack** (LED 1 → PE12) ⚠️ *recomendado, falta que lo decidas* | Con los LEDs invertidos, los pads de jacks vecinos quedaron a 0,097 mm con redes distintas; el DRC lo marca | DEC §1 |
| **Polaridad del conector de parlantes `+ − − +`**, por nombre de red | El `pinfunction` del netlist miente; mandan los nombres de red | CLAUDE.md, PL (final: «La polaridad del conector…») |
| **Micrófono MEMS I2S DNP: MSM261S4030H0R**, sobre las redes I2S2 | Cero patas nuevas y sin circuito analógico. Los otros dos modelos son de puerto inferior y están discontinuados | MIC |
| **Un segundo micrófono igual, DNP, en el ESP32** (GPIO6 SCK, GPIO7 WS, GPIO3 SD), en la placa (2026-11-18) | Chequeo de parlantes y escucha remota sin tocar los buses de audio del STM32. Costo de centavos. Con él montado se pierde el JTAG externo de J1 | MIC |

### Geometría y firmware

| Decisión | Por qué, en una línea | Detalle |
|---|---|---|
| **Array de 2 filas × 16 columnas entrelazadas**, 80 mm (paso efectivo 40 mm) | La gente camina por el piso: hace falta foco angosto en horizontal y ancho en vertical. El ancho del haz es inversamente proporcional a la apertura, así que el array tiene que ser **bajo**. Duplica el techo sin aliasing: 2144 → 4288 Hz | PL §0.8 |
| **`SPK_LAYOUT_2X16=1`** y **`TX_MAX_DELAY_SAMPLES` 128 → 192** | La diagonal nueva son 175 muestras; con 128 el firmware recortaba el retardo en silencio | PL §0.8.1, AU |
| **Sincronismo por PD2**, no por PC13 | La errata ES0392 §2.2.30: conmutar PC13 corrompe el cristal de 32 kHz, sin workaround | AU §4 |
| **USB del STM32: Full-Speed en PA11/PA12**, USB-C en J8, **USBLC6 primero y el choque FL1 después** (Murata DLW21SN900HQ2L, `C97855`), VBUS detectado en PA5 | Sirve para flashear sin ST-Link. **No es una interfaz de audio**: 8 canales × 24 bits son 1152 B por trama y el máximo Full-Speed es 1023 B | USB §0, §5 |
| **Proceso:** rellenos de zona recalculados antes de cada commit (`tools/check_pcb.sh`), `pad_to_mask_clearance` 0,05 mm, reglas propias para IC3 e IC4 en `DS.kicad_dru` | KiCad plotea el cobre guardado, no el recalculado | CLAUDE.md, AU §2 |

---

## 2. Decisiones descartadas — no volver a proponerlas

| Idea | Por qué murió |
|---|---|
| Entrada de 20 V / ventana 15–21 V / SMBJ20A | Reemplazada por 24 V el 2026-10-23 |
| «24 V no sirve» (README de hardware, hasta hoy) | Era la regla de la DS11, cuyo MP2315S aguanta 26 V absolutos. **La DS11 nunca va a 24 V**; la DS12 sí |
| `C2158014` como AP63356 | Es el AP63357 (con ráfagas). El correcto es `C2157973` |
| El tap de VOUT para cerrar el lazo DAC→ADC | Las 8 entradas del AK5538 ya están en J9. Para autoprueba, PC0 (opcional) |
| PC13 como línea de timing | Errata del cristal (arriba) |
| Subir el 33 Ω de R20 a 5,1 kΩ | Medido: dejaba SD en ≈2 V. Hoy la propuesta es 470 Ω (abierta, §3) |
| «El ADC no necesita bias» | Falso: la hoja pide 0,48–0,52 × AVDD |
| USB High-Speed / interfaz de audio 8×8 | Pide PHY ULPI externo y LQFP176: cambiar de encapsulado |
| Interfaz de cámara DVP/MIPI (DNP) | El H743 no tiene MIPI; DCMI en LQFP100 pisa audio, EEPROM y PAM_SD en D4–D7. Archivado en [`DS12_CAMARA_DVP_FUTURO.md`](DS12_CAMARA_DVP_FUTURO.md) |
| ICS-43434 y SPH0645LM4H-B de micrófono | Puerto inferior y discontinuados |
| Analizar stock para la DS12 | Los componentes ya están comprados (BOM manda sobre el netlist) |
| Igualar largos de pista por el skew entre bancos | El skew de layout es 67 ps contra 4 µs medidos: eran arranques de periféricos |
| Issue #15 / modo 17 (barrido vertical a 800 Hz) | Con 2 filas no hay steering vertical que aliasear |

---

## 3. Abierto — falta que decidas o midas

| Qué | Qué falta | Dónde |
|---|---|---|
| **R20: 33 Ω → 470 Ω** (`C25117`, huella 0402) | Decidir. Probarlo en una DS11 implica encender amplificadores | DEC §3 |
| **LED 2 de J9** (R164–R167, PE13) | Inclinación al 18-nov: **conservarlo** (sin cerrar). Hay un reruteo que deja J9 en 0 violaciones; falta M13b, M13 y confirmar los 0,397 mm de agujero a agujero con JLCPCB | DEC §1 |
| **Medida M13** (brillo del LED a ≈2,5 mA) | Medir | DEC §4 |
| **Remapeo de J9 a pares trenzados (1,2)/(3,6)/(4,5)/(7,8)** ⚠️ | Propuesto en PL §6d «para DS12»; **no encontré registro de que esté dibujado** | PL §6d |
| **`C23630` ¿cumple la ECN de ≥ 1 µF a 5,25 V?** | Sólo importa si se cambian los capacitores de `+5V_AUX` | REV §9.4 |
| **Modo sólo-USB con 0,9 A** | Medir `+3V3` en la primera DS12 | REV §9.5 |
| **MAX809S de UMW contra el reset del STM32** (salida push-pull) | Probar con el primer OTA | REV §9.4 |
| **Fuente de 24 V: pico de enchufe por el diodo de Q1** | Conectar siempre con la fuente apagada | 24V §3.4 |

## 4. Falta dibujar (según DEC §7, netlist del 28-oct)

- 15 de las 16 resistencias del bias del ADC (está R171 y el nodo).
- ~~Los dos micrófonos~~ — dibujados el 27-nov (MK1 en I2S2, MK2 en el ESP32, con sus pasivos); falta el PCB y comparar la huella con la hoja.
- El cambio de J9 (un LED por jack) y, si se acepta, R20.
- Actualizar el PCB desde el esquemático (le faltan C58, C62, R169–R171).
- ~~Firmware: el código de los LEDs de PE12/PE13~~ — hecho el 18-nov (`jack_leds.c`, con test); falta probarlo en una DS12.

Lista completa en orden de dibujo: [`DS12_CHECKLIST.md`](DS12_CHECKLIST.md).

---

## 5. Qué documento es qué

| Documento | Rol hoy |
|---|---|
| **Este** | Qué está decidido |
| [`DS12_CHECKLIST.md`](DS12_CHECKLIST.md) | Qué dibujar, en orden |
| [`DS12_ALIMENTACION.md`](DS12_ALIMENTACION.md) | Especificación de la fuente. **Ojo:** su cuerpo todavía dice 20 V y SMBJ20A; manda el banner de la cabecera y [`DS12_ENTRADA_24V.md`](DS12_ENTRADA_24V.md) |
| [`DS12_ENTRADA_24V.md`](DS12_ENTRADA_24V.md) | Los límites de 24 V y su simulación |
| [`DS12_REVISION_ALIMENTACION.md`](DS12_REVISION_ALIMENTACION.md) | Resultados de la revisión de las fuentes (S1–S4, ronda 2: §9) |
| [`DS12_REVISION_DECISIONES.md`](DS12_REVISION_DECISIONES.md) | Revisión del 10-nov: J9, R20, bias, micrófono, estado real |
| [`DS12_USB_STM32.md`](DS12_USB_STM32.md) · [`DS12_MICROFONO_DNP.md`](DS12_MICROFONO_DNP.md) | Especificaciones de esos dos bloques |
| [`DS12_PLANNING.md`](DS12_PLANNING.md) | **Historial de una discusión** más un backlog. Vigente: geometría (§0.5, §0.8), J9 (§6d) y la polaridad de los parlantes (al final). El resto (§0.1–0.4, §1) está resuelto |
| [`DS12_AUDITORIA.md`](DS12_AUDITORIA.md) | Auditoría del 11-sep; las correcciones del §4 ya se aplicaron |
| `DS12_REVISION_ALIMENTACION_PLAN.md` · `…_RONDA2.md` | **Planes ya ejecutados.** Sólo interesan como método |
