#include <auroraaz/rev1655_hook_gate.h>

#include <string.h>

#include <auroraaz/image.h>

#include "rev1655_hook_gate_private.h"
#include "sha256.h"

#define AZ_REV1655_HEADER_SIZE 0x400u
#define AZ_REV1655_TEXT_RVA 0x00210000u
#define AZ_REV1655_TEXT_SIZE 0x009573DCu
#define AZ_REV1655_IMPORT_THUNK_RVA 0x00B65DFCu
#define AZ_REV1655_IMPORT_THUNK_OFFSET \
    (AZ_REV1655_IMPORT_THUNK_RVA - AZ_REV1655_TEXT_RVA)
#define AZ_REV1655_IMPORT_THUNK_SIZE 0x000015E0u
#define AZ_REV1655_IMPORT_THUNK_STRIDE 16u
#define AZ_REV1655_IMPORT_THUNK_COUNT 350u
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

/* SHA-256 of immutable .text bytes before the final loader thunk directory. */
static const uint8_t k_text_prefix_sha256[32] = {
    0x7C, 0x7C, 0x0B, 0x93, 0x11, 0x4B, 0x34, 0x45,
    0x4D, 0x32, 0xCD, 0x21, 0xA1, 0x10, 0x7E, 0xF1,
    0x83, 0x10, 0xE8, 0x35, 0x76, 0x16, 0x33, 0xAA,
    0xD9, 0x51, 0xCD, 0x2B, 0x4D, 0xB8, 0x68, 0x47
};

/*
 * Frozen physical thunk order from original/Aurora.xex and Aurora.exe.
 * Library ownership is encoded by library_for_thunk_index() below. The
 * canonical complete-.text digest independently detects any table drift.
 */
static const uint16_t k_import_ordinals[AZ_REV1655_IMPORT_THUNK_COUNT] = {
    0x03D1u, 0x01A7u, 0x01A8u, 0x02EEu, 0x02F5u, 0x0611u,
    0x02ECu, 0x01A6u, 0x0290u, 0x0291u, 0x028Bu, 0x020Bu,
    0x0231u, 0x0232u, 0x0237u, 0x0238u, 0x00FBu, 0x00FDu,
    0x00FEu, 0x0107u, 0x00FFu, 0x0100u, 0x0102u, 0x0103u,
    0x0108u, 0x00FCu, 0x0219u, 0x0269u, 0x0267u, 0x025Au,
    0x0257u, 0x020Au, 0x03D2u, 0x07DDu, 0x07DCu, 0x07E4u,
    0x07E3u, 0x09C6u, 0x07D6u, 0x0246u, 0x09C4u, 0x0282u,
    0x0271u, 0x0250u, 0x026Fu, 0x0270u, 0x0279u, 0x07D5u,
    0x0276u, 0x09CEu, 0x09DCu, 0x09D6u, 0x09C9u, 0x09CBu,
    0x0280u, 0x01CFu, 0x01F8u, 0x01EAu, 0x024Fu, 0x024Eu,
    0x0217u, 0x0218u, 0x01ECu, 0x01F7u, 0x0135u, 0x01FCu,
    0x01F4u, 0x0210u, 0x03CAu, 0x0198u, 0x02C1u, 0x02CAu,
    0x020Eu, 0x01A4u, 0x025Du, 0x025Eu, 0x02DCu, 0x03CBu,
    0x01A9u, 0x0227u, 0x03CCu, 0x00BEu, 0x0130u, 0x0125u,
    0x012Eu, 0x0003u, 0x0246u, 0x019Au, 0x01A1u, 0x0141u,
    0x0018u, 0x0010u, 0x0029u, 0x01A8u, 0x012Au, 0x0192u,
    0x0103u, 0x0104u, 0x0244u, 0x0187u, 0x012Cu, 0x00DFu,
    0x00D9u, 0x00CFu, 0x00D2u, 0x000Du, 0x0184u, 0x0185u,
    0x0186u, 0x018Fu, 0x0190u, 0x0191u, 0x0159u, 0x015Bu,
    0x018Au, 0x013Bu, 0x00F9u, 0x0199u, 0x0247u, 0x0028u,
    0x0170u, 0x016Du, 0x0163u, 0x0256u, 0x0169u, 0x016Eu,
    0x016Au, 0x0165u, 0x018Eu, 0x00B6u, 0x00B7u, 0x00B8u,
    0x00F7u, 0x00F0u, 0x012Fu, 0x0011u, 0x0007u, 0x0016u,
    0x01D4u, 0x0022u, 0x00B1u, 0x00B4u, 0x01A5u, 0x0197u,
    0x0135u, 0x0066u, 0x013Au, 0x02D7u, 0x01CFu, 0x01B8u,
    0x02D6u, 0x01D7u, 0x0271u, 0x0272u, 0x0334u, 0x0332u,
    0x0335u, 0x0105u, 0x013Du, 0x012Du, 0x0194u, 0x009Du,
    0x00B0u, 0x0070u, 0x0200u, 0x0083u, 0x00BDu, 0x01C7u,
    0x025Bu, 0x01BDu, 0x0089u, 0x004Du, 0x007Bu, 0x01DFu,
    0x01B6u, 0x01C3u, 0x01D9u, 0x01B9u, 0x008Fu, 0x00C4u,
    0x01B4u, 0x01C9u, 0x01B1u, 0x01CAu, 0x01C5u, 0x0126u,
    0x014Du, 0x007Du, 0x0269u, 0x026Au, 0x005Fu, 0x0015u,
    0x01BAu, 0x01D3u, 0x01DCu, 0x01D5u, 0x01C2u, 0x01C6u,
    0x00BAu, 0x006Bu, 0x006Cu, 0x0099u, 0x0110u, 0x006Fu,
    0x0052u, 0x009Au, 0x0136u, 0x0196u, 0x0195u, 0x00DAu,
    0x005Au, 0x00EEu, 0x00C6u, 0x010Eu, 0x010Bu, 0x0081u,
    0x00FCu, 0x00E3u, 0x0097u, 0x00C5u, 0x0021u, 0x0140u,
    0x0084u, 0x00FDu, 0x00F6u, 0x00D1u, 0x00CCu, 0x00DCu,
    0x00FFu, 0x000Fu, 0x0009u, 0x00CEu, 0x00F5u, 0x00E8u,
    0x00EFu, 0x00E7u, 0x013Fu, 0x0001u, 0x0019u, 0x0133u,
    0x0143u, 0x00D5u, 0x00F3u, 0x00FEu, 0x00DBu, 0x00D3u,
    0x00F8u, 0x00F4u, 0x00D4u, 0x00F2u, 0x0053u, 0x011Bu,
    0x00E4u, 0x00F1u, 0x012Bu, 0x01ADu, 0x01AAu, 0x01A3u,
    0x01A5u, 0x0435u, 0x0001u, 0x0002u, 0x0003u, 0x0004u,
    0x0005u, 0x0006u, 0x0007u, 0x0008u, 0x0009u, 0x000Au,
    0x000Bu, 0x000Cu, 0x000Du, 0x000Eu, 0x000Fu, 0x0012u,
    0x0013u, 0x0014u, 0x0015u, 0x0016u, 0x0017u, 0x0018u,
    0x0019u, 0x0023u, 0x001Au, 0x001Bu, 0x001Cu, 0x001Du,
    0x001Eu, 0x0020u, 0x0022u, 0x0033u, 0x0034u, 0x0035u,
    0x003Eu, 0x0043u, 0x0044u, 0x0049u, 0x004Bu, 0x004Eu,
    0x05DDu, 0x05DCu, 0x05DEu, 0x05F1u, 0x05E1u, 0x05E2u,
    0x05E0u, 0x05E7u, 0x00C9u, 0x00CAu, 0x00CBu, 0x00CCu,
    0x00CDu, 0x00CEu, 0x00CFu, 0x00D0u, 0x00D1u, 0x00D2u,
    0x00D3u, 0x00D4u, 0x00D6u, 0x00D7u, 0x00D8u, 0x00DCu,
    0x04BFu, 0x04BEu, 0x0153u, 0x0152u, 0x0155u, 0x0154u,
    0x0151u, 0x014Fu, 0x013Eu, 0x0166u, 0x01F8u, 0x00C2u,
    0x0224u, 0x0226u, 0x00AFu, 0x01F5u, 0x01F4u, 0x01F3u,
    0x01FFu, 0x02E2u, 0x005Du, 0x0147u, 0x0119u, 0x0101u,
    0x0127u, 0x0142u
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

static uint32_t read_u32_be(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24u) |
        ((uint32_t)bytes[1] << 16u) |
        ((uint32_t)bytes[2] << 8u) |
        (uint32_t)bytes[3];
}

static void write_u32_be(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24u);
    bytes[1] = (uint8_t)(value >> 16u);
    bytes[2] = (uint8_t)(value >> 8u);
    bytes[3] = (uint8_t)value;
}

static AzRev1655ImportLibrary library_for_thunk_index(size_t thunk_index)
{
    if (thunk_index <= 80u ||
        (thunk_index >= 255u && thunk_index <= 325u)) {
        return AZ_REV1655_IMPORT_LIBRARY_XAM;
    }
    return AZ_REV1655_IMPORT_LIBRARY_XBOXKRNL;
}

size_t az_rev1655_hook_gate_import_count(void)
{
    return (size_t)AZ_REV1655_IMPORT_THUNK_COUNT;
}

int az_rev1655_hook_gate_import_descriptor(
    size_t thunk_index,
    AzRev1655ImportDescriptor *out_descriptor)
{
    if (out_descriptor == NULL ||
        thunk_index >= (size_t)AZ_REV1655_IMPORT_THUNK_COUNT) {
        return 0;
    }
    out_descriptor->library = library_for_thunk_index(thunk_index);
    out_descriptor->ordinal = k_import_ordinals[thunk_index];
    return 1;
}

static void write_canonical_import_thunk(
    uint8_t bytes[AZ_REV1655_IMPORT_THUNK_STRIDE],
    AzRev1655ImportLibrary library,
    uint16_t ordinal)
{
    const uint32_t identity = ((uint32_t)library << 16u) |
        (uint32_t)ordinal;

    write_u32_be(bytes, 0x01000000u | identity);
    write_u32_be(bytes + 4u, 0x02000000u | identity);
    write_u32_be(bytes + 8u, 0x7D6903A6u);
    write_u32_be(bytes + 12u, 0x4E800420u);
}

static int target_is_valid(uint32_t target)
{
    return target != 0u && (target & 3u) == 0u;
}

static int thunk_matches_target(const uint8_t *thunk, uint32_t target)
{
    const uint32_t low = target & 0xFFFFu;
    const uint32_t adjusted_high =
        ((target >> 16u) + (low >= 0x8000u ? 1u : 0u)) & 0xFFFFu;

    return read_u32_be(thunk) == (0x3D600000u | adjusted_high) &&
        read_u32_be(thunk + 4u) == (0x396B0000u | low) &&
        read_u32_be(thunk + 8u) == 0x7D6903A6u &&
        read_u32_be(thunk + 12u) == 0x4E800420u;
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

static AzRev1655HookGateResult validate_with_resolver(
    const AzRev1655LoadedImage *image,
    const AzRev1655ImportResolver *resolver,
    const AzRev1655HookPermit **out_permit)
{
    const uint8_t *text = NULL;
    size_t text_size = 0u;
    uint8_t digest[32];
    size_t site_index;
    size_t thunk_index;
    AzSha256Context prefix_text;
    AzSha256Context canonical_text;

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
        text != image->bytes + AZ_REV1655_TEXT_RVA ||
        text_size != (size_t)AZ_REV1655_TEXT_SIZE ||
        !range_fits(
            (size_t)AZ_REV1655_IMPORT_THUNK_OFFSET,
            (size_t)AZ_REV1655_IMPORT_THUNK_SIZE,
            text_size) ||
        (size_t)AZ_REV1655_IMPORT_THUNK_OFFSET +
            (size_t)AZ_REV1655_IMPORT_THUNK_SIZE != text_size ||
        (size_t)AZ_REV1655_IMPORT_THUNK_SIZE !=
            (size_t)AZ_REV1655_IMPORT_THUNK_COUNT *
                (size_t)AZ_REV1655_IMPORT_THUNK_STRIDE) {
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

    az_sha256_init(&prefix_text);
    az_sha256_update(
        &prefix_text,
        text,
        (size_t)AZ_REV1655_IMPORT_THUNK_OFFSET);
    canonical_text = prefix_text;
    az_sha256_final(&prefix_text, digest);
    if (!bytes_equal(digest, k_text_prefix_sha256, sizeof(digest))) {
        return AZ_REV1655_HOOK_GATE_BAD_TEXT_PREFIX_SHA256;
    }

    if (resolver == NULL || resolver->resolve == NULL) {
        return AZ_REV1655_HOOK_GATE_IMPORT_RESOLVER_REQUIRED;
    }

    for (thunk_index = 0u;
         thunk_index < (size_t)AZ_REV1655_IMPORT_THUNK_COUNT;
         ++thunk_index) {
        const AzRev1655ImportLibrary library =
            library_for_thunk_index(thunk_index);
        const uint16_t ordinal = k_import_ordinals[thunk_index];
        const uint8_t *thunk = text +
            (size_t)AZ_REV1655_IMPORT_THUNK_OFFSET +
            thunk_index * (size_t)AZ_REV1655_IMPORT_THUNK_STRIDE;
        uint8_t canonical_thunk[AZ_REV1655_IMPORT_THUNK_STRIDE];
        uint32_t expected_target = 0u;

        if (!resolver->resolve(
                resolver->context,
                library,
                ordinal,
                thunk_index,
                &expected_target) ||
            !target_is_valid(expected_target)) {
            return AZ_REV1655_HOOK_GATE_IMPORT_RESOLUTION_FAILED;
        }
        if (!thunk_matches_target(thunk, expected_target)) {
            return AZ_REV1655_HOOK_GATE_BAD_IMPORT_THUNK;
        }

        write_canonical_import_thunk(
            canonical_thunk,
            library,
            ordinal);
        az_sha256_update(
            &canonical_text,
            canonical_thunk,
            sizeof(canonical_thunk));
    }

    az_sha256_final(&canonical_text, digest);
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

AzRev1655HookGateResult az_rev1655_hook_gate_validate(
    const AzRev1655LoadedImage *image,
    const AzRev1655HookPermit **out_permit)
{
    return validate_with_resolver(image, NULL, out_permit);
}

AzRev1655HookGateResult az_rev1655_hook_gate_validate_with_import_resolver(
    const AzRev1655LoadedImage *image,
    const AzRev1655ImportResolver *resolver,
    const AzRev1655HookPermit **out_permit)
{
    return validate_with_resolver(image, resolver, out_permit);
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
    case AZ_REV1655_HOOK_GATE_BAD_TEXT_PREFIX_SHA256:
        return "bad-text-prefix-sha256";
    case AZ_REV1655_HOOK_GATE_IMPORT_RESOLVER_REQUIRED:
        return "import-resolver-required";
    case AZ_REV1655_HOOK_GATE_IMPORT_RESOLUTION_FAILED:
        return "import-resolution-failed";
    case AZ_REV1655_HOOK_GATE_BAD_IMPORT_THUNK:
        return "bad-import-thunk";
    default:
        return "unknown-rev1655-hook-gate";
    }
}
