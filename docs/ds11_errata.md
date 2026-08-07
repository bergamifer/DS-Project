# DS11 — Errata de la tirada de producción (2026-08)

> **Afecta a TODAS las placas de la tirada.** Dos cortocircuitos de cobre con la
> misma causa raíz: los gerbers se plotearon con los rellenos de zona
> desactualizados. Ambos localizados con el DRC de KiCad y reparados a mano;
> la placa quedó funcionando el 2026-08-04.

## Causa raíz común: gerbers ploteados sin re-rellenar zonas

En DS11 se agregaron vias de stitching de GND y se retocaron pistas que no
existían en DS10. Al plotear, KiCad usó **el cobre almacenado en el archivo**,
que era anterior a esos cambios — así que los planos nunca abrieron el
clearance alrededor de los objetos nuevos.

Evidencia de la cadena completa:

| Verificación | Resultado |
|---|---|
| DRC con rellenos almacenados | clearance **0.0000 mm** en ambos puntos |
| DRC con `--refill-zones` | las violaciones **desaparecen** |
| Drill de producción (`DS-PTH.drl` línea 660) | la via **se fabricó** |

O sea: **el diseño estaba bien, el ploteo no.** Por eso el defecto es idéntico
en todas las placas y no se ve con lupa (el corte está en capa interna en un
caso, y bajo un componente en el otro).

### Regla de proceso (obligatoria antes de mandar a fabricar)

1. **Refill all zones** (tecla `B` en pcbnew) inmediatamente antes de plotear.
2. Correr DRC **con** y **sin** `--refill-zones` y comparar: si difieren, los
   rellenos están desactualizados.
3. El warning *"zone fills are out-of-date"* que KiCad muestra al plotear **no
   es cosmético** — ahora sabemos que cuesta una tirada entera.

```bash
kicad-cli pcb drc --format json --output drc.json --severity-error --severity-warning DS.kicad_pcb
kicad-cli pcb drc --format json --output drc_refill.json --refill-zones --severity-error --severity-warning DS.kicad_pcb
# si drc.json tiene violaciones que drc_refill.json no tiene -> NO plotear
```

---

## Falla #1 — Via de GND cortocircuitando el plano de +3V3

- **Ubicación**: (136.815, 36.619) mm en el layout — esquina del conector
  **J3 (USB-C del ESP32)**, a ~2 mm de **R9**. Es la **única via** en 4 mm a la
  redonda, así que no hay riesgo de confundirla.
- **Qué es**: via de stitching de GND **suelta** (`free`, size 0.8 / drill 0.4)
  que atraviesa el relleno de **+3V3 en la capa interna In2** sin anillo de
  clearance. El barril metálico toca cobre vivo de +3V3.
- **Síntoma**: **+3V3 a GND ≈ 0.3 Ω**. La placa no arranca por ningún camino de
  alimentación (20 V, ST-Link o USB del ESP32).
- **Por qué era invisible**: el corto vive en una capa interna; no se ve con
  lupa y no hay un componente que se caliente de forma localizable.

**Reparación**: taladrar la via con mecha de **0.9–1.0 mm** desde arriba, a baja
velocidad, comiéndose el barril completo. Es una via de stitching entre muchas:
eliminarla no afecta la continuidad de masa. El plano de +3V3 queda con un
agujero de 1 mm, irrelevante.

**Verificación**: TP1 (`+3V3`) ↔ TP3 (`GND`) debe pasar de 0.3 Ω a cientos de Ω
o kΩ.

## Falla #2 — Pista de +5VA sobre cobre de GND, bajo C290

- **Ubicación**: zona (176–178, 123–125) mm, en el riel de los amplificadores,
  bajo/junto a **C290**.
- **Qué es**: pistas de `+5VA` en **F.Cu** con **0.0000 mm** de clearance contra
  las zonas `Ground` y `5V Analogicos`, más el pad 1 de C290 tocando ambas.
  Mismo mecanismo de relleno viejo, pero en la **cara superior**.
- **Síntoma**: **+5VA a GND en corto**. No lo detecta la medición de `+5V`
  (TP4), porque **+5VA es otro net y no tiene test point** — se mide en el pad
  `+` de C290.

**Reparación aplicada**: se **removió C290** para acceder a la pista y abrir el
cortocircuito con bisturí.

### ⚠️ Consecuencia a monitorear: bulk reducido en el riel de amplificadores

`+5VA` alimenta los **32 PAM8302A** y tenía **4× 220 µF** (C290, C291, C292,
C293) = 880 µF de bulk. Con C290 fuera quedan **660 µF**.

Esto se cruza con el riesgo ya conocido del **buck MP2315S al límite** (ver
`ds11_upgrades.md`): los 32 amplificadores **en fase** —que es exactamente lo
que hace un beam enfocado— son el peor caso de demanda instantánea, y el bulk
es justo lo que absorbe ese transitorio.

**Qué vigilar** al subir volumen con muchos canales activos:
- Reinicios o cuelgues del STM32 (brown-out del riel compartido).
- Distorsión que aparece sólo a volumen alto y con muchos canales.
- Si aparece: **volver a poblar C290** (220 µF / 16 V, LCSC C25503905) una vez
  reparada la pista, o agregar bulk equivalente cerca de FB3.

---

## Estado

- ✅ Ambas fallas reparadas en la placa de trabajo; **compila, flashea y
  arranca** (2026-08-04).
- ⬜ El resto de las placas de la tirada necesita **las dos reparaciones**.
- ⬜ Para una eventual re-tirada: regenerar gerbers **con refill** y verificar
  con el doble DRC de arriba.

## Nota independiente — J7 pin 1 (3.3 V del ST-Link)

No es un defecto de fabricación sino de esquemático, pero conviene tenerlo
junto: el **pin 1 de J7 va directo al riel `+3V3`**, mientras que el pin 10
(5 V) sí pasa por el Schottky **D11**. Sin esa protección, el ST-Link intenta
alimentar toda la placa (STM32H743 + 4 PCM1690 + AK5538 + ESP32 ≈ 700 mA),
colapsa y se apaga su LED.

**Mitigación**: cortar el conductor del **pin 1** en el cable del ST-Link. Para
programar alcanza con SWDIO (2), SWCLK (3), GND (4), NRST (6) y —opcional—
UART7 (7, 8). **Para DS12**: poner un Schottky en serie en el pin 1, igual que
el D11 del pin 10.
