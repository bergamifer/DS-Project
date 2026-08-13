# DS12 — Backlog de hardware (próxima versión / lote)

> Abierto 2026-07-08, con la **DS11 ya enviada a fabricar**. Acá se acumula todo lo que
> se detectó tarde o se decidió posponer, para no perderlo. Nada de esto bloqueó la DS11;
> son mejoras/verificaciones para la próxima vuelta. Ver [DS11_PLANNING.md](DS11_PLANNING.md).

---

## 🔴 Prioridad alta

### 1. Margen del buck a 24V — el TVS NO protege al MP2315S
Verificado con el datasheet del D8 (SMBJ24A, C908801) el 2026-07-08:
- SMBJ24A: standoff **24V**, VBR(min) **26.7V**, **VC (clamping) 38.9V** @ 15.4A, 600W, unidireccional. Parte correcta.
- Pero **MP2315S abs max = 26V** → está **por debajo incluso del VBR (26.7V)** del TVS. En un transitorio, el buck muere antes de que el TVS empiece a conducir, y el clamp real (38.9V) está muy por encima. **El TVS no da protección útil al buck en operación a 24V.**
- **Mitigación ya decidida para DS11** (§8.3): alimentar el sistema a **19-20V**. Sigue vigente.
- **Para DS12, elegir una de**:
  - Buck de mayor tensión de entrada (abs max ≥36V; p.ej. familia 36V/60V) → recupera margen real a 24V y permite el daisy-chain a 24V sin caminar sobre vidrio.
  - Si se fija 19-20V como tensión de sistema definitiva: TVS de standoff más bajo (SMBJ18A/20A) que clampee por debajo de los 26V del buck → ahí sí protege.
  - Bulk/snubber de entrada dimensionado para el transitorio del hot-plug del daisy-chain.

### 2. Tap de línea VOUT + R 1kΩ — NO entró en la DS11
Estaba planificado (DS11_PLANNING §8.2) pero **no aparece en el BOM final** de la DS11
(ni la R de 1k ni el header). Se perdió en el cierre. Reincorporar en DS12:
- Header 2 pines **DNP** desde VOUT ch0 del PCM1690 (antes del PAM) vía **R serie 1 kΩ** + GND.
- Habilita: self-test de toda la cadena DAC→ADC (chirp + correlación del AUTOFOCUS),
  verificación empírica de la latencia del nodo, y el **"oscilador en la cabecera"**
  (un nodo genera contenido y lo inyecta al daisy-chain analógico → los demás lo
  beamformean coherente). Ver [../../docs/multinode.md](../../docs/multinode.md) §2.
- Costo: 2 pads + 1 R.

### 2b. J7 pin 1 (3V3 del ST-Link) sin protección — poner Schottky
Detectado en el bring-up de la DS11 (2026-08). El **pin 1 de J7 va directo al riel
`+3V3`**, mientras que el pin 10 (5V) sí pasa por el Schottky **D11**. Sin esa
protección el ST-Link intenta alimentar toda la placa (STM32H743 + 4 PCM1690 + AK5538
+ ESP32 ≈ 700 mA), no puede, y se apaga su LED — que es exactamente el síntoma que
hizo perder horas creyendo que había un corto. Mitigación en DS11: **cortar el pin 1
del cable**. Para DS12: **Schottky en serie en el pin 1**, igual que el D11 del pin 10.
Detalle en [../../docs/ds11_errata.md](../../docs/ds11_errata.md).

### 2c. Regenerar gerbers CON refill de zonas — regla de proceso
La tirada entera de DS11 salió con **dos cortocircuitos de cobre** porque los gerbers se
plotearon con los rellenos de zona desactualizados. No es un cambio de PCB: es una regla
de proceso obligatoria antes de mandar a fabricar (refill + doble DRC comparado). El
procedimiento exacto está en [../../docs/ds11_errata.md](../../docs/ds11_errata.md).

---

## 🟡 Prioridad media

### 3. Línea de timing GPIO4→PC13 (ESP32→STM32)
Hedge opcional analizado el 2026-07-07 (verificado en netlist). No entró en DS11.
Para DS12, si se quiere un disparo de timing sin jitter entre procesadores:
GPIO4 (U5 pin 18) → PC13 (U7 pad 7) con **R serie ~220 Ω** cerca del ESP32; PC13 como
entrada/EXTI con pull interno (activa-baja). **No rutear la inversa** — el UART ya es
interrupt-driven en ambos sentidos. Ver [../../docs/esp32_roles.md](../../docs/esp32_roles.md).

### 4. Ferrites de salida de los PAM (64 posiciones) — solo si se quiere full power
Hoy: un solo bead genérico (C21286, "FerriteBead" 0805) para las 67 posiciones,
incluidas las 64 de salida de parlante (2 por parlante) + FB2/FB3 de potencia del +5VA.
Para el uso real (muy por debajo del full output) **está bien** — la corriente es chica,
no satura, filtra bien. **Solo si DS12 va a correr a plena potencia**: especificar para
las 64 de salida un bead de clase-D con **corriente nominal ≥1.5-2 A y DCR ≤0.1 Ω**
(el genérico de señal ~0.3 Ω/200-500 mA saturaría y perdería filtrado a ~1 A de pico).
Dejar el chico solo donde la corriente es de mA (FB1/VREF+).

### 5. C45/C46 en 0201 → pasar a 0402
Son los caps de carga del cristal de 32.768 kHz (8.2 pF, valor correcto). Es el **único
0201 de toda la placa** (el resto es 0402+). Migrar a 0402 mismo valor: más fácil de
montar, retrabajable a mano, no fuerza proceso. Footprint heredado.

### 6b. Barrel jack DC de entrada, **DNP** — para banco y experimentación
Pedido de Fernando (2026-08-13). Hoy la entrada de alimentación son **borneras Phoenix**,
que son lo correcto para instalación fija pero incómodas en la mesa: cada vez que se
quiere probar con una fuente comercial (notebook-style, bench supply con plug) hay que
pelar cable y atornillar. Agregar en paralelo un **jack barrel DNP** (sin poblar de
fábrica) que se puede soldar sólo en las placas de laboratorio.

- **Footprint**: DC-005 / barrel 5.5 × 2.1 mm, through-hole (el 2.1 mm es el estándar de
  las fuentes tipo notebook y de los bench supplies con plug).
- **En paralelo con la Phoenix de entrada**, mismo net, antes del TVS y del buck — así
  toda la protección existente (D8, fusible si lo hay) cubre las dos entradas por igual.
- **Tensión**: la placa corre a **19-20 V** (ver ítem 1), que es justo el rango de los
  cargadores de notebook — encaja natural. Marcar la serigrafía con **"19-20V"** y la
  polaridad (centro positivo) para no quemar el buck con una fuente de 24 V.
- ⚠️ **No poblar los dos a la vez** con fuentes distintas conectadas: serigrafiar
  "USE ONE" o similar. Dos fuentes en paralelo por el mismo net es un problema.
- Costo: un footprint THT y un pad extra. DNP = cero costo en la tirada de producción.

### 6. RN5/RN6 y arrays en general → considerar discretas
Ya hubo que reemplazar RN1-4 (62Ω sin stock) por 16 discretas de 75Ω. RN5/RN6 (C1980,
4.7k array) siguen siendo arrays. Para DS12, evaluar discretas de entrada: menos riesgo
de rotación en JLCPCB y de quiebres de stock. (Trade-off: área.)

---

## 🟢 Verificaciones al hacer bring-up de la DS11 (no son cambios de PCB)

- **AP2125 (U10, C110463)**: confirmar en el bring-up que el rail 3.3VA mide **3,3V**
  (el comment del BOM decía solo "AP2125"; verificar que la parte enviada es la variante
  -3.3 y no otra tensión fija). Para DS12: fijar el C-number exacto del AP2125-3.3.
- **10µF cerámicos (C1691)**: si son de 10V, el derating por DC bias a 5,15V deja
  ~5-6µF efectivos. Aceptable para decoupling; solo tenerlo presente si algún rail lo
  necesita nominal.
- **Sincronía BOM ↔ doc de planning**: el D8 salió con C908801 (correcto) pero el doc
  tenía C726830. Mantener los C-number del BOM igual que el planning para no repetir el
  mecanismo por el que entró el SM6T15A en la tanda 1.

---

## ✅ Confirmado OK en la DS11 (no tocar)
- Sustituciones de BOM validadas: 100pF→C327198 (YAGEO NP0 ±1%), 4.7nF→C2961271
  (**C0G confirmado por Fernando**), RN1-4→16×75Ω discretas (R133-148, R131=33Ω intacto
  en MCLK), electrolíticos en rieles ≤5V salvo C56 (50V en +24V), divisor buck 40.2k/7.5k.
- D8 = SMBJ24A unidireccional legítimo (datasheet verificado).
- Rotation corrections aplicadas: JST_SH_BM10B=180, AP2125/SOT-23-5, PRTR/SOT-143=90,
  220µF/CP_Elec_6.3x7.7=180.
