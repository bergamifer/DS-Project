# Refutación 2 — U1, U2, U3, E1, A1', K1

Scripts con autoprueba y prueba negativa: `u_vbus.py` (salidas `u_vbus.out` y `j8_pa5.out`), `e1_en.py` y `r2_a1.py` (este importa `refutacion/r_a1.py`).

| | Veredicto |
|---|---|
| U1 | CONFIRMADO, **sólo junto con U2** |
| U2 | CONFIRMADO con correcciones |
| U3 | **REFUTADO** (la causa) |
| E1 | CONFIRMADO |
| A1' | **REFUTADO** |
| K1 | CONFIRMADO (gravedad baja) |

**U1.** Con U2, a 85 °C: 0,17 V con fuga típica y 0,40 V con la máxima. Con 5,1 k: 0,89 / 2,04 V; con 10 k: 1,73 / 3,95 V. **Sin U2**, el pull-up de D+ entra al VBUS por U6 y deja **1,29–1,50 V**: falla. Type-C R2.5 p.186: la fuente «shall apply a load to VBUS», pero no dice de cuánto, así que el 1 kΩ quita esa dependencia. No afecta a S1, al modo sólo-USB ni a la relación de PA5.

**U2.** La hoja del USBLC6 lo permite: su Fig. 15 pone el pin 5 en «+VCC» con 100 nF. No abre caminos nuevos:
- con +3V3 en 0, D+ está en 0;
- con D+ en 3,6 V, el diodo conduce menos de 1 µA;
- el ESP32 y el STM32 ya tienen sus propios diodos a +3V3.

Correcciones:
- **C31 es de 4,7 µF** (`C19666`), no de 10 µF. Los 0,26 V salen de 1,2 µC / 4,7 µF; con la pérdida por tensión continua son ≈0,5 V.
- En la prueba Back Voltage, **D+ sigue en 2,88–3,07 V**, con límite de 0,40 V.
- Se pierde el clamp de U9 en J8. El peor caso de A1' ya lo excluía, pero desaparece el margen de hoy (5,80–5,85 contra 5,87 V).

**U3.** Lo confirmado: el pull-up está prendido por defecto (TRM v1.4: «pull-up value of GPIO19 is 1 by default») y no hay entrada de VBUS (ni en el TRM ni en la red de U5). Lo que no se sostiene: el camino por D+ sale de +3V3 y pierde la caída del diodo, así que **no puede llevar el VBUS a más de ≈3,0 V**. Sin descarga, la fuga de D2 sola da 4,80 V; el ESP32 solo da ≤ 3,01 V. Que 100 kΩ no alcanza es cierto, por las dos causas: 2,77 V a 25 °C hoy y 4,79 V a 85 °C con U2. La medición M1 lo resuelve: ≈4,8 V es D2, ≈3 V es el ESP32.

**E1.** La hoja del MINI-1 (v1.6, p.25) dice «R = 10 kΩ and C = 1 µF». Rampa máxima, criterio estricto / laxo: 0,66 / 4,56 ms con 0,1 µF y 7,08 / 46 ms con 1 µF. Con C28 un 20 % bajo, el criterio laxo ya falla a 4 ms. Pulsar K1 descarga 5,4 µJ; al soltarlo, EN tarda 13,9 ms en subir. **ABIERTO**: medir la rampa real de +3V3 y EN con osciloscopio.

**A1'.** Los números citados (6,004 / 5,803 / 5,670 V) son la fila **sin** ×0,9. **Con** ×0,9 da **6,087 / 5,873 / 5,730 V**: con 4 se pasa de 5,8 V y hacen falta 5. Sin el ×0,9, 5,803 V también pasa. Con VBUS de 5,5 V da 6,29 / 6,05 / 5,88 V; todos quedan debajo de los 6,5 V absolutos. Alternativa más barata: C55 de **2,2 µF** (`C23630`, ya en el BOM) con 3 × C45783 da **5,758 V**. **ABIERTO** si cumple la ECN (≥ 1,0 µF a 5,25 V): falta la curva de pérdida por tensión continua del C23630.

**K1.** AK5538 p.6: TDMIN «pulled down by 100 kΩ internally». p.7, en modo PCM: «TDMIN, TEST: Connect to DVSS». El modo es PCM porque DP está a GND. En la DS11 estaba al aire y anda gracias a esa resistencia interna; conectarlo a GND no cuesta nada.

## Problemas nuevos
1. **J3 no aprueba Back Voltage ni con U2.** Habría que leer VBUS con GPIO2 o GPIO3, que están libres, y apagar el pull-up por firmware. El ROM igual lo deja prendido mientras arranca. El daño práctico es chico: ≈0,2 mA hacia una PC apagada.
2. **Hoy PA5 cree ver VBUS sin cable.** La fuga de D8 contra 162 kΩ da 2,4–2,96 V desde ~60 °C, o a 25 °C con fuga máxima. Con U1 baja a ≤ 0,25 V.
3. **U1 viola la corriente de suspensión** (USB 2.0 §7.2.3: 0,5 / 2,5 mA; toma 4,4–5,5 mA). Ninguna resistencia sola cumple las dos cosas: la suspensión pide ≥ 2,1 kΩ y vSafe0V en caliente pide ≤ 1,8 kΩ. S1 ya la viola, así que es aceptable.
4. J8 también va a depender de U2 si un firmware futuro prende el pull-up del STM32 sin mirar PA5.
5. El BOM llama al `C45783` «22µF, 16V», pero A1' supone 25 V. Hay que verificarlo.
6. K1 dice «BOOT ESP32» pero está conectado a EN: es un reset.
