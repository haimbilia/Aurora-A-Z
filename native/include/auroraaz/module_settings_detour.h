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
#define AZ_MODULE_SETTINGS_LABEL_LENGTH 10u

typedef struct AzModuleSettingsDetourStatus {
    uint32_t hook_calls;
    uint32_t requests_taken;
    uint8_t pending;
    uint8_t disabled;
} AzModuleSettingsDetourStatus;

void az_module_settings_detour_reset(void);
void az_module_settings_detour_begin_shutdown(void);
uint8_t az_module_settings_detour_take_request(void);
void az_module_settings_detour_snapshot_status(
    AzModuleSettingsDetourStatus *status);

/* Rewrites an already-allocated target std::wstring without invoking a C++
 * ABI from plugin code. The caller resolves external_storage from the target
 * string object; it must provide at least (capacity + 1) UTF-16 code units. */
uint8_t az_module_settings_write_label(
    uint8_t *wstring_object,
    uint16_t *external_storage,
    uint32_t external_code_units);

/* Called only for module key 7 by the Rev1655 assembly entry. */
void az_rev1655_module_settings_detour_c(void);

void az_rev1655_module_settings_direct_detour_entry(void);

#ifdef __cplusplus
}
#endif

#endif
