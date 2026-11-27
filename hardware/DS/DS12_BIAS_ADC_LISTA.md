# DS12 — las 15 resistencias de bias del ADC que faltan

> **2026-11-18.** Derivado del esquemático guardado (commit `b31521d`) con `tools/sch2net.py`, no a ojo.
> Hecho para **dibujar sin tener que descubrir nada**: qué red, qué valor, qué pieza. Si esta lista y el
> esquemático se contradicen, gana el esquemático — y corre `tools/verificar_bias_adc.py`.

## Qué es y por qué

El AK5538 no trae polarización DC en sus entradas: con un micrófono o un cable sin nada, las 16 patas
quedan a ≈0 V, lejos del punto medio que pide la hoja (0,48–0,52 × AVDD) (medido en la DS11: M4). La solución, ya decidida y
**confirmada en banco** (M4b = 3,296 V): una red pasiva —un nodo de 1,65 V (VCM) y **una resistencia de
10 kΩ por pata**, sin buffer—. Pasa-altos diferencial ≈18 Hz. Todo en `DS12_CHECKLIST.md` §B.

## Lo que ya está

El nodo VCM es la red `Net-(C58-Pad1)`: **R169** (10 kΩ a `+3.3VA`) y **R170** (10 kΩ a GND) hacen el
divisor; **C58** (100 nF) y **C62** (10 µF) lo filtran. Falta sólo repartirlo.

## Qué dibujar

**Una resistencia de 10 kΩ, 0402, `C25744`** (la misma pieza de R169, R170 y R171) entre el nodo
`Net-(C58-Pad1)` y cada una de las 15 redes siguientes. **Del lado del chip**: en la red `AINnP/N`, donde
están la capacitancia de 100 pF y la pata del IC3 — es donde ya está R171 en AIN4N, y no del lado del jack.
Los números R172… son sugeridos: KiCad numera solo al anotar, y R168 está libre.

| Entrada | Pata de IC3 | Ya hay en la red | Resistencia | |
|---|---|---|---|---|
| `AIN1N` | 60 | R129 (470 Ω) · C280 (100 pF) | R172 | ☐ |
| `AIN1P` | 59 | R130 (470 Ω) · C281 (100 pF) | R173 | ☐ |
| `AIN2N` | 63 | R127 (470 Ω) · C278 (100 pF) | R174 | ☐ |
| `AIN2P` | 64 | R128 (470 Ω) · C279 (100 pF) | R175 | ☐ |
| `AIN3N` | 4 | R125 (470 Ω) · C276 (100 pF) | R176 | ☐ |
| `AIN3P` | 3 | R126 (470 Ω) · C277 (100 pF) | R177 | ☐ |
| `AIN4N` | 7 | R123 (470 Ω) · C274 (100 pF) | **R171** | ✅ ya dibujada |
| `AIN4P` | 8 | R124 (470 Ω) · C275 (100 pF) | R178 | ☐ |
| `AIN5N` | 10 | R121 (470 Ω) · C288 (100 pF) | R179 | ☐ |
| `AIN5P` | 9 | R122 (470 Ω) · C289 (100 pF) | R180 | ☐ |
| `AIN6N` | 13 | R119 (470 Ω) · C286 (100 pF) | R181 | ☐ |
| `AIN6P` | 14 | R120 (470 Ω) · C287 (100 pF) | R182 | ☐ |
| `AIN7N` | 18 | R117 (470 Ω) · C284 (100 pF) | R183 | ☐ |
| `AIN7P` | 17 | R118 (470 Ω) · C285 (100 pF) | R184 | ☐ |
| `AIN8N` | 21 | R115 (470 Ω) · C282 (100 pF) | R185 | ☐ |
| `AIN8P` | 22 | R116 (470 Ω) · C283 (100 pF) | R186 | ☐ |

## Cómo se comprueba

```bash
tools/sch2net.py hardware/DS/DS.kicad_sch /tmp/DS.net
tools/verificar_bias_adc.py /tmp/DS.net        # hoy: "1 de 16"; al terminar: "16 de 16", exit 0
tools/verificar_bias_adc.py --autoprueba       # el chequeo puede fallar: lo demuestra
```

Comprueba que cada una de las 16 redes tiene **una** resistencia al VCM, que vale 10 kΩ, y que no cuelga
nada de más. Hoy da **1 de 16** (sólo AIN4N), que es lo que dice el checklist: el verificador no está
inventando un número.

## Tres cosas para no equivocarse

- **No va una resistencia de 22 kΩ ni de 100 kΩ.** Las dos aparecen en documentos viejos (22 k por el
  ruido con la entrada abierta; 100 k en la primera auditoría). Quedó en 10 kΩ el 2026-10-27.
- **El pedido de JLCPCB:** son 15 piezas más por placa. `C25744` ya está en el BOM; sólo cambia la
  cantidad.
- **En el PCB** estas 15 más C58, C62, R169, R170 y R171 todavía no existen (ver
  `DS12_REVISION_DECISIONES.md` §7).
