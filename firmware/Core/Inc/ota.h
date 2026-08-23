/**
 * @file    ota.h
 * @brief   Actualización de firmware por WiFi — mapa de flash y sesión de carga.
 *
 * Lo comparten DOS binarios distintos: la aplicación (que recibe la imagen y la
 * deja en el área de staging) y el bootloader (que la copia sobre la app). Por
 * eso no depende del HAL: sólo <stdint.h>.
 *
 * ─────────────────────────── POR QUÉ ASÍ ───────────────────────────
 *
 * El camino "obvio" para un OTA en un STM32 es el bootloader ROM de ST, que se
 * entra moviendo BOOT0 y NRST. En la DS11 ninguno de esos dos pines llega al
 * ESP32: BOOT0 va a C49/K3/R14/R21/U7-94 y NRST a C36/C52/IC1/J7-6/K4/U7-14.
 * Sólo los botones y el conector del ST-Link. Sin un cable agregado a mano, ese
 * camino no existe.
 *
 * El segundo camino clásico es SWAP_BANK: escribir el banco inactivo y darlo
 * vuelta. Funciona, pero SWAP_BANK es un OPTION BYTE, y los option bytes son la
 * única parte de este chip donde un error se puede volver IRREVERSIBLE (un RDP
 * mal escrito deja el micro cerrado para siempre). Como la consigna era "sin
 * riesgos para el microprocesador", acá NO se tocan los option bytes. Nunca.
 *
 * Queda el tercero, que es el que se implementó:
 *
 *   0x08000000  BOOTLOADER   128 kB   banco 1, sector 0
 *               Se flashea UNA vez con el ST-Link y no se actualiza jamás por
 *               OTA. Es el único código que escribe el banco 1.
 *
 *   0x08020000  APLICACIÓN   896 kB   banco 1, sectores 1..7
 *               Acá corre el firmware. Hoy ocupa ~80 kB de los 896.
 *
 *   0x08100000  STAGING     1024 kB   banco 2, sectores 0..7
 *               Acá aterriza la imagen que llega por WiFi. Hoy está VACÍO:
 *               el firmware entero entra en el banco 1.
 *
 * La garantía de que esto no puede romper el equipo se apoya en dos hechos:
 *
 *   1. La aplicación sólo sabe escribir el BANCO 2. El chequeo de rango está en
 *      un solo lugar (stage_range_ok en ota.c) y el número de banco es una
 *      constante, no una variable. Aunque el ESP32 mandara basura, aunque la
 *      imagen viniera corrupta, lo peor que puede pasar es que quede basura en
 *      un banco que nadie ejecuta. El firmware que está corriendo no se toca.
 *
 *   2. La copia staging→app la hace el bootloader y es IDEMPOTENTE. Si se corta
 *      la luz en el medio, al arrancar de nuevo encuentra la cabecera todavía
 *      válida y vuelve a copiar desde cero. La cabecera se borra recién DESPUÉS
 *      de que la copia se verificó. No hay ningún estado intermedio del que no
 *      se salga solo.
 *
 * El precio es que hay un flasheo manual inicial (bootloader + app en su nueva
 * dirección) y que el ST-Link sigue siendo la red de seguridad para el caso que
 * esto NO cubre: subir un firmware que compila pero no arranca. Ver
 * docs/ota_wifi.md.
 */

#ifndef OTA_H
#define OTA_H

#include <stdint.h>

/* ===================== Mapa de flash ===================== */

#define OTA_BOOT_BASE       0x08000000UL
#define OTA_BOOT_SIZE       (128UL * 1024UL)

#define OTA_APP_BASE        0x08020000UL
#define OTA_APP_SIZE        (896UL * 1024UL)      /* sectores 1..7 del banco 1 */

#define OTA_STAGE_BASE      0x08100000UL          /* banco 2 completo */
#define OTA_STAGE_SIZE      (1024UL * 1024UL)

/* La imagen no arranca en 0x08100000 sino 256 B más adelante: los primeros
 * bytes son la cabecera. Está alineado a 256 para que la copia y el borrado
 * caigan siempre en múltiplos de la palabra de flash (32 B). */
#define OTA_STAGE_IMG_OFF   0x100UL
#define OTA_STAGE_IMG       (OTA_STAGE_BASE + OTA_STAGE_IMG_OFF)

/* Techo de la imagen: tiene que entrar en la app Y en el staging con su
 * cabecera. El primero es el que manda. */
#define OTA_IMG_MAX         OTA_APP_SIZE

#define OTA_SECTOR_SIZE     (128UL * 1024UL)      /* sector del H743 */
#define OTA_FLASH_WORD      32UL                  /* la escritura mínima: 256 bits */

/* ===================== Cabecera del staging =====================
 *
 * Ocupa EXACTAMENTE una palabra de flash y se escribe de una sola vez, al final
 * de la transferencia. Esto importa: en el H7 cada palabra de 32 B se puede
 * escribir una sola vez por borrado, y una palabra escrita a medias se lee con
 * error de ECC (que en una lectura normal es un bus fault). Escribirla entera
 * y de última reduce esa ventana a los ~100 µs que tarda la programación —
 * y el bootloader igual la cubre, ver Boot_HardFault en boot_main.c.
 *
 * Que la cabecera se escriba ÚLTIMA también es lo que hace que una transferencia
 * interrumpida sea inofensiva: sin cabecera válida el bootloader no copia nada. */

#define OTA_HDR_MAGIC       0x31315344UL          /* "DS11" en little-endian */

typedef struct __attribute__((packed)) {
    uint32_t magic;     /* OTA_HDR_MAGIC */
    uint32_t len;       /* bytes de imagen a partir de OTA_STAGE_IMG */
    uint32_t crc;       /* CRC32 (zlib) de esos len bytes */
    uint32_t fw_id;     /* informativo: hash git corto del binario subido */
    uint32_t build;     /* informativo: epoch unix de la compilación */
    uint32_t rsv[3];
} ota_hdr_t;

_Static_assert(sizeof(ota_hdr_t) == OTA_FLASH_WORD,
               "la cabecera tiene que ser exactamente una palabra de flash");

/* ===================== Identidad del binario =====================
 * Las define el Makefile y las compila fw_version.c, que se reconstruye en cada
 * `make`. Ver el comentario de ese archivo. */
extern const uint32_t fw_id;      /* hash git corto; 0 = se compiló sin git */
extern const uint32_t fw_build;   /* epoch unix de la compilación */

/* ===================== CRC32 =====================
 *
 * CRC-32 de zlib (poly reflejado 0xEDB88320, init y final invertidos), tabla de
 * 16 entradas. Es el mismo que calculan `zlib.crc32` en Python y `crc32` en la
 * línea de comandos, así que la imagen se puede verificar a mano desde la PC.
 *
 * Está reimplementado también en el ESP32 (firmware_esp32c3/main/main.c) en vez
 * de usar el de la ROM: las variantes de la ROM difieren en si el CRC entra y
 * sale invertido, y un desacuerdo silencioso acá haría fallar TODAS las cargas
 * con "CRC no coincide" sin ninguna pista de por qué. */

uint32_t OTA_Crc32(uint32_t crc, const void *data, uint32_t len);

#define OTA_CRC32_INIT      0xFFFFFFFFUL
#define OTA_Crc32Final(c)   ((c) ^ 0xFFFFFFFFUL)

/* ===================== Sesión de carga (lado aplicación) =====================
 *
 * Todo esto corre en el MAIN LOOP, nunca en una ISR: el borrado de un sector
 * tarda cientos de milisegundos y la programación hace espera activa sobre el
 * registro de estado del flash. */

typedef enum {
    OTA_ST_IDLE = 0,    /* sin sesión */
    OTA_ST_RECV,        /* recibiendo bytes */
    OTA_ST_READY,       /* imagen completa y verificada, esperando el reinicio */
    OTA_ST_ERROR        /* sesión abortada; ver OTA_GetError() */
} ota_state_t;

/**
 * @brief Abre una sesión: borra los sectores de staging que hagan falta.
 * @param len    tamaño de la imagen en bytes (>0, ≤ OTA_IMG_MAX)
 * @return 0 si quedó lista para recibir, negativo si no.
 *
 * ⚠️ BLOQUEA cientos de ms por sector borrado. El audio pierde deadlines
 * mientras tanto — por eso el llamador apaga los amplificadores antes.
 */
int8_t OTA_Begin(uint32_t len, uint32_t fw_id, uint32_t build);

/**
 * @brief Agrega el siguiente tramo de la imagen. Los bytes se acumulan en RAM
 *        y se bajan a flash de a palabras de 32 B.
 * @return 0 ok, negativo error (la sesión queda en OTA_ST_ERROR).
 */
int8_t OTA_Data(const uint8_t *data, uint16_t n);

/**
 * @brief Cierra la sesión: baja el resto del buffer, relee de flash, compara el
 *        CRC y —sólo si coincide— escribe la cabecera.
 * @param crc  CRC32 que calculó el emisor sobre la imagen completa.
 * @return 0 si la imagen quedó armada y lista para que el bootloader la aplique.
 *
 * Si el CRC no coincide NO se escribe la cabecera: la imagen a medio escribir
 * queda en el banco 2 y el bootloader la ignora por completo.
 */
int8_t OTA_End(uint32_t crc);

/** @brief Cancela la sesión sin escribir la cabecera. Inofensivo en cualquier estado. */
void OTA_Abort(void);

/**
 * @brief ¿El vector de excepciones de esta imagen es el de una aplicación
 *        linkeada para OTA_APP_BASE?
 * @param img_base  dónde está la imagen AHORA (staging, o ya copiada).
 * @param len       su tamaño.
 * @return 1 plausible, 0 no.
 *
 * Esto NO es una verificación de integridad —para eso está el CRC— sino de
 * IDENTIDAD, y ataja el error más probable de toda la transición: subir un .bin
 * viejo, compilado cuando la aplicación arrancaba en 0x08000000. Ese archivo
 * tiene un CRC perfectamente válido y se copiaría sin una sola queja, pero
 * todas sus direcciones absolutas apuntarían 128 kB más abajo y el equipo
 * quedaría arrancando en un HardFault sin ninguna pista de por qué.
 *
 * Las dos primeras palabras de un firmware ARM son el puntero de pila inicial y
 * el Reset_Handler. Si el segundo no cae dentro del hueco de la aplicación, la
 * imagen no fue compilada para correr ahí. Se comprueba en los dos lados: en
 * OTA_End para poder decirlo con un mensaje claro antes de escribir nada, y en
 * el bootloader porque es el último que puede evitar el desastre.
 */
int8_t OTA_VectorsPlausible(uint32_t img_base, uint32_t len);

ota_state_t OTA_GetState(void);
uint32_t    OTA_GetReceived(void);   /* bytes aceptados hasta ahora */
uint32_t    OTA_GetExpected(void);   /* tamaño anunciado en OTA_Begin */
const char *OTA_GetError(void);      /* "" si no hubo error */

/**
 * @brief Hook llamado durante las esperas largas (borrado, verificación).
 *
 * Definición __weak vacía; main.c la redefine para servir los fills de audio,
 * igual que UART_Protocol_Yield. No alcanza para tapar un borrado de sector
 * (ahí el CPU está en espera activa sobre el flash y no vuelve hasta terminar),
 * pero sí para la verificación del CRC y para los huecos entre sectores.
 */
void OTA_Yield(void);

#endif /* OTA_H */
