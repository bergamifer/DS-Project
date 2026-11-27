# Cuentas y simulaciones de la revisión de alimentación de la DS12

Acompañan a [`../DS12_REVISION_ALIMENTACION.md`](../DS12_REVISION_ALIMENTACION.md). Son Python puro,
sin dependencias. Cada script empieza con una **autoprueba** contra un resultado conocido (y una prueba
negativa: algo que *tiene* que fallar). Si la autoprueba no pasa, el script se detiene antes de dar números.

| script | qué responde | ítem |
|---|---|---|
| `s1_reparto.py` | con el 20 V puesto, cuánta corriente de `+5V_AUX` entrega la PC por D2 y cuánta `+5VA` por D19, según el VBUS | S1 |
| `s4_hotplug.py` | enchufar el USB con la placa **sin** 20 V: pico de corriente, sobretensión y caída del VBUS del hub | S4 |
| `s4f_verif.py` | enchufar un cable con VBUS vivo en **J8 con el 20 V puesto**: el golpe de ariete en `+5V_AUX` | S4.f |
| `s4f_mitig.py` | qué capacidad en `+5V_AUX` lo frena, y por qué un electrolítico no sirve | S4.f |
| `a1_ncaps.py` | cuántos `C45783` hacen falta, con su curva real de capacidad contra tensión (Samsung, en `C45783_dcbias.json`) y la tolerancia | A1 (ronda 2) |
| `a2_drenaje.py` | qué resistencia de drenaje necesita `+5V_AUX` para que la fuga de D23 no lo levante, con la Fig. 2 del SS34 | A2 (ronda 2) |

**Ronda 2** (7-oct): en [`ronda2/`](ronda2/) están el informe y los scripts de cada analista y de cada refutador
(`usb/`, `heredado/`, `completitud/`, `ruido/`, `refutacion/`). Algunos leen hojas de datos de `.ds12_audit/hojas/`, que es
local y no está en git; las fuentes están en `ronda2/CONTEXTO.md`.

**Medidas de banco** para cerrar lo que el papel no decide: [`medidas_ds11.md`](medidas_ds11.md).

```bash
cd hardware/DS/revision_alimentacion
python3 a1_ncaps.py        # tarda ~80 s: barre 9 cables × 3 C55 × 5 cantidades × 2 piezas
```

**Modelo del diodo.** Todos usan el SS34 de MDD (`C8678`) ajustado a la Fig. 3 de su hoja: 0,34 V a 0,1 A y 0,45 V
a 1 A. Por debajo de 0,1 A la figura no tiene datos y el modelo extrapola. Por eso `s1_reparto.py` barre el factor
de idealidad n, que es lo que gobierna esa zona.

**Lo que estos scripts no saben:** el layout (inductancias de pista, dónde cae cada capacitor) y el valor real de
cada pieza en la placa. Los números son de esquemático: se confirman midiendo.
