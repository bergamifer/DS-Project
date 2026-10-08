# Contexto común — ronda 2 de la revisión de alimentación de la DS12

Proyecto Domosónica: instalación sonora de ~500 parlantes. La placa DS12 lleva STM32H743, ESP32-C3-MINI-1, ADC AK5538VN,
4 DAC PCM1690, 32 amplificadores clase D PAM8302A, buck IC4 = AP63356 (`+5VA`, amplificadores) y buck U11 = AP63301 (`+3V3`).
Escribí en **castellano rioplatense**. El lector final es Fernando, arquitecto y no ingeniero electrónico: explicá el
**mecanismo**, no sólo el resultado.

## Reglas duras
- **Sólo lectura sobre el repo** `/Users/bergamifer/STM32CubeIDE/DS-Firmware`: no edites nada fuera de tu carpeta y no
  commitees. Trabajá en `.ds12_audit/r2/<tu carpeta>/`, que está en `.gitignore`. Las hojas de datos que bajes van a
  `.ds12_audit/hojas/` (agregá su `.txt` con `pdftotext -layout`).
- **NUNCA contactes las placas** (192.168.100.x), ni uses su API HTTP, ni enciendas nada. Encender los amplificadores es
  una decisión física de una persona.
- **No mires el PCB** (`hardware/DS/DS.kicad_pcb`). Lo que dependa del layout lo anotás aparte como «depende del PCB», sin desarrollarlo.
- Toda afirmación lleva **cómo se verificó**: comando sobre el netlist, documento + página/tabla + frase exacta, o script
  reproducible. Los scripts llevan **autoprueba** contra un resultado conocido y una **prueba negativa** (algo que tiene que fallar).
  Si algo no se puede decidir con lo disponible: **ABIERTO**, con la medida que lo cerraría. Nunca «confirmado de memoria».
- Los arreglos usan, si se puede, piezas que ya estén en `hardware/DS/jlcpcb/production_files/BOM-DS.csv` (columna LCSC).
  El stock no se analiza: ya está comprado.
- **Escribí tu informe final TAMBIÉN en `informe.md` dentro de tu carpeta**, y actualizalo a medida que avances (si te cortan,
  queda lo hecho).

## Archivos
- Netlist digerido de la DS12: `.ds12_audit/{componentes.tsv, redes.txt, pines_por_ic.txt}`. Viene de `hardware/DS/DS.net`
  (1-oct 15:29) y está verificado vigente al 7-oct. `redes.txt` = nombre · nº de nodos · ref.pin(función). `pines_por_ic.txt` =
  bloques `=== REF` sólo para U/IC/J/Q/L/FB; resistencias y capacitores se buscan en `redes.txt`. `componentes.tsv` =
  ref · value · footprint · LCSC · description. **El description a veces está mal copiado: mandan value, footprint y LCSC.**
- Netlist de la DS11 (la placa anterior, fabricada): `.ds12_audit/dig11/` (mismo formato).
- Hojas de datos en texto: `.ds12_audit/hojas/`:
  - `AP63356.txt` (DS41949 Rev 3-2), `AP63300.txt` (DS42002 Rev 3-2, AP63300/AP63301), `AP2125.txt` (DS38144);
  - `AK5538VN.txt` (015099878-E-02), `PCM1690.txt` (SBAS448B), `PAM8302A.txt` (DS41333 Rev 6-2);
  - `AO3401A.txt`, `stm32h743vi.txt` (DS12110 Rev 10), `esp32-c3-mini-1.txt`, `cap220uf.txt`;
  - `SS34_MDD.txt` y `ss34_p2-2.png`: render a 300 dpi de la p.2, con la Fig.2 (corriente inversa vs % de VR a 25/75/100 °C)
    y la Fig.3 (curva directa);
  - `usb20/usb_20.txt` (norma USB 2.0 completa), `usb20/Device Capacitance ECN.pdf`, `usb2_ecs.txt`.

  Los PDFs originales están en `hardware/DS/datasheets/`.
- Firmware actual (el de la DS11, con el que va a arrancar la DS12): `firmware/Core/Src/main.c`, `boot_main.c`,
  `firmware/Core/Inc/pin_definitions.h`, `firmware/DS-Firmware.ioc`, `firmware/Makefile`. ESP32: `firmware_esp32c3/`.
- Documentos (son AFIRMACIONES a verificar, no verdad):
  - `hardware/DS/DS12_ALIMENTACION.md`, `DS12_USB_STM32.md`;
  - `hardware/DS/DS12_REVISION_ALIMENTACION.md` (resultados de la ronda 1 y §8 de la ronda 2);
  - `hardware/DS/DS12_REVISION_ALIMENTACION_RONDA2.md` (el plan de esta ronda: leé la parte que te toca).
- Scripts de la revisión, con autoprueba: `hardware/DS/revision_alimentacion/` (`s1_reparto.py`, `s4_hotplug.py`,
  `s4f_verif.py`, `s4f_mitig.py`, `a1_ncaps.py`, `a2_drenaje.py`). Reusalos en vez de reescribir.

## Hechos ya establecidos (no los re-derives; sí podés usarlos)
- **Golpe de ariete en J8 (S4.f).** Con 20 V presentes, enchufar en J8 un cable con VBUS vivo lleva `+5V_AUX` a 6,6–7,1 V.
  Hoy `+5V_AUX` tiene sólo C57 (1 µF) + C59 (100 nF). La propuesta es agregar **3 × C45783** (Samsung CL21A226MAQNNNE, 22 µF,
  25 V, X5R, 0805; conserva 10,7 µF a 5 V) y **1 kΩ de drenaje** (`C11702`) de `+5V_AUX` a GND.
- **S1.** Con 20 V presentes, D2, D8 y D11 reparten la carga de `+5V_AUX` con D19. La PC entrega de 0 a toda la carga según
  su VBUS. Nada vuelve hacia la PC.
- **S2.** En modo sólo-USB `+5VA` = 0 V y `+3V3` está vivo: el firmware puede subir los 32 SD. Viene de la DS11.
- **Modo sólo-USB.** `VIN_B` (red `/DS_Power/IN`) queda en ≈3,84–3,98 V. El AP63301 arranca con ≤ 3,7 V (DS42002 p.4) y
  tiene modo de baja caída (p.18).
