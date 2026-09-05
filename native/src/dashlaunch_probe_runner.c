/* On-demand loader test. Never edits launch.ini or installs a boot plugin. */
#include <stddef.h>
#include <stdint.h>
#include <xecore/xam.h>
#include <xecore/xboxkrnl.h>
#include <auroraaz/dashlaunch_probe.h>

typedef struct ProbeReport {
    uint32_t magic, version, phase, load_status, export_status;
    AzDashlaunchProbe state;
} ProbeReport;
static ProbeReport report = {0x415A5230u, 1u, 0u, 0xFFFFFFFFu, 0xFFFFFFFFu,
    {0u, 0u, 0u, 0u, 0u, 0u}};
static char report_path[] = "game:\\probe-result.bin";
static char probe_path[] = "game:\\AuroraAZ-boot-probe.xex";
static char aurora_path[] = "Hdd:\\Aurora\\Aurora.xex";

static int save_report(void)
{
    uint32_t written = 0u;
    HANDLE file = CreateFileA(report_path, GENERIC_WRITE, FILE_SHARE_READ,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == NULL || file == INVALID_HANDLE_VALUE) return 0;
    (void)WriteFile(file, &report, sizeof(report), &written, NULL);
    (void)CloseHandle(file);
    return written == sizeof(report);
}

int main(void)
{
    HMODULE module = NULL;
    void *function = NULL;
    report.phase = 1u;
    /* Require a durable pre-load marker before invoking the loader. */
    if (!save_report()) return 1;
    report.load_status = (uint32_t)XexLoadImage(probe_path, 0xAu, 0u, &module);
    report.phase = 2u;
    (void)save_report();
    if ((int32_t)report.load_status >= 0 && module != NULL) {
        report.export_status = (uint32_t)XexGetProcedureAddress(module, 5u, &function);
        if ((int32_t)report.export_status >= 0 && function != NULL) {
            uint32_t address = ((uint32_t (*)(void))(uintptr_t)function)();
            const volatile AzDashlaunchProbe *state =
                (const volatile AzDashlaunchProbe *)(uintptr_t)address;
            if (MmIsAddressValid((void *)(uintptr_t)address) &&
                MmIsAddressValid((void *)(uintptr_t)(address + sizeof(*state) - 1u)) &&
                state->magic == 0x415A4230u) {
                report.state = *state;
                report.phase = 3u;
            }
        }
        (void)save_report();
        (void)XexUnloadImage(module);
    }
    XamLoaderLaunchTitle(aurora_path, 0u);
    return 0;
}
