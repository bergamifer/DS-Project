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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define AUDIO_SAMPLE_RATE    48000
#define AUDIO_BUFFER_SAMPLES 480      // 10ms @ 48kHz
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
// Only U2/DAC0 (ch0-7) is populated on this DS10 — walk 8 channels.
// Set to 32 on DS11 to validate the full speaker map + JSPK wiring.
#define WALKER_NUM_CH        8

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
#define CAL_NUM_CH        WALKER_NUM_CH           // scan the populated channels
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Beamforming config — 32 channels (expandable from ESP32's 16-ch ds_config_t) */
typedef struct {
    uint8_t  master_gain;                   // 0-255 → 0.0-1.0
    uint8_t  mute_global;                   // 0=unmuted, 1=muted
    uint16_t delays[BEAM_NUM_CHANNELS];     // delay in samples per channel
    uint8_t  gains[BEAM_NUM_CHANNELS];      // gain per channel 0-255
    uint32_t mutes;                         // bitfield: bit N = channel N muted
    int8_t   master_vol_db;                 // PCM1690 attenuation, dB (0..-63, ≤-64 mute)
    uint8_t  last_mode;                     // ui_states index restored at boot
} beam_config_t;

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
// delay_line[0] = ADC ch0 (main beam source); [1]-[7] stubs for future expansion
static int32_t delay_line[8][DELAY_LINE_SIZE] __attribute__((section(".dma_buffer"), aligned(32)));
static uint32_t delay_write_pos = 0;

// Active beamforming config (copied from ESP32 ds_config_t on update)
static beam_config_t beam_config;

/* ===== Test Tone: 1kHz sine @ 48kHz sample rate =====
 * 48 samples per period (48000/1000 = 48), amplitude = -6dBFS (0x400000 peak)
 * 480 samples per half-buffer = exactly 10 periods → no phase error across calls
 * SET test_tone_enabled = 0 to restore normal beamforming mode
 */
static const int32_t sine_table_1khz[48] = {
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
 * ADC channel (ch0→ch0 …). Only DAC0/U2 (ch0-7) is populated on this board, so
 * you hear ADC ch0-7 on U2. The ADC signal is low-level (hum ~-55dBFS, noise
 * floor ~-95dBFS), so it is boosted by (1<<adc_monitor_shift) before the DAC,
 * then clamped to 24-bit FS. Takes priority over test tone and beamform. */
static volatile uint8_t adc_monitor_enabled = 1;   /* 1 = live ADC→DAC monitor */
static volatile uint8_t adc_monitor_shift = 5;     /* gain boost: 5 = ×32 ≈ +30dB */

/* ===== Operating states — cycled with user button K2 (PA4, press = LOW) =====
 * Flat list: each press advances one state and prints the new one on UART7.
 *   0 ADC monitor  → 1 channel walker → 2..6 RX beam at preset azimuths → wrap
 * Exactly one of adc_monitor_enabled / walker_enabled / beam_enabled is set. */
typedef struct {
  const char *name;
  uint8_t monitor, walker, beam;
  int16_t az_deg;                 // RX beam steering azimuth
  uint8_t tx;                     // 1 = TX steered output (Phase 2)
  int16_t fx_mm, fy_mm, fz_mm;    // TX focal point, relative to array center
  uint8_t af;                     // 1 = AUTOFOCUS mic-scan mode
  uint8_t tone;                   // 1 = internal tone source feeds beam_line
  uint16_t tone_hz;               // tone frequency (keep ≤2140: d=λ/2 limit)
} ui_state_t;
static const ui_state_t ui_states[] = {
  { .name = "ADC monitor (+30dB)",          .monitor = 1 },
  { .name = "Channel walker (1s/ch)",       .walker = 1 },
  { .name = "RX beam az=0 (broadside)",     .beam = 1, .az_deg =   0 },
  { .name = "RX beam az=+30",               .beam = 1, .az_deg =  30 },
  { .name = "RX beam az=-30",               .beam = 1, .az_deg = -30 },
  { .name = "RX beam az=+60",               .beam = 1, .az_deg =  60 },
  { .name = "RX beam az=-60",               .beam = 1, .az_deg = -60 },
  // TX steering presets (Phase 2). Source = RX beam at broadside (az=0);
  // each output channel gets its near-field focal delay. X in the frame of
  // a LISTENER FACING THE CONES (if L/R turn out mirrored on the real box,
  // flip the sign of SPK coords — one line). Y toward row 4.
  { .name = "TX focus center @1m",          .beam = 1, .tx = 1,
    .fx_mm =    0, .fy_mm = 0, .fz_mm = 1000 },
  { .name = "TX focus left 0.5m @1m",       .beam = 1, .tx = 1,
    .fx_mm = -500, .fy_mm = 0, .fz_mm = 1000 },
  { .name = "TX focus right 0.5m @1m",      .beam = 1, .tx = 1,
    .fx_mm =  500, .fy_mm = 0, .fz_mm = 1000 },
  { .name = "TX focus center NEAR 0.3m",    .beam = 1, .tx = 1,
    .fx_mm =    0, .fy_mm = 0, .fz_mm =  300 },
  // Measured focal calibration: mic at the target point → ADC input 1 (c0),
  // chirp scan of every speaker → TOF-derived delays + polarity, then a
  // steered tone focused at the mic. Re-enter the mode to re-scan.
  { .name = "AUTOFOCUS scan (mic->ADC c0)",  .af = 1 },
  // Internal tone → beam_line → the TX steering path runs UNCHANGED (Q8
  // fractional delays, gains, signs): directionality validation with no mic
  // on the board (docs/ds10_tone_beam_plan.md). fy=-240 puts the focus in
  // the plane of row 1 — the only populated row; a linear array cannot
  // discriminate Y anyway. 2kHz spot @1m: ±14cm; 1kHz: ±27cm (ratio ~2 is
  // the self-validating test). APPEND ONLY: the ESP32/web/app mirror these
  // indices.
  { .name = "TX tone 2k focus @1m",         .beam = 1, .tx = 1, .tone = 1,
    .tone_hz = 2000, .fx_mm = 0, .fy_mm = -240, .fz_mm = 1000 },
  { .name = "TX tone 1k focus @1m",         .beam = 1, .tx = 1, .tone = 1,
    .tone_hz = 1000, .fx_mm = 0, .fy_mm = -240, .fz_mm = 1000 },
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
/* Output boost for the ADC-fed beam (weak). MUST be 0 in tone modes: the
 * internal tone is already -6dBFS and ×32 would clip hard into the PAMs —
 * UI_ApplyState() sets this per mode. */
static volatile uint8_t beam_out_shift = 5;

/* ===== Internal tone source (TX directionality without a mic) =====
 * Writes beam_line[] at the same positions/cadence as the RX beam, so the
 * TX steering path is reused as-is and the delay stays a true TIME delay at
 * any frequency (unlike the CAL_DONE tone, whose phase trick is 1kHz-only).
 * Phase accumulator in table-index Q16 over sine_table_1khz[48] (one exact
 * 1kHz period @48k): step per sample = f/1000 indices. Main-loop context. */
static volatile uint8_t  tone_enabled = 0;
static volatile uint16_t tone_freq_hz = 2000;
static uint32_t tone_phase_q16 = 0;
/* Dither de frecuencia (Fase B, docs/ds10_tone_beam_plan.md): triángulo ±5%
 * a ~3Hz alrededor de tone_base_hz. El haz es de retardos puros (banda ancha)
 * → no cambia; los modos de la sala se promedian y la medición se estabiliza.
 * El acumulador de fase de Tone_NextSample es continuo → sin clicks. La
 * frecuencia se actualiza una vez por half-buffer (pasos ≤0.3% cada 10ms).
 * Techo: 2000×1.05 = 2100 Hz < 2140 (límite d=λ/2, sin grating lobes). */
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
static uint16_t cal_quality[BEAM_NUM_CHANNELS]; // peak/avg ×10 (≥~50 = solid)

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

// Underrun detection: flag set but not serviced before next ISR
static volatile uint32_t underrun_count = 0;

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
static void UART_Print(const char *str);
static void Print_DMA_Config(void);
static void Beam_UpdateDelays(int16_t az_deg);
static void Beam_InitTxGeometry(void);
static void Beam_UpdateTxFocus(int16_t fx_mm, int16_t fy_mm, int16_t fz_mm);
static uint8_t Apply_Master_Volume(int8_t db);
static void UI_ApplyState(uint8_t idx);
static void UI_ApplyCustomBeam(const ds_beam_req_t *req);
static void Cal_Init(void);
static void Cal_StartScan(void);
static void Cal_Process(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

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
  UART_Print("\r\n=== DS10 Beamforming Audio ===\r\n");
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

  // Default beam config: all gains max, no delays, unmuted
  beam_config.master_gain = 255;
  beam_config.mute_global = 0;
  memset(beam_config.delays, 0, sizeof(beam_config.delays));
  memset(beam_config.gains, 255, sizeof(beam_config.gains));  // 0xFF = unity gain
  beam_config.mutes = 0;
  beam_config.master_vol_db = -24;   // matches PCM1690 init default → first boot unchanged
  beam_config.last_mode = 0;         // monitor
  Beam_InitTxGeometry();
  Cal_Init();                       // AUTOFOCUS chirp reference
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

  // Slaves must start before masters (SAI sync requirement)
  HAL_StatusTypeDef status;
  status = HAL_SAI_Transmit_DMA(&hsai_BlockB1, (uint8_t*)audio_buffer_1b, AUDIO_BUFFER_SIZE * 2);
  sprintf(uart_buf, "SAI1_B DMA: %s\r\n", status == HAL_OK ? "OK" : "FAIL");
  UART_Print(uart_buf);

  status = HAL_SAI_Transmit_DMA(&hsai_BlockB2, (uint8_t*)audio_buffer_2b, AUDIO_BUFFER_SIZE * 2);
  sprintf(uart_buf, "SAI2_B DMA: %s\r\n", status == HAL_OK ? "OK" : "FAIL");
  UART_Print(uart_buf);

  // Masters — SAI1_A generates MCLK (PE2) which PCM1690 needs for POR
  status = HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t*)audio_buffer, AUDIO_BUFFER_SIZE * 2);
  sprintf(uart_buf, "SAI1_A DMA: %s (MCLK 24.576MHz running)\r\n", status == HAL_OK ? "OK" : "FAIL");
  UART_Print(uart_buf);

  status = HAL_SAI_Transmit_DMA(&hsai_BlockA2, (uint8_t*)audio_buffer_2a, AUDIO_BUFFER_SIZE * 2);
  sprintf(uart_buf, "SAI2_A DMA: %s\r\n", status == HAL_OK ? "OK" : "FAIL");
  UART_Print(uart_buf);

  // === STEP 2: AK5538VN — configure SAI3_B and release power-down ===
  UART_Print("AK5538VN: starting SAI3_B RX DMA...\r\n");
  memset(audio_buffer_3b_rx, 0, sizeof(audio_buffer_3b_rx));
  __DSB();
  status = HAL_SAI_Receive_DMA(&hsai_BlockB3, (uint8_t*)audio_buffer_3b_rx, ADC_BUFFER_SIZE * 2);
  sprintf(uart_buf, "SAI3_B RX DMA: %s\r\n", status == HAL_OK ? "OK" : "FAIL");
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
    v = 0x01;  // RSTN=1 — release (stable MCLK/BICK/LRCK already running)
    HAL_I2C_Mem_Write(&hi2c3, AK5538_ADDR, 0x01, I2C_MEMADD_SIZE_8BIT, &v, 1, 50);
    HAL_Delay(5);

    // Readback verify R01-R03
    uint8_t r1 = 0xEE, r2 = 0xEE, r3 = 0xEE;
    HAL_I2C_Mem_Read(&hi2c3, AK5538_ADDR, 0x01, I2C_MEMADD_SIZE_8BIT, &r1, 1, 50);
    HAL_I2C_Mem_Read(&hi2c3, AK5538_ADDR, 0x02, I2C_MEMADD_SIZE_8BIT, &r2, 1, 50);
    HAL_I2C_Mem_Read(&hi2c3, AK5538_ADDR, 0x03, I2C_MEMADD_SIZE_8BIT, &r3, 1, 50);
    sprintf(uart_buf, "  readback R01=0x%02X R02=0x%02X R03=0x%02X (want R02=0x31 R03=0x40)\r\n",
            r1, r2, r3);
    UART_Print(uart_buf);
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

  // === STEP 5: Enable amplifier bank ===
  HAL_GPIO_WritePin(PAM_SD_PORT, PAM_SD_PIN, GPIO_PIN_SET);
  UART_Print("PAM8302A: SD HIGH — 32 amplifiers enabled\r\n");

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
    if (need_fill_first_half) {
      need_fill_first_half = 0;
      __DSB();
      uint32_t t0 = DWT->CYCCNT;
      Fill_Audio_Beamform(&audio_buffer[0], AUDIO_BUFFER_SAMPLES, 0);
      __DSB();
      uint32_t dt = DWT->CYCCNT - t0;
      if (dt < fill_min_cycles) fill_min_cycles = dt;
      if (dt > fill_max_cycles) fill_max_cycles = dt;
      fill_sum_cycles += dt;
      fill_sample_count++;
    }
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
      fill_sample_count++;
    }

    // SAI1_B: channels 8-15 (DAC1 U3)
    if (need_fill_1b_first_half) {
      need_fill_1b_first_half = 0;
      __DSB();
      Fill_Audio_Beamform(&audio_buffer_1b[0], AUDIO_BUFFER_SAMPLES, 8);
      __DSB();
    }
    if (need_fill_1b_second_half) {
      need_fill_1b_second_half = 0;
      __DSB();
      Fill_Audio_Beamform(&audio_buffer_1b[AUDIO_BUFFER_SIZE], AUDIO_BUFFER_SAMPLES, 8);
      __DSB();
    }

    // SAI2_A: channels 16-23 (DAC2 U1)
    if (need_fill_2a_first_half) {
      need_fill_2a_first_half = 0;
      __DSB();
      Fill_Audio_Beamform(&audio_buffer_2a[0], AUDIO_BUFFER_SAMPLES, 16);
      __DSB();
    }
    if (need_fill_2a_second_half) {
      need_fill_2a_second_half = 0;
      __DSB();
      Fill_Audio_Beamform(&audio_buffer_2a[AUDIO_BUFFER_SIZE], AUDIO_BUFFER_SAMPLES, 16);
      __DSB();
    }

    // SAI2_B: channels 24-31 (DAC3 U4)
    if (need_fill_2b_first_half) {
      need_fill_2b_first_half = 0;
      __DSB();
      Fill_Audio_Beamform(&audio_buffer_2b[0], AUDIO_BUFFER_SAMPLES, 24);
      __DSB();
    }
    if (need_fill_2b_second_half) {
      need_fill_2b_second_half = 0;
      __DSB();
      Fill_Audio_Beamform(&audio_buffer_2b[AUDIO_BUFFER_SIZE], AUDIO_BUFFER_SAMPLES, 24);
      __DSB();
    }

    // ===== AUTOFOCUS scan driver (chunked correlation, never blocks fills) =====
    if (autofocus_enabled) Cal_Process();

    // ===== K2 button (PA4, external pull-up, press = LOW): cycle op state =====
    {
      static uint8_t k2_prev = 1;
      static uint32_t k2_change_dwt = 0;
      uint8_t k2_now = (HAL_GPIO_ReadPin(OPT_SW_K2_PORT, OPT_SW_K2_PIN) == GPIO_PIN_SET);
      if (k2_now != k2_prev && (DWT->CYCCNT - k2_change_dwt) > (30UL * 480000UL)) {
        k2_prev = k2_now;                          // 30ms debounce @480MHz
        k2_change_dwt = DWT->CYCCNT;
        if (k2_now == 0) {                         // falling edge = press
          UI_ApplyState((ui_state_idx + 1) % UI_NUM_STATES);
        }
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

      if (UART_Protocol_StatusRequested()) {
        ds_status_t st;
        st.proto_ver = PROTO_VERSION;
        st.mode_idx = ui_state_idx;
        st.num_modes = UI_NUM_STATES;
        st.rx_az_deg = beam_az_deg;
        st.fx_mm = tx_focus_x_mm;
        st.fy_mm = tx_focus_y_mm;
        st.fz_mm = tx_focus_z_mm;
        st.underruns = underrun_count;
        st.sai_errors = error_count;
        st.ovf = (uint8_t)HAL_GPIO_ReadPin(AK_OVF_PORT, AK_OVF_PIN);
        st.cal_state = (uint8_t)cal_state;
        st.master_vol_db = beam_config.master_vol_db;
        for (uint32_t ch = 0; ch < ADC_CHANNELS; ch++) {
          int32_t pk = 0;
          for (uint32_t f = 0; f < ADC_BUFFER_SAMPLES; f++) {
            int32_t v = audio_buffer_3b_rx[f * ADC_CHANNELS + ch] >> 8;
            if (v < 0) v = -v;
            if (v > pk) pk = v;
          }
          st.adc_pk[ch] = pk;
        }
        UART_Protocol_SendStatus(&st);
      }
    }

    // ===== Check for new ESP32 config → apply to beam_config =====
    if (UART_Protocol_HasNewConfig()) {
      const ds_config_t *cfg = UART_Protocol_GetConfig();
      beam_config.master_gain = cfg->master_gain;
      beam_config.mute_global = cfg->mute_global;
      // Map 16 ESP32 channels into first 16 beam channels
      for (uint32_t i = 0; i < CFG_NUM_CHANNELS; i++) {
        beam_config.delays[i] = cfg->delays[i];
        beam_config.gains[i] = cfg->gains[i];
      }
      beam_config.mutes = (uint32_t)cfg->mutes;  // lower 16 bits
      // In RX beam mode, delays[0..7] act as RAW per-mic RX delays —
      // experimentation path that overrides the computed steering table.
      if (beam_enabled) {
        for (uint32_t m = 0; m < MIC_COUNT; m++) {
          uint16_t d = cfg->delays[m];
          rx_delay[m] = (d > MAX_DELAY_SAMPLES) ? MAX_DELAY_SAMPLES : d;
        }
        UART_Print("BEAM: raw RX delays applied from ESP32 config\r\n");
      }
      UART_Protocol_PrintConfig();
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
      const char *esp_ip = UART_Protocol_GetEspIp();
      snprintf(uart_buf, sizeof(uart_buf),
              "[%lus] Half: %lu/s, Full: %lu/s, Err: %lu, Undr: %lu, RX: %lu bytes | ESP: %s\r\n",
              HAL_GetTick() / 1000, half_per_sec, full_per_sec,
              error_count, underrun_count, UART_Protocol_GetRxByteCount(),
              esp_ip[0] ? esp_ip : "--");
      UART_Print(uart_buf);

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
        int32_t pk[ADC_CHANNELS] = {0};
        for (uint32_t f = 0; f < ADC_BUFFER_SAMPLES; f++) {
          for (uint32_t ch = 0; ch < ADC_CHANNELS; ch++) {
            int32_t v = audio_buffer_3b_rx[f * ADC_CHANNELS + ch] >> 8; // 24-bit
            int32_t a = v < 0 ? -v : v;
            if (a > pk[ch]) pk[ch] = a;
          }
        }
        // RX DMA liveness (~100/s) + live OVF (PA15): 1 = analog over-range.
        static uint32_t last_rx3b = 0;
        uint32_t rx_now = cb_rx3b_count;
        uint32_t rx_per_sec = rx_now - last_rx3b;
        last_rx3b = rx_now;
        int ovf = HAL_GPIO_ReadPin(AK_OVF_PORT, AK_OVF_PIN);

        sprintf(uart_buf,
                "     ADCpk: c0=%ld c1=%ld c2=%ld c3=%ld c4=%ld c5=%ld c6=%ld c7=%ld | OVF=%d RXdma=%lu/s\r\n",
                (long)pk[0], (long)pk[1], (long)pk[2], (long)pk[3],
                (long)pk[4], (long)pk[5], (long)pk[6], (long)pk[7], ovf, rx_per_sec);
        UART_Print(uart_buf);

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
          sprintf(uart_buf, "     Temp: %ld.%ld C\r\n", temp / 10, t_abs % 10);
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

        // Reset counters + flags + latency stats for next cycle
        // Atomic reset: disable IRQ so ISR can't write between our resets
        __disable_irq();
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
  HAL_UART_Transmit(&huart7, (uint8_t*)str, strlen(str), 100);
}

/**
 * @brief Print DMA configuration details for verification
 */
static void Print_DMA_Config(void) {
  char buf[128];

  UART_Print("\r\n--- DMA1 Stream 0 Configuration ---\r\n");

  // Get DMA1_Stream0 CR register
  uint32_t cr = DMA1_Stream0->CR;

  // Direction: Bits 7:6 (00=P2M, 01=M2P, 10=M2M)
  uint32_t dir = (cr >> 6) & 0x03;
  sprintf(buf, "Direction: %s (0x%02lX)\r\n",
          dir == 0x01 ? "Memory-to-Peripheral [OK]" :
          dir == 0x00 ? "Peripheral-to-Memory" : "Memory-to-Memory",
          dir);
  UART_Print(buf);

  // Circular mode: Bit 8
  uint32_t circ = (cr >> 8) & 0x01;
  sprintf(buf, "Circular Mode: %s\r\n", circ ? "ENABLED [OK]" : "DISABLED");
  UART_Print(buf);

  // Memory increment: Bit 10
  uint32_t minc = (cr >> 10) & 0x01;
  sprintf(buf, "Memory Increment: %s\r\n", minc ? "ENABLED [OK]" : "DISABLED");
  UART_Print(buf);

  // Peripheral increment: Bit 9
  uint32_t pinc = (cr >> 9) & 0x01;
  sprintf(buf, "Periph Increment: %s\r\n", pinc ? "ENABLED" : "DISABLED [OK]");
  UART_Print(buf);

  // Memory data size: Bits 14:13 (00=Byte, 01=HalfWord, 10=Word)
  uint32_t msize = (cr >> 13) & 0x03;
  sprintf(buf, "Memory Data Size: %s (0x%02lX)\r\n",
          msize == 0x02 ? "32-bit Word [OK]" :
          msize == 0x01 ? "16-bit HalfWord" : "8-bit Byte",
          msize);
  UART_Print(buf);

  // Peripheral data size: Bits 12:11
  uint32_t psize = (cr >> 11) & 0x03;
  sprintf(buf, "Periph Data Size: %s (0x%02lX)\r\n",
          psize == 0x02 ? "32-bit Word [OK]" :
          psize == 0x01 ? "16-bit HalfWord" : "8-bit Byte",
          psize);
  UART_Print(buf);

  // Priority: Bits 17:16 (00=Low, 01=Medium, 10=High, 11=VeryHigh)
  uint32_t pl = (cr >> 16) & 0x03;
  const char* prio_str[] = {"Low", "Medium", "High", "Very High"};
  sprintf(buf, "Priority: %s\r\n", prio_str[pl]);
  UART_Print(buf);

  // Interrupts: TCIE (bit 4), HTIE (bit 3), TEIE (bit 2), DMEIE (bit 1)
  uint32_t tcie = (cr >> 4) & 0x01;
  uint32_t htie = (cr >> 3) & 0x01;
  sprintf(buf, "Interrupts: TCIE=%s, HTIE=%s\r\n",
          tcie ? "ON [OK]" : "OFF", htie ? "ON [OK]" : "OFF");
  UART_Print(buf);

  // DMAMUX Configuration
  UART_Print("\r\n--- DMAMUX1 Channel 0 ---\r\n");
  uint32_t dmamux_ccr = DMAMUX1_Channel0->CCR;
  uint32_t req_id = dmamux_ccr & 0xFF;  // Request ID in bits 7:0
  sprintf(buf, "Request ID: %lu (expected 87 for SAI1_A) %s\r\n",
          req_id, req_id == 87 ? "[OK]" : "[MISMATCH!]");
  UART_Print(buf);

  // Buffer address verification
  UART_Print("\r\n--- Memory Addresses ---\r\n");
  uint32_t buf_addr = (uint32_t)audio_buffer;
  sprintf(buf, "Audio Buffer: 0x%08lX ", buf_addr);
  UART_Print(buf);

  // Check if in RAM_D2 (0x30000000 - 0x30047FFF)
  if (buf_addr >= 0x30000000 && buf_addr < 0x30048000) {
    UART_Print("(RAM_D2) [OK - DMA accessible]\r\n");
  } else if (buf_addr >= 0x20000000 && buf_addr < 0x20020000) {
    UART_Print("(DTCMRAM) [ERROR - NOT DMA accessible!]\r\n");
  } else if (buf_addr >= 0x24000000 && buf_addr < 0x24080000) {
    UART_Print("(AXI SRAM) [WARNING - Use MDMA only]\r\n");
  } else {
    UART_Print("(Unknown region)\r\n");
  }

  // DMA peripheral address (SAI1 data register)
  uint32_t par = DMA1_Stream0->PAR;
  sprintf(buf, "SAI1_A DR: 0x%08lX\r\n", par);
  UART_Print(buf);

  // DMA memory address
  uint32_t m0ar = DMA1_Stream0->M0AR;
  sprintf(buf, "DMA M0AR: 0x%08lX\r\n", m0ar);
  UART_Print(buf);

  // Number of data items
  uint32_t ndtr = DMA1_Stream0->NDTR;
  sprintf(buf, "NDTR (items): %lu\r\n", ndtr);
  UART_Print(buf);

  // Stream enabled?
  uint32_t en = cr & 0x01;
  sprintf(buf, "Stream Enabled: %s\r\n", en ? "YES" : "NO");
  UART_Print(buf);

  UART_Print("------------------------------------\r\n\r\n");
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
static void Beam_UpdateTxFocus(int16_t fx_mm, int16_t fy_mm, int16_t fz_mm) {
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
  for (uint32_t d = 0; d < 4; d++)
    for (uint8_t ch = 1; ch <= 8; ch++)
      PCM1690_SetAttenuation(&hi2c1, dac_addrs[d], ch, atten);
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
  walker_enabled = 0;
  adc_monitor_enabled = 0;
  autofocus_enabled = 0;
  cal_state = CAL_IDLE;
  cal_capturing = 0;
  if (!beam_enabled) memset(beam_line, 0, sizeof(beam_line));
  beam_enabled = 1;
  if (req->type == 1) {
    /* El tono interno se PRESERVA a propósito: steerear el foco del tono
     * (modos 12/13 + POST /beam type=tx) es el protocolo de medición
     * mic-fijo/foco-móvil (docs/ds10_next_steps.md §A). */
    tx_steer_enabled = 1;
    Beam_UpdateDelays(0);                 // RX source: broadside
    Beam_UpdateTxFocus(req->fx_mm, req->fy_mm, req->fz_mm);
    UART_Print("MODE: TX focus custom (via ESP32)\r\n");
  } else {
    /* RX custom escucha los mics: el tono interno pisaría beam_line, y el
     * shift de tono (0) dejaría la salida 30dB abajo. */
    tx_steer_enabled = 0;
    tone_enabled = 0;
    beam_out_shift = 5;
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
  cal_settle = CAL_SETTLE_HALVES;
  cal_state = CAL_SETTLE;
  UART_Print("AUTOFOCUS: mic at the focal point -> ADC input 1 (c0). Scanning...\r\n");
}

/** Cross-correlation of the capture at one lag against the chirp reference. */
static int64_t Cal_CorrAt(uint32_t lag) {
  int64_t acc = 0;
  const int32_t *cap = &cal_capture[lag];
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

  cal_tof_q8[cal_ch] = (cal_best_lag << 8) + (int32_t)(frac * 256.0f);
  cal_meas_sign[cal_ch] = cal_best_sign;
  cal_quality[cal_ch] = (uint16_t)q10;

  int32_t tq = cal_tof_q8[cal_ch];
  sprintf(uart_buf, "  CAL ch%u: tof=%ld.%02ld smp sign=%c q=%lu%s\r\n",
          (unsigned)cal_ch, (long)(tq >> 8), (long)((tq & 0xFF) * 100 / 256),
          (cal_best_sign < 0) ? '-' : '+', (unsigned long)q10,
          (q10 < 50) ? "  (LOW — mic signal?)" : "");
  UART_Print(uart_buf);

  cal_ch++;
  if (cal_ch < CAL_NUM_CH) {                 // next speaker
    cal_settle = CAL_SETTLE_HALVES;
    cal_state = CAL_SETTLE;
    return;
  }

  // All speakers measured: focal delays = max(tof) − tof_k, signs applied
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
 * @brief Switch operating state (K2 button): sets exactly one mode flag.
 */
static void UI_ApplyState(uint8_t idx) {
  const ui_state_t *st = &ui_states[idx];
  ui_state_idx = idx;
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
  /* Tone source is -6dBFS: the +30dB boost that lifts the weak ADC beam
   * would clip ×32 into the PAMs (plan §3.1). */
  beam_out_shift = st->tone ? 0 : 5;
  if (!st->af) {                     // leaving AUTOFOCUS aborts any scan
    cal_state = CAL_IDLE;
    cal_capturing = 0;
  }
  if (st->beam) Beam_UpdateDelays(st->az_deg);
  if (st->tx) Beam_UpdateTxFocus(st->fx_mm, st->fy_mm, st->fz_mm);
  if (st->walker) walker_frame = 0;
  /* Persist the mode, debounced: a real change arms the deadline; the boot
   * restore re-applies the same idx so it doesn't (idx == last_mode). */
  if (idx != beam_config.last_mode) {
    beam_config.last_mode = idx;
    Arm_Config_Save();
  }
  sprintf(uart_buf, "MODE [%u/%u]: %s\r\n", (unsigned)(idx + 1),
          (unsigned)UI_NUM_STATES, st->name);
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
  for (uint32_t i = 0; i < samples; i++) {
    for (uint32_t ch = 0; ch < ADC_CHANNELS; ch++) {
      delay_line[ch][(wp + i) & DELAY_LINE_MASK] = rx_buf[i * ADC_CHANNELS + ch];
    }
  }

  /* AUTOFOCUS capture: append the measurement mic (24-bit) until the window
   * is full. Started in the same loop pass as the chirp emission, so the
   * emission→capture offset is constant for every speaker and cancels in
   * the relative TOFs. */
  if (cal_capturing) {
    for (uint32_t i = 0; i < samples && cal_cap_count < CAL_CAPTURE_LEN; i++) {
      cal_capture[cal_cap_count++] = rx_buf[i * ADC_CHANNELS + CAL_MIC_CH] >> 8;
    }
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

  delay_write_pos = (wp + samples) & DELAY_LINE_MASK;
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

  /* AUTOFOCUS mode: during the scan, emit the chirp on one speaker at a time
   * (everything else silent); once done, play a 1kHz tone steered with the
   * MEASURED delays & polarities — audibly focused at the mic position. */
  if (autofocus_enabled) {
    // SETTLE countdown ticks on the always-firing SAI1_A fills
    if (ch_offset == 0 && cal_state == CAL_SETTLE && cal_settle > 0) {
      if (--cal_settle == 0) cal_state = CAL_ARM;
    }

    if (cal_state == CAL_ARM && ch_offset == (cal_ch & ~7u)) {
      // Emit the 480-sample chirp on cal_ch (exactly one half-buffer) and
      // start the mic capture in this same pass — constant offset per ch.
      uint32_t local = cal_ch & 7u;
      for (uint32_t i = 0; i < samples; i++) {
        for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
          buffer[i * AUDIO_CHANNELS + ch] = (ch == local) ? cal_chirp[i] : 0;
        }
      }
      cal_cap_count = 0;
      cal_capturing = 1;
      cal_state = CAL_CAPT;
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
            s <<= beam_out_shift;
            if (s > 8388607) s = 8388607;
            else if (s < -8388608) s = -8388608;
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
      s <<= beam_out_shift;
      if (s > 8388607) s = 8388607;
      else if (s < -8388608) s = -8388608;
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
        uint32_t src = ch_offset + ch;
        if (src > 7) src = 0;                       // only ADC ch0-7 exist
        int32_t s = delay_line[src][read_pos] >> 8; // 32-bit ADC → 24-bit
        s <<= adc_monitor_shift;                    // gain boost
        if (s > 8388607) s = 8388607;               // clamp to +FS
        else if (s < -8388608) s = -8388608;        // clamp to -FS
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
