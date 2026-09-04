#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <auroraaz/compatibility.h>
#include <auroraaz/filters.h>
#include <auroraaz/image.h>
#include <auroraaz/layout.h>
#include <auroraaz/selector.h>

#define ENTRY_OFFSET 0x005F50E0u
#define PLUGIN_MANAGER_OFFSET 0x0017CBF8u
#define MODULE_LOADER_OFFSET 0x00178B50u

static int failures = 0;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

static const uint8_t k_entry_probe[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x48, 0x16, 0x2B, 0xE5,
    0x3B, 0xE1, 0xFE, 0x10, 0x94, 0x21, 0xFE, 0x10
};

static const uint8_t k_plugin_manager_probe[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x48, 0x5D, 0xB0, 0xD1,
    0x3B, 0xE1, 0xFF, 0x60, 0x94, 0x21, 0xFF, 0x60
};

static const uint8_t k_module_loader_probe[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x48, 0x5D, 0xF1, 0x5D,
    0x94, 0x21, 0xFF, 0x30, 0x81, 0x63, 0x00, 0x34
};

static void write_u16_le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)(value >> 8u);
}

static void write_u32_le(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8u) & 0xFFu);
    bytes[2] = (uint8_t)((value >> 16u) & 0xFFu);
    bytes[3] = (uint8_t)(value >> 24u);
}

static uint8_t *make_rev1655_image(void)
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
    write_u32_le(text_section + 8u, AZ_REV1655_TEXT_SIZE);
    write_u32_le(text_section + 12u, 0x00210000u);

    return image;
}

static void test_selector_flow(void)
{
    AzSelectorState state;
    AzSelectorResult result;

    az_selector_init(&state);
    CHECK(state.mode == AZ_MODE_COVERFLOW);
    CHECK(state.selected_index == 0u);
    CHECK(state.applied_index == AZ_NO_GLYPH);

    result = az_selector_dispatch(
        &state, AZ_COMMAND_NEXT, AZ_EDGE_CLAMP, 1u);
    CHECK(result.handled == 0u);
    CHECK(state.mode == AZ_MODE_COVERFLOW);

    result = az_selector_dispatch(
        &state, AZ_COMMAND_ENTER, AZ_EDGE_CLAMP, 1u);
    CHECK(result.handled == 1u);
    CHECK(state.mode == AZ_MODE_SELECTING);
    CHECK(state.selected_index == 0u);
    CHECK(state.selection_changed == 0u);

    result = az_selector_dispatch(
        &state, AZ_COMMAND_APPLY, AZ_EDGE_CLAMP, 1u);
    CHECK(result.handled == 1u);
    CHECK(result.request_filter == 0u);
    CHECK(state.mode == AZ_MODE_COVERFLOW);
    CHECK(state.applied_index == AZ_NO_GLYPH);

    result = az_selector_dispatch(
        &state, AZ_COMMAND_ENTER, AZ_EDGE_CLAMP, 1u);
    CHECK(result.handled == 1u);

    result = az_selector_dispatch(
        &state, AZ_COMMAND_NEXT, AZ_EDGE_CLAMP, 1u);
    CHECK(result.handled == 1u);
    CHECK(state.selected_index == 1u);
    CHECK(state.selection_changed == 1u);

    result = az_selector_dispatch(
        &state, AZ_COMMAND_APPLY, AZ_EDGE_CLAMP, 1u);
    CHECK(result.handled == 1u);
    CHECK(result.request_filter == 1u);
    CHECK(result.filter_index == 1u);
    CHECK(state.mode == AZ_MODE_COVERFLOW);
    CHECK(state.applied_index == 1u);
    CHECK(state.selection_changed == 0u);

    result = az_selector_dispatch(
        &state, AZ_COMMAND_ENTER, AZ_EDGE_CLAMP, 0u);
    CHECK(result.handled == 0u);
    CHECK(state.mode == AZ_MODE_COVERFLOW);
}

static void test_selector_fail_closed(void)
{
    AzSelectorState state;
    AzSelectorResult result;

    result = az_selector_dispatch(
        NULL, AZ_COMMAND_APPLY, AZ_EDGE_CLAMP, 1u);
    CHECK(result.handled == 0u);
    CHECK(result.request_filter == 0u);
    CHECK(result.filter_index == AZ_NO_GLYPH);

    az_selector_init(&state);
    state.mode = (AzSelectorMode)99;
    state.selected_index = 1u;
    result = az_selector_dispatch(
        &state, AZ_COMMAND_APPLY, AZ_EDGE_CLAMP, 1u);
    CHECK(result.handled == 0u);
    CHECK(result.request_filter == 0u);
    CHECK(state.mode == (AzSelectorMode)99);

    state.mode = AZ_MODE_SELECTING;
    state.selected_index = AZ_GLYPH_COUNT;
    result = az_selector_dispatch(
        &state, AZ_COMMAND_APPLY, AZ_EDGE_CLAMP, 1u);
    CHECK(result.handled == 0u);
    CHECK(result.request_filter == 0u);
    CHECK(state.applied_index == AZ_NO_GLYPH);

    result = az_selector_dispatch(
        &state, AZ_COMMAND_NEXT, AZ_EDGE_CLAMP, 0u);
    CHECK(result.handled == 0u);
    CHECK(state.mode == AZ_MODE_COVERFLOW);
    CHECK(state.selected_index == 0u);
}

static void test_edge_behavior(void)
{
    AzSelectorState state;
    unsigned int index;

    az_selector_init(&state);
    (void)az_selector_dispatch(&state, AZ_COMMAND_ENTER, AZ_EDGE_CLAMP, 1u);
    (void)az_selector_dispatch(&state, AZ_COMMAND_PREVIOUS, AZ_EDGE_CLAMP, 1u);
    CHECK(state.selected_index == 0u);

    (void)az_selector_dispatch(&state, AZ_COMMAND_PREVIOUS, AZ_EDGE_WRAP, 1u);
    CHECK(state.selected_index == AZ_GLYPH_COUNT - 1u);

    (void)az_selector_dispatch(&state, AZ_COMMAND_NEXT, AZ_EDGE_CLAMP, 1u);
    CHECK(state.selected_index == AZ_GLYPH_COUNT - 1u);

    (void)az_selector_dispatch(&state, AZ_COMMAND_NEXT, AZ_EDGE_WRAP, 1u);
    CHECK(state.selected_index == 0u);

    for (index = 0u; index < 12u; ++index) {
        (void)az_selector_dispatch(&state, AZ_COMMAND_NEXT, AZ_EDGE_CLAMP, 1u);
    }
    CHECK(state.selected_index == 12u);

    az_selector_leave_coverflow(&state);
    CHECK(state.mode == AZ_MODE_COVERFLOW);
    CHECK(state.selected_index == 0u);
}

static void test_filter_mapping(void)
{
    static const char *const expected_filters[AZ_GLYPH_COUNT] = {
        NULL,
        "NameFilter.Other",
        "NameFilter.A - F.A",
        "NameFilter.A - F.B",
        "NameFilter.A - F.C",
        "NameFilter.A - F.D",
        "NameFilter.A - F.E",
        "NameFilter.A - F.F",
        "NameFilter.G - L.G",
        "NameFilter.G - L.H",
        "NameFilter.G - L.I",
        "NameFilter.G - L.J",
        "NameFilter.G - L.K",
        "NameFilter.G - L.L",
        "NameFilter.M - R.M",
        "NameFilter.M - R.N",
        "NameFilter.M - R.O",
        "NameFilter.M - R.P",
        "NameFilter.M - R.Q",
        "NameFilter.M - R.R",
        "NameFilter.S - X.S",
        "NameFilter.S - X.T",
        "NameFilter.S - X.U",
        "NameFilter.S - X.V",
        "NameFilter.S - X.W",
        "NameFilter.S - X.X",
        "NameFilter.Y - Z.Y",
        "NameFilter.Y - Z.Z"
    };
    uint8_t index;

    CHECK(az_glyph_for_index(0u) == '\0');
    CHECK(az_glyph_for_index(1u) == '#');
    CHECK(az_glyph_for_index(2u) == 'A');
    CHECK(az_glyph_for_index(27u) == 'Z');
    CHECK(az_glyph_for_index(28u) == '\0');
    CHECK(strcmp(az_label_for_index(0u), "ALL") == 0);
    CHECK(strcmp(az_label_for_index(1u), "#") == 0);
    CHECK(strcmp(az_label_for_index(27u), "Z") == 0);
    CHECK(az_label_for_index(28u) == NULL);

    CHECK(az_filter_method_for_index(0u) == NULL);
    CHECK(strcmp(az_filter_method_for_index(1u), "NameFilter.Other") == 0);
    CHECK(strcmp(az_filter_method_for_index(2u), "NameFilter.A - F.A") == 0);
    CHECK(strcmp(az_filter_method_for_index(7u), "NameFilter.A - F.F") == 0);
    CHECK(strcmp(az_filter_method_for_index(8u), "NameFilter.G - L.G") == 0);
    CHECK(strcmp(az_filter_method_for_index(14u), "NameFilter.M - R.M") == 0);
    CHECK(strcmp(az_filter_method_for_index(20u), "NameFilter.S - X.S") == 0);
    CHECK(strcmp(az_filter_method_for_index(26u), "NameFilter.Y - Z.Y") == 0);
    CHECK(strcmp(az_filter_method_for_index(27u), "NameFilter.Y - Z.Z") == 0);
    CHECK(az_filter_method_for_index(28u) == NULL);

    CHECK(az_filter_index_for_method("NameFilter.Other") == 1u);
    CHECK(az_filter_index_for_method("NameFilter.A - F.A") == 2u);
    CHECK(az_filter_index_for_method("NameFilter.Y - Z.Z") == 27u);
    CHECK(az_filter_index_for_method("NameFilter.A - F.a") == AZ_NO_GLYPH);
    CHECK(az_filter_index_for_method("NameFilter.A - F.A.extra") ==
        AZ_NO_GLYPH);
    CHECK(az_filter_index_for_method("") == AZ_NO_GLYPH);
    CHECK(az_filter_index_for_method(NULL) == AZ_NO_GLYPH);

    CHECK(expected_filters[AZ_FILTER_ALL_INDEX] == NULL);
    for (index = AZ_FILTER_OTHER_INDEX; index < AZ_GLYPH_COUNT; ++index) {
        CHECK(az_filter_method_for_index(index) != NULL);
        CHECK(strcmp(
            az_filter_method_for_index(index),
            expected_filters[index]) == 0);
    }
}

static void test_layout(void)
{
    const AzVisualStyle *style = az_mockup_visual_style();
    uint8_t index;

    CHECK(style->logical_width == 1280.0f);
    CHECK(style->logical_height == 720.0f);
    CHECK(style->row_center_x == 640.0f);
    CHECK(style->glyph_pitch == 34.25f);
    CHECK(style->baseline_y == 372.0f);
    CHECK(style->em_size == 36.0f);
    CHECK(style->shadow_offset_x == 2.0f);
    CHECK(style->shadow_offset_y == 2.0f);
    CHECK(style->inactive_alpha == 216u);
    CHECK(style->selected_alpha == 255u);
    CHECK(style->shadow_alpha == 176u);
    CHECK(az_glyph_center_x(0u) + az_glyph_center_x(27u) == 1280.0f);
    CHECK(az_glyph_center_x(28u) == 640.0f);

    for (index = 1u; index < AZ_GLYPH_COUNT; ++index) {
        CHECK(
            az_glyph_center_x(index) - az_glyph_center_x((uint8_t)(index - 1u)) ==
            style->glyph_pitch);
    }
}

static void test_compatibility_gate(void)
{
    uint8_t *text = (uint8_t *)calloc(AZ_REV1655_TEXT_SIZE, 1u);

    CHECK(text != NULL);
    if (text == NULL) {
        return;
    }

    memcpy(text + ENTRY_OFFSET, k_entry_probe, sizeof(k_entry_probe));
    memcpy(
        text + PLUGIN_MANAGER_OFFSET,
        k_plugin_manager_probe,
        sizeof(k_plugin_manager_probe));
    memcpy(
        text + MODULE_LOADER_OFFSET,
        k_module_loader_probe,
        sizeof(k_module_loader_probe));

    CHECK(az_validate_rev1655_text(
        text, AZ_REV1655_TEXT_SIZE, AZ_REV1655_TEXT_BASE) ==
        AZ_COMPATIBLE_REV1655);
    CHECK(az_validate_rev1655_text(
        text, AZ_REV1655_TEXT_SIZE - 1u, AZ_REV1655_TEXT_BASE) ==
        AZ_COMPAT_BAD_TEXT_SIZE);
    CHECK(az_validate_rev1655_text(
        text, AZ_REV1655_TEXT_SIZE, AZ_REV1655_TEXT_BASE + 4u) ==
        AZ_COMPAT_BAD_TEXT_BASE);

    CHECK(az_validate_rev1655_text(
        NULL, AZ_REV1655_TEXT_SIZE, AZ_REV1655_TEXT_BASE) ==
        AZ_COMPAT_BAD_TEXT_BASE);

    text[ENTRY_OFFSET] ^= 0x01u;
    CHECK(az_validate_rev1655_text(
        text, AZ_REV1655_TEXT_SIZE, AZ_REV1655_TEXT_BASE) ==
        AZ_COMPAT_BAD_ENTRY_PROBE);
    text[ENTRY_OFFSET] ^= 0x01u;

    text[PLUGIN_MANAGER_OFFSET] ^= 0x01u;
    CHECK(az_validate_rev1655_text(
        text, AZ_REV1655_TEXT_SIZE, AZ_REV1655_TEXT_BASE) ==
        AZ_COMPAT_BAD_PLUGIN_MANAGER_PROBE);
    text[PLUGIN_MANAGER_OFFSET] ^= 0x01u;

    text[MODULE_LOADER_OFFSET] ^= 0x01u;
    CHECK(az_validate_rev1655_text(
        text, AZ_REV1655_TEXT_SIZE, AZ_REV1655_TEXT_BASE) ==
        AZ_COMPAT_BAD_MODULE_LOADER_PROBE);

    free(text);
}

static void test_compatibility_result_names(void)
{
    CHECK(strcmp(
        az_compatibility_result_name(AZ_COMPATIBLE_REV1655),
        "rev1655") == 0);
    CHECK(strcmp(
        az_compatibility_result_name(AZ_COMPAT_BAD_TEXT_BASE),
        "bad-text-base") == 0);
    CHECK(strcmp(
        az_compatibility_result_name(AZ_COMPAT_BAD_TEXT_SIZE),
        "bad-text-size") == 0);
    CHECK(strcmp(
        az_compatibility_result_name(AZ_COMPAT_BAD_ENTRY_PROBE),
        "bad-entry-probe") == 0);
    CHECK(strcmp(
        az_compatibility_result_name(AZ_COMPAT_BAD_PLUGIN_MANAGER_PROBE),
        "bad-plugin-manager-probe") == 0);
    CHECK(strcmp(
        az_compatibility_result_name(AZ_COMPAT_BAD_MODULE_LOADER_PROBE),
        "bad-module-loader-probe") == 0);
    CHECK(strcmp(
        az_compatibility_result_name((AzCompatibilityResult)99),
        "unknown") == 0);
}

static void test_image_gate(void)
{
    uint8_t *image = make_rev1655_image();
    const uint8_t *text = NULL;
    size_t text_size = 0u;

    CHECK(image != NULL);
    if (image == NULL) {
        return;
    }

    CHECK(az_locate_rev1655_text(
        image, AZ_REV1655_NT_IMAGE_SIZE, &text, &text_size) ==
        AZ_IMAGE_REV1655);
    CHECK(text == image + 0x00210000u);
    CHECK(text_size == AZ_REV1655_TEXT_SIZE);

    CHECK(az_locate_rev1655_text(
        NULL, AZ_REV1655_NT_IMAGE_SIZE, &text, &text_size) ==
        AZ_IMAGE_NULL);
    CHECK(text == NULL);
    CHECK(text_size == 0u);

    CHECK(az_locate_rev1655_text(
        image, AZ_REV1655_NT_IMAGE_SIZE - 1u, NULL, NULL) ==
        AZ_IMAGE_BAD_SIZE);

    image[0] = 0u;
    CHECK(az_locate_rev1655_text(
        image, AZ_REV1655_NT_IMAGE_SIZE, NULL, NULL) ==
        AZ_IMAGE_BAD_DOS_HEADER);
    image[0] = (uint8_t)'M';

    image[0xF8u] = 0u;
    CHECK(az_locate_rev1655_text(
        image, AZ_REV1655_NT_IMAGE_SIZE, NULL, NULL) ==
        AZ_IMAGE_BAD_NT_HEADER);
    image[0xF8u] = (uint8_t)'P';

    image[0xFCu] ^= 0x01u;
    CHECK(az_locate_rev1655_text(
        image, AZ_REV1655_NT_IMAGE_SIZE, NULL, NULL) ==
        AZ_IMAGE_BAD_MACHINE);
    image[0xFCu] ^= 0x01u;

    image[0xFEu] = 8u;
    CHECK(az_locate_rev1655_text(
        image, AZ_REV1655_NT_IMAGE_SIZE, NULL, NULL) ==
        AZ_IMAGE_BAD_OPTIONAL_HEADER);
    image[0xFEu] = 9u;

    image[0x120u] ^= 0x01u;
    CHECK(az_locate_rev1655_text(
        image, AZ_REV1655_NT_IMAGE_SIZE, NULL, NULL) ==
        AZ_IMAGE_BAD_IDENTITY);
    image[0x120u] ^= 0x01u;

    image[0x240u] = (uint8_t)'x';
    CHECK(az_locate_rev1655_text(
        image, AZ_REV1655_NT_IMAGE_SIZE, NULL, NULL) ==
        AZ_IMAGE_BAD_TEXT_SECTION);
    image[0x240u] = (uint8_t)'.';

    CHECK(strcmp(az_image_result_name(AZ_IMAGE_REV1655),
        "rev1655-image") == 0);
    CHECK(strcmp(az_image_result_name(AZ_IMAGE_BAD_TEXT_SECTION),
        "bad-text-section") == 0);
    CHECK(strcmp(az_image_result_name((AzImageResult)99),
        "unknown-image") == 0);

    free(image);
}

int main(void)
{
    test_selector_flow();
    test_selector_fail_closed();
    test_edge_behavior();
    test_filter_mapping();
    test_layout();
    test_compatibility_gate();
    test_compatibility_result_names();
    test_image_gate();

    if (failures != 0) {
        fprintf(stderr, "%d test assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }

    puts("AuroraAZ core tests passed");
    return EXIT_SUCCESS;
}
