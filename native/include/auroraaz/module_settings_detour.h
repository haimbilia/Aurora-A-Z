#ifndef AURORAAZ_MODULE_SETTINGS_DETOUR_H
#define AURORAAZ_MODULE_SETTINGS_DETOUR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZ_REV1655_MODULE_SETTINGS_ADDRESS 0x822C8B88u
#define AZ_REV1655_MODULE_SETTINGS_CONTINUE_ADDRESS 0x822C8B8Cu
#define AZ_REV1655_MODULE_SETTINGS_LOAD_ADDRESS 0x822C8BF0u
#define AZ_REV1655_MODULE_SETTINGS_SCENE_ADDRESS 0x822C8C38u
#define AZ_REV1655_MODULE_SETTINGS_SCENE_CONTINUE_ADDRESS 0x822C8C3Cu
#define AZ_REV1655_MODULE_SETTINGS_FIRST_INSTRUCTION 0x2F1E0001u
#define AZ_REV1655_MODULE_SETTINGS_SCENE_FIRST_INSTRUCTION 0x807F0070u
#define AZ_REV1655_NETDBG_MODULE_KEY 7u
#define AZ_REV1655_MODULE_ICON_HANDLE_OFFSET 0x60u
#define AZ_REV1655_MODULE_LIST_HANDLE_OFFSET 0x68u
#define AZ_MODULE_SETTINGS_LABEL_LENGTH 10u
#define AZ_MODULE_SETTINGS_MODE_BROWSE 0u
#define AZ_MODULE_SETTINGS_MODE_FILTER 1u

typedef struct AzModuleSettingsDetourStatus {
    uint32_t hook_calls;
    uint32_t requests_taken;
    uint8_t pending;
    uint8_t disabled;
} AzModuleSettingsDetourStatus;

void az_module_settings_detour_reset(void);
void az_module_settings_detour_begin_shutdown(void);
const uint16_t *az_module_settings_scene_path(void);
void az_module_settings_capture_controller(uint32_t controller);
void az_module_settings_capture_scene(uint32_t scene);
uint32_t az_module_settings_live_scene(void);
uint32_t az_module_settings_live_controller(void);
uint32_t az_module_settings_scene_generation(void);
uint8_t az_module_settings_request_mode(uint32_t mode);
uint8_t az_module_settings_take_mode_request(uint32_t *mode);
void az_module_settings_detour_snapshot_status(
    AzModuleSettingsDetourStatus *status);

/* Rewrites an already-allocated target std::wstring without invoking a C++
 * ABI from plugin code. The caller resolves external_storage from the target
 * string object; it must provide at least (capacity + 1) UTF-16 code units. */
uint8_t az_module_settings_write_label(
    uint8_t *wstring_object,
    uint16_t *external_storage,
    uint32_t external_code_units);

void az_rev1655_module_settings_direct_detour_entry(void);
void az_rev1655_module_settings_scene_capture_detour_entry(void);

#ifdef __cplusplus
}
#endif

#endif
