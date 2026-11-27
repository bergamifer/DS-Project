# Refutación — ronda 2 (A1, A2, A4, N6)

Scripts en esta carpeta, con autoprueba y prueba negativa: `r_a1*.py` y `r_a2.py`. Las salidas están en `*.out`.

| Ítem | Veredicto |
|---|---|
| A1.a | CONFIRMADO |
| A1.b | **REFUTADO** |
| A1.c | **REFUTADO** (el número) |
| A1.d | CONFIRMADO |
| A2.a | CONFIRMADO, con omisiones |
| A2.b | **REFUTADO** en la cobertura térmica; el 1 kΩ queda |
| A2.c | CONFIRMADO |
| A4.a | CONFIRMADO |
| A4.b | CONFIRMADO en lo eléctrico · **REFUTADO** en el BOM |
| A4.c | CONFIRMADO |
| N6.a | CONFIRMADO · `-rst` ABIERTO |
| N6.b | **REFUTADO** tal como está escrito |

## Evidencia

**A1.a** En LCSC, C45783 es el CL21A226MAQNNNE: 22 µF, 25 V, X5R, ±20 %, 0805. La curva del JSON no la pude volver a bajar de Samsung.

**A1.b** El script da los números afirmados, pero su peor caso no es el peor (n = 3, sin clamp):
- Con +5VA en su tolerancia (5,226 V), +5V_AUX arranca más arriba y el pico da **5,843 V**. Con el AK5538 en PDN y el 1 kΩ, 5,891 V.
- C55 arranca descargado, y a 0 V vale hasta 5,17 µF (el barrido corta en 4): 5,86 V.
- Con 22 µF × 0,9 (la curva es a 120 Hz; el golpe es de ~100 kHz), 5,838 V.
- Todo junto: 3 → 6,00 V · 4 → 5,80 V · 5 → 5,67 V.

Nunca se pasa de 6,5 V, así que no hay daño. ESR, ESL, C57/C59 y el clamp no influyen; las pistas dependen del PCB.

**A1.c** El 1,60 V sale sólo con 3 × 10,7 µF lineales. Al enchufar, los capacitores arrancan en 0 V, donde valen 22 µF. Con eso la caída del hub es **1,95–2,06 V**.

**A1.d** Confirmado con la ECN: «greater than CRPB min … from 0V to 5.25V, regardless of whether … powered or unpowered». Quedó sin probar 2,2 µF (C23630): con C55 = 2 µF, n = 3 da 5,53 V.

**A2.a** Coinciden la Fig. 2, el AP2125 (60/90 µA) y el AK5538 en PDN (10/100 µA, p. 9). Faltan en la lista la fuga de D8 hacia 162 kΩ (es un sumidero y ayuda) y la salida a J5.10/J6.10 por F6/F7 **sin diodo**.

**A2.b** Desde 100 °C, la «envolvente del máximo» de a2 es igual a la típica (630 µA). La tabla de MDD (5 mA a 125 °C) contradice su propia Fig. 2, que ya da 3 mA típicos a 100 °C. Verificable: la fuga típica llega a 5 mA a ~128 °C, y a 0,5 mA a ~97 °C (no 89). El máximo a 85 °C queda ABIERTO.

**A2.c** Las tres razones se sostienen: en modo sólo-USB +5VA vale 0, y el AP63301 necesita 3,7 V (DS42002 p. 6). Hay una cuarta: sin MCLK, el AK5538 no sale de su power-down interno (Fig. 59, p. 55).

**A4.a** DS12110 p. 72 lo confirma, y el `.ioc` no nombra a PE15. El firmware la pone en analógico (main.c:4558–4562): hay que leerla antes de esa línea.

**A4.b** RPU 30–50 kΩ y VIL 0,3 VDD (tabla 60): lee 0,106 V. El modo analógico tras el reset también está en la hoja (p. 32). Pero **C17168 no está en el BOM ni en ningún netlist**, lo que refuerza el 1 kΩ (C11702). Falta dibujarla: PE15 sigue al aire.

**A4.c** `dig11/redes.txt:611` la da al aire. Una fuga de 250 nA sobre 50 kΩ baja 12 mV: lee alto.

**N6.a**
- Medido: OTA → RSR 0x01460000, SFT + PIN (ds11_errata.md:145); MAX809 → 0x00460000, PIN (reporte_2026-09.md:202).
- boot_main.c no toca RSR; HardFault no resetea; no hay IWDG; el ESP32 no está en NRST.
- **ABIERTO** que `-rst` sea un SYSRESETREQ: no pude bajar el UM2237. Además, `flash-cube` son dos invocaciones; si el núcleo corre entre ellas, el primer arranque ve sólo PIN. Se cierra con `make reset` en el banco.

**N6.b** En los datos no hay «intento pendiente», sólo un contador. Leído como `fails > 0`, después de AS_GAVEUP un OTA re-arma el encendido automático de los 32 amplificadores sin que nadie lo decida. Hace falta una marca propia (por ejemplo RTC->BKP3R), borrada en AS_DONE.

## Problemas nuevos

1. **PA5 dice «hay host» sin cable.** La fuga de D8 levanta el nodo VBUS: con un SS34 típico, PA5 lee alto desde 60 °C; con uno en el máximo de tabla, a 25 °C.
2. El `value` de C45783 dice «16V», pero la pieza es de 25 V. Uno de 16 V rompería A1.
3. Los underruns reinician los 60 s de prueba. Si siguen apareciendo, cada apagado nocturno cuenta como falla y a las tres noches el nodo queda mudo.
4. El ESP32 decodifica RSR con bits equivocados (main.c:957–963): POR = 19 y WWDG = 25.
5. +5V_AUX sale a J5 y J6 sin diodo.
