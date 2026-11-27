# Ronda 2 · Bloque B (lo heredado) + S12: informe final del analista «heredado»

Netlist verificado (`anexo_verificacion.md`). `BOM-DS.csv` es el de la **DS11**: las piezas nuevas se identificaron por LCSC/JLCPCB.

| # | Veredicto | Número corregido |
|---|---|---|
| 1 Arranque buck A | **CONFIRMADO**, cifras corregidas | 1,29 mF (0,98–1,58). Pico en L1: 2,3 A típ., 3,0 A peor, contra 4,0 A mín. |
| 2 Lazo ≈1,4 kHz | **ABIERTO** | 1,3–2,6 kHz y MF 35–64°, según la red interna supuesta |
| 3 UVLO / salidas | UVLO **REFUTADO**; salidas **CONFIRMADAS** | Enciende 13,6–14,9 V y apaga 10,2–12,1 V (13,4–15,2 / 9,3–13,0 con I_EN ±33 %). `+5VA` 4,95–5,23; `+3V3` 3,20–3,36 |
| 4 MAX809S | Número **REFUTADO**, conclusión **CONFIRMADA** | Suelta el reset a 2,78–3,08 V y lo da a ≈95 % de eso |
| 5 Q1 + zener | **CONFIRMADO** | Compuerta entre 7 y 10,5 V (≈10,9 a 85 °C) |
| 6 TVS + S12 | **CONFIRMADO** a 21–22 V | 24 V: depende de la pieza |
| 7 Inductores | **CONFIRMADO** | L1 9,6 A contra ≤6,5 A; L2 7,2 A contra ≤5,4 A |
| 8 AP63301 con TX | **CONFIRMADO** sobre la base dada | La caída típica es 3,06 V, no 3,26. Con TX, `VIN_B` ≥ 3,55 V |
| 9 DFU | Motivo **REFUTADO** | El bootloader no usa el cristal |

## Evidencia

1. **DS41949 p.6:** tSS = 4 ms, sólo típico; IPEAK_LIMIT 4,0/5,0/6,0 A. **p.16 §7:** limita ciclo a ciclo y entra en hiccup recién tras «512 cycles» seguidos. No hay tope de COUT. Con SD bajo los PAM toman ≤1 µA (DS41333). El buck también carga los 48 µF de `+5VA_DAC`.
   `b1_arranque.py` (autoprueba: 1,46 A heredado). Márgenes: 0,54 A con los DAC con reloj; 0,28 A si además SD está alto; −0,02 A con 1 A en la expansión.
2. gm = 0,3 mS y RT = 0,2 V/A existen **sólo para COMP externo** (p.22, Eq.16); la red interna no se publica. El 1,4 kHz es Eq.16 invertida con el ejemplo externo; con el modelo completo esa red cruza en 2,3 kHz con 39° (`b2_lazo.py`; autoprueba: el ejemplo de la hoja cruza en 44,6 kHz).
   C25503905 es **polímero** (ESR 24 mΩ máx): su cero en 30 kHz suma 2–5°. No ayuda ni perjudica.
3. `b3_uvlo_vout.py`: reproduce Eq.2/3 (p.15) y los nominales 14,27/11,12/5,088/3,280 V. El rango heredado movía sólo VEN_H/VEN_L. R ±1 % (LCSC). IEN no tiene mín/máx en DS41949; para I1 usé el rango del AP63301 (DS42002: 1–2 µA). `+5VA` máx 5,226 V: margen 0,27 V contra los PAM (5,5 V) y 0,77 V contra el AP2125 (6,0 V).
4. **UMW p.6:** S-type 2,857/2,93/3,003 V a 25 °C; 2,78–3,08 V de −40 a 125 °C; «VHYST … 95%VRST». En sólo-USB, `+3V3` baja de 2,93 V sólo si el AP63301 ya se apagó por UVLO.
5. **BZT52C10:** VZ 9,5–10,5 V @ 5 mA; IR ≤ 0,1 µA @ 7 V. Por eso a 0,05–0,2 mA queda entre 7 V y su valor a 5 mA. Con 15 V de entrada, RDS(on) ≤ 60 mΩ. En inversa: VDS −21 V contra −30 V, con VGS = 0.
6. **SMBJ20A:** VRWM 20 V; VBR 22,2–24,5 V @ 1 mA; VC 32,4 V @ 18,6 A, por debajo de los 35 V de los bucks.
   A 21–22 V la pieza está debajo de VBR mín: <1 mA. A 24 V, una pieza de 22,2 V tomaría ~3 A: abre F1 y D9 puede quedar en corto; una de 24,5 V anda normal.
7. **Sunlord** p.6/p.8 (escaneado; transcripción en `hojas/`). **Isat = caída del 30 % de L, a 20 °C** (nota ※3/※4). Pico: límite máx más un tON mínimo de sobrepaso.
8. **DS42002 p.15:** «falls below 3.06V». El 3,26 V no está garantizado.
   `b8_solo_usb_tx.py` usa la Fig.3 del SS34 digitalizada (`digit_ss34.py`). El TX suma 0,33 A y entre 72 y 290 mV de caída (F2 60–210 mΩ, cable 0–0,5 Ω).
9. **AN2606 Rev 40** (2019):
   - Tabla 90: USB «clocked by HSI48» con CRS, en «forced device mode».
   - Tabla 91, V13.3: «Switch USB clock input from HSE to HSI48».
   - §4.3: «keep the RX pins of unused bootloader interfaces … at a known level».
   - PA10 queda fijo en alto (R159+R152) y PG ya subió cuando arranca el bootloader.

## Texto corregido para el artifact

- **Descarté, arranque:** «Carga ~1,3 mF (hasta 1,6) en 4 ms típicos: la bobina llega a 2,3–3,0 A contra un límite de al menos 4,0 A. Sólo se acercaría con 1 A tomado de la expansión al arrancar.»
- **Primera prueba, `+5VA`:** «La hoja no publica la compensación interna. Estimo un cruce de 1,3–2,6 kHz y un margen de 35–64°, sin garantía. Escalón de carga en TP6: más de dos oscilaciones visibles indica poco margen.»
- **UVLO:** «Enciende entre 13,6 y 14,9 V y apaga entre 10,2 y 12,1 V; las corrientes de EN, que la hoja no acota, lo corren hasta ±0,9 V.»
- **Supervisor:** «Suelta el reset entre 2,78 y 3,08 V y lo da a ≈95 % de eso; el riel no baja de 3,20 V.»
- **Sólo USB:** «… y una vez andando se apaga recién debajo de 3,06 V típico.»
- **Hipótesis 2:** «Con 24 V puede andar o abrir F1, según la pieza.»
- **Paso 7:** «No usar el DFU la primera vez: no por el cristal (el bootloader usa su reloj interno), sino porque el ST-Link es el camino probado y hay patas RX al aire.»

## Problemas nuevos

- **N-a.** VON llega a 14,9–15,2 V antes de las caídas de F1/Q1/cable. La ventana conviene escribirla «16–21 V».
- **N-b.** La base de sólo-USB usa 0,36 V por SS34, que en la Fig.3 son ≈0,2 A; a 0,9 A son 0,43 V y la base queda en 3,69–3,83 V. Con 4,75 V en el puerto de la PC y un cable en el límite de la norma, 3,24–3,38 V. **ABIERTO:** osciloscopio en C319 con el cable real y el ESP32 transmitiendo.
- **N-c.** RX al aire: PB15, PA3 (sólo a J6.6) y PE14. Un 100 k `C25741` las fija, si interesa el DFU.
- **N-d.** UMW da «VDD Rise Rate 100 V/s» y `+3V3` sube a ~820 V/s. Probablemente quiso decir V/µs. **ABIERTO.**
- **N-e.** El cero del Cff C318 (18 kHz) queda lejos del cruce nuevo: ya no actúa.
- **N-f.** AL §3.1 pide Q1 «VDS ≥ −40 V»; el AO3401A es de −30 V. No importa eléctricamente, pero el documento contradice la pieza.
- **N-g.** PA9/PA10 están libres pero sin pad ni conector.

## Archivos

- Scripts (con autoprueba y prueba negativa): `b1_arranque.py`, `b2_lazo.py`, `b3_uvlo_vout.py`, `b8_solo_usb_tx.py`, `digit_ss34.py`.
- Hojas bajadas a `.ds12_audit/hojas/`: MAX809S_UMW, BZT52C10_MDD, SMBJ20A_MDD, C25503905 (`cap220uf.pdf` es su catálogo: 220 µF/16 V, 24 mΩ), AN2606 (local; st.com no respondió), `MWSA_L1_L2_transcripcion.txt`.
