# Convenciones del repo DS-Project

Lo que no se deduce leyendo el código. Si algo de acá se contradice con el repo, gana el
repo — y corregí este archivo.

## Idioma

**Todo en castellano rioplatense**: documentos, comentarios y mensajes de commit. El
proyecto lo lee Fernando, que es arquitecto, no ingeniero electrónico: explicar el
mecanismo, no sólo el resultado.

## Quién lee esto

Una instalación sonora de ~500 parlantes (**Domosónica**), no un producto. Las decisiones
se toman por lo que le pasa a la obra, y las restricciones reales son plata, tiempo y que
el equipo tiene que funcionar solo durante una muestra.

## Hardware — las tres reglas que costaron plata

1. **KiCad no recalcula los rellenos de zona al plotear.** Usa el cobre guardado dentro del
   `.kicad_pcb`. Ese mecanismo arruinó **dos tandas** de la DS11 y apareció una tercera vez
   latente. Antes de cualquier commit que toque el PCB: `tools/check_pcb.sh` (hay un hook,
   se instala con `--install`, una vez por copia del repo). Al exportar:
   `kicad-cli pcb export gerbers --check-zones`.
2. **El stock y los part numbers se verifican contra
   `hardware/DS/jlcpcb/production_files/BOM-DS.csv`, no contra el netlist.** Los dos no
   coinciden: para el 4,7 nF, `DS.txt` dice `C883564` y el BOM dice `C2961271`.
3. **La polaridad del conector de parlantes es `+ − − +`**, y el campo `pinfunction` del
   netlist MIENTE sobre esto: viene del símbolo y es igual en los 32 amplificadores. Lo que
   manda son los **nombres de red**, que se invierten entre A1 y A2.

Para preguntas de conectividad: `tools/netlist_digest.py` deja tres archivos grepables en
`.ds12_audit/`. `awk -F'\t' '$2==1' .ds12_audit/redes.txt` lista los pines al aire.

## Firmware

```bash
cd firmware && make -j4         # DS11 (por defecto) · make -j4 BOARD=ds12 para la DS12
make flash-stlink               # ⚠️ no anda en todas las placas: si no, make todo-cube
make ota OTA_HOST=192.168.100.77
```

- **La app vive en `0x08020000`**, no en `0x08000000`: hay un bootloader propio adelante.
- **El DMA no llega a DTCMRAM.** Los buffers de audio van en `RAM_D2`, `aligned(32)`.
- **`nano.specs` no soporta `%f`**: aritmética entera en los mensajes.
- Si `make` "pasa" un test que debería fallar, fijate que no haya salteado el rebuild
  (`rm -f build/main.o`). Ya dio un falso OK.

## Seguridad — física, no informática

- **Encender 32 amplificadores es una acción física y la decide una persona.** Ninguna
  herramienta los enciende sola.
- Con micrófonos enchufados, **los modos 0, 2–6 y 15 son acople**: llevan la entrada del
  ADC a los 32 parlantes, y no hay limitador ni AGC en ninguna parte del firmware.
- **`POST /reset` no reinicia el chip**: es un reset de fábrica y deja la placa en modo 0,
  que es el del acople.

## Método

**Cuando una herramienta produce un número, preguntarle también qué haría si no hubiera
nada que medir.** El analizador de fase produjo un informe completo a partir de −138 dBFS
de ruido del ADC con los amplificadores apagados. Por eso varias herramientas tienen
`--autoprueba` y pisos absolutos además de relativos: el veredicto de un umbral sin prueba
es una opinión.

Lo mismo para los hallazgos: verificar antes de afirmar, y decir **cómo** se verificó.

## Dónde está cada cosa

| | |
|---|---|
| Mapa del repo, estado y build | [README.md](README.md) |
| La placa: revisiones, docs de DS12, reglas de DRC | [hardware/DS/README.md](hardware/DS/README.md) |
| Herramientas de medición y de placa | [tools/README.md](tools/README.md) |
| Por qué se decidió cada cosa, mes a mes | `docs/reporte_2026-0{7,8,9}.md` |
| Operación en lenguaje llano | [docs/guia_operacion.md](docs/guia_operacion.md) |
| Los cortos de fábrica de la DS11 | [docs/ds11_errata.md](docs/ds11_errata.md) |
