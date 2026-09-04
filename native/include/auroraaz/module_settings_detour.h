#ifndef AURORAAZ_MODULE_SETTINGS_DETOUR_H
#define AURORAAZ_MODULE_SETTINGS_DETOUR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZ_REV1655_MODULE_SETTINGS_ADDRESS 0x822C8CE8u
#define AZ_REV1655_MODULE_SETTINGS_CONTINUE_ADDRESS 0x822C8CECu
#define AZ_REV1655_MODULE_SETTINGS_FIRST_INSTRUCTION 0x38A00001u
#define AZ_REV1655_NETDBG_MODULE_KEY 7u

void az_module_settings_detour_reset(void);
void az_module_settings_detour_begin_shutdown(void);
uint8_t az_module_settings_detour_take_request(void);

/* Called only for module key 7 by the Rev1655 assembly entry. */
void az_rev1655_module_settings_detour_c(void);

void az_rev1655_module_settings_direct_detour_entry(void);

#ifdef __cplusplus
}
#endif

#endif
