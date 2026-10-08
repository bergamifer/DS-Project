# Ronda 2 — S11 (ruido de +5VA hacia el ADC y los DAC) y S10 (apagado al retirar los 20 V)

Analista «ruido», 2026-10-07. Autoprueba y prueba negativa: `s11_ruido.py` (7/7; detecta R de VREFH corrompida) y
`s10_apagado.py` (6/6; detecta C corrompida).

## Tabla

| ítem | estimación | sev. |
|---|---|---|
| S11a buck A | ΔIL 1,23 App × 3,8 mΩ → **1,9 mV pico a 450 kHz ±6 %** | 🟢 |
| S11b 32 PAM | 0,6 mV/tono (0,3 W/canal), 3 mV (2,5 W); 3,6–17 mV rms en 200–300 kHz | 🟢 |
| S11b batidos PAM-PAM | en una red lineal **no existen**; la única alinealidad antes del LDO es D19 (detector): ≤ −159 dBFS | 🟢 |
| S11c FB69 hoy (C57+C59) | resuena a 172 kHz; **Q 1,1 con 43 mA, 3,8–4,3 con la expansión cargada (0,5–1 A): +11 a +14 dB, +6 dB a 200 kHz** | 🟡 |
| S11c FB69 con 3×C45783 | f0 28 kHz, Q ≤ 0,7 (peor +0,65 dB a 15 kHz); **−39 dB a 250 kHz** | 🟢 |
| S11d llegada al ADC | central ≤ −179 dBFS. Peor caso combinado hoy: −138 dBFS/Hz (piso −154); **con 3×22 µF: ≤ −172 dBFS/Hz, tonos ≤ −180 dBFS** | 🟢 |
| S11 en banda (programa) | 0,1–0,5 V de rizado audible en +5VA → AVDD ≤ −136 dBFS; VREFH ≤ −110 dBc (sólo con señal) | 🟢 |
| 2. +5VA → FB3 → PCM1690 | FB3: −23 dB a 250 kHz. **No hay PSRR publicado.** Con 0,5 V a 600 Hz: −69 dB si la referencia es VCOM y **−20 dB si es VCC1 directo** | 🟡 ABIERTO |
| S10 diodo de cuerpo de IC4 | **no conduce**: máx(+5VA − VIN_LOCAL) ≤ −0,17 V en todo caso realista | 🟢 REFUTADO |
| S10 sostén de +3V3 | lo da VIN_LOCAL: **+3V3 sobrevive a +5VA 4,6 ms (amps OFF), 5,2 (reposo), 7,0 (señal), 8,3 (bus)** | 🟢 |
| S10 reencendidos | IC4: 0 (histéresis 3,2 V). U11: 0; VIN_B rebota a 3,30–3,38 V contra 3,5 V típ. (sin mínimo publicado) | 🟢/ABIERTO |
| S10 Q1 al revés | sí, conduce mientras VIN_LOCAL > 0,5–1,3 V; la descarga se reparte con el bus y no entra nada de +5VA | 🟢 |
| S10 SD por encima del VDD de los PAM | con los amps encendidos: **1,1 ms (reposo) a 6,4 ms (con señal)** con el MCU vivo (es S2, en cada apagado) | 🟡 |
| S10 pop | con señal, +5VA pasa de 4,5 a 2,1 V en 0,75 ms con SD alto; DS41333 p. 8 pide SD bajo antes | 🟡 firmware |

## Mecanismo

**S11.** El ADC tiene dos puertas. Por **AVDD** el rizado entra como una señal: el decimador lo frena ≥85 dB (AK5538 p. 10)
y después se pliega a la banda. Lo acoté con PSRR de AVDD = 0 dB, porque la hoja da 60 dB sólo a 1 kHz (Nota 11). **VREFH** es
la regla con la que el ADC mide (Nota 10: FS = 0,56·VREFH). Su rizado multiplica: sin señal no da nada, pero en alta frecuencia
baja a la banda el ruido de cuantización del modulador. La f_mod no está publicada: barrí 64/128/256 fs y acoté S_q en
−60 dBFS/Hz, como si no hubiera noise shaping. Hay tres filtros en cadena: FB69 con su capacitor; el AP2125 (DS38144 p. 16,
digitalizado; sin dato por encima de 100 kHz); y el RC de VREFH (AKM p. 68), que da sólo −18 a −32 dB a 250 kHz porque lo limita
la ESR del electrolítico. Hoy el primero casi no filtra: 0,9 µH con 1 µF forman un LC a 172 kHz, y con mucha corriente por D19
su resistencia dinámica (nVT/I) deja de amortiguarlo.

**S10.** VIN_LOCAL cae de 19,7 a 11,1 V (VOFF del EN) en 2 ms con señal y 8,6 ms con los amps apagados, y ahí IC4 se apaga.
Desde entonces +5VA (1,2 mF) alimenta el DAC (4 × 74 mA: el MCLK nunca para), los PAM y +5V_AUX, mientras que VIN_LOCAL
(≈220 µF) sólo alimenta a U11 (2,1 W). +5VA pierde la carrera y el diodo de cuerpo nunca se polariza; D23 tampoco conduce.
Sólo con el DAC sin consumo (cota) conducirían: 2–4 mA el diodo y 0,5 A D23, que darían +1,5 ms de sostén. Inocuo.

## Acción recomendada (piezas del BOM)

1. **Hardware: ninguna pieza nueva.** Mantener los **3 × C45783 + 1 kΩ (C11702)** de A1/A2: además de S4.f, bajan la
   resonancia de FB69 a 28 kHz y suman ≈35 dB a 250 kHz, lo que cierra el peor caso de S11. No tocar VREFH (circuito de AKM).
2. **Firmware (S10/S2):** `SD` en bajo al caer +5VA mediante el **watchdog analógico del ADC sobre PC1** (≈2,25 V en el pin,
   4,5 V en +5VA). Leerlo por sondeo a 1 kHz no alcanza: con señal, de 4,58 V al UVLO de los PAM pasan 0,78 ms. PG avisa
   220 µs después del 90 %, pero sólo si funciona con EN en bajo (ABIERTO).

## Depende del PCB (sin desarrollar)
- Inductancia de pistas entre los PAM, el bulk y la toma de FB69 (z(+5VA) se modeló concentrada).
- Acople magnético de las corrientes de los PAM hacia AIN y VREFH; retorno de masa; dónde va el 0,1 µF de VREFH.

## ABIERTO: medidas que lo cierran
- **f_mod y PSRR en alta frecuencia del AK5538:** en una DS11 (cadena casi igual: +5VA → FB2 → D9 → +5V → U10), con las
  entradas 0–3 vacías y en un modo que no lleve el ADC a los parlantes, comparar el espectro con los amps OFF y ON. Criterio:
  ningún tono por encima de −140 dBFS por bin y un piso (hoy −113,4 dBFS) que no suba más de 0,5 dB. **Encender los amps lo decide Fernando.**
- **PSRR del PCM1690:** tono por un canal con los demás amps sonando; buscar productos en 2f y 3f.
- PG del AP63356 con EN en bajo; ESR del RVT1A101M0505 (LCR a 100 kHz); umbral mínimo de subida del UVLO del AP63301.

## Problemas nuevos
1. **Resonancia FB69/C57 hoy** (Q hasta 4,3 con la expansión cargada): se va con A1.
2. **Corto en el bus de +20 V:** Q1 conduce al revés hasta ≈1 V; después el diodo de cuerpo de IC4 descarga +5VA hacia el
   corto con Q1 en zona lineal (≈1 A durante algunos ms). DS41949 no especifica ese diodo: ABIERTO, riesgo bajo.
3. **Al apagar, VREFH queda ≈0,15 V sobre AVDD** (los 100 µF van detrás de los 20 Ω). La Nota 8 pide ≤ 0,1 V, pero es una
   condición de operación, no un máximo absoluto (el límite es IIN 10 mA), y pasa igual con el circuito de AKM: 🟢.

## Archivos
`.ds12_audit/r2/ruido/`: `informe.md`, `s11_ruido.py`, `s10_apagado.py`, `salida_s11.txt`, `salida_s10.txt`, `red.py`.
`.ds12_audit/hojas/`: `C21286.pdf/.txt`, `c21286_p24-24.png`, `ap2125_p16-16.png`, `ap63356_p10-10.png`, `ap63356_p11-11.png`.
