#if !defined(AURORAAZ_XBOX360)
#error "netdbg_exports.c must only be built for the Xbox 360 target"
#endif

#include <stdint.h>

/*
 * SynthXEX v0.0.6 does not create an HvImageExportTable.  Reserve the exact
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
 * game:\\Plugins\\NetDbgDll.xex.  The canary deliberately provides the ABI
 * without starting a network service or consuming Aurora's log messages.
 *
 * Aurora ignores the return values at the recovered call sites.  Returning
 * zero keeps this first loader proof inert while ensuring every resolved
 * function pointer is valid.
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
    return 0u;
}

uint32_t AuroraAZNetDbgWrite(const char *message)
{
    (void)message;
    return 0u;
}

uint32_t AuroraAZNetDbgReserved(void)
{
    return 0u;
}
