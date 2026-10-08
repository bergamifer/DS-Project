# DS12 — Plan de revisión del esquemático: fuentes de alimentación y su convivencia

> Escrito el 2026-10-01 para que lo ejecute Opus. **Es un plan, no un informe**: todo lo que
> figura como *sospecha* (🔎) sale de leer el netlist y los documentos, **no está verificado**, y
> el trabajo consiste justamente en confirmarlo o refutarlo contra la hoja de datos.
> Fernando lo lee: cada hallazgo tiene que explicar el **mecanismo**, no sólo el resultado.
>
> **Estado (2026-10-01):** **S1, S2 y S4** están ejecutados, con refutación ciega y un dato plantado
> que se detectó. Resultados en [DS12_REVISION_ALIMENTACION.md](DS12_REVISION_ALIMENTACION.md):
> - **S1** confirmado (🟢 documento).
> - **S2** confirmado (🟡 firmware).
> - **S4** de norma, sin consecuencia, y la refutación encontró **S4.f** (🟡: sobretensión en
>   `+5V_AUX` al enchufar J8 en caliente).
> - Aparecieron **N1–N6**. N1 (la fuga de D23) va primero en la próxima ronda.
>
> Quedan **S3 y S5–S12**.

---

## 0. Alcance — qué sí y qué no

| | |
|---|---|
| ✅ **Sí** | Todo el camino de energía **en el esquemático**: entrada +20V → F1/Q1/D21/D9 → los dos bucks (IC4 AP63356, U11 AP63301) → `+5VA`, `+3V3`, `+5VA_DAC`, `+5V_AUX`, `+3.3VA` (U10 AP2125); el OR de USB/ST-Link; supervisor; pines de alimentación de cada consumidor; y las señales que cruzan de un dominio de alimentación a otro (PG, SD, PDN_AK, I²C, etc.). |
| ❌ **No** | **El PCB.** No abrir `DS.kicad_pcb`, no correr `check_pcb.sh`, no opinar de layout, vías, planos ni ruteo. Si un hallazgo *depende* del layout, se anota en una lista aparte «para cuando Fernando empiece el PCB» y no se desarrolla. |
| ❌ **No** | **Entradas del ADC sin polarización** (`DS12_AUDITORIA.md` §3.2 / `DS12_CHECKLIST.md:164`): Fernando todavía no lo revisó, sigue pendiente. **Única frontera:** si el nodo VCM saliera de `+3.3VA`, carga al AP2125 → dejar una línea reservada en el presupuesto de corriente de ese riel y nada más. |
| ❌ **No** | Stock y disponibilidad: ya está todo comprado. Los arreglos que se propongan tienen que **preferir valores y part numbers que ya están en `hardware/DS/jlcpcb/production_files/BOM-DS.csv`** (el BOM manda sobre el netlist). |

**Fuente de verdad, en orden:** (1) hoja de datos del fabricante → (2) `hardware/DS/DS.net` →
(3) los `DS12_*.md`, que son **afirmaciones a verificar, no verdad**: la propia auditoría del
11-sep les encontró cinco errores y la revisión del 28-sep otros seis.

**Estado del netlist (verificado hoy):** `DS.net` exportado **2026-10-01 15:29:40**; los
`.kicad_sch` se guardaron por última vez a las **15:15** → el netlist es posterior, está vigente.
`tools/sch2net.py` regenerado hoy da la misma electricidad (difiere sólo en 7 componentes no
eléctricos: FID1-3, J1, J2, J4, J10, y 7 nombres de red cosméticos). 664 componentes, 635 redes.

**Qué ya está hecho y no hay que repetir** (`DS12_ALIMENTACION.md`, revisión 29-sep): se verificó
la **tabla de pines** de IC4, U11, Q1, U10, etc. contra su datasheet. Esta revisión va **más
allá de la tabla de pines**: máximos absolutos, condiciones de operación, circuito de referencia
(valores de L/C/R y límites de capacidad de salida), secuencia de encendido, qué pasa con un pin
alimentado en un chip que no lo está, y la convivencia entre fuentes. Se hace un spot-check de la
tabla de pines (cinco minutos, sirve de prueba de que el método funciona), no una revisión.

---

## 1. Preparación (hacerla una sola vez, antes de lanzar agentes)

La auditoría del 11-sep **agotó el presupuesto** porque doce agentes parsearon el netlist cada
uno por su cuenta. No repetirlo.

1. **Regenerar el digest** (el de `.ds12_audit/` es del 11-sep: está viejo):
   `tools/netlist_digest.py hardware/DS/DS.net .ds12_audit` → `componentes.tsv`, `redes.txt`,
   `pines_por_ic.txt`. Todos los agentes leen **esos tres archivos**, nadie parsea el `.net`.
   Imprimir de cuándo es el netlist en el informe final.
2. **Bajar los datasheets que faltan** a `hardware/DS/datasheets/` (hoy hay 15 archivos; **faltan
   casi todos los que importan**). Un solo agente o el propio Opus, **antes** de las rondas, para
   que todos citen el mismo PDF/revisión:

   | falta | part number | para qué |
   |---|---|---|
   | AP63356 | **DS41949 Rev 3-2** | buck A. **No está** en la carpeta |
   | AP63301 | DS42002 Rev 3-2 | buck B. `AP63300.pdf` **probablemente** es éste: mirar la portada |
   | AP2125 | AP2125K-3.3 | LDO analógico |
   | PAM8302A | PAM8302AASCR | 32 amplificadores (abs max de SD, PSRR, secuencia) |
   | PCM1690 | TI SBAS… | 4 DAC (VCC1/VCC2 vs VDD, POR, secuencia) |
   | AK5538VN | AKM | ADC (AVDD/TVDD, LDOE/VDD18, secuencia, PDN) |
   | MAX809 | MAX809S | supervisor (umbrales, tRP, tipo de salida) |
   | AO3401A, BZT52C10, BAT54C, SS34, SMBJ20A | | polaridad inversa y OR |
   | MWSA1004S / MWSA0603S | L1 / L2 | Isat y corriente nominal (la hoja del L2 ya está: `4.7uH.pdf`) |
   | ferrite `C21286` (FB1, FB3, FB69) | | **corriente nominal y caída de impedancia con DC** |
   | 100 µF/35 V `C2836437`, 220 µF/16 V `C25503905` | | ESR, rizado, ¿electrolítico o polímero? |
   | CAT24C256, USBLC6-2P6, PRTR5V0U2X, SMD1206P110TF/16, BSMD1206-050-30V | | pines de alimentación, tensiones máximas |

   Ya están: `stm32h743vi.pdf`, `esp32-c3-mini-1.pdf`, `4.7uH.pdf`, `MP2315S.pdf` (**es el de la
   DS11: no usarlo**). Regla para citar: **documento + revisión + página/tabla + la frase exacta**.
   Una cita que no se pueda ubicar no cuenta.
3. **Mapa de designadores.** `DS12_ALIMENTACION.md` usa nombres propios que **no coinciden** con el
   esquemático (§12). Todo el informe usa los del **netlist**: IC4 = buck A (AP63356), U11 = buck B
   (AP63301), D9 = TVS, D21 = zener de Q1, D23 = `+5V_AUX`→`VIN_B`, FB69 + D19 = `+5VA`→`+5V_AUX`.
4. **Cada herramienta que se escriba lleva prueba negativa** (regla del repo: «preguntarle qué
   haría si no hubiera nada que medir»). Ejemplo: un chequeo «cada pin de alimentación está en el
   riel que dice el datasheet» se prueba **intercambiando a propósito dos pines en una copia del
   netlist** y verificando que lo marca. Una calculadora (divisores, UVLO, rizado, corriente de
   arranque) **primero reproduce los números ya conocidos** (5,088 V · 3,280 V · UVLO 14,3/11,1 V)
   y recién después se usa para algo nuevo. Los cálculos con tolerancias se hacen en **peor caso**
   (R ±1 %, VFB ±1 %, VIN y carga en los extremos), no sólo nominal.

---

## 2. El mapa del terreno (extraído del netlist de hoy)

```
 J11/J12 ─ +20V ─ F1 ─ Q1(P-FET; D→F1, S→VIN_LOCAL) ─ +VIN_LOCAL ─┬─ IC4 VIN  (EN: R151 470k / R156 40,2k)
  (cadena)         D21 zener 10 V (gate-source)   D9 SMBJ20A       │   AP63356 → L1 6,8µH → +5VA
                   R149 100k (gate→GND)            C273,C314 100µF │      PG → R159 a +3V3 / R152→PA10 / D3
                                                   C316,C324,C326  │
                                                   10µF + C315     └─ D20 (SS34) ─→ /DS_Power/IN  ("VIN_B")
 USB ESP32 (J3): VBUS─F2─D2 ─┐                                                      │
 USB STM32 (J8): VBUS─F3─D8 ─┼─► +5V_AUX ──► D23 (SS34, A=+5V_AUX, K=VIN_B) ───────┤
 ST-Link J7.10 ────────D11 ──┤     │                                                 ▼
 +5VA ─FB69─D19(SS34)────────┘     ├─ U10 AP2125 → +3.3VA (AK5538 AVDD1/2, 4×VREFH, 8× PRTR5V0U2X VCC)
                                   └─ F6/F7 → J5.10 / J6.10           U11 AP63301 (EN=VIN) → L2 4,7µH → +3V3
 +5VA ─┬─ 32× PAM8302A VDD (+5×220µF C290-293.. , C69,C70 22µF, 32 bypass)
       └─ FB3 → +5VA_DAC → 4× PCM1690 VCC1/VCC2
 +3V3 ─┬─ STM32 (VDD×6, VBAT, VDD33USB) ─ FB1 → VDDA/VREF+       ─ ESP32-C3 (3V3) ─ 4×PCM1690 VDD
       ├─ AK5538 TVDD, PW0-2, LDOE ─ MAX809S (→NRST) ─ CAT24C256 ─ pull-ups ─ J5.9/J6.9 vía F4/F5 (PTC)
       └─ J7.1 vía R31 100 Ω (VTref)
 Lecturas: PC1 = +5VA/2 (R162/R163/C327) · PA10 = PG (R152) · PA5 = VBUS (R160/R161) · PC7→R157→PDN_AK (R158 a GND, D3 → PG)
```

Los **diez estados de alimentación** que el análisis tiene que cubrir (los siete del §5 de
`DS12_ALIMENTACION.md` más tres que ese documento no tiene):

| # | estado | nota |
|---|---|---|
| E1 | 20 V solo | normal |
| E2 | 20 V + USB (uno o los dos) + ST-Link | los tres «ORs» a la vez |
| E3 | **solo USB** (sin 20 V) | flasheo / bring-up |
| E4 | solo ST-Link **con** USB | |
| E5 | solo ST-Link **sin** USB | no debería encender nada |
| E6 | 20 V cayendo por UVLO (14,3 → 11,1 V) | amps off, MCU vivo |
| E7 | **enchufe en caliente** de 20 V (rampa de entrada) | 🆕 |
| E8 | **apagado**: 20 V se va, el USB queda, y al revés | 🆕 transiciones, no estados fijos |
| E9 | **arranque simultáneo** de 15 placas de la cadena | 🆕 |
| E10 | cortos: en `+3V3`, `+5VA`, `+5V_AUX`, expansión (J5/J6), VBUS | los del §6 |

> **El artefacto central de la revisión es una tabla de estados**: para cada E1…E10, la tensión
> esperada de `+20V`, `VIN_LOCAL`, `VIN_B`, `+5VA`, `+5V_AUX`, `+3V3`, `+3.3VA`, `+5VA_DAC`, PG,
> `PDN_AK` y el pin SD de los PAM. Con esa tabla completa, cada pin de cada chip se cruza contra
> «¿qué le pasa si *su* riel está en X y *esta* señal en Y?». Sin ella no se ven los problemas de
> convivencia: los detalla uno solo y el otro lado.

---

## 3. Workstreams

### A. Cada fuente contra su hoja de datos (analista A)

Para **cada** regulador, y por cada pin: valor dibujado vs. valor recomendado, **con cita**.

- **A1 · Entrada** — F1, Q1 (AO3401A: VGS máx, RDS(on) a VGS=−10 V, cuerpo-diodo en el hot-plug),
  D21 (zener 10 V: el margen de VGS con la tolerancia), R149, D9 SMBJ20A (VRWM, fuga a 20 V,
  tensión de clamp contra los 35 V de ambos bucks y contra el máximo de EN/BST/PG), C273/C314
  (¿electrolítico de qué tipo, ESR a 100 kHz?), C315/C316/C324/C326.
  🔎 *¿Y si el ladrillo entrega más de 20 V en vacío?* (los de «19 V» suelen medir 20-21 V; un
  de 24 V quemaría D9). Cuenta contra la fuga y la VBR del SMBJ20A.
- **A2 · Buck A, IC4 (AP63356, DS41949)** — pin por pin y además: VIN/EN/BST/PG máx absoluto;
  **R150/R153** (5,088 V en peor caso: ¿se pasa de 5,5 V que es el máximo de PAM8302A, de VCC de
  PCM1690 y de AP2125?); UVLO con R151/R156 en peor caso (VON/VOFF, y el efecto de las corrientes
  I1/I2 con tolerancias); L1 (valor, Isat, **corriente nominal térmica**) contra la tabla de
  componentes **para 5 V** del datasheet; Cff 220 pF (C318); capacitores de entrada
  (corriente RMS 1,53 A contra lo que entregan 3×10 µF/50 V X7R **con el sesgo de CC a 20 V**);
  BST; PG (pull-up, **máximo de tensión y corriente de sumidero**, y qué hace sin VIN).
  🔎 **Capacidad de salida y arranque.** La salida de `+5VA` tiene ≈1,1 mF de bulk
  (C290-293 y compañía) + C69/C70 + los 32 bypass de los PAM. Mirar: ¿el datasheet limita la
  capacidad de salida máxima? ¿cuál es el soft-start y entonces cuánta corriente de carga exige
  (I = C·dV/dt, **≈1,4 A sólo para cargar el bulk** a 4 ms), contra el límite de corriente del
  chip y la carga de los PAM? ¿Entra en *hiccup* en el arranque? ¿La compensación interna es
  estable con esa capacidad y con un electrolítico de ESR conocida (la 220 µF de `C25503905`:
  confirmar si es polímero o aluminio)?
- **A3 · Buck B, U11 (AP63301, DS42002)** — lo mismo, y además:
  🔎 **Modo USB (E3).** `VIN_B` queda en ≈3,84–3,98 V (documento, §3.4). Contra: UVLO de
  encendido y de apagado y su histéresis; **ciclo de trabajo máximo / tiempo mínimo de apagado**
  (3,28 V desde 3,9 V pide ≈84 % de duty); **dropout** a 0,9 A con la RDS(on) del lado alto
  + DCR de L2; qué hace el chip cuando se queda sin duty. ¿Un pico de TX del ESP32 puede bajar
  `VIN_B` bajo el UVLO y generar un ciclo de reinicios idéntico al síntoma de agosto?
  Y L2 (Isat/Irms), Cff 100 pF contra la tabla del 3,3 V, capacitores de entrada y salida.
- **A4 · U10 (AP2125K-3.3)** — VIN máx. 6 V contra **todos los casos de `+5V_AUX`** (incluido el
  hot-plug de USB: ¿hay picos > 6 V?); EN atado a VIN con R30 100 k (¿es válido en la rampa?);
  caída de dropout y PSRR **a la frecuencia que de verdad hay**: 450 kHz (buck A), 500 kHz (buck B),
  200-300 kHz (32 PAM). Capacitores de salida (C63, C64, C67 22 µF) contra el rango de capacidad
  y ESR que pide el chip: **C67 22 µF puede estar fuera de la ventana de estabilidad**.
- **A5 · IC1 (MAX809S)** — umbral mín/típ/máx **contra lo que necesita cada consumidor** (STM32:
  VDD mín; **ESP32-C3: 3,0 V mín.**; PCM1690; VDD33USB ≥3,0 V). Duración de reset (tRP), tipo de
  salida (push-pull) contra el NRST del STM32 y su pull-up. 🔎 *¿Hay una franja
  2,93–3,0 V en la que corre el ESP32 fuera de especificación?*
- **A6 · El OR de fuentes** — D2, D8, D11, D19, D20, D23 (SS34) y PTC F2/F3/F4-F7 y FB69/FB3/FB1:
  tensiones inversas, caídas con la corriente real **de cada modo**, fuga del SS34 en reverso a
  temperatura, **corriente nominal y saturación del ferrite `C21286`** (0805 con 220 mΩ: ¿qué
  pasa con 1 A de expansión pasando por FB69? ¿y cuánto de su impedancia queda con el sesgo?).
- **A7 · Conectores de energía** — J11/J12 (corriente nominal del `C41784016` contra los 1,2 A
  por placa × placas de la cadena; es la única parte del arnés que el esquemático fija),
  USB-C J3/J8 (CC1/CC2: R10/R11/R22/R23 son 5,1 kΩ de **Rd**; si no, un host C-a-C no entrega
  VBUS), J7 (pin 1 con R31, pin 10 con D11), J5/J6 pines 9/10.

### B. Cada consumidor contra su hoja de datos (analista B)

Para cada chip: **todos** los pines de alimentación, rango de tensión, máximo absoluto,
secuencia, qué pasa con una entrada digital por encima del riel propio, desacople recomendado.

- **PAM8302A ×32** — VDD (2,0–5,5 V; máx absoluto), **pin SD**: ¿hay diodo de protección a VDD?
  ¿máximo absoluto de SD respecto de VDD? ¿corriente de entrada? Pull-down de SD (¿existe en el
  esquemático?) y qué hace con SD flotando durante el arranque del STM32.
- **PCM1690 ×4** — VCC1/VCC2 (5 V analógico, vía FB3) **y** VDD (3,3 V): ¿hay requisito de orden
  entre ambos? ¿qué vigila el POR (el documento dice sólo VDD, §7.3.5)? ¿qué hace el chip con
  VCC en 0 y VDD en 3,3 V (E3)? VCOM, desacople, **pines `MS/ADR0`, `TEST/ADR1`, `RSV`** (los
  netlists los muestran en `+3V3`/GND: confirmar contra la tabla de pines).
- **AK5538VN** — AVDD1/2 (`+3.3VA`), TVDD (`+3V3`), **LDOE → ¿de qué riel sale el VDD18 interno?**,
  secuencia de AVDD/TVDD/PDN, PDN en bajo ≥150 ns tras subir los rieles, y qué pasa si AVDD sigue
  a `+5V_AUX` (sube con el USB, **antes** que el 20 V) pero TVDD tarda otra rampa. Desacople de
  los cuatro VREFH y de VDD18.
- **STM32H743** — los seis VDD, VBAT (a `+3V3`), **VDD33USB**, VDDA/VREF+ por FB1, VCAP, orden
  de rampas, tiempo de subida máximo, y las entradas de PC1/PA5/PA10/PC7 contra FT (5 V-tolerantes
  o no) por si algún divisor sale mal.
- **ESP32-C3-MINI-1** — 3V3 (3,0–3,6 V, corriente pico), rampa y la red de EN, sus capacitores.
- **CAT24C256, USBLC6-2P6, PRTR5V0U2X ×8** (VCC a `+3.3VA`: son carga del AP2125 y un camino de
  fuga hacia el riel analógico), PTC, LEDs y pull-ups que cuelgan de cada riel.

### C. Convivencia — lo que ninguna hoja de datos describe sola (analista C)

Este es el **centro** del pedido. Una checklist, cada ítem cierra con una tabla (estado × riel)
o con un cálculo.

1. **Secuencia y rampas.** Orden real de encendido de `+3V3`, `+5V_AUX`, `+3.3VA`, `+5VA_DAC`,
   `+5VA` en E1/E3/E7, con tiempos (soft-start de cada buck, UVLO del A, retardo de PG de 3,5 ms).
   ¿Algún consumidor exige un orden que el esquemático no garantiza?
2. **Pin alimentado en un chip apagado (la fuente de los daños silenciosos).** Para cada señal que
   cruza de un dominio al otro, en **cada** estado: ¿puede un pin a 3,3 V alimentar parásitamente
   (por el diodo de protección a VDD) a un chip cuyo riel está en 0? Lista mínima:
   - 🔎 **E3:** `PD3 → R20 33 Ω → SD` de los 32 PAM con `+5VA = 0`. El documento lo resuelve «en
     firmware» pero **no cuenta el camino por hardware**: ¿cuánta corriente entra por los
     diodos de SD, a qué tensión queda `+5VA`, y **podría quedar por encima del mínimo de 2,0 V
     de los PAM** (amplificadores encendiéndose parcialmente, alimentados desde un GPIO)?
     Considerar también la ventana de arranque **antes** de que corra el firmware.
   - 🔎 **PG de IC4** con VIN=0 y R159 a `+3V3`: ¿la hoja de datos admite un pin PG a 3,3 V con el
     chip sin alimentar? ¿Y EN, FB, SW, BST?
   - **PCM1690** con VDD=3,3 V y VCC=0 (SAI, I²C, MCLK, AMUTE, ~RST llegan desde el STM32).
   - **AK5538** con TVDD arriba y AVDD en otra rampa; `PDN_AK` a través de D3 (BAT54C) hacia PG.
   - **I²C** (CAT24C256 + 4 PCM1690 + pull-ups a `+3V3`) cuando algún chip está apagado.
   - **Flujo inverso por el cuerpo de los bucks:** si `VIN_LOCAL` cae por debajo de `+5VA`
     (1,1 mF de bulk), 🔎 ¿el diodo de cuerpo del lado alto de IC4 alimenta `VIN_LOCAL` y, por
     D20, a `VIN_B` (hold-up fantasma de `+3V3`, PG en estado indeterminado)?
3. **Arbitraje de las fuentes en `+5V_AUX`.** 🔎 **Sospecha concreta**: el documento (§5, tabla de
   modos) dice que con 20 V presente «D2 queda en reverso y la PC no entrega nada». Pero en
   operación normal `+5V_AUX ≈ 5,09 V − FB69 − Vf(D19) ≈ 4,7–4,8 V`, y el VBUS de una PC es
   4,75–5,25 V menos F2 y el diodo. Con SS34 de caída pequeña a baja corriente, **D2 (y D8, D11)
   podrían conducir y repartir la carga con el buck**. Hacer la cuenta de equilibrio entre las dos
   ramas (curva V-I del SS34 a baja corriente, impedancias, tolerancias) para el rango de VBUS
   y de `+5VA`. Si D2 conduce: ¿importa? (corriente que se le pide a la PC con 20 V presente,
   lazos de masa por el cable USB, transitorios al enchufar/desenchufar el USB con la placa en marcha,
   ST-Link con 5 V propio compitiendo.)
4. **Enchufe en caliente y corriente de arranque.**
   - 20 V: C273 + C314 (200 µF) + 5×10 µF contra F1 rápido de 2 A y el SMBJ20A.
     Ya hay una cuenta (A²s, §11.2); revisarla con los capacitores **realmente** dibujados.
   - 🔎 **USB (E3):** la capacidad total que ve VBUS (`VIN_B`: C319+C325 = 20 µF, C60 = 22 µF,
     C320, +5V_AUX: C57, C59, entrada de U10, expansión) **supera los 10 µF** que la especificación
     USB permite al enchufar. Un puerto con limitación de corriente se dispara o se cuelga: es
     exactamente el cuadro «la placa no arranca desde USB». Calcular la corriente de arranque con
     el PTC (F2/F3) y el SS34 en serie, y ver qué parte de esos 42 µF cuelga del lado
     del diodo.
   - 15 placas (E9): arranque simultáneo, caída de tensión de la cadena y UVLO de 14,3 V
     (histéresis de 3,2 V): ¿se puede entrar en oscilación de encender/apagar los amplificadores?
5. **Presupuesto de corriente y tensión, riel por riel y por modo.** Tabla con carga típica/pico
   de cada consumidor contra el límite de cada fuente **y de cada elemento en serie** (F2: 1,1 A de
   hold; FB69; D19; D2; PTC de 500 mA ×4; L1/L2; el soft-start de IC4). Incluir la línea
   reservada `+3.3VA` para el VCM del ADC. Los peores casos relevantes: USB-solo con ESP32
   transmitiendo; 20 V con ambas expansiones al máximo; arranque de los 32 PAM.
6. **Convivencia de los lazos y ruido en el camino analógico.** Cadena
   `+5VA → FB69 → D19 → +5V_AUX → U10 → +3.3VA → AVDD/VREFH del AK5538`:
   cuánto de la conmutación del buck A (400-500 kHz con FSS ±6 %), de los 32 PAM (200-300 kHz
   libres, sin sincronizar) y del buck B (450-550 kHz) llega al AVDD, contra el PSRR del AP2125
   **a esas frecuencias** (no a 1 kHz) y contra la exigencia de AVDD/VREFH del AK5538. ¿Es el
   ferrite FB69 realmente un filtro ahí (el documento mismo dice que sólo actúa por encima de
   1 MHz)? Lo mismo para `+5VA → FB3 → +5VA_DAC` y `+3V3 → FB1 → VDDA`.
   Esto es **sólo con lo que dice el esquemático**: no se opina de dónde cae cada pista.
7. **Modos de falla y su coordinación.** Para cada corto de E10: quién protege, cuánto tarda
   (PTC: segundos; hiccup; fusible), **qué ve el STM32 mientras tanto** (¿MAX809S lo resetea antes
   de que el PTC abra?), y si el corto de un riel deja a otro en un estado peligroso para un chip
   (el caso de §2 pero provocado por una falla).
8. **Referencias de tierra y banderas, a nivel esquemático.** GND único (hay una única red
   `GND` de 419 nodos): J7.4 y J7.9 son ambos GND; J7 y J5/J6 son el mismo conector físico
   (`DS12_AUDITORIA.md` §3.1: confirmar si se corrigió); apantallamiento de los USB-C
   (`Net-(J3-SHIELD)`, `Net-(J8-SHIELD)`: a dónde van); `PWR_FLAG` y ERC en cada riel de
   alimentación (`tools/sch2net.py … --erc`); **pines al aire de alimentación**
   (`awk -F'\t' '$2==1' redes.txt`: hay 103 redes de un solo nodo, filtrar las de pines de
   alimentación o `unconnected-` de chips que sí deban estar conectados).
9. **Observabilidad.** ¿Cada riel tiene punto de prueba (TP1-TP8) y **está conectado**? ¿PC1, PA5,
   PA10 alcanzan para que el firmware distinga E1-E6? (La lectura PC1 = `+5VA`/2: divisor 100 k/100 k
   con 4,7 nF; revisar impedancia de fuente contra el tiempo de muestreo del ADC del H743 y la
   tensión máxima en el peor caso.)

### D. Auditor numérico independiente (analista D)

No lee las conclusiones de A/B/C. **Recalcula desde cero, con script y peor caso**, cada cifra
que aparece en `DS12_ALIMENTACION.md`: 5,088 V, 3,280 V, UVLO, rizado, corriente RMS de
entrada, margen del modo USB, balance térmico de cada buck, Isat de L1/L2 contra el límite de
pico real de cada chip, A²s del fusible, tiempo de caída de `+5VA`, y **los capacitores con su
sesgo de CC real** (un 10 µF/50 V X7R 1210 a 20 V no son 10 µF; un 22 µF/16 V X5R 0805 a 5 V
tampoco). Para cada número: ¿coincide? Si no, ¿cuál es el error y qué componente toca?

---

## 4. Cómo orquestar a los agentes (para que se refuten de verdad)

Presupuesto sugerido: **4 analistas + 3 refutadores + Opus de árbitro**, no más.

**Ronda 1 — análisis (4 agentes en paralelo, alcances disjuntos).** A, B, C, D del §3.
Cada uno recibe: este plan, los tres archivos del digest, la carpeta de datasheets, el mapa de
estados vacío (§2) y la regla de citas. **Ninguno parsea el `.net`.** Cada hallazgo se entrega en
el formato del §5, con **veredicto propio y cómo lo verificó**.

**Ronda 2 — refutación cruzada (3 agentes nuevos, sin el razonamiento de los analistas).**
Cada refutador recibe sólo la **afirmación + la evidencia** (cita, comando, cálculo) y tiene la
instrucción de **destruirla**: buscar en la hoja de datos la frase que la contradice, rehacer la
cuenta con otras tolerancias, buscar el estado de alimentación en que no se cumple, buscar si ya
está cubierto por otro componente del esquemático (el error clásico: «falta un pull-down» cuando
hay uno en otra hoja). Reparto cruzado: los hallazgos de A los refuta quien no leyó A, etc.
Resultados posibles, y sólo estos:

| veredicto | significa |
|---|---|
| **CONFIRMADO** | el refutador intentó y no pudo; reprodujo la cita y la cuenta por su lado |
| **REFUTADO** | tiene la cita o el cálculo que lo contradice (se descarta y se anota por qué) |
| **ABIERTO** | ni se confirma ni se refuta con las hojas de datos disponibles → va a la lista de «hay que medir» |

Regla del repo: **«verificar antes de afirmar y decir cómo»**. Un hallazgo sin comando de netlist,
cita o cálculo reproducible no entra al informe, aunque suene bien.

**Ronda 3 — arbitraje (Opus).** Reconcilia lo que A/B/C/D y los refutadores se contradicen,
ordena por severidad, y **separa las decisiones que son de Fernando** (compromisos entre costo,
margen y cantidad de componentes) de las correcciones sin discusión.

**Control de calidad antes de cerrar:** hacer pasar por el mismo proceso **un hallazgo plantado**
(un error inventado y conocido, p. ej. «R153 es 6,8 kΩ») para comprobar que la ronda 2 lo
detecta. Si no lo detecta, el método no mide nada (es el caso de las `-138 dBFS` del analizador
de fase). Y al revés: **un hallazgo ya conocido como real** (el PTC del §3.3 de la auditoría)
tiene que salir confirmado.

---

## 5. Formato de cada hallazgo

```
ID · severidad · refs del netlist
  Severidad: 🔴 bloquea (daña un chip, impide arrancar o falla en un modo previsto)
             🟡 ajustar (margen insuficiente, funciona en banco y falla en campo)
             🟢 vigilar / mejora barata
  Qué pasa, con el mecanismo explicado para un arquitecto (no sólo el resultado)
  En qué estado(s) E1…E10 ocurre
  Evidencia del esquemático: el comando exacto contra el digest (net, pines)
  Evidencia del datasheet: documento · revisión · página/tabla · la frase
  Cuenta (si la hay): el script y el peor caso
  Refutación: quién, qué intentó, resultado
  Arreglo mínimo: con valores/part numbers que YA estén en BOM-DS.csv; si requiere uno nuevo, decirlo
  ¿Afecta a layout? sí/no  (si sí: lista aparte, no se desarrolla)
```

**Entregables:**
1. `hardware/DS/DS12_REVISION_ALIMENTACION.md` — el informe, con la **tabla de estados (§2)
   completa** al principio, los hallazgos ordenados por severidad, la lista de **«hay que medir en
   banco»** (con el valor esperado y el umbral, como el §9 de `DS12_ALIMENTACION.md`) y la lista
   «para cuando empiece el PCB».
2. Las **correcciones propuestas a `DS12_ALIMENTACION.md`**, sin aplicarlas: mostrar el diff y
   esperar que Fernando decida.
3. Nada se commitea ni se toca en el esquemático ni en el `.net`: **es revisión, no edición.**

---

## 6. Sospechas de partida (🔎 todas sin verificar — sirven de semilla, no de conclusión)

Ordenadas por lo que más probablemente importe. **Cada una puede resultar falsa**, y que un
refutador la tire abajo es un buen resultado.

| # | sospecha | dónde se mira | por qué la sospecho |
|---|---|---|---|
| S1 | **D2/D8/D11 conducen con el 20 V presente** (no quedan en reverso) | C3 / A6 | `+5V_AUX` ≈ 4,7–4,8 V contra VBUS ≈ 4,75–5,25 V; contradice la tabla del §5 |
| S2 | En E3, `PD3` **alimenta parcialmente los 32 PAM** por los diodos del pin SD | C2 / B | el documento lo trata sólo por firmware; el camino por hardware no está cuantificado |
| S3 | **Arranque de `+5VA`** con 1,1 mF de bulk: corriente de carga ≈1,4 A + carga, ¿hiccup? ¿Cout máxima? ¿estabilidad con ESR de la 220 µF? | A2 | datasheet de IC4 no leído todavía en este punto |
| S4 | **El USB no puede enchufarse sin superar 10 µF** (≈42 µF colgados de VBUS vía D2/D23) | C4 | la placa «no arranca desde USB» en el peor momento (flasheo) |
| S5 | **Modo USB: AP63301 al borde** (VIN ≈3,84–3,98 V, UVLO, duty máximo ≈84 %) | A3 | margen de 40-180 mV contra el UVLO, **sin considerar duty ni dropout** |
| S6 | **FB69 (0805, 220 mΩ)** sin corriente nominal verificada, con hasta 1 A de expansión + U10 | A6 | puede saturar y dejar de filtrar justo cuando hace falta |
| S7 | **C67 22 µF a la salida del AP2125** fuera de la ventana de capacidad estable | A4 | el chip especifica un rango de Cout; el DS11 tenía otro |
| S8 | **MAX809S libera el reset a ≈2,93-3,0 V**, por debajo del mínimo de 3,0 V del ESP32-C3 | A5 | las dos hojas de datos |
| S9 | **PG de IC4** en un chip sin VIN (E3) viola un máximo absoluto | A2 / C2 | el documento sólo analizó qué *lee* el STM32, no qué *soporta* el pin |
| S10 | **Flujo inverso por el cuerpo de IC4** al caer `VIN_LOCAL` bajo `+5VA`: alimenta `VIN_B` por D20 | C2 | 1,1 mF contra 255 µF; no hay diodo que lo impida en el lado de entrada |
| S11 | **Ruido del camino analógico**: FB69 no filtra a 200-500 kHz y el AP2125 baja de 65 dB a ~30 | C6 | el documento mismo dice que el bead actúa de 1 MHz para arriba |
| S12 | **Un ladrillo >20 V en vacío** quema D9 o lleva F1 a abrir | A1 | SMBJ20A tiene VRWM = 20 V exacto; los adaptadores de «19 V» miden más sin carga |

---

## 7. Lo que este plan **no puede** resolver

- Los números que dependen de **cómo cae físicamente la corriente** (layout) o del **valor real
  de un cerámico en la placa** (el campo `value` del netlist no trae el dieléctrico). Se anotan
  como «medir».
- El comportamiento real de **un puerto USB de una PC concreta** frente al hot-plug: se acota con
  la especificación, se cierra con una medición.
- Los datasheets que no se consigan: el hallazgo queda **ABIERTO**, nunca «confirmado de memoria».
