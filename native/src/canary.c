#if !defined(AURORAAZ_XBOX360)
#error "canary.c must only be built for the Xbox 360 target"
#endif

#include <stdint.h>
#include <stddef.h>

#include <xecore/xboxkrnl.h>

#include <auroraaz/canary.h>
#include <auroraaz/compatibility.h>
#include <auroraaz/image.h>

#define AZ_DLL_PROCESS_DETACH 0u
#define AZ_REV1655_THREAD_WRAPPER_ADDRESS 0x82361AA8u
#define AZ_REV1655_XAPI_THREAD_STARTUP_ADDRESS 0x82804650u
#define AZ_STATUS_REVISION_MISMATCH 0xC0000059u
#define AZ_STATUS_THREAD_NOT_CREATED 0xC0000017u
#define AZ_MONITOR_INTERVAL_100NS (-1000000LL)
#define AZ_CONTROL_WAIT_100NS (-10000LL)

typedef HANDLE (*AzRev1655ThreadWrapper)(
    void *start_address,
    void *start_context);

typedef struct AzCanaryResult {
    AzImageResult image;
    AzCompatibilityResult compatibility;
} AzCanaryResult;

static uint32_t g_monitor_state = AZ_CANARY_MONITOR_STOPPED;
static HANDLE g_monitor_thread = NULL;
static uint32_t g_monitor_worker_entered = 0u;

static const uint32_t k_rev1655_xapi_thread_startup_probe[] = {
    0x7D8802A6u,
    0x48163679u,
    0x3BE1FF80u,
    0x9421FF80u,
    0x7C7E1B78u,
    0x7C9D2378u,
    0x39600000u,
    0x917F0050u
};

static const uint32_t k_rev1655_thread_wrapper_probe[] = {
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

static void observe_start(
    AzCanaryStartObserver observer,
    const AzCanaryStartSnapshot *snapshot,
    void *observer_context)
{
    if (observer != NULL) {
        observer(snapshot, observer_context);
    }
}

static uint8_t rev1655_xapi_thread_startup_is_valid(void)
{
    const volatile uint32_t *actual =
        (const volatile uint32_t *)(uintptr_t)
            AZ_REV1655_XAPI_THREAD_STARTUP_ADDRESS;
    size_t index;

    if (!MmIsAddressValid((void *)(uintptr_t)actual) ||
        !MmIsAddressValid((void *)(uintptr_t)(
            (const uint8_t *)(uintptr_t)actual +
            sizeof(k_rev1655_xapi_thread_startup_probe) - 1u))) {
        return 0u;
    }

    for (index = 0u;
         index < sizeof(k_rev1655_xapi_thread_startup_probe) /
             sizeof(k_rev1655_xapi_thread_startup_probe[0]);
         ++index) {
        if (actual[index] != k_rev1655_xapi_thread_startup_probe[index]) {
            return 0u;
        }
    }

    return 1u;
}

static uint8_t rev1655_thread_wrapper_is_valid(void)
{
    const volatile uint32_t *actual =
        (const volatile uint32_t *)(uintptr_t)
            AZ_REV1655_THREAD_WRAPPER_ADDRESS;
    size_t index;

    if (!MmIsAddressValid((void *)(uintptr_t)actual) ||
        !MmIsAddressValid((void *)(uintptr_t)(
            (const uint8_t *)(uintptr_t)actual +
            sizeof(k_rev1655_thread_wrapper_probe) - 1u))) {
        return 0u;
    }

    for (index = 0u;
         index < sizeof(k_rev1655_thread_wrapper_probe) /
             sizeof(k_rev1655_thread_wrapper_probe[0]);
         ++index) {
        if (actual[index] != k_rev1655_thread_wrapper_probe[index]) {
            return 0u;
        }
    }

    return 1u;
}

static void wait_for_monitor_control(void)
{
    int64_t interval = AZ_CONTROL_WAIT_100NS;

    (void)KeDelayExecutionThread(0u, 0u, &interval);
}

static AzCanaryResult validate_running_aurora(void)
{
    const uint8_t *image =
        (const uint8_t *)(uintptr_t)AZ_REV1655_IMAGE_BASE;
    const uint8_t *text = NULL;
    size_t text_size = 0u;
    AzCanaryResult result = {
        AZ_IMAGE_NULL,
        AZ_COMPAT_BAD_TEXT_BASE
    };

    if (!MmIsAddressValid((void *)image) ||
        !MmIsAddressValid((void *)(image + 0x3FFu))) {
        return result;
    }

    result.image = az_locate_rev1655_text(
        image,
        (size_t)AZ_REV1655_NT_IMAGE_SIZE,
        &text,
        &text_size);
    if (result.image != AZ_IMAGE_REV1655 || text == NULL ||
        text_size == 0u ||
        !MmIsAddressValid((void *)text) ||
        !MmIsAddressValid((void *)(text + text_size - 1u))) {
        return result;
    }

    result.compatibility = az_validate_rev1655_text(
        text,
        text_size,
        (uint32_t)(uintptr_t)text);
    return result;
}

static uint32_t monitor_aurora(void *context)
{
    uint32_t state;

    (void)context;
    __atomic_store_n(
        &g_monitor_worker_entered,
        1u,
        __ATOMIC_RELEASE);

    do {
        state = __atomic_load_n(&g_monitor_state, __ATOMIC_ACQUIRE);
        if (state == AZ_CANARY_MONITOR_STARTING) {
            wait_for_monitor_control();
        }
    } while (state == AZ_CANARY_MONITOR_STARTING);

    if (state == AZ_CANARY_MONITOR_RUNNING) {
        const AzCanaryResult result = validate_running_aurora();
        DbgPrint(
            "AuroraAZ: canary image=%s, compatibility=%s\n",
            az_image_result_name(result.image),
            az_compatibility_result_name(result.compatibility));
    }

    /*
     * Stay observable to NOVA without touching the module loader.  Detach can
     * therefore stop and join this thread while the loader lock is held: the
     * worker needs only this atomic flag and a bounded kernel delay to exit.
     */
    while (__atomic_load_n(&g_monitor_state, __ATOMIC_ACQUIRE) ==
           AZ_CANARY_MONITOR_RUNNING) {
        {
            int64_t interval = AZ_MONITOR_INTERVAL_100NS;
            (void)KeDelayExecutionThread(0u, 0u, &interval);
        }
    }

    return 0u;
}

uint32_t AuroraAZCanaryGetMonitorState(void)
{
    return __atomic_load_n(&g_monitor_state, __ATOMIC_ACQUIRE);
}

uint32_t AuroraAZCanaryGetWorkerEntered(void)
{
    return __atomic_load_n(
        &g_monitor_worker_entered,
        __ATOMIC_ACQUIRE);
}

uint32_t AuroraAZCanaryStartMonitor(
    AzCanaryStartObserver observer,
    void *observer_context)
{
    uint32_t expected = AZ_CANARY_MONITOR_STOPPED;
    HANDLE monitor_thread = NULL;
    AzRev1655ThreadWrapper create_thread;
    AzCanaryStartSnapshot snapshot = {
        AZ_CANARY_START_CREATE_PENDING,
        AZ_CANARY_MONITOR_STARTING,
        AZ_CANARY_STATUS_NOT_ATTEMPTED,
        AZ_CANARY_STATUS_NOT_ATTEMPTED
    };

    if (!__atomic_compare_exchange_n(
            &g_monitor_state,
            &expected,
            AZ_CANARY_MONITOR_STARTING,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        snapshot.phase = AZ_CANARY_START_ALREADY_ACTIVE;
        snapshot.state = expected;
        observe_start(observer, &snapshot, observer_context);
        return 0u;
    }

    __atomic_store_n(
        &g_monitor_worker_entered,
        0u,
        __ATOMIC_RELEASE);
    observe_start(observer, &snapshot, observer_context);

    /*
     * Use Aurora Rev1655's complete, known-working thread wrapper.  It calls
     * ExCreateThread with XapiThreadStartup, selects processor 3, sets
     * priority 15, and resumes the handle once.  Validate both private code
     * ranges before branching through either revision-specific address.
     */
    if (rev1655_xapi_thread_startup_is_valid() == 0u ||
        rev1655_thread_wrapper_is_valid() == 0u) {
        __atomic_store_n(
            &g_monitor_state,
            AZ_CANARY_MONITOR_STOPPED,
            __ATOMIC_RELEASE);
        snapshot.phase = AZ_CANARY_START_COMPLETE;
        snapshot.state = AZ_CANARY_MONITOR_STOPPED;
        snapshot.ex_create_thread_status =
            AZ_STATUS_REVISION_MISMATCH;
        observe_start(observer, &snapshot, observer_context);
        return AZ_STATUS_REVISION_MISMATCH;
    }

    create_thread = (AzRev1655ThreadWrapper)(uintptr_t)
        AZ_REV1655_THREAD_WRAPPER_ADDRESS;
    monitor_thread = create_thread(
        (void *)(uintptr_t)&monitor_aurora,
        NULL);
    if (monitor_thread == NULL || monitor_thread == INVALID_HANDLE_VALUE) {
        __atomic_store_n(
            &g_monitor_state,
            AZ_CANARY_MONITOR_STOPPED,
            __ATOMIC_RELEASE);
        snapshot.phase = AZ_CANARY_START_COMPLETE;
        snapshot.state = AZ_CANARY_MONITOR_STOPPED;
        snapshot.ex_create_thread_status = AZ_STATUS_THREAD_NOT_CREATED;
        observe_start(observer, &snapshot, observer_context);
        return AZ_STATUS_THREAD_NOT_CREATED;
    }

    g_monitor_thread = monitor_thread;
    snapshot.ex_create_thread_status = 0u;
    snapshot.phase = AZ_CANARY_START_CREATE_RETURNED;
    observe_start(observer, &snapshot, observer_context);

    /*
     * The validated Aurora wrapper already resumed the thread.  Publish the
     * running state.  The worker's first side effect is only a release-store;
     * later NetDbg calls observe that sticky value and perform file I/O back
     * on Aurora's established logger thread.
     */
    snapshot.phase = AZ_CANARY_START_RESUME_RETURNED;
    snapshot.nt_resume_thread_status = 0u;
    observe_start(observer, &snapshot, observer_context);

    snapshot.phase = AZ_CANARY_START_COMPLETE;
    snapshot.state = AZ_CANARY_MONITOR_RUNNING;
    observe_start(observer, &snapshot, observer_context);
    __atomic_store_n(
        &g_monitor_state,
        AZ_CANARY_MONITOR_RUNNING,
        __ATOMIC_RELEASE);
    return 0u;
}

void AuroraAZCanaryStopMonitor(void)
{
    for (;;) {
        uint32_t state =
            __atomic_load_n(&g_monitor_state, __ATOMIC_ACQUIRE);

        if (state == AZ_CANARY_MONITOR_STOPPED) {
            return;
        }
        if (state == AZ_CANARY_MONITOR_STARTING ||
            state == AZ_CANARY_MONITOR_STOPPING) {
            wait_for_monitor_control();
            continue;
        }

        if (state == AZ_CANARY_MONITOR_RUNNING) {
            uint32_t expected = AZ_CANARY_MONITOR_RUNNING;

            if (__atomic_compare_exchange_n(
                    &g_monitor_state,
                    &expected,
                    AZ_CANARY_MONITOR_STOPPING,
                    0,
                    __ATOMIC_ACQ_REL,
                    __ATOMIC_ACQUIRE)) {
                break;
            }
        }
    }

    if (g_monitor_thread != NULL) {
        NTSTATUS wait_status;

        do {
            wait_status = NtWaitForSingleObjectEx(
                (uint32_t)(uintptr_t)g_monitor_thread,
                0u,
                0u,
                NULL);
        } while (FAILED(wait_status));

        (void)NtClose(g_monitor_thread);
        g_monitor_thread = NULL;
    }

    __atomic_store_n(
        &g_monitor_state,
        AZ_CANARY_MONITOR_STOPPED,
        __ATOMIC_RELEASE);
}

int DllMain(void *module, uint32_t reason, void *reserved)
{
    (void)module;
    (void)reserved;

    if (reason == AZ_DLL_PROCESS_DETACH) {
        AuroraAZCanaryStopMonitor();
    }

    return 1;
}
