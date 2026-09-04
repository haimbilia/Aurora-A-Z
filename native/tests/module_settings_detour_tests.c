#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <auroraaz/module_settings_detour.h>

static int failures = 0;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

int main(void)
{
    AzModuleSettingsDetourStatus status;
    uint8_t wstring_object[0x1Cu];
    uint16_t storage[17];
    uint32_t size;
    uint32_t capacity = 16u;
    static const uint16_t expected[] = {
        'A', 'u', 'r', 'o', 'r', 'a', ' ', 'A', '-', 'Z', 0
    };

    memset(wstring_object, 0, sizeof(wstring_object));
    memset(storage, 0x7f, sizeof(storage));
    memcpy(wstring_object + 0x14u, &capacity, sizeof(capacity));
    CHECK(az_module_settings_write_label(
        wstring_object, storage, 17u) == 1u);
    memcpy(&size, wstring_object + 0x10u, sizeof(size));
    CHECK(size == AZ_MODULE_SETTINGS_LABEL_LENGTH);
    CHECK(memcmp(storage, expected, sizeof(expected)) == 0);

    capacity = 9u;
    memcpy(wstring_object + 0x14u, &capacity, sizeof(capacity));
    CHECK(az_module_settings_write_label(
        wstring_object, storage, 17u) == 0u);
    capacity = 16u;
    memcpy(wstring_object + 0x14u, &capacity, sizeof(capacity));
    CHECK(az_module_settings_write_label(
        wstring_object, storage, 10u) == 0u);

    az_module_settings_detour_reset();
    az_module_settings_detour_snapshot_status(&status);
    CHECK(status.hook_calls == 0u);
    CHECK(status.requests_taken == 0u);
    CHECK(status.pending == 0u);
    CHECK(status.disabled == 0u);
    CHECK(az_module_settings_detour_take_request() == 0u);
    az_rev1655_module_settings_detour_c();
    az_module_settings_detour_snapshot_status(&status);
    CHECK(status.hook_calls == 1u);
    CHECK(status.pending == 1u);
    CHECK(az_module_settings_detour_take_request() == 1u);
    az_module_settings_detour_snapshot_status(&status);
    CHECK(status.requests_taken == 1u);
    CHECK(az_module_settings_detour_take_request() == 0u);

    /* Repeated opens coalesce into one worker request. Aurora's original
     * task completion tail is not replaced by this bridge. */
    az_rev1655_module_settings_detour_c();
    az_rev1655_module_settings_detour_c();
    CHECK(az_module_settings_detour_take_request() == 1u);
    CHECK(az_module_settings_detour_take_request() == 0u);

    az_module_settings_detour_begin_shutdown();
    az_rev1655_module_settings_detour_c();
    CHECK(az_module_settings_detour_take_request() == 0u);

    if (failures != 0) {
        fprintf(stderr, "%d module settings assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("AuroraAZ module settings detour tests passed");
    return EXIT_SUCCESS;
}
