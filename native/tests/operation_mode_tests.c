#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <auroraaz/operation_mode.h>

int main(void)
{
    char bytes[AZ_OPERATION_MODE_CONFIG_MAX_SIZE];
    size_t size;

    assert(az_operation_mode_sanitize(0u) == AZ_OPERATION_MODE_BROWSE);
    assert(az_operation_mode_sanitize(1u) == AZ_OPERATION_MODE_FILTER);
    assert(az_operation_mode_sanitize(2u) == AZ_OPERATION_MODE_BROWSE);
    assert(strcmp(
        az_operation_mode_name(AZ_OPERATION_MODE_BROWSE), "Browse") == 0);
    assert(strcmp(
        az_operation_mode_name(AZ_OPERATION_MODE_FILTER), "Filter") == 0);

    size = az_operation_mode_serialize(
        AZ_OPERATION_MODE_FILTER, bytes, sizeof(bytes));
    assert(size == strlen("version=1\nmode=filter\n"));
    assert(az_operation_mode_parse(
        (const uint8_t *)bytes, size) == AZ_OPERATION_MODE_FILTER);

    size = az_operation_mode_serialize(
        AZ_OPERATION_MODE_BROWSE, bytes, sizeof(bytes));
    assert(size == strlen("version=1\nmode=browse\n"));
    assert(az_operation_mode_parse(
        (const uint8_t *)bytes, size) == AZ_OPERATION_MODE_BROWSE);

    assert(az_operation_mode_serialize(
        AZ_OPERATION_MODE_FILTER, bytes, 4u) == 0u);
    assert(az_operation_mode_parse(NULL, 0u) == AZ_OPERATION_MODE_BROWSE);
    assert(az_operation_mode_parse(
        (const uint8_t *)"version=1\nmode=unknown\n",
        strlen("version=1\nmode=unknown\n")) == AZ_OPERATION_MODE_BROWSE);
    assert(az_operation_mode_parse(
        (const uint8_t *)"version=1\nmode=filter",
        strlen("version=1\nmode=filter")) == AZ_OPERATION_MODE_BROWSE);
    return 0;
}
