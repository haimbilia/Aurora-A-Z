#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <auroraaz/image.h>
#include <auroraaz/rev1655_hook_gate.h>

#include "rev1655_hook_gate_private.h"
#include "sha256.h"

#define REV1655_TEXT_RVA 0x00210000u
#define REV1655_TEXT_SIZE 0x009573DCu
#define REV1655_THUNK_OFFSET 0x00955DFCu
#define REV1655_THUNK_COUNT 350u
#define REV1655_THUNK_SIZE 16u
#define REV1655_INPUT_RVA 0x00801D90u
#define REV1655_RENDER_MENU_RVA 0x00358A08u
#define REV1655_FONT_END_RVA 0x0047E390u

static int failures = 0;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

static const uint8_t k_input_signature[20] = {
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

static const uint8_t k_sha256_empty[32] = {
    0xE3, 0xB0, 0xC4, 0x42, 0x98, 0xFC, 0x1C, 0x14,
    0x9A, 0xFB, 0xF4, 0xC8, 0x99, 0x6F, 0xB9, 0x24,
    0x27, 0xAE, 0x41, 0xE4, 0x64, 0x9B, 0x93, 0x4C,
    0xA4, 0x95, 0x99, 0x1B, 0x78, 0x52, 0xB8, 0x55
};

static const uint8_t k_sha256_abc[32] = {
    0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA,
    0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
    0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C,
    0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD
};

static const uint8_t k_sha256_million_a[32] = {
    0xCD, 0xC7, 0x6E, 0x5C, 0x99, 0x14, 0xFB, 0x92,
    0x81, 0xA1, 0xC7, 0xE2, 0x84, 0xD7, 0x3E, 0x67,
    0xF1, 0x80, 0x9A, 0x48, 0xA4, 0x97, 0x20, 0x0E,
    0x04, 0x6D, 0x39, 0xCC, 0xC7, 0x11, 0x2C, 0xD0
};

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
        (uint16_t)((uint16_t)bytes[1] << 8u));
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8u) |
        ((uint32_t)bytes[2] << 16u) |
        ((uint32_t)bytes[3] << 24u);
}

static uint32_t read_u32_be(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24u) |
        ((uint32_t)bytes[1] << 16u) |
        ((uint32_t)bytes[2] << 8u) |
        (uint32_t)bytes[3];
}

static void write_u16_le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8u);
}

static void write_u32_le(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8u);
    bytes[2] = (uint8_t)(value >> 16u);
    bytes[3] = (uint8_t)(value >> 24u);
}

static void write_u32_be(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24u);
    bytes[1] = (uint8_t)(value >> 16u);
    bytes[2] = (uint8_t)(value >> 8u);
    bytes[3] = (uint8_t)value;
}

static uint8_t *make_synthetic_image(void)
{
    const size_t pe_offset = 0xF8u;
    const size_t optional_offset = pe_offset + 24u;
    const size_t section_table_offset = optional_offset + 0xE0u;
    uint8_t *image = (uint8_t *)calloc(AZ_REV1655_NT_IMAGE_SIZE, 1u);
    uint8_t *text_section;

    if (image == NULL) {
        return NULL;
    }

    image[0] = (uint8_t)'M';
    image[1] = (uint8_t)'Z';
    write_u32_le(image + 0x3Cu, (uint32_t)pe_offset);
    image[pe_offset] = (uint8_t)'P';
    image[pe_offset + 1u] = (uint8_t)'E';
    write_u16_le(image + pe_offset + 4u, 0x01F2u);
    write_u16_le(image + pe_offset + 6u, 9u);
    write_u16_le(image + pe_offset + 20u, 0xE0u);
    write_u16_le(image + optional_offset, 0x010Bu);
    write_u32_le(image + optional_offset + 16u, 0x008050E0u);
    write_u32_le(image + optional_offset + 28u, AZ_REV1655_IMAGE_BASE);
    write_u32_le(image + optional_offset + 56u, AZ_REV1655_NT_IMAGE_SIZE);
    write_u32_le(image + optional_offset + 60u, 0x400u);

    text_section = image + section_table_offset + (2u * 40u);
    memcpy(text_section, ".text", 5u);
    write_u32_le(text_section + 8u, 0x009573DCu);
    write_u32_le(text_section + 12u, REV1655_TEXT_RVA);

    memcpy(image + REV1655_INPUT_RVA,
        k_input_signature, sizeof(k_input_signature));
    memcpy(image + REV1655_RENDER_MENU_RVA,
        k_render_menu_signature, sizeof(k_render_menu_signature));
    memcpy(image + REV1655_FONT_END_RVA,
        k_font_end_signature, sizeof(k_font_end_signature));
    return image;
}

static int range_fits(size_t offset, size_t length, size_t total)
{
    return offset <= total && length <= total - offset;
}

static uint8_t *load_pe_as_image(const char *path)
{
    FILE *file;
    long file_length;
    uint8_t *raw = NULL;
    uint8_t *image = NULL;
    size_t raw_size;
    size_t pe_offset;
    size_t optional_offset;
    size_t section_table_offset;
    size_t headers_size;
    uint16_t section_count;
    uint16_t optional_size;
    uint16_t section_index;

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    file_length = ftell(file);
    if (file_length <= 0L || fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    raw_size = (size_t)file_length;
    raw = (uint8_t *)malloc(raw_size);
    if (raw == NULL || fread(raw, 1u, raw_size, file) != raw_size) {
        free(raw);
        fclose(file);
        return NULL;
    }
    fclose(file);

    if (!range_fits(0u, 0x40u, raw_size)) {
        free(raw);
        return NULL;
    }
    pe_offset = (size_t)read_u32_le(raw + 0x3Cu);
    if (!range_fits(pe_offset, 24u, raw_size)) {
        free(raw);
        return NULL;
    }
    section_count = read_u16_le(raw + pe_offset + 6u);
    optional_size = read_u16_le(raw + pe_offset + 20u);
    optional_offset = pe_offset + 24u;
    if (!range_fits(optional_offset, (size_t)optional_size, raw_size)) {
        free(raw);
        return NULL;
    }
    headers_size = (size_t)read_u32_le(raw + optional_offset + 60u);
    section_table_offset = optional_offset + (size_t)optional_size;
    if (!range_fits(0u, headers_size, raw_size) ||
        !range_fits(section_table_offset,
            (size_t)section_count * 40u, raw_size)) {
        free(raw);
        return NULL;
    }

    image = (uint8_t *)calloc(AZ_REV1655_NT_IMAGE_SIZE, 1u);
    if (image == NULL) {
        free(raw);
        return NULL;
    }
    memcpy(image, raw, headers_size);

    for (section_index = 0u; section_index < section_count; ++section_index) {
        const uint8_t *section = raw + section_table_offset +
            ((size_t)section_index * 40u);
        const size_t virtual_address =
            (size_t)read_u32_le(section + 12u);
        const size_t raw_section_size =
            (size_t)read_u32_le(section + 16u);
        const size_t raw_offset = (size_t)read_u32_le(section + 20u);

        if (!range_fits(raw_offset, raw_section_size, raw_size) ||
            !range_fits(
                virtual_address,
                raw_section_size,
                AZ_REV1655_NT_IMAGE_SIZE)) {
            free(image);
            free(raw);
            return NULL;
        }
        memcpy(image + virtual_address, raw + raw_offset, raw_section_size);
    }

    free(raw);
    return image;
}

typedef struct TestImportResolver {
    uint32_t targets[REV1655_THUNK_COUNT];
    AzRev1655ImportLibrary libraries[REV1655_THUNK_COUNT];
    uint16_t ordinals[REV1655_THUNK_COUNT];
    size_t fail_index;
    size_t calls;
} TestImportResolver;

static int test_resolve_import(
    void *context,
    AzRev1655ImportLibrary library,
    uint16_t ordinal,
    size_t thunk_index,
    uint32_t *out_target)
{
    TestImportResolver *resolver = (TestImportResolver *)context;

    if (resolver == NULL || out_target == NULL ||
        thunk_index >= (size_t)REV1655_THUNK_COUNT ||
        thunk_index == resolver->fail_index ||
        library != resolver->libraries[thunk_index] ||
        ordinal != resolver->ordinals[thunk_index]) {
        return 0;
    }
    ++resolver->calls;
    *out_target = resolver->targets[thunk_index];
    return 1;
}

static void initialize_test_resolver(TestImportResolver *resolver)
{
    size_t index;

    memset(resolver, 0, sizeof(*resolver));
    resolver->fail_index = (size_t)REV1655_THUNK_COUNT;
    for (index = 0u; index < (size_t)REV1655_THUNK_COUNT; ++index) {
        AzRev1655ImportDescriptor descriptor;

        CHECK(az_rev1655_hook_gate_import_descriptor(
            index, &descriptor) != 0);
        resolver->libraries[index] = descriptor.library;
        resolver->ordinals[index] = descriptor.ordinal;
        resolver->targets[index] = 0x81004000u +
            (uint32_t)index * 0x00000104u;
    }

    /* Exercise both addi immediate cases, including adjusted-high carry. */
    resolver->targets[0] = 0x81234000u;
    resolver->targets[1] = 0x8123FFFCu;
}

static void encode_loaded_thunk(uint8_t *thunk, uint32_t target)
{
    const uint32_t low = target & 0xFFFFu;
    const uint32_t adjusted_high =
        ((target >> 16u) + (low >= 0x8000u ? 1u : 0u)) & 0xFFFFu;

    write_u32_be(thunk, 0x3D600000u | adjusted_high);
    write_u32_be(thunk + 4u, 0x396B0000u | low);
    write_u32_be(thunk + 8u, 0x7D6903A6u);
    write_u32_be(thunk + 12u, 0x4E800420u);
}

static void make_image_loaded(
    uint8_t *image,
    const TestImportResolver *resolver)
{
    size_t index;
    uint8_t *thunks = image + REV1655_TEXT_RVA + REV1655_THUNK_OFFSET;

    for (index = 0u; index < (size_t)REV1655_THUNK_COUNT; ++index) {
        encode_loaded_thunk(
            thunks + index * (size_t)REV1655_THUNK_SIZE,
            resolver->targets[index]);
    }
}

static AzRev1655ImportResolver make_resolver_api(
    TestImportResolver *resolver)
{
    AzRev1655ImportResolver api;

    api.resolve = test_resolve_import;
    api.context = resolver;
    return api;
}

static void test_sha256(void)
{
    static const uint8_t abc[] = { 0x61u, 0x62u, 0x63u };
    static const uint8_t split_message[] =
        "incremental SHA-256 crosses a block boundary without changing it";
    uint8_t digest[32];
    uint8_t expected[32];
    uint8_t thousand_a[1000];
    AzSha256Context context;
    size_t index;

    az_sha256(NULL, 0u, digest);
    CHECK(memcmp(digest, k_sha256_empty, sizeof(digest)) == 0);
    az_sha256(abc, sizeof(abc), digest);
    CHECK(memcmp(digest, k_sha256_abc, sizeof(digest)) == 0);

    memset(digest, 0xFF, sizeof(digest));
    az_sha256(NULL, 1u, digest);
    CHECK(digest[0] == 0u);

    az_sha256(split_message, sizeof(split_message) - 1u, expected);
    az_sha256_init(&context);
    az_sha256_update(&context, split_message, 1u);
    az_sha256_update(&context, split_message + 1u, 31u);
    az_sha256_update(
        &context,
        split_message + 32u,
        sizeof(split_message) - 1u - 32u);
    az_sha256_final(&context, digest);
    CHECK(memcmp(digest, expected, sizeof(digest)) == 0);

    memset(thousand_a, (int)'a', sizeof(thousand_a));
    az_sha256_init(&context);
    for (index = 0u; index < 1000u; ++index) {
        az_sha256_update(&context, thousand_a, sizeof(thousand_a));
    }
    az_sha256_final(&context, digest);
    CHECK(memcmp(digest, k_sha256_million_a, sizeof(digest)) == 0);

    az_sha256_init(&context);
    az_sha256_update(&context, NULL, 1u);
    memset(digest, 0xFF, sizeof(digest));
    az_sha256_final(&context, digest);
    CHECK(digest[0] == 0u);
}

static void test_fail_closed_gate(void)
{
    uint8_t *bytes = make_synthetic_image();
    AzRev1655LoadedImage image;
    const AzRev1655HookPermit *permit = NULL;

    CHECK(bytes != NULL);
    if (bytes == NULL) {
        return;
    }
    image.bytes = bytes;
    image.size = AZ_REV1655_NT_IMAGE_SIZE;
    image.virtual_address = AZ_REV1655_IMAGE_BASE;

    CHECK(az_rev1655_hook_gate_validate(&image, &permit) ==
        AZ_REV1655_HOOK_GATE_BAD_IMAGE_HEADER_SHA256);
    CHECK(permit == NULL);

    bytes[REV1655_INPUT_RVA + sizeof(k_input_signature) - 1u] ^= 1u;
    CHECK(az_rev1655_hook_gate_validate(&image, &permit) ==
        AZ_REV1655_HOOK_GATE_BAD_INPUT_SIGNATURE);
    bytes[REV1655_INPUT_RVA + sizeof(k_input_signature) - 1u] ^= 1u;

    bytes[REV1655_RENDER_MENU_RVA + sizeof(k_render_menu_signature) - 1u] ^=
        1u;
    CHECK(az_rev1655_hook_gate_validate(&image, &permit) ==
        AZ_REV1655_HOOK_GATE_BAD_RENDER_MENU_SIGNATURE);
    bytes[REV1655_RENDER_MENU_RVA + sizeof(k_render_menu_signature) - 1u] ^=
        1u;

    bytes[REV1655_FONT_END_RVA + sizeof(k_font_end_signature) - 1u] ^= 1u;
    CHECK(az_rev1655_hook_gate_validate(&image, &permit) ==
        AZ_REV1655_HOOK_GATE_BAD_FONT_END_SIGNATURE);
    bytes[REV1655_FONT_END_RVA + sizeof(k_font_end_signature) - 1u] ^= 1u;

    image.virtual_address += 4u;
    CHECK(az_rev1655_hook_gate_validate(&image, &permit) ==
        AZ_REV1655_HOOK_GATE_BAD_IMAGE_BASE);
    image.virtual_address = AZ_REV1655_IMAGE_BASE;
    --image.size;
    CHECK(az_rev1655_hook_gate_validate(&image, &permit) ==
        AZ_REV1655_HOOK_GATE_BAD_IMAGE_SIZE);
    image.size = AZ_REV1655_NT_IMAGE_SIZE;

    bytes[0] = 0u;
    CHECK(az_rev1655_hook_gate_validate(&image, &permit) ==
        AZ_REV1655_HOOK_GATE_BAD_IMAGE_LAYOUT);
    bytes[0] = (uint8_t)'M';

    CHECK(az_rev1655_hook_gate_validate(NULL, &permit) ==
        AZ_REV1655_HOOK_GATE_NULL_ARGUMENT);
    CHECK(az_rev1655_hook_gate_validate(&image, NULL) ==
        AZ_REV1655_HOOK_GATE_NULL_ARGUMENT);
    CHECK(az_rev1655_hook_gate_site(NULL,
        AZ_REV1655_HOOK_SITE_INPUT_WRAPPER) == NULL);
    CHECK(strcmp(az_rev1655_hook_gate_result_name(
        AZ_REV1655_HOOK_GATE_BAD_TEXT_SHA256),
        "bad-text-sha256") == 0);
    CHECK(strcmp(az_rev1655_hook_gate_result_name(
        (AzRev1655HookGateResult)99),
        "unknown-rev1655-hook-gate") == 0);

    free(bytes);
}

static void check_resolved_site(
    const AzRev1655HookPermit *permit,
    const AzRev1655LoadedImage *image,
    AzRev1655HookSiteId id,
    uint32_t expected_address,
    size_t expected_signature_size)
{
    const AzRev1655HookSiteDescriptor *descriptor =
        az_rev1655_hook_gate_site(permit, id);
    AzRev1655ResolvedHookSite resolved;

    CHECK(descriptor != NULL);
    if (descriptor == NULL) {
        return;
    }
    CHECK(az_rev1655_hook_gate_resolve_site(
        permit, descriptor, image, &resolved) ==
        AZ_REV1655_HOOK_GATE_OK);
    CHECK(resolved.target_address == expected_address);
    CHECK(resolved.expected_instruction == 0x7D8802A6u);
    CHECK(resolved.complete_signature_size == expected_signature_size);
}

static void test_exact_fixture(const char *path)
{
    uint8_t *bytes = load_pe_as_image(path);
    AzRev1655LoadedImage image;
    const AzRev1655HookPermit *permit = NULL;
    const AzRev1655HookPermit *live_permit;
    const AzRev1655HookSiteDescriptor *descriptor;
    AzRev1655ResolvedHookSite resolved;
    uint8_t *second_image;
    uint8_t *first_thunk;
    TestImportResolver test_resolver;
    AzRev1655ImportResolver resolver_api;
    size_t index;
    size_t xam_count = 0u;
    size_t kernel_count = 0u;

    if (bytes == NULL) {
        printf("Rev1655 fixture unavailable at %s; exact tests skipped\n",
            path);
        return;
    }
    image.bytes = bytes;
    image.size = AZ_REV1655_NT_IMAGE_SIZE;
    image.virtual_address = AZ_REV1655_IMAGE_BASE;
    initialize_test_resolver(&test_resolver);
    resolver_api = make_resolver_api(&test_resolver);

    CHECK(az_rev1655_hook_gate_import_count() ==
        (size_t)REV1655_THUNK_COUNT);
    CHECK(az_rev1655_hook_gate_import_descriptor(
        (size_t)REV1655_THUNK_COUNT, NULL) == 0);
    for (index = 0u; index < (size_t)REV1655_THUNK_COUNT; ++index) {
        AzRev1655ImportDescriptor import_descriptor;
        const uint8_t *raw_thunk = bytes + REV1655_TEXT_RVA +
            REV1655_THUNK_OFFSET + index * (size_t)REV1655_THUNK_SIZE;
        uint32_t identity;

        CHECK(az_rev1655_hook_gate_import_descriptor(
            index, &import_descriptor) != 0);
        identity = ((uint32_t)import_descriptor.library << 16u) |
            (uint32_t)import_descriptor.ordinal;
        CHECK(read_u32_be(raw_thunk) == (0x01000000u | identity));
        CHECK(read_u32_be(raw_thunk + 4u) == (0x02000000u | identity));
        CHECK(read_u32_be(raw_thunk + 8u) == 0x7D6903A6u);
        CHECK(read_u32_be(raw_thunk + 12u) == 0x4E800420u);

        if (import_descriptor.library == AZ_REV1655_IMPORT_LIBRARY_XAM) {
            ++xam_count;
        } else if (import_descriptor.library ==
                AZ_REV1655_IMPORT_LIBRARY_XBOXKRNL) {
            ++kernel_count;
        } else {
            CHECK(0);
        }
    }
    CHECK(xam_count == 152u);
    CHECK(kernel_count == 198u);
    CHECK(test_resolver.libraries[80] == AZ_REV1655_IMPORT_LIBRARY_XAM);
    CHECK(test_resolver.libraries[81] ==
        AZ_REV1655_IMPORT_LIBRARY_XBOXKRNL);
    CHECK(test_resolver.libraries[254] ==
        AZ_REV1655_IMPORT_LIBRARY_XBOXKRNL);
    CHECK(test_resolver.libraries[255] == AZ_REV1655_IMPORT_LIBRARY_XAM);
    CHECK(test_resolver.libraries[325] == AZ_REV1655_IMPORT_LIBRARY_XAM);
    CHECK(test_resolver.libraries[326] ==
        AZ_REV1655_IMPORT_LIBRARY_XBOXKRNL);

    /* Raw/unresolved thunks are never a valid runtime image. */
    CHECK(az_rev1655_hook_gate_validate(&image, &permit) ==
        AZ_REV1655_HOOK_GATE_IMPORT_RESOLVER_REQUIRED);
    CHECK(permit == NULL);
    CHECK(az_rev1655_hook_gate_validate_with_import_resolver(
        &image, &resolver_api, &permit) ==
        AZ_REV1655_HOOK_GATE_BAD_IMPORT_THUNK);
    CHECK(permit == NULL);

    make_image_loaded(bytes, &test_resolver);
    first_thunk = bytes + REV1655_TEXT_RVA + REV1655_THUNK_OFFSET;
    CHECK(read_u32_be(first_thunk) == 0x3D608123u);
    CHECK(read_u32_be(first_thunk + 4u) == 0x396B4000u);
    CHECK(read_u32_be(first_thunk + REV1655_THUNK_SIZE) == 0x3D608124u);
    CHECK(read_u32_be(first_thunk + REV1655_THUNK_SIZE + 4u) ==
        0x396BFFFCu);

    test_resolver.calls = 0u;
    CHECK(az_rev1655_hook_gate_validate_with_import_resolver(
        &image, &resolver_api, &permit) == AZ_REV1655_HOOK_GATE_OK);
    CHECK(permit != NULL);
    CHECK(test_resolver.calls == (size_t)REV1655_THUNK_COUNT);
    if (permit == NULL) {
        free(bytes);
        return;
    }

    check_resolved_site(permit, &image,
        AZ_REV1655_HOOK_SITE_INPUT_WRAPPER, 0x82801D90u, 20u);
    check_resolved_site(permit, &image,
        AZ_REV1655_HOOK_SITE_RENDER_MENU, 0x82358A08u, 16u);
    check_resolved_site(permit, &image,
        AZ_REV1655_HOOK_SITE_FONT_END, 0x8247E390u, 16u);
    CHECK(az_rev1655_hook_gate_site(permit,
        AZ_REV1655_HOOK_SITE_COUNT) == NULL);

    descriptor = az_rev1655_hook_gate_site(
        permit, AZ_REV1655_HOOK_SITE_FONT_END);
    CHECK(descriptor != NULL);
    bytes[REV1655_FONT_END_RVA + sizeof(k_font_end_signature) - 1u] ^= 1u;
    CHECK(az_rev1655_hook_gate_resolve_site(
        permit, descriptor, &image, &resolved) ==
        AZ_REV1655_HOOK_GATE_BAD_FONT_END_SIGNATURE);
    bytes[REV1655_FONT_END_RVA + sizeof(k_font_end_signature) - 1u] ^= 1u;

    second_image = (uint8_t *)malloc(AZ_REV1655_NT_IMAGE_SIZE);
    CHECK(second_image != NULL);
    if (second_image != NULL) {
        AzRev1655LoadedImage other = image;
        memcpy(second_image, bytes, AZ_REV1655_NT_IMAGE_SIZE);
        other.bytes = second_image;
        CHECK(az_rev1655_hook_gate_resolve_site(
            permit, descriptor, &other, &resolved) ==
            AZ_REV1655_HOOK_GATE_PERMIT_IMAGE_MISMATCH);
        free(second_image);
    }

    live_permit = permit;
    bytes[REV1655_TEXT_RVA + 0x100u] ^= 1u;
    CHECK(az_rev1655_hook_gate_validate_with_import_resolver(
        &image, &resolver_api, &permit) ==
        AZ_REV1655_HOOK_GATE_BAD_TEXT_PREFIX_SHA256);
    CHECK(permit == NULL);
    CHECK(az_rev1655_hook_gate_site(live_permit,
        AZ_REV1655_HOOK_SITE_INPUT_WRAPPER) == NULL);
    bytes[REV1655_TEXT_RVA + 0x100u] ^= 1u;

    bytes[0x3FFu] ^= 1u;
    CHECK(az_rev1655_hook_gate_validate_with_import_resolver(
        &image, &resolver_api, &permit) ==
        AZ_REV1655_HOOK_GATE_BAD_IMAGE_HEADER_SHA256);
    CHECK(permit == NULL);
    bytes[0x3FFu] ^= 1u;

    CHECK(az_rev1655_hook_gate_validate(&image, &permit) ==
        AZ_REV1655_HOOK_GATE_IMPORT_RESOLVER_REQUIRED);
    CHECK(permit == NULL);

    test_resolver.fail_index = 17u;
    CHECK(az_rev1655_hook_gate_validate_with_import_resolver(
        &image, &resolver_api, &permit) ==
        AZ_REV1655_HOOK_GATE_IMPORT_RESOLUTION_FAILED);
    CHECK(permit == NULL);
    test_resolver.fail_index = (size_t)REV1655_THUNK_COUNT;

    test_resolver.ordinals[23] ^= 1u;
    CHECK(az_rev1655_hook_gate_validate_with_import_resolver(
        &image, &resolver_api, &permit) ==
        AZ_REV1655_HOOK_GATE_IMPORT_RESOLUTION_FAILED);
    CHECK(permit == NULL);
    test_resolver.ordinals[23] ^= 1u;

    test_resolver.targets[31] += 4u;
    CHECK(az_rev1655_hook_gate_validate_with_import_resolver(
        &image, &resolver_api, &permit) ==
        AZ_REV1655_HOOK_GATE_BAD_IMPORT_THUNK);
    CHECK(permit == NULL);
    test_resolver.targets[31] -= 4u;

    first_thunk[0] = 0x61u;
    CHECK(az_rev1655_hook_gate_validate_with_import_resolver(
        &image, &resolver_api, &permit) ==
        AZ_REV1655_HOOK_GATE_BAD_IMPORT_THUNK);
    CHECK(permit == NULL);
    first_thunk[0] = 0x3Du;

    /* Xenia's lis/ori form is intentionally not accepted on real hardware. */
    first_thunk[4] = 0x61u;
    CHECK(az_rev1655_hook_gate_validate_with_import_resolver(
        &image, &resolver_api, &permit) ==
        AZ_REV1655_HOOK_GATE_BAD_IMPORT_THUNK);
    CHECK(permit == NULL);
    first_thunk[4] = 0x39u;

    first_thunk[15] ^= 1u;
    CHECK(az_rev1655_hook_gate_validate_with_import_resolver(
        &image, &resolver_api, &permit) ==
        AZ_REV1655_HOOK_GATE_BAD_IMPORT_THUNK);
    CHECK(permit == NULL);
    first_thunk[15] ^= 1u;

    test_resolver.targets[9] = 0u;
    CHECK(az_rev1655_hook_gate_validate_with_import_resolver(
        &image, &resolver_api, &permit) ==
        AZ_REV1655_HOOK_GATE_IMPORT_RESOLUTION_FAILED);
    CHECK(permit == NULL);
    test_resolver.targets[9] = 0x81004000u + 9u * 0x00000104u;

    CHECK(az_rev1655_hook_gate_validate_with_import_resolver(
        &image, NULL, &permit) ==
        AZ_REV1655_HOOK_GATE_IMPORT_RESOLVER_REQUIRED);
    CHECK(permit == NULL);

    test_resolver.calls = 0u;
    CHECK(az_rev1655_hook_gate_validate_with_import_resolver(
        &image, &resolver_api, &permit) == AZ_REV1655_HOOK_GATE_OK);
    CHECK(permit != NULL);
    CHECK(test_resolver.calls == (size_t)REV1655_THUNK_COUNT);

    free(bytes);
}

static uint8_t *load_file_exact(const char *path, size_t expected_size)
{
    FILE *file = fopen(path, "rb");
    uint8_t *bytes;
    long file_size;

    if (file == NULL || fseek(file, 0L, SEEK_END) != 0) {
        if (file != NULL) {
            fclose(file);
        }
        return NULL;
    }
    file_size = ftell(file);
    if (file_size < 0L || (size_t)file_size != expected_size ||
        fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    bytes = (uint8_t *)malloc(expected_size);
    if (bytes == NULL ||
        fread(bytes, 1u, expected_size, file) != expected_size) {
        free(bytes);
        fclose(file);
        return NULL;
    }
    fclose(file);
    return bytes;
}

static void test_hardware_text_fixture(
    const char *pe_path,
    const char *live_text_path)
{
    uint8_t *image_bytes = load_pe_as_image(pe_path);
    uint8_t *live_text = load_file_exact(
        live_text_path, (size_t)REV1655_TEXT_SIZE);
    TestImportResolver test_resolver;
    AzRev1655ImportResolver resolver_api;
    AzRev1655LoadedImage image;
    const AzRev1655HookPermit *permit = NULL;
    size_t index;
    size_t difference_count = 0u;

    CHECK(image_bytes != NULL);
    CHECK(live_text != NULL);
    if (image_bytes == NULL || live_text == NULL) {
        free(image_bytes);
        free(live_text);
        return;
    }

    CHECK(memcmp(
        image_bytes + REV1655_TEXT_RVA,
        live_text,
        (size_t)REV1655_THUNK_OFFSET) == 0);
    for (index = 0u; index < (size_t)REV1655_TEXT_SIZE; ++index) {
        if (image_bytes[REV1655_TEXT_RVA + index] != live_text[index]) {
            const size_t slot_offset = index >=
                    (size_t)REV1655_THUNK_OFFSET ?
                (index - (size_t)REV1655_THUNK_OFFSET) %
                    (size_t)REV1655_THUNK_SIZE :
                (size_t)REV1655_THUNK_SIZE;

            ++difference_count;
            CHECK(slot_offset < 8u);
        }
    }
    CHECK(difference_count == 2794u);

    /*
     * Fixture-only mirror: this proves the captured opcode/layout/canonical
     * hash path. Production must never derive its authoritative resolver
     * answer from the thunk under validation.
     */
    initialize_test_resolver(&test_resolver);
    for (index = 0u; index < (size_t)REV1655_THUNK_COUNT; ++index) {
        const uint8_t *thunk = live_text + REV1655_THUNK_OFFSET +
            index * (size_t)REV1655_THUNK_SIZE;
        const uint32_t high_word = read_u32_be(thunk);
        const uint32_t low_word = read_u32_be(thunk + 4u);
        const uint32_t low = low_word & 0xFFFFu;
        uint32_t target;

        CHECK((high_word & 0xFFFF0000u) == 0x3D600000u);
        CHECK((low_word & 0xFFFF0000u) == 0x396B0000u);
        CHECK(read_u32_be(thunk + 8u) == 0x7D6903A6u);
        CHECK(read_u32_be(thunk + 12u) == 0x4E800420u);
        target = (high_word & 0xFFFFu) << 16u;
        if (low >= 0x8000u) {
            target -= 0x10000u - low;
        } else {
            target += low;
        }
        test_resolver.targets[index] = target;
    }

    memcpy(
        image_bytes + REV1655_TEXT_RVA,
        live_text,
        (size_t)REV1655_TEXT_SIZE);
    image.bytes = image_bytes;
    image.size = AZ_REV1655_NT_IMAGE_SIZE;
    image.virtual_address = AZ_REV1655_IMAGE_BASE;
    resolver_api = make_resolver_api(&test_resolver);
    CHECK(az_rev1655_hook_gate_validate_with_import_resolver(
        &image, &resolver_api, &permit) == AZ_REV1655_HOOK_GATE_OK);
    CHECK(permit != NULL);

    free(live_text);
    free(image_bytes);
}

int main(int argc, char **argv)
{
    test_sha256();
    test_fail_closed_gate();
    if (argc > 1) {
        test_exact_fixture(argv[1]);
    }
    if (argc > 2) {
        test_hardware_text_fixture(argv[1], argv[2]);
    }

    if (failures != 0) {
        fprintf(stderr, "%d hook-gate assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("Rev1655 strict hook gate tests passed");
    return EXIT_SUCCESS;
}
