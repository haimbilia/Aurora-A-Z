#if !defined(AURORAAZ_XBOX360)
#error "netdbg_bootstrap.c must only be built for the Xbox 360 target"
#endif

#include <stddef.h>
#include <stdint.h>

#include <xecore/xam.h>
#include <xecore/xboxkrnl.h>

#include <auroraaz/netdbg_bootstrap.h>
#include <auroraaz/hook_runtime.h>
#include <auroraaz/rev1655_runtime.h>

#include "rev1655_thread_private.h"

#define AZ_M2A_MARKER_WORKER_ENTERED 1u
#define AZ_M2A_MARKER_RUNTIME_RETURNED 2u
#define AZ_M2A_RESULT_NOT_ATTEMPTED 0xFFFFFFFFu

typedef struct AzM2aMarker {
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
} AzM2aMarker;

typedef char AzM2aMarkerMustBe48Bytes[
    (sizeof(AzM2aMarker) == 48u) ? 1 : -1];

static uint32_t g_bootstrap_claimed = 0u;
#if defined(AURORAAZ_DASHLAUNCH_PLUGIN)
static uint32_t g_dashlaunch_bootstrap_claimed = 0u;
#endif
static char g_m2a_marker_path[] =
#if defined(AURORAAZ_DASHLAUNCH_PLUGIN)
    "Hdd:\\Aurora\\Data\\Logs\\AuroraAZ-M2a.bin";
#else
    "game:\\Data\\Logs\\AuroraAZ-M2a.bin";
#endif

static void write_m2a_marker(uint32_t phase, uint32_t runtime_result)
{
    const AzHookArenaDiagnostics diagnostics =
        az_hook_arena_diagnostics();
    const AzM2aMarker marker = {
        {'A', 'Z', 'M', '2'},
        3u,
        (uint32_t)sizeof(AzM2aMarker),
        phase,
        runtime_result,
        diagnostics.embedded_base,
        diagnostics.validation_failures,
        diagnostics.protection_before,
        diagnostics.protection_after,
        diagnostics.target_address,
        diagnostics.target_protection_before,
        diagnostics.target_protection_after
    };
    HANDLE file;
    uint32_t bytes_written = 0u;

    file = CreateFileA(
        g_m2a_marker_path,
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
        (void *)&marker,
        (uint32_t)sizeof(marker),
        &bytes_written,
        NULL);
    (void)CloseHandle(file);
}

static uint32_t bootstrap_rev1655_runtime(void *context)
{
    AzRev1655RuntimeResult result;

    (void)context;
    write_m2a_marker(
        AZ_M2A_MARKER_WORKER_ENTERED,
        AZ_M2A_RESULT_NOT_ATTEMPTED);
    result = az_rev1655_runtime_start(
        AZ_REV1655_RUNTIME_STAGE_OVERLAY_CANARY);
    write_m2a_marker(
        AZ_M2A_MARKER_RUNTIME_RETURNED,
        (uint32_t)result);

    return (uint32_t)result;
}

uint32_t AuroraAZNetDbgBootstrapStart(void)
{
    uint32_t expected = 0u;
    HANDLE thread = NULL;
    AzRev1655ThreadCreateResult create_result;
    AzRev1655RuntimeResult pin_result;

    /* Aurora's logger may recursively dispatch while startup is in flight.
     * Claim before the first kernel call so every nested/subsequent write is
     * a constant-time no-op. */
    if (!__atomic_compare_exchange_n(
            &g_bootstrap_claimed,
            &expected,
            1u,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        return 0u;
    }

    /* Close the only unload window before ordinal 4 returns to Aurora. This
     * synchronous path is deliberately quiet: recursive logger dispatch sees
     * g_bootstrap_claimed and becomes a constant-time no-op. */
    pin_result = az_rev1655_runtime_pin_module(
        (uint32_t)(uintptr_t)&AuroraAZNetDbgWrite);
    if (pin_result != AZ_REV1655_RUNTIME_OK) {
        return AZ_STATUS_REVISION_MISMATCH;
    }

    create_result = az_rev1655_thread_create(
        (void *)(uintptr_t)&bootstrap_rev1655_runtime,
        NULL,
        &thread);
    if (create_result ==
        AZ_REV1655_THREAD_CREATE_REVISION_MISMATCH) {
        return AZ_STATUS_REVISION_MISMATCH;
    }
    if (create_result != AZ_REV1655_THREAD_CREATE_OK) {
        return AZ_STATUS_THREAD_NOT_CREATED;
    }

    /* The validated Aurora wrapper has configured and resumed the worker.
     * Aurora owns this optional DLL for the title process lifetime. Closing
     * the caller handle cannot stop the executing thread, and avoids any
     * loader-lock join at module detach. */
    (void)NtClose(thread);
    return 0u;
}

#if defined(AURORAAZ_DASHLAUNCH_PLUGIN)
static uint32_t dashlaunch_bootstrap_worker(void *context)
{
    AzRev1655RuntimeResult result = AZ_REV1655_RUNTIME_LIFETIME_REJECTED;
    uint32_t attempt;

    (void)context;
    write_m2a_marker(AZ_M2A_MARKER_WORKER_ENTERED,
        AZ_M2A_RESULT_NOT_ATTEMPTED);

    /* This worker uses only kernel imports until Aurora's admission gate
     * passes. Allow two minutes for boot; do not depend on the game: mount. */
    for (attempt = 0u; attempt < 240u; ++attempt) {
        int64_t interval;

        result = az_rev1655_runtime_pin_dashlaunch_module(
            (uint32_t)(uintptr_t)&AuroraAZNetDbgWrite);
        if (result == AZ_REV1655_RUNTIME_OK) {
            HANDLE thread = NULL;
            /* Runtime initialization needs Aurora's title-thread startup.
             * Only call that wrapper after its code has been validated. */
            if (az_rev1655_thread_create(
                    (void *)(uintptr_t)&bootstrap_rev1655_runtime,
                    NULL, &thread) == AZ_REV1655_THREAD_CREATE_OK) {
                (void)NtClose(thread);
                return 0u;
            }
            result = AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED;
            break;
        }
        interval = -5000000LL;
        (void)KeDelayExecutionThread(0u, 0u, &interval);
        if ((attempt % 10u) == 0u) {
            write_m2a_marker(AZ_M2A_MARKER_WORKER_ENTERED, (uint32_t)result);
        }
    }

    write_m2a_marker(AZ_M2A_MARKER_RUNTIME_RETURNED, (uint32_t)result);
    return (uint32_t)result;
}

/* Independent of Aurora's CRT/XapiThreadStartup. The kernel invokes this
 * startup routine with the requested entry and context. Explicit termination
 * avoids returning through a title CRT that has not been loaded yet. */
static void dashlaunch_thread_startup(
    uint32_t (*entry)(void *), void *context)
{
    (void)ExTerminateThread(entry(context));
}
#endif

int DllMain(void *module, uint32_t reason, void *reserved)
{
    (void)module;
    (void)reserved;

#if defined(AURORAAZ_DASHLAUNCH_PLUGIN)
    if (reason == 1u) {
        uint32_t expected = 0u;
        HANDLE thread = NULL;

        if (__atomic_compare_exchange_n(
                &g_dashlaunch_bootstrap_claimed,
                &expected,
                1u,
                0,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE) &&
            !FAILED(ExCreateThread(
                &thread, 0x10000u, NULL,
                (void *)(uintptr_t)&dashlaunch_thread_startup,
                (void *)(uintptr_t)&dashlaunch_bootstrap_worker,
                NULL, 2u))) {
            (void)NtResumeThread(thread, NULL);
            (void)NtClose(thread);
        }
    }
#else
    (void)reason;
#endif

    /* This is a process-lifetime optional module. Never wait for workers or
     * restore hooks from DllMain: the loader lock makes synchronous teardown
     * unsafe, while process detach destroys all title threads and mappings. */
    return 1;
}
