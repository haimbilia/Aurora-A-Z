#ifndef AURORAAZ_REV1655_HOOK_GATE_H
#define AURORAAZ_REV1655_HOOK_GATE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Public code selects a reviewed site by role; target addresses stay private. */
typedef enum AzRev1655HookSiteId {
    AZ_REV1655_HOOK_SITE_INPUT_WRAPPER = 0,
    AZ_REV1655_HOOK_SITE_RENDER_MENU,
    AZ_REV1655_HOOK_SITE_FONT_END,
    AZ_REV1655_HOOK_SITE_COUNT
} AzRev1655HookSiteId;

typedef enum AzRev1655HookGateResult {
    AZ_REV1655_HOOK_GATE_OK = 0,
    AZ_REV1655_HOOK_GATE_NULL_ARGUMENT,
    AZ_REV1655_HOOK_GATE_BAD_IMAGE_BASE,
    AZ_REV1655_HOOK_GATE_BAD_IMAGE_SIZE,
    AZ_REV1655_HOOK_GATE_BAD_IMAGE_LAYOUT,
    AZ_REV1655_HOOK_GATE_BAD_INPUT_SIGNATURE,
    AZ_REV1655_HOOK_GATE_BAD_RENDER_MENU_SIGNATURE,
    AZ_REV1655_HOOK_GATE_BAD_FONT_END_SIGNATURE,
    AZ_REV1655_HOOK_GATE_BAD_IMAGE_HEADER_SHA256,
    AZ_REV1655_HOOK_GATE_BAD_TEXT_SHA256,
    AZ_REV1655_HOOK_GATE_BAD_PERMIT,
    AZ_REV1655_HOOK_GATE_BAD_SITE_ID,
    AZ_REV1655_HOOK_GATE_BAD_SITE_DESCRIPTOR,
    AZ_REV1655_HOOK_GATE_PERMIT_IMAGE_MISMATCH
} AzRev1655HookGateResult;

typedef struct AzRev1655LoadedImage {
    const uint8_t *bytes;
    size_t size;
    uint32_t virtual_address;
} AzRev1655LoadedImage;

typedef struct AzRev1655HookPermit AzRev1655HookPermit;
typedef struct AzRev1655HookSiteDescriptor AzRev1655HookSiteDescriptor;

/*
 * Validates the exact loaded Rev1655 PE header and complete .text SHA-256,
 * plus every supported hook-site window. No permit is returned on mismatch.
 * Call once before installing any hook because a hook changes .text.
 */
AzRev1655HookGateResult az_rev1655_hook_gate_validate(
    const AzRev1655LoadedImage *image,
    const AzRev1655HookPermit **out_permit);

/* Returns an opaque, reviewed descriptor only for a live validated permit. */
const AzRev1655HookSiteDescriptor *az_rev1655_hook_gate_site(
    const AzRev1655HookPermit *permit,
    AzRev1655HookSiteId site_id);

const char *az_rev1655_hook_gate_result_name(
    AzRev1655HookGateResult result);

#ifdef __cplusplus
}
#endif

#endif
