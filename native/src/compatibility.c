#include <auroraaz/compatibility.h>

#define AZ_REV1655_ENTRY_OFFSET 0x005F50E0u
#define AZ_REV1655_PLUGIN_MANAGER_OFFSET 0x0017CBF8u
#define AZ_REV1655_MODULE_LOADER_OFFSET 0x00178B50u

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

static int bytes_equal(const uint8_t *actual, const uint8_t *expected, size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        if (actual[index] != expected[index]) {
            return 0;
        }
    }

    return 1;
}

AzCompatibilityResult az_validate_rev1655_text(
    const uint8_t *text,
    size_t text_size,
    uint32_t text_virtual_address)
{
    if (text == NULL || text_virtual_address != AZ_REV1655_TEXT_BASE) {
        return AZ_COMPAT_BAD_TEXT_BASE;
    }

    if (text_size != (size_t)AZ_REV1655_TEXT_SIZE) {
        return AZ_COMPAT_BAD_TEXT_SIZE;
    }

    if (!bytes_equal(
            text + AZ_REV1655_ENTRY_OFFSET,
            k_entry_probe,
            sizeof(k_entry_probe))) {
        return AZ_COMPAT_BAD_ENTRY_PROBE;
    }

    if (!bytes_equal(
            text + AZ_REV1655_PLUGIN_MANAGER_OFFSET,
            k_plugin_manager_probe,
            sizeof(k_plugin_manager_probe))) {
        return AZ_COMPAT_BAD_PLUGIN_MANAGER_PROBE;
    }

    if (!bytes_equal(
            text + AZ_REV1655_MODULE_LOADER_OFFSET,
            k_module_loader_probe,
            sizeof(k_module_loader_probe))) {
        return AZ_COMPAT_BAD_MODULE_LOADER_PROBE;
    }

    return AZ_COMPATIBLE_REV1655;
}

const char *az_compatibility_result_name(AzCompatibilityResult result)
{
    switch (result) {
    case AZ_COMPATIBLE_REV1655:
        return "rev1655";
    case AZ_COMPAT_BAD_TEXT_BASE:
        return "bad-text-base";
    case AZ_COMPAT_BAD_TEXT_SIZE:
        return "bad-text-size";
    case AZ_COMPAT_BAD_ENTRY_PROBE:
        return "bad-entry-probe";
    case AZ_COMPAT_BAD_PLUGIN_MANAGER_PROBE:
        return "bad-plugin-manager-probe";
    case AZ_COMPAT_BAD_MODULE_LOADER_PROBE:
        return "bad-module-loader-probe";
    default:
        return "unknown";
    }
}
