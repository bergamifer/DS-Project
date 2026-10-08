# Matriz de cobertura de la alimentación — DS12 (ronda 2, A5)

Generada por `a5_matriz.py` (autoprueba: 8 positivas + 4 negativas) sobre el digest del `DS.net` del 1-oct.
Estados: **verificado antes** (rondas anteriores, no se rehizo) · **OK ahora** · **PROBLEMA** · **ABIERTO**.

## 0. Verificado antes (no se rehizo)

| chip / pieza | qué | cuándo |
|---|---|---|
| IC4 AP63356 | pines | 29-sep / ronda 1 |
| U11 AP63301 | pines | 29-sep / ronda 1 |
| U10 AP2125 | pines | 29-sep |
| Q1 AO3401A | pines | 29-sep |
| IC1 MAX809S | pines | 29-sep |
| D9 SMBJ20A | pines | 29-sep |
| D3 BAT54C | pines | 29-sep |
| D2, D8, D11, D19, D20, D23 (SS34) | OR de alimentación | ronda 1 (S1) |
| J8, U9, FL1 | USB del STM32 | ronda 1 |
| J3 | USB del ESP32 | ronda 1 |
| F1–F7 | fusibles | ronda 1 |
| S1, S2, S4 | reparto, SD, enchufe en caliente | ronda 1 |

## 1. Pines de alimentación contra la hoja de cada chip

| chip | pin | nombre en la hoja | red en el netlist | estado | nota | fuente |
|---|---|---|---|---|---|---|
| IC3 | 1 | AVSS1 | GND | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 2 | AVDD1 3,0–3,6 V | +3.3VA | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 5 | VREFL2 | GND | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 6 | VREFH2 | Net-(IC3-VREFH2) | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 11 | VREFH3 | Net-(IC3-VREFH3) | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 12 | VREFL3 | GND | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 15 | AVDD2 3,0–3,6 V | +3.3VA | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 16 | AVSS2 | GND | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 19 | VREFH4 | Net-(IC3-VREFH4) | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 20 | VREFL4 | GND | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 25 | TVDD 3,0–3,6 V (LDOE=H) | +3V3 | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 26 | DVSS | GND | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 27 | VDD18 (salida del LDO, sólo capacitor) | Net-(IC3-VDD18) | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 57 | LDOE = H (LDO interno encendido) | +3V3 | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 61 | VREFL1 | GND | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 62 | VREFH1 | Net-(IC3-VREFH1) | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| IC3 | 65 | pad expuesto (a VSS para Ta>70 °C, p.7) | GND | OK ahora |  | AK5538VN 015099878-E-02 p.4–6 «Pin Functions» |
| U1–U4 (PCM1690) | 12 | VDD +3,3 V | +3V3 | OK ahora |  | PCM1690 SBAS448B p.4–5 «Pin Functions» |
| U1–U4 (PCM1690) | 13 | DGND | GND | OK ahora |  | PCM1690 SBAS448B p.4–5 «Pin Functions» |
| U1–U4 (PCM1690) | 25 | VCC1 +5 V | +5VA_DAC | OK ahora |  | PCM1690 SBAS448B p.4–5 «Pin Functions» |
| U1–U4 (PCM1690) | 26 | VCOM | red propia de cada chip | OK ahora | red propia con C11; red propia con C17; red propia con C23; red propia con C5 | PCM1690 SBAS448B p.4–5 «Pin Functions» |
| U1–U4 (PCM1690) | 27 | AGND1 | GND | OK ahora |  | PCM1690 SBAS448B p.4–5 «Pin Functions» |
| U1–U4 (PCM1690) | 46 | AGND2 | GND | OK ahora |  | PCM1690 SBAS448B p.4–5 «Pin Functions» |
| U1–U4 (PCM1690) | 47 | VCC2 +5 V | +5VA_DAC | OK ahora |  | PCM1690 SBAS448B p.4–5 «Pin Functions» |
| U1–U4 (PCM1690) | 49 | PowerPAD | GND | OK ahora |  | PCM1690 SBAS448B p.4–5 «Pin Functions» |
| U7 | 6 | VBAT (sin batería: a VDD, DS12110 §3.19) | +3V3 | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 10 | VSS | GND | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 11 | VDD | +3V3 | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 14 | NRST | /stm32_custom/NRST | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 19 | VSSA | GND | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 20 | VREF+ | /stm32_custom/VREF+ | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 21 | VDDA | /stm32_custom/VREF+ | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 26 | VSS | GND | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 27 | VDD | +3V3 | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 48 | VCAP | Net-(C37-Pad1) | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 49 | VSS | GND | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 50 | VDD | +3V3 | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 73 | VCAP | Net-(C39-Pad1) | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 74 | VSS | GND | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 75 | VDD | +3V3 | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 99 | VSS | GND | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U7 | 100 | VDD | +3V3 | OK ahora |  | DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55 |
| U5 | 1 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 2 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 3 | 3V3 | +3V3 | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 8 | EN (no dejar flotante) | /esp32-mini/EN{slash}CHIP_PU | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 11 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 14 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 36 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 37 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 38 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 39 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 40 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 41 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 42 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 43 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 44 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 45 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 46 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 47 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 48 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 49 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 50 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 51 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 52 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U5 | 53 | GND | GND | OK ahora |  | ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25 |
| U12–U43 (32 PAM) | 6 | VDD 2,0–5,5 V | +5VA | OK ahora |  | PAM8302A DS41333 Rev6-2 «Pin Description» p.2 |
| U12–U43 (32 PAM) | 7 | GND | GND | OK ahora |  | PAM8302A DS41333 Rev6-2 «Pin Description» p.2 |
| U8 | 8 | VCC 1,8–5,5 V | +3V3 | OK ahora |  | CAT24C256 Rev14 p.1 «Pin Configuration» (TSSOP) |
| U8 | 4 | VSS | GND | OK ahora |  | CAT24C256 Rev14 p.1 «Pin Configuration» (TSSOP) |
| D10, D12–D18 (PRTR5V0U2X) | 4 | VCC (cátodos del puente) | +3.3VA | OK ahora |  | UMW PRTR5V0U2X p.1 «Pinning information»: 4 = cátodos (VCC), 1 = ánodo del zener (GND) |
| D10, D12–D18 (PRTR5V0U2X) | 1 | GND | GND | OK ahora |  | UMW PRTR5V0U2X p.1 «Pinning information»: 4 = cátodos (VCC), 1 = ánodo del zener (GND) |
| U6 | 5 | VBUS | /esp32-mini/VBUS | OK ahora |  | USBLC6-2 (ST) p.1 Fig.1: 2 = GND, 5 = VBUS |
| U9 | 5 | VBUS | /stm32_custom/VBUS | verificado antes |  | USBLC6-2 (ST) p.1 Fig.1: 2 = GND, 5 = VBUS |
| U6 | 2 | GND | GND | OK ahora |  | USBLC6-2 (ST) p.1 Fig.1: 2 = GND, 5 = VBUS |
| U9 | 2 | GND | GND | OK ahora |  | USBLC6-2 (ST) p.1 Fig.1: 2 = GND, 5 = VBUS |

## 2. Desacople contra lo que pide cada hoja (capacitores de la misma hoja del esquemático)

| chip | red | hay | pide | estado | falta | fuente |
|---|---|---|---|---|---|---|
| IC3 | +3.3VA | C259=10u cer C260=0.1uF C265=10u cer C266=0.1uF | 10 µF; 0,1 µF | OK ahora |  | AK5538 Fig.75 p.67: AVDD1 y AVDD2, 0,1 µF + 10 µF cada uno |
| IC3 | +3V3 | C269=0.1uF C272=10u cer | 10 µF; 0,1 µF | OK ahora |  | AK5538 Fig.75 p.67: TVDD 0,1 µF + 10 µF |
| IC3 | Net-(IC3-VDD18) | C268=0.1uF C271=4.7u | 4,7 µF; 0,1 µF | OK ahora |  | AK5538 Fig.75 p.67: VDD18 0,1 µF + 4,7 µF |
| IC3 | Net-(IC3-VREFH1) | C257=100uF C258=0.1uF | 100 µF electrolítico; 0,1 µF cerámico | OK ahora |  | AK5538 p.68 §2: 20 Ω desde AVDD + 100 µF ∥ 0,1 µF |
| IC3 | Net-(IC3-VREFH2) | C261=0.1uF C262=100uF | 100 µF electrolítico; 0,1 µF cerámico | OK ahora |  | AK5538 p.68 §2: 20 Ω desde AVDD + 100 µF ∥ 0,1 µF |
| IC3 | Net-(IC3-VREFH3) | C263=0.1uF C264=100uF | 100 µF electrolítico; 0,1 µF cerámico | OK ahora |  | AK5538 p.68 §2: 20 Ω desde AVDD + 100 µF ∥ 0,1 µF |
| IC3 | Net-(IC3-VREFH4) | C267=0.1uF C270=100uF | 100 µF electrolítico; 0,1 µF cerámico | OK ahora |  | AK5538 p.68 §2: 20 Ω desde AVDD + 100 µF ∥ 0,1 µF |
| U1 | +3V3 | C1=1u C6=10u EL | 10 µF electrolítico; 1 µF cerámico | OK ahora |  | PCM1690 Fig.41 p.37 + §9: VDD 1 µF + 10 µF |
| U2 | +3V3 | C7=1u C12=10u EL | 10 µF electrolítico; 1 µF cerámico | OK ahora |  | PCM1690 Fig.41 p.37 + §9: VDD 1 µF + 10 µF |
| U3 | +3V3 | C13=1u C18=10u EL | 10 µF electrolítico; 1 µF cerámico | OK ahora |  | PCM1690 Fig.41 p.37 + §9: VDD 1 µF + 10 µF |
| U4 | +3V3 | C19=1u C24=10u EL | 10 µF electrolítico; 1 µF cerámico | OK ahora |  | PCM1690 Fig.41 p.37 + §9: VDD 1 µF + 10 µF |
| U1 | +5VA_DAC | C2=10u EL C3=1u C4=1u | 10 µF electrolítico; 1 µF cerámico ×2 (VCC1, VCC2) | OK ahora |  | PCM1690 Fig.41 p.37 + §9 |
| U2 | +5VA_DAC | C8=10u EL C9=1u C10=1u | 10 µF electrolítico; 1 µF cerámico ×2 (VCC1, VCC2) | OK ahora |  | PCM1690 Fig.41 p.37 + §9 |
| U3 | +5VA_DAC | C14=10u EL C15=1u C16=1u | 10 µF electrolítico; 1 µF cerámico ×2 (VCC1, VCC2) | OK ahora |  | PCM1690 Fig.41 p.37 + §9 |
| U4 | +5VA_DAC | C20=10u EL C21=1u C22=1u | 10 µF electrolítico; 1 µF cerámico ×2 (VCC1, VCC2) | OK ahora |  | PCM1690 Fig.41 p.37 + §9 |
| U7 | +3V3 | C32=4.7u C33=0.1uF C34=0.1uF C38=0.1uF C40=0.1uF C41=0.1uF C42=0.1uF C47=0.1uF | 4,7 µF; 100 nF × N=5 pines VDD | OK ahora |  | DS12110 Fig.15 p.103 (±20 % tolerado) |
| U7 | /stm32_custom/VREF+ | C35=1u C43=0.1uF C44=10u cer | 1 µF ×2 (VDDA y VREF+); 100 nF ×2 | PROBLEMA | 100 nF ×2: hay 1 de 2 | DS12110 Fig.15 p.103: VDDA 100 nF+1 µF; VREF+ 100 nF+1 µF |
| U7 | Net-(C37-Pad1) | C37=2.2u | 2,2 µF ±20 %, ESR<100 mΩ | OK ahora |  | DS12110 Tabla 25 p.107 |
| U7 | Net-(C39-Pad1) | C39=2.2u | 2,2 µF ±20 %, ESR<100 mΩ | OK ahora |  | DS12110 Tabla 25 p.107 |
| U5 | +3V3 | C26=10u cer C27=0.1uF | 10 µF; 0,1 µF | OK ahora |  | ESP32-C3-MINI-1 Fig.7 p.25 (C1, C2) |
| U5 | /esp32-mini/EN{slash}CHIP_PU | C28=0.1uF | 1 µF (RC con 10 kΩ) | PROBLEMA | 1 µF (RC con 10 kΩ): hay 0 de 1 | ESP32-C3-MINI-1 p.25: «R = 10 kΩ and C = 1 µF» |
| U12+U13 | +5VA | C89=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |
| U30+U31 | +5VA | C188=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |
| U32+U33 | +5VA | C199=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |
| U34+U35 | +5VA | C210=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |
| U36+U37 | +5VA | C221=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |
| U38+U39 | +5VA | C232=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |
| U40+U41 | +5VA | C243=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |
| U42+U43 | +5VA | C254=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |
| U14+U15 | +5VA | C100=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |
| U16+U17 | +5VA | C111=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |
| U18+U19 | +5VA | C122=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |
| U20+U21 | +5VA | C133=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |
| U22+U23 | +5VA | C144=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |
| U24+U25 | +5VA | C155=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |
| U26+U27 | +5VA | C166=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |
| U28+U29 | +5VA | C177=1u | 1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca | PROBLEMA | 1 de 1 µF para 2 chips | PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)» |

## 3. Tensión nominal de cada capacitor de alimentación (LCSC) contra el máximo de su red

Margen = 1 − V_red / V_nominal. Umbral 25 %. «est» = estacionario, «tr» = transitorio.

| C | value | LCSC | redes | V nom | V est / margen | V tr / margen | dieléctrico, tamaño | estado | nota |
|---|---|---|---|---|---|---|---|---|---|
| C1 | 1u | C15849 | +3V3/GND | 50 | 3.36 / 93% | 3.6 / 93% | X5R 0603 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C2 | 10u EL | C134745 | +5VA_DAC/GND | 25 | 5.23 / 79% | 5.75 / 77% | Al D4x5.7 | OK | igual que +5VA (FB3) |
| C3 | 1u | C15849 | +5VA_DAC/GND | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | igual que +5VA (FB3) |
| C4 | 1u | C15849 | +5VA_DAC/GND | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | igual que +5VA (FB3) |
| C5 | 10u EL | C134745 | Net-(U1-VCOM)/GND | 25 | 2.75 / 89% | 2.75 / 89% | Al D4x5.7 | OK | 0,5 × VCC1 (PCM1690) |
| C6 | 10u EL | C134745 | +3V3/GND | 25 | 3.36 / 87% | 3.6 / 86% | Al D4x5.7 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C7 | 1u | C15849 | +3V3/GND | 50 | 3.36 / 93% | 3.6 / 93% | X5R 0603 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C8 | 10u EL | C134745 | +5VA_DAC/GND | 25 | 5.23 / 79% | 5.75 / 77% | Al D4x5.7 | OK | igual que +5VA (FB3) |
| C9 | 1u | C15849 | +5VA_DAC/GND | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | igual que +5VA (FB3) |
| C10 | 1u | C15849 | +5VA_DAC/GND | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | igual que +5VA (FB3) |
| C11 | 10u EL | C134745 | Net-(U2-VCOM)/GND | 25 | 2.75 / 89% | 2.75 / 89% | Al D4x5.7 | OK | 0,5 × VCC1 (PCM1690) |
| C12 | 10u EL | C134745 | +3V3/GND | 25 | 3.36 / 87% | 3.6 / 86% | Al D4x5.7 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C13 | 1u | C15849 | +3V3/GND | 50 | 3.36 / 93% | 3.6 / 93% | X5R 0603 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C14 | 10u EL | C134745 | +5VA_DAC/GND | 25 | 5.23 / 79% | 5.75 / 77% | Al D4x5.7 | OK | igual que +5VA (FB3) |
| C15 | 1u | C15849 | +5VA_DAC/GND | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | igual que +5VA (FB3) |
| C16 | 1u | C15849 | +5VA_DAC/GND | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | igual que +5VA (FB3) |
| C17 | 10u EL | C134745 | Net-(U3-VCOM)/GND | 25 | 2.75 / 89% | 2.75 / 89% | Al D4x5.7 | OK | 0,5 × VCC1 (PCM1690) |
| C18 | 10u EL | C134745 | +3V3/GND | 25 | 3.36 / 87% | 3.6 / 86% | Al D4x5.7 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C19 | 1u | C15849 | +3V3/GND | 50 | 3.36 / 93% | 3.6 / 93% | X5R 0603 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C20 | 10u EL | C134745 | +5VA_DAC/GND | 25 | 5.23 / 79% | 5.75 / 77% | Al D4x5.7 | OK | igual que +5VA (FB3) |
| C21 | 1u | C15849 | +5VA_DAC/GND | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | igual que +5VA (FB3) |
| C22 | 1u | C15849 | +5VA_DAC/GND | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | igual que +5VA (FB3) |
| C23 | 10u EL | C134745 | Net-(U4-VCOM)/GND | 25 | 2.75 / 89% | 2.75 / 89% | Al D4x5.7 | OK | 0,5 × VCC1 (PCM1690) |
| C24 | 10u EL | C134745 | +3V3/GND | 25 | 3.36 / 87% | 3.6 / 86% | Al D4x5.7 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C26 | 10u cer | C1691 | GND/+3V3 | 6.3 | 3.36 / 47% | 3.6 / 43% | X5R 0603 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C27 | 0.1uF | C1525 | +3V3/GND | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C28 | 0.1uF | C1525 | /esp32-mini/EN{slash}CHIP_PU/GND | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | +3V3 por R8 |
| C31 | 4.7u | C19666 | /esp32-mini/VBUS/GND | 16 | 5.5 / 66% | 8.5 / 47% | X5R 0603 | OK | VBUS USB 5,5 V; enchufe en caliente simulado hasta 8,5 V |
| C32 | 4.7u | C19666 | +3V3/GND | 16 | 3.36 / 79% | 3.6 / 78% | X5R 0603 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C33 | 0.1uF | C1525 | +3V3/GND | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C34 | 0.1uF | C1525 | +3V3/GND | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C35 | 1u | C15849 | /stm32_custom/VREF+/GND | 50 | 3.36 / 93% | 3.6 / 93% | X5R 0603 | OK | +3V3 por FB1 |
| C36 | 0.1uF | C1525 | GND//stm32_custom/NRST | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | +3V3 |
| C37 | 2.2u | C23630 | Net-(C37-Pad1)/GND | 16 | 1.35 / 92% | 1.35 / 92% | X5R 0603 | OK | VCAP (VCORE máx 1,35 V) |
| C38 | 0.1uF | C1525 | +3V3/GND | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C39 | 2.2u | C23630 | Net-(C39-Pad1)/GND | 16 | 1.35 / 92% | 1.35 / 92% | X5R 0603 | OK | VCAP (VCORE máx 1,35 V) |
| C40 | 0.1uF | C1525 | +3V3/GND | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C41 | 0.1uF | C1525 | +3V3/GND | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C42 | 0.1uF | C1525 | GND/+3V3 | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C43 | 0.1uF | C1525 | /stm32_custom/VREF+/GND | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | +3V3 por FB1 |
| C44 | 10u cer | C1691 | /stm32_custom/VREF+/GND | 6.3 | 3.36 / 47% | 3.6 / 43% | X5R 0603 | OK | +3V3 por FB1 |
| C47 | 0.1uF | C1525 | GND/+3V3 | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C52 | 0.1uF | C1525 | GND//stm32_custom/NRST | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | +3V3 |
| C55 | 4.7u | C19666 | /stm32_custom/VBUS/GND | 16 | 5.5 / 66% | 8.5 / 47% | X5R 0603 | OK | VBUS USB 5,5 V; enchufe en caliente simulado hasta 8,5 V |
| C57 | 1u | C15849 | +5V_AUX/GND | 50 | 5.23 / 90% | 8.0 / 84% | X5R 0603 | OK | ≤5,23 V; golpe de ariete J8 hoy 7–8 V (≤5,8 V con 3×C45783) |
| C59 | 0.1uF | C1525 | GND/+5V_AUX | 16 | 5.23 / 67% | 8.0 / 50% | X7R 0402 | OK | ≤5,23 V; golpe de ariete J8 hoy 7–8 V (≤5,8 V con 3×C45783) |
| C60 | 22uF 50V | C72505 | /DS_Power/IN/GND | 50 | 20.0 / 60% | 32.0 / 36% | Al D6.3x5.4 | OK | ≈20 V; transitorio = VIN_LOCAL − D20 |
| C61 | 0.1uF | C1525 | Net-(U11-SW)/Net-(U11-BST) | 16 | 6.0 / 62% | 6.0 / 62% | X7R 0402 | OK | VBST − VSW ≤ 6 V (DS42002 abs máx) |
| C63 | 0.1uF | C1525 | +3.3VA/GND | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | AP2125 3,3 V |
| C64 | 4.7u | C19666 | +3.3VA/GND | 16 | 3.36 / 79% | 3.6 / 78% | X5R 0603 | OK | AP2125 3,3 V |
| C67 | 22µF, 16V, X5R, 0805 | C45783 | +3.3VA/GND | 25 | 3.36 / 87% | 3.6 / 86% | X5R 0805 | OK | el value dice 16 V, LCSC dice 25 V; AP2125 3,3 V |
| C69 | 22µF, 16V | C45783 | +5VA/GND | 25 | 5.23 / 79% | 5.75 / 77% | X5R 0805 | OK | el value dice 16 V, LCSC dice 25 V; 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C70 | 22µF, 16V | C45783 | +5VA/GND | 25 | 5.23 / 79% | 5.75 / 77% | X5R 0805 | OK | el value dice 16 V, LCSC dice 25 V; 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C71 | 220uF x16V | C25503905 | +5VA/GND | 16 | 5.23 / 67% | 5.75 / 64% | Al-polímero D6.3x7.7 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C72 | 0.1uF | C1525 | +5VA/GND | 16 | 5.23 / 67% | 5.75 / 64% | X7R 0402 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C73 | 10u cer | C1691 | GND/+5VA | 6.3 | 5.23 / 17% | 5.75 / 9% | X5R 0603 | MARGEN BAJO | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C74 | 10u cer | C1691 | GND/+5VA | 6.3 | 5.23 / 17% | 5.75 / 9% | X5R 0603 | MARGEN BAJO | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C75 | 10u cer | C1691 | GND/+5VA | 6.3 | 5.23 / 17% | 5.75 / 9% | X5R 0603 | MARGEN BAJO | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C76 | 10u cer | C1691 | GND/+5VA | 6.3 | 5.23 / 17% | 5.75 / 9% | X5R 0603 | MARGEN BAJO | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C77 | 10u cer | C1691 | GND/+5VA | 6.3 | 5.23 / 17% | 5.75 / 9% | X5R 0603 | MARGEN BAJO | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C78 | 10u cer | C1691 | GND/+5VA | 6.3 | 5.23 / 17% | 5.75 / 9% | X5R 0603 | MARGEN BAJO | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C79 | 10u cer | C1691 | GND/+5VA | 6.3 | 5.23 / 17% | 5.75 / 9% | X5R 0603 | MARGEN BAJO | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C80 | 10u cer | C1691 | GND/+5VA | 6.3 | 5.23 / 17% | 5.75 / 9% | X5R 0603 | MARGEN BAJO | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C89 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C100 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C111 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C122 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C133 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C144 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C155 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C166 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C177 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C188 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C199 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C210 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C221 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C232 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C243 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C254 | 1u | C15849 | GND/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | X5R 0603 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C257 | 100uF | C18221491 | Net-(IC3-VREFH1)/GND | 10 | 3.36 / 66% | 3.6 / 64% | Al D5x5.4 | OK | +3.3VA por 20 Ω |
| C258 | 0.1uF | C1525 | GND/Net-(IC3-VREFH1) | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | +3.3VA por 20 Ω |
| C259 | 10u cer | C1691 | +3.3VA/GND | 6.3 | 3.36 / 47% | 3.6 / 43% | X5R 0603 | OK | AP2125 3,3 V |
| C260 | 0.1uF | C1525 | +3.3VA/GND | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | AP2125 3,3 V |
| C261 | 0.1uF | C1525 | GND/Net-(IC3-VREFH2) | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | +3.3VA por 20 Ω |
| C262 | 100uF | C18221491 | Net-(IC3-VREFH2)/GND | 10 | 3.36 / 66% | 3.6 / 64% | Al D5x5.4 | OK | +3.3VA por 20 Ω |
| C263 | 0.1uF | C1525 | GND/Net-(IC3-VREFH3) | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | +3.3VA por 20 Ω |
| C264 | 100uF | C18221491 | Net-(IC3-VREFH3)/GND | 10 | 3.36 / 66% | 3.6 / 64% | Al D5x5.4 | OK | +3.3VA por 20 Ω |
| C265 | 10u cer | C1691 | GND/+3.3VA | 6.3 | 3.36 / 47% | 3.6 / 43% | X5R 0603 | OK | AP2125 3,3 V |
| C266 | 0.1uF | C1525 | +3.3VA/GND | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | AP2125 3,3 V |
| C267 | 0.1uF | C1525 | Net-(IC3-VREFH4)/GND | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | +3.3VA por 20 Ω |
| C268 | 0.1uF | C1525 | GND/Net-(IC3-VDD18) | 16 | 1.98 / 88% | 1.98 / 88% | X7R 0402 | OK | LDO interno del AK5538 |
| C269 | 0.1uF | C1525 | +3V3/GND | 16 | 3.36 / 79% | 3.6 / 78% | X7R 0402 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C270 | 100uF | C18221491 | Net-(IC3-VREFH4)/GND | 10 | 3.36 / 66% | 3.6 / 64% | Al D5x5.4 | OK | +3.3VA por 20 Ω |
| C271 | 4.7u | C19666 | Net-(IC3-VDD18)/GND | 16 | 1.98 / 88% | 1.98 / 88% | X5R 0603 | OK | LDO interno del AK5538 |
| C272 | 10u cer | C1691 | +3V3/GND | 6.3 | 3.36 / 47% | 3.6 / 43% | X5R 0603 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C273 | 100uF x35V | C2836437 | +VIN_LOCAL/GND | 35 | 21.0 / 40% | 32.4 / 7% | Al D6.3x7.7 | MARGEN BAJO EN TRANSITORIO | ladrillo 20–21 V; transitorio = clamp del SMBJ20A |
| C290 | 220uF x16V | C25503905 | +5VA/GND | 16 | 5.23 / 67% | 5.75 / 64% | Al-polímero D6.3x7.7 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C291 | 220uF x16V | C25503905 | +5VA/GND | 16 | 5.23 / 67% | 5.75 / 64% | Al-polímero D6.3x7.7 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C292 | 220uF x16V | C25503905 | +5VA/GND | 16 | 5.23 / 67% | 5.75 / 64% | Al-polímero D6.3x7.7 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C293 | 220uF x16V | C25503905 | +5VA/GND | 16 | 5.23 / 67% | 5.75 / 64% | Al-polímero D6.3x7.7 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C314 | 100uF x35V | C2836437 | +VIN_LOCAL/GND | 35 | 21.0 / 40% | 32.4 / 7% | Al D6.3x7.7 | MARGEN BAJO EN TRANSITORIO | ladrillo 20–21 V; transitorio = clamp del SMBJ20A |
| C315 | 0.1uF x 50v | C14663 | +VIN_LOCAL/GND | 50 | 21.0 / 58% | 32.4 / 35% | X7R 0603 | OK | ladrillo 20–21 V; transitorio = clamp del SMBJ20A |
| C316 | 10uF x 50V | C138687 | +VIN_LOCAL/GND | 50 | 21.0 / 58% | 32.4 / 35% | X7R 1210 | OK | ladrillo 20–21 V; transitorio = clamp del SMBJ20A |
| C317 | 0.1uF | C1525 | Net-(IC4-BST)/Net-(IC4-SW) | 16 | 6.0 / 62% | 6.0 / 62% | X7R 0402 | OK | VBST − VSW ≤ 6 V (DS41949 abs máx) |
| C318 | 220pF | C39122 | /DS_Power/FB_A/+5VA | 50 | 5.23 / 90% | 5.75 / 88% | C0G 0402 | OK | 5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto) |
| C319 | 10uF x 50V | C138687 | /DS_Power/IN/GND | 50 | 20.0 / 60% | 32.0 / 36% | X7R 1210 | OK | ≈20 V; transitorio = VIN_LOCAL − D20 |
| C320 | 0.1uF x 50v | C14663 | /DS_Power/IN/GND | 50 | 20.0 / 60% | 32.0 / 36% | X7R 0603 | OK | ≈20 V; transitorio = VIN_LOCAL − D20 |
| C321 | 22µF, 16V | C45783 | +3V3/GND | 25 | 3.36 / 87% | 3.6 / 86% | X5R 0805 | OK | el value dice 16 V, LCSC dice 25 V; 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C322 | 22µF, 16V | C45783 | +3V3/GND | 25 | 3.36 / 87% | 3.6 / 86% | X5R 0805 | OK | el value dice 16 V, LCSC dice 25 V; 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C323 | 100pF | C327198 | +3V3//DS_Power/FB_B | 50 | 3.36 / 93% | 3.6 / 93% | NP0 0402 | OK | 3,20–3,36 V (B); transitorio = OVP +10 % del AP63301 |
| C324 | 10uF x 50V | C138687 | +VIN_LOCAL/GND | 50 | 21.0 / 58% | 32.4 / 35% | X7R 1210 | OK | ladrillo 20–21 V; transitorio = clamp del SMBJ20A |
| C325 | 10uF x 50V | C138687 | /DS_Power/IN/GND | 50 | 20.0 / 60% | 32.0 / 36% | X7R 1210 | OK | ≈20 V; transitorio = VIN_LOCAL − D20 |
| C326 | 10uF x 50V | C138687 | +VIN_LOCAL/GND | 50 | 21.0 / 58% | 32.4 / 35% | X7R 1210 | OK | ladrillo 20–21 V; transitorio = clamp del SMBJ20A |
