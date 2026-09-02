#ifndef AURORAAZ_COMPATIBILITY_H
#define AURORAAZ_COMPATIBILITY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZ_REV1655_TEXT_BASE 0x82210000u
#define AZ_REV1655_TEXT_SIZE 0x009573DCu

typedef enum AzCompatibilityResult {
    AZ_COMPATIBLE_REV1655 = 0,
    AZ_COMPAT_BAD_TEXT_BASE,
    AZ_COMPAT_BAD_TEXT_SIZE,
    AZ_COMPAT_BAD_ENTRY_PROBE,
    AZ_COMPAT_BAD_PLUGIN_MANAGER_PROBE,
    AZ_COMPAT_BAD_MODULE_LOADER_PROBE
} AzCompatibilityResult;

AzCompatibilityResult az_validate_rev1655_text(
    const uint8_t *text,
    size_t text_size,
    uint32_t text_virtual_address);

const char *az_compatibility_result_name(AzCompatibilityResult result);

#ifdef __cplusplus
}
#endif

#endif
