#if !defined(AURORAAZ_XBOX360)
#error "rev1655_runtime.c must only be built for the Xbox 360 target"
#endif

#include <stddef.h>
#include <stdint.h>

#include <xecore/xboxkrnl.h>

#include <auroraaz/hook_runtime.h>
#include <auroraaz/image.h>
#include <auroraaz/input_detour.h>
#include <auroraaz/rev1655_hook_gate.h>
#include <auroraaz/rev1655_runtime.h>

#include "rev1655_hook_gate_private.h"
#include "rev1655_thread_private.h"

#define AZ_IMAGE_VALIDATION_STRIDE 0x1000u
#define AZ_M2A_INPUT_TARGET_ADDRESS 0x82801D90u
#define AZ_INPUT_SIGNATURE_SIZE 20u
#define AZ_OBSERVATION_DRAIN_BUDGET 8u
#define AZ_WORKER_INTERVAL_100NS (-500000LL)
#define AZ_CONTROL_WAIT_100NS (-10000LL)

typedef char AzM2aInputContinuationMustMatch[
    AZ_M2A_INPUT_TARGET_ADDRESS + 4u ==
        AZ_REV1655_INPUT_WRAPPER_CONTINUE_ADDRESS ? 1 : -1];

typedef struct AzRev1655Runtime {
    AzHookArena arena;
    AzLiveHook input_hook;
    HANDLE worker_thread;
    volatile uint32_t state;
    volatile uint32_t stage;
    volatile uint32_t observations_logged;
} AzRev1655Runtime;

static AzRev1655Runtime g_runtime;

static uint32_t load_u32(const volatile uint32_t *value)
{
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static void store_u32(volatile uint32_t *value, uint32_t replacement)
{
    __atomic_store_n(value, replacement, __ATOMIC_RELEASE);
}

static void wait_for_control(void)
{
    int64_t interval = AZ_CONTROL_WAIT_100NS;

    (void)KeDelayExecutionThread(0u, 0u, &interval);
}

static void wait_for_input(void)
{
    int64_t interval = AZ_WORKER_INTERVAL_100NS;

    (void)KeDelayExecutionThread(0u, 0u, &interval);
}

/* Validate every page that the exact-image gate will read before hashing. */
static uint8_t image_range_is_mapped(const uint8_t *image, size_t size)
{
    size_t offset;
    const uintptr_t start = (uintptr_t)image;

    if (image == NULL || size == 0u ||
        start > UINTPTR_MAX - (size - 1u)) {
        return 0u;
    }

    for (offset = 0u; offset < size;
         offset += (size_t)AZ_IMAGE_VALIDATION_STRIDE) {
        if (!MmIsAddressValid((void *)(image + offset))) {
            return 0u;
        }
    }

    return MmIsAddressValid((void *)(image + size - 1u)) ? 1u : 0u;
}

static AzRev1655RuntimeResult validate_input_site(
    AzRev1655LoadedImage *image,
    AzRev1655ResolvedHookSite *resolved)
{
    const AzRev1655HookPermit *permit = NULL;
    const AzRev1655HookSiteDescriptor *descriptor;
    AzRev1655HookGateResult gate_result;

    image->bytes = (const uint8_t *)(uintptr_t)AZ_REV1655_IMAGE_BASE;
    image->size = (size_t)AZ_REV1655_NT_IMAGE_SIZE;
    image->virtual_address = AZ_REV1655_IMAGE_BASE;

    if (image_range_is_mapped(image->bytes, image->size) == 0u) {
        DbgPrint("AuroraAZ: M2a image mapping rejected\n");
        return AZ_REV1655_RUNTIME_IMAGE_UNMAPPED;
    }

    /* This verifies the header, complete .text hash, and every reviewed site
     * window.  No arena, bridge state, thread, or executable byte is mutated
     * before the complete gate succeeds. */
    gate_result = az_rev1655_hook_gate_validate(image, &permit);
    if (gate_result != AZ_REV1655_HOOK_GATE_OK) {
        DbgPrint(
            "AuroraAZ: M2a image gate rejected: %s\n",
            az_rev1655_hook_gate_result_name(gate_result));
        return AZ_REV1655_RUNTIME_IMAGE_REJECTED;
    }

    descriptor = az_rev1655_hook_gate_site(
        permit,
        AZ_REV1655_HOOK_SITE_INPUT_WRAPPER);
    if (descriptor == NULL) {
        DbgPrint("AuroraAZ: M2a input descriptor rejected\n");
        return AZ_REV1655_RUNTIME_SITE_REJECTED;
    }

    gate_result = az_rev1655_hook_gate_resolve_site(
        permit,
        descriptor,
        image,
        resolved);
    if (gate_result != AZ_REV1655_HOOK_GATE_OK) {
        DbgPrint(
            "AuroraAZ: M2a live input gate rejected: %s\n",
            az_rev1655_hook_gate_result_name(gate_result));
        return AZ_REV1655_RUNTIME_SITE_REJECTED;
    }

    if (resolved->target_address != AZ_M2A_INPUT_TARGET_ADDRESS ||
        resolved->expected_instruction !=
            AZ_REV1655_INPUT_WRAPPER_FIRST_INSTRUCTION ||
        resolved->complete_signature_size !=
            (size_t)AZ_INPUT_SIGNATURE_SIZE) {
        DbgPrint(
            "AuroraAZ: M2a wrong input site target=%08X insn=%08X size=%u\n",
            (unsigned int)resolved->target_address,
            (unsigned int)resolved->expected_instruction,
            (unsigned int)resolved->complete_signature_size);
        return AZ_REV1655_RUNTIME_WRONG_INPUT_SITE;
    }

    return AZ_REV1655_RUNTIME_OK;
}

static void log_observation(const AzInputDetourObservation *observation)
{
    DbgPrint(
        "AuroraAZ: M2a input n=%u frame=%u lr=%08X vk=%04X flags=%04X "
        "user=%u hid=%u control=%u event=%u command=%u coverflow=%u "
        "would=%u consumed=%u filter=%u stage=%u/%u\n",
        (unsigned int)observation->serial,
        (unsigned int)observation->input_frame,
        (unsigned int)observation->caller_return_address,
        (unsigned int)observation->keystroke.virtual_key,
        (unsigned int)observation->keystroke.flags,
        (unsigned int)observation->keystroke.user_index,
        (unsigned int)observation->keystroke.hid_code,
        (unsigned int)observation->translation.control,
        (unsigned int)observation->translation.event,
        (unsigned int)observation->translation.command,
        (unsigned int)observation->coverflow_active,
        (unsigned int)observation->would_handle,
        (unsigned int)observation->consumed,
        (unsigned int)observation->filter_queued,
        (unsigned int)observation->requested_stage,
        (unsigned int)observation->effective_stage);
}

/* One worker pass has a fixed logging budget, even if input remains busy. */
static uint32_t drain_observation_pass(void)
{
    uint32_t drained = 0u;

    while (drained < AZ_OBSERVATION_DRAIN_BUDGET) {
        AzInputDetourObservation observation;
        const AzInputDetourResult result =
            az_rev1655_input_detour_take_observation(&observation);

        if (result == AZ_INPUT_DETOUR_NO_OBSERVATION) {
            break;
        }
        if (result != AZ_INPUT_DETOUR_OK) {
            DbgPrint(
                "AuroraAZ: M2a observation drain rejected: %s\n",
                az_input_detour_result_name(result));
            break;
        }

        log_observation(&observation);
        (void)__atomic_add_fetch(
            &g_runtime.observations_logged,
            1u,
            __ATOMIC_ACQ_REL);
        ++drained;
    }

    return drained;
}

static void drain_all_observations(void)
{
    while (drain_observation_pass() == AZ_OBSERVATION_DRAIN_BUDGET) {
        /* Keep each pass bounded and yield between full batches. */
        wait_for_control();
    }
}

static void remove_input_hook_safely(void)
{
    uint8_t reported_failure = 0u;

    for (;;) {
        const AzHookRuntimeResult result =
            az_live_hook_remove(&g_runtime.input_hook);

        if (result == AZ_HOOK_RUNTIME_OK) {
            break;
        }
        if (result != AZ_HOOK_RUNTIME_QUIESCING &&
            reported_failure == 0u) {
            DbgPrint(
                "AuroraAZ: M2a hook removal waiting: %s\n",
                az_hook_runtime_result_name(result));
            reported_failure = 1u;
        }

        (void)drain_observation_pass();
        wait_for_control();
    }

    while (az_live_hook_can_unload(&g_runtime.input_hook) == 0u ||
           az_rev1655_input_detour_shutdown_ready() == 0u) {
        (void)drain_observation_pass();
        wait_for_control();
    }
}

static void rollback_installed_input_hook(void)
{
    AzInputDetourStatus status;

    (void)az_rev1655_input_detour_request_stage(AZ_INPUT_DETOUR_OFF);
    az_rev1655_input_detour_publish_verification(0u, 0u, 0u, 0u);

    for (;;) {
        const AzHookRuntimeResult result =
            az_live_hook_remove(&g_runtime.input_hook);

        if (result == AZ_HOOK_RUNTIME_OK) {
            break;
        }
        if (result != AZ_HOOK_RUNTIME_QUIESCING) {
            DbgPrint(
                "AuroraAZ: M2a startup rollback waiting: %s\n",
                az_hook_runtime_result_name(result));
        }
        wait_for_control();
    }

    do {
        az_rev1655_input_detour_snapshot_status(&status);
        if (status.in_flight != 0u) {
            wait_for_control();
        }
    } while (status.in_flight != 0u);

    drain_all_observations();
}

static uint32_t input_observe_worker(void *context)
{
    uint32_t state;

    (void)context;

    do {
        state = load_u32(&g_runtime.state);
        if (state == (uint32_t)AZ_REV1655_RUNTIME_STARTING) {
            wait_for_control();
        }
    } while (state == (uint32_t)AZ_REV1655_RUNTIME_STARTING);

    if (state == (uint32_t)AZ_REV1655_RUNTIME_RUNNING) {
        DbgPrint(
            "AuroraAZ: M2a input observe active target=%08X "
            "consume=disabled\n",
            (unsigned int)AZ_M2A_INPUT_TARGET_ADDRESS);
    }

    while (load_u32(&g_runtime.state) ==
           (uint32_t)AZ_REV1655_RUNTIME_RUNNING) {
        (void)drain_observation_pass();
        wait_for_input();
    }

    if (load_u32(&g_runtime.state) ==
        (uint32_t)AZ_REV1655_RUNTIME_STOPPING) {
        AzInputDetourStatus status;

        /* OFF and revoked gates precede target restoration.  OBSERVE never
         * owns a key, so shutdown has no consumed release to synthesize. */
        az_rev1655_input_detour_begin_shutdown();
        remove_input_hook_safely();
        drain_all_observations();
        az_rev1655_input_detour_snapshot_status(&status);
        DbgPrint(
            "AuroraAZ: M2a input observe stopped calls=%u keys=%u "
            "drops=%u logged=%u consumed=%u\n",
            (unsigned int)status.main_calls,
            (unsigned int)status.successful_keys,
            (unsigned int)status.observation_drops,
            (unsigned int)load_u32(&g_runtime.observations_logged),
            (unsigned int)status.consumed_controls);
        store_u32(
            &g_runtime.stage,
            (uint32_t)AZ_REV1655_RUNTIME_STAGE_DISABLED);
        store_u32(
            &g_runtime.state,
            (uint32_t)AZ_REV1655_RUNTIME_CLOSED);
    }

    return 0u;
}

static void wait_for_worker_exit(void)
{
    if (g_runtime.worker_thread != NULL) {
        NTSTATUS wait_status;

        do {
            wait_status = NtWaitForSingleObjectEx(
                (uint32_t)(uintptr_t)g_runtime.worker_thread,
                0u,
                0u,
                NULL);
        } while (FAILED(wait_status));

        (void)NtClose(g_runtime.worker_thread);
        g_runtime.worker_thread = NULL;
    }
}

static void stop_starting_worker(void)
{
    /* The resumed worker is waiting on STARTING. Release it and join before
     * any caller may unload module code. */
    store_u32(
        &g_runtime.state,
        (uint32_t)AZ_REV1655_RUNTIME_STOPPED);
    wait_for_worker_exit();
}

static AzRev1655RuntimeResult start_input_observe(void)
{
    AzRev1655LoadedImage image;
    AzRev1655ResolvedHookSite resolved;
    AzRev1655RuntimeResult validation;
    AzHookRuntimeResult hook_result;
    AzInputDetourResult detour_result;
    AzRev1655ThreadCreateResult create_result;
    HANDLE worker_thread = NULL;

    validation = validate_input_site(&image, &resolved);
    if (validation != AZ_REV1655_RUNTIME_OK) {
        return validation;
    }

    if (az_rev1655_thread_wrapper_is_valid() == 0u) {
        DbgPrint("AuroraAZ: M2a thread-wrapper probe rejected\n");
        return AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED;
    }

    hook_result = az_hook_arena_create_rev1655(&g_runtime.arena);
    if (hook_result != AZ_HOOK_RUNTIME_OK) {
        DbgPrint(
            "AuroraAZ: M2a near arena rejected: %s\n",
            az_hook_runtime_result_name(hook_result));
        return AZ_REV1655_RUNTIME_ARENA_FAILED;
    }

    az_rev1655_input_detour_reset();
    az_rev1655_input_detour_publish_verification(1u, 0u, 0u, 0u);

    create_result = az_rev1655_thread_create(
        (void *)(uintptr_t)&input_observe_worker,
        NULL,
        &worker_thread);
    if (create_result != AZ_REV1655_THREAD_CREATE_OK) {
        az_rev1655_input_detour_publish_verification(0u, 0u, 0u, 0u);
        (void)az_hook_arena_release_uninstalled(&g_runtime.arena);
        if (create_result ==
            AZ_REV1655_THREAD_CREATE_REVISION_MISMATCH) {
            DbgPrint("AuroraAZ: M2a thread wrapper changed before call\n");
            return AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED;
        }
        DbgPrint("AuroraAZ: M2a worker creation failed\n");
        return AZ_REV1655_RUNTIME_THREAD_CREATE_FAILED;
    }
    g_runtime.worker_thread = worker_thread;

    hook_result = az_live_hook_install(
        &g_runtime.arena,
        resolved.target_address,
        resolved.expected_instruction,
        (const void *)(uintptr_t)&az_rev1655_input_detour_entry,
        &g_runtime.input_hook);
    if (hook_result != AZ_HOOK_RUNTIME_OK) {
        az_rev1655_input_detour_publish_verification(0u, 0u, 0u, 0u);
        stop_starting_worker();
        if (g_runtime.arena.used == 0u) {
            (void)az_hook_arena_release_uninstalled(&g_runtime.arena);
        }
        DbgPrint(
            "AuroraAZ: M2a input hook rejected: %s\n",
            az_hook_runtime_result_name(hook_result));
        return AZ_REV1655_RUNTIME_HOOK_INSTALL_FAILED;
    }

    /* Render, filter, control-confirmation, and scene gates remain false. */
    az_rev1655_input_detour_publish_verification(1u, 1u, 0u, 0u);
    detour_result = az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_OBSERVE);
    if (detour_result != AZ_INPUT_DETOUR_OK) {
        rollback_installed_input_hook();
        stop_starting_worker();
        DbgPrint(
            "AuroraAZ: M2a observe stage rejected: %s\n",
            az_input_detour_result_name(detour_result));
        return AZ_REV1655_RUNTIME_DETOUR_STAGE_FAILED;
    }

    store_u32(
        &g_runtime.stage,
        (uint32_t)AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE);
    /* Aurora's wrapper has resumed the worker, which waits in STARTING while
     * the hook and observe gate are prepared. This release-store is the only
     * publication that lets it touch the runtime. */
    store_u32(
        &g_runtime.state,
        (uint32_t)AZ_REV1655_RUNTIME_RUNNING);
    return AZ_REV1655_RUNTIME_OK;
}

AzRev1655RuntimeResult az_rev1655_runtime_start(
    AzRev1655RuntimeStage stage)
{
    uint32_t expected;
    AzRev1655RuntimeResult result;

    if (stage != AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE) {
        return AZ_REV1655_RUNTIME_BAD_STAGE;
    }

    expected = load_u32(&g_runtime.state);
    if (expected == (uint32_t)AZ_REV1655_RUNTIME_RUNNING &&
        load_u32(&g_runtime.stage) == (uint32_t)stage) {
        return AZ_REV1655_RUNTIME_OK;
    }
    if (expected == (uint32_t)AZ_REV1655_RUNTIME_CLOSED) {
        return AZ_REV1655_RUNTIME_CLOSED_RESULT;
    }
    if (expected != (uint32_t)AZ_REV1655_RUNTIME_STOPPED ||
        !__atomic_compare_exchange_n(
            &g_runtime.state,
            &expected,
            (uint32_t)AZ_REV1655_RUNTIME_STARTING,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        return AZ_REV1655_RUNTIME_BUSY;
    }

    store_u32(&g_runtime.observations_logged, 0u);
    result = start_input_observe();
    if (result != AZ_REV1655_RUNTIME_OK) {
        store_u32(
            &g_runtime.stage,
            (uint32_t)AZ_REV1655_RUNTIME_STAGE_DISABLED);
        store_u32(
            &g_runtime.state,
            (uint32_t)AZ_REV1655_RUNTIME_STOPPED);
    }
    return result;
}

void az_rev1655_runtime_shutdown(void)
{
    for (;;) {
        uint32_t state = load_u32(&g_runtime.state);

        if (state == (uint32_t)AZ_REV1655_RUNTIME_STOPPED ||
            state == (uint32_t)AZ_REV1655_RUNTIME_CLOSED) {
            return;
        }
        if (state == (uint32_t)AZ_REV1655_RUNTIME_STARTING ||
            state == (uint32_t)AZ_REV1655_RUNTIME_STOPPING) {
            wait_for_control();
            continue;
        }
        if (state == (uint32_t)AZ_REV1655_RUNTIME_RUNNING) {
            uint32_t expected =
                (uint32_t)AZ_REV1655_RUNTIME_RUNNING;

            if (__atomic_compare_exchange_n(
                    &g_runtime.state,
                    &expected,
                    (uint32_t)AZ_REV1655_RUNTIME_STOPPING,
                    0,
                    __ATOMIC_ACQ_REL,
                    __ATOMIC_ACQUIRE)) {
                break;
            }
            continue;
        }

        return;
    }

    wait_for_worker_exit();
}

AzRev1655RuntimeState az_rev1655_runtime_state(void)
{
    return (AzRev1655RuntimeState)load_u32(&g_runtime.state);
}

AzRev1655RuntimeStage az_rev1655_runtime_stage(void)
{
    return (AzRev1655RuntimeStage)load_u32(&g_runtime.stage);
}

uint32_t az_rev1655_runtime_observations_logged(void)
{
    return load_u32(&g_runtime.observations_logged);
}

const char *az_rev1655_runtime_result_name(AzRev1655RuntimeResult result)
{
    switch (result) {
    case AZ_REV1655_RUNTIME_OK:
        return "ok";
    case AZ_REV1655_RUNTIME_BAD_STAGE:
        return "bad-stage";
    case AZ_REV1655_RUNTIME_BUSY:
        return "busy";
    case AZ_REV1655_RUNTIME_CLOSED_RESULT:
        return "closed";
    case AZ_REV1655_RUNTIME_IMAGE_UNMAPPED:
        return "image-unmapped";
    case AZ_REV1655_RUNTIME_IMAGE_REJECTED:
        return "image-rejected";
    case AZ_REV1655_RUNTIME_SITE_REJECTED:
        return "site-rejected";
    case AZ_REV1655_RUNTIME_WRONG_INPUT_SITE:
        return "wrong-input-site";
    case AZ_REV1655_RUNTIME_ARENA_FAILED:
        return "arena-failed";
    case AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED:
        return "thread-startup-rejected";
    case AZ_REV1655_RUNTIME_THREAD_CREATE_FAILED:
        return "thread-create-failed";
    case AZ_REV1655_RUNTIME_HOOK_INSTALL_FAILED:
        return "hook-install-failed";
    case AZ_REV1655_RUNTIME_DETOUR_STAGE_FAILED:
        return "detour-stage-failed";
    default:
        return "unknown";
    }
}
