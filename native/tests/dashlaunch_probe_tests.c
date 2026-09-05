#include <stdint.h>
#include <stdio.h>
#include <auroraaz/dashlaunch_probe.h>
int DllMain(void *, uint32_t, void *);
int main(void)
{
    if (g_az_dashlaunch_probe.magic != 0x415A4230u ||
        g_az_dashlaunch_probe.calls != 0u) return 1;
    if (DllMain((void *)(uintptr_t)0x91D00000u, 1u, NULL) != 1 ||
        g_az_dashlaunch_probe.attach_seen != 1u ||
        g_az_dashlaunch_probe.module != 0x91D00000u) return 2;
    if (DllMain(NULL, 0u, NULL) != 1 ||
        g_az_dashlaunch_probe.last_reason != 0u ||
        g_az_dashlaunch_probe.attach_seen != 1u ||
        g_az_dashlaunch_probe.calls != 2u) return 3;
    puts("DashLaunch memory-only probe tests passed");
    return 0;
}
