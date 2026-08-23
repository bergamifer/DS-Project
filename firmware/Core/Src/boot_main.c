/**
 * @file    boot_main.c
 * @brief   Bootloader de la DS11 — 0x08000000, sector 0 del banco 1.
 *
 * Es el único código que escribe el banco 1, y el único que no se actualiza
 * nunca por WiFi. Se flashea a mano con el ST-Link, una vez.
 *
 * Hace exactamente esto, en este orden, y nada más:
 *
 *   1. Lee la cabecera del staging (0x08100000). Si no tiene la firma correcta,
 *      salta a la aplicación y listo. Éste es el camino de TODOS los arranques
 *      normales: cuesta unos microsegundos.
 *   2. Si la firma está, verifica el CRC32 de la imagen ENTERA en el banco 2.
 *      Si no cierra, no toca nada y salta a la aplicación que ya estaba.
 *   3. Recién ahí borra los sectores de la app y copia. Si se corta la luz en el
 *      medio, la cabecera sigue estando y en el próximo arranque vuelve a
 *      empezar de cero. Esta es la propiedad que hace que no exista un estado
 *      del que no se salga solo.
 *   4. Verifica el CRC de lo copiado, borra la cabecera del staging (para no
 *      repetir la copia en cada arranque, y para que un flasheo posterior por
 *      ST-Link no quede pisado) y salta.
 *
 * NO toca los option bytes. Ni SWAP_BANK, ni RDP, ni nada. Es lo único de este
 * chip que puede quedar irreversiblemente mal, y por eso está fuera del alcance
 * de todo el diseño. Ver ota.h.
 *
 * Corre a 64 MHz sobre el HSI, sin configurar PLLs: la aplicación arma su propio
 * reloj de 480 MHz cuando arranca. Cuanto menos toca esto, menos hay que
 * deshacer antes de saltar.
 */

#include "stm32h7xx_hal.h"
#include "ota.h"
#include <string.h>

/* ============================================================
 * Salida por UART7 (PE7 RX / PE8 TX, AF7) — los mismos pines que usa la
 * aplicación, así el bootloader habla por la misma terminal serie sin cambiar
 * nada del banco de pruebas.
 * ============================================================ */

static UART_HandleTypeDef huart_dbg;

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != UART7) return;

    GPIO_InitTypeDef g = {0};
    __HAL_RCC_UART7_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    g.Pin       = GPIO_PIN_7 | GPIO_PIN_8;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF7_UART7;
    HAL_GPIO_Init(GPIOE, &g);
}

static void bp(const char *s)
{
    HAL_UART_Transmit(&huart_dbg, (uint8_t *)s, (uint16_t)strlen(s), 200);
}

/* Formateo propio en vez de sprintf: mantiene el bootloader chico y sin newlib
 * de por medio. Son ocho líneas y no vuelven a tocarse. */
static void bp_u32(uint32_t v)
{
    char b[11];
    int i = 10;
    b[i--] = '\0';
    if (v == 0) { b[i--] = '0'; }
    while (v) { b[i--] = (char)('0' + (v % 10U)); v /= 10U; }
    bp(&b[i + 1]);
}

static void bp_hex32(uint32_t v)
{
    static const char h[] = "0123456789ABCDEF";
    char b[11];
    b[0] = '0'; b[1] = 'x';
    for (int i = 0; i < 8; i++) b[2 + i] = h[(v >> (28 - 4 * i)) & 0xF];
    b[10] = '\0';
    bp(b);
}

/* ============================================================
 * LED de actividad: D4 = PA6. En la aplicación lo maneja TIM3 por PWM; acá es
 * un GPIO común. Encendido = copiando. Si alguna vez queda encendido fijo,
 * el bootloader se colgó en la copia — que es exactamente lo que uno quiere
 * poder ver de un vistazo.
 * ============================================================ */

static void led_init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin   = GPIO_PIN_6;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
}

static void led(int on)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ============================================================
 * Salto a la aplicación
 * ============================================================ */

static void boot_stall(const char *why)
{
    bp("\r\nBOOT: "); bp(why);
    bp("\r\nBOOT: no hay aplicacion valida. Flashear con ST-Link:\r\n");
    bp("      st-flash --reset write build/ds11_beamforming.bin 0x08020000\r\n");
    for (;;) {                       /* parpadeo rápido = 'flasheame' */
        led(1); HAL_Delay(120);
        led(0); HAL_Delay(120);
    }
}

__attribute__((noreturn)) static void jump_to_app(void)
{
    uint32_t sp = *(volatile uint32_t *)(uintptr_t)(OTA_APP_BASE);
    uint32_t pc = *(volatile uint32_t *)(uintptr_t)(OTA_APP_BASE + 4U);

    /* El primer word de un vector de excepciones es el puntero de pila inicial.
     * Si no apunta a una RAM de este chip, lo que hay en 0x08020000 no es un
     * firmware — casi seguro flash borrada. Saltar ahí sería un HardFault en
     * bucle sin ninguna pista; mejor quedarse parpadeando y decirlo. */
    uint32_t region = sp & 0xFF000000UL;
    if (!(region == 0x20000000UL || region == 0x24000000UL || region == 0x30000000UL)) {
        boot_stall("el vector de la app no tiene un stack pointer valido");
    }

    bp("BOOT: -> app\r\n");
    while (huart_dbg.Instance->ISR & USART_ISR_BUSY) { }   /* que salga el último byte */

    led(0);

    /* Desmontaje explícito y mínimo, en vez de HAL_RCC_DeInit() + HAL_DeInit().
     * Los dos son mazas: reinician periféricos que este código nunca encendió y
     * tocan el árbol de relojes. Acá se apaga exactamente lo que se prendió —
     * un UART y dos puertos GPIO— y el RCC se deja como está, que es como quedó
     * después del reset: HSI a 64 MHz, sin PLLs. La aplicación arma su reloj de
     * 480 MHz desde ese mismo punto de partida cuando arranca sola. */
    HAL_UART_DeInit(&huart_dbg);
    __HAL_RCC_UART7_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOE, GPIO_PIN_7 | GPIO_PIN_8);
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_6);

    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFUL;
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }

    SCB->VTOR = OTA_APP_BASE;
    __DSB();
    __ISB();
    __enable_irq();

    /* En ensamblador a propósito: entre cambiar el MSP y saltar no puede haber
     * ni un acceso a la pila vieja, y con C eso depende de si el compilador
     * decidió derramar `pc` a memoria. */
    __asm volatile (
        "msr msp, %0 \n"
        "bx  %1      \n"
        :: "r" (sp), "r" (pc) : "memory");

    for (;;) { }                      /* inalcanzable */
}

/* ============================================================
 * Red de seguridad para el ECC
 *
 * En el H7 cada palabra de 32 B se programa una sola vez, y una palabra que
 * quedó escrita A MEDIAS —por ejemplo, si se corta la luz en los ~100 µs que
 * tarda grabar la cabecera— se lee con doble error de ECC, y eso NO devuelve
 * basura: genera un bus fault. Sin esto, ese caso dejaría la placa arrancando
 * en un HardFault del bootloader, que es justo el estado del que este diseño
 * promete que siempre se sale.
 *
 * El handler no intenta reanudar: reescribe el PC apilado para que el retorno
 * de excepción caiga en fault_recover(), que ignora el staging y arranca la
 * aplicación que ya estaba. Un arranque normal con un mensaje raro es un
 * desenlace perfectamente aceptable.
 * ============================================================ */

__attribute__((noreturn)) static void fault_recover(void)
{
    bp("\r\nBOOT: el staging no se puede leer (ECC). Se ignora la actualizacion.\r\n");
    jump_to_app();
}

void boot_fault_c(uint32_t *frame)
{
    SCB->CFSR = SCB->CFSR;            /* los bits de fallo se limpian escribiendo 1 */
    SCB->HFSR = SCB->HFSR;
    frame[6] = (uint32_t)(uintptr_t)fault_recover;   /* PC apilado */
    frame[7] |= (1UL << 24);                         /* xPSR: bit T, seguimos en Thumb */
}

__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile (
        "tst lr, #4          \n"
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "b boot_fault_c      \n");
}

/* El startup deja SysTick_Handler en el Default_Handler, que es un bucle
 * infinito: sin esto, el primer tick cuelga el bootloader. */
void SysTick_Handler(void) { HAL_IncTick(); }

/* ============================================================
 * Escritura del banco 1 — el único lugar de todo el proyecto que lo hace
 * ============================================================ */

static int app_range_ok(uint32_t addr, uint32_t len)
{
    if (len == 0U) return 0;
    if (addr < OTA_APP_BASE) return 0;                        /* jamás el bootloader */
    if (addr + len < addr) return 0;
    if (addr + len > OTA_APP_BASE + OTA_APP_SIZE) return 0;
    if ((addr % OTA_FLASH_WORD) != 0U) return 0;
    return 1;
}

/** @return 0 ok. Borra los sectores del banco 1 que ocupa una imagen de `len`. */
static int app_erase(uint32_t len)
{
    uint32_t nsec = (len + OTA_SECTOR_SIZE - 1U) / OTA_SECTOR_SIZE;
    if (nsec == 0U || nsec > 7U) return -1;   /* sectores 1..7; el 0 es esto mismo */

    for (uint32_t i = 0; i < nsec; i++) {
        FLASH_EraseInitTypeDef e;
        uint32_t bad = 0;
        e.TypeErase    = FLASH_TYPEERASE_SECTORS;
        e.Banks        = FLASH_BANK_1;
        e.Sector       = 1U + i;      /* el sector 0 del banco 1 es el bootloader */
        e.NbSectors    = 1;
        e.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        if (HAL_FLASHEx_Erase(&e, &bad) != HAL_OK) return -1;
        led(i & 1);
    }
    __DSB();
    return 0;
}

/** @return 0 ok. Copia staging→app. `len` se redondea a palabra de flash. */
static int app_copy(uint32_t len)
{
    uint32_t words = (len + OTA_FLASH_WORD - 1U) / OTA_FLASH_WORD;

    for (uint32_t w = 0; w < words; w++) {
        uint32_t dst = OTA_APP_BASE  + w * OTA_FLASH_WORD;
        uint32_t src = OTA_STAGE_IMG + w * OTA_FLASH_WORD;

        if (!app_range_ok(dst, OTA_FLASH_WORD)) return -1;
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, dst, src) != HAL_OK) return -1;

        if ((w & 0x3FU) == 0U) led((w >> 6) & 1);   /* parpadeo cada 2 kB */
    }
    __DSB();
    return 0;
}

static uint32_t crc_region(uint32_t base, uint32_t len)
{
    return OTA_Crc32Final(OTA_Crc32(OTA_CRC32_INIT,
                                   (const void *)(uintptr_t)base, len));
}

/** Borra el sector 0 del staging: se lleva puesta la cabecera. */
static int stage_clear_header(void)
{
    FLASH_EraseInitTypeDef e;
    uint32_t bad = 0;
    e.TypeErase    = FLASH_TYPEERASE_SECTORS;
    e.Banks        = FLASH_BANK_2;
    e.Sector       = 0;
    e.NbSectors    = 1;
    e.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    if (HAL_FLASHEx_Erase(&e, &bad) != HAL_OK) return -1;
    __DSB();
    return 0;
}

/* ============================================================ */

int main(void)
{
    HAL_Init();
    SystemCoreClockUpdate();
    led_init();

    huart_dbg.Instance            = UART7;
    huart_dbg.Init.BaudRate       = 115200;
    huart_dbg.Init.WordLength     = UART_WORDLENGTH_8B;
    huart_dbg.Init.StopBits       = UART_STOPBITS_1;
    huart_dbg.Init.Parity         = UART_PARITY_NONE;
    huart_dbg.Init.Mode           = UART_MODE_TX_RX;
    huart_dbg.Init.HwFlowCtl      = UART_HWCONTROL_NONE;
    huart_dbg.Init.OverSampling   = UART_OVERSAMPLING_16;
    huart_dbg.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    HAL_UART_Init(&huart_dbg);

    bp("\r\n=== DS11 boot ===\r\n");

    /* La lectura que puede explotar por ECC. A partir de acá y hasta que la
     * cabecera esté validada, cualquier bus fault cae en fault_recover(). */
    const ota_hdr_t *hdr = (const ota_hdr_t *)(uintptr_t)OTA_STAGE_BASE;

    if (hdr->magic != OTA_HDR_MAGIC) {
        bp("BOOT: sin actualizacion pendiente\r\n");
        jump_to_app();
    }

    uint32_t len = hdr->len;
    uint32_t crc = hdr->crc;

    bp("BOOT: actualizacion pendiente, ");
    bp_u32(len);
    bp(" bytes, crc ");
    bp_hex32(crc);
    bp("\r\n");

    if (len == 0U || len > OTA_IMG_MAX) {
        bp("BOOT: tamano invalido, se ignora\r\n");
        jump_to_app();
    }

    /* Verificación ANTES de tocar nada del banco 1. Este orden es lo que hace
     * que una imagen corrupta no pueda dejar sin firmware al equipo: si el CRC
     * no cierra, la aplicación que ya estaba sigue intacta y arranca. */
    bp("BOOT: verificando staging... ");
    if (crc_region(OTA_STAGE_IMG, len) != crc) {
        bp("NO coincide, se ignora\r\n");
        jump_to_app();
    }
    bp("ok\r\n");

    /* Último filtro antes de tocar el banco 1: que la imagen esté compilada
     * para correr en 0x08020000. La aplicación ya lo comprobó antes de aceptar
     * la carga, pero éste es el que importa — es el que puede evitar copiar
     * sobre un firmware que anda algo que no va a arrancar. */
    if (!OTA_VectorsPlausible(OTA_STAGE_IMG, len)) {
        bp("BOOT: la imagen no esta linkeada para 0x08020000, se ignora\r\n");
        jump_to_app();
    }

    if (HAL_FLASH_Unlock() != HAL_OK) {
        bp("BOOT: no se pudo destrabar la flash\r\n");
        jump_to_app();
    }

    bp("BOOT: aplicando (no cortar la alimentacion)...\r\n");
    led(1);

    if (app_erase(len) != 0) {
        HAL_FLASH_Lock();
        boot_stall("fallo el borrado de la aplicacion");
    }

    if (app_copy(len) != 0) {
        HAL_FLASH_Lock();
        boot_stall("fallo la copia de la aplicacion");
    }

    if (crc_region(OTA_APP_BASE, len) != crc) {
        HAL_FLASH_Lock();
        /* La copia salió mal pero el staging sigue bueno y la cabecera sigue
         * puesta: reiniciar vuelve a intentarlo. Por eso se reinicia en vez de
         * saltar a una app a medio escribir. */
        bp("BOOT: la copia no verifica. Reintentando en el proximo arranque.\r\n");
        HAL_Delay(200);
        NVIC_SystemReset();
    }

    /* Recién ahora se borra la cabecera. Todo lo anterior era repetible; esto es
     * lo que cierra el ciclo. Que se haga por BORRADO y no escribiendo una marca
     * tiene dos motivos: no hay que reescribir una palabra ya escrita (imposible
     * en el H7 sin borrar), y un flasheo posterior por ST-Link no queda pisado
     * por una copia fantasma en el arranque siguiente. */
    stage_clear_header();
    HAL_FLASH_Lock();

    bp("BOOT: actualizacion aplicada, ");
    bp_u32(len);
    bp(" bytes\r\n");
    led(0);

    jump_to_app();
}

void Error_Handler(void)
{
    boot_stall("Error_Handler");
}
