#if !defined(AURORAAZ_XBOX360)
#error "rev1655_runtime.c must only be built for the Xbox 360 target"
#endif

#include <stddef.h>
#include <stdint.h>

#include <xecore/xam.h>
#include <xecore/xboxkrnl.h>

#include <auroraaz/compatibility.h>
#include <auroraaz/hook_runtime.h>
#include <auroraaz/image.h>
#include <auroraaz/input_detour.h>
#include <auroraaz/m2a_input_telemetry.h>
#include <auroraaz/netdbg_bootstrap.h>
#include <auroraaz/netdbg_lifetime_rev1655.h>
#include <auroraaz/rev1655_hook_gate.h>
#include <auroraaz/rev1655_runtime.h>

#include "rev1655_hook_gate_private.h"
#include "rev1655_thread_private.h"

#define AZ_IMAGE_VALIDATION_STRIDE 0x1000u
#define AZ_REV1655_PE_HEADER_SIZE 0x400u
#define AZ_M2A_INPUT_TARGET_ADDRESS 0x82801D90u
#define AZ_INPUT_SIGNATURE_SIZE 20u
#define AZ_OBSERVATION_DRAIN_BUDGET 8u
#define AZ_TELEMETRY_FLUSH_TICKS 5u
#define AZ_WORKER_INTERVAL_100NS (-500000LL)
#define AZ_CONTROL_WAIT_100NS (-10000LL)

#define AZ_LIFETIME_UNPINNED 0u
#define AZ_LIFETIME_PINNING 1u
#define AZ_LIFETIME_PINNED 2u
#define AZ_LIFETIME_FAILED 3u
#define AZ_REV1655_IMPORT_LIBRARY_COUNT 2u

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
    volatile uint32_t lifetime_state;
    volatile uint32_t pinned_ordinal4_export;
    AzM2aInputTelemetry telemetry;
    uint32_t telemetry_flush_ticks;
} AzRev1655Runtime;

static AzRev1655Runtime g_runtime;
static char g_input_telemetry_slot_a_path[] =
    AZ_M2A_INPUT_TELEMETRY_SLOT_A_PATH;
static char g_input_telemetry_slot_b_path[] =
    AZ_M2A_INPUT_TELEMETRY_SLOT_B_PATH;

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

static uint8_t write_complete_file(
    char *path,
    const uint8_t *bytes,
    uint32_t size)
{
    HANDLE file;
    uint32_t bytes_written = 0u;
    int write_succeeded;
    int close_succeeded;

    if (path == NULL || bytes == NULL || size == 0u) {
        return 0u;
    }

    file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == NULL || file == INVALID_HANDLE_VALUE) {
        return 0u;
    }

    write_succeeded = WriteFile(
        file,
        (void *)bytes,
        size,
        &bytes_written,
        NULL);
    close_succeeded = CloseHandle(file);
    return write_succeeded != 0 &&
        bytes_written == size &&
        close_succeeded != 0 ? 1u : 0u;
}

#if defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
void az_rev1655_runtime_test_reset_lifetime(void)
{
    store_u32(&g_runtime.pinned_ordinal4_export, 0u);
    store_u32(&g_runtime.lifetime_state, AZ_LIFETIME_UNPINNED);
}

uint8_t az_rev1655_runtime_test_write_complete_file(
    char *path,
    const uint8_t *bytes,
    uint32_t size)
{
    return write_complete_file(path, bytes, size);
}
#endif

static uint8_t read_complete_file(
    char *path,
    uint8_t *bytes,
    uint32_t size)
{
    HANDLE file;
    uint32_t bytes_read = 0u;
    int read_succeeded;
    int close_succeeded;

    if (path == NULL || bytes == NULL || size == 0u) {
        return 0u;
    }

    file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == NULL || file == INVALID_HANDLE_VALUE) {
        return 0u;
    }

    read_succeeded = ReadFile(
        file,
        bytes,
        size,
        &bytes_read,
        NULL);
    close_succeeded = CloseHandle(file);
    return read_succeeded != 0 &&
        bytes_read == size &&
        close_succeeded != 0 ? 1u : 0u;
}

/* Continue the serial space left by an earlier title session. Without this,
 * a valid stale slot with a larger generation could outrank the new run. */
static void seed_input_telemetry_generation(void)
{
    uint8_t slot_a[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint8_t slot_b[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint8_t selected_slot = AZ_M2A_INPUT_TELEMETRY_SLOT_A;
    uint32_t generation = 0u;
    const uint8_t *valid_a = NULL;
    const uint8_t *valid_b = NULL;

    if (read_complete_file(
            g_input_telemetry_slot_a_path,
            slot_a,
            (uint32_t)sizeof(slot_a)) != 0u) {
        valid_a = slot_a;
    }
    if (read_complete_file(
            g_input_telemetry_slot_b_path,
            slot_b,
            (uint32_t)sizeof(slot_b)) != 0u) {
        valid_b = slot_b;
    }

    if (az_m2a_input_telemetry_select_newest_be(
            valid_a,
            valid_a != NULL ? sizeof(slot_a) : 0u,
            valid_b,
            valid_b != NULL ? sizeof(slot_b) : 0u,
            &selected_slot,
            &generation) == AZ_M2A_INPUT_TELEMETRY_OK) {
        (void)selected_slot;
        (void)az_m2a_input_telemetry_seed_generation(
            &g_runtime.telemetry,
            generation);
    }
}

/* Worker-only durable evidence. A failed or torn slot never acknowledges the
 * in-memory revision; the same CRC-protected generation is retried later. */
static void flush_input_telemetry(uint8_t force)
{
    AzInputDetourStatus status;
    uint8_t record[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint32_t revision_token = 0u;
    uint32_t generation = 0u;
    char *path;

    az_rev1655_input_detour_snapshot_status(&status);
    (void)az_m2a_input_telemetry_update_runtime(
        &g_runtime.telemetry,
        1u,
        (AzRev1655RuntimeState)load_u32(&g_runtime.state),
        status.observation_drops);

    if (az_m2a_input_telemetry_is_dirty(&g_runtime.telemetry) == 0u) {
        g_runtime.telemetry_flush_ticks = 0u;
        return;
    }

    if (force == 0u) {
        if (g_runtime.telemetry_flush_ticks < AZ_TELEMETRY_FLUSH_TICKS) {
            ++g_runtime.telemetry_flush_ticks;
        }
        if (g_runtime.telemetry_flush_ticks < AZ_TELEMETRY_FLUSH_TICKS) {
            return;
        }
    }
    g_runtime.telemetry_flush_ticks = 0u;

    if (az_m2a_input_telemetry_snapshot_be(
            &g_runtime.telemetry,
            record,
            sizeof(record),
            &revision_token) != AZ_M2A_INPUT_TELEMETRY_OK ||
        az_m2a_input_telemetry_validate_record_be(
            record,
            sizeof(record),
            &generation) != AZ_M2A_INPUT_TELEMETRY_OK) {
        return;
    }

    path = (generation & 1u) != 0u ?
        g_input_telemetry_slot_a_path :
        g_input_telemetry_slot_b_path;
    if (write_complete_file(
            path,
            record,
            (uint32_t)sizeof(record)) != 0u) {
        (void)az_m2a_input_telemetry_acknowledge(
            &g_runtime.telemetry,
            revision_token);
    }
}

#if defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
void az_rev1655_runtime_test_telemetry_init(uint32_t generation)
{
    az_m2a_input_telemetry_init(&g_runtime.telemetry);
    if (generation != 0u) {
        (void)az_m2a_input_telemetry_seed_generation(
            &g_runtime.telemetry,
            generation);
    }
    g_runtime.telemetry_flush_ticks = 0u;
    store_u32(
        &g_runtime.state,
        (uint32_t)AZ_REV1655_RUNTIME_RUNNING);
}

AzM2aInputTelemetryResult az_rev1655_runtime_test_telemetry_record(
    const AzInputDetourObservation *observation)
{
    return az_m2a_input_telemetry_record(
        &g_runtime.telemetry,
        observation);
}

void az_rev1655_runtime_test_telemetry_flush(uint8_t force)
{
    flush_input_telemetry(force);
}

uint8_t az_rev1655_runtime_test_telemetry_is_dirty(void)
{
    return az_m2a_input_telemetry_is_dirty(&g_runtime.telemetry);
}

void az_rev1655_runtime_test_telemetry_finish(void)
{
    store_u32(
        &g_runtime.state,
        (uint32_t)AZ_REV1655_RUNTIME_STOPPED);
}
#endif

/* Validate every page that the exact-image gate will actually read. The
 * loaded PE has intentionally unmapped gaps outside its header and .text;
 * probing the entire SizeOfImage would reject the genuine Rev1655 image. */
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

typedef struct AzRev1655RuntimeImportResolverContext {
    HMODULE modules[AZ_REV1655_IMPORT_LIBRARY_COUNT];
    uint8_t module_queries[AZ_REV1655_IMPORT_LIBRARY_COUNT];
} AzRev1655RuntimeImportResolverContext;

static const char *import_module_identity(
    AzRev1655ImportLibrary library,
    size_t *module_index)
{
    if (module_index == NULL) {
        return NULL;
    }

    switch (library) {
    case AZ_REV1655_IMPORT_LIBRARY_XAM:
        *module_index = 0u;
        return "xam.xex";
    case AZ_REV1655_IMPORT_LIBRARY_XBOXKRNL:
        *module_index = 1u;
        return "xboxkrnl.exe";
    default:
        return NULL;
    }
}

/* Resolve each frozen import identity from the loader, never from mutable
 * thunk/IAT bytes. Module handles are looked up once per complete validation;
 * each of the 350 callbacks still performs its own ordinal resolution. */
static int resolve_runtime_import_target(
    void *context,
    AzRev1655ImportLibrary library,
    uint16_t ordinal,
    size_t thunk_index,
    uint32_t *out_target)
{
    AzRev1655RuntimeImportResolverContext *resolver_context =
        (AzRev1655RuntimeImportResolverContext *)context;
    const char *identity;
    HMODULE module;
    void *procedure = NULL;
    uintptr_t target;
    size_t module_index = 0u;
    NTSTATUS status;

    (void)thunk_index;
    if (resolver_context == NULL || out_target == NULL) {
        return 0;
    }
    *out_target = 0u;

    identity = import_module_identity(library, &module_index);
    if (identity == NULL ||
        module_index >= (size_t)AZ_REV1655_IMPORT_LIBRARY_COUNT) {
        return 0;
    }

    if (resolver_context->module_queries[module_index] == 0u) {
        resolver_context->module_queries[module_index] = 1u;
        status = XexGetModuleHandle(
            identity,
            &resolver_context->modules[module_index]);
        if (FAILED(status) ||
            resolver_context->modules[module_index] == NULL) {
            resolver_context->modules[module_index] = NULL;
            return 0;
        }
    }

    module = resolver_context->modules[module_index];
    if (module == NULL) {
        return 0;
    }

    status = XexGetProcedureAddress(module, (uint32_t)ordinal, &procedure);
    if (FAILED(status) || procedure == NULL) {
        return 0;
    }

    target = (uintptr_t)procedure;
    if (target > (uintptr_t)(UINT32_MAX - 3u) ||
        (target & (uintptr_t)3u) != (uintptr_t)0u ||
        !MmIsAddressValid((void *)target) ||
        !MmIsAddressValid((void *)(target + (uintptr_t)3u))) {
        return 0;
    }

    *out_target = (uint32_t)target;
    return 1;
}

static AzRev1655RuntimeResult validate_input_site(
    AzRev1655LoadedImage *image,
    AzRev1655ResolvedHookSite *resolved,
    uint8_t log_failures)
{
    const AzRev1655HookPermit *permit = NULL;
    const AzRev1655HookSiteDescriptor *descriptor;
    AzRev1655RuntimeImportResolverContext resolver_context = {
        {NULL, NULL},
        {0u, 0u}
    };
    const AzRev1655ImportResolver import_resolver = {
        resolve_runtime_import_target,
        &resolver_context
    };
    AzRev1655HookGateResult gate_result;

    image->bytes = (const uint8_t *)(uintptr_t)AZ_REV1655_IMAGE_BASE;
    image->size = (size_t)AZ_REV1655_NT_IMAGE_SIZE;
    image->virtual_address = AZ_REV1655_IMAGE_BASE;

    if (image_range_is_mapped(
            image->bytes,
            (size_t)AZ_REV1655_PE_HEADER_SIZE) == 0u ||
        image_range_is_mapped(
            (const uint8_t *)(uintptr_t)AZ_REV1655_TEXT_BASE,
            (size_t)AZ_REV1655_TEXT_SIZE) == 0u) {
        if (log_failures != 0u) {
            DbgPrint("AuroraAZ: M2a image mapping rejected\n");
        }
        return AZ_REV1655_RUNTIME_IMAGE_UNMAPPED;
    }

    /* This verifies the header, immutable .text prefix, all loader-resolved
     * import thunks, canonical complete-.text hash, and every reviewed site.
     * No arena, bridge state, thread, or executable byte is mutated before
     * the complete gate succeeds. */
    gate_result = az_rev1655_hook_gate_validate_with_import_resolver(
        image,
        &import_resolver,
        &permit);
    if (gate_result != AZ_REV1655_HOOK_GATE_OK) {
        if (log_failures != 0u) {
            DbgPrint(
                "AuroraAZ: M2a image gate rejected: %s\n",
                az_rev1655_hook_gate_result_name(gate_result));
        }
        return AZ_REV1655_RUNTIME_IMAGE_REJECTED;
    }

    descriptor = az_rev1655_hook_gate_site(
        permit,
        AZ_REV1655_HOOK_SITE_INPUT_WRAPPER);
    if (descriptor == NULL) {
        if (log_failures != 0u) {
            DbgPrint("AuroraAZ: M2a input descriptor rejected\n");
        }
        return AZ_REV1655_RUNTIME_SITE_REJECTED;
    }

    gate_result = az_rev1655_hook_gate_resolve_site(
        permit,
        descriptor,
        image,
        resolved);
    if (gate_result != AZ_REV1655_HOOK_GATE_OK) {
        if (log_failures != 0u) {
            DbgPrint(
                "AuroraAZ: M2a live input gate rejected: %s\n",
                az_rev1655_hook_gate_result_name(gate_result));
        }
        return AZ_REV1655_RUNTIME_SITE_REJECTED;
    }

    if (resolved->target_address != AZ_M2A_INPUT_TARGET_ADDRESS ||
        resolved->expected_instruction !=
            AZ_REV1655_INPUT_WRAPPER_FIRST_INSTRUCTION ||
        resolved->complete_signature_size !=
            (size_t)AZ_INPUT_SIGNATURE_SIZE) {
        if (log_failures != 0u) {
            DbgPrint(
                "AuroraAZ: M2a wrong input site target=%08X insn=%08X size=%u\n",
                (unsigned int)resolved->target_address,
                (unsigned int)resolved->expected_instruction,
                (unsigned int)resolved->complete_signature_size);
        }
        return AZ_REV1655_RUNTIME_WRONG_INPUT_SITE;
    }

    return AZ_REV1655_RUNTIME_OK;
}

AzRev1655RuntimeResult az_rev1655_runtime_pin_module(
    uint32_t expected_ordinal4_export)
{
    AzRev1655LoadedImage image;
    AzRev1655ResolvedHookSite resolved;
    AzNetDbgLifetimeRev1655Status lifetime_status;
    AzNetDbgLifetimeRev1655Result lifetime_result;
    AzRev1655RuntimeResult validation;
    uint32_t expected_state;
    uint32_t state;

    if (expected_ordinal4_export == 0u) {
        return AZ_REV1655_RUNTIME_LIFETIME_REJECTED;
    }

    state = load_u32(&g_runtime.lifetime_state);
    if (state == AZ_LIFETIME_PINNED) {
        return load_u32(&g_runtime.pinned_ordinal4_export) ==
            expected_ordinal4_export ?
                AZ_REV1655_RUNTIME_OK :
                AZ_REV1655_RUNTIME_LIFETIME_REJECTED;
    }
    if (state != AZ_LIFETIME_UNPINNED) {
        return state == AZ_LIFETIME_PINNING ?
            AZ_REV1655_RUNTIME_BUSY :
            AZ_REV1655_RUNTIME_LIFETIME_REJECTED;
    }

    expected_state = AZ_LIFETIME_UNPINNED;
    if (!__atomic_compare_exchange_n(
            &g_runtime.lifetime_state,
            &expected_state,
            AZ_LIFETIME_PINNING,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        return expected_state == AZ_LIFETIME_PINNED &&
            load_u32(&g_runtime.pinned_ordinal4_export) ==
                expected_ordinal4_export ?
                    AZ_REV1655_RUNTIME_OK :
                    AZ_REV1655_RUNTIME_BUSY;
    }

    /* The ordinal-4 callback performs this complete read-only revision gate
     * synchronously. Aurora cannot regain control and unload key 7 before the
     * subsequent policy CAS has made this image title-resident. */
    validation = validate_input_site(&image, &resolved, 0u);
    if (validation != AZ_REV1655_RUNTIME_OK) {
        store_u32(&g_runtime.lifetime_state, AZ_LIFETIME_FAILED);
        return validation;
    }
    if (az_rev1655_thread_wrapper_is_valid() == 0u) {
        store_u32(&g_runtime.lifetime_state, AZ_LIFETIME_FAILED);
        return AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED;
    }

    lifetime_result = az_rev1655_netdbg_lifetime_pin_default(
        1u,
        expected_ordinal4_export,
        &lifetime_status);
    if (lifetime_result != AZ_NETDBG_LIFETIME_OK ||
        lifetime_status.pinned_for_title_lifetime == 0u ||
        lifetime_status.compare_exchange_succeeded == 0u ||
        lifetime_status.policy_after !=
            AZ_REV1655_NETDBG_POLICY_RESIDENT) {
        store_u32(&g_runtime.lifetime_state, AZ_LIFETIME_FAILED);
        return AZ_REV1655_RUNTIME_LIFETIME_REJECTED;
    }

    store_u32(
        &g_runtime.pinned_ordinal4_export,
        expected_ordinal4_export);
    store_u32(&g_runtime.lifetime_state, AZ_LIFETIME_PINNED);
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
        (void)az_m2a_input_telemetry_record(
            &g_runtime.telemetry,
            &observation);
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

    if (state == (uint32_t)AZ_REV1655_RUNTIME_RUNNING ||
        state == (uint32_t)AZ_REV1655_RUNTIME_STOPPING) {
        seed_input_telemetry_generation();
    }

    if (state == (uint32_t)AZ_REV1655_RUNTIME_RUNNING) {
        DbgPrint(
            "AuroraAZ: M2a input observe active target=%08X "
            "consume=disabled\n",
            (unsigned int)AZ_M2A_INPUT_TARGET_ADDRESS);
        flush_input_telemetry(1u);
    }

    while (load_u32(&g_runtime.state) ==
           (uint32_t)AZ_REV1655_RUNTIME_RUNNING) {
        (void)drain_observation_pass();
        flush_input_telemetry(0u);
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
        flush_input_telemetry(1u);
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

    validation = validate_input_site(&image, &resolved, 1u);
    if (validation != AZ_REV1655_RUNTIME_OK) {
        return validation;
    }

    if (az_rev1655_thread_wrapper_is_valid() == 0u) {
        DbgPrint("AuroraAZ: M2a thread-wrapper probe rejected\n");
        return AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED;
    }

    az_rev1655_input_detour_reset();
    az_rev1655_input_detour_publish_verification(1u, 0u, 0u, 0u);

    create_result = az_rev1655_thread_create(
        (void *)(uintptr_t)&input_observe_worker,
        NULL,
        &worker_thread);
    if (create_result != AZ_REV1655_THREAD_CREATE_OK) {
        az_rev1655_input_detour_publish_verification(0u, 0u, 0u, 0u);
        if (create_result ==
            AZ_REV1655_THREAD_CREATE_REVISION_MISMATCH) {
            DbgPrint("AuroraAZ: M2a thread wrapper changed before call\n");
            return AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED;
        }
        DbgPrint("AuroraAZ: M2a worker creation failed\n");
        return AZ_REV1655_RUNTIME_THREAD_CREATE_FAILED;
    }
    g_runtime.worker_thread = worker_thread;

    hook_result = az_live_hook_install_direct(
        resolved.target_address,
        resolved.expected_instruction,
        (const void *)(uintptr_t)&az_rev1655_input_direct_detour_entry,
        &g_runtime.input_hook);
    if (hook_result != AZ_HOOK_RUNTIME_OK) {
        az_rev1655_input_detour_publish_verification(0u, 0u, 0u, 0u);
        stop_starting_worker();
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
    if (load_u32(&g_runtime.lifetime_state) != AZ_LIFETIME_PINNED ||
        load_u32(&g_runtime.pinned_ordinal4_export) !=
            (uint32_t)(uintptr_t)&AuroraAZNetDbgWrite) {
        return AZ_REV1655_RUNTIME_LIFETIME_REJECTED;
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
    az_m2a_input_telemetry_init(&g_runtime.telemetry);
    g_runtime.telemetry_flush_ticks = 0u;
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
    case AZ_REV1655_RUNTIME_LIFETIME_REJECTED:
        return "lifetime-rejected";
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
