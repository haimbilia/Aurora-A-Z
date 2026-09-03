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
    AZ_REV1655_HOOK_GATE_PERMIT_IMAGE_MISMATCH,
    AZ_REV1655_HOOK_GATE_BAD_TEXT_PREFIX_SHA256,
    AZ_REV1655_HOOK_GATE_IMPORT_RESOLVER_REQUIRED,
    AZ_REV1655_HOOK_GATE_IMPORT_RESOLUTION_FAILED,
    AZ_REV1655_HOOK_GATE_BAD_IMPORT_THUNK
} AzRev1655HookGateResult;

typedef struct AzRev1655LoadedImage {
    const uint8_t *bytes;
    size_t size;
    uint32_t virtual_address;
} AzRev1655LoadedImage;

typedef struct AzRev1655HookPermit AzRev1655HookPermit;
typedef struct AzRev1655HookSiteDescriptor AzRev1655HookSiteDescriptor;

typedef enum AzRev1655ImportLibrary {
    AZ_REV1655_IMPORT_LIBRARY_XAM = 0,
    AZ_REV1655_IMPORT_LIBRARY_XBOXKRNL = 1
} AzRev1655ImportLibrary;

/*
 * Supplies an authoritative target for one frozen Rev1655 import identity.
 * The implementation must be independent of the thunk bytes being checked.
 * thunk_index is included so a trusted loader-policy adapter can account for
 * per-import redirects without baking one console's redirect addresses into
 * the gate. Return nonzero only when out_target is exact and trustworthy.
 */
typedef int (*AzRev1655ResolveImportTarget)(
    void *context,
    AzRev1655ImportLibrary library,
    uint16_t ordinal,
    size_t thunk_index,
    uint32_t *out_target);

typedef struct AzRev1655ImportResolver {
    AzRev1655ResolveImportTarget resolve;
    void *context;
} AzRev1655ImportResolver;

/*
 * Compatibility entry point. It deliberately has no trusted import resolver,
 * so a pristine loaded image reaches IMPORT_RESOLVER_REQUIRED and never earns
 * a permit. Runtime integration must use the resolver-aware entry point below.
 */
AzRev1655HookGateResult az_rev1655_hook_gate_validate(
    const AzRev1655LoadedImage *image,
    const AzRev1655HookPermit **out_permit);

/*
 * Validates the exact immutable Rev1655 .text prefix and all 350 final loader
 * thunks against authoritative resolved targets, then verifies the canonical
 * complete-.text SHA-256. Raw/unresolved import markers are never accepted.
 * Call once before installing any hook because a hook changes .text.
 */
AzRev1655HookGateResult az_rev1655_hook_gate_validate_with_import_resolver(
    const AzRev1655LoadedImage *image,
    const AzRev1655ImportResolver *resolver,
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
