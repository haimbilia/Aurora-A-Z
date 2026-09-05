#if !defined(AURORAAZ_XBOX360)
#error "netdbg_m2a_exports.c must only be built for the Xbox 360 target"
#endif

#include <stdint.h>

#include <auroraaz/netdbg_bootstrap.h>
#if defined(AURORAAZ_NETDBG_TITLE_EXIT_SHUTDOWN)
#include <auroraaz/rev1655_runtime.h>
#endif

/*
 * SynthXEX v0.0.6 does not create an HvImageExportTable. Reserve the exact
 * mapped, code-page-backed space that scripts/xex_exports.py fills after the
 * PE link and before SynthXEX calculates the XEX page hashes.
 *
 * Four exports require 11 fixed big-endian words plus four function RVAs:
 * (11 + 4) * 4 = 0x3c bytes.
 */
#if defined(__clang__) || defined(__GNUC__)
__attribute__((section(".xexexp"), used, aligned(4)))
#endif
const uint8_t g_auroraaz_xex_export_reserve[0x3Cu] = {0};

/*
 * Aurora Rev1655 resolves NetDbgDll ordinals 2-5 immediately after loading
 * game:\\Plugins\\NetDbgDll.xex. Aurora ignores the return values at every
 * recovered call site, so the compatibility surface always reports success.
 *
 * Ordinal 4 is the deterministic logger-dispatch callback. It is the only
 * entry that starts AuroraAZ; the bootstrap itself is atomically one-shot and
 * returns before any image validation, hook mutation, or logging occurs.
 */
uint32_t AuroraAZNetDbgConfigure(
    uint32_t command_port,
    uint32_t debug_port,
    uint32_t mode)
{
    (void)command_port;
    (void)debug_port;
    (void)mode;
    return 0u;
}

uint32_t AuroraAZNetDbgShutdown(void)
{
#if defined(AURORAAZ_NETDBG_TITLE_EXIT_SHUTDOWN)
    /* Aurora calls ordinal 3 while tearing down its logger for title launch.
     * Never wait on Aurora's caller: the pinned worker owns hook restoration
     * and exits asynchronously before the title process is replaced. */
    az_rev1655_runtime_request_shutdown();
#endif
    return 0u;
}

uint32_t AuroraAZNetDbgWrite(const char *message)
{
    (void)message;
    (void)AuroraAZNetDbgBootstrapStart();
    return 0u;
}

uint32_t AuroraAZNetDbgReserved(void)
{
#if defined(AURORAAZ_BOOT_PROBE)
    extern uint32_t AuroraAZProbeStatusAddress(void);
    return AuroraAZProbeStatusAddress();
#else
    return 0u;
#endif
}
