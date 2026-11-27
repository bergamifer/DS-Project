# Ronda 2 · Bloque B (afirmaciones heredadas) + S12 — informe del analista «heredado»

> En curso (2026-10-07). Este archivo se actualiza a medida que avanzo: si se corta, queda lo hecho.

## Verificación del netlist (hecha)

Comandos sobre `.ds12_audit/{componentes.tsv,redes.txt}` (netlist del 1-oct 15:29, vigente):

- `Net-(IC4-EN)`: IC4.2, R151.1, R156.2; R151.2 en `+VIN_LOCAL`, R156.1 en GND → R151 470 k arriba, R156 40,2 k abajo. ✔
- `/DS_Power/FB_A`: IC4.3, R150.1, R153.2, C318.1; R150.2 y C318 en `+5VA`, R153.1 en GND → 40,2 k / 7,5 k, Cff 220 pF. ✔
- `/DS_Power/FB_B`: U11.1, R155.1, R154.2, C323.2; R155.2 en `+3V3`, R154.1 en GND → 62 k / 20 k, Cff 100 pF. ✔
- L1 6.8uH `C408485` footprint `L_Sunlord_MWSA1004S`; L2 4.7uH `C408447` `L_Sunlord_MWSA0603S`. ✔
- `+5VA` (71 nodos): 5 × 220 µF `C25503905` + 2 × 22 µF `C45783` + 8 × 10 µF `C1691` + 16 × 1 µF `C15849` + 100 nF + 220 pF
  = **1240 µF nominal**. ✔ Además, detrás de FB3 (`+5VA_DAC`): 4 × 10 µF electrolíticos `C134745` + 8 × 1 µF = **48 µF** que el
  buck A también carga al arrancar, y `+5V_AUX` por FB69 + D19.
- IC1 MAX809S `C347371`; D21 BZT52C10 `C173431`; D9 SMBJ20A `C364296`; Q1 AO3401A `C15127`; R149 100 k (gate→GND). ✔
  `Net-(D21-A)` = D21.2, Q1.1 (G), R149.2; D21.1 (K) en `+VIN_LOCAL` = source de Q1. ✔
- ⚠️ El `description` de D9 dice «15.4A 24V 26.7V 38.9V 5uA»: son los números del **SMBJ24A**. Manda el LCSC (ver fila 6).
- ⚠️ El `BOM-DS.csv` de `production_files` es el de la **DS11** (IC1 = MAX809TEUR+T `C16711`, D8 = SMBJ24A `C908801`,
  L1 = 4,7 µH): no sirve para verificar las piezas nuevas de la DS12.

(sigue)

## Avance (borrador por fila, antes del resumen final)

- **Hojas bajadas** (a `.ds12_audit/hojas/`, con `.txt`): `MAX809S_UMW_C347371`, `BZT52C10_MDD_C173431`, `SMBJ20A_MDD_C364296`,
  `C25503905` (ChuangHui SS221M160E7R7R4Z00ZZ), `AN2606` (Rev 40, copia local de la Mac). LCSC confirma por título de página:
  C347371 = UMW MAX809S, C173431 = MDD BZT52C10, C364296 = MDD SMBJ20A, C25503905 = ChuangHui SS221M160 (polímero sólido).
  JLCPCB confirma C408485 = MWSA1004S-6R8MT y C408447 = MWSA0603S-4R7MT.
- **Fila 4 (MAX809S):** UMW p.6: VRST subiendo, S: 2,857/2,93/3,003 V a 25 °C; 2,78–3,08 V de −40 a 125 °C. Al bajar: «VHYST … 95 %VRST».
  TR 150/240/560 ms. «3,00 V como máximo» vale sólo a 25 °C.
- **Fila 5:** BZT52C10 MDD: VZ 9,5/10/10,5 V @ 5 mA, ZZT ≤ 30 Ω, IR ≤ 0,1 µA @ 7 V. Sin curva a baja corriente.
- **Fila 6:** SMBJ20A MDD p.2: VRWM 20,0 V; VBR 22,20–24,50 V @ IT 1 mA; VC 32,4 V @ IPP 18,6 A; IR ≤ 1 µA @ VRWM.
- **Fila 7:** catálogo Sunlord MWSA-S (escaneado, `datasheets/4.7uH.pdf`, p.6 y p.8, leído en imagen):
  MWSA1004S-6R8MT 6,8 µH ±20 %, DCR 25 mΩ, Isat 9,6 (col. «Max») / 12,0 típ, Irms 7,0/8,5 A;
  MWSA0603S-4R7MT 4,7 µH ±20 %, DCR 33 mΩ, Isat 7,2 / 9,0 típ, Irms 5,0/6,0 A. Nota ※3/※4 (p.11): Isat = caída de **30 %** de L, a 20 °C.
- **Fila 9:** AN2606 Rev 40 §43 (Tabla 90, 91, Fig. 57) y §4.3 (nota de pines RX). Ver texto final.
