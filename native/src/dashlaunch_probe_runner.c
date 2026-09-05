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
static char report_path[] = "\\Device\\Harddisk0\\Partition1\\AuroraAZProbe\\probe-result.bin";
static char probe_path[] = "game:\\AuroraAZ-boot-probe.xex";
static char aurora_path[] = "Hdd:\\Aurora\\Aurora.xex";

static int save_report(void)
{
    HANDLE file = NULL;
    ANSI_STRING name = {(uint16_t)(sizeof(report_path) - 1u),
        (uint16_t)sizeof(report_path), report_path};
    OBJECT_ATTRIBUTES attributes = {0u, &name, OBJ_CASE_INSENSITIVE};
    IO_STATUS_BLOCK io = {0};
    int64_t offset = 0;
    /* Use a physical device path and synchronous kernel I/O. This does not
     * depend on XAM's Win32 file wrappers or the current game: mount. */
    NTSTATUS status = NtCreateFile(&file, 0x40100000u, &attributes, &io,
        NULL, 0x80u, 3u, 5u, 0x60u);
    if ((int32_t)status < 0 || file == NULL) return 0;
    status = NtWriteFile(file, NULL, NULL, NULL, &io,
        &report, (uint32_t)sizeof(report), &offset);
    if ((int32_t)status >= 0) (void)NtFlushBuffersFile(file, &io);
    (void)NtClose(file);
    return (int32_t)status >= 0;
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
