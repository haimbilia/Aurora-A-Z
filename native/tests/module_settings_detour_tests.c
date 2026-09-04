#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
    az_module_settings_detour_reset();
    CHECK(az_module_settings_detour_take_request() == 0u);
    az_rev1655_module_settings_detour_c();
    CHECK(az_module_settings_detour_take_request() == 1u);
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
