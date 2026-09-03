# Datos crudos de la sesión del 3 de septiembre de 2026

Primera medición con micrófonos. Sala de 3 × 2,5 m, placa `DS_1E77C4`.
Las conclusiones están en `docs/banco_dos_microfonos.md` §6; acá están los
números para que cualquiera pueda rehacerlas o contradecirlas.

| archivo | qué es | cómo leerlo |
|---|---|---|
| `cal1.json` | AUTOFOCUS, micrófono izquierdo (entonces en c0) | `calfase.py docs/datos/cal1.json --pares --mic 1` |
| `cal2.json` | AUTOFOCUS, **los dos** micrófonos (c0 y c1) | `calfase.py docs/datos/cal2.json --pares-fijo --sep 250` |
| `fase1.json` | barrido de fase, micrófonos en c0 y **c7** | anterior al corrimiento de entradas (§6.4) |
| `fase2.json` | barrido de fase, micrófonos en c0 y c1, geometría corregida | `fase.py docs/datos/fase2.json --pares --ch 1 --mic 14 -46 954` |
| `perfil_ok.json` | perfil del haz a 2 kHz, geometría corregida | `perfil.py docs/datos/perfil_ok.json --canales 0 1 --sep 247` |
| `ident.json` | primer perfil, **antes** de corregir la geometría | sirve de contraejemplo: el haz mal formado |

Posiciones de los micrófonos, ajustadas desde los tiempos de vuelo del chirp
(residuo 3,9 µs = 1,3 mm):

```
c0  (−233, −52, 948) mm    el de la izquierda
c1  (+14,  −46, 954) mm    el del centro
separación 247 mm — la cinta métrica decía 250
```

Y el mapeo del arnés, que hay que pasarle a todo: **canal `k` → cono `k^1`**.
