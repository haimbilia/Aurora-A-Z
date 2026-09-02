#ifndef AURORAAZ_REV1655_HOOK_GATE_PRIVATE_H
#define AURORAAZ_REV1655_HOOK_GATE_PRIVATE_H

#include <stddef.h>
#include <stdint.h>

#include <auroraaz/rev1655_hook_gate.h>

/* Private bridge consumed by the hook installer, never by feature code. */
typedef struct AzRev1655ResolvedHookSite {
    uint32_t target_address;
    uint32_t expected_instruction;
    size_t complete_signature_size;
} AzRev1655ResolvedHookSite;

/* Rechecks the complete live window before revealing an install target. */
AzRev1655HookGateResult az_rev1655_hook_gate_resolve_site(
    const AzRev1655HookPermit *permit,
    const AzRev1655HookSiteDescriptor *descriptor,
    const AzRev1655LoadedImage *image,
    AzRev1655ResolvedHookSite *out_site);

#endif
