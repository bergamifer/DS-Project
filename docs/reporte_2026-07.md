# Reporte — El camino recorrido (junio-julio 2026)

> Escrito 2026-07-03 para lectura posterior. Cubre desde el root cause del ADC hasta la
> unificación de repos, con el registro de decisiones y su racional. Los detalles técnicos
> profundos viven en los docs enlazados; esto es el mapa.

---

## 1. Dónde estábamos al arrancar julio

El bring-up de la DS10 (mayo-junio) había dejado: audio limpio verificado en parlantes
(DAC0/U2, único poblado tras el desastre de rotación de JLCPCB), EEPROM funcionando,
ESP32↔STM32 comunicándose… y un misterio: **el ADC AK5538 "capturaba" pero ignoraba por
completo sus entradas** — cortocircuitar los pines no cambiaba nada. Lo que en junio se había
leído como "hum sano" era en realidad los 8 canales raileados a ±full-scale.

## 2. El root cause que destrabó todo (2026-06-29 → validado 2026-07-02)

**El clock mode del AK5538 nunca se había configurado.** Los bits CKS3-0 (Control1, reg 0x02)
arrancan en 0000 = espera MCLK de 128×fs, pero la placa le da 512×fs (24.576MHz compartido
con los DACs). El modulador delta-sigma corría al oversampling equivocado → saturación
permanente → OVF=1 → ±FS sin importar la entrada. **Fix: R02=0x31** (CKS=0110 → 512fs Normal,
DIF=00, HPFE=1) escrito con RSTN=0. El OVF cayó a 0, el piso de ruido quedó en ~±60 LSB, y
una antena en c0 mostró tracking real de señal.

**Regla permanente que dejó:** en modo I2C los pines de strap CKS/TDM/DIF del AK5538/AK5578
se reusan para el bus y arrancan en 0 → clock mode, TDM y formato SIEMPRE se configuran por
registro, en cada boot. (Documentado en [ds10_bringup_findings.md](historico/ds10_bringup_findings.md)
y en [ak5538vn_ref.md](ak5538vn_ref.md).)

De ahí salió el **modo monitor** (ADC→DAC en vivo, +30dB) para *escuchar* lo que captura el
ADC — validado de oído, y después el test de contacto confirmó **los 8 canales de entrada
funcionando** (2026-07-02).

## 3. La doble revisión con Fable (2026-07-02)

Con el ADC vivo, se corrió una revisión profunda en dos frentes:

**Hardware (netlist DS11):** hallazgos priorizados en [ds11_upgrades.md](ds11_upgrades.md).
Los críticos: falta de bias de mic en J9 (resuelto por decisión: entradas de línea), polaridad
alternada pares/impares en las salidas + swap JSPK09/10 (decisión pendiente pre-fab), MP2315S
al límite con 24V (mitigación: alimentar a 19-20V), y verificaciones positivas importantes
(dominio único de MCLK, VREF según AKM, buses de control sin conflictos).

**Firmware (auditoría de main.c):** dos hallazgos accionables — la **carrera de
`delay_write_pos`** (los 4 fills TX leían el puntero por separado con el RX procesándose
después: inofensivo en passthrough, letal para la coherencia de fase con delays reales) y el
**heartbeat verboso bloqueante** (~15 underruns espurios por bloque). Ambos corregidos y
validados en hardware el mismo día (soak test de 9.5+ min, Err=0, jitter ~0µs). Y el plan
completo de fases hacia beamforming real: `beamforming_roadmap.md` (eliminado — todas las fases implementadas, ver `ds11_perf.md`).

## 4. Las fases, todas en un día (2026-07-02, rama `feature/beamforming`)

| Fase | Qué es | Commit | Estado |
|---|---|---|---|
| 0 | Fix carrera wp + channel walker (tono 1s/canal) + botón K2 cicla modos | `7fe165d` | walker+K2 **validados en hardware** |
| 1 | RX delay-and-sum real (8 mics → beam, steering por azimuth, 5 presets) | `7fe165d` | corre sin regresión; falta validación eléctrica |
| 2 | TX steering near-field desde geometría real (4×8 ladrillo 80/160/40mm), foco (x,y,z) | `7fe165d` | delays verificados vs cálculo Python independiente |
| 3 | Delays fraccionales Q8 (1/256 muestra) con interpolación lineal | `2c32f3b` | compila+corre; validación acústica pendiente |
| — | **AUTOFOCUS** (idea de Fernando): mic en el punto focal → chirp scan → TOF sub-muestra + polaridad medida por parlante → foco medido, no modelado | `2c32f3b` | implementado completo; necesita mic para probar |
| 4 | Control remoto total: CMD_SET_MODE/SET_BEAM/GET_STATUS + UI web embebida en el ESP32 | `5eac2b3` | ambos targets compilan; falta flashear y probar |

**El AUTOFOCUS merece subrayado:** convierte los dos hallazgos críticos de hardware
(polaridad, mapeo) en cosas *medibles y auto-corregibles* — el sistema barre un chirp por
parlante, correlaciona contra la referencia, y deriva delay + polaridad reales de cada canal,
absorbiendo errores de geometría, arnés y fabricación de una pasada. Baja el costo de la
imperfección física del array.

## 5. Unificación de repos (2026-07-03)

`DS-Firmware` (público) → **`DS-Project` (privado)**, absorbiendo el repo KiCad `DS` en
`hardware/DS/` vía `git subtree` con historial completo. El repo viejo `bergamifer/DS` quedó
privado + archivado como referencia. Backups: mirror clones en `~/backups/` + tags
`pre-unification` en ambos remotos. La carpeta local de trabajo no cambió
(`~/STM32CubeIDE/DS-Firmware`); la carpeta Dropbox del KiCad sigue siendo el lugar de edición
de KiCad — los cambios de hardware se traen con `git subtree pull` cuando haya commits nuevos.

## 6. Registro de decisiones (con racional)

| Decisión | Racional | Dónde está |
|---|---|---|
| Entradas de línea, sin bias de mic en J9 | Fuentes activas/autoalimentadas; el auto-bias del AK5538 alcanza para línea | ds11_upgrades.md |
| Protección por placa (no centralizada en la 1ª del daisy-chain) | Mínimo riesgo de diseño > ahorro de BOM; cada placa queda autónoma | conversación 2026-07-02 → esquemático |
| Cc=2.2µF film + Rs=470Ω por línea | HPF de pocos Hz contra la Zin del ADC; <10mA de falla (AKM) | **aplicado**: C294-309, R115-130 |
| R amortiguación MCLK→ADC = 33Ω en el punto de bifurcación | La R absorbe la reflexión cerca del split, no del chip receptor | **aplicado**: R131 |
| BOOT0 con botón se mantiene | Reflasheo por bootloader "por las dudas", costo ~0 | K3+R14, no tocar |
| Alimentar a 19-20V (no 24V) | MP2315S a 24V = margen cero vs abs max; gratis vs re-spin | DS11_PLANNING §8.3 |
| tx_delay_q8 separado de beam_config.delays | El ESP32 pisa delays[] en cada comando; aliasearlos destruiría el foco silenciosamente | main.c, roadmap Fase 2 |
| Delays TX en Q8 + interpolación lineal (no FIR polifásico) | Primer escalón; el FIR solo si una medición del patrón lo pide | roadmap Fase 3 |
| Geometría del array como tabla en el STM32 (no en ESP32) | Una sola fuente de verdad; el ESP32 manda solo el foco/ángulo | main.c |
| UI_ApplyState como única puerta de cambio de modo (K2 y remoto) | Cero estado duplicado entre botón físico y web | main.c, Fase 4 |
| Monorepo vía git subtree (no submodules) | Un `git pull` trae todo; sin detached HEADs; historial preservado | ds-project-unification (memoria) |
| Repo unificado privado | El diseño de PCB deja de ser público; credenciales ya estaban en secrets.h gitignoreado | GitHub |
| Checklist pre-fab ÚNICO en DS11_PLANNING §8 | Dos checklists = divergencia garantizada (el problema que motivó esta limpieza) | hardware/DS/DS11_PLANNING.md |

## 7. Limpieza de documentación (2026-07-03)

- `docs/historico/` ← ds9_pin_audit, ds9_pin_map, pcm1802_registers (ADC de DS9), Schematic_DS9.pdf
- Netlist: **una sola copia** en `hardware/DS/DS.txt` (export 2026-07-02, el más nuevo); borrado `docs/DS11_netlist.net`
- `DS_Hardware_Map_v2.md`: única copia en `docs/` (la más nueva); la de `hardware/DS/` es un puntero
- `WebServer.md` → `webserver/README.md` (junto al código que documenta)
- `memory_map.md` → `docs/`
- Checklist pre-producción de ds11_upgrades.md → movido/unificado en DS11_PLANNING §8
- Corregido MCLK 12.288→24.576MHz en hardware/DS/README.md (dato viejo, era el root cause del dual-rate)

## 8. Qué falta (en orden de valor)

1. **Validación eléctrica de Fase 1**: mismo tono en 2+ entradas → BEAMpk máximo en az=0, caída al steerear.
2. **Primer AUTOFOCUS con mic real** — valida TODA la cadena de medición.
3. **Flashear ESP32 + probar la UI web** (`http://<IP>/`) y el cambio de modos remoto.
4. **Decisión polaridad/swap pre-JLCPCB** (DS11_PLANNING §8.2) + resto del checklist §8.
5. Al llegar los DACs de Mouser: repoblar U1/U3/U4 → walker de 8→32 canales.
6. Al llegar la DS11: `WALKER_NUM_CH`/`CAL_NUM_CH`=32, PE12 a GPIO, channel walker + AUTOFOCUS sobre el array completo.

---

*Commits clave: `19f098d` (fix CKS + monitor), `89a3d1c` (carrera wp), `edbe30f` (netlist review docs), `7fe165d` (fases 0-2), `2c32f3b` (fase 3 + AUTOFOCUS), `5eac2b3` (fase 4), `976a705` (fusión hardware).*
