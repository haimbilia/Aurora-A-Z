#include <stdio.h>

#include <auroraaz/content_launch_detour.h>

static unsigned int failures;
static unsigned int request_calls;
static unsigned int shutdown_calls;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

void az_rev1655_runtime_request_shutdown(void)
{
    ++request_calls;
}

void az_rev1655_runtime_shutdown(void)
{
    ++shutdown_calls;
}

int main(void)
{
    az_rev1655_content_launch_detour_c();
    CHECK(request_calls == 1u);
    CHECK(shutdown_calls == 0u);

    az_rev1655_content_launch_detour_c();
    CHECK(request_calls == 2u);
    CHECK(shutdown_calls == 0u);

    if (failures != 0u) {
        fprintf(stderr, "%u content-launch test(s) failed\n", failures);
        return 1;
    }
    puts("content-launch detour tests passed");
    return 0;
}
