/**
 * @file    fw_version.c
 * @brief   Identidad del binario: de qué commit salió y cuándo se compiló.
 *
 * Existe como archivo aparte por una sola razón: el Makefile lo recompila
 * SIEMPRE, en cada `make`, aunque no haya cambiado nada. Si estos dos valores
 * fueran macros dentro de main.c, quedarían congelados en el .o hasta que main.c
 * se recompilara por otro motivo — y un identificador de versión que a veces
 * miente es peor que no tener ninguno.
 *
 * Se volvió necesario con la actualización por WiFi: hasta ahora "qué firmware
 * está corriendo" era "el último que flasheé", y eso alcanzaba. Ahora la placa
 * puede tener un binario que se subió desde otra máquina, o desde el celular,
 * o hace tres semanas. El par (fw_id, fw_build) sale por /status y se ve en la
 * página.
 */

#include <stdint.h>

/* Los define el Makefile. Los valores por defecto son para cuando alguien
 * compila este archivo suelto o sin git: 0 significa "no se sabe". */
#ifndef FW_ID
#define FW_ID     0u
#endif
#ifndef FW_BUILD
#define FW_BUILD  0u
#endif

const uint32_t fw_id    = (uint32_t)FW_ID;      /* hash git corto del commit */
const uint32_t fw_build = (uint32_t)FW_BUILD;   /* epoch unix de la compilación */
