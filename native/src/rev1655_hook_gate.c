#include <auroraaz/rev1655_hook_gate.h>

#include <string.h>

#include <auroraaz/image.h>

#include "rev1655_hook_gate_private.h"
#include "sha256.h"

#define AZ_REV1655_HEADER_SIZE 0x400u
#define AZ_REV1655_TEXT_RVA 0x00210000u
#define AZ_REV1655_INPUT_RVA 0x00801D90u
#define AZ_REV1655_RENDER_MENU_RVA 0x00358A08u
#define AZ_REV1655_FONT_END_RVA 0x0047E390u
#define AZ_REV1655_PERMIT_SEAL 0x415A1655u

struct AzRev1655HookPermit {
    const uint8_t *image_bytes;
    size_t image_size;
    uint32_t image_virtual_address;
    uint32_t seal;
};

struct AzRev1655HookSiteDescriptor {
    AzRev1655HookSiteId id;
    uint32_t image_rva;
    const uint8_t *signature;
    size_t signature_size;
    AzRev1655HookGateResult mismatch_result;
};

/* SHA-256 of original/Aurora.exe's complete 0x400-byte PE header. */
static const uint8_t k_image_header_sha256[32] = {
    0x5F, 0x74, 0x1C, 0xAD, 0xD0, 0x89, 0xB3, 0x2B,
    0x2E, 0xF5, 0xFC, 0xDD, 0xDF, 0xDF, 0x66, 0x8A,
    0x9E, 0x43, 0x44, 0xAE, 0x61, 0xDF, 0x6F, 0x3F,
    0x6E, 0x76, 0xFF, 0x11, 0x98, 0x23, 0x69, 0x25
};

/* SHA-256 of .text's exact 0x9573DC virtual bytes in Rev1655. */
static const uint8_t k_text_sha256[32] = {
    0xEE, 0x2F, 0xB2, 0xEB, 0xA8, 0x44, 0xEE, 0x1C,
    0x44, 0x4A, 0xD5, 0xD1, 0x0A, 0x52, 0xD6, 0x47,
    0x4B, 0xC4, 0xCD, 0x9F, 0xE1, 0xB8, 0x9B, 0x06,
    0xB6, 0x5B, 0x3E, 0x92, 0xD2, 0xA1, 0x77, 0xEB
};

static const uint8_t k_input_wrapper_signature[20] = {
    0x7D, 0x88, 0x02, 0xA6, 0x91, 0x81, 0xFF, 0xF8,
    0x94, 0x21, 0xFF, 0xA0, 0x90, 0x61, 0x00, 0x74,
    0x2B, 0x03, 0x00, 0xFF
};

static const uint8_t k_render_menu_signature[16] = {
    0x7D, 0x88, 0x02, 0xA6, 0x48, 0x60, 0xF2, 0xBD,
    0xDB, 0xC1, 0xFF, 0xC8, 0xDB, 0xE1, 0xFF, 0xD0
};

static const uint8_t k_font_end_signature[16] = {
    0x7D, 0x88, 0x02, 0xA6, 0x48, 0x4E, 0x99, 0x35,
    0x94, 0x21, 0xFF, 0x80, 0x81, 0x63, 0x00, 0xB4
};

static const struct AzRev1655HookSiteDescriptor k_sites[] = {
    {
        AZ_REV1655_HOOK_SITE_INPUT_WRAPPER,
        AZ_REV1655_INPUT_RVA,
        k_input_wrapper_signature,
        sizeof(k_input_wrapper_signature),
        AZ_REV1655_HOOK_GATE_BAD_INPUT_SIGNATURE
    },
    {
        AZ_REV1655_HOOK_SITE_RENDER_MENU,
        AZ_REV1655_RENDER_MENU_RVA,
        k_render_menu_signature,
        sizeof(k_render_menu_signature),
        AZ_REV1655_HOOK_GATE_BAD_RENDER_MENU_SIGNATURE
    },
    {
        AZ_REV1655_HOOK_SITE_FONT_END,
        AZ_REV1655_FONT_END_RVA,
        k_font_end_signature,
        sizeof(k_font_end_signature),
        AZ_REV1655_HOOK_GATE_BAD_FONT_END_SIGNATURE
    }
};

static struct AzRev1655HookPermit g_permit;

static int bytes_equal(
    const uint8_t *actual,
    const uint8_t *expected,
    size_t count)
{
    size_t index;
    uint8_t difference = 0u;

    for (index = 0u; index < count; ++index) {
        difference = (uint8_t)(difference |
            (uint8_t)(actual[index] ^ expected[index]));
    }
    return difference == 0u;
}

static int range_fits(size_t offset, size_t length, size_t total)
{
    return offset <= total && length <= total - offset;
}

static void revoke_permit(void)
{
    g_permit.image_bytes = NULL;
    g_permit.image_size = 0u;
    g_permit.image_virtual_address = 0u;
    g_permit.seal = 0u;
}

static int permit_is_live(const AzRev1655HookPermit *permit)
{
    return permit == &g_permit &&
        g_permit.seal == AZ_REV1655_PERMIT_SEAL &&
        g_permit.image_bytes != NULL;
}

static const struct AzRev1655HookSiteDescriptor *descriptor_for_id(
    AzRev1655HookSiteId id)
{
    const unsigned int index = (unsigned int)id;

    if (index >= (unsigned int)AZ_REV1655_HOOK_SITE_COUNT) {
        return NULL;
    }
    return &k_sites[index];
}

static AzRev1655HookGateResult validate_descriptor_window(
    const AzRev1655LoadedImage *image,
    const struct AzRev1655HookSiteDescriptor *descriptor)
{
    if (!range_fits(
            (size_t)descriptor->image_rva,
            descriptor->signature_size,
            image->size)) {
        return AZ_REV1655_HOOK_GATE_BAD_IMAGE_LAYOUT;
    }
    if (!bytes_equal(
            image->bytes + descriptor->image_rva,
            descriptor->signature,
            descriptor->signature_size)) {
        return descriptor->mismatch_result;
    }
    return AZ_REV1655_HOOK_GATE_OK;
}

AzRev1655HookGateResult az_rev1655_hook_gate_validate(
    const AzRev1655LoadedImage *image,
    const AzRev1655HookPermit **out_permit)
{
    const uint8_t *text = NULL;
    size_t text_size = 0u;
    uint8_t digest[32];
    size_t site_index;

    revoke_permit();
    if (out_permit != NULL) {
        *out_permit = NULL;
    }
    if (image == NULL || out_permit == NULL || image->bytes == NULL) {
        return AZ_REV1655_HOOK_GATE_NULL_ARGUMENT;
    }
    if (image->virtual_address != AZ_REV1655_IMAGE_BASE) {
        return AZ_REV1655_HOOK_GATE_BAD_IMAGE_BASE;
    }
    if (image->size != (size_t)AZ_REV1655_NT_IMAGE_SIZE) {
        return AZ_REV1655_HOOK_GATE_BAD_IMAGE_SIZE;
    }
    if (az_locate_rev1655_text(
            image->bytes,
            image->size,
            &text,
            &text_size) != AZ_IMAGE_REV1655 ||
        text != image->bytes + AZ_REV1655_TEXT_RVA) {
        return AZ_REV1655_HOOK_GATE_BAD_IMAGE_LAYOUT;
    }

    for (site_index = 0u;
         site_index < (size_t)AZ_REV1655_HOOK_SITE_COUNT;
         ++site_index) {
        const AzRev1655HookGateResult site_result =
            validate_descriptor_window(image, &k_sites[site_index]);
        if (site_result != AZ_REV1655_HOOK_GATE_OK) {
            return site_result;
        }
    }

    az_sha256(image->bytes, AZ_REV1655_HEADER_SIZE, digest);
    if (!bytes_equal(digest, k_image_header_sha256, sizeof(digest))) {
        return AZ_REV1655_HOOK_GATE_BAD_IMAGE_HEADER_SHA256;
    }

    az_sha256(text, text_size, digest);
    if (!bytes_equal(digest, k_text_sha256, sizeof(digest))) {
        return AZ_REV1655_HOOK_GATE_BAD_TEXT_SHA256;
    }

    g_permit.image_bytes = image->bytes;
    g_permit.image_size = image->size;
    g_permit.image_virtual_address = image->virtual_address;
    g_permit.seal = AZ_REV1655_PERMIT_SEAL;
    *out_permit = &g_permit;
    return AZ_REV1655_HOOK_GATE_OK;
}

const AzRev1655HookSiteDescriptor *az_rev1655_hook_gate_site(
    const AzRev1655HookPermit *permit,
    AzRev1655HookSiteId site_id)
{
    if (!permit_is_live(permit)) {
        return NULL;
    }
    return descriptor_for_id(site_id);
}

AzRev1655HookGateResult az_rev1655_hook_gate_resolve_site(
    const AzRev1655HookPermit *permit,
    const AzRev1655HookSiteDescriptor *descriptor,
    const AzRev1655LoadedImage *image,
    AzRev1655ResolvedHookSite *out_site)
{
    const struct AzRev1655HookSiteDescriptor *known_descriptor;
    AzRev1655HookGateResult result;

    if (out_site != NULL) {
        memset(out_site, 0, sizeof(*out_site));
    }
    if (permit == NULL || descriptor == NULL || image == NULL ||
        out_site == NULL || image->bytes == NULL) {
        return AZ_REV1655_HOOK_GATE_NULL_ARGUMENT;
    }
    if (!permit_is_live(permit)) {
        return AZ_REV1655_HOOK_GATE_BAD_PERMIT;
    }
    if (image->bytes != permit->image_bytes ||
        image->size != permit->image_size ||
        image->virtual_address != permit->image_virtual_address) {
        return AZ_REV1655_HOOK_GATE_PERMIT_IMAGE_MISMATCH;
    }

    known_descriptor = descriptor_for_id(descriptor->id);
    if (known_descriptor == NULL) {
        return AZ_REV1655_HOOK_GATE_BAD_SITE_ID;
    }
    if (known_descriptor != descriptor) {
        return AZ_REV1655_HOOK_GATE_BAD_SITE_DESCRIPTOR;
    }

    result = validate_descriptor_window(image, known_descriptor);
    if (result != AZ_REV1655_HOOK_GATE_OK) {
        return result;
    }

    out_site->target_address = image->virtual_address +
        known_descriptor->image_rva;
    out_site->expected_instruction =
        ((uint32_t)known_descriptor->signature[0] << 24u) |
        ((uint32_t)known_descriptor->signature[1] << 16u) |
        ((uint32_t)known_descriptor->signature[2] << 8u) |
        (uint32_t)known_descriptor->signature[3];
    out_site->complete_signature_size = known_descriptor->signature_size;
    return AZ_REV1655_HOOK_GATE_OK;
}

const char *az_rev1655_hook_gate_result_name(
    AzRev1655HookGateResult result)
{
    switch (result) {
    case AZ_REV1655_HOOK_GATE_OK:
        return "rev1655-hook-gate-ok";
    case AZ_REV1655_HOOK_GATE_NULL_ARGUMENT:
        return "null-argument";
    case AZ_REV1655_HOOK_GATE_BAD_IMAGE_BASE:
        return "bad-image-base";
    case AZ_REV1655_HOOK_GATE_BAD_IMAGE_SIZE:
        return "bad-image-size";
    case AZ_REV1655_HOOK_GATE_BAD_IMAGE_LAYOUT:
        return "bad-image-layout";
    case AZ_REV1655_HOOK_GATE_BAD_INPUT_SIGNATURE:
        return "bad-input-wrapper-signature";
    case AZ_REV1655_HOOK_GATE_BAD_RENDER_MENU_SIGNATURE:
        return "bad-render-menu-signature";
    case AZ_REV1655_HOOK_GATE_BAD_FONT_END_SIGNATURE:
        return "bad-font-end-signature";
    case AZ_REV1655_HOOK_GATE_BAD_IMAGE_HEADER_SHA256:
        return "bad-image-header-sha256";
    case AZ_REV1655_HOOK_GATE_BAD_TEXT_SHA256:
        return "bad-text-sha256";
    case AZ_REV1655_HOOK_GATE_BAD_PERMIT:
        return "bad-permit";
    case AZ_REV1655_HOOK_GATE_BAD_SITE_ID:
        return "bad-site-id";
    case AZ_REV1655_HOOK_GATE_BAD_SITE_DESCRIPTOR:
        return "bad-site-descriptor";
    case AZ_REV1655_HOOK_GATE_PERMIT_IMAGE_MISMATCH:
        return "permit-image-mismatch";
    default:
        return "unknown-rev1655-hook-gate";
    }
}
