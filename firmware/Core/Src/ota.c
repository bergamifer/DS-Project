/**
 * @file    ota.c
 * @brief   Lado APLICACIÓN de la actualización por WiFi: recibe la imagen y la
 *          deja lista en el banco 2. No la aplica — de eso se encarga el
 *          bootloader (boot_main.c) en el próximo arranque.
 *
 * Todo lo de acá corre en el MAIN LOOP. Nada toca flash desde una ISR: borrar
 * un sector tarda cientos de milisegundos con el CPU en espera activa sobre el
 * registro de estado, y hacerlo con las interrupciones de audio de por medio
 * sería, además de inútil, muy difícil de razonar.
 *
 * Ver ota.h para por qué el mapa de flash es el que es.
 */

#include "ota.h"
#include "stm32h7xx_hal.h"
#include <string.h>

/* ============================================================
 * CRC-32 (zlib) por nibbles — tabla de 16 entradas, 64 bytes.
 * A ~8 ciclos/byte, los 80 kB de la imagen son ~1,4 ms a 480 MHz.
 * ============================================================ */

static const uint32_t crc32_tab[16] = {
    0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL,
    0x76DC4190UL, 0x6B6B51F4UL, 0x4DB26158UL, 0x5005713CUL,
    0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
    0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL
};

uint32_t OTA_Crc32(uint32_t crc, const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    while (len--) {
        crc ^= *p++;
        crc = (crc >> 4) ^ crc32_tab[crc & 0x0FU];
        crc = (crc >> 4) ^ crc32_tab[crc & 0x0FU];
    }
    return crc;
}

/* main.c la redefine para servir los fills de audio. */
__weak void OTA_Yield(void) { }

int8_t OTA_VectorsPlausible(uint32_t img_base, uint32_t len)
{
    if (len < 8U) return 0;

    uint32_t sp = *(volatile uint32_t *)(uintptr_t)(img_base);
    uint32_t pc = *(volatile uint32_t *)(uintptr_t)(img_base + 4U);

    /* El puntero de pila inicial tiene que apuntar a alguna RAM del H743. */
    uint32_t region = sp & 0xFF000000UL;
    if (!(region == 0x20000000UL ||     /* DTCM  — es donde está hoy */
          region == 0x24000000UL ||     /* AXI SRAM */
          region == 0x30000000UL)) {    /* RAM_D2 */
        return 0;
    }

    /* Y el Reset_Handler, dentro del hueco de la aplicación. Ésta es la que
     * caza el .bin viejo linkeado para 0x08000000: su Reset_Handler cae en
     * 0x0800xxxx y no en 0x0802xxxx. */
    if (pc < OTA_APP_BASE || pc >= OTA_APP_BASE + len) return 0;
    if ((pc & 1U) == 0U) return 0;      /* Thumb: el bit 0 va siempre en 1 */

    return 1;
}

/* ============================================================
 * Estado de la sesión
 * ============================================================ */

static ota_state_t ota_state = OTA_ST_IDLE;
static uint32_t    ota_expected;      /* bytes anunciados en Begin */
static uint32_t    ota_received;      /* bytes aceptados */
static uint32_t    ota_flushed;       /* bytes ya bajados a flash */
static uint32_t    ota_fw_id;
static uint32_t    ota_build;
static const char *ota_err = "";

/* Acumulador de una palabra de flash. Los tramos que llegan por UART son de
 * 126 bytes (lo que entra en el payload del protocolo) y la flash se escribe de
 * a 32: sin este buffer intermedio habría que atar el tamaño del tramo al de la
 * palabra, y cambiar uno rompería el otro. */
static uint8_t  ota_word[OTA_FLASH_WORD] __attribute__((aligned(4)));
static uint32_t ota_word_n;

/* ============================================================
 * Escritura de flash — el chequeo de rango vive SÓLO acá
 * ============================================================ */

/* La única barrera entre "el ESP32 manda cualquier cosa" y "se escribe donde no
 * se debe". Se le pide que sea imposible de eludir: toda escritura pasa por
 * stage_program(), stage_program() llama a esto antes de tocar nada, y el banco
 * es la constante FLASH_BANK_2, nunca una variable. El firmware que está
 * corriendo vive en el banco 1 y esta función no lo puede alcanzar. */
static int stage_range_ok(uint32_t addr, uint32_t len)
{
    if (len == 0U) return 0;
    if (addr < OTA_STAGE_BASE) return 0;
    if (addr + len < addr) return 0;                             /* overflow */
    if (addr + len > OTA_STAGE_BASE + OTA_STAGE_SIZE) return 0;
    if ((addr % OTA_FLASH_WORD) != 0U) return 0;                 /* alineación */
    return 1;
}

/**
 * @brief Programa una palabra de 32 B en el staging.
 * @param src  32 bytes alineados a 4.
 */
static int8_t stage_program(uint32_t addr, const uint8_t *src)
{
    if (!stage_range_ok(addr, OTA_FLASH_WORD)) { ota_err = "direccion fuera del staging"; return -1; }

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, addr, (uint32_t)src) != HAL_OK) {
        ota_err = "fallo la escritura de flash";
        return -1;
    }
    __DSB();
    /* Relectura inmediata: si la palabra no quedó, mejor enterarse acá que
     * después, cuando el CRC global falle y no se sepa en qué byte. */
    if (memcmp((const void *)(uintptr_t)addr, src, OTA_FLASH_WORD) != 0) {
        ota_err = "la flash no leyo lo que se escribio";
        return -1;
    }
    return 0;
}

/**
 * @brief Borra los primeros `nsec` sectores del banco 2.
 *
 * De a uno, con OTA_Yield() entre sector y sector. Dentro de un sector no hay
 * nada que hacer: el borrado es una espera activa de la que no se vuelve hasta
 * que el flash termina.
 */
static int8_t stage_erase(uint32_t nsec)
{
    if (nsec == 0U || nsec > (OTA_STAGE_SIZE / OTA_SECTOR_SIZE)) {
        ota_err = "cantidad de sectores invalida";
        return -1;
    }

    for (uint32_t s = 0; s < nsec; s++) {
        FLASH_EraseInitTypeDef e;
        uint32_t bad_sector = 0;

        e.TypeErase    = FLASH_TYPEERASE_SECTORS;
        e.Banks        = FLASH_BANK_2;   /* constante, nunca una variable */
        e.Sector       = s;
        e.NbSectors    = 1;
        e.VoltageRange = FLASH_VOLTAGE_RANGE_3;

        if (HAL_FLASHEx_Erase(&e, &bad_sector) != HAL_OK) {
            ota_err = "fallo el borrado del staging";
            return -1;
        }
        OTA_Yield();
    }
    __DSB();
    return 0;
}

/* ============================================================
 * API de sesión
 * ============================================================ */

int8_t OTA_Begin(uint32_t len, uint32_t fw_id, uint32_t build)
{
    OTA_Abort();                       /* una sesión vieja a medias no molesta */
    ota_err = "";

    if (len == 0U || len > OTA_IMG_MAX) {
        ota_err = "tamano de imagen fuera de rango";
        ota_state = OTA_ST_ERROR;
        return -1;
    }

    /* Un vector de excepciones válido empieza con un puntero de pila en RAM y
     * un Reset_Handler en flash. No es una verificación fuerte —para eso está
     * el CRC— pero descarta de entrada el error más probable de todos: subir
     * un archivo que no es el firmware. */
    if (len < 8U) { ota_err = "imagen demasiado corta"; ota_state = OTA_ST_ERROR; return -1; }

    /* Sólo se borra lo que se va a usar. Con una imagen de 80 kB es UN sector
     * (~cientos de ms) en lugar de los ocho del banco. */
    uint32_t need = OTA_STAGE_IMG_OFF + len;
    uint32_t nsec = (need + OTA_SECTOR_SIZE - 1U) / OTA_SECTOR_SIZE;

    if (HAL_FLASH_Unlock() != HAL_OK) {
        ota_err = "no se pudo destrabar la flash";
        ota_state = OTA_ST_ERROR;
        return -1;
    }

    int8_t rc = stage_erase(nsec);
    HAL_FLASH_Lock();
    if (rc != 0) { ota_state = OTA_ST_ERROR; return -1; }

    ota_expected = len;
    ota_received = 0;
    ota_flushed  = 0;
    ota_word_n   = 0;
    ota_fw_id    = fw_id;
    ota_build    = build;
    ota_state    = OTA_ST_RECV;
    return 0;
}

int8_t OTA_Data(const uint8_t *data, uint16_t n)
{
    if (ota_state != OTA_ST_RECV) { ota_err = "no hay sesion abierta"; return -1; }
    if (n == 0U) return 0;

    if (ota_received + n > ota_expected) {
        ota_err = "llegaron mas bytes de los anunciados";
        ota_state = OTA_ST_ERROR;
        return -1;
    }

    if (HAL_FLASH_Unlock() != HAL_OK) {
        ota_err = "no se pudo destrabar la flash";
        ota_state = OTA_ST_ERROR;
        return -1;
    }

    uint16_t i = 0;
    while (i < n) {
        uint32_t room = OTA_FLASH_WORD - ota_word_n;
        uint32_t take = ((uint32_t)(n - i) < room) ? (uint32_t)(n - i) : room;
        memcpy(&ota_word[ota_word_n], &data[i], take);
        ota_word_n += take;
        i          += (uint16_t)take;

        if (ota_word_n == OTA_FLASH_WORD) {
            if (stage_program(OTA_STAGE_IMG + ota_flushed, ota_word) != 0) {
                HAL_FLASH_Lock();
                ota_state = OTA_ST_ERROR;
                return -1;
            }
            ota_flushed += OTA_FLASH_WORD;
            ota_word_n = 0;
        }
    }

    HAL_FLASH_Lock();
    ota_received += n;
    return 0;
}

int8_t OTA_End(uint32_t crc)
{
    if (ota_state != OTA_ST_RECV) { ota_err = "no hay sesion abierta"; return -1; }

    if (ota_received != ota_expected) {
        ota_err = "faltaron bytes";
        ota_state = OTA_ST_ERROR;
        return -1;
    }

    if (HAL_FLASH_Unlock() != HAL_OK) {
        ota_err = "no se pudo destrabar la flash";
        ota_state = OTA_ST_ERROR;
        return -1;
    }

    /* Cola: la última palabra casi nunca cae justa en 32 B. Se rellena con 0xFF
     * —el valor de la flash borrada— así el relleno no cambia nada si alguna
     * vez se compara contra el binario original. */
    if (ota_word_n != 0U) {
        memset(&ota_word[ota_word_n], 0xFF, OTA_FLASH_WORD - ota_word_n);
        if (stage_program(OTA_STAGE_IMG + ota_flushed, ota_word) != 0) {
            HAL_FLASH_Lock();
            ota_state = OTA_ST_ERROR;
            return -1;
        }
        ota_flushed += OTA_FLASH_WORD;
        ota_word_n = 0;
    }

    /* Verificación sobre lo que quedó EN LA FLASH, no sobre lo que se recibió.
     * Es la diferencia entre "el UART anduvo" y "la imagen está bien escrita",
     * que es lo que el bootloader va a copiar. */
    uint32_t acc = OTA_CRC32_INIT;
    const uint8_t *img = (const uint8_t *)(uintptr_t)OTA_STAGE_IMG;
    uint32_t left = ota_expected, off = 0;
    while (left) {
        uint32_t chunk = (left > 4096U) ? 4096U : left;
        acc = OTA_Crc32(acc, &img[off], chunk);
        off  += chunk;
        left -= chunk;
        OTA_Yield();                    /* ~85 µs por trozo: el audio no se entera */
    }
    acc = OTA_Crc32Final(acc);

    if (acc != crc) {
        /* Sin cabecera no hay imagen: el bootloader va a ignorar por completo
         * lo que quedó escrito. No hay nada que limpiar. */
        HAL_FLASH_Lock();
        ota_err = "el CRC no coincide";
        ota_state = OTA_ST_ERROR;
        return -1;
    }

    /* El archivo llegó entero — pero ¿es un firmware para ESTA dirección? Ver
     * OTA_VectorsPlausible: un .bin viejo, de cuando la app arrancaba en
     * 0x08000000, pasa el CRC sin problema y después no arranca. */
    if (!OTA_VectorsPlausible(OTA_STAGE_IMG, ota_expected)) {
        HAL_FLASH_Lock();
        ota_err = "no parece un firmware para 0x08020000 (recompilar)";
        ota_state = OTA_ST_ERROR;
        return -1;
    }

    /* La cabecera va ÚLTIMA y de una sola vez. Hasta este instante la carga es
     * reversible sin hacer nada; a partir de acá el próximo arranque aplica. */
    ota_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = OTA_HDR_MAGIC;
    hdr.len   = ota_expected;
    hdr.crc   = crc;
    hdr.fw_id = ota_fw_id;
    hdr.build = ota_build;

    int8_t rc = stage_program(OTA_STAGE_BASE, (const uint8_t *)&hdr);
    HAL_FLASH_Lock();

    if (rc != 0) { ota_state = OTA_ST_ERROR; return -1; }

    ota_state = OTA_ST_READY;
    return 0;
}

void OTA_Abort(void)
{
    /* No hay nada que deshacer: sin cabecera escrita, lo que haya quedado en el
     * banco 2 es invisible para el bootloader. */
    ota_state  = OTA_ST_IDLE;
    ota_word_n = 0;
    ota_received = 0;
    ota_flushed  = 0;
}

ota_state_t OTA_GetState(void)    { return ota_state; }
uint32_t    OTA_GetReceived(void) { return ota_received; }
uint32_t    OTA_GetExpected(void) { return ota_expected; }
const char *OTA_GetError(void)    { return ota_err; }
