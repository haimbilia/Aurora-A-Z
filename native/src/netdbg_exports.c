#if !defined(AURORAAZ_XBOX360)
#error "netdbg_exports.c must only be built for the Xbox 360 target"
#endif

#include <stdint.h>

#include <xecore/xam.h>

#include <auroraaz/canary.h>

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

typedef struct AzM1Record {
    uint8_t magic[4];
    uint32_t version;
    uint32_t record_size;
    uint32_t call_count;
    uint32_t source_ordinal;
    uint32_t phase;
    uint32_t state;
    uint32_t ex_create_thread_status;
    uint32_t nt_resume_thread_status;
} AzM1Record;

typedef char AzM1RecordMustBe36Bytes[
    (sizeof(AzM1Record) == 36u) ? 1 : -1];

#define AZ_M1_SOURCE_CONFIGURE_ORDINAL 2u
#define AZ_M1_SOURCE_WRITE_ORDINAL 4u

static char g_m1_marker_path[] =
    "game:\\Data\\Logs\\AuroraAZ-M1.bin";
static char g_m1_worker_marker_path[] =
    "game:\\Data\\Logs\\AuroraAZ-M1-worker.bin";
static uint32_t g_m1_start_claimed = 0u;
static AzM1Record g_m1_identity;

static void write_m1_record(
    char *path,
    const AzM1Record *record)
{
    HANDLE file;
    uint32_t bytes_written = 0u;

    file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == NULL || file == INVALID_HANDLE_VALUE) {
        return;
    }

    (void)WriteFile(
        file,
        (void *)record,
        (uint32_t)sizeof(*record),
        &bytes_written,
        NULL);
    (void)CloseHandle(file);
}

static void update_m1_record(
    const AzCanaryStartSnapshot *snapshot,
    void *context)
{
    const AzM1Record *identity =
        (const AzM1Record *)context;
    AzM1Record record = *identity;
    char *path = g_m1_marker_path;

    record.phase = snapshot->phase;
    record.state = snapshot->state;
    record.ex_create_thread_status =
        snapshot->ex_create_thread_status;
    record.nt_resume_thread_status =
        snapshot->nt_resume_thread_status;
    if (snapshot->phase == AZ_CANARY_START_WORKER_ENTERED) {
        path = g_m1_worker_marker_path;
    }
    write_m1_record(path, &record);
}

static void try_start_m1(uint32_t source_ordinal)
{
    uint32_t expected = 0u;
    const AzM1Record marker = {
        {'A', 'Z', 'M', '1'},
        3u,
        (uint32_t)sizeof(AzM1Record),
        1u,
        source_ordinal,
        AZ_CANARY_START_ORDINAL_ENTRY,
        AZ_CANARY_MONITOR_STOPPED,
        AZ_CANARY_STATUS_NOT_ATTEMPTED,
        AZ_CANARY_STATUS_NOT_ATTEMPTED
    };

    /*
     * Ordinal 4 runs inside Aurora's logger dispatcher.  Claim startup before
     * any file or thread API can itself produce a nested log message.
     */
    if (!__atomic_compare_exchange_n(
            &g_m1_start_claimed,
            &expected,
            1u,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        return;
    }

    /*
     * The worker proof has its own file so a later startup-phase write on
     * the caller thread cannot overwrite it.  Remove any old proof before
     * publishing this run's immutable identity and creating the worker.
     */
    (void)DeleteFileA(g_m1_worker_marker_path);
    g_m1_identity = marker;
    g_m1_identity.state = AuroraAZCanaryGetMonitorState();
    write_m1_record(g_m1_marker_path, &g_m1_identity);
    (void)AuroraAZCanaryStartMonitor(
        update_m1_record,
        &g_m1_identity);
}

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

    try_start_m1(AZ_M1_SOURCE_CONFIGURE_ORDINAL);
    return 0u;
}

uint32_t AuroraAZNetDbgShutdown(void)
{
    /* Ordinal 3 represents network loss as well as final logger shutdown.
     * Module detach owns process-lifetime teardown. */
    return 0u;
}

uint32_t AuroraAZNetDbgWrite(const char *message)
{
    (void)message;
    try_start_m1(AZ_M1_SOURCE_WRITE_ORDINAL);
    return 0u;
}

uint32_t AuroraAZNetDbgReserved(void)
{
    return 0u;
}
