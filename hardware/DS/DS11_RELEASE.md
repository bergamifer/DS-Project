# DS11 — release de hardware (punto de retorno)

> Congelado el 2026-08-26, antes de empezar a tocar el PCB para la DS12.
> Tag: **`ds11-release`**. Diseño de hardware sin cambios desde `f8f8ef0` (2026-08-01).

Este archivo existe por una sola razón: que dentro de un año se pueda **volver a la DS11
y mandarla a imprimir otra vez** sin depender de la memoria de nadie ni de archivos
sueltos en el disco.

---

## 1. Qué es la DS11

La placa que se envió a fabricar el **2026-07-08** y se recibió el **2026-08-01** — la
primera con los 32 canales poblados. Es la que está sobre la mesa y contra la que se
escribió todo el firmware actual.

| | |
|---|---|
| Dimensiones | 118,16 × 101,26 mm |
| Capas | 4 (F.Cu / In1.Cu / In2.Cu / B.Cu), 1,6 mm FR4 |
| Perforaciones PTH | 886 |
| Componentes a colocar | 619 (CPL) / 71 líneas de BOM |
| Núcleo | STM32H743 + AK5538VN + 4× PCM1690 + 32× PAM8302A + ESP32-C3-MINI-1 |

## 2. Cómo volver a ella

```bash
# ver el proyecto de la DS11 sin perder lo que estés haciendo ahora
git worktree add /tmp/ds11 ds11-release
open /tmp/ds11/hardware/DS/DS.kicad_pro

# o, si querés pisar el estado actual del hardware (¡commiteá antes lo tuyo!)
git checkout ds11-release -- hardware/DS/
```

## 3. Qué compone el release

Todo dentro de `hardware/DS/`:

- **Esquemático**: `DS.kicad_sch` (raíz) + las hojas `STM32.kicad_sch`,
  `DAC_PCM1690.kicad_sch`, `AK5538.kicad_sch`, `Amp_Bank.kicad_sch`,
  `ALL_AMPS.kicad_sch`, `DS_Power.kicad_sch`, `esp32-mini.kicad_sch`,
  `stm32_custom.kicad_sch`, `connectors_ampli.kicad_sch`, `hat_amplis.kicad_sch`,
  `power_supply.kicad_sch`.
- **PCB**: `DS.kicad_pcb` + `DS.kicad_pro`.
- **Librerías propias**: `librerias/` (incluye el AK5538VN, que no está en las libs de
  KiCad), más `fp-lib-table` / `sym-lib-table`.
- **Archivos de producción** (`jlcpcb/production_files/`):
  - `GERBER-DS.zip` — los 13 gerberes + drills exactos que se subieron a JLCPCB.
  - `BOM-DS.csv` — lista de materiales con códigos LCSC.
  - `CPL-DS.csv` — posiciones y rotaciones para el pick & place.
- **Plots de revisión**: `plots_png_600/` (12 capas a 600 dpi) y `plot/DS.pdf`
  (esquemático completo impreso).

## 4. Verificación (2026-08-26)

Se regeneraron las capas de cobre y el contorno desde el `DS.kicad_pcb` de este tag,
con el mismo KiCad 10.0.3, y se compararon contra los gerberes que fueron a fábrica:

| capa | resultado |
|---|---|
| F.Cu | idéntico byte a byte |
| In1.Cu | idéntico byte a byte |
| In2.Cu | idéntico byte a byte |
| B.Cu | idéntico byte a byte |
| Edge.Cuts | idéntico byte a byte |

(La comparación ignora las dos líneas de fecha del encabezado, que cambian en cada
exportación.) **El proyecto de KiCad de este tag produce la placa que está sobre la
mesa** — no hay drift entre lo que se fabricó y lo que está versionado.

Para repetir la verificación:

```bash
kicad-cli pcb export gerbers --layers "F.Cu,In1.Cu,In2.Cu,B.Cu,Edge.Cuts" \
  --no-protel-ext -o /tmp/gbr hardware/DS/DS.kicad_pcb
unzip -o hardware/DS/jlcpcb/production_files/GERBER-DS.zip -d /tmp/gbr_fab
diff <(grep -v 'CreationDate\|Created by KiCad' /tmp/gbr_fab/DS-CuTop.gbr) \
     <(grep -v 'CreationDate\|Created by KiCad' /tmp/gbr/DS-F_Cu.gbr)
```

## 5. Cómo re-pedirla

No hace falta abrir KiCad. Los tres archivos de `jlcpcb/production_files/` son
suficientes: se sube `GERBER-DS.zip` al pedido, y `BOM-DS.csv` + `CPL-DS.csv` en el
paso de ensamblado. Cuidar dos cosas que ya mordieron una vez:

- **Rotaciones**: JLCPCB rota algunos encapsulados respecto de KiCad. Revisar la
  previsualización del pick & place antes de confirmar.
- **Stock LCSC**: los códigos del BOM son de julio de 2026. Los que caigan fuera de
  stock hay que reemplazarlos a mano (ya pasó con los arrays RN1-4, ver el historial).

## 6. Errata conocida

La DS11 **funciona**, pero no está limpia. Todo lo que se detectó después de mandarla a
fabricar está en [`DS12_PLANNING.md`](DS12_PLANNING.md), con los P0 arriba de todo:
riel digital colgado de `+5VA` por bead+diodo (margen de dropout del AP1117-33), el TVS
SMBJ24A que no clampea por debajo del máximo del buck, la polaridad alternada de los
PAM, y el swap JSPK09/10. Si vas a mandar a imprimir más placas de *esta* versión,
leelo primero y decidí conscientemente que te las bancás.

## 7. Qué no está versionado (a propósito)

- `jlcpcb/gerber/` — los gerberes sueltos; son exactamente el contenido del `.zip` que
  sí está trackeado.
- `DS-backups/`, `*.kicad_sch_old`, `*.kicad_prl` — autoguardados y estado de ventana de
  KiCad. Ruido local, cientos de MB.
- `Versiones/` a partir de la DS10 — la copia local del release DS10 es redundante con
  el tag `ds10-release`. Lo que sí está trackeado ahí es el histórico DS7/DS8/DS9.

Todo lo demás bajo `hardware/DS/` está en el repo: 308 archivos, incluidos `renders/`,
`3d/` (STEP), `CNC/` y los PDF de `plot/`.
