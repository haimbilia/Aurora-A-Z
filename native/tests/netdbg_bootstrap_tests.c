#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xecore/xam.h>
#include <xecore/xboxkrnl.h>

#include <auroraaz/rev1655_runtime.h>
#include <auroraaz/hook_runtime.h>

#define CHECK(condition)                                                   \
    do {                                                                   \
        if (!(condition)) {                                                \
            fprintf(stderr, "check failed: %s:%d: %s\n",                 \
                __FILE__, __LINE__, #condition);                           \
            exit(1);                                                       \
        }                                                                  \
    } while (0)

typedef uint32_t (*TestWorker)(void *context);

typedef struct TestM2aMarker {
    uint8_t magic[4];
    uint32_t version;
    uint32_t record_size;
    uint32_t phase;
    uint32_t runtime_result;
    uint32_t arena_base;
    uint32_t arena_validation_failures;
    uint32_t arena_protection_before;
    uint32_t arena_protection_after;
    uint32_t target_address;
    uint32_t target_protection_before;
    uint32_t target_protection_after;
} TestM2aMarker;

const uint32_t g_auroraaz_test_xapi_thread_startup[8] = {
    0x7D8802A6u,
    0x48163679u,
    0x3BE1FF80u,
    0x9421FF80u,
    0x7C7E1B78u,
    0x7C9D2378u,
    0x39600000u,
    0x917F0050u
};

const uint32_t g_auroraaz_test_thread_wrapper_probe[25] = {
    0x7D8802A6u,
    0x9181FFF8u,
    0x9421FFA0u,
    0x3D608280u,
    0x7C882378u,
    0x7C671B78u,
    0x39200002u,
    0x38CB4650u,
    0x38A10054u,
    0x38800000u,
    0x38610050u,
    0x488049B9u,
    0x38800003u,
    0x80610050u,
    0x484A0879u,
    0x3880000Fu,
    0x80610050u,
    0x484A0645u,
    0x80610050u,
    0x484A256Du,
    0x80610050u,
    0x38210060u,
    0x8181FFF8u,
    0x7D8803A6u,
    0x4E800020u
};

static uint32_t wrapper_calls;
static uint32_t close_calls;
static uint32_t marker_open_calls;
static uint32_t marker_write_calls;
static uint32_t marker_close_calls;
static uint32_t runtime_start_calls;
static uint32_t runtime_pin_calls;
static uint32_t runtime_shutdown_request_calls;
static uint32_t observed_ordinal4_export;
static TestWorker pending_worker;
static void *pending_worker_context;
static AzRev1655RuntimeStage observed_stage;
static TestM2aMarker observed_markers[8];
#if defined(AURORAAZ_DASHLAUNCH_PLUGIN)
static uint32_t kernel_create_calls, resume_calls, delay_calls, admission_calls;
static TestWorker kernel_worker;
static void (*kernel_startup)(TestWorker, void *);
NTSTATUS ExCreateThread(HANDLE *handle, uint32_t stack, uint32_t *id,
    void *startup, void *entry, void *context, uint32_t flags)
{
    CHECK(stack == 0x10000u && id == NULL && context == NULL && flags == 2u);
    CHECK(wrapper_calls == 0u && admission_calls == 0u);
    ++kernel_create_calls;
    kernel_worker = (TestWorker)(uintptr_t)entry;
    kernel_startup = (void (*)(TestWorker, void *))(uintptr_t)startup;
    *handle = (HANDLE)(uintptr_t)0x1234u;
    return 0;
}
NTSTATUS NtResumeThread(HANDLE handle, uint32_t *count)
{
    CHECK(handle == (HANDLE)(uintptr_t)0x1234u && count == NULL);
    ++resume_calls;
    return 0;
}
NTSTATUS ExTerminateThread(uint32_t result) { CHECK(result == 0u); return 0; }
NTSTATUS KeDelayExecutionThread(uint32_t mode, uint32_t alert, int64_t *interval)
{
    CHECK(mode == 0u && alert == 0u && *interval == -5000000LL);
    ++delay_calls;
    return 0;
}
AzRev1655RuntimeResult az_rev1655_runtime_pin_dashlaunch_module(uint32_t address)
{
    CHECK(address != 0u);
    ++admission_calls;
    if (admission_calls < 4u) return AZ_REV1655_RUNTIME_LIFETIME_REJECTED;
    runtime_pin_calls = 1u;
    return AZ_REV1655_RUNTIME_OK;
}
#endif

AzHookArenaDiagnostics az_hook_arena_diagnostics(void)
{
    const AzHookArenaDiagnostics diagnostics = {
        0x82D90000u,
        AZ_HOOK_ARENA_DIAG_PROTECT_MISMATCH,
        0x20u,
        0x20u,
        0x82801D90u,
        0x20u,
        0x40u
    };
    return diagnostics;
}

uint32_t AuroraAZNetDbgConfigure(
    uint32_t command_port,
    uint32_t debug_port,
    uint32_t mode);
uint32_t AuroraAZNetDbgShutdown(void);
uint32_t AuroraAZNetDbgWrite(const char *message);
uint32_t AuroraAZNetDbgReserved(void);
int DllMain(void *module, uint32_t reason, void *reserved);

bool MmIsAddressValid(void *address)
{
    return address != NULL;
}

HANDLE g_auroraaz_test_thread_wrapper(
    void *start_address,
    void *start_context)
{
    CHECK(runtime_pin_calls == 1u);
    ++wrapper_calls;
    pending_worker = (TestWorker)(uintptr_t)start_address;
    pending_worker_context = start_context;
    return (HANDLE)(uintptr_t)0x1234u;
}

AzRev1655RuntimeResult az_rev1655_runtime_pin_module(
    uint32_t expected_ordinal4_export)
{
    ++runtime_pin_calls;
    observed_ordinal4_export = expected_ordinal4_export;
    return AZ_REV1655_RUNTIME_OK;
}

HANDLE CreateFileA(
    const char *path,
    uint32_t desired_access,
    uint32_t share_mode,
    void *security_attributes,
    uint32_t creation_disposition,
    uint32_t flags_and_attributes,
    HANDLE template_file)
{
#if defined(AURORAAZ_DASHLAUNCH_PLUGIN)
    CHECK(strcmp(path, "Hdd:\\Aurora\\Data\\Logs\\AuroraAZ-M2a.bin") == 0);
#else
    CHECK(strcmp(path, "game:\\Data\\Logs\\AuroraAZ-M2a.bin") == 0);
#endif
    CHECK(desired_access == GENERIC_WRITE);
    CHECK(share_mode == FILE_SHARE_READ);
    CHECK(security_attributes == NULL);
    CHECK(creation_disposition == CREATE_ALWAYS);
    CHECK(flags_and_attributes == FILE_ATTRIBUTE_NORMAL);
    CHECK(template_file == NULL);
    ++marker_open_calls;
    return (HANDLE)(uintptr_t)0x5678u;
}

int WriteFile(
    HANDLE file,
    void *buffer,
    uint32_t bytes_to_write,
    uint32_t *bytes_written,
    void *overlapped)
{
    CHECK(file == (HANDLE)(uintptr_t)0x5678u);
    CHECK(buffer != NULL);
    CHECK(bytes_to_write == (uint32_t)sizeof(TestM2aMarker));
    CHECK(bytes_written != NULL);
    CHECK(overlapped == NULL);
    CHECK(marker_write_calls < 8u);
    if (marker_write_calls < 8u) {
        memcpy(
            &observed_markers[marker_write_calls],
            buffer,
            sizeof(TestM2aMarker));
    }
    ++marker_write_calls;
    *bytes_written = bytes_to_write;
    return 1;
}

int CloseHandle(HANDLE handle)
{
    CHECK(handle == (HANDLE)(uintptr_t)0x5678u);
    ++marker_close_calls;
    return 1;
}

NTSTATUS NtClose(HANDLE handle)
{
    CHECK(handle == (HANDLE)(uintptr_t)0x1234u);
    ++close_calls;
    return 0;
}

AzRev1655RuntimeResult az_rev1655_runtime_start(
    AzRev1655RuntimeStage stage)
{
    ++runtime_start_calls;
    observed_stage = stage;
    return AZ_REV1655_RUNTIME_OK;
}

void az_rev1655_runtime_request_shutdown(void)
{
    ++runtime_shutdown_request_calls;
}

int main(void)
{
#if defined(AURORAAZ_DASHLAUNCH_PLUGIN)
    CHECK(DllMain(NULL, 1u, NULL) == 1);
    CHECK(DllMain(NULL, 1u, NULL) == 1);
    CHECK(kernel_create_calls == 1u && resume_calls == 1u);
    CHECK(wrapper_calls == 0u && runtime_start_calls == 0u);
    CHECK(kernel_startup != NULL && kernel_worker != NULL);
    kernel_startup(kernel_worker, NULL);
    CHECK(delay_calls == 3u && admission_calls == 4u);
    CHECK(wrapper_calls == 1u && runtime_start_calls == 0u);
    CHECK(pending_worker != NULL);
    CHECK(pending_worker(NULL) == AZ_REV1655_RUNTIME_OK);
    CHECK(runtime_start_calls == 1u);
    puts("DashLaunch boot-before-Aurora handoff passed");
    return 0;
#else
    CHECK(AuroraAZNetDbgConfigure(730u, 731u, 1u) == 0u);
    CHECK(AuroraAZNetDbgShutdown() == 0u);
    CHECK(runtime_shutdown_request_calls == 1u);
    CHECK(AuroraAZNetDbgReserved() == 0u);
    CHECK(wrapper_calls == 0u);
    CHECK(runtime_start_calls == 0u);

    CHECK(AuroraAZNetDbgWrite("first") == 0u);
    CHECK(AuroraAZNetDbgWrite("recursive") == 0u);
    CHECK(AuroraAZNetDbgWrite("later") == 0u);
    CHECK(wrapper_calls == 1u);
    CHECK(runtime_pin_calls == 1u);
    CHECK(observed_ordinal4_export ==
        (uint32_t)(uintptr_t)&AuroraAZNetDbgWrite);
    CHECK(close_calls == 1u);
    CHECK(pending_worker != NULL);
    CHECK(runtime_start_calls == 0u);

    CHECK(pending_worker(pending_worker_context) ==
        (uint32_t)AZ_REV1655_RUNTIME_OK);
    CHECK(runtime_start_calls == 1u);
    CHECK(observed_stage == AZ_REV1655_RUNTIME_STAGE_OVERLAY_CANARY);
    CHECK(marker_open_calls == 2u);
    CHECK(marker_write_calls == 2u);
    CHECK(marker_close_calls == 2u);
    CHECK(memcmp(observed_markers[0].magic, "AZM2", 4u) == 0);
    CHECK(observed_markers[0].version == 3u);
    CHECK(observed_markers[0].record_size ==
        (uint32_t)sizeof(TestM2aMarker));
    CHECK(observed_markers[0].phase == 1u);
    CHECK(observed_markers[0].runtime_result == 0xFFFFFFFFu);
    CHECK(observed_markers[1].phase == 2u);
    CHECK(observed_markers[1].runtime_result ==
        (uint32_t)AZ_REV1655_RUNTIME_OK);
    CHECK(observed_markers[1].arena_base == 0x82D90000u);
    CHECK(observed_markers[1].arena_validation_failures ==
        AZ_HOOK_ARENA_DIAG_PROTECT_MISMATCH);
    CHECK(observed_markers[1].arena_protection_before == 0x20u);
    CHECK(observed_markers[1].arena_protection_after == 0x20u);
    CHECK(observed_markers[1].target_address == 0x82801D90u);
    CHECK(observed_markers[1].target_protection_before == 0x20u);
    CHECK(observed_markers[1].target_protection_after == 0x40u);

    CHECK(AuroraAZNetDbgShutdown() == 0u);
    CHECK(runtime_shutdown_request_calls == 2u);

    CHECK(DllMain(NULL, 1u, NULL) == 1);
    CHECK(close_calls == 1u);

    CHECK(DllMain(NULL, 0u, NULL) == 1);
    CHECK(close_calls == 1u);

    puts("AuroraAZ NetDbg M2a bootstrap host tests passed");
    return 0;
#endif
}
