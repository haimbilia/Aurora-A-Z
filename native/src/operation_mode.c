#include <string.h>

#include <auroraaz/operation_mode.h>

static const char k_browse_config[] = "version=1\nmode=browse\n";
static const char k_filter_config[] = "version=1\nmode=filter\n";

AzOperationMode az_operation_mode_sanitize(uint32_t value)
{
    return value == (uint32_t)AZ_OPERATION_MODE_FILTER ?
        AZ_OPERATION_MODE_FILTER : AZ_OPERATION_MODE_BROWSE;
}

const char *az_operation_mode_name(AzOperationMode mode)
{
    return az_operation_mode_sanitize((uint32_t)mode) ==
        AZ_OPERATION_MODE_FILTER ? "Filter" : "Browse";
}

AzOperationMode az_operation_mode_parse(
    const uint8_t *bytes,
    size_t size)
{
    const size_t filter_size = sizeof(k_filter_config) - 1u;
    const size_t browse_size = sizeof(k_browse_config) - 1u;

    if (bytes == NULL) {
        return AZ_OPERATION_MODE_BROWSE;
    }
    if (size == filter_size &&
        memcmp(bytes, k_filter_config, filter_size) == 0) {
        return AZ_OPERATION_MODE_FILTER;
    }
    if (size == browse_size &&
        memcmp(bytes, k_browse_config, browse_size) == 0) {
        return AZ_OPERATION_MODE_BROWSE;
    }
    return AZ_OPERATION_MODE_BROWSE;
}

size_t az_operation_mode_serialize(
    AzOperationMode mode,
    char *destination,
    size_t capacity)
{
    const char *source;
    size_t size;

    source = az_operation_mode_sanitize((uint32_t)mode) ==
        AZ_OPERATION_MODE_FILTER ? k_filter_config : k_browse_config;
    size = strlen(source);
    if (destination == NULL || capacity < size) {
        return 0u;
    }
    memcpy(destination, source, size);
    return size;
}
