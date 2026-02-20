/**
 * @file main_i2c_scan.c
 * @brief DS9 Board Sanity Test — I2C scan + UART output
 *
 * Tests:
 *  1. UART7 debug output (PA8 RX, PA15 TX, 115200)
 *  2. 3.3V analog rail enable (PA10)
 *  3. I2C1 scan (PB8 SCL, PB9 SDA) — reports every responding device
 *
 * Expected devices on DS9:
 *  0x4A = PCM1690 DAC0 (U3) — ch 0-7   [may be BROKEN in DS9 — SAI pin error]
 *  0x4B = PCM1690 DAC1 (U4) — ch 8-15  [SAI1_A works]
 *  0x4C = PCM1690 DAC2 (U2) — ch 16-23 [SAI2_A works]
 *  0x4D = PCM1690 DAC3 (U8) — ch 24-31 [needs bodge]
 *  0x50 = CAT24C256 EEPROM (U6)
 *
 * Build:  make i2c-scan
 * Flash:  make i2c-flash
 * Monitor: minicom -D /dev/tty.usbserial-XXXX -b 115200  (USB-UART on PA0 TX, PA1 RX)
 *
 * LED behavior during scan:
 *  D6 (PC4) = ON during scan (heartbeat)
 *  D7 (PC5) = blinks once per I2C device found
 *  D11 (PB0) = all-on burst at end if any PCM1690 found
 *  D10 (PB1) = all-on burst at end if EEPROM found
 */

#include "stm32h7xx_hal.h"
#include <string.h>

/* ── Peripherals ────────────────────────────────────────────── */
static UART_HandleTypeDef huart4;
static I2C_HandleTypeDef  hi2c1;
static I2C_HandleTypeDef  hi2c4;  /* EEPROM on PB6(SCL)/PB7(SDA) AF6 */

/* ── Known I2C devices ──────────────────────────────────────── */
typedef struct { uint8_t addr7; const char *name; } I2C_KnownDevice;

/* I2C1 (PB8/PB9): PCM1690 DACs
 * base=0x4C, ADR1=pin23, ADR0=pin22
 * U3(DAC0): ADR1=0,ADR0=0→0x4C  U4(DAC1): ADR1=1,ADR0=0→0x4E
 * U2(DAC2): ADR1=0,ADR0=1→0x4D  U8(DAC3): ADR1=1,ADR0=1→0x4F */
static const I2C_KnownDevice known_i2c1[] = {
    { 0x4C, "PCM1690 DAC0 (U3) ch0-7  [SAI broken in DS9]" },
    { 0x4E, "PCM1690 DAC1 (U4) ch8-15 [SAI1_A OK]" },
    { 0x4D, "PCM1690 DAC2 (U2) ch16-23[SAI2_A OK]" },
    { 0x4F, "PCM1690 DAC3 (U8) ch24-31[needs bodge]" },
};
#define KNOWN_I2C1_COUNT (sizeof(known_i2c1) / sizeof(known_i2c1[0]))

/* I2C4 (PB6/PB7): EEPROM
 * CAT24C256 32KB EEPROM, all address pins to GND → 0x50 */
static const I2C_KnownDevice known_i2c4[] = {
    { 0x50, "CAT24C256 EEPROM (U6) 32KB" },
};
#define KNOWN_I2C4_COUNT (sizeof(known_i2c4) / sizeof(known_i2c4[0]))

/* ── LED pins ───────────────────────────────────────────────── */
#define LED_D6_PIN   GPIO_PIN_4   /* PC4 */
#define LED_D7_PIN   GPIO_PIN_5   /* PC5 */
#define LED_D11_PIN  GPIO_PIN_0   /* PB0 */
#define LED_D10_PIN  GPIO_PIN_1   /* PB1 */

/* ── DAC RST pins (DS9 netlist, active LOW) ─────────────────── */
/* No pull-ups on board → chips stay in reset unless explicitly released */
#define DAC0_RST_PIN   GPIO_PIN_10  /* PE10 — pad 40 */
#define DAC0_RST_PORT  GPIOE
#define DAC1_RST_PIN   GPIO_PIN_3   /* PE3  — pad 2  */
#define DAC1_RST_PORT  GPIOE
#define DAC2_RST_PIN   GPIO_PIN_10  /* PC10 — pad 78 */
#define DAC2_RST_PORT  GPIOC
#define DAC3_RST_PIN   GPIO_PIN_15  /* PB15 — pad 54 */
#define DAC3_RST_PORT  GPIOB

/* ── DAC AMUTEI pins (INPUT to PCM1690, active LOW = muted) ─── */
/* Drive HIGH to allow audio output. Floating → undefined mute state. */
#define DAC0_AMUTEI_PIN   GPIO_PIN_11  /* PE11 */
#define DAC0_AMUTEI_PORT  GPIOE
#define DAC1_AMUTEI_PIN   GPIO_PIN_1   /* PE1  */
#define DAC1_AMUTEI_PORT  GPIOE
#define DAC2_AMUTEI_PIN   GPIO_PIN_11  /* PC11 */
#define DAC2_AMUTEI_PORT  GPIOC
#define DAC3_AMUTEI_PIN   GPIO_PIN_14  /* PB14 */
#define DAC3_AMUTEI_PORT  GPIOB

/* ── DAC AMUTEO pins (OUTPUT from PCM1690, LOW = chip auto-muted) */
/* Configure as inputs on STM32 to read chip auto-mute status.      */
#define DAC0_AMUTEO_PIN   GPIO_PIN_12  /* PE12 */
#define DAC0_AMUTEO_PORT  GPIOE
#define DAC1_AMUTEO_PIN   GPIO_PIN_0   /* PE0  */
#define DAC1_AMUTEO_PORT  GPIOE
#define DAC2_AMUTEO_PIN   GPIO_PIN_12  /* PC12 */
#define DAC2_AMUTEO_PORT  GPIOC
#define DAC3_AMUTEO_PIN   GPIO_PIN_13  /* PB13 */
#define DAC3_AMUTEO_PORT  GPIOB

/* ── Forward declarations ───────────────────────────────────── */
static void SystemClock_Config(void);
static void Periph_Init(void);
static void uart_print(const char *s);
static void uart_println(const char *s);
static void uart_print_hex(uint8_t v);
static void uart_print_dec(uint32_t v);
static const char *lookup_i2c1(uint8_t addr7);

/* ═══════════════════════════════════════════════════════════════
 * MAIN
 * ═══════════════════════════════════════════════════════════════ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    Periph_Init();

    /* ── Banner ── */
    uart_println("\r\n\r\n========================================");
    uart_println("  DS9 Board Test — I2C Scan v2");
    uart_println("  Clock: HSI 64MHz");
    uart_println("  I2C1 (PB8/PB9): PCM1690 DACs");
    uart_println("  I2C4 (PB6/PB7): EEPROM CAT24C256");
    uart_println("========================================");
    uart_println("  3.3V analog rail: ENABLED (PA10)");
    uart_println("  DAC RST: asserting LOW...");

    HAL_Delay(10);  /* hold reset */

    /* ── Release DAC resets (HIGH = normal operation) ── */
    HAL_GPIO_WritePin(DAC0_RST_PORT, DAC0_RST_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DAC1_RST_PORT, DAC1_RST_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DAC2_RST_PORT, DAC2_RST_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DAC3_RST_PORT, DAC3_RST_PIN, GPIO_PIN_SET);

    uart_println("  DAC RST: released HIGH (PE10,PE3,PC10,PB15)");
    uart_println("  DAC AMUTEI: HIGH (unmuted) on PE11,PE1,PC11,PB14");

    /* ── Generate MCLK on PE2 for PCM1690 initialization ── */
    /* PCM1690 needs 3846 SCKI cycles after RST release to complete POR.
     * Without MCLK, the chip stays in post-reset and I2C never enables!
     * Bit-bang ~8000 cycles on PE2 (SCKI) via 1k series resistor to chip. */
    uart_println("  Generating MCLK on PE2 (~8000 cycles)...");
    {
        GPIO_InitTypeDef gm = {0};
        gm.Pin   = GPIO_PIN_2;
        gm.Mode  = GPIO_MODE_AF_PP;  /* will switch to output below */
        gm.Pull  = GPIO_NOPULL;
        gm.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gm.Mode  = GPIO_MODE_OUTPUT_PP;
        HAL_GPIO_Init(GPIOE, &gm);

        __disable_irq();
        for (uint32_t i = 0; i < 8000; i++) {
            GPIOE->BSRR = GPIO_PIN_2;        /* HIGH */
            __NOP(); __NOP(); __NOP();
            GPIOE->BSRR = GPIO_PIN_2 << 16;  /* LOW */
            __NOP(); __NOP(); __NOP();
        }
        __enable_irq();
    }
    uart_println("  MCLK done. Waiting 10ms...");
    HAL_Delay(10);

    /* ── AMUTEO probe: tests if +3V3 reaches PCM1690 area ── */
    /* Each AMUTEO has 1k external pull-up to +3V3 (R4/R8/R12/R38).
     * STM32 has internal ~40kΩ pull-DOWN enabled.
     *   H = 1k pull-up wins → +3V3 present at chip → wiring OK
     *   L = no pull-up → +3V3 NOT reaching chip → open trace! */
    uart_print("  AMUTEO +3V3 probe (H=rail OK, L=open trace): ");
    uart_print("DAC0="); uart_print((HAL_GPIO_ReadPin(DAC0_AMUTEO_PORT, DAC0_AMUTEO_PIN) == GPIO_PIN_SET) ? "H" : "L");
    uart_print(" DAC1="); uart_print((HAL_GPIO_ReadPin(DAC1_AMUTEO_PORT, DAC1_AMUTEO_PIN) == GPIO_PIN_SET) ? "H" : "L");
    uart_print(" DAC2="); uart_print((HAL_GPIO_ReadPin(DAC2_AMUTEO_PORT, DAC2_AMUTEO_PIN) == GPIO_PIN_SET) ? "H" : "L");
    uart_print(" DAC3="); uart_println((HAL_GPIO_ReadPin(DAC3_AMUTEO_PORT, DAC3_AMUTEO_PIN) == GPIO_PIN_SET) ? "H" : "L");

    uart_println("\r\n  [I2C1] Scanning PCM1690 DACs (0x01..0x7F) ...");

    HAL_Delay(10);

    uint32_t scan_num = 0;

    while (1)
    {
        scan_num++;
        uart_print("\r\n[Scan #");
        uart_print_dec(scan_num);
        uart_println("]");

        /* D6 on = scanning */
        HAL_GPIO_WritePin(GPIOC, LED_D6_PIN, GPIO_PIN_SET);

        /* ── I2C1: scan all PCM1690 addresses ── */
        uart_println("  [I2C1 PB8/PB9] PCM1690 DACs:");
        uint8_t pcm_found = 0;
        for (uint8_t addr = 0x01; addr < 0x78; addr++)
        {
            HAL_StatusTypeDef res = HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 2, 20);
            if (res == HAL_OK)
            {
                uart_print("    [FOUND] 0x");
                uart_print_hex(addr);
                uart_print("  ");
                const char *name = lookup_i2c1(addr);
                uart_println(name ? name : "(unknown)");
                if (addr >= 0x4C && addr <= 0x4F) pcm_found++;
                HAL_GPIO_WritePin(GPIOC, LED_D7_PIN, GPIO_PIN_SET);
                HAL_Delay(50);
                HAL_GPIO_WritePin(GPIOC, LED_D7_PIN, GPIO_PIN_RESET);
                HAL_Delay(50);
            }
        }
        if (pcm_found == 0) uart_println("    (none)");
        uint32_t err1 = HAL_I2C_GetError(&hi2c1);
        if (err1) {
            uart_print("    I2C1 err=0x");
            uart_print_hex((uint8_t)(err1 >> 8)); uart_print_hex((uint8_t)err1);
            uart_print(" state=0x"); uart_print_hex((uint8_t)HAL_I2C_GetState(&hi2c1));
            uart_println("");
        }

        /* ── I2C4: scan EEPROM ── */
        uart_println("  [I2C4 PB6/PB7] EEPROM:");
        uint8_t eeprom_found = 0;
        for (uint8_t i = 0; i < KNOWN_I2C4_COUNT; i++)
        {
            uint8_t addr = known_i2c4[i].addr7;
            HAL_StatusTypeDef res = HAL_I2C_IsDeviceReady(&hi2c4, (uint16_t)(addr << 1), 2, 20);
            if (res == HAL_OK)
            {
                uart_print("    [FOUND] 0x");
                uart_print_hex(addr);
                uart_print("  ");
                uart_println(known_i2c4[i].name);
                eeprom_found = 1;
            } else {
                uart_print("    [MISS]  0x");
                uart_print_hex(addr);
                uart_print("  ");
                uart_println(known_i2c4[i].name);
            }
        }
        uint32_t err4 = HAL_I2C_GetError(&hi2c4);
        if (err4) {
            uart_print("    I2C4 err=0x");
            uart_print_hex((uint8_t)(err4 >> 8)); uart_print_hex((uint8_t)err4);
            uart_print(" state=0x"); uart_print_hex((uint8_t)HAL_I2C_GetState(&hi2c4));
            uart_println("");
        }

        /* ── Summary ── */
        uart_println("  ----------------------------------------");
        uart_print("  PCM1690 DACs: "); uart_print_dec(pcm_found); uart_println("/4");
        uart_print("  EEPROM:       "); uart_println(eeprom_found ? "OK" : "MISS");
        if (pcm_found == 4 && eeprom_found)
            uart_println("  [OK] All devices found!");
        else if (eeprom_found && pcm_found == 0)
            uart_println("  [PARTIAL] EEPROM ok, PCM1690 fail -> chip-specific issue");
        else if (!eeprom_found && pcm_found == 0)
            uart_println("  [FAIL] Nothing found — check power & I2C pull-ups");
        uart_println("  ----------------------------------------");

        /* ── Extra diagnostics when PCM1690 not found ── */
        if (pcm_found == 0)
        {
            /* 1. RST readback — confirm output pins are actually HIGH */
            uart_print("  [DIAG] RST readback: ");
            uart_print("D0="); uart_print((HAL_GPIO_ReadPin(DAC0_RST_PORT, DAC0_RST_PIN)==GPIO_PIN_SET)?"H":"L");
            uart_print(" D1="); uart_print((HAL_GPIO_ReadPin(DAC1_RST_PORT, DAC1_RST_PIN)==GPIO_PIN_SET)?"H":"L");
            uart_print(" D2="); uart_print((HAL_GPIO_ReadPin(DAC2_RST_PORT, DAC2_RST_PIN)==GPIO_PIN_SET)?"H":"L");
            uart_print(" D3="); uart_println((HAL_GPIO_ReadPin(DAC3_RST_PORT, DAC3_RST_PIN)==GPIO_PIN_SET)?"H":"L");
            uart_println("  (all should be H — if L, RST pin is shorted to GND)");

            /* 2. AMUTEI readback */
            uart_print("  [DIAG] AMUTEI readback: ");
            uart_print("D0="); uart_print((HAL_GPIO_ReadPin(DAC0_AMUTEI_PORT, DAC0_AMUTEI_PIN)==GPIO_PIN_SET)?"H":"L");
            uart_print(" D1="); uart_print((HAL_GPIO_ReadPin(DAC1_AMUTEI_PORT, DAC1_AMUTEI_PIN)==GPIO_PIN_SET)?"H":"L");
            uart_print(" D2="); uart_print((HAL_GPIO_ReadPin(DAC2_AMUTEI_PORT, DAC2_AMUTEI_PIN)==GPIO_PIN_SET)?"H":"L");
            uart_print(" D3="); uart_println((HAL_GPIO_ReadPin(DAC3_AMUTEI_PORT, DAC3_AMUTEI_PIN)==GPIO_PIN_SET)?"H":"L");

            /* 3. PA10 inverted test — maybe enable is active LOW */
            uart_println("  [DIAG] Trying PA10=LOW (testing inverted rail enable)...");
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
            HAL_Delay(150);  /* wait for rail to stabilize */
            uint8_t found_inv = 0;
            for (uint8_t a = 0x4C; a <= 0x4F; a++) {
                if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(a << 1), 2, 20) == HAL_OK)
                    found_inv++;
            }
            uart_print("    DACs found with PA10=LOW: ");
            uart_print_dec(found_inv);
            uart_println(found_inv ? " !! PA10 POLARITY WAS WRONG !!" : " (none)");
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);  /* restore */
            HAL_Delay(50);

            /* 4. Slow I2C test at ~10kHz */
            uart_println("  [DIAG] Trying I2C1 at ~10kHz...");
            hi2c1.Instance->CR1 &= ~I2C_CR1_PE;
            hi2c1.Instance->TIMINGR = 0xF04263C3;  /* ~10kHz @ HSI 64MHz */
            hi2c1.Instance->CR1 |= I2C_CR1_PE;
            uint8_t found_slow = 0;
            for (uint8_t a = 0x01; a < 0x78; a++) {
                if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(a << 1), 2, 100) == HAL_OK)
                    found_slow++;
            }
            uart_print("    Devices at 10kHz: "); uart_print_dec(found_slow);
            uart_println(found_slow ? " — found something!" : " (none)");
            hi2c1.Instance->CR1 &= ~I2C_CR1_PE;
            hi2c1.Instance->TIMINGR = 0x10427FC7;
            hi2c1.Instance->CR1 |= I2C_CR1_PE;
            hi2c1.ErrorCode = HAL_I2C_ERROR_NONE;

            /* 5. I2C4 NACK reference — healthy bus NACK = 0x0004, not 0x0020.
             *    If I2C1 = TIMEOUT and I2C4 = NACK, the I2C1 bus is being held. */
            uart_println("  [DIAG] I2C4 NACK reference (addr 0x55, no device)...");
            HAL_I2C_IsDeviceReady(&hi2c4, (uint16_t)(0x55 << 1), 1, 20);
            {
                uint32_t err4r = HAL_I2C_GetError(&hi2c4);
                uart_print("    I2C4 err=0x");
                uart_print_hex((uint8_t)(err4r >> 8)); uart_print_hex((uint8_t)err4r);
                if      (err4r == 0x0004) uart_println(" = NACK (normal — no device)");
                else if (err4r == 0x0020) uart_println(" = TIMEOUT (I2C4 bus blocked?)");
                else                     uart_println(" = other");
            }

            /* 6. Scan I2C1 with chips held in RESET.
             *    In reset, PCM1690 must release SDA/SCL.
             *    TIMEOUT→NACK = chips were holding the bus.
             *    TIMEOUT stays  = something else on I2C1 is holding SCL. */
            uart_println("  [DIAG] I2C1 scan with RST=LOW (chips in reset)...");
            HAL_GPIO_WritePin(DAC0_RST_PORT, DAC0_RST_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(DAC1_RST_PORT, DAC1_RST_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(DAC2_RST_PORT, DAC2_RST_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(DAC3_RST_PORT, DAC3_RST_PIN, GPIO_PIN_RESET);
            HAL_Delay(10);
            hi2c1.ErrorCode = HAL_I2C_ERROR_NONE;
            for (uint8_t a = 0x4C; a <= 0x4F; a++)
                HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(a << 1), 1, 20);
            {
                uint32_t err_r = HAL_I2C_GetError(&hi2c1);
                uart_print("    I2C1 err RST=LOW: 0x");
                uart_print_hex((uint8_t)(err_r >> 8)); uart_print_hex((uint8_t)err_r);
                if      (err_r == 0x0004) uart_println(" = NACK => chips WERE holding bus!");
                else if (err_r == 0x0020) uart_println(" = TIMEOUT => bus still held (not the chips)");
                else if (err_r == 0x0000) uart_println(" = no error (chip ACK'd in reset??)");
                else                     uart_println("");
            }
            HAL_GPIO_WritePin(DAC0_RST_PORT, DAC0_RST_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(DAC1_RST_PORT, DAC1_RST_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(DAC2_RST_PORT, DAC2_RST_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(DAC3_RST_PORT, DAC3_RST_PIN, GPIO_PIN_SET);
            hi2c1.ErrorCode = HAL_I2C_ERROR_NONE;

            /* 7. SCL/SDA bus level after all scans (read raw GPIOB IDR) */
            {
                uint8_t scl = (GPIOB->IDR & GPIO_PIN_8) ? 1 : 0;
                uint8_t sda = (GPIOB->IDR & GPIO_PIN_9) ? 1 : 0;
                uart_print("  [DIAG] I2C1 idle: SCL(PB8)="); uart_print(scl ? "H" : "L");
                uart_print("  SDA(PB9)="); uart_println(sda ? "H" : "L");
                if (!scl) uart_println("    !! SCL stuck LOW — a device is holding the clock !!");
                if (!sda) uart_println("    !! SDA stuck LOW — a device is holding data !!");
            }

            /* 8. SPI bit-bang test — detect if chips are in SPI mode.
             *    PCM1690 SPI: pin21=clock, pin20=bidir data, pin22=MS (chip select).
             *    U3/U4 have pin22 (ADR0) tied to GNDA → MS=LOW → always selected in SPI.
             *    If MODE is HIGH (floating), chips are in SPI mode and respond here.
             *    Read register 65 (audio format, default=0x00 after reset).
             *    0xFF = no response (pull-up) → chips NOT in SPI → MODE is correct (LOW).
             *    Other value → CHIP IS IN SPI MODE → MODE pin problem! */
            uart_println("  [DIAG] SPI bit-bang test (detect wrong MODE pin)...");
            {
                /* Disable I2C1 to free PB8/PB9 */
                hi2c1.Instance->CR1 &= ~I2C_CR1_PE;

                GPIO_InitTypeDef gs = {0};
                /* PB8 = SPI clock (output PP), PB9 = SPI data (output PP initially) */
                gs.Mode = GPIO_MODE_OUTPUT_PP;
                gs.Pull = GPIO_NOPULL;
                gs.Speed = GPIO_SPEED_FREQ_LOW;
                gs.Pin = GPIO_PIN_8 | GPIO_PIN_9;
                HAL_GPIO_Init(GPIOB, &gs);
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET); /* CLK idle LOW */

                /* Send read command: bit7=1(read) | bits6:0=0x41 (reg 65) → 0xC1 */
                uint8_t cmd = 0xC1;
                for (int b = 7; b >= 0; b--) {
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9,
                        ((cmd >> b) & 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);  /* CLK high */
                    for (volatile int d = 0; d < 20; d++);
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET); /* CLK low */
                    for (volatile int d = 0; d < 20; d++);
                }

                /* Switch PB9 to input with pull-up (reads 0xFF if nobody drives) */
                gs.Pin = GPIO_PIN_9;
                gs.Mode = GPIO_MODE_INPUT;
                gs.Pull = GPIO_PULLUP;
                HAL_GPIO_Init(GPIOB, &gs);

                /* Read 8 bits of data */
                uint8_t rx = 0;
                for (int b = 7; b >= 0; b--) {
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
                    for (volatile int d = 0; d < 20; d++);
                    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9) == GPIO_PIN_SET)
                        rx |= (1 << b);
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
                    for (volatile int d = 0; d < 20; d++);
                }

                uart_print("    SPI read reg65 = 0x"); uart_print_hex(rx);
                if (rx == 0xFF)
                    uart_println(" (no response — MODE is correct, chips in I2C mode)");
                else {
                    uart_println(" !! RESPONDED via SPI — MODE pin is HIGH !!");
                    uart_println("    → R7/R11/R15/R41 not pulling MODE low!");
                }

                /* Also try reading reg 66 (operation control) for confirmation */
                gs.Pin = GPIO_PIN_9;
                gs.Mode = GPIO_MODE_OUTPUT_PP;
                gs.Pull = GPIO_NOPULL;
                HAL_GPIO_Init(GPIOB, &gs);
                cmd = 0xC2; /* read reg 66 */
                for (int b = 7; b >= 0; b--) {
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9,
                        ((cmd >> b) & 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
                    for (volatile int d = 0; d < 20; d++);
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
                    for (volatile int d = 0; d < 20; d++);
                }
                gs.Pin = GPIO_PIN_9;
                gs.Mode = GPIO_MODE_INPUT;
                gs.Pull = GPIO_PULLUP;
                HAL_GPIO_Init(GPIOB, &gs);
                uint8_t rx2 = 0;
                for (int b = 7; b >= 0; b--) {
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
                    for (volatile int d = 0; d < 20; d++);
                    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9) == GPIO_PIN_SET)
                        rx2 |= (1 << b);
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
                    for (volatile int d = 0; d < 20; d++);
                }
                uart_print("    SPI read reg66 = 0x"); uart_print_hex(rx2);
                if (rx2 != 0xFF) uart_println(" (confirmed SPI response)");
                else             uart_println("");

                /* Restore PB8/PB9 as I2C1 AF4 OD */
                gs.Pin = GPIO_PIN_8 | GPIO_PIN_9;
                gs.Mode = GPIO_MODE_AF_OD;
                gs.Pull = GPIO_NOPULL;
                gs.Speed = GPIO_SPEED_FREQ_LOW;
                gs.Alternate = GPIO_AF4_I2C1;
                HAL_GPIO_Init(GPIOB, &gs);
                hi2c1.Instance->CR1 |= I2C_CR1_PE;
                hi2c1.ErrorCode = HAL_I2C_ERROR_NONE;
            }
        }

        HAL_GPIO_WritePin(GPIOC, LED_D6_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, LED_D11_PIN, pcm_found   ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, LED_D10_PIN, eeprom_found ? GPIO_PIN_SET : GPIO_PIN_RESET);

        uart_println("  Next scan in 5s...");
        HAL_Delay(5000);
    }
}

/* ═══════════════════════════════════════════════════════════════
 * CLOCK — HSI 64MHz (no crystal needed)
 * ═══════════════════════════════════════════════════════════════ */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY));

    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_DIV1;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&osc);

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                         RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    clk.SYSCLKDivider  = RCC_SYSCLK_DIV1;
    clk.AHBCLKDivider  = RCC_HCLK_DIV1;
    clk.APB3CLKDivider = RCC_APB3_DIV1;
    clk.APB1CLKDivider = RCC_APB1_DIV1;
    clk.APB2CLKDivider = RCC_APB2_DIV1;
    clk.APB4CLKDivider = RCC_APB4_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_1);
}

/* ═══════════════════════════════════════════════════════════════
 * PERIPHERAL INIT
 * ═══════════════════════════════════════════════════════════════ */
static void Periph_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* ── Enable GPIO clocks ── */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* ── 3.3V analog rail enable: PA10 HIGH ── */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
    gpio.Pin   = GPIO_PIN_10;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* ── DAC RST: assert LOW first (chips in reset) ── */
    /* RST is active LOW. No pull-ups on board → must drive explicitly. */
    __HAL_RCC_GPIOE_CLK_ENABLE();
    HAL_GPIO_WritePin(DAC0_RST_PORT, DAC0_RST_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DAC1_RST_PORT, DAC1_RST_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DAC2_RST_PORT, DAC2_RST_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DAC3_RST_PORT, DAC3_RST_PIN, GPIO_PIN_RESET);
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin   = DAC0_RST_PIN | DAC1_RST_PIN;
    HAL_GPIO_Init(DAC0_RST_PORT, &gpio);  /* PE10 + PE3, same GPIOE */
    gpio.Pin   = DAC2_RST_PIN;
    HAL_GPIO_Init(DAC2_RST_PORT, &gpio);  /* PC10, GPIOC */
    gpio.Pin   = DAC3_RST_PIN;
    HAL_GPIO_Init(DAC3_RST_PORT, &gpio);  /* PB15, GPIOB */

    /* ── AMUTEI: drive HIGH (unmuted) ── */
    /* AMUTEI active LOW: HIGH = normal, LOW = hardware mute.
     * Must be driven explicitly — no pull-ups on board. */
    HAL_GPIO_WritePin(DAC0_AMUTEI_PORT, DAC0_AMUTEI_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DAC1_AMUTEI_PORT, DAC1_AMUTEI_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DAC2_AMUTEI_PORT, DAC2_AMUTEI_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DAC3_AMUTEI_PORT, DAC3_AMUTEI_PIN, GPIO_PIN_SET);
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin   = DAC0_AMUTEI_PIN | DAC1_AMUTEI_PIN;  /* PE11 + PE1, GPIOE */
    HAL_GPIO_Init(DAC0_AMUTEI_PORT, &gpio);
    gpio.Pin   = DAC2_AMUTEI_PIN;                    /* PC11, GPIOC */
    HAL_GPIO_Init(DAC2_AMUTEI_PORT, &gpio);
    gpio.Pin   = DAC3_AMUTEI_PIN;                    /* PB14, GPIOB */
    HAL_GPIO_Init(DAC3_AMUTEI_PORT, &gpio);

    /* ── AMUTEO: configure as inputs with PULLDOWN ── */
    /* PCB has external 1k pull-up to +3V3 on each AMUTEO.
     * STM32 internal pull-down is ~40kΩ. So:
     *   AMUTEO=H → external 1k wins → +3V3 IS present at chip area
     *   AMUTEO=L → no external pull → +3V3 NOT reaching chip area */
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_PULLDOWN;
    gpio.Pin   = DAC0_AMUTEO_PIN | DAC1_AMUTEO_PIN;  /* PE12 + PE0, GPIOE */
    HAL_GPIO_Init(DAC0_AMUTEO_PORT, &gpio);
    gpio.Pin   = DAC2_AMUTEO_PIN;                    /* PC12, GPIOC */
    HAL_GPIO_Init(DAC2_AMUTEO_PORT, &gpio);
    gpio.Pin   = DAC3_AMUTEO_PIN;                    /* PB13, GPIOB */
    HAL_GPIO_Init(DAC3_AMUTEO_PORT, &gpio);

    /* ── LEDs: PC4, PC5 and PB0, PB1 as output ── */
    HAL_GPIO_WritePin(GPIOC, LED_D6_PIN | LED_D7_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, LED_D11_PIN | LED_D10_PIN, GPIO_PIN_RESET);
    gpio.Pin  = LED_D6_PIN | LED_D7_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin  = LED_D11_PIN | LED_D10_PIN;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* ── UART4: PA0(TX) PA1(RX) AF8 115200 ── */
    __HAL_RCC_UART4_CLK_ENABLE();
    gpio.Pin       = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF8_UART4;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart4.Instance          = UART4;
    huart4.Init.BaudRate     = 115200;
    huart4.Init.WordLength   = UART_WORDLENGTH_8B;
    huart4.Init.StopBits     = UART_STOPBITS_1;
    huart4.Init.Parity       = UART_PARITY_NONE;
    huart4.Init.Mode         = UART_MODE_TX_RX;
    huart4.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart4.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart4);

    /* ── I2C1 bus state check (before AF config) ── */
    /* Read SCL/SDA levels — should be HIGH when bus is idle */
    gpio.Pin   = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_Delay(1);
    {
        uint8_t scl = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8) == GPIO_PIN_SET) ? 1 : 0;
        uint8_t sda = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9) == GPIO_PIN_SET) ? 1 : 0;
        uart_print("  I2C1 bus: SCL=");
        uart_print(scl ? "H" : "L");
        uart_print(" SDA=");
        uart_println(sda ? "H (idle OK)" : "L (BUS STUCK!)");
    }

    /* ── I2C1: PB8(SCL) PB9(SDA) AF4 100kHz ── */
    /* Timing for 100kHz Standard Mode @ 64MHz PCLK1:
     * PRESC=1  → t_presc = 31.25ns
     * SCLL=0xC7(199) → tSCLL = 200*31.25 = 6250ns  (spec ≥ 4700ns) ✓
     * SCLH=0x7F(127) → tSCLH = 128*31.25 = 4000ns  (spec ≥ 4000ns) ✓
     * SCLDEL=4, SDADEL=2                                              ✓
     */
    __HAL_RCC_I2C1_CLK_ENABLE();
    gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode      = GPIO_MODE_AF_OD;
    gpio.Pull      = GPIO_NOPULL;   /* external 4.7k pull-ups on PCB (RN1) */
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &gpio);

    hi2c1.Instance             = I2C1;
    hi2c1.Init.Timing          = 0x10427FC7;   /* 100kHz @ HSI 64MHz */
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    HAL_StatusTypeDef i2c_status = HAL_I2C_Init(&hi2c1);
    uart_print("  I2C1 init: ");
    uart_println(i2c_status == HAL_OK ? "OK" : "ERROR");

    /* ── I2C4 bus state check ── */
    gpio.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_Delay(1);
    {
        uint8_t scl4 = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == GPIO_PIN_SET) ? 1 : 0;
        uint8_t sda4 = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_SET) ? 1 : 0;
        uart_print("  I2C4 bus: SCL=");
        uart_print(scl4 ? "H" : "L");
        uart_print(" SDA=");
        uart_println(sda4 ? "H (idle OK)" : "L (BUS STUCK!)");
    }

    /* ── I2C4: PB6(SCL) PB7(SDA) AF6 100kHz — EEPROM ── */
    __HAL_RCC_I2C4_CLK_ENABLE();
    gpio.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode      = GPIO_MODE_AF_OD;
    gpio.Pull      = GPIO_NOPULL;   /* pull-ups on PCB */
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF6_I2C4;
    HAL_GPIO_Init(GPIOB, &gpio);

    hi2c4.Instance             = I2C4;
    hi2c4.Init.Timing          = 0x10427FC7;  /* 100kHz @ HSI 64MHz (APB4=64MHz) */
    hi2c4.Init.OwnAddress1     = 0;
    hi2c4.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c4.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c4.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c4.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    HAL_StatusTypeDef i2c4_status = HAL_I2C_Init(&hi2c4);
    uart_print("  I2C4 init: ");
    uart_println(i2c4_status == HAL_OK ? "OK" : "ERROR");
}

/* ═══════════════════════════════════════════════════════════════
 * UART helpers (polled, no printf needed)
 * ═══════════════════════════════════════════════════════════════ */
static void uart_print(const char *s)
{
    HAL_UART_Transmit(&huart4, (const uint8_t *)s, (uint16_t)strlen(s), 100);
}

static void uart_println(const char *s)
{
    uart_print(s);
    uart_print("\r\n");
}

static void uart_print_hex(uint8_t v)
{
    const char hex[] = "0123456789ABCDEF";
    char buf[3] = { hex[v >> 4], hex[v & 0xF], '\0' };
    uart_print(buf);
}

static void uart_print_dec(uint32_t v)
{
    char buf[11];
    uint8_t i = 10;
    buf[10] = '\0';
    if (v == 0) { uart_print("0"); return; }
    while (v && i) { buf[--i] = '0' + (v % 10); v /= 10; }
    uart_print(&buf[i]);
}

static const char *lookup_i2c1(uint8_t addr7)
{
    for (uint8_t i = 0; i < KNOWN_I2C1_COUNT; i++)
        if (known_i2c1[i].addr7 == addr7) return known_i2c1[i].name;
    return NULL;
}

/* HAL SysTick */
void SysTick_Handler(void) { HAL_IncTick(); }
