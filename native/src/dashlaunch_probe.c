#include <stddef.h>
#include <stdint.h>
#include <auroraaz/dashlaunch_probe.h>

/* No kernel calls, threads, waits, file I/O, Aurora access or hooks. The
 * dedicated section lets a later external reader inspect loader entry. */
__attribute__((section(".azboot"), used, aligned(4)))
volatile AzDashlaunchProbe g_az_dashlaunch_probe = {
    0x415A4230u, 1u, 0u, 0xFFFFFFFFu, 0u, 0u
};

/* The common export table is present only to reuse the validated packager.
 * None of its exports activate any runtime code. */
uint32_t AuroraAZNetDbgBootstrapStart(void) { return 0u; }
uint32_t AuroraAZProbeStatusAddress(void)
{
    return (uint32_t)(uintptr_t)&g_az_dashlaunch_probe;
}

int DllMain(void *module, uint32_t reason, void *reserved)
{
    (void)reserved;
    ++g_az_dashlaunch_probe.calls;
    g_az_dashlaunch_probe.last_reason = reason;
    g_az_dashlaunch_probe.module = (uint32_t)(uintptr_t)module;
    if (reason == 1u) g_az_dashlaunch_probe.attach_seen = 1u;
    return 1;
}
