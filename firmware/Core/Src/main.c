/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "uart_protocol.h"
#include "pcm1690.h"
#include "pin_definitions.h"
#include "eeprom_config.h"
#include "ota.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define AUDIO_SAMPLE_RATE    48000
/* 240 muestras = 5 ms por medio buffer @48kHz.
 *
 * Era 480 (10 ms). El buffer domina la latencia total: de los 11,4 ms del
 * pipeline, 10 eran esto. Bajarlo a la mitad deja el total en ~6,4 ms (−44%).
 *
 * El precio es que el deadline de fill pasa de 10 ms a 5 ms. Eso obligó a hacer
 * cooperativas las cuatro operaciones que bloqueaban más que eso —los prints del
 * heartbeat, el guardado en EEPROM, SendStatus y el cambio de volumen— porque
 * cualquiera de ellas habría producido pops. Ese trabajo ya está hecho y
 * verificado con 0 underruns; ver docs/ds11_perf.md.
 *
 * ADC_BUFFER_SAMPLES lo sigue automáticamente, así que el pipeline entero queda
 * consistente. */
#define AUDIO_BUFFER_SAMPLES 240      // 5ms @ 48kHz (era 480 = 10ms)
#define AUDIO_CHANNELS       8        // TDM8 (8 channels per PCM1690)
#define AUDIO_BUFFER_SIZE    (AUDIO_BUFFER_SAMPLES * AUDIO_CHANNELS)

#define CPU_FREQ_MHZ         480      // CPU clock after PLL1 config

#define ADC_CHANNELS         8        // AK5538VN 8ch TDM256 input
#define ADC_BUFFER_SAMPLES   AUDIO_BUFFER_SAMPLES  // 480 samples = 10ms
#define ADC_BUFFER_SIZE      (ADC_BUFFER_SAMPLES * ADC_CHANNELS)   // 960 samples

// Beamforming delay line
#define DELAY_LINE_SIZE      1024     // power of 2 for fast & mask
#define DELAY_LINE_MASK      (DELAY_LINE_SIZE - 1)
#define MAX_DELAY_SAMPLES    64       // ~1.33ms @ 48kHz = ~45.7cm at 343 m/s
#define BEAM_NUM_CHANNELS    32

// ===== RX beamforming (delay-and-sum over the 8 ADC inputs) =====
#define MIC_COUNT            8
#define SPEED_OF_SOUND_MM_S  343000.0f
// PLACEHOLDER geometry: uniform linear array along X, 50mm pitch.
// EDIT mic_pos_x_mm[] below when the real input/mic geometry is decided —
// the steering math only depends on that table.
#define MIC_SPACING_MM       50.0f

// ===== TX beamforming — speaker array geometry (DS 4×8 brick pattern) =====
// Listener frame, facing the cones: X = to the LISTENER's right, Y = from
// row 1 toward row 4 (physical up/down TBC by user), Z = out toward listener.
// Rows 1&3 (r=0,2) aligned; rows 2&4 (r=1,3) shifted +40mm in X.
// Channel map placeholder: ch = row*8 + col, col 0 = leftmost (row-major) —
// to be validated with the channel walker on real DS11 hardware.
#define SPK_ROWS         4
#define SPK_COLS         8
#define SPK_X_PITCH_MM   80.0f
#define SPK_Y_PITCH_MM   160.0f
#define SPK_ROW_DX_MM    40.0f    // brick offset of rows 2&4 (r=1,3)
// Array extents: X = 7×80+40 = 600mm (brick offset included), Y = 3×160 =
// 480mm ⇒ diagonal ≈ 768mm ≈ 108 samples @48kHz. By the triangle inequality
// no focal point can require a delay spread beyond the diagonal, so 128
// covers every possible focus. Delay-line bound: delay+480 ≤ 1024 ⇒ ≤544 ✓.
#define TX_MAX_DELAY_SAMPLES 128

// ===== Channel walker (sequential per-channel test tone) =====
#define WALKER_SECONDS_PER_CH 1
// DS11: los 4 PCM1690 poblados → 32 canales. El walker recorre el array
// completo (32 s por vuelta) y valida el mapa canal→parlante + el cableado
// JSPK, incluida la doble inversión deliberada de los canales impares
// (ver docs/ds11_upgrades.md: al JST llega la polaridad correcta).
#define WALKER_NUM_CH        32

// ===== AUTOFOCUS: measured focal calibration with a microphone =====
// Place a mic AT the desired focal point, feed it into ADC input 1 (ch0).
// The scan plays a 10ms chirp through each speaker in turn, captures the mic,
// and cross-correlates against the chirp reference → per-speaker time of
// flight. delay_k = max(tof) − tof_k then FOCUSES the array exactly at the
// mic — measured, not modeled: absorbs geometry error, channel mapping AND
// polarity (a negative correlation peak = inverted speaker → speaker_sign).
// All fixed pipeline latencies (TX buffer wait, DAC/ADC group delay) are
// identical for every speaker, so they cancel in the relative delays.
#define CAL_MIC_CH        0                       // measurement mic = ADC ch0
#define CAL_CHIRP_LEN     480                     // 10ms 500Hz→6kHz linear chirp
#define CAL_CAPTURE_LEN   4800                    // 100ms capture window
#define CAL_MAX_LAG       (CAL_CAPTURE_LEN - CAL_CHIRP_LEN)
#define CAL_SETTLE_HALVES 10                      // 100ms of silence between speakers
#define CAL_CORR_CHUNK    128                     // lags per main-loop pass (~0.5ms)
// DS11: 32 parlantes ≈ 8 s de scan (~250 ms por parlante: settle + captura +
// correlación chunked). El mic no se mueve durante todo el barrido.
#define CAL_NUM_CH        WALKER_NUM_CH           // scan the populated channels
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Beamforming config — los 32 canales, mismo alcance que el ds_config_t del ESP32 */
typedef struct {
    uint8_t  master_gain;                   // 0-255 → 0.0-1.0
    uint8_t  mute_global;                   // 0=unmuted, 1=muted
    uint16_t delays[BEAM_NUM_CHANNELS];     // delay in samples per channel
    uint8_t  gains[BEAM_NUM_CHANNELS];      // gain per channel 0-255
    uint32_t mutes;                         // bitfield: bit N = channel N muted
    int8_t   master_vol_db;                 // PCM1690 attenuation, dB (0..-63, ≤-64 mute)
    uint8_t  last_mode;                     // ui_states index restored at boot
    uint8_t  in_gain_db;                    // ganancia de ENTRADA, dB (0..36)
    /* ===== Calibración del AUTOFOCUS, persistida (v5) =====
     * Hasta ahora el scan medía bien y el resultado se EVAPORABA: los retardos
     * se pisaban al cambiar de modo y las polaridades no sobrevivían al
     * reinicio. O sea que la calibración había que rehacerla, con el micrófono
     * en su lugar, cada vez que se encendía el equipo — que es exactamente lo
     * que no se puede pedir en una instalación.
     *
     * Persistirla es viable porque el SYNC entre bancos mide 4 µs y es
     * REPRODUCIBLE arranque tras arranque (medido 2026-08-13): si variara por
     * encendido, una calibración dejaría de valer tras un ciclo de
     * alimentación. Ese era el "no se puede" que ya no aplica.
     *
     * Son dos cosas distintas y se usan distinto:
     *  - cal_sign: propiedad del HARDWARE (cableado/polaridad del parlante). No
     *    depende de dónde esté el foco → se aplica SIEMPRE, en todos los modos.
     *  - cal_delay_q8: es UN foco, el del punto donde estaba el micrófono. NO
     *    generaliza a otros puntos → se recupera a pedido (modo 18), nunca solo.
     */
    int8_t   cal_sign[BEAM_NUM_CHANNELS];   // polaridad medida (+1/−1)
    uint16_t cal_delay_q8[BEAM_NUM_CHANNELS]; // foco medido en el punto del mic
    uint8_t  cal_valid;                     // 1 = hay una calibración guardada
} beam_config_t;
/* La calibración llevó la config de 108 a 208 bytes. Sin este chequeo, cruzar
 * el techo se manifestaría como "el equipo dejó de recordar los ajustes",
 * mucho después y lejos del campo que lo causó — el guardado es diferido y
 * falla en silencio salvo por una línea del serie. */
_Static_assert(sizeof(beam_config_t) <= EEPROM_CFG_MAX_SIZE,
               "beam_config_t no entra en el buffer de la EEPROM");

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c3;

SAI_HandleTypeDef hsai_BlockA1;
SAI_HandleTypeDef hsai_BlockB1;
SAI_HandleTypeDef hsai_BlockA2;
SAI_HandleTypeDef hsai_BlockB2;
SAI_HandleTypeDef hsai_BlockB3;
DMA_HandleTypeDef hdma_sai1_a;
DMA_HandleTypeDef hdma_sai1_b;
DMA_HandleTypeDef hdma_sai2_a;
DMA_HandleTypeDef hdma_sai2_b;
DMA_HandleTypeDef hdma_sai3_b;
UART_HandleTypeDef huart7;
UART_HandleTypeDef huart4;

ADC_HandleTypeDef hadc3;
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */

/* ===== DMA Double Buffer (ping-pong) =====
 * Layout: [HALF_A (480 × 8ch)][HALF_B (480 × 8ch)]
 *          ^                    ^
 *          HalfCplt fills A     FullCplt fills B
 *
 * TDM8: 8 channels interleaved per sample frame
 * While DMA sends HALF_B → CPU fills HALF_A (and vice versa)
 * MUST be in RAM_D2 for DMA1 access (DTCMRAM NOT DMA-accessible on STM32H7!)
 */
static int32_t audio_buffer[AUDIO_BUFFER_SIZE * 2] __attribute__((section(".dma_buffer"), aligned(32)));
static int32_t audio_buffer_1b[AUDIO_BUFFER_SIZE * 2] __attribute__((section(".dma_buffer"), aligned(32)));
static int32_t audio_buffer_2a[AUDIO_BUFFER_SIZE * 2] __attribute__((section(".dma_buffer"), aligned(32)));
static int32_t audio_buffer_2b[AUDIO_BUFFER_SIZE * 2] __attribute__((section(".dma_buffer"), aligned(32)));
static int32_t audio_buffer_3b_rx[ADC_BUFFER_SIZE * 2] __attribute__((section(".dma_buffer"), aligned(32)));

// Circular delay lines for beamforming — 8 independent channels (AK5538VN 8ch)
// delay_line[0] = ADC ch0 (main beam source); [1]-[7] las 8 entradas del modo 15.
// NO es memoria DMA: la escribe la CPU en Process_ADC_Half copiando desde
// audio_buffer_3b_rx. Por eso vive en DTCMRAM (0 wait states, acoplada al core)
// y no en RAM_D2 (vía AXI, que es donde la ponía .dma_buffer). Es la estructura
// más leída del pipeline — el modo 15 hace 8 lecturas por muestra de salida y
// por canal. 32 KB de los 128 KB de DTCM.
static int32_t delay_line[8][DELAY_LINE_SIZE] __attribute__((aligned(32)));
static uint32_t delay_write_pos = 0;

// Active beamforming config (copied from ESP32 ds_config_t on update)
static beam_config_t beam_config;

/* ===== Test Tone: 1kHz sine @ 48kHz sample rate =====
 * 48 samples per period (48000/1000 = 48), amplitude = -6dBFS (0x400000 peak)
 * 240 samples por medio buffer = exactamente 5 períodos → sin error de fase
 * entre llamadas. SET test_tone_enabled = 0 to restore normal beamforming mode
 *
 * ⚠️ NO const, A PROPÓSITO — no lo "arregles".
 *
 * Con `const` la tabla queda en .rodata, o sea en FLASH, y como el D-cache está
 * apagado cada lectura paga los wait states del flash. Sin const va a .data,
 * que este linker script pone en DTCMRAM (0 wait states, ver la sección .data
 * en STM32H743XX_FLASH.ld) y el startup la copia sola.
 *
 * No es cosmético: el modo 14 lee esta tabla DOS veces por (muestra × canal ×
 * fuente) = 61440 lecturas por ventana de 5 ms, y con la tabla en flash perdía
 * el 15% de los deadlines (1234 underruns en 20 s medidos el 2026-08-18)
 * mientras el modo 15 —que es MÁS pesado pero lee delay_line desde DTCM— no
 * perdía ninguno. Misma causa raíz que el arreglo de rendimiento de agosto: en
 * este chip lo que importa no es cuántas operaciones hacés sino de qué memoria
 * leés. Los modos 12/13 la leen una vez por muestra, por eso nunca sufrieron.
 *
 * La sección explícita tampoco es opcional: sacar el `const` no alcanza, porque
 * con -O2 GCC ve que nadie la escribe y la manda a .rodata igual. Verificar con
 *   arm-none-eabi-nm build/ds11_beamforming.elf | grep sine_table
 * → tiene que dar 0x2000xxxx (DTCM) y 'd', no 0x08xxxxxx y 'r'. */
__attribute__((section(".data")))
static int32_t sine_table_1khz[48] = {
         0,   547466,  1085575,  1605080,  2097152,  2553318,
   2965812,  3327563,  3632372,  3875034,  4051387,  4158422,
   4194304,  4158422,  4051387,  3875034,  3632372,  3327563,
   2965812,  2553318,  2097152,  1605080,  1085575,   547466,
         0,  -547466, -1085575, -1605080, -2097152, -2553318,
  -2965812, -3327563, -3632372, -3875034, -4051387, -4158422,
  -4194304, -4158422, -4051387, -3875034, -3632372, -3327563,
  -2965812, -2553318, -2097152, -1605080, -1085575,  -547466
};
static volatile uint8_t test_tone_enabled = 0; /* 1 = 1kHz test tone; 0 = use monitor/beamform */
static uint32_t test_tone_phase = 0;

/* ADC monitor mode: route the live digitized ADC straight to the DACs so you
 * can HEAR what the AK5538 is capturing. Each DAC channel plays its matching
 * ADC channel (ch0→ch0 …), repitiendo las 8 entradas en cada fila de 8
 * parlantes (DS11: 4 filas). Escalado por la ganancia de entrada (ver abajo)
 * y clampeado a FS de 24 bits. Takes priority over test tone and beamform. */
static volatile uint8_t adc_monitor_enabled = 1;   /* 1 = live ADC→DAC monitor */

/* ===== Ganancia de ENTRADA — ajustable en runtime y PERSISTENTE =====
 *
 * La estructura de ganancia estaba al revés y ese era el cuello de botella real:
 * el modo 0 amplificaba +30 dB (para poder oír un micrófono) y después el
 * atenuador del PCM1690 tiraba −40 dB. Con una fuente de LÍNEA el resultado es
 * que el clamp digital recorta mucho antes de que a los parlantes les llegue
 * nada — medido 2026-08-18: ADCpk 448804 (−25 dBFS) × 32 = +4,6 dBFS, con
 * SALIDA(clip) = 3152 recortes/s y el ADC ni enterado (OVF = 0).
 *
 * Es un ajuste GLOBAL, no por modo: la ganancia que hace falta depende de QUÉ
 * ESTÁ ENCHUFADO, no del modo de beamforming activo. Por eso se guarda en la
 * EEPROM junto al volumen — se configura una vez por instalación y sobrevive
 * al reboot (antes había que mandar el comando después de cada arranque).
 *
 * El default es 0 dB = LÍNEA, porque las entradas de la DS11 son de línea:
 * J9 no lleva bias de micrófono (ver ds11-netlist-hallazgos), así que un
 * electret necesita preamp externo igual. El +30 dB es el caso excepcional.
 *
 * Multiplicador Q8 en vez de shift, para pasos de 1 dB en lugar de 6. El
 * multiply extra por muestra es despreciable: el monitor va al 4,3% de CPU. */
static const uint16_t gain_q8_tbl[37] = {
    256,   287,   322,   362,   406,   455,   511,   573,   643,   722,
    810,   908,  1019,  1144,  1283,  1440,  1615,  1812,  2033,  2282,
   2560,  2872,  3223,  3616,  4057,  4552,  5108,  5731,  6430,  7215,
   8095,  9083, 10192, 11435, 12830, 14396, 16153 };
#define IN_GAIN_DB_MAX  36
#define IN_GAIN_DB_MIC  30      /* preset "micrófono" (×32) — con preamp externo */
#define IN_GAIN_DB_LINE 0       /* preset "línea" (×1) — el default de fábrica  */
static volatile uint8_t  in_gain_db  = IN_GAIN_DB_LINE;
static volatile uint16_t in_gain_q8  = 256;       /* = tabla[0] */

static inline int32_t Apply_In_Gain(int32_t s) {
  return (int32_t)(((int64_t)s * (int32_t)in_gain_q8) >> 8);
}

/* Medición de nivel STICKY — ver el comentario en Process_ADC_Half.
 * adc_peak_hold: pico por canal sobre TODAS las muestras (escala 24-bit).
 * adc_ovf_latch: cuántos medios buffers vieron el pin OVF alto (ADC saturando).
 * out_clip_count: cuántas muestras de SALIDA se recortaron (boost excesivo).
 * Juntos separan las dos saturaciones posibles, que suenan igual pero se
 * arreglan distinto. El pico se limpia al imprimirse (es un peak-hold); los
 * dos contadores son MONOTÓNICOS y se publican en /status — el heartbeat
 * imprime su delta. */
static volatile int32_t  adc_peak_hold[ADC_CHANNELS];
/* DOS peak-holds, no uno, porque tienen dos consumidores que se pisaban.
 *
 * El serie drena adc_peak_hold una vez por segundo. /status (telemetría, lo que
 * ve la página) NO podía leer el mismo: el que llegara primero se comía el pico
 * y el otro veía cero. La solución que había era recalcular el pico en el
 * handler de status sobre el buffer DMA vivo — o sea sobre UNA ventana de 10 ms
 * de cada intervalo de sondeo. Es exactamente el muestreo al 1% que en agosto
 * hizo concluir "al ADC le sobran 45 dB" mientras el equipo saturaba de forma
 * audible: se arregló para el serie y quedó sin arreglar para la red.
 *
 * Importa ahora porque el detector de micrófono cuelga de esto: con ventanas de
 * 10 ms un micrófono captando una sala parpadearía entre "vivo" y "muerto". */
static volatile int32_t  adc_peak_net[ADC_CHANNELS];
static volatile uint32_t adc_ovf_latch = 0;
static volatile uint32_t out_clip_count = 0;

/* ===== Detección de micrófono / señal de entrada =====
 * Nace del AUTOFOCUS: el scan mide con un micrófono en la entrada 1, y hasta
 * ahora la única forma de saber si ese micrófono estaba vivo era mirar los
 * números del serie. Si no lo estaba, el scan igual "terminaba" y dejaba 32
 * retardos de basura enfocando nada — un fallo silencioso.
 *
 * Piso de ruido MEDIDO con las entradas al aire, 12 muestras del pico de 1 s
 * sobre todas las muestras (2026-08-19): máximo 437 cuentas en escala de 24
 * bits. Ojo con los números viejos de 43-224 que andan por los comentarios: son
 * de cuando el pico se calculaba sobre una ventana de 10 ms, o sea el 1% del
 * tiempo — subestiman el piso por un factor de ~2.
 *
 * El umbral queda ~13 dB por encima de ese máximo: no lo dispara el ruido
 * propio, y cualquier micrófono con preamplificador captando una sala en
 * silencio lo cruza de sobra (−40 dBFS son 83886 cuentas, 40× el umbral).
 *
 * La histéresis existe porque una fuente real tiene pausas: alguien deja de
 * hablar y el micrófono no se desenchufó. */
#define MIC_LIVE_THRESH   2000      /* pico de 1 s que cuenta como "hay señal" */
#define MIC_LIVE_HOLD_S   3         /* segundos que sigue "vivo" tras la última */
static volatile uint8_t mic_live_mask = 0;    /* bit N = entrada N con señal */

/* ===== Operating states — cycled with user button K2 (PA4, press = LOW) =====
 * Flat list: each press advances one state and prints the new one on UART7.
 *   0 ADC monitor  → 1 channel walker → 2..6 RX beam at preset azimuths → wrap
 * Exactly one of adc_monitor_enabled / walker_enabled / beam_enabled is set. */
/* Eje del barrido automático (campo .sweep). Los valores viven acá arriba
 * porque la tabla ui_states[] los usa; la mecánica está más abajo, en el
 * bloque BARRIDO. */
#define SWEEP_AXIS_X    1
#define SWEEP_AXIS_Y    2
typedef struct {
  const char *name;
  uint8_t monitor, walker, beam;
  int16_t az_deg;                 // RX beam steering azimuth
  uint8_t tx;                     // 1 = TX steered output (Phase 2)
  int16_t fx_mm, fy_mm, fz_mm;    // TX focal point, relative to array center
  uint8_t af;                     // 1 = AUTOFOCUS mic-scan mode
  uint8_t tone;                   // 1 = internal tone source feeds beam_line
  uint16_t tone_hz;               // tone frequency (keep ≤2140: d=λ/2 limit)
  uint8_t mf;                     // 1 = MULTIFOCUS tonos (N fuentes internas)
  uint8_t mfin;                   // 1 = MULTIFOCUS de ENTRADAS (8 ADC → 8 focos)
  uint8_t sweep;                  // eje del barrido automático: 0 no, 1 = X, 2 = Y
  uint8_t calfoc;                 // 1 = usa el foco MEDIDO por AUTOFOCUS, no el geométrico
  uint8_t loopb;                  // 1 = LOOPBACK: medición eléctrica del camino de salida
  uint8_t fsweep;                 // 1 = BARRIDO DE FRECUENCIA por el lazo (Test D)
} ui_state_t;
static const ui_state_t ui_states[] = {
  { .name = "ADC monitor",                  .monitor = 1 },
  { .name = "Channel walker (1s/ch)",       .walker = 1 },
  { .name = "RX beam az=0 (broadside)",     .beam = 1, .az_deg =   0 },
  { .name = "RX beam az=+30",               .beam = 1, .az_deg =  30 },
  { .name = "RX beam az=-30",               .beam = 1, .az_deg = -30 },
  { .name = "RX beam az=+60",               .beam = 1, .az_deg =  60 },
  { .name = "RX beam az=-60",               .beam = 1, .az_deg = -60 },
  // TX steering presets (Phase 2): tono interno de 2 kHz enfocado en 4 puntos
  // distintos. Caminando la sala se oye el spot moverse — el modo se valida
  // solo, sin instrumental y sin nada conectado a la entrada.
  //
  // 2 kHz, NO 1 kHz: la apertura en X son 600 mm (8 columnas × 80 mm + 40 de
  // ladrillo). A 1 kHz eso es 1,75 λ y el spot a 1 m sale de ±286 mm — los
  // focos de izquierda y derecha, separados 1 m, casi se solapan y desde el
  // centro suenan IGUAL (verificado a oído el 2026-08-19: "8 y 9 hacen lo
  // mismo que el 13"). A 2 kHz la apertura es 3,50 λ y el spot cae a ±143 mm:
  // el doble de resolución. El techo es 2144 Hz (c/2d, lóbulos de rejilla),
  // así que 2 kHz es lo mejor que este array puede hacer.
  //
  // La fuente era el RX beam (los micrófonos) hasta 2026-08-18. El diseño
  // asumía micrófonos en la placa, y la DS11 no los tiene: J9 son entradas de
  // LÍNEA sin bias, así que estos modos reproducían lo que hubiera enchufado
  // en el ADC y no se distinguían del modo 0 salvo por el retardo. Escuchar la
  // entrada ya lo hacen los modos 2-6 (beam RX) y, mucho mejor, el 15
  // (cada entrada en su propio punto). Lo que faltaba era demostrar el
  // steering, y para eso la fuente tiene que ser interna.
  //
  // X en el marco del OYENTE MIRANDO LOS CONOS (si L/R salen espejados en la
  // caja real, invertir el signo de las coords SPK — una línea). Y hacia fila 4.
  { .name = "TX 2k ondulante LEJOS 2m",     .beam = 1, .tx = 1, .tone = 1,
    .tone_hz = 2000, .fx_mm =    0, .fy_mm = 0, .fz_mm = 2000 },
  { .name = "TX 2k ondulante izq 0.5m @1m", .beam = 1, .tx = 1, .tone = 1,
    .tone_hz = 2000, .fx_mm = -500, .fy_mm = 0, .fz_mm = 1000 },
  { .name = "TX 2k ondulante der 0.5m @1m", .beam = 1, .tx = 1, .tone = 1,
    .tone_hz = 2000, .fx_mm =  500, .fy_mm = 0, .fz_mm = 1000 },
  { .name = "TX 2k ondulante CERCA 0.3m",   .beam = 1, .tx = 1, .tone = 1,
    .tone_hz = 2000, .fx_mm =    0, .fy_mm = 0, .fz_mm =  300 },
  // Measured focal calibration: mic at the target point → ADC input 1 (c0),
  // chirp scan of every speaker → TOF-derived delays + polarity, then a
  // steered tone focused at the mic. Re-enter the mode to re-scan.
  { .name = "AUTOFOCUS scan (mic->ADC c0)",  .af = 1 },
  // Internal tone → beam_line → the TX steering path runs UNCHANGED (Q8
  // fractional delays, gains, signs): directionality validation with no mic
  // on the board (docs/historico (plan implementado: modos 12-13)). DS11: las 4 filas pobladas →
  // fy=0 (centro del array); en DS10 iba a -240 para apuntar al plano de la
  // única fila poblada. Ahora la apertura vertical (480mm) también enfoca.
  // 2kHz spot @1m: ±14cm; 1kHz: ±27cm (ratio ~2 is the self-validating
  // test). APPEND ONLY: the ESP32/web/app mirror these indices.
  { .name = "TX 2k ondulante centro @1m",   .beam = 1, .tx = 1, .tone = 1,
    .tone_hz = 2000, .fx_mm = 0, .fy_mm = 0, .fz_mm = 1000 },
  { .name = "TX 1k ondulante centro @1m (spot 2x)", .beam = 1, .tx = 1, .tone = 1,
    .tone_hz = 1000, .fx_mm = 0, .fy_mm = 0, .fz_mm = 1000 },
  // MULTIFOCUS: 4 tonos internos, cada uno enfocado en su propio punto —
  // caminando la sala, un analizador espectral "lee" la posición por cuál
  // frecuencia domina. Precuela de la intervención de 8 entradas analógicas
  // (mismas tablas mf_delay_q8; solo cambiará la fuente de señal).
  { .name = "MULTIFOCUS 4 tonos @1m",       .mf = 1 },
  // MULTIFOCUS de entradas — LA INTERVENCIÓN: cada entrada analógica del
  // AK5538 se proyecta enfocada en su propio punto de la sala (8 señales
  // reales — voces, instrumentos — en 8 lugares). Mismas tablas de foco que
  // el modo 14; la fuente es delay_line[src] en vez de tonos.
  { .name = "MULTIFOCUS 8 entradas @1m",    .mfin = 1 },
  // BARRIDO: el foco recorre solo la sala de izquierda a derecha y vuelta. Es la
  // demostración del steering que NO exige caminar ni comparar dos modos: parado
  // en un punto fijo, el nivel sube cuando el foco pasa por vos y baja cuando se
  // va. Mismo tono ondulante de 2 kHz que los presets.
  { .name = "TX 2k BARRIDO izq<->der @1m", .beam = 1, .tx = 1, .tone = 1,
    .tone_hz = 2000, .fx_mm = 0, .fy_mm = 0, .fz_mm = 1000, .sweep = SWEEP_AXIS_X },
  // BARRIDO VERTICAL — NO es "el 16 pero en Y": es un EXPERIMENTO, y la
  // frecuencia es el experimento.
  //
  // El 2026-08-19 Fernando reportó que el eje vertical del modo 15 no se
  // percibe. La causa es geométrica: las filas están a 160 mm, que a 2 kHz son
  // 0,93 λ — muy por encima del límite de muestreo espacial d ≤ λ/2. El array
  // vertical está ALIASEADO a esa frecuencia: en vez de un foco produce lóbulos
  // de rejilla. El techo vertical es c/2d = 1072 Hz, contra 2144 Hz del
  // horizontal (columnas a 80 mm).
  //
  // A 800 Hz (±5% de dither → 840 Hz máx) el paso vertical es 0,37 λ: DENTRO
  // del límite, sin lóbulos de rejilla, en todo el rango de steering. Si acá se
  // percibe movimiento vertical y a 2 kHz no, la causa está confirmada como
  // aliasing y bajar el paso de filas en la DS12 se justifica. Si tampoco se
  // percibe, el limitante son las 4 filas (apertura 480 mm = 1,12 λ a 800 Hz →
  // haz de ~51°, spot de ±45 cm a 1 m) y achicar el paso NO alcanza: hay que
  // agregar filas. Las dos respuestas deciden gasto de hardware — ver
  // hardware/DS/DS12_PLANNING.md §0.5.
  { .name = "BARRIDO VERTICAL 800Hz arriba<->abajo @1m", .beam = 1, .tx = 1,
    .tone = 1, .tone_hz = 800, .fx_mm = 0, .fy_mm = 0, .fz_mm = 1000,
    .sweep = SWEEP_AXIS_Y },
  // FOCO CALIBRADO — recupera de la EEPROM los retardos MEDIDOS por el último
  // AUTOFOCUS, en vez de calcularlos con la geometría nominal. Absorbe error de
  // geometría, de mapeo de canales y de polaridad: es el foco real en el punto
  // donde estaba el micrófono.
  //
  // Existe como modo aparte, y no aplicándose solo al arrancar, porque
  // cal_delay_q8 NO es una corrección general: es UN foco, el de ese punto.
  // Aplicarlo por defecto pisaría el foco de cualquier otro modo con el de un
  // micrófono que ya no está ahí. Las POLARIDADES sí se aplican siempre (son
  // del hardware, no del punto) y se restauran en el boot.
  //
  // Si no hay calibración guardada, el modo lo dice y cae al foco geométrico
  // de 1 m — no se queda mudo ni miente.
  { .name = "TX foco CALIBRADO (AUTOFOCUS guardado)", .beam = 1, .tx = 1,
    .tone = 1, .tone_hz = 2000, .fx_mm = 0, .fy_mm = 0, .fz_mm = 1000,
    .calfoc = 1 },
  // LOOPBACK: no suena para nadie — es un modo de banco. Emite el chirp por un
  // canal de cada banco y mide lo que vuelve por el ADC a través de los
  // adaptadores pasivos (docs/loopback_debug.md). Informa por el serie cada 2 s.
  // Sin adaptadores conectados dice "sin senal" y no hace nada más.
  { .name = "LOOPBACK electrico (bancos 0/8/16/24 -> ADC 0-3)", .loopb = 1 },
  // BARRIDO DE FRECUENCIA: tampoco suena para nadie. Emite un tono por canal de
  // cada banco y lo detecta coherentemente en las ocho entradas, punto por punto
  // de 20 Hz a 20 kHz. Con el cable de J13 mide la cadena DAC → ADC SIN
  // amplificadores ni 24 V; ver el comentario largo en FS_SETTLE para qué NO
  // alcanza a ver. Informa la tabla completa cada ~7 s.
  { .name = "BARRIDO DE FRECUENCIA por el lazo (20 Hz - 20 kHz)", .fsweep = 1 },
};
#define UI_NUM_STATES (sizeof(ui_states) / sizeof(ui_states[0]))
static volatile uint8_t ui_state_idx = 0;

/* ===== Channel walker state ===== */
static volatile uint8_t walker_enabled = 0;
// Frame counter @48kHz; advanced ONLY by the ch_offset==0 (SAI1_A) fill so all
// 4 banks derive the same active channel. Banks 1B/2A/2B see it post-advance
// (+480), which is phase-exact here: 480 % 48 == 0 (10 full sine periods).
static uint32_t walker_frame = 0;

/* ===== RX beam (delay-and-sum) state =====
 * beam_line[] holds the summed beam in 24-bit scale (delay_line samples are
 * 32-bit ADC scale, so each term is >>8 before accumulating; the sum of 8 is
 * then >>3 so a coherent identical signal on all mics comes out at ~unity).
 * Written by Process_ADC_Half at the same positions/cadence as delay_line,
 * read by the TX fills exactly like the ADC monitor reads delay_line. */
static volatile uint8_t beam_enabled = 0;
static int32_t beam_line[DELAY_LINE_SIZE] __attribute__((aligned(32)));
static uint16_t rx_delay[MIC_COUNT] = {0};        // per-mic delay in samples
static volatile int16_t beam_az_deg = 0;
/* Ganancia de salida del beam. Cuando la fuente es el ADC sigue a in_gain_q8
 * (misma cadena de ganancia que el monitor: los modos 2-6 recortaban igual que
 * el 0 cuando la entrada era de línea). MUST be unity (256) en modos de tono:
 * el tono interno ya está a -6dBFS y ×32 satura contra los PAM —
 * UI_ApplyState() la fija por modo. */
static volatile uint16_t beam_out_q8 = 256;

/* ===== Internal tone source (TX directionality without a mic) =====
 * Writes beam_line[] at the same positions/cadence as the RX beam, so the
 * TX steering path is reused as-is and the delay stays a true TIME delay at
 * any frequency (unlike the CAL_DONE tone, whose phase trick is 1kHz-only).
 * Phase accumulator in table-index Q16 over sine_table_1khz[48] (one exact
 * 1kHz period @48k): step per sample = f/1000 indices. Main-loop context. */
static volatile uint8_t  tone_enabled = 0;
static volatile uint16_t tone_freq_hz = 2000;
static uint32_t tone_phase_q16 = 0;
/* Dither de frecuencia (Fase B, docs/historico (plan implementado: modos 12-13)): triángulo ±5%
 * a ~3Hz alrededor de tone_base_hz. El haz es de retardos puros (banda ancha)
 * → no cambia; los modos de la sala se promedian y la medición se estabiliza.
 * El acumulador de fase de Tone_NextSample es continuo → sin clicks. La
 * frecuencia se actualiza una vez por half-buffer (pasos ≤0.3% cada 10ms).
 * Techo: 2000×1.05 = 2100 Hz < 2140 (límite d=λ/2, sin grating lobes). */
/* Dither de frecuencia: triángulo de ±5% a 3 Hz sobre la base (a 2 kHz son
 * 1900-2100 Hz). Se oye como un tono ONDULANTE, y de ahí el nombre de los modos
 * — no es un tono puro y llamarlo así confundía.
 *
 * No es cosmético ni un defecto: un tono PURO en una sala arma ondas
 * estacionarias con nodos fijos en el espacio, así que caminando se escucha el
 * patrón de la SALA en vez del foco del beam. El dither los emborrona. La
 * verificación del steering del 2026-08-19 —modo 8 más fuerte a 50 cm a la
 * izquierda, modo 9 a la derecha— funcionó gracias a esto. */
#define TONE_DITHER_PERIOD 16000   /* samples @48kHz → LFO de 3 Hz */
static volatile uint16_t tone_base_hz = 2000;
static uint32_t tone_dither_pos = 0;
// Input/mic X positions in mm (linear placeholder — see MIC_SPACING_MM note)
static float mic_pos_x_mm[MIC_COUNT] = {
  0.0f * MIC_SPACING_MM, 1.0f * MIC_SPACING_MM, 2.0f * MIC_SPACING_MM,
  3.0f * MIC_SPACING_MM, 4.0f * MIC_SPACING_MM, 5.0f * MIC_SPACING_MM,
  6.0f * MIC_SPACING_MM, 7.0f * MIC_SPACING_MM
};

/* ===== TX steering (Phase 2) state =====
 * tx_delay[] is deliberately SEPARATE from beam_config.delays[]: the ESP32
 * config path overwrites beam_config.delays[0..15] on every command (and in
 * beam mode delays[0..7] double as raw RX delays), so aliasing TX steering
 * onto it would silently destroy the focus each time a config arrives.
 * Gains/mutes/master ARE shared from beam_config (per-channel trims). */
static volatile uint8_t tx_steer_enabled = 0;
/* TX delays in Q8 fixed point (Fase 3): value = samples×256, so one LSB is
 * 1/256 sample ≈ 0.03mm of acoustic path. The fill interpolates linearly
 * between the two adjacent beam_line samples — the first rung of the
 * sub-sample ladder (polyphase FIR only if measurements ever demand it). */
static uint16_t tx_delay_q8[BEAM_NUM_CHANNELS] = {0}; // ≤ TX_MAX_DELAY_SAMPLES<<8
static int8_t speaker_sign[BEAM_NUM_CHANNELS];       // +1 all — DS11 alternating-
                                                     // polarity fix goes here later
static float spk_x_mm[BEAM_NUM_CHANNELS];            // built by Beam_InitTxGeometry
static float spk_y_mm[BEAM_NUM_CHANNELS];
static float spk_center_x_mm = 0, spk_center_y_mm = 0;
// Last TX focal point — reported in CMD_STATUS telemetry
static int16_t tx_focus_x_mm = 0, tx_focus_y_mm = 0, tx_focus_z_mm = 0;

/* ===== BARRIDO (modos 16 y 17) — el foco recorre la sala solo =====
 * Los presets 8/9 prueban el steering, pero exigen pararse en un punto
 * descentrado y comparar dos modos de memoria. Acá el foco se mueve solo: parado
 * donde sea, el nivel sube cuando el foco pasa por vos y baja cuando se va. Es
 * la demostración que se entiende sin explicación previa.
 *
 * Triángulo de ±SWEEP_X_MM en 2×SWEEP_HALF_MS. Se reapunta cada SWEEP_STEP_MS
 * desde el main loop: 32 raíces cuadradas por paso, ~3 µs, despreciable. El paso
 * de foco por actualización es de ~14 mm = 0,04 muestras de delay, así que no
 * hay salto audible al reapuntar. */
#define SWEEP_X_MM      700     /* amplitud a cada lado del centro */
/* Amplitud vertical menor que la horizontal, y no por gusto: la apertura en Y
 * son 480 mm contra 600 mm en X, y a 800 Hz el haz vertical es MUCHO más ancho
 * (±45 cm a 1 m). Barrer ±700 mm sacaría el foco fuera de la zona donde el
 * array todavía puede formarlo, y el recorrido se percibiría como un simple
 * subir y bajar de volumen. ±600 mm es poco más de un ancho de haz a cada
 * lado: el máximo recorrido que sigue siendo un foco moviéndose. */
#define SWEEP_Y_MM      600
#define SWEEP_HALF_MS   2500    /* medio recorrido → ciclo completo de 5 s */
#define SWEEP_STEP_MS   50      /* 20 actualizaciones por segundo */
static volatile uint8_t sweep_axis = 0;   /* 0 = quieto, SWEEP_AXIS_X / _Y */

/* ===== MULTIFOCUS (modo 14) — N fuentes internas, cada una con su foco =====
 * Precuela de la intervención de 8 entradas analógicas: la infraestructura
 * (tabla de delays POR FUENTE + Beam_UpdateMFocus) es la que después va a
 * alimentar el modo 8-in — ahí la fuente será delay_line[src] (los ADC) en
 * vez de tonos, con las MISMAS tablas de foco.
 * Para senos, un retardo de tiempo d equivale EXACTO a un offset de fase
 * d×inc — se precalcula en mf_phoff_q16 al setear foco/frecuencia, así el
 * fill no multiplica en 64 bits por muestra. La fase de cada ventana se
 * deriva de mf_frame.
 *
 * ⚠️ mf_frame lo avanza **Process_ADC_Half**, junto a delay_write_pos, y NINGÚN
 * fill lo toca. Antes lo avanzaba el fill de SAI1_A y los otros tres bancos lo
 * leían ya adelantado un buffer entero: eso sólo era inocuo si f×buffer/48000
 * daba entero, o sea con buffer de 480 y frecuencias múltiplo de 100 Hz. Al
 * bajar el buffer a 240 la condición pasó a ser múltiplo de **200**, y
 * 1300/1900/2100 dejaron de cumplirla → las filas 2-3 quedaban en CONTRAFASE
 * con las 0-1 para 3 de los 4 tonos: el foco no salía débil, salía cancelado.
 *
 * Moverlo a otro banco sólo cambiaba de banco privilegiado y dejaba en pie el
 * segundo problema: si ESE banco no se llenaba en una pasada, la fase no
 * avanzaba y el tono repetía la misma ventana. El reloj del pipeline es el RX
 * —Service_Fills lo procesa antes que los 4 TX para que todos vean el mismo
 * snapshot de wp—, así que mf_frame va ahí y el problema desaparece de raíz. */
#define MF_MAX_SRC   8              // techo: las 8 entradas de la intervención
#define MF_NUM_SRC   4              // fuentes activas hoy (4 tonos)
#define MF_SRC_SHIFT 2              // c/fuente a -6dBFS/4 → suma ≤ -6dBFS
#define TONE_PERIOD_Q16 (48u << 16)
static volatile uint8_t mfocus_enabled = 0;
// Defaults: sin relaciones armónicas entre sí, todas < 2140 Hz (límite d=λ/2).
// Focos separados 300mm ≥ ancho del spot (~±14cm @2kHz, ~±23cm @1.3kHz).
// Ya NO hace falta que sean múltiplo de nada: la fase entre bancos dejó de
// depender de la frecuencia al mover el avance de mf_frame al último banco.
static const uint16_t mf_def_freq[MF_NUM_SRC] = { 1300, 1600, 1900, 2100 };
static const int16_t  mf_def_fx[MF_NUM_SRC]   = { -450, -150,  150,  450 };
static uint16_t mf_freq_hz[MF_MAX_SRC];
static int16_t  mf_fx_mm[MF_MAX_SRC], mf_fy_mm[MF_MAX_SRC], mf_fz_mm[MF_MAX_SRC];
static uint32_t mf_inc_q16[MF_MAX_SRC];                      // paso de fase/muestra
static uint16_t mf_delay_q8[MF_MAX_SRC][BEAM_NUM_CHANNELS];  // foco por fuente
static uint32_t mf_phoff_q16[MF_MAX_SRC][BEAM_NUM_CHANNELS]; // delay→fase precalc
static uint32_t mf_frame = 0;       // avanzado SOLO por el fill de SAI1_A

/* Modo 15: MULTIFOCUS de ENTRADAS — la intervención. Cada entrada analógica
 * (delay_line[src], capturadas siempre por Process_ADC_Half) se proyecta
 * enfocada en su punto usando LAS MISMAS tablas mf_delay_q8 del modo 14,
 * con delay temporal verdadero (interpolación Q8 sobre la línea, igual que
 * el TX steering sobre beam_line) — vale para señal arbitraria. */
#define MF_IN_NUM_SRC MF_MAX_SRC    // las 8 entradas del AK5538
static volatile uint8_t mfocus_in_enabled = 0;
/* La ganancia de entrada de este modo es la global (in_gain_q8): ver el
 * comentario de Apply_In_Gain. Ojo con subirla acá: son 8 fuentes sumadas,
 * y 8 × FS × ×32 desbordaría el int32 del acumulador — por eso el clamp
 * del lazo va ANTES del gain por canal. */

/* ===== AUTOFOCUS scan state =====
 * Sequence per speaker: SETTLE (silence, room decay) → ARM (next fill of the
 * speaker's bank emits the chirp and starts the mic capture) → CAPT (fill
 * 100ms) → CORR (cross-correlation, chunked across main-loop passes so the
 * audio pipeline never misses a deadline) → next speaker … → DONE (play a
 * steered tone focused at the mic, using the MEASURED delays & signs). */
typedef enum {
  CAL_IDLE = 0, CAL_SETTLE, CAL_ARM, CAL_CAPT, CAL_CORR, CAL_DONE
} cal_state_t;
static volatile uint8_t autofocus_enabled = 0;
static volatile cal_state_t cal_state = CAL_IDLE;
static volatile uint8_t  cal_ch = 0;            // speaker being measured
static volatile uint16_t cal_settle = 0;        // settle halves remaining
static volatile uint8_t  cal_capturing = 0;
/* Posición dentro del chirp: existe porque el chirp (480) es más largo que una
 * ventana de fill (240) y hay que emitirlo a lo largo de varias. */
static volatile uint16_t cal_chirp_pos = 0;
static volatile uint16_t cal_cap_count = 0;
static int32_t cal_chirp[CAL_CHIRP_LEN];        // 24-bit chirp ref (Cal_Init)
static int32_t cal_capture[CAL_CAPTURE_LEN];    // mic capture, 24-bit
// Correlation scratch (persist across chunked passes)
static uint32_t cal_corr_lag = 0;
static int64_t  cal_best_mag = 0;
static int32_t  cal_best_lag = 0;
static int8_t   cal_best_sign = 1;
static int64_t  cal_mag_sum = 0;                // for the peak/avg quality ratio
// Per-speaker results
static int32_t  cal_tof_q8[BEAM_NUM_CHANNELS];  // time of flight, Q8 samples
static int8_t   cal_meas_sign[BEAM_NUM_CHANNELS];
static uint16_t cal_quality[BEAM_NUM_CHANNELS]; // peak/avg ×10 (≥CAL_Q_MIN = solid)
/* Umbral de calidad = peak/avg de la correlación ×10, o sea "cuánto sobresale
 * el eco del parlante por encima del resto de la ventana".
 *
 * ⚠️ EL 50 QUE HABÍA ERA EXACTAMENTE EL VALOR DEL RUIDO. Fernando lo detectó el
 * 2026-08-20: con el micrófono DESCONECTADO el scan reportaba "4/32 útiles" una
 * vez y "7/32" otra. No era casual, y la cuenta lo confirma. Correlacionando
 * ruido blanco contra el chirp, |c| es media-normal:
 *
 *   promedio  = σ·√(2/π)          = 0,798 σ
 *   máximo sobre N = CAL_MAX_LAG = 4320 lags
 *             ≈ σ·√(2·ln N)       = 4,09 σ
 *   peak/avg  ≈ 5,13  →  q10 ≈ 51
 *
 * O sea que el umbral estaba clavado en la MEDIANA de la distribución del
 * ruido: cada canal era una moneda al aire, y 4-7 de 32 caían del lado bueno.
 *
 * Con 100 hace falta un máximo de ~8σ, cuya probabilidad sobre 4320 lags es del
 * orden de 5·10⁻¹²: nunca. Una detección real tiene ganancia de proceso de
 * √480 ≈ 22 en amplitud, así que queda muy por encima. */
#define CAL_Q_MIN 100
/* Pico absoluto de la captura, porque peak/avg es un cociente y NO distingue
 * "señal" de "ruido fuerte": si al micrófono le subís el preamp, el ruido crece
 * arriba y abajo y el cociente no se entera. Este gate sí. */
static volatile int32_t cal_cap_peak = 0;
/* Progreso y resultado del scan, publicados en /status (v7). Sin esto, correr
 * el AUTOFOCUS exigía tener el cable serie enchufado para saber si sirvió. */
static volatile uint8_t cal_progress = 0;   // parlantes medidos, 0..CAL_NUM_CH
static volatile uint8_t cal_ok_count = 0;   // de esos, con q ≥ CAL_Q_MIN

/* ===== LOOPBACK — medir el camino de salida por vía ELÉCTRICA =====
 *
 * El AUTOFOCUS mide a través de la sala: lo que devuelve incluye eco,
 * reverberación y la posición del micrófono. Este modo mide el EQUIPO, con un
 * cable: se realimentan salidas de parlante a entradas del ADC a través de un
 * adaptador pasivo (divisor + pasabajos, ver docs/loopback_debug.md §4.2).
 *
 * Lo que habilita, y no se puede medir de ninguna otra forma hoy:
 *
 *   El SYNC entre bancos MEDIDO DEL LADO ANALÓGICO. El número de 4 µs que
 *   publica el heartbeat sale de comparar marcas de DWT->CYCCNT en los
 *   callbacks de DMA: mide cuándo se entera el SOFTWARE, no cuándo sale la
 *   señal. SAI1_A y SAI2_A son masters asíncronos entre sí y su offset queda
 *   fijo al arrancar los streams — es un dato de hardware que nunca se
 *   verificó por fuera del micro.
 *
 * Por eso se emite en los cuatro bancos A LA VEZ y se captura en la misma
 * ventana: el ADC muestrea sus 8 entradas en la misma trama TDM, así que es
 * perfectamente síncrono. Cualquier diferencia entre los retardos medidos es
 * skew REAL de la salida.
 *
 * La correlación es la del AUTOFOCUS. La diferencia es que acá no hay sala en
 * el medio: sin eco, el pico es limpio y el resultado es del equipo. */
#define LB_NUM_EMIT     4      /* se emite por un canal de cada banco: 0, 8, 16, 24 */
/* Se miden las OCHO entradas, no cuatro.
 *
 * La primera versión capturaba las entradas 0-3 asumiendo que cada sonda iba a
 * su índice. Eso obliga a cablear de una forma concreta y a acertarle — y el
 * primer cable real (J13 → entrada 4) ya no encajaba. Midiendo las ocho, el
 * firmware DESCUBRE dónde entró la señal en vez de exigir que se lo declaren:
 * se conecta donde sea y el informe dice qué entrada la vio. */
#define LB_NUM_IN       ADC_CHANNELS
/* 50 ms de ventana. La ida y vuelta DAC→adaptador→ADC son unos pocos buffers
 * (~15-20 ms); 50 ms deja margen de sobra y el retardo absoluto se imprime,
 * así que si alguna vez cayera cerca del borde se vería. */
#define LB_CAPTURE_LEN  2400
#define LB_MAX_LAG      (LB_CAPTURE_LEN - CAL_CHIRP_LEN)
#define LB_CORR_CHUNK   128    /* lags por pasada — mismo criterio que el scan */
#define LB_PERIOD_MS    2000   /* repetir la medición cada 2 s */
/* Canal de salida de cada sonda → se realimenta a la ENTRADA del mismo índice.
 * Uno por banco a propósito: es la configuración que mide el skew entre los
 * cuatro SAI. Con un solo adaptador, la sonda 0 mide y el resto informa
 * "sin señal" — el modo no exige tener los cuatro. */
static const uint8_t lb_emit_ch[LB_NUM_EMIT] = { 0, 8, 16, 24 };
typedef enum { LB_IDLE = 0, LB_ARM, LB_CAPT, LB_CORR, LB_WAIT } lb_state_t;
static volatile uint8_t   lb_enabled = 0;
static volatile lb_state_t lb_state = LB_IDLE;
/* int16 y no int32: ocho entradas × 2400 muestras entrarían 76,8 kB en 32 bits y
 * no hay tanto DTCM libre. A 16 bits son los mismos 38,4 kB que ocupaban cuatro
 * canales en 32, y NO se pierde información útil: el piso de ruido del ADC son
 * ~437 cuentas en escala de 24 bits, o sea ~1,7 después del >>16, contra 0,5 de
 * paso de cuantización. El ruido propio ya dithera el bit menos significativo.
 * Para correlacionar sobra: 480 muestras dan 21,9× de ganancia de proceso. */
static int16_t  lb_capture[LB_NUM_IN][LB_CAPTURE_LEN];
static volatile uint16_t lb_cap_count = 0;
static volatile uint8_t  lb_capturing = 0;
static volatile uint16_t lb_chirp_pos = 0;
/* Posición de emisión POR BANCO. Antes los cuatro leían lb_chirp_pos y sólo el
 * banco 24 lo avanzaba, para que dentro de una pasada de Service_Fills los
 * cuatro emitieran el mismo trozo. Eso vale mientras cada pasada llene UNA
 * mitad por banco — pero si el lazo se demora más de 5 ms, la pasada llena las
 * DOS: los bancos 0/8/16 emiten el mismo trozo dos veces mientras el 24 avanza
 * dos veces, y quedan 240 muestras desfasados. Con una posición propia por
 * banco el resultado no depende de cómo se agrupen las mitades en la pasada.
 * Medido: pasa 7 veces por arranque (contador dbl_fill), siempre antes de la
 * primera captura — pero un guardado de EEPROM bloquea ~12 ms y lo dispararía
 * en plena medición, metiendo 5 ms de skew en el modo que mide skew. */
static volatile uint16_t lb_bank_pos[4] = {0,0,0,0};
static volatile uint32_t lb_next_ms = 0;
/* Estado de la correlación, persistente entre pasadas (igual que el scan) */
static uint8_t  lb_probe = 0;
static uint32_t lb_corr_lag = 0;
static int64_t  lb_best_mag = 0, lb_mag_sum = 0;
static int32_t  lb_best_lag = 0;
static int8_t   lb_best_sign = 1;
/* Fase de los DMA en el instante de armar el chirp. Se llenan en el fill del
 * banco 24 y se imprimen en el informe; ver el comentario largo allá. Los dos
 * DMA están programados con la misma cuenta (AUDIO_BUFFER_SIZE*2 =
 * ADC_BUFFER_SIZE*2 = 3840 palabras), así que las posiciones son directamente
 * comparables sin escalar. */
#define LB_DMA_WORDS    (AUDIO_BUFFER_SIZE * 2)
static volatile uint32_t lb_ndtr_rx = 0, lb_ndtr_tx = 0;

/* Instrumentación del ARMADO de la captura — la carrera que §10 dejó anotada.
 *
 * El síntoma: 1 de cada 25 arranques informaba exactamente 240,00 muestras menos
 * —medio buffer justo— con la MISMA fase de DMA. §9 estableció que la fase
 * determina la latencia uno a uno, así que si la fase no cambió, lo que cambió
 * no es el pipeline: es dónde EMPIEZA la ventana capturada.
 *
 * Por qué puede correrse medio buffer: `lb_capturing = 1` se pone en el fill del
 * banco 24, y Service_Fills procesa las mitades del RX ANTES de los cuatro
 * fills. Al armar, la mitad que el RX acaba de terminar puede estar ya consumida
 * —y entonces la captura abre con la mitad que el DMA está escribiendo ahora— o
 * seguir pendiente porque el borde cayó después del chequeo de arriba —y
 * entonces abre con la anterior, 240 muestras más atrás—. Las dos alternativas
 * están separadas por exactamente medio buffer, que es el tamaño del artefacto.
 *
 * Estos tres registros convierten esa conjetura en dato: qué había pendiente al
 * armar, con qué mitad abrió la captura de verdad, y cuántos bordes de RX
 * pasaron en el medio. */
static volatile uint8_t  lb_arm_pend = 0;   /* mitades de RX pendientes al armar */
static volatile uint8_t  lb_cap_half = 0xFF;/* mitad del RX que ABRIÓ la captura */
static volatile uint32_t lb_arm_rxcb = 0;   /* cb_rx3b_count al armar */
static volatile uint32_t lb_cap_rxcb = 0;   /* cb_rx3b_count al primer copiado */

/** @brief Convierte dos lecturas de NDTR en la fase RX-TX, en muestras.
 *
 * NDTR cuenta hacia ABAJO desde la cuenta programada, así que la posición del
 * DMA es (total - NDTR), y dividida por los canales da el índice de muestra
 * dentro del doble buffer. La resta se normaliza a [0, nsamp) porque es una fase
 * circular: sin envolver, el mismo desfasaje se leería como +470 o como -10
 * según dónde caiga.
 */
static int32_t DMA_PhaseFrom(uint32_t ndtr_rx, uint32_t ndtr_tx)
{
    int32_t nsamp = (int32_t)(LB_DMA_WORDS / AUDIO_CHANNELS);
    int32_t d = (int32_t)((LB_DMA_WORDS - ndtr_rx) / ADC_CHANNELS)
              - (int32_t)((LB_DMA_WORDS - ndtr_tx) / AUDIO_CHANNELS);
    while (d < 0)       d += nsamp;
    while (d >= nsamp)  d -= nsamp;
    return d;
}

/** @brief Fase RX-TX de los DMA, medida AHORA.
 *
 * Este número determina la latencia del pipeline de este nodo — medido sobre 20
 * arranques con mapeo uno a uno (docs/loopback_debug.md §9).
 *
 * Lo importante: sale de dos registros. **No hace falta loopback, ni J13, ni
 * adaptador, ni micrófono.** El cable hizo falta para descubrir la relación, no
 * para usarla: cualquier nodo de una instalación de 10-16 cajas puede reportar
 * el suyo y saber en qué estado arrancó (multinode.md §2, nivel 2).
 *
 * Se lee con las interrupciones apagadas para que ningún ISR caiga entre las dos
 * lecturas — el DMA avanza aunque el CPU esté en otra cosa.
 */
static int32_t DMA_PhaseSamples(void)
{
    uint32_t nrx, ntx;
    uint32_t pm = __get_PRIMASK();
    __disable_irq();
    nrx = __HAL_DMA_GET_COUNTER(&hdma_sai3_b);
    ntx = __HAL_DMA_GET_COUNTER(&hdma_sai2_b);
    __set_PRIMASK(pm);
    return DMA_PhaseFrom(nrx, ntx);
}
/* Resultados por ENTRADA */
static int32_t  lb_lag_q8[LB_NUM_IN];
static int8_t   lb_sign[LB_NUM_IN];
static uint16_t lb_q10[LB_NUM_IN];
static int32_t  lb_peak[LB_NUM_IN];
/* Umbral de pico en escala de 16 bits: MIC_LIVE_THRESH está en 24 bits. */
#define LB_PEAK_MIN   (MIC_LIVE_THRESH >> 8)

/* ===== BARRIDO DE FRECUENCIA por el lazo (Test D) =============================
 *
 * Qué mide: amplitud contra frecuencia de la cadena DAC → ADC, emitiendo un
 * tono por los mismos canales que el LOOPBACK y detectándolo coherentemente en
 * las ocho entradas. Con el cable de J13 puesto, eso es la salida del PCM1690
 * del canal 24 tomada antes del amplificador — así que **no hace falta 24 V ni
 * amplificadores**, que es justamente lo que hoy está bloqueado.
 *
 * ⚠️ Alcance, para no leer de más el resultado. J13 pincha entre la R serie de
 * 1 kΩ y el capacitor de acople de 220 nF, y con la placa sin 24 V el PAM8302A
 * está apagado. O sea que este barrido **NO ve el pasaalto de 723 Hz** de §2.1
 * —ése lo forman el acople y la entrada del amplificador— ni nada del clase D.
 * Lo que se mide es DAC + cable + entrada del ADC. Confirmar los 723 Hz sigue
 * necesitando los adaptadores de §4.2 y alimentación.
 *
 * Por qué detección coherente y no pico. El pico mide señal MÁS ruido, y en los
 * extremos del barrido —donde la respuesta cae— eso es un piso que miente:
 * la curva se aplana en el ruido en vez de seguir bajando. Correlacionando
 * contra seno y coseno de la MISMA frecuencia, el ruido se promedia a cero y la
 * ventana de 9600 muestras da ~40 dB de ganancia de proceso. Además la magnitud
 * sqrt(I²+Q²) no depende de la fase, así que no hay que alinear nada con el
 * retardo del lazo (273,7 muestras, §13).
 *
 * La referencia sale de `sine_table_1khz`, la MISMA tabla y la misma
 * interpolación que usan los modos de tono: si se reimplementara acá, el
 * barrido mediría su propia copia. El error de la interpolación lineal sobre 48
 * puntos son −53 dB de armónicos, y la detección coherente ni los ve. */
#define FS_SETTLE     4800u   /* 100 ms de asentamiento: 6τ del acople de entrada */
#define FS_WINDOW     9600u   /* 200 ms de medición — múltiplo exacto de 240 */
/* Cuentas en la referencia para llamar viva a una entrada. Bajo a propósito:
 * con 100 el barrido de volumen se cortaba en −24 dB —tapando justo el rango
 * operativo de −40 dB (§3)— cuando el piso real de la detección coherente son
 * 1-3 cuentas, medido en las entradas sin cable. Con 10 llega a −45 dB. Debajo
 * de ~15 cuentas el error crece a más de 1 dB: el barrido informa el nivel y
 * quien lo lee decide, en vez de que un umbral decida por él. */
#define FS_LIVE_MIN   10
#define FS_PAUSE_MS   2000u
/* Log-espaciadas de 20 Hz a 20 kHz, con 723 (el pasaalto del amp, §2.1) y 800
 * (el modo 17) agregadas aunque este camino no las vea: si alguna vez se mide
 * con los adaptadores, la grilla es la misma y las corridas se comparan. */
#define FS_NUM_BASE   22
/* Cada punto se mide DOS veces, a f y a f+FS_DF, para sacar el retardo de grupo
 * de la diferencia de fase. La separación no es libre: el retardo se resuelve
 * sin ambigüedad sólo si Δφ < π, o sea τ < 1/(2·Δf). Con 40 Hz eso son 12,5 ms
 * = 600 muestras, contra los ~274 medidos en §13 — 2,2× de margen. Más chico
 * daría más margen pero menos resolución en la fase; más grande promedia el
 * retardo sobre una banda demasiado ancha. */
#define FS_DF         40
#define FS_NUM_PTS    (FS_NUM_BASE * 2)
#define FS_REF_BASE   12      /* 1000 Hz — el 0 dB de la tabla */
static const uint16_t fs_base[FS_NUM_BASE] = {
     20,   30,   45,   63,   90,  125,  180,  250,  355,  500,
    723,  800, 1000, 1400, 2000, 2800, 4000, 5600, 8000,11200,
  16000,20000
};
static volatile uint8_t  fs_enabled = 0;
static volatile uint8_t  fs_idx = 0;
static volatile uint8_t  fs_point_ready = 0;
static volatile uint32_t fs_n = 0;          /* muestras dentro del punto (contexto RX) */
static volatile uint32_t fs_next_ms = 0;
static uint32_t fs_tx_phase = 0;            /* fase del emisor  (contexto de fill) */
static uint32_t fs_bank_ph[4] = {0,0,0,0};  /* fase por banco — ver lb_bank_pos */
static uint32_t fs_rx_phase = 0;            /* fase del detector (contexto RX) */
static int64_t  fs_i[ADC_CHANNELS], fs_q[ADC_CHANNELS];
/* I y Q normalizados, no la magnitud: con los dos se saca amplitud Y fase, y la
 * fase es la que da el retardo de grupo. Guardar sólo el módulo la tiraba. */
static float fs_fi[FS_NUM_PTS][ADC_CHANNELS];
static float fs_fq[FS_NUM_PTS][ADC_CHANNELS];
/* Fase del EMISOR al abrir cada punto. No se fuerza a cero: se anota.
 *
 * El retardo de grupo sale de restar las fases de las dos sub-medidas de un
 * punto, y eso sólo funciona si las dos comparten origen. La primera versión lo
 * conseguía poniendo `fs_tx_phase = 0` al abrir cada punto — y funcionaba para
 * la fase, pero **rompía la amplitud**: medido en A/B contra el mismo firmware
 * sin esa línea, 4 kHz caía 34%, 2 kHz 19% y 1 kHz 10%, de forma perfectamente
 * repetible en 7 barridos, mientras el resto de la tabla no se movía. El
 * mecanismo exacto quedó sin identificar (ver docs/loopback_debug.md §15).
 *
 * Anotar la fase en vez de pisarla da el mismo origen común SIN tocar un solo
 * sample emitido, así que la amplitud queda como estaba y el retardo también
 * sale. La corrección es sumarle 2π·fs_tx0/(48·65536) a la fase detectada. */
static uint32_t fs_tx0[FS_NUM_PTS];

/** @brief Incremento de fase Q16 sobre la tabla de 48 entradas, para fs_freqs[i].
 *  Idéntico al de Tone_NextSample: a 1 kHz da exactamente una entrada por
 *  muestra (48 muestras por ciclo a 48 kHz). Lo usan el emisor y el detector,
 *  así que las dos frecuencias son la misma por construcción y la magnitud no
 *  deriva por más larga que sea la ventana. */
static inline uint16_t FS_FreqOf(uint8_t i) {
  return (uint16_t)(fs_base[i >> 1] + ((i & 1u) ? FS_DF : 0u));
}
static inline uint32_t FS_Inc(uint8_t i) {
  /* El clamp no es paranoia: lo leen el emisor y el detector desde
   * Service_Fills, que corre ANIDADO dentro de UART_Print mientras el informe
   * se imprime. Ahí fs_idx vale FS_NUM_PTS por un rato y sin este clamp se
   * indexa la tabla fuera de rango — HardFault con BFAR justo pasado el final
   * del DTCM, que es exactamente como se descubrió. */
  if (i >= FS_NUM_PTS) i = 0;
  return ((uint32_t)FS_FreqOf(i) << 16) / 1000u;
}
/** @brief La frecuencia REAL del punto, no la nominal.
 *  `inc` es entero, así que 1040 Hz pedidos son 1039,99 Hz emitidos. La
 *  diferencia es despreciable para la amplitud y NO lo es para el retardo de
 *  grupo, que divide por Δf: usar el nominal metería un sesgo de escala. */
static inline float FS_FreqReal(uint8_t i) {
  return (float)FS_Inc(i) * 1000.0f / 65536.0f;
}
/** @brief Un punto de la tabla de seno, interpolado, con la fase en Q16. */
static inline int32_t FS_Sine(uint32_t ph) {
  uint32_t idx = ph >> 16;
  int32_t  fr  = (int32_t)(ph & 0xFFFF);
  int32_t  s0  = sine_table_1khz[idx];
  int32_t  s1  = sine_table_1khz[(idx + 1u) % 48u];
  return s0 + (int32_t)(((int64_t)(s1 - s0) * fr) >> 16);
}
#define FS_PHASE_MAX  (48u << 16)
_Static_assert(FS_SETTLE % AUDIO_BUFFER_SAMPLES == 0
               && FS_WINDOW % AUDIO_BUFFER_SAMPLES == 0,
               "los puntos tienen que cerrar en un borde de mitad");
_Static_assert(FS_REF_BASE < FS_NUM_BASE, "la referencia esta fuera de la tabla");
_Static_assert(FS_NUM_PTS <= 255, "fs_idx es uint8_t");

/* Extra right-shift applied to the -6dBFS sine table.
 * Now 0: the PAM8302A clipping headroom is handled by the PCM1690's own
 * -24dB digital attenuation (registers 72-79, PCM1690_ATTEN_DEFAULT). The
 * -6dBFS table → -30dBFS at the DAC output → clean ~0.5W at the speaker.
 * Kept as a knob for quick test-tone level changes (each step = -6dB). */
static volatile uint8_t test_tone_shift = 0;

// DS10: RST and AMUTEI are shared bus signals (PC11/PC12) — no per-chip GPIO.
// PCM1690_Init receives NULL pins; RST/MUTE are controlled before calling Init.
static const uint8_t dac_addrs[4] = {
    PCM1690_ADDR_DAC0, PCM1690_ADDR_DAC1, PCM1690_ADDR_DAC2, PCM1690_ADDR_DAC3
};

/* Estado del banco de amplificadores (pin SD, PD3). ARRANCA EN 0: tras el
 * incidente del 2026-08-04 (reinicio al primer tono del walker, con el buck
 * al limite y 660uF de bulk en vez de 880) los amps ya NO se habilitan solos
 * al bootear — se prenden por comando (POST /amps) una vez que la placa esta
 * estable. Asi un problema de alimentacion no deja la placa en loop de
 * reinicio imposible de diagnosticar. */
static volatile uint8_t amps_enabled = 0;
/* 1 = un POST /beam pisó los flags de modo: ui_state_idx ya no describe lo que
 * suena. Se publica en el status (STATE_F_CUSTOM) para que la UI no mienta. */
static volatile uint8_t custom_beam_active = 0;

// EEPROM deferred save flag (set when config changes, save in low-priority window)
static volatile uint8_t eeprom_save_pending = 0;
// Config-save debounce: HAL_GetTick() deadline (0 = nothing pending). Any config
// change (modo K2, /volume, CMD_UPDATE_CFG) re-arma el deadline; la EEPROM se
// escribe UNA vez, 3s después del último cambio. Un slider arrastrado ya no
// genera una ráfaga de saves de ~12ms (≈6 underruns cada uno).
#define CFG_SAVE_DEBOUNCE_MS 3000
static volatile uint32_t cfg_save_due_tick = 0;

static inline void Arm_Config_Save(void) {
  cfg_save_due_tick = HAL_GetTick() + CFG_SAVE_DEBOUNCE_MS;
  if (cfg_save_due_tick == 0) cfg_save_due_tick = 1;  // 0 = "nothing pending"
}

// LED dimmer state machine (DS10 TIM3 4-channel: CH1=D4/PA6 Yellow, CH2=D5/PA7 Red,
//                                                  CH3=D6/PB0 Green,  CH4=D7/PB1 White)
// 4 phases of 3s each (12s full cycle): one LED fade-in 0→max while others off.
#define DIMMER_STEPS     300UL
#define DIMMER_STEP_CYC  (480000000UL / 100UL)  // 10ms @ 480MHz
#define DIMMER_ARR       999UL
#define DIMMER_NUM_PHASES 4
static uint8_t  dimmer_phase    = 0;
static uint32_t dimmer_step     = 0;
static uint32_t last_dimmer_dwt = 0;
static const uint32_t dimmer_channels[DIMMER_NUM_PHASES] = {
    TIM_CHANNEL_1,  // D4 Yellow PA6
    TIM_CHANNEL_2,  // D5 Red    PA7
    TIM_CHANNEL_3,  // D6 Green  PB0
    TIM_CHANNEL_4,  // D7 White  PB1
};

// ===== ISR → Main loop deferred processing flags =====
// ISR sets flag, main loop processes buffer and clears flag
// This keeps ISR execution < 20 cycles (vs ~50000 with sinf())
/* Cuántas veces una pasada de Service_Fills llenó LAS DOS mitades del mismo
 * banco. Importa porque el avance del emisor (chirp del modo 19, tono del 20)
 * vive en el banco 24: en una pasada doble, los bancos 0/8/16 emiten el mismo
 * trozo dos veces mientras el 24 avanza dos veces, y quedan 240 muestras
 * desfasados. Justo el error que el modo 19 existe para medir. */
static volatile uint32_t dbl_fill[4] = {0,0,0,0};
static volatile uint8_t need_fill_first_half = 0;
static volatile uint8_t need_fill_second_half = 0;
static volatile uint8_t need_fill_2a_first_half = 0;
static volatile uint8_t need_fill_2a_second_half = 0;
static volatile uint8_t need_fill_1b_first_half = 0;
static volatile uint8_t need_fill_1b_second_half = 0;
static volatile uint8_t need_fill_2b_first_half = 0;
static volatile uint8_t need_fill_2b_second_half = 0;
static volatile uint8_t rx_3b_first_half_ready = 0;
static volatile uint8_t rx_3b_second_half_ready = 0;

/* Underrun = bandera puesta y no servida antes de la próxima ISR, o sea un
 * deadline de fill perdido = medio buffer repetido = un pop.
 *
 * Hay DOS contadores a propósito. `underrun_count` se pone en cero cada 60 s,
 * porque el resumen periódico informa "por ciclo". `underrun_total` NO se
 * resetea nunca y es el que va a la telemetría: sin él, `/status` parecía
 * acumulativo pero retrocedía sola cada minuto, y cualquier medición por
 * diferencia daba números imposibles (negativos incluidos). Lo mismo para los
 * errores del SAI. */
static volatile uint32_t underrun_count = 0;
static volatile uint32_t underrun_total = 0;
static volatile uint32_t sai_error_total = 0;

// Callback counters for debug
static volatile uint32_t half_complete_count = 0;
static volatile uint32_t full_complete_count = 0;
static volatile uint32_t error_count = 0;
static volatile uint32_t last_sai_error = 0;

// Per-SAI-block callback counters (Half+Full) — to localize silent channels.
// If 1B/2A/2B don't advance, those DACs never get the tone (ch8-31 silent).
static volatile uint32_t cb_a1_count = 0;
static volatile uint32_t cb_1b_count = 0;
static volatile uint32_t cb_2a_count = 0;
static volatile uint32_t cb_2b_count = 0;
// RX (ADC) DMA callback counter — if this does NOT advance ~100/s, the SAI3_B
// RX DMA is stalled and audio_buffer_3b_rx is frozen (explains "input does
// nothing"). This is the decisive ADC data-path liveness check.
static volatile uint32_t cb_rx3b_count = 0;

// ===== Latency & Jitter measurement via DWT cycle counter =====
// DWT->CYCCNT runs at CPU clock (480MHz → 2.08ns/cycle)
static volatile uint32_t last_half_isr_cycles = 0;
static volatile uint32_t last_full_isr_cycles = 0;

// Jitter tracking: callback period variation (Half + Full combined)
static volatile uint32_t jitter_min_cycles = 0xFFFFFFFF;
static volatile uint32_t jitter_max_cycles = 0;
static volatile uint64_t jitter_sum_cycles = 0;
static volatile uint32_t jitter_sample_count = 0;

/* Costo TOTAL de los 4 bancos (DS11): el min/avg/max de abajo mide SOLO los
 * fills de SAI1_A, así que con 32 canales subrepresenta ~4× el trabajo real.
 * fill_all_cycles acumula TODOS los fills; el heartbeat lo derivada a %CPU.
 * Presupuesto: 480M ciclos/s → cada 1% son 4.8M ciclos/s. */
static volatile uint64_t fill_all_cycles = 0;
/* Último % de CPU en fills (×10), calculado en el heartbeat y publicado por
 * telemetría: sin esto, "este modo no entra en presupuesto" sólo se podía
 * diagnosticar con el cable serie enchufado. */
static volatile uint16_t cpu_fills_x10 = 0;
/* Offset entre bancos: timestamp del último Half callback de cada master.
 * SAI1_A y SAI2_A son masters asíncronos entre sí — si arrancan separados,
 * ese desfase queda fijo para siempre. Se reporta en la línea SYNC. */
static volatile uint32_t cb_a1_half_dwt = 0;
static volatile uint32_t cb_2a_half_dwt = 0;

// Processing latency: fill buffer + cache clean
static volatile uint32_t fill_min_cycles = 0xFFFFFFFF;
static volatile uint32_t fill_max_cycles = 0;
static volatile uint64_t fill_sum_cycles = 0;
static volatile uint32_t fill_sample_count = 0;

// UART TX buffer
static char uart_buf[128];

// Test cycle counter (for 60s status reports)
static uint32_t test_cycle = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C3_Init(void);
static void MX_UART7_Init(void);
static void MX_SAI1_Init(void);
static void MX_UART4_Init(void);
static void MX_SAI2_Init(void);
static void MX_SAI3_Init(void);
static void MX_ADC3_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
static int32_t Read_CPU_Temperature(void);
static void Fill_Audio_Beamform(int32_t *buffer, uint32_t samples, uint32_t ch_offset);
static void Process_ADC_Half(int32_t *rx_buf, uint32_t samples);
static void Service_Fills(void);
static void Set_In_Gain(uint8_t db);
static void Set_Amps(uint8_t on);
static void Config_LoadDefaults(void);
static void Config_FactoryReset(void);
static void UART_Print(const char *str);
static void Check_Memory_Placement(void);
static void Beam_UpdateDelays(int16_t az_deg);
static void Beam_InitTxGeometry(void);
static void Beam_UpdateTxFocus(int16_t fx_mm, int16_t fy_mm, int16_t fz_mm);
static void Beam_SetTxFocus(int16_t fx_mm, int16_t fy_mm, int16_t fz_mm);
static void Beam_UpdateMFocus(uint8_t src, uint16_t f_hz,
                              int16_t fx_mm, int16_t fy_mm, int16_t fz_mm);
static void MFocus_ApplyDefaults(void);
static void MFocusIn_ApplyDefaults(void);
static uint8_t Apply_Master_Volume(int8_t db);
static void UI_ApplyState(uint8_t idx);
static void UI_ApplyCustomBeam(const ds_beam_req_t *req);
static void Cal_Init(void);
static void Cal_StartScan(void);
static void Cal_Process(void);
static void LB_Process(void);
static void FS_Process(void);
static void OTA_ServiceRequests(void);
static void Health_Run(ds_health_t *h);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief Servicia todos los fills pendientes (RX del ADC + los 4 bancos TX).
 *
 * Extraido del main loop para poder llamarlo TAMBIEN desde UART_Print mientras
 * espera al UART. Un heartbeat de ~238 chars a 115200 bloquea ~20 ms, mas que el
 * deadline de 10 ms del fill, y cada deadline perdido es un medio buffer repetido
 * en los 32 canales a la vez = un pop audible una vez por segundo. Con esto el
 * print deja de robarle tiempo al audio.
 *
 * El guard de reentrada es defensivo: hoy ningun fill imprime, pero si alguno lo
 * hiciera en el futuro esto evita la recursion en vez de desbordar el stack.
 */
/* Overrides de los hooks __weak de eeprom_config.c y uart_protocol.c: durante
 * los ~12 ms de un guardado en EEPROM y los ~5,2 ms de una trama de status,
 * seguir sirviendo los fills en vez de dejar morir los deadlines. Los tres
 * bloqueos largos del sistema (print, EEPROM, status) quedan así cooperativos. */
void EEPROM_Yield(void)        { Service_Fills(); }
void UART_Protocol_Yield(void) { Service_Fills(); }
/* Lo mismo para la verificación del CRC de una imagen OTA (~1,4 ms de un tirón
 * sobre 80 kB). El borrado de un sector NO se puede cooperativizar: ahí el CPU
 * queda en espera activa sobre el registro de estado del flash. Por eso el
 * borrado se hace con los amplificadores ya apagados. */
void OTA_Yield(void)           { Service_Fills(); }

/* ============================================================
 * Actualización de firmware por WiFi — lado main loop
 *
 * Recibe la imagen del ESP32 y la deja en el banco 2 (ota.c). No la aplica: eso
 * lo hace el bootloader en el próximo arranque, y por eso una carga que sale
 * mal —CRC que no cierra, WiFi que se corta, la corriente que se va— no cuesta
 * absolutamente nada: el firmware que está corriendo vive en el banco 1 y este
 * código no lo puede tocar. Ver ota.h.
 *
 * El equipo queda MUDO durante la carga, a propósito: el borrado del sector
 * bloquea el CPU cientos de milisegundos y eso son decenas de deadlines de fill
 * perdidos. En vez de dejar que salga por los parlantes como una serie de pops,
 * se apagan los amplificadores en FW_BEGIN. Vuelven con el reinicio (arrancan
 * apagados) y se prenden a mano como siempre.
 * ============================================================ */

/* Número de tramo esperado. La comprobación no es paranoia: sin ella, un ACK
 * que se pierda hace que el ESP32 reenvíe el mismo tramo y el STM32 lo escriba
 * DOS VECES, corriendo todo lo que sigue medio tramo — con el CRC fallando al
 * final y sin ninguna pista de dónde. */
static volatile int16_t sync_us_last = 0;   /* último SYNC entre bancos, ver el heartbeat */

/* ============================================================
 * Captura remota del ADC
 *
 * Guarda las 8 entradas crudas para bajarlas y analizarlas afuera. Hasta ahora
 * medir era mirar números por el serie: el modo 19 hace una medición completa y
 * devuelve UN pico y UN retardo, y todo lo demás —la forma de onda, el
 * contenido en frecuencia, lo que pasa entre canales— se calculaba adentro y se
 * tiraba. Con las muestras afuera, la placa deja de tener que saber de antemano
 * qué se le va a preguntar.
 *
 * Vive en la AXI SRAM, que estaba ENTERA sin usar: 256 kB acá no le sacan nada
 * a nadie. En DTCM no entraría (quedan ~23 kB) y en RAM_D2 competiría con los
 * buffers de DMA.
 *
 * Se guarda en int32 con los 24 bits del ADC y se transmite en int16 con
 * autorrango — ver ds_cap_info_t. Guardar la precisión completa y decidir el
 * desplazamiento DESPUÉS, con el pico ya conocido, es lo que permite capturar
 * tanto un chirp a fondo de escala como el piso de ruido de 437 cuentas.
 * ============================================================ */
#define CAP_MAX_SAMPLES   8192      /* 170 ms a 48 kHz; 8192×8×4 = 256 kB */

typedef enum { CAP_IDLE = 0, CAP_FILL, CAP_READY } cap_state_t;

static int32_t cap_buf[CAP_MAX_SAMPLES][ADC_CHANNELS] __attribute__((section(".axi_ram")));
static volatile cap_state_t cap_state = CAP_IDLE;
static volatile uint16_t    cap_want = 0;      /* muestras pedidas */
static volatile uint16_t    cap_count = 0;     /* muestras ya guardadas */
static uint8_t              cap_shift = 0;     /* autorrango, calculado al llenar */
static int32_t              cap_peak = 0;
static uint8_t              cap_info_due = 0;  /* hay un CAP_START esperando respuesta */

static uint16_t ota_seq_next = 0;

/* Reinicio diferido tras una carga exitosa. No se reinicia en el acto: primero
 * tiene que salir el ACK, y después el ESP32 tiene que contestarle al navegador
 * que la carga terminó. Si el STM32 se reinicia antes, la página muestra un
 * error justo cuando todo salió bien. */
static uint32_t ota_reboot_at = 0;

/* Una carga que se queda a mitad —el ESP32 se reinicia, el navegador se cierra,
 * el WiFi se cae— dejaría la sesión abierta para siempre, con el equipo mudo y
 * /status informando "recibiendo 43%" indefinidamente. No es peligroso, pero es
 * un estado del que nadie sabe cómo salir. Treinta segundos son un orden de
 * magnitud más que el tramo más lento. */
#define OTA_IDLE_TIMEOUT_MS  30000

/* ============================================================
 * Autodiagnóstico — lo que los chips REALMENTE tienen adentro
 *
 * Vuelve a leer por I2C la configuración de los cuatro PCM1690, del AK5538VN y
 * de la EEPROM. No es telemetría: es una prueba activa, y por eso existe.
 *
 * El caso que justifica todo esto: un PCM1690 al que se le fue el registro de
 * formato deja de decodificar el TDM y saca ruido o silencio — y sin embargo
 * `underruns` sigue en cero, el CPU sigue igual y el /status entero se ve
 * perfecto. Ningún contador pasivo lo ve. Sólo se nota escuchando, o leyendo
 * los registros.
 *
 * Se devuelven los VALORES, no un veredicto: un rojo que no se puede discutir
 * es un rojo en el que no se confía.
 *
 * ⚠️ Bloquea unos pocos ms de I2C. Corre en el main loop, nunca en una ISR.
 * ============================================================ */
static void Health_Run(ds_health_t *h)
{
  memset(h, 0, sizeof(*h));

  for (uint32_t d = 0; d < 4; d++) {
    uint8_t v = 0;
    if (PCM1690_ReadReg(&hi2c1, dac_addrs[d], PCM1690_REG_FORMAT, &v) == PCM1690_OK) {
      h->dac_present |= (uint8_t)(1u << d);
      h->dac_fmt[d] = v;
      if (PCM1690_ReadReg(&hi2c1, dac_addrs[d], PCM1690_REG_ATTEN_CH1, &v) == PCM1690_OK)
        h->dac_att[d] = v;
    }
    Service_Fills();       /* cada lectura son ~200 µs; igual no se dejan morir */
  }

  /* AK5538VN por I2C3. R02/R03/R04 son los que se escriben en el arranque; que
   * sigan ahí confirma que el chip no se reinició por su cuenta. */
  for (uint8_t r = 1; r <= 4; r++) {
    uint8_t v = 0xEE;
    if (HAL_I2C_Mem_Read(&hi2c3, AK5538_ADDR, r, I2C_MEMADD_SIZE_8BIT, &v, 1, 50) == HAL_OK) {
      h->ak_present = 1;
      h->ak_regs[r - 1] = v;
    }
  }
  Service_Fills();

  h->eeprom_present = EEPROM_IsAvailable() ? 1 : 0;
  h->eeprom_ver     = EEPROM_CFG_VERSION;
  h->sync_us        = sync_us_last;
}

static void OTA_ServiceRequests(void)
{
  static uint32_t ota_last_ms = 0;

  if (UART_Protocol_GetFwAbort()) {
    OTA_Abort();
    UART_Print("OTA: carga cancelada\r\n");
  }

  if (OTA_GetState() == OTA_ST_RECV && ota_last_ms != 0 &&
      (HAL_GetTick() - ota_last_ms) > OTA_IDLE_TIMEOUT_MS) {
    OTA_Abort();
    ota_last_ms = 0;
    UART_Print("OTA: la carga se cortó y no volvió — sesión abandonada\r\n");
  }

  static uint8_t pl[PROTO_MAX_PAYLOAD];
  uint8_t  cmd = 0;
  uint16_t n   = 0;
  if (!UART_Protocol_TakeFwRequest(&cmd, pl, &n)) return;

  ota_last_ms = HAL_GetTick();

  uint8_t ok = 0;

  if (cmd == CMD_FW_BEGIN && n == sizeof(ds_fw_begin_t)) {
    ds_fw_begin_t b;
    memcpy(&b, pl, sizeof(b));
    Set_Amps(0);                       /* mudo antes de bloquear en el borrado */
    ok = (OTA_Begin(b.len, b.fw_id, b.build) == 0);
    ota_seq_next = 0;
    if (ok) {
      snprintf(uart_buf, sizeof(uart_buf),
               "OTA: recibiendo %lu bytes (id %08lX) — amps apagados\r\n",
               (unsigned long)b.len, (unsigned long)b.fw_id);
    } else {
      snprintf(uart_buf, sizeof(uart_buf), "OTA: no arranco — %s\r\n", OTA_GetError());
    }
    UART_Print(uart_buf);

  } else if (cmd == CMD_FW_DATA && n >= 3) {
    uint16_t seq = (uint16_t)pl[0] | ((uint16_t)pl[1] << 8);
    if (seq == ota_seq_next) {
      ok = (OTA_Data(&pl[2], (uint16_t)(n - 2)) == 0);
      if (ok) ota_seq_next++;
      else {
        snprintf(uart_buf, sizeof(uart_buf), "OTA: fallo en el tramo %u — %s\r\n",
                 (unsigned)seq, OTA_GetError());
        UART_Print(uart_buf);
      }
    } else if ((uint16_t)(seq + 1U) == ota_seq_next) {
      /* Reenvío del tramo anterior: se perdió NUESTRO ack, no sus datos. Se
       * vuelve a ACKear sin escribir nada. */
      ok = 1;
    } else {
      snprintf(uart_buf, sizeof(uart_buf),
               "OTA: tramo fuera de orden (llego %u, esperaba %u) — carga abortada\r\n",
               (unsigned)seq, (unsigned)ota_seq_next);
      UART_Print(uart_buf);
      OTA_Abort();
    }

  } else if (cmd == CMD_FW_END && n == 4) {
    uint32_t crc = (uint32_t)pl[0]        | ((uint32_t)pl[1] << 8)
                 | ((uint32_t)pl[2] << 16) | ((uint32_t)pl[3] << 24);
    ok = (OTA_End(crc) == 0);
    if (ok) {
      UART_Print("OTA: imagen verificada. Reiniciando para aplicarla...\r\n");
      ota_reboot_at = HAL_GetTick() + 1500;
    } else {
      snprintf(uart_buf, sizeof(uart_buf), "OTA: imagen RECHAZADA — %s\r\n", OTA_GetError());
      UART_Print(uart_buf);
    }
  }

  UART_Protocol_FwReply(ok);
}

static void Service_Fills(void) {
  static volatile uint8_t in_service = 0;
  if (in_service) return;
  in_service = 1;

  // ===== Deferred buffer fill: beamform from delay line =====
  // ISR sets flag, we process here in main context.
  //
  // SAI3_B RX must run BEFORE the 4 TX fills below: all 4 TX groups read
  // delay_write_pos independently, so if RX advanced it between two TX
  // fills within the same pass, those groups would read different time
  // windows — a phase skew across DAC banks that's silent in passthrough
  // (delay=0) but would break beamforming once real per-channel delays
  // are in use. Processing RX first makes wp stable for the rest of
  // this loop iteration, so all 4 TX fills see the same snapshot.
  if (rx_3b_first_half_ready) {
    rx_3b_first_half_ready = 0;
    __DSB();
    Process_ADC_Half(&audio_buffer_3b_rx[0], ADC_BUFFER_SAMPLES);
  }
  if (rx_3b_second_half_ready) {
    rx_3b_second_half_ready = 0;
    __DSB();
    Process_ADC_Half(&audio_buffer_3b_rx[ADC_BUFFER_SIZE], ADC_BUFFER_SAMPLES);
  }

  // SAI1_A: channels 0-7
  uint8_t did_first_0 = 0;
  if (need_fill_first_half) {
    need_fill_first_half = 0;
    did_first_0 = 1;
    __DSB();
    uint32_t t0 = DWT->CYCCNT;
    Fill_Audio_Beamform(&audio_buffer[0], AUDIO_BUFFER_SAMPLES, 0);
    __DSB();
    uint32_t dt = DWT->CYCCNT - t0;
    if (dt < fill_min_cycles) fill_min_cycles = dt;
    if (dt > fill_max_cycles) fill_max_cycles = dt;
    fill_sum_cycles += dt;
    fill_all_cycles += dt;
    fill_sample_count++;
  }
  if (need_fill_second_half && did_first_0) dbl_fill[0]++;
  if (need_fill_second_half) {
    need_fill_second_half = 0;
    __DSB();
    uint32_t t0 = DWT->CYCCNT;
    Fill_Audio_Beamform(&audio_buffer[AUDIO_BUFFER_SIZE], AUDIO_BUFFER_SAMPLES, 0);
    __DSB();
    uint32_t dt = DWT->CYCCNT - t0;
    if (dt < fill_min_cycles) fill_min_cycles = dt;
    if (dt > fill_max_cycles) fill_max_cycles = dt;
    fill_sum_cycles += dt;
    fill_all_cycles += dt;
    fill_sample_count++;
  }

  // SAI1_B: channels 8-15 (DAC1 U3)
  uint8_t did_first_1 = 0;
  if (need_fill_1b_first_half) {
    need_fill_1b_first_half = 0;
    did_first_1 = 1;
    __DSB();
    uint32_t t0 = DWT->CYCCNT;
    Fill_Audio_Beamform(&audio_buffer_1b[0], AUDIO_BUFFER_SAMPLES, 8);
    __DSB();
    fill_all_cycles += DWT->CYCCNT - t0;
  }
  if (need_fill_1b_second_half && did_first_1) dbl_fill[1]++;
  if (need_fill_1b_second_half) {
    need_fill_1b_second_half = 0;
    __DSB();
    uint32_t t0 = DWT->CYCCNT;
    Fill_Audio_Beamform(&audio_buffer_1b[AUDIO_BUFFER_SIZE], AUDIO_BUFFER_SAMPLES, 8);
    __DSB();
    fill_all_cycles += DWT->CYCCNT - t0;
  }

  // SAI2_A: channels 16-23 (DAC2 U1)
  uint8_t did_first_2 = 0;
  if (need_fill_2a_first_half) {
    need_fill_2a_first_half = 0;
    did_first_2 = 1;
    __DSB();
    uint32_t t0 = DWT->CYCCNT;
    Fill_Audio_Beamform(&audio_buffer_2a[0], AUDIO_BUFFER_SAMPLES, 16);
    __DSB();
    fill_all_cycles += DWT->CYCCNT - t0;
  }
  if (need_fill_2a_second_half && did_first_2) dbl_fill[2]++;
  if (need_fill_2a_second_half) {
    need_fill_2a_second_half = 0;
    __DSB();
    uint32_t t0 = DWT->CYCCNT;
    Fill_Audio_Beamform(&audio_buffer_2a[AUDIO_BUFFER_SIZE], AUDIO_BUFFER_SAMPLES, 16);
    __DSB();
    fill_all_cycles += DWT->CYCCNT - t0;
  }

  // SAI2_B: channels 24-31 (DAC3 U4)
  uint8_t did_first_3 = 0;
  if (need_fill_2b_first_half) {
    need_fill_2b_first_half = 0;
    did_first_3 = 1;
    __DSB();
    uint32_t t0 = DWT->CYCCNT;
    Fill_Audio_Beamform(&audio_buffer_2b[0], AUDIO_BUFFER_SAMPLES, 24);
    __DSB();
    fill_all_cycles += DWT->CYCCNT - t0;
  }
  if (need_fill_2b_second_half && did_first_3) dbl_fill[3]++;
  if (need_fill_2b_second_half) {
    need_fill_2b_second_half = 0;
    __DSB();
    uint32_t t0 = DWT->CYCCNT;
    Fill_Audio_Beamform(&audio_buffer_2b[AUDIO_BUFFER_SIZE], AUDIO_BUFFER_SAMPLES, 24);
    __DSB();
    fill_all_cycles += DWT->CYCCNT - t0;
  }

  in_service = 0;
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* Tabla de vectores en 0x08020000. Desde que existe la actualización por WiFi
   * la aplicación NO vive en el principio de la flash: los primeros 128 kB son
   * del bootloader (ver ota.h). El bootloader ya deja VTOR apuntando acá antes
   * de saltar, pero se repite por dos motivos, y los dos pasan seguido: correr
   * la app directamente desde el debugger sin pasar por el bootloader, y
   * flashearla sola con el ST-Link. Sin esto, la primera interrupción salta a
   * la tabla del bootloader — o sea, a cualquier lado.
   *
   * Va antes de HAL_Init() porque después ya hay SysTick corriendo. */
  SCB->VTOR = 0x08020000UL;
  __DSB();

  /* I-cache ON. El M7 corre a 480 MHz pero la flash tiene 4 wait states, así
   * que sin caché de instrucciones cada vuelta del lazo de fill paga el fetch.
   * Medido 2026-08-13 (modo 7): 716856 ciclos para 480 muestras × 8 canales =
   * 186 ciclos por muestra de salida, para ~30 instrucciones de trabajo real.
   * No hay problema de coherencia: el código no se automodifica.
   *
   * El D-cache sigue APAGADO a propósito: los buffers DMA en RAM_D2 exigirían
   * mantenimiento de caché en cada callback, y el intento anterior
   * (SCB_CleanDCache_by_Addr sin D-cache habilitado) terminó en HardFault. */
  SCB_EnableICache();
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_I2C3_Init();
  MX_UART4_Init();   /* UART4 (PA0/PA1) — ESP32 binary protocol link */
  MX_UART7_Init();
  MX_SAI1_Init();
  MX_SAI2_Init();
  MX_SAI3_Init();
  /* USER CODE BEGIN 2 */

  // Startup message
  UART_Print("\r\n=== DS11 Beamforming Audio (32ch) ===\r\n");
  /* Qué binario es y desde dónde corre. Con actualización por WiFi las dos
   * cosas dejaron de ser obvias: la imagen pudo haberla subido otro. */
  snprintf(uart_buf, sizeof(uart_buf), "firmware %08lX  build %lu  @ 0x%08lX\r\n",
           (unsigned long)fw_id, (unsigned long)fw_build, (unsigned long)SCB->VTOR);
  UART_Print(uart_buf);

  /* Revisión de silicio — NO es cosmético.
   *
   * Errata ES0392 §2.2.21: "VOS0 voltage scaling level is not available on
   * silicon revision Y and W devices. This limits the maximum CPU frequency to
   * 400 MHz." Workaround: usar revisión V o X.
   *
   * Este firmware configura VOS0 + 480 MHz. Sobre un chip revisión Y o W eso
   * queda FUERA DE ESPECIFICACIÓN: la escritura de VOS0 no toma efecto y el
   * core termina corriendo a 480 MHz con el voltaje de VOS1. La placa de banco
   * aguantó 19,6 h con Err=0, lo que indica que su silicio es V o X — pero eso
   * NO está garantizado para las demás placas de la tirada ni para tiradas
   * futuras: JLCPCB puede entregar otro date code sin avisar.
   *
   * Por eso se imprime en cada arranque. Si alguna vez sale Y o W, hay que
   * bajar a 400 MHz (PLL1 P=2 con N=160) antes de confiar en nada más. */
  {
    uint32_t rev = (DBGMCU->IDCODE >> 16) & 0xFFFFu;
    const char *name, *verdict;
    switch (rev) {
      case 0x1001: name = "Z"; verdict = "!! SIN VOS0 — 480MHz FUERA DE SPEC"; break;
      case 0x1003: name = "Y"; verdict = "!! SIN VOS0 — 480MHz FUERA DE SPEC"; break;
      case 0x2001: name = "X"; verdict = "OK (VOS0 disponible)";               break;
      case 0x2003: name = "V"; verdict = "OK (VOS0 disponible)";               break;
      default:     name = "?"; verdict = "DESCONOCIDA — verificar ES0392";     break;
    }
    sprintf(uart_buf, "Silicio: rev %s (REV_ID=0x%04lX) — %s\r\n",
            name, (unsigned long)rev, verdict);
    UART_Print(uart_buf);
  }

  Check_Memory_Placement();
  UART_Print("CPU: 480MHz (VOS0), HCLK: 240MHz, APB: 120MHz\r\n");
  UART_Print("PLL3: 24.576MHz MCLK (512xfS) — MckOverSampling=ENABLE\r\n");
  UART_Print("SAI1_A/B → 4xPCM1690 (32ch TDM8), SAI3_B ← AK5538VN (8ch TDM256)\r\n");
  UART_Print("MODE: ADC monitor (default). K2: monitor -> walker -> RX beam -> TX focus -> AUTOFOCUS\r\n\r\n");

  // === ADC3 temperature sensor ===
  MX_ADC3_Init();

  // === TIM3 PWM — LED dimmer (4ch: D4/PA6, D5/PA7, D6/PB0, D7/PB1) ===
  MX_TIM3_Init();

  // === Beamforming init ===
  // Zero delay lines
  memset(delay_line, 0, sizeof(delay_line));
  delay_write_pos = 0;

  Config_LoadDefaults();
  Beam_InitTxGeometry();
  Cal_Init();                       // AUTOFOCUS chirp reference
  MFocusIn_ApplyDefaults();         // MULTIFOCUS: filas 0-7 válidas (modo 15)
  MFocus_ApplyDefaults();           // filas 0-3 = presets de tonos (modo 14)
  UART_Print("Beamforming initialized (delay lines zeroed, gains=max)\r\n");

  // === EEPROM init + load saved config ===
  {
    UART_Print("EEPROM: Initializing I2C4 AF6 (PB6/PB7)...\r\n");
    HAL_StatusTypeDef i2c_eep_st = EEPROM_Init();
    sprintf(uart_buf, "EEPROM: I2C4 init=%d, available=%u\r\n", (int)i2c_eep_st, EEPROM_IsAvailable());
    UART_Print(uart_buf);

    if (EEPROM_IsAvailable()) {
      eeprom_status_t eep_status = EEPROM_LoadConfig(&beam_config, sizeof(beam_config));
      if (eep_status == EEPROM_OK) {
        UART_Print("EEPROM: Config loaded OK\r\n");
        sprintf(uart_buf, "  master_gain=%u mute=%u mutes=0x%08lX\r\n",
                beam_config.master_gain, beam_config.mute_global, beam_config.mutes);
        UART_Print(uart_buf);
        /* Las POLARIDADES medidas se restauran SIEMPRE: son del cableado, no
         * del punto donde estaba el micrófono, así que valen para todos los
         * modos. Los RETARDOS no — esos son un foco, y se recuperan a pedido
         * con el modo 18. Distinguirlos es la razón de que la calibración se
         * pueda persistir sin que arruine el resto de los modos. */
        if (beam_config.cal_valid) {
          uint32_t invertidos = 0;
          for (uint32_t k = 0; k < BEAM_NUM_CHANNELS; k++) {
            speaker_sign[k] = beam_config.cal_sign[k];
            if (speaker_sign[k] < 0) invertidos++;
          }
          sprintf(uart_buf, "  AUTOFOCUS: calibracion guardada restaurada "
                  "(%lu parlantes con polaridad invertida). Foco medido "
                  "disponible en el modo 18.\r\n", (unsigned long)invertidos);
          UART_Print(uart_buf);
        }
      } else if (eep_status == EEPROM_ERR_MAGIC) {
        UART_Print("EEPROM: No saved config (first boot) — using defaults\r\n");
      } else if (eep_status == EEPROM_ERR_CRC) {
        UART_Print("EEPROM: CRC error — using defaults\r\n");
      } else {
        sprintf(uart_buf, "EEPROM: Load error %d — using defaults\r\n", (int)eep_status);
        UART_Print(uart_buf);
      }
    } else {
      UART_Print("EEPROM: Not detected on I2C4 — using defaults\r\n");
    }
  }

  // === STEP 1: Pre-fill all TX buffers with silence, start SAI DMA (brings MCLK up) ===
  memset(audio_buffer, 0, sizeof(audio_buffer));
  memset(audio_buffer_1b, 0, sizeof(audio_buffer_1b));
  memset(audio_buffer_2a, 0, sizeof(audio_buffer_2a));
  memset(audio_buffer_2b, 0, sizeof(audio_buffer_2b));
  __DSB();

  /* Arranque de los 4 bloques SIN NADA EN EL MEDIO — crítico en DS11.
   *
   * SAI1_A y SAI2_A son dos masters ASÍNCRONOS entre sí: cada uno arranca su
   * propio stream cuando se le pone SAIEN, y ese offset inicial no se corrige
   * nunca (comparten PLL3 → misma frecuencia, pero no fase). Antes había un
   * UART_Print de ~41 chars (≈3.6 ms a 115200, bloqueante) entre los dos
   * arranques: los bancos ch0-15 y ch16-31 quedaban con sus ciclos de DMA
   * desfasados milisegundos.
   *
   * Eso rompe el invariante del fix de 2026-07-02 (procesar RX antes de los 4
   * fills para que todos compartan el mismo delay_write_pos): con los bancos
   * desfasados, sus callbacks caen en pasadas distintas del main loop y toman
   * valores de wp que difieren hasta en 480 muestras → offset acústico de
   * hasta ~10 ms (3.4 m de camino) entre las filas 0-1 y las filas 2-3.
   * En DS10 era invisible: sólo U2 estaba poblado.
   *
   * Los prints van DESPUÉS de los cuatro arranques. El delta real entre
   * bancos se mide en el heartbeat (línea SYNC) — no lo supongas, miralo. */
  HAL_StatusTypeDef st_b1, st_b2, st_a1, st_a2;
  st_b1 = HAL_SAI_Transmit_DMA(&hsai_BlockB1, (uint8_t*)audio_buffer_1b, AUDIO_BUFFER_SIZE * 2);
  st_b2 = HAL_SAI_Transmit_DMA(&hsai_BlockB2, (uint8_t*)audio_buffer_2b, AUDIO_BUFFER_SIZE * 2);
  // Masters — SAI1_A generates MCLK (PE2) which PCM1690 needs for POR
  st_a1 = HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t*)audio_buffer, AUDIO_BUFFER_SIZE * 2);
  st_a2 = HAL_SAI_Transmit_DMA(&hsai_BlockA2, (uint8_t*)audio_buffer_2a, AUDIO_BUFFER_SIZE * 2);

  sprintf(uart_buf, "SAI DMA: 1B=%s 2B=%s 1A=%s 2A=%s (MCLK 24.576MHz running)\r\n",
          st_b1 == HAL_OK ? "OK" : "FAIL", st_b2 == HAL_OK ? "OK" : "FAIL",
          st_a1 == HAL_OK ? "OK" : "FAIL", st_a2 == HAL_OK ? "OK" : "FAIL");
  UART_Print(uart_buf);
  HAL_StatusTypeDef status = st_a1;   // el resto del boot mira este

  // === STEP 2: AK5538VN — configure SAI3_B and release power-down ===
  UART_Print("AK5538VN: starting SAI3_B RX DMA...\r\n");
  memset(audio_buffer_3b_rx, 0, sizeof(audio_buffer_3b_rx));
  __DSB();

  /* ===== Arranque DETERMINÍSTICO del RX: engancharlo a un borde del TX =====
   *
   * La fase relativa de los DMA de RX y TX determina POR COMPLETO la latencia
   * del pipeline. No es una hipótesis: se midió sobre 20 arranques y el mapeo es
   * uno a uno, sin excepciones (docs/loopback_debug.md §9).
   *
   * Sin esto, esa fase quedaba librada a cuánto tardó el boot en llegar hasta
   * acá — y el camino es variable: el print de arriba son ~3,3 ms bloqueantes, y
   * la lectura de EEPROM de más arriba tiene ramas de distinta duración según se
   * detecte o no. Resultado medido: tres estados, 4,17 muestras de dispersión =
   * 87 µs = 30 mm de camino acústico. Entre nodos de una instalación eso son
   * 66° de error de fase a 2,1 kHz — no es despreciable para un beam conjunto.
   *
   * El TX no se puede mover: SAI1_A genera el MCLK que los PCM1690 y el AK5538
   * necesitan para completar su POR, así que tiene que arrancar primero. Lo que
   * sí se puede fijar es el RX.
   *
   * Se espera a que el DMA del banco 24 dé la vuelta —NDTR cuenta hacia abajo y
   * al recargarse SUBE, así que un `now > prev` es el wrap— y se arranca el RX
   * justo ahí. Ese borde ocurre cada 10 ms y es idéntico en todos los arranques,
   * así que la fase deja de depender del camino de boot.
   *
   * El timeout existe para que un TX que no arrancó no cuelgue el boot: si pasa,
   * se sigue igual y se arranca el RX sin alinear — peor fase, pero equipo vivo.
   * Se reporta abajo para que no sea un fallo silencioso. */
  uint8_t rx_aligned = 0;
  {
    uint32_t t0   = HAL_GetTick();
    uint32_t prev = __HAL_DMA_GET_COUNTER(&hdma_sai2_b);
    for (;;) {
      uint32_t now = __HAL_DMA_GET_COUNTER(&hdma_sai2_b);
      if (now > prev) { rx_aligned = 1; break; }      /* wrapeó */
      prev = now;
      if ((HAL_GetTick() - t0) > 50u) break;          /* el TX no corre */
    }
  }
  status = HAL_SAI_Receive_DMA(&hsai_BlockB3, (uint8_t*)audio_buffer_3b_rx, ADC_BUFFER_SIZE * 2);
  sprintf(uart_buf, "SAI3_B RX DMA: %s (fase %s)\r\n",
          status == HAL_OK ? "OK" : "FAIL",
          rx_aligned ? "alineada al TX" : "SIN ALINEAR - el TX no wrapeo");
  UART_Print(uart_buf);

  // Release AK5538 from PDN: HIGH = Normal operation (per AK5538VN datasheet "L=PDN, H=Normal")
  // Hardware strapping configures TDM256/slave/32-bit/LDO — no I2C writes needed for bring-up
  HAL_GPIO_WritePin(AK_PDN_PORT, AK_PDN_PIN, GPIO_PIN_SET);
  HAL_Delay(15);  // ≥10ms POR time after PDN release
  UART_Print("AK5538VN: PDN released (PC7 HIGH) — TDM256 slave active\r\n");
  sprintf(uart_buf, "AK5538VN OVF=%d (0=OK)\r\n",
          HAL_GPIO_ReadPin(AK_OVF_PORT, AK_OVF_PIN));
  UART_Print(uart_buf);

  // I2C3 full bus scan — not required for operation (hardware strapping does
  // the config), but an ACK proves the chip is powered and out of PDN.
  // The design review said addr 7-bit 0x14, but it has a track record of
  // errors (MCLK freq, I2C4 AF11); AKM ADCs usually sit at 0x10-0x13
  // depending on CAD0/CAD1 strapping. Scan the whole bus and report.
  {
    uint32_t i2c3_found = 0;
    UART_Print("AK5538VN: scanning I2C3...\r\n");
    for (uint8_t a7 = 0x08; a7 <= 0x77; a7++) {
      if (HAL_I2C_IsDeviceReady(&hi2c3, (uint16_t)(a7 << 1), 1, 5) == HAL_OK) {
        sprintf(uart_buf, "  I2C3 ACK at 7-bit 0x%02X (8-bit 0x%02X)\r\n", a7, a7 << 1);
        UART_Print(uart_buf);
        i2c3_found++;
      }
    }
    if (i2c3_found == 0) {
      UART_Print("  I2C3: NO devices found — check AK5538 3.3VA supply, PDN pin, I2C-enable strapping\r\n");
    }
  }

  // AK5538 register dump (found at 0x22 = 7-bit 0x11). Shows how the chip
  // self-configured from its hardware straps — needed to verify/fix the TDM
  // output mode via I2C (RX data currently looks misframed: only slot 0).
  {
    uint8_t rv;
    UART_Print("AK5538VN: register dump (8-bit addr 0x22):\r\n");
    for (uint32_t base = 0x00; base <= 0x10; base += 0x08) {
      char *p = uart_buf;
      p += sprintf(p, "  R%02lX:", base);
      for (uint32_t r = base; r < base + 8; r++) {
        if (HAL_I2C_Mem_Read(&hi2c3, AK5538_ADDR, (uint16_t)r,
                             I2C_MEMADD_SIZE_8BIT, &rv, 1, 50) == HAL_OK) {
          p += sprintf(p, " %02X", rv);
        } else {
          p += sprintf(p, " --");
        }
      }
      sprintf(p, "\r\n");
      UART_Print(uart_buf);
    }
  }

  // AK5538/AK5578 register map (verified against AKM datasheet, 2026-06):
  //   0x02 Control1: [.][CKS3][CKS2][CKS1][CKS0][DIF1][DIF0][HPFE]
  //   0x03 Control2: [.][TDM1][TDM0][.....................]
  // TWO things must be programmed (straps don't, since CKS/TDM pins are reused
  // for I2C and default to 0):
  //  1) CKS3-0 = MCLK/fs ratio (Table 5). DEFAULT 0000 = 128fs Quad-Speed —
  //     but we feed 512fs (24.576MHz shared MCLK for the PCM1690s). That
  //     mismatch makes the delta-sigma modulator run at the wrong oversampling
  //     → it saturates → OVF=1 → output rails on all 8 ch, INDEPENDENT of the
  //     analog input. THIS was the "ADC ignores its inputs" root cause.
  //     For 512fs @ 48kHz (Normal Speed) → CKS3-0 = 0110.
  //     R02 = [0][0110][00][1] = 0x31  (CKS=0110, DIF=00 24-bit MSB, HPFE=1).
  //  2) TDM1-0 = 10 → TDM256 (R03 = 0x40).
  // Clock/format changes require RSTN=0 (PM2 0x01 bit0) during the write, then
  // RSTN=1 with a stable clock already running (it is).
  {
    uint8_t v;
    UART_Print("AK5538VN: clock mode (CKS=512fs) + TDM256 via I2C...\r\n");
    v = 0x00;  // RSTN=0 — hold in reset while changing clock mode / format
    HAL_I2C_Mem_Write(&hi2c3, AK5538_ADDR, 0x01, I2C_MEMADD_SIZE_8BIT, &v, 1, 50);
    v = 0x31;  // Control1: CKS3-0=0110 (512fs Normal), DIF=00 (24-bit MSB), HPFE=1
    HAL_I2C_Mem_Write(&hi2c3, AK5538_ADDR, 0x02, I2C_MEMADD_SIZE_8BIT, &v, 1, 50);
    v = 0x40;  // Control2: TDM[1:0]=10 → TDM256
    HAL_I2C_Mem_Write(&hi2c3, AK5538_ADDR, 0x03, I2C_MEMADD_SIZE_8BIT, &v, 1, 50);
    /* Control3 (0x04): [DP][0][0][0][0][0][SD][SLOW]
     * SD=1 → SHORT DELAY, SLOW=0 → sharp roll-off  →  R04 = 0x02.
     *
     * Baja el group delay del filtro de diezmado de 19/fs a 5/fs = de 396 µs a
     * 104 µs a 48 kHz (datasheet §9, fs=48k). **Los 292 µs son gratis**: el
     * passband (22 kHz) y la atenuación de stopband (85 dB) son IDÉNTICOS entre
     * SHARP y SHORT DELAY SHARP; lo único que cambia es que el filtro deja de
     * ser de fase lineal (ΔGD pasa de 0 a 2.8/fs máx.).
     *
     * Por qué esa distorsión de fase NO afecta al beamforming: el filtro es el
     * mismo en los 8 canales, así que es un retardo COMÚN. El patrón del haz lo
     * fijan los delays RELATIVOS que aplica el STM32, que no se tocan. Sólo
     * afecta marginalmente al timbre.
     *
     * El registro manda sobre el strapping: la placa tiene los pines SD y SLOW a
     * GND (IC3.48, IC3.47), pero el datasheet §"Operation Mode Control" dice que
     * "in serial control mode, register settings are prioritized so that all pin
     * settings except the MSN pin setting are ignored", y estamos en modo I2C
     * (I2C=H en IC3.54, PSN=L en IC3.53). El readback de abajo lo verifica. */
    v = 0x02;
    HAL_I2C_Mem_Write(&hi2c3, AK5538_ADDR, 0x04, I2C_MEMADD_SIZE_8BIT, &v, 1, 50);
    v = 0x01;  // RSTN=1 — release (stable MCLK/BICK/LRCK already running)
    HAL_I2C_Mem_Write(&hi2c3, AK5538_ADDR, 0x01, I2C_MEMADD_SIZE_8BIT, &v, 1, 50);
    HAL_Delay(5);

    // Readback verify R01-R03
    uint8_t r1 = 0xEE, r2 = 0xEE, r3 = 0xEE, r4 = 0xEE;
    HAL_I2C_Mem_Read(&hi2c3, AK5538_ADDR, 0x01, I2C_MEMADD_SIZE_8BIT, &r1, 1, 50);
    HAL_I2C_Mem_Read(&hi2c3, AK5538_ADDR, 0x02, I2C_MEMADD_SIZE_8BIT, &r2, 1, 50);
    HAL_I2C_Mem_Read(&hi2c3, AK5538_ADDR, 0x03, I2C_MEMADD_SIZE_8BIT, &r3, 1, 50);
    HAL_I2C_Mem_Read(&hi2c3, AK5538_ADDR, 0x04, I2C_MEMADD_SIZE_8BIT, &r4, 1, 50);
    sprintf(uart_buf,
            "  readback R01=0x%02X R02=0x%02X R03=0x%02X R04=0x%02X"
            " (want R02=0x31 R03=0x40 R04=0x02)\r\n", r1, r2, r3, r4);
    UART_Print(uart_buf);
    /* R04=0x02 confirma que el registro pisó al strapping de los pines SD/SLOW
     * → group delay del ADC = 5/fs = 104 µs en vez de 19/fs = 396 µs.
     * Si lee 0x00, el chip está ignorando el registro: revisar que I2C=H y
     * PSN=L, y volver a contar los 396 µs en el presupuesto de latencia. */
  }

  // === STEP 3: PCM1690 init (shared RST/MUTE bus, DS10 style) ===
  // DAC_RST (PC11) was held LOW since GPIO init — MCLK now running ≥5ms
  HAL_Delay(5);
  UART_Print("\r\nPCM1690: releasing shared RST (PC11)...\r\n");
  HAL_GPIO_WritePin(DAC_RST_PORT, DAC_RST_PIN, GPIO_PIN_SET);
  HAL_Delay(12);  // ≥10ms PCM1690 internal POR after RST release

  uint32_t dac_ok = 0;
  for (uint32_t d = 0; d < 4; d++) {
    PCM1690_Status ds = PCM1690_Init(&hi2c1, dac_addrs[d], NULL);
    sprintf(uart_buf, "  DAC%lu (0x%02X): %s\r\n", d, dac_addrs[d],
            ds == PCM1690_OK ? "OK" : "FAIL");
    UART_Print(uart_buf);
    if (ds == PCM1690_OK) dac_ok++;

    // Register readback on first DAC for verification
    if (d == 0 && ds == PCM1690_OK) {
      uint8_t regval;
      const uint8_t regs[] = {64, 65, 66, 68, 70, 72, 73, 74, 75, 76, 77, 78, 79};
      const char *names[] = {"SYSTEM","FORMAT","OPER","MUTE","DEEMPH",
                              "ATT1","ATT2","ATT3","ATT4","ATT5","ATT6","ATT7","ATT8"};
      UART_Print("  DAC0 register readback:\r\n");
      for (uint32_t r = 0; r < 13; r++) {
        if (PCM1690_ReadReg(&hi2c1, dac_addrs[0], regs[r], &regval) == PCM1690_OK) {
          sprintf(uart_buf, "    Reg%u (%s) = 0x%02X\r\n", regs[r], names[r], regval);
        } else {
          sprintf(uart_buf, "    Reg%u (%s) = READ FAIL\r\n", regs[r], names[r]);
        }
        UART_Print(uart_buf);
      }
    }
  }
  sprintf(uart_buf, "PCM1690: %lu/4 DACs initialized OK\r\n", dac_ok);
  UART_Print(uart_buf);

  // Unmute all DACs (AMUTEI HIGH = unmuted)
  HAL_GPIO_WritePin(DAC_MUTE_IN_PORT, DAC_MUTE_IN_PIN, GPIO_PIN_SET);
  UART_Print("PCM1690: AMUTEI HIGH — DACs unmuted\r\n");

  // Restore saved master volume (EEPROM load already populated master_vol_db;
  // the DAC init just forced −24dB, so re-apply the persisted value on top).
  {
    uint8_t atd = Apply_Master_Volume(beam_config.master_vol_db);
    snprintf(uart_buf, sizeof(uart_buf),
             "VOLUME: %d dB al boot (ATDA=0x%02X)\r\n",
             (int)beam_config.master_vol_db, atd);
    UART_Print(uart_buf);
  }

  // === STEP 4: Verify all 4 TX SAI blocks ===
  // SAIEN=1 means enabled. WCKCFG=1 (master only) = wrong clock config → the
  // block can't generate valid BCK/FS → its DACs get garbage/nothing. OVRUDR=
  // underrun. If SAI2_A shows WCKCFG=1 or any block SAIEN=0, that's a firmware
  // bug (not the rework) and explains the silent channels.
  {
    SAI_Block_TypeDef *blk[4] = { SAI1_Block_A, SAI1_Block_B, SAI2_Block_A, SAI2_Block_B };
    const char *nm[4] = { "SAI1_A(U2)", "SAI1_B(U3)", "SAI2_A(U1)", "SAI2_B(U4)" };
    for (uint32_t i = 0; i < 4; i++) {
      uint32_t cr1 = blk[i]->CR1;
      uint32_t sr  = blk[i]->SR;
      sprintf(uart_buf, "%s: SAIEN=%lu MODE=%lu SR=0x%08lX OVRUDR=%lu WCKCFG=%lu FLVL=%lu\r\n",
              nm[i], (cr1 >> 16) & 1, (cr1 >> 0) & 3, sr,
              (sr >> 0) & 1, (sr >> 2) & 1, (sr >> 16) & 7);
      UART_Print(uart_buf);
    }
  }

  // === STEP 5: amplificadores APAGADOS al boot (ver amps_enabled) ===
  HAL_GPIO_WritePin(PAM_SD_PORT, PAM_SD_PIN, GPIO_PIN_RESET);
  amps_enabled = 0;
  UART_Print("PAM8302A: SD LOW — 32 amps APAGADOS. Habilitar con POST /amps {\"on\":true}\r\n");

  // === ESP32 UART protocol init ===
  UART_Protocol_Init(&huart4, &huart7);
  /* D6 (PB0/TIM3_CH3) brightness will ramp via dimmer — no separate GPIO toggle needed */

  // Reset counters + flags (underruns during init are expected)
  need_fill_first_half = 0;
  need_fill_second_half = 0;
  need_fill_1b_first_half = 0;
  need_fill_1b_second_half = 0;
  need_fill_2a_first_half = 0;
  need_fill_2a_second_half = 0;
  need_fill_2b_first_half = 0;
  need_fill_2b_second_half = 0;
  rx_3b_first_half_ready = 0;
  rx_3b_second_half_ready = 0;
  half_complete_count = 0;
  full_complete_count = 0;
  underrun_count = 0;
  error_count = 0;
  __DSB();

  // Enable DWT cycle counter for latency measurement (ARM Cortex-M7)
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  // Reset jitter tracking AFTER DWT enable
  __disable_irq();
  last_half_isr_cycles = 0;
  last_full_isr_cycles = 0;
  jitter_min_cycles = 0xFFFFFFFF;
  jitter_max_cycles = 0;
  jitter_sum_cycles = 0;
  jitter_sample_count = 0;
  fill_min_cycles = 0xFFFFFFFF;
  fill_max_cycles = 0;
  fill_sum_cycles = 0;
  fill_sample_count = 0;
  __enable_irq();
  UART_Print("DWT cycle counter enabled (latency measurement)\r\n");

  // Variables for main loop
  uint32_t last_half = 0;
  uint32_t last_full = 0;
  uint32_t test_start = HAL_GetTick();
  // DWT-based 1s heartbeat — works even if SysTick is frozen
  uint32_t last_hb_dwt = DWT->CYCCNT;

  // Restore the last-used mode from EEPROM. AUTOFOCUS is NOT auto-restored:
  // it would fire a chirp scan at boot without the mic placed → fall to
  // monitor (and, since 0 != last_mode there, that write-back deliberately
  // forgets AUTOFOCUS). The normal case restores idx == last_mode, so
  // UI_ApplyState leaves the save disarmed.
  // La ganancia de entrada se restaura ANTES del modo: UI_ApplyState deriva de
  // ella la ganancia del beam, así que tiene que estar puesta primero.
  Set_In_Gain(beam_config.in_gain_db);
  snprintf(uart_buf, sizeof(uart_buf),
           "INGAIN: +%u dB al boot (%s)\r\n", (unsigned)in_gain_db,
           in_gain_db ? "guardado en EEPROM" : "linea");
  UART_Print(uart_buf);
  {
    uint8_t bm = beam_config.last_mode;
    if (bm >= UI_NUM_STATES || ui_states[bm].af) bm = 0;
    UI_ApplyState(bm);
  }

  UART_Print("[STM32] main loop started — SAI+DMA running\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    Service_Fills();

    // ===== AUTOFOCUS scan driver (chunked correlation, never blocks fills) =====
    if (autofocus_enabled) Cal_Process();

    // ===== LOOPBACK driver — misma disciplina: correlación en trozos =====
    if (lb_enabled) LB_Process();
    if (fs_enabled) FS_Process();

    /* ===== BARRIDO: mover el foco, 20 veces por segundo =====
     * Triángulo entre −amp y +amp sobre el eje elegido. Va acá y no en el fill
     * porque Beam_SetTxFocus usa floats y escribe tx_delay_q8[], que el fill
     * lee: en el main loop el peor caso es que un fill agarre la tabla a mitad
     * de actualización, y como el paso son ~14 mm (0,04 muestras) eso no se
     * oye. */
    if (sweep_axis) {
      static uint32_t sweep_last_ms = 0;
      uint32_t now_ms = HAL_GetTick();
      if (now_ms - sweep_last_ms >= SWEEP_STEP_MS) {
        sweep_last_ms = now_ms;
        uint32_t ph = now_ms % (2u * SWEEP_HALF_MS);          /* 0 … 2·half */
        int32_t tri = (ph < SWEEP_HALF_MS)                    /* 0→half→0 */
                      ? (int32_t)ph
                      : (int32_t)(2u * SWEEP_HALF_MS - ph);
        int32_t amp = (sweep_axis == SWEEP_AXIS_Y) ? SWEEP_Y_MM : SWEEP_X_MM;
        /* tri: 0..SWEEP_HALF_MS  →  v: −amp..+amp */
        int32_t v = (2 * amp * tri) / (int32_t)SWEEP_HALF_MS - amp;
        if (sweep_axis == SWEEP_AXIS_Y) Beam_SetTxFocus(0, (int16_t)v, 1000);
        else                            Beam_SetTxFocus((int16_t)v, 0, 1000);
      }
    }

    /* ===== K2 button (PA4, external pull-up, press = LOW) =====
     * Pulsación CORTA (al soltar) = siguiente modo.
     * Pulsación LARGA (≥1 s, actúa sin soltar) = encender/apagar los amps.
     *
     * El toggle de amps existía solo por HTTP, y los amps arrancan APAGADOS: sin
     * WiFi la placa quedaba muda sin forma de recuperarla desde el hardware. Con
     * esto el equipo es operable sin red — que es la condición para que alguien
     * lo use sin una terminal al lado. Además, como "on" republica el pulso de
     * SD, la pulsación larga es el gesto de recuperación del UVLO enclavado de
     * los PAM8302A: si tras un hundimiento del +5VA quedaron canales mudos, esto
     * los revive.
     *
     * El modo se cambia al SOLTAR, no al presionar: si se disparara en el flanco
     * de bajada, toda pulsación larga cambiaría de modo antes de tocar los amps. */
    {
      static uint8_t  k2_prev = 1;
      static uint32_t k2_change_dwt = 0;
      static uint32_t k2_press_dwt = 0;
      static uint8_t  k2_long_done = 0;
      uint8_t k2_now = (HAL_GPIO_ReadPin(OPT_SW_K2_PORT, OPT_SW_K2_PIN) == GPIO_PIN_SET);
      if (k2_now != k2_prev && (DWT->CYCCNT - k2_change_dwt) > (30UL * 480000UL)) {
        k2_prev = k2_now;                          // 30ms debounce @480MHz
        k2_change_dwt = DWT->CYCCNT;
        if (k2_now == 0) {                         // flanco de bajada = presionado
          k2_press_dwt = DWT->CYCCNT;
          k2_long_done = 0;
        } else if (!k2_long_done) {                // soltó sin llegar a larga
          UI_ApplyState((ui_state_idx + 1) % UI_NUM_STATES);
        }
      }
      /* Umbral de 1 s. DWT->CYCCNT da la vuelta cada ~8,9 s a 480 MHz; la resta
       * sin signo lo maneja mientras el intervalo medido sea menor que eso. */
      if (!k2_prev && !k2_long_done &&
          (DWT->CYCCNT - k2_press_dwt) > (1000UL * 480000UL)) {
        k2_long_done = 1;                          // el soltar ya no cambia modo
        UART_Print("K2 largo: ");
        Set_Amps(!amps_enabled);
      }
    }

    // ===== Remote control from ESP32 (Fase 4) =====
    {
      int32_t midx = UART_Protocol_GetModeRequest();
      if (midx >= 0) {
        if (midx < (int32_t)UI_NUM_STATES) {
          UI_ApplyState((uint8_t)midx);
        } else {
          sprintf(uart_buf, "ESP32: mode %ld out of range (0-%u)\r\n",
                  (long)midx, (unsigned)(UI_NUM_STATES - 1));
          UART_Print(uart_buf);
        }
      }

      ds_beam_req_t breq;
      if (UART_Protocol_GetBeamRequest(&breq)) {
        UI_ApplyCustomBeam(&breq);
      }

      // Debounced config persistence: write only once the changes settle.
      if (cfg_save_due_tick != 0 &&
          (int32_t)(HAL_GetTick() - cfg_save_due_tick) >= 0) {
        cfg_save_due_tick = 0;
        if (EEPROM_IsAvailable()) eeprom_save_pending = 1;
      }

      // ===== Master volume: PCM1690 attenuation (all modes, walker incl.) =====
      // Applied here (main loop) not the ISR: I2C writes must not run in IRQ.
      int8_t vol_db;
      if (UART_Protocol_GetVolumeRequest(&vol_db)) {
        uint8_t atten = Apply_Master_Volume(vol_db);
        beam_config.master_vol_db = vol_db;        // persist with the rest of the config
        Arm_Config_Save();                         // debounced: no save-burst per slider drag
        snprintf(uart_buf, sizeof(uart_buf),
                 "VOLUME: %d dB → ATDA=0x%02X (save diferido)\r\n",
                 (int)vol_db, atten);
        UART_Print(uart_buf);
      }

      /* Enable del banco de amplificadores por comando. Herramienta de
       * diagnostico: con los amps apagados corre TODO el pipeline (SAI, DMA,
       * beamforming) en silencio, asi se aisla si un reinicio viene de la
       * demanda de los 32 PAM8302A sobre el buck (que quedo con menos bulk
       * tras remover C290 — ver docs/ds11_errata.md). */
      uint8_t amps_on;
      {
        uint8_t ig_db;
        if (UART_Protocol_GetInGainRequest(&ig_db)) {
          Set_In_Gain(ig_db);
          beam_config.in_gain_db = in_gain_db;   // el valor CLAMPEADO, no el pedido
          Arm_Config_Save();                     // sobrevive al reboot
          snprintf(uart_buf, sizeof(uart_buf),
                   "INGAIN: +%u dB (ganancia de ENTRADA; el volumen es la salida)\r\n",
                   (unsigned)in_gain_db);
          UART_Print(uart_buf);
        }
      }

      if (UART_Protocol_GetAmpsRequest(&amps_on)) Set_Amps(amps_on);

      if (UART_Protocol_GetFactoryResetRequest()) Config_FactoryReset();

      OTA_ServiceRequests();

      /* Reinicio diferido tras una carga OTA verificada: acá termina el trabajo
       * de la aplicación. Lo que sigue lo hace el bootloader. */
      if (ota_reboot_at != 0 && (int32_t)(HAL_GetTick() - ota_reboot_at) >= 0) {
        UART_Print("OTA: reiniciando.\r\n");
        HAL_Delay(20);                 /* que salga la línea antes del reset */
        NVIC_SystemReset();
      }

      /* ===== Captura remota del ADC ===== */
      {
        uint16_t cn;
        if (UART_Protocol_GetCapStart(&cn)) {
          if (cn == 0) cn = 1;
          if (cn > CAP_MAX_SAMPLES) cn = CAP_MAX_SAMPLES;
          cap_want = cn;
          cap_count = 0;
          cap_info_due = 1;
          cap_state = CAP_FILL;       /* lo llena Process_ADC_Half */
        }

        /* La captura terminó: recién acá se calcula el autorrango y se contesta
         * el CAP_START. El pico se busca sobre TODO lo capturado, así el
         * desplazamiento es uno solo para el archivo entero y la escala no
         * cambia a mitad de camino. */
        if (cap_info_due && cap_state == CAP_READY) {
          cap_info_due = 0;
          int32_t pk = 0;
          for (uint32_t i = 0; i < cap_want; i++)
            for (uint32_t ch = 0; ch < ADC_CHANNELS; ch++) {
              int32_t v = cap_buf[i][ch];
              if (v < 0) v = -v;
              if (v > pk) pk = v;
            }
          cap_peak = pk;
          cap_shift = 0;
          while ((pk >> cap_shift) > 32767) cap_shift++;

          ds_cap_info_t info;
          info.n_samples   = cap_want;
          info.n_channels  = ADC_CHANNELS;
          info.shift       = cap_shift;
          info.rate_hz     = 48000;
          info.peak        = cap_peak;
          info.total_bytes = (uint32_t)cap_want * ADC_CHANNELS * 2u;
          UART_Protocol_SendCapInfo(&info);
        }

        uint32_t coff;
        if (UART_Protocol_GetCapRead(&coff)) {
          uint32_t total = (uint32_t)cap_want * ADC_CHANNELS * 2u;
          uint8_t  chunk[CAP_CHUNK_BYTES];
          uint16_t n = 0;
          if (cap_state == CAP_READY && coff < total) {
            uint32_t left = total - coff;
            n = (uint16_t)((left > CAP_CHUNK_BYTES) ? CAP_CHUNK_BYTES : left);
            /* offset en bytes → índice de muestra int16 → (i, ch) */
            for (uint16_t k = 0; k < n; k += 2) {
              uint32_t idx = (coff + k) / 2u;
              int32_t  v   = cap_buf[idx / ADC_CHANNELS][idx % ADC_CHANNELS] >> cap_shift;
              if (v >  32767) v =  32767;      /* el autorrango lo evita, pero */
              if (v < -32768) v = -32768;      /* recortar es mejor que envolver */
              chunk[k]     = (uint8_t)((int16_t)v & 0xFF);
              chunk[k + 1] = (uint8_t)(((int16_t)v >> 8) & 0xFF);
            }
          }
          UART_Protocol_SendCapData(coff, chunk, n);   /* n=0 = fuera de rango */
        }
      }

      if (UART_Protocol_SelfTestRequested()) {
        ds_health_t h;
        Health_Run(&h);
        UART_Protocol_SendHealth(&h);
      }

      if (UART_Protocol_StatusRequested()) {
        ds_status_t st;
        st.proto_ver = PROTO_VERSION;
        st.mode_idx = ui_state_idx;
        st.num_modes = UI_NUM_STATES;
        st.rx_az_deg = beam_az_deg;
        st.fx_mm = tx_focus_x_mm;
        st.fy_mm = tx_focus_y_mm;
        st.fz_mm = tx_focus_z_mm;
        st.underruns  = underrun_total + underrun_count;   // acumulado real
        st.sai_errors = sai_error_total + error_count;
        st.ovf = (uint8_t)HAL_GPIO_ReadPin(AK_OVF_PORT, AK_OVF_PIN);
        st.cal_state = (uint8_t)cal_state;
        /* Con signo, envuelta alrededor de 0 — no en [0,480) como la usa el
         * informe del loopback. Motivo: la fase real es 0 ±1 muestra de ruido
         * de lectura (§9), y en el rango sin signo ese −1 sale como **479**.
         * Un consumidor que compare nodos leería medio buffer de diferencia
         * donde hay una muestra. Para telemetría, lo útil es la distancia a
         * cero, y el campo ya es int16 justamente para eso. */
        {
          int32_t ph = DMA_PhaseSamples();
          int32_t half = (int32_t)(LB_DMA_WORDS / AUDIO_CHANNELS) / 2;
          if (ph > half) ph -= (int32_t)(LB_DMA_WORDS / AUDIO_CHANNELS);
          st.dma_phase = (int16_t)ph;
        }
        st.master_vol_db = beam_config.master_vol_db;
        st.in_gain_db = in_gain_db;
        st.adc_ovf_count = adc_ovf_latch;
        st.out_clip_count = out_clip_count;
        st.state_flags = (amps_enabled ? STATE_F_AMPS : 0)
                       | (custom_beam_active ? STATE_F_CUSTOM : 0)
                       | (beam_config.cal_valid ? STATE_F_CALIB : 0);
        st.uptime_ms = HAL_GetTick();
        st.cpu_fills_x10 = cpu_fills_x10;
        st.mic_live_mask = mic_live_mask;
        st.cal_progress  = cal_progress;
        st.cal_ok_count  = cal_ok_count;
        st.fw_id    = fw_id;
        st.fw_build = fw_build;
        st.ota_state = (uint8_t)OTA_GetState();
        {
          uint32_t exp = OTA_GetExpected();
          st.ota_pct = (OTA_GetState() == OTA_ST_RECV && exp)
                       ? (uint8_t)((OTA_GetReceived() * 100U) / exp)
                       : ((OTA_GetState() == OTA_ST_READY) ? 100U : 0U);
        }
        /* Pico REAL sobre todas las muestras desde el sondeo anterior, drenado
         * al leerlo. Acá se recalculaba sobre el buffer DMA vivo, o sea 10 ms
         * de cada intervalo — ver el comentario de adc_peak_net. */
        for (uint32_t ch = 0; ch < ADC_CHANNELS; ch++) {
          st.adc_pk[ch] = adc_peak_net[ch];
          adc_peak_net[ch] = 0;
        }
        UART_Protocol_SendStatus(&st);
      }
    }

    // ===== Check for new ESP32 config → apply to beam_config =====
    /* Snapshot: se trabaja sobre una copia, nunca sobre el buffer que escribe
     * el ISR (ver UART_Protocol_TakeConfig). Estático y no local porque son 102
     * bytes y este bloque está adentro del lazo principal. */
    static ds_config_t cfg_snapshot;
    if (UART_Protocol_TakeConfig(&cfg_snapshot)) {
      const ds_config_t *cfg = &cfg_snapshot;
      beam_config.master_gain = cfg->master_gain;
      beam_config.mute_global = cfg->mute_global;
      /* Desde 2026-08-18 la config del ESP32 cubre los 32 canales; antes eran
       * 16 y las filas 3 y 4 no se podían silenciar ni atenuar desde la red. */
      for (uint32_t i = 0; i < CFG_NUM_CHANNELS && i < BEAM_NUM_CHANNELS; i++) {
        beam_config.delays[i] = cfg->delays[i];
        beam_config.gains[i] = cfg->gains[i];
      }
      beam_config.mutes = cfg->mutes;
      // In RX beam mode, delays[0..7] act as RAW per-mic RX delays —
      // experimentation path that overrides the computed steering table.
      if (beam_enabled) {
        for (uint32_t m = 0; m < MIC_COUNT; m++) {
          uint16_t d = cfg->delays[m];
          rx_delay[m] = (d > MAX_DELAY_SAMPLES) ? MAX_DELAY_SAMPLES : d;
        }
        UART_Print("BEAM: raw RX delays applied from ESP32 config\r\n");
      }
      UART_Protocol_PrintConfig(cfg);
      UART_Print("Config applied to beamformer\r\n");

      // Debounced EEPROM save — coalesces bursts of config frames
      Arm_Config_Save();
    }

    // ===== LED dimmer — TIM3 PWM step (10ms/step, 300 steps = 3s/LED, 12s full cycle) =====
    // Sweeps through all 4 LEDs (D4→D5→D6→D7), one fading-in at a time, others off.
    if (last_dimmer_dwt != 0 && (DWT->CYCCNT - last_dimmer_dwt) >= DIMMER_STEP_CYC) {
      last_dimmer_dwt += DIMMER_STEP_CYC;
      uint32_t ccr = (dimmer_step * DIMMER_ARR) / (DIMMER_STEPS - 1);
      for (uint32_t p = 0; p < DIMMER_NUM_PHASES; p++) {
        __HAL_TIM_SET_COMPARE(&htim3, dimmer_channels[p], (p == dimmer_phase) ? ccr : 0);
      }
      dimmer_step++;
      if (dimmer_step >= DIMMER_STEPS) {
        dimmer_step = 0;
        dimmer_phase = (dimmer_phase + 1) % DIMMER_NUM_PHASES;
      }
    }

    // ===== Deferred EEPROM save (blocks ~12ms, causes underruns — acceptable) =====
    if (eeprom_save_pending) {
      eeprom_save_pending = 0;
      UART_Print("EEPROM: Saving config...\r\n");
      eeprom_status_t eep_st = EEPROM_SaveConfig(&beam_config, sizeof(beam_config));
      if (eep_st == EEPROM_OK) {
        UART_Print("EEPROM: Config saved OK\r\n");
      } else {
        sprintf(uart_buf, "EEPROM: Save FAILED (err=%d)\r\n", (int)eep_st);
        UART_Print(uart_buf);
      }
    }

    // Print status every second — DWT-based (480M cycles = 1s, SysTick-independent)
    if ((DWT->CYCCNT - last_hb_dwt) >= 480000000UL) {
      last_hb_dwt = DWT->CYCCNT;

      // LED now toggled in Half callback at 50Hz for DMA sync visual

      // Calculate callbacks per second
      uint32_t half_per_sec = half_complete_count - last_half;
      uint32_t full_per_sec = full_complete_count - last_full;
      last_half = half_complete_count;
      last_full = full_complete_count;

      // Verbose details only every 10th heartbeat — the per-second print is kept
      // to ONE line. Long multi-line bursts on UART7 block the main loop long
      // enough to delay the SAI fills → spurious underruns (~6/s). One line/sec
      // keeps the block short.
      static uint32_t hb_count = 0;
      uint8_t verbose = ((hb_count++ % 10) == 0);

      // Expected: 50 half + 50 full per second (20ms buffer cycle @ 48kHz TDM8)
      // snprintf: con la IP anexada la linea puede exceder los 128B de uart_buf.
      char esp_ip[ESP_IP_MAXLEN + 1];
      UART_Protocol_CopyEspIp(esp_ip, sizeof(esp_ip));
      snprintf(uart_buf, sizeof(uart_buf),
              "[%lus] Half: %lu/s, Full: %lu/s, Err: %lu, Undr: %lu, RX: %lu bytes | ESP: %s\r\n",
              HAL_GetTick() / 1000, half_per_sec, full_per_sec,
              error_count, underrun_count, UART_Protocol_GetRxByteCount(),
              esp_ip[0] ? esp_ip : "--");
      UART_Print(uart_buf);


      /* SYNC + CPU — las dos métricas nuevas de DS11, una línea por segundo.
       *
       * SYNC = desfase entre los ciclos de DMA de los dos masters (SAI1_A =
       * filas 0-1, SAI2_A = filas 2-3). Se toma la diferencia de timestamps
       * de sus Half callbacks módulo el ciclo completo (20 ms), con signo en
       * ±10 ms. DEBE dar unos pocos µs: si da milisegundos, los dos bancos
       * están tocando ventanas de tiempo distintas y el beamforming vertical
       * no es coherente (ver comentario del arranque de los SAI).
       *
       * CPU = fracción del presupuesto gastada en los 4 fills. 480M ciclos/s
       * = 100%. Es el número real del modo 15 con 32 canales; el Fill de más
       * abajo mide sólo SAI1_A. */
      {
        /* Diferencia CON SIGNO: la resta unsigned y el cast a int32 dan el
         * delta correcto aunque DWT->CYCCNT haya dado la vuelta (vale para
         * |delta| < 2^31 ciclos ≈ 4.5 s; acá los callbacks son cada 20 ms).
         * NO usar % sobre el unsigned: 2^32 no es múltiplo del ciclo, así
         * que el módulo se corrompe justo en el wrap. */
        const int32_t CYC_PER_BUFCYCLE = 9600000;      // 20 ms @480MHz
        int32_t skew = (int32_t)(cb_2a_half_dwt - cb_a1_half_dwt);
        while (skew >  CYC_PER_BUFCYCLE / 2) skew -= CYC_PER_BUFCYCLE;
        while (skew < -CYC_PER_BUFCYCLE / 2) skew += CYC_PER_BUFCYCLE;
        int32_t skew_us = skew / (int32_t)CPU_FREQ_MHZ;
        /* Se guarda para el autodiagnóstico: hasta ahora el SYNC entre bancos
         * sólo existía en esta línea del serie, o sea que verificarlo pedía un
         * cable. Es de las métricas que más importan —milisegundos acá
         * significan que el enfoque vertical no existe— y era la menos visible. */
        sync_us_last = (int16_t)((skew_us > 32767) ? 32767 :
                                 ((skew_us < -32768) ? -32768 : skew_us));

        static uint64_t last_fill_all = 0;
        uint64_t now_fill = fill_all_cycles;
        uint32_t cpu_x10 = (uint32_t)(((now_fill - last_fill_all) * 1000ULL) / 480000000ULL);
        last_fill_all = now_fill;
        cpu_fills_x10 = (uint16_t)(cpu_x10 > 65535 ? 65535 : cpu_x10);

        snprintf(uart_buf, sizeof(uart_buf),
                 "     SYNC bancos: %ld us (%ld cyc) | CPU fills: %lu.%lu%% | "
                 "fase RX-TX: %ld\r\n",
                 (long)skew_us, (long)skew,
                 (unsigned long)(cpu_x10 / 10), (unsigned long)(cpu_x10 % 10),
                 (long)DMA_PhaseSamples());
        UART_Print(uart_buf);
      }

      // 8-channel ADC peak meter — printed EVERY second (one line) so an
      // injected tone is visible live on whatever input pair it lands on.
      // Peak = max(|min|,|max|) over the first half-buffer, shown 24-bit.
      // Floating input → ~8.3M (railed hum). Grounded → ~0. Tone → scales
      // linearly with the generator level. Use this to map input→TDM slot.
      {
        // Per-channel PEAK = max(|v|) over the half-buffer, in 24-bit. Now that
        // the clock mode is correct the ADC no longer rails, so peak ≈ noise
        // floor (~100) on quiet inputs and jumps to the signal amplitude when a
        // tone is injected — linear with the generator level. Use this to map
        // input→TDM slot and confirm the ADC tracks a real signal.
        /* Peak-hold acumulado en Process_ADC_Half sobre TODAS las muestras del
         * último segundo (no una ventana de 10 ms como antes) + latch del pin
         * OVF. Se leen y se limpian acá. */
        int32_t pk[ADC_CHANNELS];
        for (uint32_t ch = 0; ch < ADC_CHANNELS; ch++) {
          pk[ch] = adc_peak_hold[ch];
          adc_peak_hold[ch] = 0;
        }

        /* ===== ¿Hay un micrófono vivo en cada entrada? =====
         * Se calcula acá y no en el handler de /status porque acá la cadencia
         * es de 1 Hz exacto: pk[] ES el pico del último segundo sobre todas las
         * muestras. En el handler la cadencia la fija el sondeo del ESP32 y la
         * histéresis en segundos no significaría nada.
         *
         * Arranca en "callado" a propósito: inicializado en cero, un equipo
         * recién encendido sin nada enchufado habría mostrado la casilla del
         * micrófono en verde durante los primeros MIC_LIVE_HOLD_S segundos. */
        _Static_assert(ADC_CHANNELS == 8, "el inicializador de mic_quiet_s asume 8");
        static uint8_t mic_quiet_s[ADC_CHANNELS] = {
          MIC_LIVE_HOLD_S, MIC_LIVE_HOLD_S, MIC_LIVE_HOLD_S, MIC_LIVE_HOLD_S,
          MIC_LIVE_HOLD_S, MIC_LIVE_HOLD_S, MIC_LIVE_HOLD_S, MIC_LIVE_HOLD_S };
        uint8_t live = 0;
        for (uint32_t ch = 0; ch < ADC_CHANNELS; ch++) {
          if (pk[ch] >= MIC_LIVE_THRESH)              mic_quiet_s[ch] = 0;
          else if (mic_quiet_s[ch] < MIC_LIVE_HOLD_S) mic_quiet_s[ch]++;
          if (mic_quiet_s[ch] < MIC_LIVE_HOLD_S)      live |= (uint8_t)(1u << ch);
        }
        mic_live_mask = live;
        /* Los contadores son MONOTÓNICOS y acá se imprime el delta contra la
         * impresión anterior. Antes se ponían en cero al imprimir, lo que
         * hacía imposible exponerlos por telemetría: el que leyera primero se
         * los comía. Con delta local, /status puede publicar el acumulado y la
         * UI derivar "está saturando AHORA" de que el número crezca. */
        static uint32_t last_ovf = 0, last_clip = 0;
        uint32_t ovf_n  = adc_ovf_latch  - last_ovf;   last_ovf  = adc_ovf_latch;
        uint32_t clip_n = out_clip_count - last_clip;  last_clip = out_clip_count;

        // RX DMA liveness (~100/s).
        static uint32_t last_rx3b = 0;
        uint32_t rx_now = cb_rx3b_count;
        uint32_t rx_per_sec = rx_now - last_rx3b;
        last_rx3b = rx_now;

        sprintf(uart_buf,
                "     ADCpk: c0=%ld c1=%ld c2=%ld c3=%ld c4=%ld c5=%ld c6=%ld c7=%ld | RXdma=%lu/s\r\n",
                (long)pk[0], (long)pk[1], (long)pk[2], (long)pk[3],
                (long)pk[4], (long)pk[5], (long)pk[6], (long)pk[7], rx_per_sec);
  {
    /* Pasadas dobles por banco — ver dbl_fill. Con los cuatro en 0 el emisor
     * de los modos 19/20 no puede haber desfasado los bancos. */
    char db[96];
    snprintf(db, sizeof(db), "     dblfill: b0=%lu b8=%lu b16=%lu b24=%lu\r\n",
             (unsigned long)dbl_fill[0], (unsigned long)dbl_fill[1],
             (unsigned long)dbl_fill[2], (unsigned long)dbl_fill[3]);
    UART_Print(db);
  }
        UART_Print(uart_buf);

        /* Las dos saturaciones, separadas. Sólo se imprime si alguna ocurrió,
         * para que cuando aparezca la línea sea inequívoca:
         *   OVF>0  → satura el ADC       → bajar la fuente / poner atenuador
         *   CLIP>0 → satura el boost     → usar otro modo, no tocar el hardware
         * ADCpk de referencia: 8388607 es fondo de escala; 838860 son -20 dBFS. */
        if (ovf_n || clip_n) {
          sprintf(uart_buf,
                  "     >>> SATURACION: ADC(OVF)=%lu  SALIDA(clip)=%lu  <<<\r\n",
                  (unsigned long)ovf_n, (unsigned long)clip_n);
          UART_Print(uart_buf);
        }

        // RX beam meter: peak of the freshest 480 beam samples. With the same
        // signal injected on all mics: steering at the (electrical) source
        // → BEAMpk ≈ ADCpk of one channel; steering away → BEAMpk drops.
        if (beam_enabled) {
          uint32_t bwp = delay_write_pos;
          int32_t bpk = 0;
          for (uint32_t i = 0; i < ADC_BUFFER_SAMPLES; i++) {
            int32_t v = beam_line[(bwp - ADC_BUFFER_SAMPLES + i) & DELAY_LINE_MASK];
            if (v < 0) v = -v;
            if (v > bpk) bpk = v;
          }
          sprintf(uart_buf, "     BEAM az=%d pk=%ld\r\n", (int)beam_az_deg, (long)bpk);
          UART_Print(uart_buf);
        }
      }

      // LED dimmer: initialize DWT reference on first heartbeat
      if (last_dimmer_dwt == 0) last_dimmer_dwt = DWT->CYCCNT;

      if (verbose) {
        // Latency stats (cycles → microseconds: cycles / CPU_FREQ_MHZ)
        if (fill_sample_count > 0) {
          uint32_t fill_avg = (uint32_t)(fill_sum_cycles / fill_sample_count);
          sprintf(uart_buf, "     Fill: min=%lu avg=%lu max=%lu cyc (%lu/%lu/%lu us)\r\n",
                  fill_min_cycles, fill_avg, fill_max_cycles,
                  fill_min_cycles / CPU_FREQ_MHZ, fill_avg / CPU_FREQ_MHZ, fill_max_cycles / CPU_FREQ_MHZ);
          UART_Print(uart_buf);
        }
        if (jitter_sample_count > 0) {
          uint32_t jitter_avg = (uint32_t)(jitter_sum_cycles / jitter_sample_count);
          uint32_t jitter_range = jitter_max_cycles - jitter_min_cycles;
          sprintf(uart_buf, "     Jitter: period min=%lu avg=%lu max=%lu cyc (range=%lu = %lu us)\r\n",
                  jitter_min_cycles, jitter_avg, jitter_max_cycles,
                  jitter_range, jitter_range / CPU_FREQ_MHZ);
          UART_Print(uart_buf);
        }

        // CPU temperature
        int32_t temp = Read_CPU_Temperature();
        if (temp != -999) {
          int32_t t_abs = temp >= 0 ? temp : -temp;
          /* Aviso térmico. A VOS0 (480 MHz) la T_J máxima es 105 °C, NO 125 —
           * Table 113 del datasheet. Correr a 480 MHz cuesta 20 °C de margen, y
           * estamos además en el máximo simultáneo de fCPU, HCLK y PCLK.
           *
           * El soak al aire libre dio 38-40 °C, o sea 65 °C de sobra. Pero esto
           * va a terminar en una caja con 32 amplificadores adentro, y AN5036 §4.4
           * advierte que la disipación CRECE con la temperatura de juntura: el
           * calentamiento no es lineal, se realimenta. Por eso el aviso avisa
           * temprano (85 °C) en vez de justo antes del límite. */
          const char *warn = "";
          if (temp >= 1000)     warn = "  <<< CRITICO: limite VOS0 = 105 C";
          else if (temp >= 850) warn = "  <<< ALTO (limite VOS0 = 105 C)";
          sprintf(uart_buf, "     Temp: %ld.%ld C%s\r\n",
                  temp / 10, t_abs % 10, warn);
          UART_Print(uart_buf);
        }

        // TXbuf/fills-per-SAI/ADC hex dump lines removed: they were bring-up
        // aids for the "ch8-31 silent" and "ADC railed at ±FS" bugs, both
        // root-caused and fixed. Blocking UART7 for ~50ms every 10th
        // heartbeat was costing ~15 spurious underruns per verbose print
        // (see comment above `verbose` — this loop has no UART7 DMA channel,
        // so trimming the block is the cheap fix). ADCpk above still gives a
        // live per-channel peak every second for input→slot mapping.
      }

      // Check SAI status
      HAL_SAI_StateTypeDef sai_state = HAL_SAI_GetState(&hsai_BlockA1);
      if (sai_state != HAL_SAI_STATE_BUSY_TX) {
        sprintf(uart_buf, "WARNING: SAI State = %d (expected BUSY_TX=2)\r\n", sai_state);
        UART_Print(uart_buf);
        sprintf(uart_buf, "SAI Error: 0x%08lX, Last: 0x%08lX\r\n",
                HAL_SAI_GetError(&hsai_BlockA1), last_sai_error);
        UART_Print(uart_buf);
      }

      // 60-second test summary (repeating)
      if (HAL_GetTick() - test_start >= 60000) {
        test_cycle++;
        uint32_t total_time = test_cycle * 60;

        // Snapshot counters BEFORE blocking UART prints
        // (UART printing takes ~50ms total, can cause spurious underrun)
        uint32_t snap_half = half_complete_count;
        uint32_t snap_full = full_complete_count;
        uint32_t snap_err = error_count;
        uint32_t snap_undr = underrun_count;

        // Snapshot latency stats
        uint32_t snap_fill_min = fill_min_cycles;
        uint32_t snap_fill_max = fill_max_cycles;
        uint32_t snap_fill_avg = fill_sample_count > 0
            ? (uint32_t)(fill_sum_cycles / fill_sample_count) : 0;
        uint32_t snap_jitter_min = jitter_min_cycles;
        uint32_t snap_jitter_max = jitter_max_cycles;
        uint32_t snap_jitter_avg = jitter_sample_count > 0
            ? (uint32_t)(jitter_sum_cycles / jitter_sample_count) : 0;

        uint8_t test_passed = (snap_err == 0 && snap_undr == 0
                               && snap_half >= 2900 && snap_full >= 2900);

        UART_Print("\r\n========== 60 SECOND TEST COMPLETE ==========\r\n");
        sprintf(uart_buf, "Cycle #%lu (Total time: %lu s)\r\n", test_cycle, total_time);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Total Half callbacks: %lu (expected ~3000)\r\n", snap_half);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Total Full callbacks: %lu (expected ~3000)\r\n", snap_full);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Total Errors: %lu\r\n", snap_err);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Avg Half rate: %lu/s (expected 50)\r\n", snap_half / 60);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Avg Full rate: %lu/s (expected 50)\r\n", snap_full / 60);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Total Underruns: %lu\r\n", snap_undr);
        UART_Print(uart_buf);

        // Latency characterization report
        uint32_t buf_latency_us = (AUDIO_BUFFER_SAMPLES * 1000) / (AUDIO_SAMPLE_RATE / 1000);
        UART_Print("\r\n--- LATENCY CHARACTERIZATION ---\r\n");
        sprintf(uart_buf, "Buffer: %d samples = %lu us\r\n",
                AUDIO_BUFFER_SAMPLES, buf_latency_us);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Fill min: %lu cyc = %lu us\r\n",
                snap_fill_min, snap_fill_min / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Fill avg: %lu cyc = %lu us\r\n",
                snap_fill_avg, snap_fill_avg / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Fill max: %lu cyc = %lu us\r\n",
                snap_fill_max, snap_fill_max / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Total pipeline: %lu us + %lu us = %lu us\r\n",
                buf_latency_us, snap_fill_avg / CPU_FREQ_MHZ,
                buf_latency_us + snap_fill_avg / CPU_FREQ_MHZ);
        UART_Print(uart_buf);

        UART_Print("\r\n--- JITTER ANALYSIS ---\r\n");
        // Period between consecutive Half (or Full) callbacks = full buffer cycle = 2 × half-buffer
        uint32_t expected_period = (uint32_t)((uint64_t)AUDIO_BUFFER_SAMPLES * 2 * CPU_FREQ_MHZ * 1000000ULL / AUDIO_SAMPLE_RATE);
        sprintf(uart_buf, "Expected period: %lu cyc (%lu us)\r\n",
                expected_period, expected_period / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Measured min:    %lu cyc (%lu us)\r\n",
                snap_jitter_min, snap_jitter_min / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Measured avg:    %lu cyc (%lu us)\r\n",
                snap_jitter_avg, snap_jitter_avg / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Measured max:    %lu cyc (%lu us)\r\n",
                snap_jitter_max, snap_jitter_max / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
        uint32_t jitter_range = snap_jitter_max - snap_jitter_min;
        sprintf(uart_buf, "Jitter range:    %lu cyc = %lu us\r\n",
                jitter_range, jitter_range / CPU_FREQ_MHZ);
        UART_Print(uart_buf);

        if (test_passed) {
          UART_Print("\r\n>>> TEST PASSED: DMA pipeline clean! <<<\r\n");
        } else {
          UART_Print("\r\n>>> TEST FAILED: Check errors above <<<\r\n");
        }
        UART_Print("==============================================\r\n\r\n");

        /* Reset de estadísticas para el próximo ciclo. IRQ apagada para que el
         * ISR no escriba entre medio.
         *
         * ⚠️ Acá ADEMÁS se ponían en cero las diez banderas need_fill_* /
         * rx_3b_*_ready, y eso NO es resetear una estadística: es TIRAR TRABAJO
         * PENDIENTE. Una bandera puesta significa "este medio buffer todavía no
         * se llenó"; borrarla sin llenarlo deja que el DMA reproduzca el
         * contenido anterior — medio buffer repetido en los 32 canales, o sea
         * un pop, UNA VEZ POR MINUTO. Encaja con el "pedito cuando imprime el
         * serial" que se venía reportando: los prints ya se hicieron
         * cooperativos, pero esto seguía descartando fills.
         *
         * Dejarlas puestas es estrictamente mejor: el fill se sirve tarde (dato
         * correcto, posición corrida) en vez de no servirse (dato viejo
         * garantizado). Y si alguna quedara realmente trabada, ahora se ve en
         * el contador acumulado en vez de barrerse bajo la alfombra. */
        __disable_irq();
        underrun_total  += underrun_count;   // el acumulado sobrevive al reset
        sai_error_total += error_count;
        half_complete_count = 0;
        full_complete_count = 0;
        error_count = 0;
        underrun_count = 0;
        last_half = 0;
        last_full = 0;
        // Reset latency tracking
        last_half_isr_cycles = 0;
        last_full_isr_cycles = 0;
        jitter_min_cycles = 0xFFFFFFFF;
        jitter_max_cycles = 0;
        jitter_sum_cycles = 0;
        jitter_sample_count = 0;
        fill_min_cycles = 0xFFFFFFFF;
        fill_max_cycles = 0;
        fill_sum_cycles = 0;
        fill_sample_count = 0;
        __DSB();
        __enable_irq();
        test_start = HAL_GetTick();
      }
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure voltage scaling: VOS0 (480MHz) requires stepping through VOS1 first
   *  VOS1 → enable SYSCFG → VOS0 (overdrive)
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** PLL1: HSE 25MHz / M=5 = 5MHz VCI → N=192 → 960MHz VCO → /P=2 = 480MHz SYSCLK
   *  VCI range: 4-8MHz (RCC_PLL1VCIRANGE_2)
   *  VCO range: wide 192-960MHz (RCC_PLL1VCOWIDE)
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Bus clocks: SYSCLK=480MHz, HCLK=240MHz, APBx=120MHz
   *  HCLK /2 = 240MHz (max for VOS0)
   *  APB1-4 /2 = 120MHz each (I2C1, UART7 derive from APB)
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI1|RCC_PERIPHCLK_SAI2|RCC_PERIPHCLK_UART7;
  PeriphClkInitStruct.PLL3.PLL3M = 5;
  PeriphClkInitStruct.PLL3.PLL3N = 68;
  PeriphClkInitStruct.PLL3.PLL3P = 14;  // 24.576 MHz MCLK (512×fS) for PCM1690 single-rate TDM
  PeriphClkInitStruct.PLL3.PLL3Q = 3;
  PeriphClkInitStruct.PLL3.PLL3R = 2;
  PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
  PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
  PeriphClkInitStruct.PLL3.PLL3FRACN = 6660;  // VCO=344.0625MHz, /14 = 24.576 MHz MCLK (512×fS)
  PeriphClkInitStruct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL3;
  PeriphClkInitStruct.Sai23ClockSelection = RCC_SAI23CLKSOURCE_PLL3;
  PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_PLL3;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x307075B1;  // 100kHz SM @ 120MHz APB1 (was 0x00909FCE @ 37.5MHz)
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SAI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SAI1_Init(void)
{

  /* USER CODE BEGIN SAI1_Init 0 */

  /* USER CODE END SAI1_Init 0 */

  /* USER CODE BEGIN SAI1_Init 1 */

  /* USER CODE END SAI1_Init 1 */
  hsai_BlockA1.Instance = SAI1_Block_A;
  hsai_BlockA1.Init.Protocol = SAI_FREE_PROTOCOL;
  hsai_BlockA1.Init.AudioMode = SAI_MODEMASTER_TX;
  hsai_BlockA1.Init.DataSize = SAI_DATASIZE_24;  // 24-bit: SAI sends bits [23:0] of DMA word (right-aligned, RM0433)
  hsai_BlockA1.Init.FirstBit = SAI_FIRSTBIT_MSB;
  hsai_BlockA1.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
  hsai_BlockA1.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockA1.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockA1.Init.NoDivider = SAI_MCK_OVERSAMPLING_DISABLE;
  hsai_BlockA1.Init.MckOverSampling = SAI_MCK_OVERSAMPLING_ENABLE;  // MCLK=2×BCK → 512×fS for single-rate TDM
  hsai_BlockA1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockA1.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_48K;
  hsai_BlockA1.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockA1.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockA1.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockA1.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockA1.Init.PdmInit.Activation = DISABLE;
  hsai_BlockA1.Init.PdmInit.MicPairsNbr = 1;
  hsai_BlockA1.Init.PdmInit.ClockEnable = SAI_PDM_CLOCK1_ENABLE;
  hsai_BlockA1.FrameInit.FrameLength = 256;            // TDM8: 8 slots × 32 bits
  hsai_BlockA1.FrameInit.ActiveFrameLength = 1;        // Short pulse: 1 BCK at frame start
  hsai_BlockA1.FrameInit.FSDefinition = SAI_FS_STARTFRAME; // Pulse marks frame start (not L/R split)
  hsai_BlockA1.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;   // Negative pulse: falling edge = I2S frame start
  hsai_BlockA1.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT; // I2S timing: data 1 BCK after FS
  hsai_BlockA1.SlotInit.FirstBitOffset = 0;
  hsai_BlockA1.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
  hsai_BlockA1.SlotInit.SlotNumber = 8;                // TDM8: 8 slots
  hsai_BlockA1.SlotInit.SlotActive = 0x000000FF;       // All 8 slots active
  if (HAL_SAI_Init(&hsai_BlockA1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SAI1_Init 2 */

  /* SAI1_B: Slave TX synchronized with SAI1_A (channels 8-15) */
  hsai_BlockB1.Instance = SAI1_Block_B;
  hsai_BlockB1.Init.Protocol = SAI_FREE_PROTOCOL;
  hsai_BlockB1.Init.AudioMode = SAI_MODESLAVE_TX;
  hsai_BlockB1.Init.DataSize = SAI_DATASIZE_24;  // match master
  hsai_BlockB1.Init.FirstBit = SAI_FIRSTBIT_MSB;
  hsai_BlockB1.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
  hsai_BlockB1.Init.Synchro = SAI_SYNCHRONOUS;
  hsai_BlockB1.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockB1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockB1.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockB1.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockB1.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockB1.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockB1.Init.PdmInit.Activation = DISABLE;
  hsai_BlockB1.Init.PdmInit.MicPairsNbr = 1;
  hsai_BlockB1.Init.PdmInit.ClockEnable = SAI_PDM_CLOCK1_ENABLE;
  hsai_BlockB1.FrameInit.FrameLength = 256;
  hsai_BlockB1.FrameInit.ActiveFrameLength = 1;        // Match master: short pulse
  hsai_BlockB1.FrameInit.FSDefinition = SAI_FS_STARTFRAME;
  hsai_BlockB1.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;   // Match master: negative pulse
  hsai_BlockB1.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;
  hsai_BlockB1.SlotInit.FirstBitOffset = 0;
  hsai_BlockB1.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
  hsai_BlockB1.SlotInit.SlotNumber = 8;
  hsai_BlockB1.SlotInit.SlotActive = 0x000000FF;
  if (HAL_SAI_Init(&hsai_BlockB1) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE END SAI1_Init 2 */

}

/**
  * @brief UART7 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART7_Init(void)
{

  /* USER CODE BEGIN UART7_Init 0 */

  /* USER CODE END UART7_Init 0 */

  /* USER CODE BEGIN UART7_Init 1 */

  /* USER CODE END UART7_Init 1 */
  huart7.Instance = UART7;
  huart7.Init.BaudRate = 115200;
  huart7.Init.WordLength = UART_WORDLENGTH_8B;
  huart7.Init.StopBits = UART_STOPBITS_1;
  huart7.Init.Parity = UART_PARITY_NONE;
  huart7.Init.Mode = UART_MODE_TX_RX;
  huart7.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart7.Init.OverSampling = UART_OVERSAMPLING_16;
  huart7.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart7.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart7.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart7) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart7, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart7, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart7) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART7_Init 2 */

  /* USER CODE END UART7_Init 2 */

}

/**
  * @brief UART4 Initialization Function (ESP32 communication)
  * @note  PA0=TX, PA1=RX, 115200 8N1
  */
static void MX_UART4_Init(void)
{
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart4.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart4, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart4, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SAI2 Initialization Function (DAC2 + DAC3)
  * @note  SAI2_A: Master TX TDM8, PD13(FS)/PD12(SCK)/PD11(SD)
  *        SAI2_B: Slave TX synced to SAI2_A, PD8(SD only)
  *        Both use PLL3 (same as SAI1) for identical sample rate
  */
static void MX_SAI2_Init(void)
{
  /* SAI2_A: Master TX - identical TDM8 config as SAI1_A */
  hsai_BlockA2.Instance = SAI2_Block_A;
  hsai_BlockA2.Init.Protocol = SAI_FREE_PROTOCOL;
  hsai_BlockA2.Init.AudioMode = SAI_MODEMASTER_TX;
  hsai_BlockA2.Init.DataSize = SAI_DATASIZE_24;
  hsai_BlockA2.Init.FirstBit = SAI_FIRSTBIT_MSB;
  hsai_BlockA2.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
  hsai_BlockA2.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockA2.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockA2.Init.NoDivider = SAI_MCK_OVERSAMPLING_DISABLE;
  hsai_BlockA2.Init.MckOverSampling = SAI_MCK_OVERSAMPLING_ENABLE;  // MCLK=2×BCK → 512×fS for single-rate TDM
  hsai_BlockA2.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockA2.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_48K;
  hsai_BlockA2.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockA2.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockA2.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockA2.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockA2.Init.PdmInit.Activation = DISABLE;
  hsai_BlockA2.Init.PdmInit.MicPairsNbr = 1;
  hsai_BlockA2.Init.PdmInit.ClockEnable = SAI_PDM_CLOCK1_ENABLE;
  hsai_BlockA2.FrameInit.FrameLength = 256;
  hsai_BlockA2.FrameInit.ActiveFrameLength = 1;        // Short pulse: 1 BCK at frame start
  hsai_BlockA2.FrameInit.FSDefinition = SAI_FS_STARTFRAME;
  hsai_BlockA2.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;   // Negative pulse
  hsai_BlockA2.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT; // I2S timing: data 1 BCK after FS
  hsai_BlockA2.SlotInit.FirstBitOffset = 0;
  hsai_BlockA2.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
  hsai_BlockA2.SlotInit.SlotNumber = 8;
  hsai_BlockA2.SlotInit.SlotActive = 0x000000FF;
  if (HAL_SAI_Init(&hsai_BlockA2) != HAL_OK)
  {
    Error_Handler();
  }

  /* SAI2_B: Slave TX - synchronized with SAI2_A (shares SCK/FS internally) */
  hsai_BlockB2.Instance = SAI2_Block_B;
  hsai_BlockB2.Init.Protocol = SAI_FREE_PROTOCOL;
  hsai_BlockB2.Init.AudioMode = SAI_MODESLAVE_TX;
  hsai_BlockB2.Init.DataSize = SAI_DATASIZE_24;
  hsai_BlockB2.Init.FirstBit = SAI_FIRSTBIT_MSB;
  hsai_BlockB2.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
  hsai_BlockB2.Init.Synchro = SAI_SYNCHRONOUS;
  hsai_BlockB2.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockB2.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockB2.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockB2.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockB2.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockB2.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockB2.Init.PdmInit.Activation = DISABLE;
  hsai_BlockB2.Init.PdmInit.MicPairsNbr = 1;
  hsai_BlockB2.Init.PdmInit.ClockEnable = SAI_PDM_CLOCK1_ENABLE;
  hsai_BlockB2.FrameInit.FrameLength = 256;
  hsai_BlockB2.FrameInit.ActiveFrameLength = 1;        // Match master: short pulse
  hsai_BlockB2.FrameInit.FSDefinition = SAI_FS_STARTFRAME;
  hsai_BlockB2.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;   // Match master: negative pulse
  hsai_BlockB2.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;
  hsai_BlockB2.SlotInit.FirstBitOffset = 0;
  hsai_BlockB2.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
  hsai_BlockB2.SlotInit.SlotNumber = 8;
  hsai_BlockB2.SlotInit.SlotActive = 0x000000FF;
  if (HAL_SAI_Init(&hsai_BlockB2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SAI3 Initialization Function (ADC input — AK5538VN)
  * @note  SAI3_B: Master RX, TDM256 8ch 32-bit, PD8(SCK)/PD9(SD)/PD10(FS) AF6
  *        STM32 generates BICK (12.288MHz) and LRCK (48kHz). AK5538 is slave (MSN=GND).
  *        TDM256: FrameLength=256 (8 slots × 32 bit), BICK = 256×48kHz = 12.288MHz
  */
static void MX_SAI3_Init(void)
{
  hsai_BlockB3.Instance = SAI3_Block_B;
  hsai_BlockB3.Init.Protocol = SAI_FREE_PROTOCOL;
  hsai_BlockB3.Init.AudioMode = SAI_MODEMASTER_RX;
  hsai_BlockB3.Init.DataSize = SAI_DATASIZE_32;          // AK5538VN: 32-bit word
  hsai_BlockB3.Init.FirstBit = SAI_FIRSTBIT_MSB;
  hsai_BlockB3.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
  hsai_BlockB3.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockB3.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockB3.Init.NoDivider = SAI_MCK_OVERSAMPLING_DISABLE;
  hsai_BlockB3.Init.MckOverSampling = SAI_MCK_OVERSAMPLING_DISABLE;
  hsai_BlockB3.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockB3.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_48K;
  hsai_BlockB3.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockB3.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockB3.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockB3.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockB3.Init.PdmInit.Activation = DISABLE;
  hsai_BlockB3.Init.PdmInit.MicPairsNbr = 1;
  hsai_BlockB3.Init.PdmInit.ClockEnable = SAI_PDM_CLOCK1_ENABLE;
  hsai_BlockB3.FrameInit.FrameLength = 256;              // TDM256: 8 slots × 32 bits
  hsai_BlockB3.FrameInit.ActiveFrameLength = 1;          // Short FS pulse (TDM start-frame)
  hsai_BlockB3.FrameInit.FSDefinition = SAI_FS_STARTFRAME;
  hsai_BlockB3.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
  hsai_BlockB3.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;
  hsai_BlockB3.SlotInit.FirstBitOffset = 0;
  hsai_BlockB3.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
  hsai_BlockB3.SlotInit.SlotNumber = 8;                  // 8 ADC channels
  hsai_BlockB3.SlotInit.SlotActive = 0x000000FF;         // All 8 slots
  if (HAL_SAI_Init(&hsai_BlockB3) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_ADC3_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  // ADC3 kernel clock: per_ck (HSI 64MHz default), DIV8 = 8MHz
  RCC_PeriphCLKInitTypeDef adc_clk = {0};
  adc_clk.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  adc_clk.AdcClockSelection = RCC_ADCCLKSOURCE_CLKP;
  if (HAL_RCCEx_PeriphCLKConfig(&adc_clk) != HAL_OK) {
    UART_Print("ADC clock config FAILED\r\n");
    return;
  }

  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV8;
  hadc3.Init.Resolution = ADC_RESOLUTION_16B;
  hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc3.Init.LowPowerAutoWait = DISABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc3.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc3) != HAL_OK) {
    UART_Print("ADC3 init FAILED\r\n");
    return;
  }

  HAL_ADCEx_Calibration_Start(&hadc3, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);

  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK) {
    UART_Print("ADC3 channel config FAILED\r\n");
  }
}

static int32_t Read_CPU_Temperature(void)
{
  HAL_ADC_Start(&hadc3);
  if (HAL_ADC_PollForConversion(&hadc3, 10) != HAL_OK) {
    return -999;
  }
  uint32_t raw = HAL_ADC_GetValue(&hadc3);
  HAL_ADC_Stop(&hadc3);

  // Factory calibration: TS_CAL1 @30C, TS_CAL2 @110C (16-bit, VREF=3.3V)
  uint16_t ts_cal1 = *((uint16_t *)0x1FF1E820);
  uint16_t ts_cal2 = *((uint16_t *)0x1FF1E840);

  // Linear interpolation, x10 for 0.1C precision (no float)
  int32_t temp_x10 = 300 + (int32_t)(raw - ts_cal1) * 800 / (int32_t)(ts_cal2 - ts_cal1);
  return temp_x10;
}

/**
  * @brief I2C3 Initialization Function (AK5538VN ADC — PA8/PC9 AF4)
  */
static void MX_I2C3_Init(void)
{
  hi2c3.Instance = I2C3;
  hi2c3.Init.Timing = 0x307075B1;   // 100kHz SM @ 120MHz APB1 (same as I2C1)
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_TIM3_Init(void)
{
  TIM_OC_InitTypeDef sConfigOC = {0};

  // TIM3 clock = APB1 × 2 = 240MHz
  // Prescaler 239 → counter @ 1MHz, ARR 999 → PWM @ 1kHz, 1000 steps resolution
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 239;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = DIMMER_ARR;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
    UART_Print("TIM3 PWM init FAILED\r\n");
    return;
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK ||
      HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK ||
      HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK ||
      HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK) {
    UART_Print("TIM3 CH1-4 config FAILED\r\n");
    return;
  }

  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);  // D4 PA6 Yellow
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);  // D5 PA7 Red
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);  // D6 PB0 Green
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);  // D7 PB1 White
  UART_Print("TIM3 PWM started — 4 LEDs active (1kHz, 300 steps, 3s cycle)\r\n");
}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration (SAI1_A) */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  /* DMA1_Stream1_IRQn interrupt configuration (SAI2_A) */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  /* DMA1_Stream2_IRQn interrupt configuration (SAI2_B) */
  HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
  /* DMA1_Stream3_IRQn interrupt configuration (SAI1_B) */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
  /* DMA1_Stream4_IRQn interrupt configuration (SAI3_B RX) */
  HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
  /* DMAMUX1_OVR_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMAMUX1_OVR_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMAMUX1_OVR_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Enable all port clocks */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /* ---- Safe pre-init output levels ---- */
  /* DAC_RST LOW (all 4 PCM1690 held in reset until MCLK is running) */
  HAL_GPIO_WritePin(DAC_RST_PORT, DAC_RST_PIN, GPIO_PIN_RESET);
  /* DAC_MUTE_IN LOW (AMUTEI LOW = muted, safe during power-up) */
  HAL_GPIO_WritePin(DAC_MUTE_IN_PORT, DAC_MUTE_IN_PIN, GPIO_PIN_RESET);
  /* PAM_SD LOW (all 32 amps off until audio is ready) */
  HAL_GPIO_WritePin(PAM_SD_PORT, PAM_SD_PIN, GPIO_PIN_RESET);
  /* AK_PDN LOW = Reset & Power-down (chip OFF until SAI3 ready) — datasheet: L=PDN, H=Normal */
  HAL_GPIO_WritePin(AK_PDN_PORT, AK_PDN_PIN, GPIO_PIN_RESET);
  /* EEPROM_WP LOW (write enabled — needed for config save) */
  HAL_GPIO_WritePin(EEPROM_WP_PORT, EEPROM_WP_PIN, GPIO_PIN_RESET);

  /* ---- Output: DAC shared control ---- */
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

  GPIO_InitStruct.Pin = DAC_RST_PIN | DAC_MUTE_IN_PIN;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);    // PC11=DAC_RST, PC12=DAC_MUTE_IN

  /* ---- Input: DAC_MUTE_OUT (AMUTEO wire-OR) ---- */
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Pin  = DAC_MUTE_OUT_PIN;
  HAL_GPIO_Init(DAC_MUTE_OUT_PORT, &GPIO_InitStruct);    // PC10

  /* ---- Output: PAM amp bank shutdown ---- */
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Pin  = PAM_SD_PIN;
  HAL_GPIO_Init(PAM_SD_PORT, &GPIO_InitStruct);           // PD3

  /* ---- Output: AK5538 control ---- */
  GPIO_InitStruct.Pin = AK_PDN_PIN | AK_DIF1_PIN;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);                 // PC7=AK_PDN, PC8=AK_DIF1
  HAL_GPIO_WritePin(AK_DIF1_PORT, AK_DIF1_PIN, GPIO_PIN_RESET); // DIF1=LOW (normal)

  /* ---- Input: AK5538 overflow flag ---- */
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Pin  = AK_OVF_PIN;
  HAL_GPIO_Init(AK_OVF_PORT, &GPIO_InitStruct);           // PA15

  /* ---- Input: User button K2 ---- */
  GPIO_InitStruct.Pin = OPT_SW_K2_PIN;
  HAL_GPIO_Init(OPT_SW_K2_PORT, &GPIO_InitStruct);        // PA4

  /* ---- Output: EEPROM write protect ---- */
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Pin  = EEPROM_WP_PIN;
  HAL_GPIO_Init(EEPROM_WP_PORT, &GPIO_InitStruct);        // PB5

  /* ---- Analog: unused pins (EMI reduction, per DS10_DESIGN_REVIEW §11.11) ---- */
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Pin  = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_9 | GPIO_PIN_10
                       | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);   // PE0,PE1,PE9,PE10,PE12-PE15

  GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);   // PB2,PB4,PB15

  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3
                       | GPIO_PIN_6;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);   // PC0,PC1,PC2,PC3,PC6

  GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_5 | GPIO_PIN_6
                       | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);   // PD2,PD5,PD6,PD14,PD15

  GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_9 | GPIO_PIN_10;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);   // PA5,PA9,PA10
}

/* USER CODE BEGIN 4 */

/**
 * @brief Print string via UART (blocking)
 * @note  DS10 debug goes out UART7 (PE7/PE8) → J7 header pins 7/8 → ST-Link VCP.
 *        UART4 (PA0/PA1) stays reserved for the ESP32 binary protocol.
 */
static void UART_Print(const char *str) {
  /* TX byte a byte, atendiendo los fills pendientes mientras se espera el TXE.
   *
   * Antes esto era un HAL_UART_Transmit bloqueante. A 115200 cada carácter son
   * ~87 µs, así que el heartbeat (~238 chars) bloqueaba el main loop ~20 ms —
   * el DOBLE del deadline de 10 ms del fill. Cada deadline perdido hace que el
   * DMA reemita medio buffer viejo en los 32 canales a la vez: un salto de fase
   * audible como un "pop", una vez por segundo, y más fuerte cada 10 s cuando
   * sale el bloque verbose. Medido en el modo 12 (tono enfocado) el 2026-08-14.
   *
   * Ahora el tiempo muerto entre carácter y carácter se usa para servir los
   * fills. La consola se ve igual: que la UART quede idle unos µs de más entre
   * bytes no corrompe nada, sólo espacia los caracteres. */
  while (*str) {
    while (!(huart7.Instance->ISR & USART_ISR_TXE_TXFNF)) {
      Service_Fills();
    }
    huart7.Instance->TDR = (uint8_t)*str++;
  }
}

/**
 * @brief Verifica que cada estructura crítica esté en la memoria que le toca.
 *
 * Silencioso cuando está bien; grita cuando no. Las dos clases de bug que más
 * caro salieron en este proyecto son de UBICACIÓN, no de lógica:
 *   - un buffer de DMA en DTCM → el DMA no lo alcanza y no sale audio;
 *   - una tabla leída en el lazo de fill en FLASH → con el D-cache apagado se
 *     pagan wait states por muestra (el modo 14 perdía el 15% de los deadlines
 *     por esto, ver docs/ds11_perf.md §7).
 * Ninguna de las dos da error de compilación y ninguna se ve leyendo el código.
 *
 * Reemplaza al viejo Print_DMA_Config(), que volcaba 100 líneas de registros en
 * cada arranque y hacía años que nadie llamaba.
 */
#define IN_RAM_D2(p) ((uint32_t)(p) >= 0x30000000u && (uint32_t)(p) < 0x30048000u)
#define IN_DTCM(p)   ((uint32_t)(p) >= 0x20000000u && (uint32_t)(p) < 0x20020000u)

static void Check_Memory_Placement(void) {
  struct { const void *addr; const char *name; uint8_t want_dma; } chk[] = {
    { audio_buffer,        "audio_buffer(SAI1_A)", 1 },
    { audio_buffer_1b,     "audio_buffer_1b",      1 },
    { audio_buffer_2a,     "audio_buffer_2a",      1 },
    { audio_buffer_2b,     "audio_buffer_2b",      1 },
    { audio_buffer_3b_rx,  "audio_buffer_3b_rx",   1 },
    { delay_line,          "delay_line",           0 },
    { beam_line,           "beam_line",            0 },
    { sine_table_1khz,     "sine_table_1khz",      0 },
    { mf_phoff_q16,        "mf_phoff_q16",         0 },
  };
  uint32_t bad = 0;
  for (uint32_t i = 0; i < sizeof(chk) / sizeof(chk[0]); i++) {
    uint8_t ok = chk[i].want_dma ? IN_RAM_D2(chk[i].addr) : IN_DTCM(chk[i].addr);
    if (!ok) {
      bad++;
      sprintf(uart_buf, "  !! %s @ 0x%08lX — deberia estar en %s\r\n",
              chk[i].name, (unsigned long)(uint32_t)chk[i].addr,
              chk[i].want_dma ? "RAM_D2 (accesible por DMA)" : "DTCM (0 wait states)");
      UART_Print(uart_buf);
    }
  }
  if (bad) {
    UART_Print("  !! REVISAR EL LINKER SCRIPT / LOS ATRIBUTOS DE SECCION !!\r\n");
  } else {
    UART_Print("Memoria: buffers DMA en RAM_D2, tablas calientes en DTCM — OK\r\n");
  }
}

/**
 * @brief Recompute per-mic RX delays for a far-field steering angle.
 * @param az_deg Azimuth in degrees; 0 = broadside, + toward higher mic index.
 *
 * τ_m = (x_m · sin(az)) / c, normalized so the smallest delay is 0, rounded
 * to whole samples @48kHz. Runs only on state/config changes (float math is
 * fine here — never per-sample). Prints the resulting table (integers only:
 * nano.specs printf has no %f).
 */
static void Beam_UpdateDelays(int16_t az_deg) {
  float s = sinf((float)az_deg * 3.14159265f / 180.0f);
  float d[MIC_COUNT];
  float dmin = 1e9f;
  for (uint32_t m = 0; m < MIC_COUNT; m++) {
    d[m] = mic_pos_x_mm[m] * s * (float)AUDIO_SAMPLE_RATE / SPEED_OF_SOUND_MM_S;
    if (d[m] < dmin) dmin = d[m];
  }
  for (uint32_t m = 0; m < MIC_COUNT; m++) {
    int32_t di = (int32_t)(d[m] - dmin + 0.5f);
    if (di > MAX_DELAY_SAMPLES) di = MAX_DELAY_SAMPLES;
    rx_delay[m] = (uint16_t)di;
  }
  beam_az_deg = az_deg;
  sprintf(uart_buf, "BEAM delays (az=%d): [%u %u %u %u %u %u %u %u] samples\r\n",
          (int)az_deg, rx_delay[0], rx_delay[1], rx_delay[2], rx_delay[3],
          rx_delay[4], rx_delay[5], rx_delay[6], rx_delay[7]);
  UART_Print(uart_buf);
}

/**
 * @brief Build speaker_pos[32] from the 4×8 brick-pattern array geometry.
 *
 * ch = row*8 + col (row-major placeholder — validate with the channel walker
 * on real DS11 hardware). Row 1 (r=0) = ch0-7 = U2, col 0 = leftmost in the
 * listener frame. Rows 2&4 (r=1,3) shifted +SPK_ROW_DX_MM in X.
 * Also computes the array center, the origin for focal-point presets.
 */
static void Beam_InitTxGeometry(void) {
  float sx = 0, sy = 0;
  for (uint32_t ch = 0; ch < BEAM_NUM_CHANNELS; ch++) {
    uint32_t r = ch / SPK_COLS, c = ch % SPK_COLS;
    spk_x_mm[ch] = (float)c * SPK_X_PITCH_MM + ((r & 1) ? SPK_ROW_DX_MM : 0.0f);
    spk_y_mm[ch] = (float)r * SPK_Y_PITCH_MM;
    speaker_sign[ch] = 1;
    sx += spk_x_mm[ch];
    sy += spk_y_mm[ch];
  }
  spk_center_x_mm = sx / BEAM_NUM_CHANNELS;
  spk_center_y_mm = sy / BEAM_NUM_CHANNELS;
  sprintf(uart_buf, "TX array: 4x8 brick, X0..%d Y0..%d, center (%d,%d) mm\r\n",
          (int)(7 * SPK_X_PITCH_MM + SPK_ROW_DX_MM), (int)(3 * SPK_Y_PITCH_MM),
          (int)spk_center_x_mm, (int)spk_center_y_mm);
  UART_Print(uart_buf);
}

/**
 * @brief Recompute per-speaker TX delays for a near-field focal point.
 * @param fx_mm,fy_mm,fz_mm Focus relative to the array center (listener
 *        frame: +X right facing the cones, +Y toward row 4, +Z outward).
 *
 * d_k = |p − spk_k| / c; delay_k = round((max_j d_j − d_k) · fs): the
 * farthest speaker fires first (delay 0), the nearest last, so all
 * wavefronts arrive at the focus together. Prints one row per line for
 * geometric sanity-checking. Runs only on state changes (float OK).
 */
/* Versión silenciosa. La ruidosa (Beam_UpdateTxFocus, abajo) imprime la tabla de
 * delays por fila, que es útil al entrar a un modo pero imposible en el modo
 * BARRIDO, que reapunta 20 veces por segundo. */
static void Beam_SetTxFocus(int16_t fx_mm, int16_t fy_mm, int16_t fz_mm) {
  tx_focus_x_mm = fx_mm;
  tx_focus_y_mm = fy_mm;
  tx_focus_z_mm = fz_mm;
  float px = spk_center_x_mm + (float)fx_mm;
  float py = spk_center_y_mm + (float)fy_mm;
  float pz = (float)fz_mm;
  float d[BEAM_NUM_CHANNELS];
  float dmax = 0;
  for (uint32_t ch = 0; ch < BEAM_NUM_CHANNELS; ch++) {
    float dx = px - spk_x_mm[ch], dy = py - spk_y_mm[ch];
    d[ch] = sqrtf(dx * dx + dy * dy + pz * pz);
    if (d[ch] > dmax) dmax = d[ch];
  }
  for (uint32_t ch = 0; ch < BEAM_NUM_CHANNELS; ch++) {
    // Q8 fixed point: keep the sub-sample fraction instead of rounding
    int32_t dq = (int32_t)((dmax - d[ch]) * (float)AUDIO_SAMPLE_RATE * 256.0f
                           / SPEED_OF_SOUND_MM_S + 0.5f);
    if (dq > (TX_MAX_DELAY_SAMPLES << 8)) dq = TX_MAX_DELAY_SAMPLES << 8;
    tx_delay_q8[ch] = (uint16_t)dq;
  }
}

/** @brief Igual que Beam_SetTxFocus pero además imprime la tabla de delays
 *         por fila, para sanity-check geométrico al entrar a un modo. */
static void Beam_UpdateTxFocus(int16_t fx_mm, int16_t fy_mm, int16_t fz_mm) {
  Beam_SetTxFocus(fx_mm, fy_mm, fz_mm);
  sprintf(uart_buf, "TX focus (%d,%d,%d)mm rel center — delays/row (samples.frac):\r\n",
          (int)fx_mm, (int)fy_mm, (int)fz_mm);
  UART_Print(uart_buf);
  for (uint32_t r = 0; r < SPK_ROWS; r++) {
    uint16_t *t = &tx_delay_q8[r * SPK_COLS];
    sprintf(uart_buf,
            "  r%lu: %u.%02u %u.%02u %u.%02u %u.%02u %u.%02u %u.%02u %u.%02u %u.%02u\r\n",
            (unsigned long)r,
            t[0] >> 8, (t[0] & 0xFF) * 100 / 256, t[1] >> 8, (t[1] & 0xFF) * 100 / 256,
            t[2] >> 8, (t[2] & 0xFF) * 100 / 256, t[3] >> 8, (t[3] & 0xFF) * 100 / 256,
            t[4] >> 8, (t[4] & 0xFF) * 100 / 256, t[5] >> 8, (t[5] & 0xFF) * 100 / 256,
            t[6] >> 8, (t[6] & 0xFF) * 100 / 256, t[7] >> 8, (t[7] & 0xFF) * 100 / 256);
    UART_Print(uart_buf);
  }
}

/**
 * @brief Recompute one MULTIFOCUS source: focal delays + phase offsets.
 * @param src   Source index (0..MF_NUM_SRC-1).
 * @param f_hz  Tone frequency (múltiplo de 100 Hz para fase exacta entre bancos).
 * @param fx_mm,fy_mm,fz_mm  Focus relative to array center (listener frame).
 *
 * Misma geometría que Beam_UpdateTxFocus pero escribiendo la fila src de
 * mf_delay_q8 — esta tabla es la que la intervención de 8 entradas va a
 * reusar tal cual. Además precalcula el offset de fase equivalente
 * (delay_q8 × inc >> 8, mod período) para que el fill no haga math de 64
 * bits por muestra. Solo en cambios de estado (float OK).
 */
/* Versión silenciosa. La ruidosa (con print) es Beam_UpdateMFocus, más abajo:
 * cuando se re-apunta UNA fuente el log es útil, pero al entrar a un modo se
 * llamaba 8 veces seguidas y esos 8 prints costaban 6 deadlines de fill —
 * medido: entrar al modo 15 daba +6 underruns SIEMPRE, y ninguna otra
 * transición costaba nada. Los defaults ahora imprimen una sola línea. */
static void Beam_SetMFocus(uint8_t src, uint16_t f_hz,
                           int16_t fx_mm, int16_t fy_mm, int16_t fz_mm) {
  mf_freq_hz[src] = f_hz;
  mf_fx_mm[src] = fx_mm;
  mf_fy_mm[src] = fy_mm;
  mf_fz_mm[src] = fz_mm;
  mf_inc_q16[src] = ((uint32_t)f_hz << 16) / 1000u;
  float px = spk_center_x_mm + (float)fx_mm;
  float py = spk_center_y_mm + (float)fy_mm;
  float pz = (float)fz_mm;
  float d[BEAM_NUM_CHANNELS];
  float dmax = 0;
  for (uint32_t ch = 0; ch < BEAM_NUM_CHANNELS; ch++) {
    float dx = px - spk_x_mm[ch], dy = py - spk_y_mm[ch];
    d[ch] = sqrtf(dx * dx + dy * dy + pz * pz);
    if (d[ch] > dmax) dmax = d[ch];
  }
  for (uint32_t ch = 0; ch < BEAM_NUM_CHANNELS; ch++) {
    int32_t dq = (int32_t)((dmax - d[ch]) * (float)AUDIO_SAMPLE_RATE * 256.0f
                           / SPEED_OF_SOUND_MM_S + 0.5f);
    if (dq > (TX_MAX_DELAY_SAMPLES << 8)) dq = TX_MAX_DELAY_SAMPLES << 8;
    mf_delay_q8[src][ch] = (uint16_t)dq;
    mf_phoff_q16[src][ch] = (uint32_t)((((uint64_t)dq * mf_inc_q16[src]) >> 8)
                                       % TONE_PERIOD_Q16);
  }
}

/** @brief Igual que Beam_SetMFocus pero además lo informa. Para cuando se
 *         re-apunta UNA fuente en vivo (POST /beam type=mf). */
static void Beam_UpdateMFocus(uint8_t src, uint16_t f_hz,
                              int16_t fx_mm, int16_t fy_mm, int16_t fz_mm) {
  Beam_SetMFocus(src, f_hz, fx_mm, fy_mm, fz_mm);
  sprintf(uart_buf, "MFOCUS src%u: %uHz @ (%d,%d,%d)mm\r\n", (unsigned)src,
          (unsigned)f_hz, (int)fx_mm, (int)fy_mm, (int)fz_mm);
  UART_Print(uart_buf);
}

/** @brief Reset the MULTIFOCUS sources to the 4 default tones/foci. */
static void MFocus_ApplyDefaults(void) {
  for (uint8_t s = 0; s < MF_NUM_SRC; s++)
    Beam_SetMFocus(s, mf_def_freq[s], mf_def_fx[s], 0, 1000);
  sprintf(uart_buf, "MFOCUS: %u tonos en sus focos por defecto\r\n",
          (unsigned)MF_NUM_SRC);
  UART_Print(uart_buf);
}

/** @brief Defaults del modo 15: las 8 entradas repartidas en X, −700..+700mm
 *         paso 200, a la altura del centro del array (DS11: 4 filas), a 1m.
 *         (La frecuencia queda seteada pero el modo de entradas no la usa.) */
static void MFocusIn_ApplyDefaults(void) {
  for (uint8_t s = 0; s < MF_IN_NUM_SRC; s++)
    Beam_SetMFocus(s, mf_def_freq[s % MF_NUM_SRC],
                   (int16_t)(-700 + 200 * (int16_t)s), 0, 1000);
  sprintf(uart_buf, "MFOCUS: %u entradas repartidas de -700 a +700mm @1m\r\n",
          (unsigned)MF_IN_NUM_SRC);
  UART_Print(uart_buf);
}

/* Master volume via PCM1690 attenuation. ATDAx = 255 + dB×2 (0.5 dB/step);
 * db≤−64 → mute. Writes the 8 channels of all 4 DACs (unpopulated ones just
 * NAK the I2C, harmless). Runs from the main loop only — never the ISR.
 * Returns the ATDA byte written, for logging. */
static uint8_t Apply_Master_Volume(int8_t db) {
  uint8_t atten;
  if (db <= -64) {
    atten = PCM1690_ATTEN_MUTE;
  } else {
    int16_t a = (db > 0) ? 0 : db;          // clamp positives to 0 dB
    atten = (uint8_t)(255 + a * 2);
  }
  /* 32 escrituras I2C (4 DACs × 8 canales) a 100 kHz ≈ 300 µs cada una =
   * **~9,6 ms de bloqueo**, justo en el borde del deadline de fill de 10 ms del
   * buffer de 480 muestras. Con el de 240 (deadline 5 ms) se perderían dos
   * deadlines seguros. Por eso se sirve el pipeline entre transacción y
   * transacción: el I2C en sí dura ~300 µs, que entra en cualquier caso.
   *
   * Sin esto, cada cambio de volumen mete un pop en los 32 canales — el mismo
   * mecanismo que los prints y la EEPROM antes del arreglo del 2026-08-14.
   * Es seguro llamar Service_Fills() acá porque esta función corre sólo desde
   * el main loop (ver el comentario de arriba), nunca desde una ISR. */
  for (uint32_t d = 0; d < 4; d++)
    for (uint8_t ch = 1; ch <= 8; ch++) {
      PCM1690_SetAttenuation(&hi2c1, dac_addrs[d], ch, atten);
      Service_Fills();
    }
  return atten;
}

/**
 * @brief Apply a custom (non-preset) beam request from the ESP32.
 *
 * Same flag hygiene as UI_ApplyState: exactly one output mode active, any
 * running AUTOFOCUS scan aborted. ui_state_idx is left untouched — the next
 * K2 press resumes cycling from the last preset (documented behavior).
 */
static void UI_ApplyCustomBeam(const ds_beam_req_t *req) {
  /* A partir de acá ui_state_idx deja de describir lo que suena. Se avisa por
   * telemetría (STATE_F_CUSTOM) en vez de mover el índice, porque moverlo
   * rompería la otra propiedad que sí se quiere: que el próximo K2 retome el
   * ciclo desde el último preset elegido. */
  custom_beam_active = 1;
  if (req->type == 2) {
    /* MULTIFOCUS: az_deg lleva el índice de fuente (reuso del struct, sin
     * cambio de tamaño de trama). Validar ANTES de tocar flags — un src
     * inválido no debe matar el modo en curso. */
    uint8_t src = (uint8_t)req->az_deg;
    if (src >= MF_MAX_SRC) {
      sprintf(uart_buf, "MFOCUS: src %u fuera de rango (0-%u)\r\n",
              (unsigned)src, (unsigned)(MF_MAX_SRC - 1));
      UART_Print(uart_buf);
      return;
    }
    walker_enabled = 0;
    adc_monitor_enabled = 0;
    autofocus_enabled = 0;
    cal_state = CAL_IDLE;
    cal_capturing = 0;
    beam_enabled = 0;
    tx_steer_enabled = 0;
    tone_enabled = 0;
    /* Si ya hay un modo MULTIFOCUS activo (tonos O entradas) se respeta;
     * si no, entra al de tonos. */
    if (!mfocus_enabled && !mfocus_in_enabled) { mfocus_enabled = 1; mf_frame = 0; }
    /* Mantiene la frecuencia actual de la fuente; solo re-apunta su foco. */
    Beam_UpdateMFocus(src, mf_freq_hz[src], req->fx_mm, req->fy_mm, req->fz_mm);
    return;
  }
  walker_enabled = 0;
  adc_monitor_enabled = 0;
  autofocus_enabled = 0;
  cal_state = CAL_IDLE;
  cal_capturing = 0;
  mfocus_enabled = 0;
  mfocus_in_enabled = 0;
  if (!beam_enabled) memset(beam_line, 0, sizeof(beam_line));
  beam_enabled = 1;
  if (req->type == 1) {
    /* El tono interno se PRESERVA a propósito: steerear el foco del tono
     * (modos 12/13 + POST /beam type=tx) es el protocolo de medición
     * mic-fijo/foco-móvil (docs/protocolo_medicion.md §A). */
    tx_steer_enabled = 1;
    Beam_UpdateDelays(0);                 // RX source: broadside
    Beam_UpdateTxFocus(req->fx_mm, req->fy_mm, req->fz_mm);
    UART_Print("MODE: TX focus custom (via ESP32)\r\n");
  } else {
    /* RX custom escucha los mics: el tono interno pisaría beam_line, y la
     * ganancia unitaria del modo de tono dejaría la salida muy abajo. */
    tx_steer_enabled = 0;
    tone_enabled = 0;
    beam_out_q8 = in_gain_q8;
    Beam_UpdateDelays(req->az_deg);
    UART_Print("MODE: RX beam custom (via ESP32)\r\n");
  }
}

/**
 * @brief Generate the AUTOFOCUS chirp reference: 500Hz→6kHz linear sweep,
 *        10ms, -6dBFS peak, 32-sample linear fades (no clicks). Its sharp,
 *        non-periodic autocorrelation gives an unambiguous TOF peak — a pure
 *        tone would alias every period.
 */
static void Cal_Init(void) {
  const float f0 = 500.0f, f1 = 6000.0f;
  const float T = (float)CAL_CHIRP_LEN / (float)AUDIO_SAMPLE_RATE;
  for (uint32_t n = 0; n < CAL_CHIRP_LEN; n++) {
    float t = (float)n / (float)AUDIO_SAMPLE_RATE;
    float ph = 6.2831853f * (f0 * t + (f1 - f0) * t * t / (2.0f * T));
    float a = 1.0f;
    if (n < 32) a = (float)n / 32.0f;
    else if (n >= CAL_CHIRP_LEN - 32) a = (float)(CAL_CHIRP_LEN - n) / 32.0f;
    cal_chirp[n] = (int32_t)(a * 4194304.0f * sinf(ph));
  }
}

/**
 * @brief Start an AUTOFOCUS scan from speaker 0 (called on entering the mode).
 */
static void Cal_StartScan(void) {
  for (uint32_t i = 0; i < BEAM_NUM_CHANNELS; i++) {
    cal_tof_q8[i] = 0;
    cal_meas_sign[i] = 1;
    cal_quality[i] = 0;
  }
  cal_ch = 0;
  cal_capturing = 0;
  cal_cap_count = 0;
  cal_chirp_pos = 0;
  cal_progress = 0;
  cal_ok_count = 0;
  cal_settle = CAL_SETTLE_HALVES;
  cal_state = CAL_SETTLE;
  UART_Print("AUTOFOCUS: mic at the focal point -> ADC input 1 (c0). Scanning...\r\n");
  /* Las dos formas de tirar 8 segundos de scan a la basura, dichas ANTES y no
   * después. Sin amplificadores no sale sonido y la correlación mide ruido; sin
   * señal en c0 no hay nada que correlacionar. En los dos casos el scan
   * terminaba igual, anunciaba "done" y dejaba 32 retardos sin sentido. */
  if (!amps_enabled) {
    UART_Print("  !! AMPS APAGADOS — no va a salir sonido. Encendelos y volve a entrar al modo.\r\n");
  }
  if (!(mic_live_mask & (1u << CAL_MIC_CH))) {
    UART_Print("  !! SIN SENAL en la entrada 1 (c0) — revisa el microfono y su preamp.\r\n");
  }
}

/** Correlación cruzada de una captura contra la referencia del chirp, a un lag.
 *  Compartida por el AUTOFOCUS y el LOOPBACK: es la misma cuenta sobre buffers
 *  distintos, y duplicarla sería duplicar también cualquier corrección futura. */
static int64_t Corr_At(const int32_t *cap_base, uint32_t lag) {
  int64_t acc = 0;
  const int32_t *cap = &cap_base[lag];
  for (uint32_t n = 0; n < CAL_CHIRP_LEN; n++) {
    acc += (int64_t)cap[n] * (int64_t)cal_chirp[n];
  }
  return acc;
}
static int64_t Cal_CorrAt(uint32_t lag) { return Corr_At(cal_capture, lag); }
/** Igual que Corr_At pero sobre las capturas de 16 bits del LOOPBACK. */
static int64_t Corr_At16(const int16_t *cap_base, uint32_t lag) {
  int64_t acc = 0;
  const int16_t *cap = &cap_base[lag];
  for (uint32_t n = 0; n < CAL_CHIRP_LEN; n++) {
    acc += (int64_t)cap[n] * (int64_t)cal_chirp[n];
  }
  return acc;
}

/**
 * @brief AUTOFOCUS driver, called every main-loop pass while the mode is on.
 *
 * Runs the correlation in CAL_CORR_CHUNK-lag slices (~0.5ms each) so the
 * audio fills never miss their 10ms deadline. On sweep completion: parabolic
 * interpolation on |corr| around the peak → sub-sample TOF (Q8), polarity
 * from the peak's sign, quality = peak/avg magnitude (×10). After the last
 * speaker: delays = max(tof) − tof_k → tx_delay_q8 + speaker_sign, and the
 * mode switches to playing a steered tone focused at the mic (CAL_DONE).
 */
static void Cal_Process(void) {
  if (cal_state == CAL_CAPT && !cal_capturing && cal_cap_count >= CAL_CAPTURE_LEN) {
    cal_corr_lag = 0;
    cal_best_mag = 0;
    cal_best_lag = 0;
    cal_best_sign = 1;
    cal_mag_sum = 0;
    cal_state = CAL_CORR;
    return;
  }
  if (cal_state != CAL_CORR) return;

  uint32_t end = cal_corr_lag + CAL_CORR_CHUNK;
  if (end > CAL_MAX_LAG) end = CAL_MAX_LAG;
  for (; cal_corr_lag < end; cal_corr_lag++) {
    int64_t c = Cal_CorrAt(cal_corr_lag);
    int64_t m = (c < 0) ? -c : c;
    cal_mag_sum += m;
    if (m > cal_best_mag) {
      cal_best_mag = m;
      cal_best_lag = (int32_t)cal_corr_lag;
      cal_best_sign = (c < 0) ? -1 : 1;
    }
  }
  if (cal_corr_lag < CAL_MAX_LAG) return;   // continue next pass

  // Sweep done — sub-sample refinement via parabola on |corr| at peak±1
  float frac = 0.0f;
  if (cal_best_lag > 0 && cal_best_lag < (int32_t)CAL_MAX_LAG - 1) {
    int64_t cm = Cal_CorrAt((uint32_t)cal_best_lag - 1);
    int64_t cp = Cal_CorrAt((uint32_t)cal_best_lag + 1);
    float mm1 = (float)((cm < 0) ? -cm : cm);
    float mp1 = (float)((cp < 0) ? -cp : cp);
    float m0  = (float)cal_best_mag;
    float den = mm1 - 2.0f * m0 + mp1;
    if (den < -1.0f) {                       // proper maximum
      frac = 0.5f * (mm1 - mp1) / den;
      if (frac > 0.5f) frac = 0.5f;
      else if (frac < -0.5f) frac = -0.5f;
    }
  }
  int64_t avg = cal_mag_sum / (int64_t)CAL_MAX_LAG;
  uint32_t q10 = (avg > 0) ? (uint32_t)(cal_best_mag * 10 / avg) : 0;
  if (q10 > 9999) q10 = 9999;
  /* Gate absoluto ANTES del cociente: si en 100 ms el micrófono no pasó del
   * piso de ruido, no hubo chirp que medir y el peak/avg da igual — hay un
   * máximo en cualquier ventana de ruido, siempre. Anular la calidad acá es lo
   * que hace que "sin micrófono" dé 0/32 y no la lotería de 4-7/32. */
  uint8_t no_signal = (cal_cap_peak < MIC_LIVE_THRESH);
  if (no_signal) q10 = 0;

  cal_tof_q8[cal_ch] = (cal_best_lag << 8) + (int32_t)(frac * 256.0f);
  cal_meas_sign[cal_ch] = cal_best_sign;
  cal_quality[cal_ch] = (uint16_t)q10;

  int32_t tq = cal_tof_q8[cal_ch];
  sprintf(uart_buf, "  CAL ch%u: tof=%ld.%02ld smp sign=%c q=%lu cap=%ld%s\r\n",
          (unsigned)cal_ch, (long)(tq >> 8), (long)((tq & 0xFF) * 100 / 256),
          (cal_best_sign < 0) ? '-' : '+', (unsigned long)q10,
          (long)cal_cap_peak,
          no_signal ? "  (SIN SENAL en el mic)"
                    : ((q10 < CAL_Q_MIN) ? "  (BAJA — ruido de sala?)" : ""));
  UART_Print(uart_buf);

  if (q10 >= CAL_Q_MIN) cal_ok_count++;
  cal_ch++;
  cal_progress = cal_ch;      /* lo lee /status: la página muestra "12/32" */
  if (cal_ch < CAL_NUM_CH) {                 // next speaker
    cal_settle = CAL_SETTLE_HALVES;
    cal_state = CAL_SETTLE;
    return;
  }

  // All speakers measured.
  /* Un scan malo NO se aplica. Antes se aplicaba siempre: con el micrófono
   * desconectado o los amplificadores apagados, la correlación elegía el máximo
   * del RUIDO, el equipo se quedaba con 32 retardos arbitrarios y después
   * "reproducía un tono enfocado en el micrófono" que no estaba enfocado en
   * ningún lado. Y encima pisaba un foco anterior que sí servía. El fallo era
   * indistinguible del éxito salvo leyendo 32 líneas de serie.
   *
   * El criterio es la mitad: con menos de la mitad de los parlantes medidos con
   * confianza, los retardos relativos ya no describen un frente de onda. Los
   * anteriores quedan intactos y el mensaje dice qué revisar. */
  if (cal_ok_count < CAL_NUM_CH / 2) {
    sprintf(uart_buf,
            "AUTOFOCUS DESCARTADO: solo %u/%u parlantes con senal utilizable "
            "(q>=%u).\r\n", cal_ok_count, (unsigned)CAL_NUM_CH, CAL_Q_MIN);
    UART_Print(uart_buf);
    UART_Print("  Revisa: amps encendidos / microfono en la entrada 1 (c0) / "
               "volumen maestro. Los focos anteriores NO se tocaron.\r\n");
    cal_state = CAL_DONE;
    return;
  }

  // Focal delays = max(tof) − tof_k, signs applied
  int32_t tmax = 0;
  for (uint32_t k = 0; k < CAL_NUM_CH; k++) {
    if (cal_tof_q8[k] > tmax) tmax = cal_tof_q8[k];
  }
  for (uint32_t k = 0; k < CAL_NUM_CH; k++) {
    int32_t dq = tmax - cal_tof_q8[k];
    if (dq > (TX_MAX_DELAY_SAMPLES << 8)) dq = TX_MAX_DELAY_SAMPLES << 8;
    if (dq < 0) dq = 0;
    tx_delay_q8[k] = (uint16_t)dq;
    speaker_sign[k] = cal_meas_sign[k];
  }
  sprintf(uart_buf, "AUTOFOCUS OK: %u/%u parlantes con senal utilizable "
          "(q>=%u).\r\n", cal_ok_count, (unsigned)CAL_NUM_CH, CAL_Q_MIN);
  UART_Print(uart_buf);

  /* Persistir. Sólo llega acá un scan que pasó el umbral de calidad, así que la
   * EEPROM nunca guarda una calibración de ruido — que es justo lo que haría
   * inservible guardarla. Diferido por la bandera: el guardado bloquea decenas
   * de ms y desde el main loop no se come deadlines de fill. */
  for (uint32_t k = 0; k < BEAM_NUM_CHANNELS; k++) {
    beam_config.cal_sign[k]     = speaker_sign[k];
    beam_config.cal_delay_q8[k] = tx_delay_q8[k];
  }
  beam_config.cal_valid = 1;
  Arm_Config_Save();
  UART_Print("AUTOFOCUS: calibracion guardada — sobrevive al reinicio.\r\n"
             "  Las POLARIDADES se aplican a todos los modos.\r\n"
             "  El FOCO medido se recupera con el modo 18 (es el punto donde "
             "estaba el microfono, no generaliza a otros).\r\n");
  UART_Print("AUTOFOCUS done — measured delays (samples.frac / sign):\r\n");
  for (uint32_t k = 0; k < CAL_NUM_CH; k++) {
    sprintf(uart_buf, "  ch%lu: %u.%02u %c q=%u\r\n", (unsigned long)k,
            tx_delay_q8[k] >> 8, (tx_delay_q8[k] & 0xFF) * 100 / 256,
            (speaker_sign[k] < 0) ? '-' : '+', cal_quality[k]);
    UART_Print(uart_buf);
  }
  UART_Print("Playing steered tone focused at the mic — ADCpk c0 = level at focus.\r\n");
  cal_state = CAL_DONE;
}

/**
 * @brief Driver del LOOPBACK, llamado en cada pasada del main loop.
 *
 * Misma estructura que Cal_Process y por el mismo motivo: la correlación
 * completa son ~2M multiplicaciones por sonda y bloquearía varios deadlines de
 * fill. Se hace de a LB_CORR_CHUNK lags por pasada, y de a una sonda por vez —
 * lo que tiene que ser simultáneo es la CAPTURA, no la cuenta posterior.
 */
static void LB_Process(void) {
  if (lb_state == LB_WAIT) {
    if ((int32_t)(HAL_GetTick() - lb_next_ms) >= 0) {
      lb_chirp_pos = 0;
      lb_bank_pos[0] = lb_bank_pos[1] = lb_bank_pos[2] = lb_bank_pos[3] = 0;
      lb_state = LB_ARM;
    }
    return;
  }
  if (lb_state == LB_CAPT && !lb_capturing && lb_cap_count >= LB_CAPTURE_LEN) {
    lb_probe = 0;
    lb_corr_lag = 0; lb_best_mag = 0; lb_mag_sum = 0;
    lb_best_lag = 0; lb_best_sign = 1;
    lb_state = LB_CORR;
    return;
  }
  if (lb_state != LB_CORR) return;

  uint32_t end = lb_corr_lag + LB_CORR_CHUNK;
  if (end > LB_MAX_LAG) end = LB_MAX_LAG;
  const int16_t *cap = lb_capture[lb_probe];
  for (; lb_corr_lag < end; lb_corr_lag++) {
    int64_t c = Corr_At16(cap, lb_corr_lag);
    int64_t m = (c < 0) ? -c : c;
    lb_mag_sum += m;
    if (m > lb_best_mag) {
      lb_best_mag = m;
      lb_best_lag = (int32_t)lb_corr_lag;
      lb_best_sign = (c < 0) ? -1 : 1;
    }
  }
  if (lb_corr_lag < LB_MAX_LAG) return;         // sigue en la próxima pasada

  /* Refinamiento sub-muestra: parábola sobre |corr| en el pico ±1. Es lo que da
   * resolución mucho mejor que una muestra (20,8 µs) — sin esto no se podría
   * hablar de un skew de 4 µs entre bancos. */
  float frac = 0.0f;
  if (lb_best_lag > 0 && lb_best_lag < (int32_t)LB_MAX_LAG - 1) {
    int64_t cm = Corr_At16(cap, (uint32_t)lb_best_lag - 1);
    int64_t cp = Corr_At16(cap, (uint32_t)lb_best_lag + 1);
    float mm1 = (float)((cm < 0) ? -cm : cm);
    float mp1 = (float)((cp < 0) ? -cp : cp);
    float den = mm1 - 2.0f * (float)lb_best_mag + mp1;
    if (den < -1.0f) {
      frac = 0.5f * (mm1 - mp1) / den;
      if (frac > 0.5f) frac = 0.5f; else if (frac < -0.5f) frac = -0.5f;
    }
  }
  int64_t avg = lb_mag_sum / (int64_t)LB_MAX_LAG;
  uint32_t q10 = (avg > 0) ? (uint32_t)(lb_best_mag * 10 / avg) : 0;
  if (q10 > 9999) q10 = 9999;

  /* Gate absoluto, mismo criterio que el AUTOFOCUS: un cociente siempre
   * encuentra un máximo, incluso sobre ruido. Sin piso absoluto, una entrada
   * al aire informaría un retardo inventado. */
  int32_t pk = 0;
  for (uint32_t n = 0; n < LB_CAPTURE_LEN; n++) {
    int32_t v = cap[n]; if (v < 0) v = -v;
    if (v > pk) pk = v;
  }
  if (pk < LB_PEAK_MIN) q10 = 0;

  lb_lag_q8[lb_probe] = (lb_best_lag << 8) + (int32_t)(frac * 256.0f);
  lb_sign[lb_probe]   = lb_best_sign;
  lb_q10[lb_probe]    = (uint16_t)q10;
  lb_peak[lb_probe]   = pk;

  lb_probe++;
  if (lb_probe < LB_NUM_IN) {                   // siguiente entrada
    lb_corr_lag = 0; lb_best_mag = 0; lb_mag_sum = 0;
    lb_best_lag = 0; lb_best_sign = 1;
    return;
  }

  /* ===== Informe ===== */
  UART_Print("\r\nLOOPBACK (chirp por ch0/8/16/24) ---------------------------\r\n");
  /* Fase de los DMA al armar. NDTR cuenta hacia ABAJO desde la cuenta programada,
   * así que la posición de lectura/escritura es (total - NDTR), y dividido por
   * los canales da el índice de muestra dentro del doble buffer (0..479).
   *
   * RX-TX se normaliza al rango [0, 480): es una fase circular, y sin envolver
   * el mismo desfasaje se leería como +470 o como -10 según dónde caiga. */
  {
    int32_t pos_rx = (int32_t)((LB_DMA_WORDS - lb_ndtr_rx) / ADC_CHANNELS);
    int32_t pos_tx = (int32_t)((LB_DMA_WORDS - lb_ndtr_tx) / AUDIO_CHANNELS);
    sprintf(uart_buf,
            "  fase DMA al armar: RX=%ld TX=%ld de %ld muestras  RX-TX=%ld\r\n",
            (long)pos_rx, (long)pos_tx, (long)(LB_DMA_WORDS / AUDIO_CHANNELS),
            (long)DMA_PhaseFrom(lb_ndtr_rx, lb_ndtr_tx));
    UART_Print(uart_buf);
  }
  /* Dónde cayó el ARMADO dentro del buffer del RX, y con qué mitad abrió la
   * captura. Sirve para decidir la carrera anotada en §10 (ver el comentario
   * largo en la declaración de lb_arm_pend).
   *
   * `back` es la distancia MEDIDA entre el instante de armar y el primer sample
   * capturado: desde la posición del DMA al armar hasta el principio de la mitad
   * que abrió, envuelta en el buffer circular. Vale q si la captura abrió con la
   * mitad que el DMA estaba escribiendo, y q+240 si abrió con la anterior.
   *
   * De ahí sale la prueba. Lo que hay que descontar de `t` NO es `back` entero
   * sino su parte de MITADES —`k`, 0 o 1—: el chirp sale y la ventana abre en
   * bordes de mitad, los dos, así que `t` está cuantizado en pasos de 240 y `q`
   * (dónde cayó el armado DENTRO de la mitad) no entra. Medido: `t` no sigue a
   * `q` en absoluto (0,26 muestras de dispersión sobre 366 informes contra 0..26
   * de `q`), y descontar `q` sólo le agrega ruido.
   *
   * Con eso, `t − 240k` es la latencia real del lazo y NO debería moverse aunque
   * la ventana se corra media vuelta. Si en el caso raro `t` baja 240 y `t−240k`
   * se queda quieto, la carrera está confirmada y el pipeline queda exonerado.
   * Si `t−240k` también salta, la explicación de §10 es falsa. */
  int32_t arm_back = 0, arm_k = 0;
  {
    int32_t nsamp = (int32_t)(LB_DMA_WORDS / AUDIO_CHANNELS);   /* 480 */
    int32_t half  = nsamp / 2;                                  /* 240 */
    int32_t pos   = (int32_t)((LB_DMA_WORDS - lb_ndtr_rx) / ADC_CHANNELS);
    int32_t w     = pos / half;                 /* mitad que el DMA ESCRIBÍA */
    int32_t q     = pos - w * half;             /* muestras desde el borde */
    if (lb_cap_half <= 1) {
      arm_back = pos - half * (int32_t)lb_cap_half;
      while (arm_back < 0) arm_back += nsamp;
      while (arm_back >= nsamp) arm_back -= nsamp;
    }
    arm_k = arm_back / half;                /* mitades de atraso: 0 o 1 */
    sprintf(uart_buf,
            "  armado: pos=%ld mitad=%ld q=%ld pend=%u cap=%u dRX=%ld back=%ld k=%ld\r\n",
            (long)pos, (long)w, (long)q, (unsigned)lb_arm_pend,
            (unsigned)lb_cap_half, (long)(lb_cap_rxcb - lb_arm_rxcb),
            (long)arm_back, (long)arm_k);
    UART_Print(uart_buf);
  }
  /* El retardo Q8 comandado, para poder cruzarlo con el medido sin suponer cuál
   * estaba puesto. Sólo se imprime si hay alguno: con 0 el modo es el de siempre
   * y la línea sería ruido visual. */
  {
    uint16_t dq = beam_config.delays[lb_emit_ch[LB_NUM_EMIT - 1]];
    if (dq) {
      sprintf(uart_buf, "  retardo Q8 comandado en ch%u: %u  (%u.%02u muestras)\r\n",
              (unsigned)lb_emit_ch[LB_NUM_EMIT - 1], (unsigned)dq,
              (unsigned)(dq >> 8), (unsigned)((dq & 0xFF) * 100 / 256));
      UART_Print(uart_buf);
    }
  }
  /* La referencia es la primera entrada CON señal, no la entrada 0: con un solo
   * cable puesto en la entrada 4, anclar en la 0 dejaría todo sin referencia. */
  int32_t ref_q8 = 0; uint8_t have_ref = 0, ref_i = 0, n_ok = 0;
  for (uint8_t p = 0; p < LB_NUM_IN; p++) {
    if (lb_q10[p] >= CAL_Q_MIN) {
      n_ok++;
      if (!have_ref) { ref_q8 = lb_lag_q8[p]; have_ref = 1; ref_i = p; }
    }
  }
  for (uint8_t p = 0; p < LB_NUM_IN; p++) {
    if (lb_q10[p] < CAL_Q_MIN) continue;        /* las mudas se resumen abajo */
    int32_t lag = lb_lag_q8[p];
    int32_t abs_us = (int32_t)(((int64_t)lag * 1000000) / (256 * AUDIO_SAMPLE_RATE));
    int32_t d_q8   = lag - ref_q8;
    int32_t d_us10 = (int32_t)(((int64_t)d_q8 * 10000000) / (256 * AUDIO_SAMPLE_RATE));
    sprintf(uart_buf,
            "  entrada%u (c%u)  t=%ld.%02ld smp (%ld us)  skew=%+ld.%01ld us  "
            "pol=%c  q=%u  pico=%ld\r\n",
            (unsigned)(p + 1), (unsigned)p,
            (long)(lag >> 8), (long)((lag & 0xFF) * 100 / 256), (long)abs_us,
            (long)(d_us10 / 10), (long)(d_us10 < 0 ? -d_us10 % 10 : d_us10 % 10),
            (lb_sign[p] < 0) ? '-' : '+', (unsigned)lb_q10[p], (long)lb_peak[p]);
    UART_Print(uart_buf);
  }
  if (have_ref) {
    sprintf(uart_buf, "  %u/%u entradas con senal; skew relativo a la entrada%u "
            "(+ = llega DESPUES)\r\n",
            (unsigned)n_ok, (unsigned)LB_NUM_IN, (unsigned)(ref_i + 1));
    UART_Print(uart_buf);
    /* La latencia descontando el origen de la ventana: es la que NO debería
     * moverse ni cuando el artefacto de §10 aparece. */
    int32_t norm_q8 = ref_q8 - (arm_k * 240 << 8);
    sprintf(uart_buf, "  t-240k = %ld.%02ld smp (latencia real, entrada%u)\r\n",
            (long)(norm_q8 >> 8), (long)((norm_q8 & 0xFF) * 100 / 256),
            (unsigned)(ref_i + 1));
    UART_Print(uart_buf);
  } else {
    /* Partido en dos: el mensaje entero no entra en uart_buf y el compilador
     * lo marcó como format-overflow. */
    UART_Print("  ninguna entrada vio el chirp. Revisa el cable; si medis por\r\n"
               "  parlante, que los amps esten encendidos y el volumen arriba.\r\n"
               "  Picos por entrada: ");
    for (uint8_t p = 0; p < LB_NUM_IN; p++) {
      sprintf(uart_buf, "c%u=%ld ", (unsigned)p, (long)lb_peak[p]);
      UART_Print(uart_buf);
    }
    UART_Print("\r\n");
  }

  lb_next_ms = HAL_GetTick() + LB_PERIOD_MS;
  lb_state = LB_WAIT;
}

/* Pausa entre barridos: durante ella el emisor calla y el detector no acumula,
 * así el log queda legible y se ve si la curva repite. */
static volatile uint8_t fs_paused = 0;

/** @brief Arranca un punto del barrido: acumuladores en cero y fase al origen.
 *  La fase del detector se reinicia por prolijidad, no por necesidad: la
 *  magnitud sqrt(I²+Q²) no depende de dónde arranque. */
static void FS_StartPoint(void) {
  for (uint32_t c = 0; c < ADC_CHANNELS; c++) { fs_i[c] = 0; fs_q[c] = 0; }
  /* Se ANOTA la fase del emisor, no se pisa: la fase medida vale
   * θ = ωD − φ_tx0, y ese término se cancela entre las dos sub-medidas de un
   * punto sólo si se conoce. Conocerlo cuesta 4 bytes; forzarlo a cero costaba
   * la amplitud. */
  if (fs_idx < FS_NUM_PTS) fs_tx0[fs_idx] = fs_tx_phase;
  fs_rx_phase = 0;
  fs_n = 0;
  fs_point_ready = 0;
}

/** @brief Entrar al modo: barrido desde el primer punto. */
static void FS_Start(void) {
  memset(fs_fi, 0, sizeof(fs_fi));
  memset(fs_fq, 0, sizeof(fs_fq));
  fs_idx = 0;
  fs_tx_phase = 0;
  fs_bank_ph[0] = fs_bank_ph[1] = fs_bank_ph[2] = fs_bank_ph[3] = 0;
  fs_paused = 0;
  FS_StartPoint();
}

/** @brief Magnitud del punto i en la entrada c, en cuentas de 24 bits. */
static inline float FS_Mag(uint8_t i, uint8_t c) {
  return 2.0f * sqrtf(fs_fi[i][c] * fs_fi[i][c] + fs_fq[i][c] * fs_fq[i][c]);
}

/** @brief dB con signo y un decimal en `dst` (>=10 bytes).
 *  nano.specs no soporta %f. Está en una sola función a propósito: hecho a mano
 *  en cada sitio, el error clásico es que −0,3 salga "+0.3" porque −3/10 da 0 y
 *  el signo se pierde. */
static void FS_FmtDb(char *dst, float db) {
  int32_t d10 = (int32_t)(db * 10.0f + (db >= 0.0f ? 0.5f : -0.5f));
  char sg = (d10 < 0) ? '-' : '+';
  if (d10 < 0) d10 = -d10;
  sprintf(dst, "%c%ld.%01ld", sg, (long)(d10 / 10), (long)(d10 % 10));
}

/** @brief Retardo de grupo del punto base k, en muestras.
 *
 * Sale de la diferencia de fase entre las dos sub-medidas: con
 * I = Σx·cos(ωn) y Q = Σx·sin(ωn), para x = A·cos(ωn+φ) queda atan2(Q,I) = −φ,
 * y un retardo puro τ da φ = −ωτ. O sea atan2(Q,I) = ωτ y τ = Δθ/(2πΔf).
 *
 * El desenvolvimiento a (−π,π] es lo que fija el rango: ver el comentario de
 * FS_DF. Devuelve 0 si el punto no tiene señal, para no informar ruido.
 */
static float FS_GroupDelay(uint8_t k, uint8_t c) {
  uint8_t i0 = (uint8_t)(k * 2), i1 = (uint8_t)(k * 2 + 1);
  if (FS_Mag(i0, c) < FS_LIVE_MIN || FS_Mag(i1, c) < FS_LIVE_MIN) return 0.0f;
  /* +φ_tx0 en cada término: es el origen del emisor, que no es cero porque el
   * acumulador corre libre entre puntos. 48·65536 es una vuelta entera. */
  const float TURN = 48.0f * 65536.0f;
  float d = (atan2f(fs_fq[i1][c], fs_fi[i1][c]) + 6.28318531f * (float)fs_tx0[i1] / TURN)
          - (atan2f(fs_fq[i0][c], fs_fi[i0][c]) + 6.28318531f * (float)fs_tx0[i0] / TURN);
  while (d >  3.14159265f) d -= 6.28318531f;
  while (d < -3.14159265f) d += 6.28318531f;
  float df = FS_FreqReal(i1) - FS_FreqReal(i0);
  if (df <= 0.0f) return 0.0f;
  return (d / (6.28318531f * df)) * (float)AUDIO_SAMPLE_RATE;
}

/** @brief Imprime la tabla, una por entrada que haya visto señal, más la
 *         diafonía hacia las otras siete.
 *
 * Igual que el LOOPBACK, el firmware DESCUBRE por dónde entró en vez de exigir
 * que se lo declaren. "Viva" se decide con el nivel en 1 kHz, no en el extremo
 * del barrido, porque ahí abajo la respuesta puede haber caído legítimamente. */
static void FS_Report(void) {
  /* Holgados a propósito: el compilador acota %ld por el peor caso de un
   * long, no por el rango real, y con buffers justos avisa por overflow. */
  char dbs[24], taus[32];
  UART_Print("\r\nBARRIDO DE FRECUENCIA (ch0/8/16/24, tono -6 dBFS) ---------\r\n");
  uint8_t live = 0xFF, n_ok = 0;
  /* La referencia de la diafonía es la entrada MÁS FUERTE, no la primera que
   * pasa el umbral.
   *
   * Antes era la primera y con FS_LIVE_MIN en 100 nunca falló: ninguna entrada
   * muda llegaba a 100 cuentas. Al bajar el umbral a 10 (§15.1, para que el
   * barrido de volumen no se censurara solo en −24 dB) se abrió este agujero:
   * con excitación alta la diafonía sola alcanza para que una entrada sin cable
   * cruce las 10 cuentas, y si su índice es menor que el del canal excitado se
   * queda con la referencia. Medido el 2026-08-26 con el tono a −12 dB: c2 marcó
   * 11 cuentas de ruido, ganó la referencia por ser índice 2 contra el 4 del
   * canal real, y la tabla informó **+149 dB** de diafonía para c4 — el canal
   * excitado, con 4,27 millones de cuentas, "acoplando" hacia el ruido.
   *
   * El máximo no tiene ese modo de falla: la excitación está decenas de dB
   * arriba de cualquier acoplamiento, así que gane quien gane el umbral, el
   * canal con cable siempre es el más grande. El umbral sigue decidiendo qué
   * entradas se IMPRIMEN, que es para lo que se bajó. */
  {
    float best = 0.0f;
    for (uint8_t c = 0; c < ADC_CHANNELS; c++) {
      float ref = FS_Mag(FS_REF_BASE * 2, c);
      if (ref >= FS_LIVE_MIN && ref > best) { best = ref; live = c; }
    }
  }
  for (uint8_t c = 0; c < ADC_CHANNELS; c++) {
    float ref = FS_Mag(FS_REF_BASE * 2, c);
    if (ref < FS_LIVE_MIN) continue;
    n_ok++;
    sprintf(uart_buf, "  entrada%u (c%u): 0 dB = %lu cuentas @ %u Hz\r\n",
            (unsigned)(c + 1), (unsigned)c, (unsigned long)(ref + 0.5f),
            (unsigned)fs_base[FS_REF_BASE]);
    UART_Print(uart_buf);
    for (uint8_t k = 0; k < FS_NUM_BASE; k++) {
      float m = FS_Mag((uint8_t)(k * 2), c);
      float r = m / ref;
      if (r < 1e-6f) r = 1e-6f;                  /* piso de -120 dB, no -inf */
      FS_FmtDb(dbs, 20.0f * log10f(r));
      float tau = FS_GroupDelay(k, c);
      int32_t t100 = (int32_t)(tau * 100.0f + (tau >= 0.0f ? 0.5f : -0.5f));
      char sg = (t100 < 0) ? '-' : '+';
      if (t100 < 0) t100 = -t100;
      sprintf(taus, "%c%ld.%02ld", sg, (long)(t100 / 100), (long)(t100 % 100));
      sprintf(uart_buf, "  %7lu Hz  %8lu  %s dB   tau %s smp\r\n",
              (unsigned long)fs_base[k], (unsigned long)(m + 0.5f), dbs, taus);
      UART_Print(uart_buf);
    }
  }
  if (!n_ok) {
    UART_Print("  ninguna entrada vio el tono. Revisa el cable de J13 (pin 1 =\r\n"
               "  senal, masa aparte); si medis por parlante hacen falta 24 V.\r\n"
               "  Nivel en la referencia por entrada: ");
    for (uint8_t c = 0; c < ADC_CHANNELS; c++) {
      sprintf(uart_buf, "c%u=%lu ", (unsigned)c,
              (unsigned long)(FS_Mag(FS_REF_BASE * 2, c) + 0.5f));
      UART_Print(uart_buf);
    }
    UART_Print("\r\n");
    return;
  }

  /* Diafonía hacia las entradas que NO tienen cable. Se compara contra el canal
   * excitado A LA MISMA FRECUENCIA, no contra su valor en 1 kHz: si la respuesta
   * del excitado cayera en un extremo, medir contra 1 kHz inventaría diafonía
   * donde sólo hay menos señal de referencia.
   *
   * ⚠️ Un número cerca del piso de ruido NO es diafonía medida: con ~2600
   * cuentas de excitación y ~6 de ruido después de la detección coherente, el
   * piso está en torno a −53 dB. Si todas las entradas mudas dan lo mismo y no
   * depende de la frecuencia, eso es el piso, no acoplamiento. */
  UART_Print("  diafonia vs el canal excitado (peor caso | en 1 kHz):\r\n");
  for (uint8_t c = 0; c < ADC_CHANNELS; c++) {
    if (c == live) continue;
    float worst = -200.0f; uint16_t at = 0; float cnt = 0.0f;
    for (uint8_t k = 0; k < FS_NUM_BASE; k++) {
      float m = FS_Mag((uint8_t)(k * 2), c);
      float d = FS_Mag((uint8_t)(k * 2), live);
      if (d <= 0.0f) continue;
      float r = m / d;
      if (r < 1e-7f) r = 1e-7f;
      float db = 20.0f * log10f(r);
      if (db > worst) { worst = db; at = fs_base[k]; cnt = m; }
    }
    FS_FmtDb(dbs, worst);
    float r1k = FS_Mag(FS_REF_BASE * 2, c) / FS_Mag(FS_REF_BASE * 2, live);
    if (r1k < 1e-7f) r1k = 1e-7f;
    FS_FmtDb(taus, 20.0f * log10f(r1k));
    sprintf(uart_buf, "    c%u  %s dB @ %5u Hz (%lu cuentas) | %s dB\r\n",
            (unsigned)c, dbs, (unsigned)at, (unsigned long)(cnt + 0.5f), taus);
    UART_Print(uart_buf);
  }
}

/** @brief Máquina del barrido, en el main loop: cierra puntos y avanza.
 *
 * El cierre se hace acá y no en Process_ADC_Half a propósito: la raíz cuadrada
 * y el logaritmo son de coma flotante y no tienen por qué correr adentro del
 * presupuesto del fill. El detector sólo levanta una bandera.
 */
static void FS_Process(void) {
  if (fs_paused) {
    if ((int32_t)(HAL_GetTick() - fs_next_ms) < 0) return;
    fs_paused = 0;
    FS_Start();
    return;
  }
  if (!fs_point_ready) return;

  /* Magnitud del punto: A = 2·sqrt(I²+Q²)/(N·Apico). Se normaliza ANTES de
   * elevar al cuadrado — I y Q llegan a ~2^58 y al cuadrado se saldrían de
   * rango en float. Queda en cuentas de 24 bits, la misma escala que ADCpk. */
  const float den = (float)FS_WINDOW * 4194304.0f;
  for (uint8_t c = 0; c < ADC_CHANNELS; c++) {
    fs_fi[fs_idx][c] = (float)fs_i[c] / den;
    fs_fq[fs_idx][c] = (float)fs_q[c] / den;
  }
  fs_idx++;
  if (fs_idx < FS_NUM_PTS) { FS_StartPoint(); return; }

  /* Pausar ANTES de informar, no después. UART_Print bloquea milisegundos y
   * cede a Service_Fills en el medio, así que durante el informe el emisor y el
   * detector siguen corriendo: hay que dejarlos en un estado válido primero. */
  fs_paused = 1;
  fs_idx = 0;
  fs_next_ms = HAL_GetTick() + FS_PAUSE_MS;
  FS_Report();
}

/** @brief Deja beam_config en valores de fábrica (sin tocar la EEPROM).
 *
 * Una sola definición de "default", usada tanto en el boot como en el reset de
 * fábrica: si estuvieran duplicadas, el reset y el primer arranque terminarían
 * dando estados distintos con el tiempo.
 */
static void Config_LoadDefaults(void) {
  beam_config.master_gain = 255;
  beam_config.mute_global = 0;
  memset(beam_config.delays, 0, sizeof(beam_config.delays));
  memset(beam_config.gains, 255, sizeof(beam_config.gains));  // 0xFF = unity gain
  beam_config.mutes = 0;
  /* −40 dB, no −24. El −24 venía de igualar el default del init del PCM1690 y
   * NO sale de la cadena de ganancia: el PAM8302A está configurado con
   * R_I = 1 kΩ, o sea A_V = 2·(142k/1k) = 284 = +49 dB. Para que no recorte, su
   * entrada tiene que quedar en ~11 mV_rms, y el PCM1690 entrega ~1,4 V_rms a
   * fondo de escala → hacen falta ≈ −42 dB de atenuación digital.
   *
   * Con −24 dB y los modos de tono (−6 dBFS) el amplificador quedaba 12 dB
   * DENTRO del recorte: el equipo arrancaba de fábrica en un punto donde
   * distorsiona. Es el fondo del lío de estructura de ganancia de agosto.
   *
   * Corroboración independiente: Fernando venía operando a −46 dB por oído.
   * Una derivación de esquemático y una elección auditiva que caen a 4 dB una
   * de otra. Detalle en docs/loopback_debug.md §3. */
  beam_config.master_vol_db = -40;
  beam_config.last_mode = 0;         // monitor
  beam_config.in_gain_db = IN_GAIN_DB_LINE;  // las entradas de la DS11 son de línea
  /* Sin calibración: polaridades en +1, que es lo que corresponde en DS11 (la
   * doble inversión de los impares es deliberada y se cancela en el JST). El
   * reset de fábrica DEBE borrar esto: una calibración vieja sobreviviendo a un
   * "volver a cero" es justo el estado raro del que no se puede salir. */
  for (uint32_t k = 0; k < BEAM_NUM_CHANNELS; k++) beam_config.cal_sign[k] = 1;
  memset(beam_config.cal_delay_q8, 0, sizeof(beam_config.cal_delay_q8));
  beam_config.cal_valid = 0;
}

/** @brief Reset de fábrica: defaults + focos + volumen + modo, y persiste.
 *
 * Existe porque el instrumento tiene muchos parámetros que se pisan en vivo
 * (foco por fuente, azimut, mutes, ganancias, volumen) y algunos se guardan
 * solos: sin una vuelta a cero se puede quedar en un estado raro sin forma
 * evidente de salir. Corre desde el main loop — hace I2C y HAL_Delay.
 */
static void Config_FactoryReset(void) {
  Config_LoadDefaults();
  MFocusIn_ApplyDefaults();          // focos de las 8 entradas (modo 15)
  MFocus_ApplyDefaults();            // focos de los 4 tonos (modo 14)
  Set_In_Gain(beam_config.in_gain_db);
  Apply_Master_Volume(beam_config.master_vol_db);
  UI_ApplyState(0);                  // monitor
  if (EEPROM_IsAvailable()) eeprom_save_pending = 1;   // persistir ya, sin debounce
  UART_Print("RESET DE FABRICA: volumen -24 dB, ganancia 0 dB, modo 0, focos por defecto\r\n");
}

/** @brief Habilita o apaga el banco de 32 PAM8302A (pin SD, PD3).
 *
 * Llamar SOLO desde el main loop: hace HAL_Delay. Con los amps apagados corre
 * TODO el pipeline (SAI, DMA, beamforming) en silencio, así se aísla si un
 * reinicio viene de la demanda de los 32 amps sobre el buck (que quedó con
 * menos bulk tras remover C290 — ver docs/ds11_errata.md).
 */
static void Set_Amps(uint8_t on) {
  if (on) {
    /* "on" SIEMPRE pasa por off primero — no es redundante.
     *
     * El PAM8302A tiene un UVLO ENCLAVADO: del datasheet, "when the supply
     * voltage drops to 2.1 V or below, the PAM8302A goes into a state of
     * shutdown, and the device comes out of its shutdown state by reset the
     * power supply or SD pin". O sea que tras cualquier hundimiento del
     * +5VA, los amps que hayan cruzado 2,1 V quedan MUDOS aunque SD siga
     * alto — y como SD ya está en 1, un "amps on" no los recuperaría.
     *
     * El síntoma sería "se me murieron algunos canales" sin causa visible,
     * que es exactamente el tipo de cosa que hace perder una tarde. Con el
     * pulso, cualquier "amps on" sirve además como comando de recuperación.
     *
     * 2 ms alcanzan de sobra: el SD del PAM8302A responde en microsegundos. */
    HAL_GPIO_WritePin(PAM_SD_PORT, PAM_SD_PIN, GPIO_PIN_RESET);
    HAL_Delay(2);
  }
  HAL_GPIO_WritePin(PAM_SD_PORT, PAM_SD_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
  amps_enabled = on;
  snprintf(uart_buf, sizeof(uart_buf), "AMPS: %s\r\n",
           on ? "ON (32 PAM8302A habilitados)" : "OFF (silencio)");
  UART_Print(uart_buf);
}

/** @brief Fija la ganancia de entrada en dB (0..36), con clamp.
 *
 * Escala TODA la cadena alimentada por el ADC: monitor, MULTIFOCUS de entradas
 * y beam. La única excepción es el beam en modos de tono, cuya fuente interna
 * ya está a un nivel conocido (-6 dBFS) — ahí queda en unidad. No persiste por
 * sí sola: el que la persiste es el handler del comando. */
static void Set_In_Gain(uint8_t db) {
  if (db > IN_GAIN_DB_MAX) db = IN_GAIN_DB_MAX;
  in_gain_db = db;
  in_gain_q8 = gain_q8_tbl[db];
  if (!tone_enabled) beam_out_q8 = in_gain_q8;
}

/**
 * @brief Switch operating state (K2 button): sets exactly one mode flag.
 */
static void UI_ApplyState(uint8_t idx) {
  const ui_state_t *st = &ui_states[idx];
  ui_state_idx = idx;
  custom_beam_active = 0;      // elegir un modo vuelve a poner al día lo que se ve
  walker_enabled = st->walker;
  tx_steer_enabled = st->tx;
  if (st->beam && !beam_enabled) memset(beam_line, 0, sizeof(beam_line));
  beam_enabled = st->beam;
  adc_monitor_enabled = st->monitor;
  autofocus_enabled = st->af;
  if (st->tone) {
    tone_base_hz = st->tone_hz;
    tone_freq_hz = st->tone_hz;
    tone_phase_q16 = 0;
    tone_dither_pos = 0;
  }
  tone_enabled = st->tone;
  mfocus_enabled = st->mf;
  if (st->mf) {                    // preset: siempre re-arranca en los defaults
    MFocus_ApplyDefaults();
    mf_frame = 0;
  }
  mfocus_in_enabled = st->mfin;
  if (st->mfin) MFocusIn_ApplyDefaults();
  sweep_axis = st->sweep;
  fs_enabled = st->fsweep;
  if (st->fsweep) FS_Start();        // entrar al modo arranca el barrido de cero
  lb_enabled = st->loopb;
  if (st->loopb) {                   // entrar al modo arranca una medición ya
    lb_chirp_pos = 0;
    lb_bank_pos[0] = lb_bank_pos[1] = lb_bank_pos[2] = lb_bank_pos[3] = 0;
    lb_capturing = 0;
    lb_state = LB_ARM;
  } else {
    lb_state = LB_IDLE;
    lb_capturing = 0;
  }
  /* Tone source is -6dBFS: el boost que levanta el beam débil del ADC lo
   * saturaría contra los PAM (plan §3.1). Con fuente de ADC, el beam usa la
   * MISMA ganancia de entrada que el resto — un ajuste, una cadena. */
  beam_out_q8 = st->tone ? 256 : in_gain_q8;
  if (!st->af) {                     // leaving AUTOFOCUS aborts any scan
    cal_state = CAL_IDLE;
    cal_capturing = 0;
  }
  if (st->beam) Beam_UpdateDelays(st->az_deg);
  if (st->tx) Beam_UpdateTxFocus(st->fx_mm, st->fy_mm, st->fz_mm);
  /* DESPUÉS del foco geométrico, a propósito: lo pisa. Así, si no hay
   * calibración guardada, el modo igual suena con el foco nominal de 1 m en vez
   * de quedar mudo — y lo dice. */
  if (st->calfoc) {
    if (beam_config.cal_valid) {
      for (uint32_t k = 0; k < BEAM_NUM_CHANNELS; k++) {
        tx_delay_q8[k]  = beam_config.cal_delay_q8[k];
        speaker_sign[k] = beam_config.cal_sign[k];
      }
      UART_Print("  foco MEDIDO restaurado desde la EEPROM (AUTOFOCUS)\r\n");
    } else {
      UART_Print("  !! no hay calibracion guardada — corre el modo 11 con el "
                 "microfono. Suena con el foco geometrico de 1 m.\r\n");
    }
  }
  if (st->walker) walker_frame = 0;
  /* Persist the mode, debounced: a real change arms the deadline; the boot
   * restore re-applies the same idx so it doesn't (idx == last_mode). */
  if (idx != beam_config.last_mode) {
    beam_config.last_mode = idx;
    Arm_Config_Save();
  }
  /* La ganancia de entrada NO se toca al cambiar de modo: es propiedad de la
   * fuente conectada, no del modo, y vive en la EEPROM. Se imprime para que
   * quede en el log de qué nivel se partía. */
  sprintf(uart_buf, "MODE [%u/%u]: %s (ganancia entrada +%u dB)\r\n",
          (unsigned)(idx + 1), (unsigned)UI_NUM_STATES, st->name,
          (unsigned)in_gain_db);
  UART_Print(uart_buf);
  if (st->af) Cal_StartScan();
}

/**
 * @brief Process ADC half-buffer: deinterleave 8ch TDM into delay lines
 * @param rx_buf  Pointer to received interleaved ch0..7 samples (AK5538VN TDM256)
 * @param samples Number of frames (half of ADC_BUFFER_SAMPLES)
 *
 * Feeds all 8 circular delay lines from ADC input.
 * Must be called BEFORE Fill_Audio_Beamform in the same processing cycle.
 */
/* One tone sample, peak 2^22 = -6dBFS: linear interpolation on the 48-entry
 * table. (s1−s0) spans up to ~2^19 × frac 2^16 → 64-bit intermediate. The
 * wrap needs a single subtract for any f < 48kHz (inc < 48<<16). */
static inline int32_t Tone_NextSample(void) {
  uint32_t inc = ((uint32_t)tone_freq_hz << 16) / 1000u;
  uint32_t idx = tone_phase_q16 >> 16;              // < 48, wrapped below
  int32_t  fr  = (int32_t)(tone_phase_q16 & 0xFFFF);
  int32_t  s0  = sine_table_1khz[idx];
  int32_t  s1  = sine_table_1khz[(idx + 1u) % 48u];
  tone_phase_q16 += inc;
  if (tone_phase_q16 >= (48u << 16)) tone_phase_q16 -= (48u << 16);
  return s0 + (int32_t)(((int64_t)(s1 - s0) * fr) >> 16);
}

static void Process_ADC_Half(int32_t *rx_buf, uint32_t samples) {
  uint32_t wp = delay_write_pos;

  /* Latch del pin OVF del AK5538. Antes se leía en el heartbeat, o sea UNA VEZ
   * POR SEGUNDO — y el `ADCpk` de ahí se calculaba sobre una sola ventana de
   * 10 ms, o sea el 1% del tiempo. Con ese muestreo se concluyó que "al ADC le
   * sobran 45 dB" mientras Fernando reportaba saturación audible: los picos
   * musicales pasaban entre medición y medición.
   *
   * Acá corre 100 veces por segundo y el peak-hold ve TODAS las muestras, así
   * que la pregunta "¿satura el ADC o satura la salida?" pasa a tener respuesta
   * en vez de discusión. Ambos se limpian al imprimirlos. */
  if (HAL_GPIO_ReadPin(AK_OVF_PORT, AK_OVF_PIN)) adc_ovf_latch++;

  /* pk[] es el pico de ESTA ventana (arranca en cero) y después se funde en los
   * dos peak-holds. Antes arrancaba desde adc_peak_hold, lo que ataba el
   * acumulador a un único consumidor. Los 16 max() del final son despreciables
   * frente al lazo de 480×8 muestras que ya corre acá. */
  int32_t pk[ADC_CHANNELS] = {0};
  for (uint32_t i = 0; i < samples; i++) {
    for (uint32_t ch = 0; ch < ADC_CHANNELS; ch++) {
      int32_t v = rx_buf[i * ADC_CHANNELS + ch];
      delay_line[ch][(wp + i) & DELAY_LINE_MASK] = v;
      v >>= 8;                                   /* escala 24-bit, como ADCpk */
      if (v < 0) v = -v;
      if (v > pk[ch]) pk[ch] = v;
    }
  }
  for (uint32_t ch = 0; ch < ADC_CHANNELS; ch++) {
    if (pk[ch] > adc_peak_hold[ch]) adc_peak_hold[ch] = pk[ch];   /* → serie */
    if (pk[ch] > adc_peak_net[ch])  adc_peak_net[ch]  = pk[ch];   /* → /status */
  }

  /* Captura remota: las OCHO entradas, crudas, en escala de 24 bits (la misma
   * que adc_pk y que el cálculo de dBFS de la página). Copia directa, sin
   * procesar: lo que se analice después se decide afuera. */
  if (cap_state == CAP_FILL) {
    uint32_t n = cap_count;
    for (uint32_t i = 0; i < samples && n < cap_want; i++, n++) {
      for (uint32_t ch = 0; ch < ADC_CHANNELS; ch++) {
        cap_buf[n][ch] = rx_buf[i * ADC_CHANNELS + ch] >> 8;
      }
    }
    cap_count = (uint16_t)n;
    if (n >= cap_want) cap_state = CAP_READY;   /* el main loop hace el resto */
  }

  /* LOOPBACK: capturar las LB_NUM_PROBES primeras entradas SIMULTÁNEAMENTE.
   * Que sea la misma trama TDM para todas es justamente lo que hace válida la
   * comparación de retardos: el lado del ADC no aporta skew. */
  if (lb_capturing) {
    uint32_t n = lb_cap_count;
    /* Primer trozo: anotar CON QUÉ MITAD abrió la captura. Es el dato que le
     * falta a §10 — junto con la posición del DMA al armar da la distancia real
     * entre el armado y el primer sample capturado, sin suponer nada. */
    if (n == 0) {
      lb_cap_half = (rx_buf == &audio_buffer_3b_rx[0]) ? 0u : 1u;
      lb_cap_rxcb = cb_rx3b_count;
    }
    for (uint32_t i = 0; i < samples && n < LB_CAPTURE_LEN; i++, n++) {
      for (uint32_t p = 0; p < LB_NUM_IN; p++) {
        lb_capture[p][n] = (int16_t)(rx_buf[i * ADC_CHANNELS + p] >> 16);
      }
    }
    lb_cap_count = (uint16_t)n;
    if (n >= LB_CAPTURE_LEN) lb_capturing = 0;
  }

  /* BARRIDO DE FRECUENCIA: detección coherente en las OCHO entradas a la vez.
   * Multiplicar por seno y coseno de la misma frecuencia y acumular es todo el
   * detector; el ruido no correlacionado se promedia a cero y quedan ~40 dB de
   * ganancia de proceso sobre la ventana de 9600 muestras. Cuesta 8×2 MAC por
   * muestra —despreciable frente al fill— y en este modo el beamforming no
   * corre, así que hay presupuesto de sobra. */
  if (fs_enabled && !fs_paused && !fs_point_ready) {
    uint32_t inc = FS_Inc(fs_idx);
    uint32_t ph  = fs_rx_phase;
    uint32_t n   = fs_n;
    for (uint32_t i = 0; i < samples; i++) {
      if (n >= FS_SETTLE) {                    /* los primeros 100 ms se tiran */
        uint32_t pc = ph + (12u << 16);        /* +90°: 12 de 48 entradas */
        if (pc >= FS_PHASE_MAX) pc -= FS_PHASE_MAX;
        int32_t sq = FS_Sine(ph);
        int32_t cq = FS_Sine(pc);
        for (uint32_t c = 0; c < ADC_CHANNELS; c++) {
          int32_t x = rx_buf[i * ADC_CHANNELS + c] >> 8;   /* escala 24 bits */
          fs_i[c] += (int64_t)x * cq;
          fs_q[c] += (int64_t)x * sq;
        }
      }
      ph += inc;
      if (ph >= FS_PHASE_MAX) ph -= FS_PHASE_MAX;
      n++;
    }
    fs_rx_phase = ph;
    fs_n = n;
    /* FS_SETTLE y FS_WINDOW son múltiplos de AUDIO_BUFFER_SAMPLES (assert
     * abajo), así que el punto cierra SIEMPRE en un borde de mitad: no queda
     * resto que se arrastre al punto siguiente y corrompa su ventana. */
    if (n >= FS_SETTLE + FS_WINDOW) fs_point_ready = 1;
  }

  /* AUTOFOCUS capture: append the measurement mic (24-bit) until the window
   * is full. Started in the same loop pass as the chirp emission, so the
   * emission→capture offset is constant for every speaker and cancels in
   * the relative TOFs. */
  if (cal_capturing) {
    int32_t cpk = cal_cap_peak;
    for (uint32_t i = 0; i < samples && cal_cap_count < CAL_CAPTURE_LEN; i++) {
      int32_t v = rx_buf[i * ADC_CHANNELS + CAL_MIC_CH] >> 8;
      cal_capture[cal_cap_count++] = v;
      if (v < 0) v = -v;
      if (v > cpk) cpk = v;          /* gate absoluto: ¿hubo ALGO en el mic? */
    }
    cal_cap_peak = cpk;
    if (cal_cap_count >= CAL_CAPTURE_LEN) cal_capturing = 0;
  }

  /* RX delay-and-sum: beam[i] = Σₘ delay_line[m][wp + i − rx_delay[m]] >> 8,
   * then >>3 so a coherent identical signal on the 8 mics sums to ~unity.
   * The rx_delay[m] look BACK into history just written/older (≤64 samples,
   * window is 1024) so no wrap hazard. 8 MAC/sample = 3840 MACs per half —
   * negligible (~0.1% CPU). Runs in main-loop context, after mic lines. */
  if (tone_enabled) {
    /* Internal source: fill beam_line exactly where the RX beam would write
     * it — the TX fills read it back with their focal delays untouched. */
    /* Fase B: dither de frecuencia — triángulo 0→peak→0 sobre la base. */
    uint32_t tri = (tone_dither_pos < TONE_DITHER_PERIOD / 2)
                   ? tone_dither_pos : (TONE_DITHER_PERIOD - tone_dither_pos);
    uint32_t base = tone_base_hz;
    tone_freq_hz = (uint16_t)(base - base / 20
                              + (base / 10) * tri / (TONE_DITHER_PERIOD / 2));
    tone_dither_pos = (tone_dither_pos + samples) % TONE_DITHER_PERIOD;
    for (uint32_t i = 0; i < samples; i++) {
      beam_line[(wp + i) & DELAY_LINE_MASK] = Tone_NextSample();
    }
  } else if (beam_enabled) {
    for (uint32_t i = 0; i < samples; i++) {
      int32_t acc = 0;
      for (uint32_t m = 0; m < MIC_COUNT; m++) {
        acc += delay_line[m][(wp + i - rx_delay[m]) & DELAY_LINE_MASK] >> 8;
      }
      beam_line[(wp + i) & DELAY_LINE_MASK] = acc >> 3;
    }
  }

  /* Los DOS relojes del pipeline avanzan juntos, acá y en ningún otro lado.
   * delay_write_pos marca la ventana; mf_frame marca la fase de los tonos del
   * MULTIFOCUS. Van juntos porque son lo mismo: el tiempo.
   *
   * mf_frame vivía en el fill de un banco, y eso arrastraba dos problemas: los
   * otros tres bancos leían un valor distinto (contrafase entre filas), y si
   * ese banco puntual no se llenaba en una pasada la fase no avanzaba. Acá no
   * hay elección de banco que hacer: Service_Fills procesa el RX ANTES que los
   * 4 TX justamente para que todos vean el mismo snapshot de wp — que mf_frame
   * viaje con wp es la misma garantía, gratis. */
  delay_write_pos = (wp + samples) & DELAY_LINE_MASK;
  mf_frame += samples;
}

/**
 * @brief Fill TX buffer with beamformed audio from delay line
 * @param buffer    Pointer to TDM8 output buffer (8ch interleaved)
 * @param samples   Number of sample frames to fill (240 per half-buffer)
 * @param ch_offset Global channel offset (0, 8, 16, or 24)
 *
 * For each of the 8 channels in this SAI block, reads the delay line at the
 * per-channel delay offset, applies gain and mute. Single beam (L only) for now.
 */
static void Fill_Audio_Beamform(int32_t *buffer, uint32_t samples, uint32_t ch_offset) {
  /* Channel walker: 1kHz tone on ONE output channel at a time, advancing every
   * WALKER_SECONDS_PER_CH. Verifies channel→speaker mapping (and, with a scope
   * or mic, polarity) one channel per second. Frame counter advances only in
   * the ch_offset==0 (SAI1_A) call — see walker_frame declaration. */
  if (walker_enabled) {
    uint32_t active_ch = (walker_frame / (WALKER_SECONDS_PER_CH * AUDIO_SAMPLE_RATE))
                         % WALKER_NUM_CH;
    for (uint32_t i = 0; i < samples; i++) {
      int32_t s = sine_table_1khz[(walker_frame + i) % 48];
      for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
        buffer[i * AUDIO_CHANNELS + ch] =
            (ch_offset + ch == active_ch) ? s : 0;
      }
    }
    if (ch_offset == 0) {
      walker_frame += samples;
      // One short line on each channel change (main-loop context, ~3ms UART)
      static uint32_t last_walker_ch = 0xFFFFFFFF;
      if (active_ch != last_walker_ch) {
        last_walker_ch = active_ch;
        sprintf(uart_buf, "WALKER: ch%lu (DAC%lu slot%lu)\r\n",
                (unsigned long)active_ch, (unsigned long)(active_ch / 8),
                (unsigned long)(active_ch % 8));
        UART_Print(uart_buf);
      }
    }
    return;
  }

  /* BARRIDO DE FRECUENCIA: el mismo tono por un canal de cada banco, en fase.
   * Los canales de emisión son los del LOOPBACK a propósito — así el día que
   * estén los adaptadores de §4.2, la misma corrida da la respuesta de los
   * cuatro bancos sin tocar nada. */
  if (fs_enabled) {
    uint8_t local = 0xFF;
    for (uint8_t p = 0; p < LB_NUM_EMIT; p++) {
      if ((lb_emit_ch[p] & ~7u) == ch_offset) local = lb_emit_ch[p] & 7u;
    }
    if (fs_paused) {
      for (uint32_t i = 0; i < samples * AUDIO_CHANNELS; i++) buffer[i] = 0;
      return;
    }
    uint32_t inc = FS_Inc(fs_idx);
    uint8_t  bk  = (uint8_t)(ch_offset >> 3);
    uint32_t ph  = fs_bank_ph[bk];
    for (uint32_t i = 0; i < samples; i++) {
      int32_t sv = FS_Sine(ph);
      ph += inc;
      if (ph >= FS_PHASE_MAX) ph -= FS_PHASE_MAX;
      for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
        buffer[i * AUDIO_CHANNELS + ch] = (ch == local) ? sv : 0;
      }
    }
    /* Cada banco lleva su propia fase, así que el resultado no depende de cómo
     * Service_Fills agrupe las mitades en la pasada. fs_tx_phase queda como
     * maestro (lo lee FS_StartPoint para anotar el origen en fs_tx0). */
    fs_bank_ph[bk] = ph;
    if (ch_offset == 24) fs_tx_phase = ph;
    return;
  }

  /* LOOPBACK: el chirp sale por UN canal de CADA banco, todos a la vez.
   *
   * Simultáneo a propósito: es lo único que permite medir el skew entre bancos.
   * Si se emitiera de a uno, cada medida tendría su propio origen de tiempo y
   * la diferencia entre ellas no significaría nada. */
  if (lb_enabled) {
    if (lb_state == LB_ARM) {
      uint8_t local = 0xFF;                    /* canal local de este banco */
      for (uint8_t p = 0; p < LB_NUM_EMIT; p++) {
        if ((lb_emit_ch[p] & ~7u) == ch_offset) local = lb_emit_ch[p] & 7u;
      }
      /* Retardo fraccional opcional sobre el chirp, para AUDITAR la
       * interpolación Q8 de punta a punta.
       *
       * Por qué existe: el equipo dice tener 81,38 ns de resolución de retardo
       * (Q8 = 1/256 de muestra) y eso nunca se verificó contra el mundo físico.
       * Si la interpolación estuviera sutilmente mal, TODOS los beams estarían
       * sutilmente mal y no hay ninguna bandera de error que lo delate. El
       * loopback mide con esa misma resolución y su piso está por debajo de
       * 81 ns (docs/loopback_debug.md §8), así que es el instrumento adecuado.
       *
       * La fórmula es DELIBERADAMENTE la misma del fill real —
       * `s0 + ((s1-s0)*fr >> 8)`— y no una copia "equivalente": si se
       * reimplementara acá, la prueba verificaría esta copia en vez del código
       * que de verdad sale por los parlantes.
       *
       * De dónde sale el valor: `beam_config.delays[]` es settable por
       * `POST /command` y el loopback no lo usa para nada, así que en ESTE modo
       * se reinterpreta como Q8 en vez de muestras enteras. Es un abuso, y por
       * eso está acotado a un modo de banco: evita tocar el protocolo, el ESP32
       * y el número de modos para una medición de laboratorio. Con delays en 0
       * —el default— el comportamiento es idéntico al de siempre.
       *
       * ⚠️ Acotar el barrido a ≤2 muestras (512 en Q8): el chirp se emite en una
       * ventana de CAL_CHIRP_LEN, así que un retardo mayor le corta la cola. Con
       * 2 de 480 muestras el corrimiento del pico es despreciable, y a cambio la
       * ventana no cambia y las medidas siguen siendo comparables con las de §8. */
      int32_t dq8 = (local == 0xFF) ? 0
                  : (int32_t)beam_config.delays[ch_offset + local];
      for (uint32_t i = 0; i < samples; i++) {
        uint32_t k  = lb_bank_pos[ch_offset >> 3] + i;
        int32_t  sv = 0;
        int32_t  t  = ((int32_t)k << 8) - dq8;
        if (t >= 0) {
          uint32_t i0 = (uint32_t)t >> 8;
          int32_t  fr = t & 0xFF;
          int32_t  s0 = (i0     < CAL_CHIRP_LEN) ? cal_chirp[i0]     : 0;
          int32_t  s1 = (i0 + 1 < CAL_CHIRP_LEN) ? cal_chirp[i0 + 1] : 0;
          sv = s0 + (((s1 - s0) * fr) >> 8);
        }
        for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
          buffer[i * AUDIO_CHANNELS + ch] = (ch == local) ? sv : 0;
        }
      }
      lb_bank_pos[ch_offset >> 3] += (uint16_t)samples;
      /* El estado (armado y fin de chirp) sigue gobernado por el banco 24, que
       * es como se calibró §13; lo que ya no depende del banco es la POSICIÓN
       * que emite cada uno — ver lb_bank_pos.
       *
       * ⚠️ El avance de estado va en el ÚLTIMO banco (24), nunca en el primero.
       * Service_Fills corre los cuatro en orden 0→8→16→24 dentro de la misma
       * pasada. Avanzando en el banco 0, los otros tres leerían lb_chirp_pos ya
       * adelantado y emitirían el trozo SIGUIENTE del chirp: los bancos
       * quedarían desfasados una ventana entre sí — y este modo existe
       * justamente para medir desfasajes entre bancos, así que se mediría el
       * error introducido acá. Es el mismo error que tuvo mf_frame. */
      if (ch_offset == 24) {
        if (lb_chirp_pos == 0) {
          lb_cap_count = 0;
          /* Fase de los DMA EN EL INSTANTE de armar, antes de nada más.
           *
           * Por qué: sobre 26 arranques la latencia del lazo cae en TRES estados
           * discretos separados 3,02 y 1,13 muestras (docs/loopback_debug.md §8).
           * Los group delays del PCM1690 y del AK5538 son constantes de datasheet,
           * así que lo que varía tiene que ser la fase relativa RX↔TX dentro del
           * STM32 — pero eso era una conjetura. NDTR la convierte en dato: si el
           * estado correlaciona con esta diferencia, está explicado, y de paso
           * queda el número con el que compensar por nodo (multinode.md §2 nivel 2).
           *
           * Se leen con las interrupciones apagadas y sin nada en el medio: si
           * un ISR cayera entre las dos lecturas, el DMA habría avanzado y la
           * fase saldría corrida. Aun así quedan ~10 ciclos de CPU de diferencia
           * entre una y otra, contra 10.000 que dura una muestra. */
          {
            uint32_t pm = __get_PRIMASK();
            __disable_irq();
            lb_ndtr_rx = __HAL_DMA_GET_COUNTER(&hdma_sai3_b);
            lb_ndtr_tx = __HAL_DMA_GET_COUNTER(&hdma_sai2_b);
            /* Dentro del MISMO bloqueo que los NDTR: si se leyeran afuera, un
             * borde de RX podría caer en el medio y la foto mezclaría dos
             * instantes — que es justamente el error que se está buscando. */
            lb_arm_pend = (uint8_t)(rx_3b_first_half_ready
                                    | (rx_3b_second_half_ready << 1));
            lb_arm_rxcb = cb_rx3b_count;
            __set_PRIMASK(pm);
          }
          lb_cap_half = 0xFF;              /* lo llena el primer copiado */
          lb_capturing = 1;
        }
        lb_chirp_pos += samples;
        if (lb_chirp_pos >= CAL_CHIRP_LEN) lb_state = LB_CAPT;
      }
      return;
    }
    for (uint32_t i = 0; i < samples * AUDIO_CHANNELS; i++) buffer[i] = 0;
    return;
  }

  /* AUTOFOCUS mode: during the scan, emit the chirp on one speaker at a time
   * (everything else silent); once done, play a 1kHz tone steered with the
   * MEASURED delays & polarities — audibly focused at the mic position. */
  if (autofocus_enabled) {
    // SETTLE countdown ticks on the always-firing SAI1_A fills
    if (ch_offset == 0 && cal_state == CAL_SETTLE && cal_settle > 0) {
      if (--cal_settle == 0) { cal_chirp_pos = 0; cal_state = CAL_ARM; }
    }

    if (cal_state == CAL_ARM && ch_offset == (cal_ch & ~7u)) {
      /* ⚠️ El chirp NO entra en una ventana. Son CAL_CHIRP_LEN = 480 muestras y
       * el buffer pasó a AUDIO_BUFFER_SAMPLES = 240 el 2026-08-18: este lazo
       * decía `cal_chirp[i]` con i < samples, o sea que emitía la PRIMERA MITAD
       * del chirp (500 Hz → ~3,2 kHz) mientras la correlación seguía usando la
       * referencia de 480 completa. El pico quedaba a la mitad y con más
       * lóbulos laterales — o sea calidad degradada en todos los scans, sin
       * ningún síntoma visible. Con CAL_Q_MIN en 100 eso pasaba de "peor" a
       * "podría no llegar al umbral con un micrófono real".
       *
       * Es el mismo tipo de bug que el de la tabla de senos y el de mf_frame:
       * el tercero que deja el cambio de 480→240 (ver ds11_perf.md §7).
       *
       * Ahora se emite a lo largo de las ventanas que hagan falta y la captura
       * arranca con la PRIMERA, así el offset emisión→captura sigue siendo
       * constante para todos los parlantes — que es lo que hace que las
       * latencias fijas del pipeline se cancelen en los retardos relativos. */
      uint32_t local = cal_ch & 7u;
      for (uint32_t i = 0; i < samples; i++) {
        uint32_t k = cal_chirp_pos + i;
        int32_t s = (k < CAL_CHIRP_LEN) ? cal_chirp[k] : 0;
        for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
          buffer[i * AUDIO_CHANNELS + ch] = (ch == local) ? s : 0;
        }
      }
      if (cal_chirp_pos == 0) {          // arrancar la captura con la 1ª ventana
        cal_cap_count = 0;
        cal_cap_peak = 0;
        cal_capturing = 1;
      }
      cal_chirp_pos += samples;
      if (cal_chirp_pos >= CAL_CHIRP_LEN) cal_state = CAL_CAPT;
      return;
    }

    if (cal_state == CAL_DONE) {
      // Steered tone: per-channel fractional phase offset (Q8) on the 1kHz
      // table (period 48 = 12288 in Q8; 480-sample halves keep it seamless).
      for (uint32_t i = 0; i < samples; i++) {
        int32_t base_q8 = (int32_t)(i % 48) << 8;
        for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
          uint32_t gch = ch_offset + ch;
          int32_t s = 0;
          if (gch < CAL_NUM_CH) {
            int32_t t = base_q8 - (int32_t)(tx_delay_q8[gch] % 12288u);
            if (t < 0) t += 12288;
            uint32_t i0 = (uint32_t)t >> 8;
            int32_t fr = t & 0xFF;
            int32_t s0 = sine_table_1khz[i0];
            int32_t s1 = sine_table_1khz[(i0 + 1) % 48];
            s = s0 + (((s1 - s0) * fr) >> 8);   // Δtable×255 ≪ 2³¹
            if (speaker_sign[gch] < 0) s = -s;
          }
          buffer[i * AUDIO_CHANNELS + ch] = s;
        }
      }
      return;
    }

    // SETTLE / CAPT / CORR: silence everywhere
    for (uint32_t i = 0; i < samples * AUDIO_CHANNELS; i++) buffer[i] = 0;
    return;
  }

  /* MULTIFOCUS: suma de N tonos, cada uno con su propia tabla de delays
   * focales. Para un seno el retardo de tiempo es EXACTAMENTE un offset de
   * fase (precalculado en mf_phoff_q16), así que el foco es tan verdadero
   * como el del modo 12/13 pero sin pasar por beam_line. La fase de la
   * ventana sale de mf_frame — ver nota en la declaración (bancos ≠0 la ven
   * +480, exacto para f múltiplo de 100 Hz). */
  if (mfocus_enabled) {
    uint32_t ph[MF_NUM_SRC];
    for (uint32_t s = 0; s < MF_NUM_SRC; s++)
      ph[s] = (uint32_t)(((uint64_t)mf_frame * mf_inc_q16[s]) % TONE_PERIOD_Q16);
    uint8_t unmuted = !beam_config.mute_global;
    for (uint32_t i = 0; i < samples; i++) {
      for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
        uint32_t gch = ch_offset + ch;
        int32_t acc = 0;
        if (unmuted && !((beam_config.mutes >> gch) & 1)) {
          for (uint32_t s = 0; s < MF_NUM_SRC; s++) {
            int32_t p = (int32_t)ph[s] - (int32_t)mf_phoff_q16[s][gch];
            if (p < 0) p += (int32_t)TONE_PERIOD_Q16;
            uint32_t i0 = (uint32_t)p >> 16;               // 0..47
            int32_t  fr = p & 0xFFFF;
            int32_t  s0 = sine_table_1khz[i0];
            int32_t  s1 = sine_table_1khz[(i0 + 1u) % 48u];
            // lerp: (s1−s0) hasta 2^20 × fr 2^16 → intermedio de 64 bits
            acc += (s0 + (int32_t)(((int64_t)(s1 - s0) * fr) >> 16)) >> MF_SRC_SHIFT;
          }
          acc = (acc * (int32_t)beam_config.gains[gch]) >> 8; // -6dBFS×255 < 2³¹
          if (speaker_sign[gch] < 0) acc = -acc;
          if (acc > 8388607) acc = 8388607;
          else if (acc < -8388608) acc = -8388608;
        }
        buffer[i * AUDIO_CHANNELS + ch] = acc;
      }
      for (uint32_t s = 0; s < MF_NUM_SRC; s++) {
        ph[s] += mf_inc_q16[s];
        if (ph[s] >= TONE_PERIOD_Q16) ph[s] -= TONE_PERIOD_Q16;
      }
    }
    /* mf_frame NO se avanza acá — lo hace Process_ADC_Half, que es el reloj
     * común del pipeline (ver la nota en la declaración). */
    return;
  }

  /* MULTIFOCUS de ENTRADAS (la intervención): cada entrada analógica se
   * proyecta con SU tabla de delays focales — 8 señales, 8 puntos de la
   * sala. Delay temporal verdadero con interpolación Q8 sobre delay_line
   * (mismo esquema que el TX steering sobre beam_line), así vale para
   * señal arbitraria (voz, música), no solo senos. */
  if (mfocus_in_enabled) {
    uint32_t wp = delay_write_pos;
    uint8_t unmuted = !beam_config.mute_global;
    /* DS11: 8 fuentes × 32 salidas es el fill más caro del firmware. El
     * delay entero y su fracción son constantes en toda la ventana, así que
     * se calculan una vez por fill (64 entradas) en vez de 8 veces por
     * muestra — saca las lecturas de tabla del lazo interno. */
    uint32_t d_int[AUDIO_CHANNELS][MF_IN_NUM_SRC];
    int32_t  d_fr[AUDIO_CHANNELS][MF_IN_NUM_SRC];
    for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
      for (uint32_t src = 0; src < MF_IN_NUM_SRC; src++) {
        uint32_t dq = mf_delay_q8[src][ch_offset + ch];
        d_int[ch][src] = dq >> 8;
        d_fr[ch][src]  = (int32_t)(dq & 0xFF);
      }
    }
    /* La ganancia de entrada se aplica UNA VEZ a la suma, no una vez por fuente.
     * Es lo mismo (la ganancia es lineal) y ahorra 7 de cada 8 multiplicaciones
     * en el lazo más caliente del firmware. Además se lee in_gain_q8 a un local:
     * es `volatile`, así que dentro del lazo el compilador estaba obligado a
     * recargarla de memoria en cada uso.
     *
     * Medido con `cpu_fills_pct` de /status: este modo estaba al tope del
     * presupuesto —tanto que ni contestaba la telemetría— y ninguna otra parte
     * del pipeline lo explicaba. Son 8 fuentes × 8 canales × 240 muestras × 4
     * bancos = 61440 multiplicaciones de 64 bits por ventana de 5 ms, que
     * reemplazaron a un simple shift cuando la ganancia pasó a ser ajustable. */
    const int32_t g_q8 = (int32_t)in_gain_q8;
    for (uint32_t i = 0; i < samples; i++) {
      for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
        uint32_t gch = ch_offset + ch;
        int32_t acc = 0;
        if (unmuted && !((beam_config.mutes >> gch) & 1)) {
          int32_t sum = 0;
          for (uint32_t src = 0; src < MF_IN_NUM_SRC; src++) {
            uint32_t base = wp - d_int[ch][src] - samples + i;
            int32_t fr = d_fr[ch][src];
            int32_t s0 = delay_line[src][base & DELAY_LINE_MASK] >> 8;
            int32_t s1 = delay_line[src][(base - 1) & DELAY_LINE_MASK] >> 8;
            // (s1−s0) 25 bits × fr 8 bits → intermedio de 64 bits
            sum += s0 + (int32_t)(((int64_t)(s1 - s0) * fr) >> 8);
          }
          /* sum: 8 fuentes × 24 bits = 27 bits; × g_q8 (≤ 2^14) = 41 bits → 64. */
          acc = (int32_t)(((int64_t)sum * g_q8) >> 8);
          // Clamp ANTES del gain por canal: 8 fuentes × boost pueden exceder
          // ±FS, y FS×255 es el máximo que el multiply soporta en int32.
          if (acc > 8388607) acc = 8388607;
          else if (acc < -8388608) acc = -8388608;
          acc = (acc * (int32_t)beam_config.gains[gch]) >> 8;
          if (speaker_sign[gch] < 0) acc = -acc;
        }
        buffer[i * AUDIO_CHANNELS + ch] = acc;
      }
    }
    return;
  }

  /* Beam modes: output derived from beam_line (the RX delay-and-sum beam,
   * 24-bit scale, written by Process_ADC_Half). */
  if (beam_enabled) {
    uint32_t wp = delay_write_pos;

    /* TX steered (Phase 2): each output channel plays the beam delayed by
     * its focal delay (farthest speaker fires first) with per-channel
     * gain/sign and the global master/mutes — wavefronts converge at the
     * focal point set by Beam_UpdateTxFocus. */
    if (tx_steer_enabled) {
      /* Volumen maestro unificado: SOLO el atenuador del PCM1690 (/volume).
       * El master_gain digital se eliminó del path (redundante, perdía SNR);
       * beam_config.master_gain queda en el struct solo por compatibilidad
       * de layout EEPROM. gains[ch] se conserva (ventaneo Hann futuro). */
      uint8_t unmuted = !beam_config.mute_global;
      for (uint32_t i = 0; i < samples; i++) {
        for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
          uint32_t gch = ch_offset + ch;
          int32_t s = 0;
          if (unmuted && !((beam_config.mutes >> gch) & 1)) {
            // Fractional delay (Q8): linear interpolation between the two
            // adjacent samples; index−1 is one sample OLDER = more delay.
            uint32_t dq = tx_delay_q8[gch];
            uint32_t base = wp - (dq >> 8) - samples + i;
            int32_t frac = (int32_t)(dq & 0xFF);
            int32_t s0 = beam_line[base & DELAY_LINE_MASK];
            int32_t s1 = beam_line[(base - 1) & DELAY_LINE_MASK];
            // (s1−s0) spans 25 bits × frac(8 bits) → use 64-bit intermediate
            s = s0 + (int32_t)(((int64_t)(s1 - s0) * frac) >> 8);
            s = (s * (int32_t)beam_config.gains[gch]) >> 8;  // 24-bit×255 < 2³¹
            if (speaker_sign[gch] < 0) s = -s;
            s = (int32_t)(((int64_t)s * (int32_t)beam_out_q8) >> 8);
            if (s > 8388607)       { s = 8388607;  out_clip_count++; }
            else if (s < -8388608) { s = -8388608; out_clip_count++; }
          }
          buffer[i * AUDIO_CHANNELS + ch] = s;
        }
      }
      return;
    }

    /* RX beam monitor: the same mono beam on every output channel, boosted
     * like the ADC monitor. */
    for (uint32_t i = 0; i < samples; i++) {
      int32_t s = beam_line[(wp - samples + i) & DELAY_LINE_MASK];
      s = (int32_t)(((int64_t)s * (int32_t)beam_out_q8) >> 8);
      if (s > 8388607)       { s = 8388607;  out_clip_count++; }
      else if (s < -8388608) { s = -8388608; out_clip_count++; }
      for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
        buffer[i * AUDIO_CHANNELS + ch] = s;
      }
    }
    return;
  }

  /* ADC monitor mode: play the live digitized ADC straight out the DACs.
   * DAC channel ch ← ADC channel (ch_offset+ch), most recent samples, boosted
   * so the low-level capture is audible, then clamped to 24-bit full scale. */
  if (adc_monitor_enabled) {
    uint32_t wp = delay_write_pos;
    for (uint32_t i = 0; i < samples; i++) {
      uint32_t read_pos = (wp - samples + i) & DELAY_LINE_MASK;
      for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
        // DS11: 8 entradas ADC → 32 salidas. Cada banco de 8 parlantes
        // (= una fila) espeja las 8 entradas, así se escucha la entrada N
        // en la columna N de las 4 filas (antes ch8-31 repetían la 0).
        uint32_t src = (ch_offset + ch) & 7u;
        int32_t s = delay_line[src][read_pos] >> 8; // 32-bit ADC → 24-bit
        s = Apply_In_Gain(s);                       // ganancia de entrada ajustable
        /* Contar los recortes: es lo que distingue "satura el ADC" (que se
         * arregla bajando la fuente o con atenuador) de "satura el boost de
         * +30 dB de este modo" (que se arregla usando otro modo). Suenan
         * igual y se arreglan distinto. */
        if (s > 8388607)       { s = 8388607;  out_clip_count++; }
        else if (s < -8388608) { s = -8388608; out_clip_count++; }
        buffer[i * AUDIO_CHANNELS + ch] = s;
      }
    }
    return;
  }

  /* Test tone mode: 1kHz sine on all 8 channels, bypasses beamforming */
  if (test_tone_enabled) {
    uint32_t phase = test_tone_phase % 48;
    for (uint32_t i = 0; i < samples; i++) {
      int32_t s = sine_table_1khz[phase] >> test_tone_shift;
      if (++phase >= 48) phase = 0;
      /* SAI with DataSize=24 transmits bits [23:0] of the FIFO word
       * (right-aligned per RM0433) — write the 24-bit sample directly.
       * The old `s << 8` placed it in [31:8]: the SAI then sent only the
       * low 16 bits → waveform wrapping → severe harmonic distortion
       * (the "square-ish 1kHz" heard on speakers, and the large HF hash
       * seen on the scope on both DS9 and DS10). */
      int32_t out = s;
      for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
        buffer[i * AUDIO_CHANNELS + ch] = out;
      }
    }
    test_tone_phase += samples;
    return;
  }

  uint32_t wp = delay_write_pos;
  uint8_t master = beam_config.mute_global ? 0 : beam_config.master_gain;

  for (uint32_t i = 0; i < samples; i++) {
    for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
      uint32_t gch = ch_offset + ch;
      uint16_t delay = beam_config.delays[gch];
      if (delay > MAX_DELAY_SAMPLES) delay = MAX_DELAY_SAMPLES;

      // Read from delay line: wp points to next write position,
      // so the most recent sample is at wp-1. We want sample that is
      // 'delay' samples old, offset by buffer position.
      uint32_t read_pos = (wp - delay - samples + i) & DELAY_LINE_MASK;
      uint32_t src_ch = (gch < 8) ? gch : 0;  // map global ch to ADC ch; clamp to 0-7
      /* ADC samples are 32-bit (SAI3_B DataSize=32); the DAC path is 24-bit
       * right-aligned (SAI1/2 DataSize=24 → bits [23:0]). Shift before gain —
       * also keeps `sample * gain` within int32 range. */
      int32_t sample = delay_line[src_ch][read_pos] >> 8;

      // Apply per-channel gain (fixed-point: * gain >> 8)
      uint8_t gain = beam_config.gains[gch];
      sample = (sample * gain) >> 8;

      // Apply master gain
      sample = (sample * master) >> 8;

      // Apply per-channel mute
      if (beam_config.mutes & (1UL << gch)) {
        sample = 0;
      }

      buffer[i * AUDIO_CHANNELS + ch] = sample;
    }
  }
}

/**
 * @brief SAI TX Half Complete callback - MINIMAL ISR
 * DMA finished sending first half → CPU can now safely fill it
 * Called every 10ms (half of 20ms buffer cycle)
 *
 * IMPORTANT: No heavy processing here! Just set flag for main loop.
 * ISR budget: ~20 cycles (vs ~50000 if sinf() was called here)
 */
void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
  if (hsai->Instance == SAI1_Block_A) {
    uint32_t now = DWT->CYCCNT;


    half_complete_count++;

    // Jitter: measure period between consecutive Half callbacks
    if (last_half_isr_cycles != 0) {
      uint32_t period = now - last_half_isr_cycles;
      // Sanity guard: expected ~9.6M cycles (20ms@480MHz). Reject > 15M (31ms) as stale/reset artifact
      if (period < 15000000) {
        if (period < jitter_min_cycles) jitter_min_cycles = period;
        if (period > jitter_max_cycles) jitter_max_cycles = period;
        jitter_sum_cycles += period;
        jitter_sample_count++;
      }
    }
    last_half_isr_cycles = now;

    // Underrun detection: previous request not yet serviced
    if (need_fill_first_half) {
      underrun_count++;
    }

    cb_a1_count++;
    cb_a1_half_dwt = now;          // referencia de fase del banco ch0-15
    need_fill_first_half = 1;
    __DSB();
  }
  else if (hsai->Instance == SAI1_Block_B) {
    if (need_fill_1b_first_half) underrun_count++;
    cb_1b_count++;
    need_fill_1b_first_half = 1;
    __DSB();
  }
  else if (hsai->Instance == SAI2_Block_A) {
    if (need_fill_2a_first_half) underrun_count++;
    cb_2a_count++;
    cb_2a_half_dwt = DWT->CYCCNT;  // referencia de fase del banco ch16-31
    need_fill_2a_first_half = 1;
    __DSB();
  }
  else if (hsai->Instance == SAI2_Block_B) {
    if (need_fill_2b_first_half) underrun_count++;
    cb_2b_count++;
    need_fill_2b_first_half = 1;
    __DSB();
  }
}

/**
 * @brief SAI TX Full Complete callback - MINIMAL ISR
 * DMA finished sending second half → CPU can now safely fill it
 */
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
  if (hsai->Instance == SAI1_Block_A) {
    uint32_t now = DWT->CYCCNT;


    full_complete_count++;

    // Jitter: measure period between consecutive Full callbacks
    if (last_full_isr_cycles != 0) {
      uint32_t period = now - last_full_isr_cycles;
      // Sanity guard: expected ~9.6M cycles (20ms@480MHz). Reject > 15M (31ms) as stale/reset artifact
      if (period < 15000000) {
        if (period < jitter_min_cycles) jitter_min_cycles = period;
        if (period > jitter_max_cycles) jitter_max_cycles = period;
        jitter_sum_cycles += period;
        jitter_sample_count++;
      }
    }
    last_full_isr_cycles = now;

    if (need_fill_second_half) {
      underrun_count++;
    }

    cb_a1_count++;
    need_fill_second_half = 1;
    __DSB();
  }
  else if (hsai->Instance == SAI1_Block_B) {
    if (need_fill_1b_second_half) underrun_count++;
    cb_1b_count++;
    need_fill_1b_second_half = 1;
    __DSB();
  }
  else if (hsai->Instance == SAI2_Block_A) {
    if (need_fill_2a_second_half) underrun_count++;
    cb_2a_count++;
    need_fill_2a_second_half = 1;
    __DSB();
  }
  else if (hsai->Instance == SAI2_Block_B) {
    if (need_fill_2b_second_half) underrun_count++;
    cb_2b_count++;
    need_fill_2b_second_half = 1;
    __DSB();
  }
}

/**
 * @brief SAI RX Half Complete callback
 * DMA filled first half of RX buffer → CPU can process it
 */
void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
  if (hsai->Instance == SAI3_Block_B) {
    rx_3b_first_half_ready = 1;
    cb_rx3b_count++;
    __DSB();
  }
}

/**
 * @brief SAI RX Full Complete callback
 * DMA filled second half of RX buffer → CPU can process it
 */
void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai) {
  if (hsai->Instance == SAI3_Block_B) {
    rx_3b_second_half_ready = 1;
    cb_rx3b_count++;
    __DSB();
  }
}

/**
 * @brief SAI Error callback
 */
void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai) {
  error_count++;
  last_sai_error = HAL_SAI_GetError(hsai);

  // Immediate debug output (UART7 → J7 → ST-Link VCP)
  char err_buf[80];
  sprintf(err_buf, "!!! SAI ERROR: 0x%08lX ", last_sai_error);
  HAL_UART_Transmit(&huart7, (uint8_t*)err_buf, strlen(err_buf), 100);

  // Decode error flags
  if (last_sai_error & 0x01) HAL_UART_Transmit(&huart7, (uint8_t*)"OVR ", 4, 10);
  if (last_sai_error & 0x02) HAL_UART_Transmit(&huart7, (uint8_t*)"UDR ", 4, 10);
  if (last_sai_error & 0x20) HAL_UART_Transmit(&huart7, (uint8_t*)"WCKCFG ", 7, 10);
  if (last_sai_error & 0x80) HAL_UART_Transmit(&huart7, (uint8_t*)"DMA ", 4, 10);
  HAL_UART_Transmit(&huart7, (uint8_t*)"\r\n", 2, 10);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
