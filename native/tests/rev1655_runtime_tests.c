#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xecore/xboxkrnl.h>

#include <auroraaz/compatibility.h>
#include <auroraaz/hook_runtime.h>
#include <auroraaz/image.h>
#include <auroraaz/input_detour.h>
#include <auroraaz/rev1655_hook_gate.h>
#include <auroraaz/rev1655_runtime.h>

#include "rev1655_hook_gate_private.h"

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

static int failures;
static uint32_t gate_calls;
static uint32_t site_calls;
static uint32_t resolve_calls;
static uint32_t arena_calls;
static uint32_t hook_install_calls;
static uint32_t hook_remove_calls;
static uint32_t thread_wrapper_calls;
static uint32_t thread_wait_calls;
static uint32_t thread_close_calls;
static uint32_t current_hook_remove_calls;
static uint32_t begin_shutdown_calls;
static uint32_t observe_stage_requests;
static uint32_t consume_stage_requests;
static uint32_t publication_violation;
static uint32_t observations_available;
static uint32_t observation_log_lines;
static uint32_t installed_target;
static uint32_t image_probe_calls;
static uint32_t sparse_gap_probe_calls;
static uintptr_t rejected_image_address;
static AzRev1655HookGateResult configured_gate_result =
    AZ_REV1655_HOOK_GATE_BAD_TEXT_SHA256;
static AzHookRuntimeResult configured_hook_install_result =
    AZ_HOOK_RUNTIME_OK;
static AzInputDetourResult configured_observe_result =
    AZ_INPUT_DETOUR_OK;
static HANDLE configured_wrapper_handle =
    (HANDLE)(uintptr_t)0x1234u;
static uint8_t reject_thread_startup;

typedef uint32_t (*TestWorker)(void *context);

static TestWorker pending_worker;
static void *pending_worker_context;
static uint8_t worker_executed;
static uint8_t shutdown_requested;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

bool MmIsAddressValid(void *address)
{
    const uintptr_t candidate = (uintptr_t)address;
    const uintptr_t image_start = (uintptr_t)AZ_REV1655_IMAGE_BASE;
    const uintptr_t image_end = image_start +
        (uintptr_t)AZ_REV1655_NT_IMAGE_SIZE;
    const uintptr_t header_end = image_start + (uintptr_t)0x400u;
    const uintptr_t text_start = (uintptr_t)AZ_REV1655_TEXT_BASE;
    const uintptr_t text_end = text_start +
        (uintptr_t)AZ_REV1655_TEXT_SIZE;
    const uintptr_t probe_start =
        (uintptr_t)&g_auroraaz_test_xapi_thread_startup[0];
    const uintptr_t probe_end = probe_start +
        sizeof(g_auroraaz_test_xapi_thread_startup);
    const uintptr_t wrapper_start =
        (uintptr_t)&g_auroraaz_test_thread_wrapper_probe[0];
    const uintptr_t wrapper_end = wrapper_start +
        sizeof(g_auroraaz_test_thread_wrapper_probe);

    if (reject_thread_startup != 0u &&
        ((candidate >= probe_start && candidate < probe_end) ||
         (candidate >= wrapper_start && candidate < wrapper_end))) {
        return false;
    }

    /* The real loaded image is sparse. Reject every address in its gaps so
     * the runtime test proves preflight touches only the PE header and .text
     * ranges that the exact-image gate hashes. */
    if (candidate >= image_start && candidate < image_end) {
        const bool in_mapped_range = (candidate < header_end) ||
            (candidate >= text_start && candidate < text_end);

        ++image_probe_calls;
        if (!in_mapped_range) {
            ++sparse_gap_probe_calls;
        }
        if (candidate == rejected_image_address) {
            return false;
        }
        return in_mapped_range;
    }
    return address != NULL;
}

int DbgPrint(const char *format, ...)
{
    va_list arguments;

    if (strstr(format, "AuroraAZ: M2a input n=") != NULL) {
        ++observation_log_lines;
    }
    va_start(arguments, format);
    va_end(arguments);
    return 0;
}

NTSTATUS KeDelayExecutionThread(
    uint32_t wait_mode,
    uint32_t alertable,
    int64_t *interval)
{
    (void)wait_mode;
    (void)alertable;
    (void)interval;
    return 0;
}

HANDLE g_auroraaz_test_thread_wrapper(
    void *start_address,
    void *start_context)
{
    ++thread_wrapper_calls;
    pending_worker = (TestWorker)(uintptr_t)start_address;
    pending_worker_context = start_context;
    worker_executed = 0u;
    return configured_wrapper_handle;
}

NTSTATUS NtClose(HANDLE handle)
{
    (void)handle;
    ++thread_close_calls;
    return 0;
}

NTSTATUS NtWaitForSingleObjectEx(
    uint32_t handle,
    uint32_t wait_mode,
    uint32_t alertable,
    int64_t *timeout)
{
    (void)handle;
    (void)wait_mode;
    (void)alertable;
    (void)timeout;

    ++thread_wait_calls;
    if (pending_worker != NULL && worker_executed == 0u) {
        worker_executed = 1u;
        (void)pending_worker(pending_worker_context);
    }
    return 0;
}

AzRev1655HookGateResult az_rev1655_hook_gate_validate(
    const AzRev1655LoadedImage *image,
    const AzRev1655HookPermit **out_permit)
{
    ++gate_calls;
    CHECK(image != NULL);
    CHECK(image->virtual_address == 0x82000000u);
    CHECK(out_permit != NULL);

    if (configured_gate_result == AZ_REV1655_HOOK_GATE_OK) {
        *out_permit = (const AzRev1655HookPermit *)(uintptr_t)1u;
    }
    else {
        *out_permit = NULL;
    }
    return configured_gate_result;
}

const AzRev1655HookSiteDescriptor *az_rev1655_hook_gate_site(
    const AzRev1655HookPermit *permit,
    AzRev1655HookSiteId site_id)
{
    ++site_calls;
    CHECK(permit != NULL);
    CHECK(site_id == AZ_REV1655_HOOK_SITE_INPUT_WRAPPER);
    return (const AzRev1655HookSiteDescriptor *)(uintptr_t)2u;
}

AzRev1655HookGateResult az_rev1655_hook_gate_resolve_site(
    const AzRev1655HookPermit *permit,
    const AzRev1655HookSiteDescriptor *descriptor,
    const AzRev1655LoadedImage *image,
    AzRev1655ResolvedHookSite *out_site)
{
    ++resolve_calls;
    CHECK(permit != NULL);
    CHECK(descriptor != NULL);
    CHECK(image != NULL);
    CHECK(out_site != NULL);
    out_site->target_address = 0x82801D90u;
    out_site->expected_instruction = 0x7D8802A6u;
    out_site->complete_signature_size = 20u;
    return AZ_REV1655_HOOK_GATE_OK;
}

const char *az_rev1655_hook_gate_result_name(
    AzRev1655HookGateResult result)
{
    (void)result;
    return "test-gate-result";
}

AzHookRuntimeResult az_hook_arena_create_rev1655(AzHookArena *arena)
{
    ++arena_calls;
    CHECK(gate_calls != 0u);
    CHECK(resolve_calls != 0u);
    arena->base = (uintptr_t)0x82D50000u;
    arena->size = AZ_HOOK_ARENA_SIZE;
    arena->used = 0u;
    return AZ_HOOK_RUNTIME_OK;
}

AzHookRuntimeResult az_hook_arena_release_uninstalled(AzHookArena *arena)
{
    CHECK(arena != NULL);
    CHECK(arena->used == 0u);
    arena->base = (uintptr_t)0u;
    arena->size = 0u;
    return AZ_HOOK_RUNTIME_OK;
}

AzHookRuntimeResult az_live_hook_install(
    AzHookArena *arena,
    uint32_t target_address,
    uint32_t expected_instruction,
    const void *detour,
    AzLiveHook *hook)
{
    ++hook_install_calls;
    CHECK(arena != NULL);
    CHECK(arena->base != (uintptr_t)0u);
    CHECK(target_address == 0x82801D90u);
    CHECK(expected_instruction == 0x7D8802A6u);
    CHECK(detour != NULL);
    CHECK(hook != NULL);
    CHECK(thread_wrapper_calls != 0u);
    CHECK(az_rev1655_runtime_state() == AZ_REV1655_RUNTIME_STARTING);

    if (configured_hook_install_result != AZ_HOOK_RUNTIME_OK) {
        return configured_hook_install_result;
    }

    installed_target = target_address;
    current_hook_remove_calls = 0u;
    arena->used = AZ_HOOK_SLOT_SIZE;
    hook->admission_address = (uintptr_t)0x82D50090u;
    hook->installed = 1u;
    hook->target_restored = 0u;
    return AZ_HOOK_RUNTIME_OK;
}

AzHookRuntimeResult az_live_hook_remove(AzLiveHook *hook)
{
    ++hook_remove_calls;
    ++current_hook_remove_calls;
    CHECK(hook != NULL);
    if (current_hook_remove_calls == 1u) {
        hook->target_restored = 1u;
        return AZ_HOOK_RUNTIME_QUIESCING;
    }
    hook->installed = 0u;
    return AZ_HOOK_RUNTIME_OK;
}

uint8_t az_live_hook_can_unload(const AzLiveHook *hook)
{
    CHECK(hook != NULL);
    return current_hook_remove_calls >= 2u ? 1u : 0u;
}

const char *az_hook_runtime_result_name(AzHookRuntimeResult result)
{
    (void)result;
    return "test-hook-result";
}

void az_rev1655_input_detour_reset(void)
{
    shutdown_requested = 0u;
}

void az_rev1655_input_detour_publish_verification(
    uint8_t image_verified,
    uint8_t input_hook_verified,
    uint8_t render_hook_verified,
    uint8_t filter_consumer_verified)
{
    (void)image_verified;
    (void)input_hook_verified;
    if (render_hook_verified != 0u || filter_consumer_verified != 0u) {
        publication_violation = 1u;
    }
}

AzInputDetourResult az_rev1655_input_detour_request_stage(
    AzInputDetourStage stage)
{
    if (stage == AZ_INPUT_DETOUR_OBSERVE) {
        ++observe_stage_requests;
        CHECK(az_rev1655_runtime_state() == AZ_REV1655_RUNTIME_STARTING);
        return configured_observe_result;
    }
    if (stage == AZ_INPUT_DETOUR_CONSUME) {
        ++consume_stage_requests;
    }
    return AZ_INPUT_DETOUR_OK;
}

void az_rev1655_input_detour_begin_shutdown(void)
{
    shutdown_requested = 1u;
    ++begin_shutdown_calls;
}

uint8_t az_rev1655_input_detour_shutdown_ready(void)
{
    return shutdown_requested;
}

AzInputDetourResult az_rev1655_input_detour_take_observation(
    AzInputDetourObservation *observation)
{
    if (observations_available == 0u) {
        return AZ_INPUT_DETOUR_NO_OBSERVATION;
    }

    memset(observation, 0, sizeof(*observation));
    observation->serial = 11u - observations_available;
    observation->input_frame = observation->serial;
    observation->caller_return_address =
        AZ_REV1655_INPUT_MAIN_RETURN_ADDRESS;
    observation->keystroke.virtual_key = AZ_VK_PAD_RTHUMB_PRESS;
    observation->keystroke.flags = AZ_KEYSTROKE_KEYDOWN;
    observation->translation.control = AZ_INPUT_CONTROL_R3;
    observation->translation.event = AZ_INPUT_EVENT_PRESS;
    observation->requested_stage = AZ_INPUT_DETOUR_OBSERVE;
    observation->effective_stage = AZ_INPUT_STAGE_OBSERVE_ONLY;
    --observations_available;
    return AZ_INPUT_DETOUR_OK;
}

void az_rev1655_input_detour_snapshot_status(AzInputDetourStatus *status)
{
    memset(status, 0, sizeof(*status));
    status->main_calls = 10u;
    status->successful_keys = 10u;
}

const char *az_input_detour_result_name(AzInputDetourResult result)
{
    (void)result;
    return "test-input-result";
}

uint32_t az_rev1655_input_detour_entry(
    uint32_t user_index,
    uint32_t flags,
    AzInputKeystroke *keystroke)
{
    (void)user_index;
    (void)flags;
    (void)keystroke;
    return 0u;
}

static void test_gate_failure_precedes_host_mutation(void)
{
    const uintptr_t rejected_text_page =
        (uintptr_t)AZ_REV1655_TEXT_BASE + (uintptr_t)0x2000u;

    CHECK(az_rev1655_runtime_start(
        AZ_REV1655_RUNTIME_STAGE_DISABLED) ==
        AZ_REV1655_RUNTIME_BAD_STAGE);
    CHECK(gate_calls == 0u);
    CHECK(arena_calls == 0u);

    rejected_image_address = rejected_text_page;
    CHECK(az_rev1655_runtime_start(
        AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE) ==
        AZ_REV1655_RUNTIME_IMAGE_UNMAPPED);
    CHECK(gate_calls == 0u);
    CHECK(site_calls == 0u);
    CHECK(resolve_calls == 0u);
    CHECK(arena_calls == 0u);
    CHECK(hook_install_calls == 0u);
    CHECK(thread_wrapper_calls == 0u);
    CHECK(image_probe_calls != 0u);
    CHECK(sparse_gap_probe_calls == 0u);
    CHECK(az_rev1655_runtime_state() == AZ_REV1655_RUNTIME_STOPPED);
    rejected_image_address = (uintptr_t)0u;

    CHECK(az_rev1655_runtime_start(
        AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE) ==
        AZ_REV1655_RUNTIME_IMAGE_REJECTED);
    CHECK(gate_calls == 1u);
    CHECK(site_calls == 0u);
    CHECK(resolve_calls == 0u);
    CHECK(arena_calls == 0u);
    CHECK(hook_install_calls == 0u);
    CHECK(thread_wrapper_calls == 0u);
    CHECK(sparse_gap_probe_calls == 0u);
    CHECK(az_rev1655_runtime_state() == AZ_REV1655_RUNTIME_STOPPED);
}

static void test_observe_only_lifecycle(void)
{
    uint32_t arena_calls_before;
    uint32_t gate_calls_after_start;
    uint32_t hook_install_calls_before;
    uint32_t hook_remove_calls_before;
    uint32_t thread_close_calls_before;
    uint32_t thread_wrapper_calls_before;
    uint32_t thread_wait_calls_before;

    configured_gate_result = AZ_REV1655_HOOK_GATE_OK;

    reject_thread_startup = 1u;
    arena_calls_before = arena_calls;
    thread_wrapper_calls_before = thread_wrapper_calls;
    CHECK(az_rev1655_runtime_start(
        AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE) ==
        AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED);
    CHECK(arena_calls == arena_calls_before);
    CHECK(thread_wrapper_calls == thread_wrapper_calls_before);
    CHECK(az_rev1655_runtime_state() == AZ_REV1655_RUNTIME_STOPPED);
    reject_thread_startup = 0u;

    configured_wrapper_handle = NULL;
    thread_wrapper_calls_before = thread_wrapper_calls;
    thread_wait_calls_before = thread_wait_calls;
    thread_close_calls_before = thread_close_calls;
    hook_install_calls_before = hook_install_calls;
    CHECK(az_rev1655_runtime_start(
        AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE) ==
        AZ_REV1655_RUNTIME_THREAD_CREATE_FAILED);
    CHECK(thread_wrapper_calls == thread_wrapper_calls_before + 1u);
    CHECK(thread_wait_calls == thread_wait_calls_before);
    CHECK(thread_close_calls == thread_close_calls_before);
    CHECK(hook_install_calls == hook_install_calls_before);
    CHECK(az_rev1655_runtime_state() == AZ_REV1655_RUNTIME_STOPPED);
    configured_wrapper_handle = (HANDLE)(uintptr_t)0x1234u;

    configured_hook_install_result = AZ_HOOK_RUNTIME_TARGET_CHANGED;
    thread_wrapper_calls_before = thread_wrapper_calls;
    thread_wait_calls_before = thread_wait_calls;
    thread_close_calls_before = thread_close_calls;
    hook_install_calls_before = hook_install_calls;
    CHECK(az_rev1655_runtime_start(
        AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE) ==
        AZ_REV1655_RUNTIME_HOOK_INSTALL_FAILED);
    CHECK(thread_wrapper_calls == thread_wrapper_calls_before + 1u);
    CHECK(thread_wait_calls == thread_wait_calls_before + 1u);
    CHECK(thread_close_calls == thread_close_calls_before + 1u);
    CHECK(hook_install_calls == hook_install_calls_before + 1u);
    CHECK(worker_executed != 0u);
    CHECK(az_rev1655_runtime_state() == AZ_REV1655_RUNTIME_STOPPED);

    configured_hook_install_result = AZ_HOOK_RUNTIME_OK;
    configured_observe_result = AZ_INPUT_DETOUR_NOT_VERIFIED;
    thread_wrapper_calls_before = thread_wrapper_calls;
    thread_wait_calls_before = thread_wait_calls;
    thread_close_calls_before = thread_close_calls;
    hook_remove_calls_before = hook_remove_calls;
    CHECK(az_rev1655_runtime_start(
        AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE) ==
        AZ_REV1655_RUNTIME_DETOUR_STAGE_FAILED);
    CHECK(thread_wrapper_calls == thread_wrapper_calls_before + 1u);
    CHECK(thread_wait_calls == thread_wait_calls_before + 1u);
    CHECK(thread_close_calls == thread_close_calls_before + 1u);
    CHECK(hook_remove_calls == hook_remove_calls_before + 2u);
    CHECK(worker_executed != 0u);
    CHECK(az_rev1655_runtime_state() == AZ_REV1655_RUNTIME_STOPPED);

    configured_observe_result = AZ_INPUT_DETOUR_OK;
    thread_wrapper_calls_before = thread_wrapper_calls;
    hook_install_calls_before = hook_install_calls;
    hook_remove_calls_before = hook_remove_calls;
    thread_wait_calls_before = thread_wait_calls;
    thread_close_calls_before = thread_close_calls;
    CHECK(az_rev1655_runtime_start(
        AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE) ==
        AZ_REV1655_RUNTIME_OK);
    CHECK(thread_wrapper_calls == thread_wrapper_calls_before + 1u);
    CHECK(hook_install_calls == hook_install_calls_before + 1u);
    CHECK(installed_target == 0x82801D90u);
    CHECK(consume_stage_requests == 0u);
    CHECK(publication_violation == 0u);
    CHECK(az_rev1655_runtime_state() == AZ_REV1655_RUNTIME_RUNNING);
    CHECK(az_rev1655_runtime_stage() ==
        AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE);

    gate_calls_after_start = gate_calls;
    CHECK(az_rev1655_runtime_start(
        AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE) ==
        AZ_REV1655_RUNTIME_OK);
    CHECK(gate_calls == gate_calls_after_start);
    CHECK(hook_install_calls == hook_install_calls_before + 1u);

    observations_available = 10u;
    az_rev1655_runtime_shutdown();
    CHECK(thread_wait_calls == thread_wait_calls_before + 1u);
    CHECK(thread_close_calls == thread_close_calls_before + 1u);
    CHECK(worker_executed != 0u);
    CHECK(begin_shutdown_calls == 1u);
    CHECK(hook_remove_calls == hook_remove_calls_before + 2u);
    CHECK(observations_available == 0u);
    CHECK(observation_log_lines == 10u);
    CHECK(az_rev1655_runtime_observations_logged() == 10u);
    CHECK(consume_stage_requests == 0u);
    CHECK(publication_violation == 0u);
    CHECK(az_rev1655_runtime_state() == AZ_REV1655_RUNTIME_CLOSED);
    CHECK(az_rev1655_runtime_stage() ==
        AZ_REV1655_RUNTIME_STAGE_DISABLED);
    CHECK(az_rev1655_runtime_start(
        AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE) ==
        AZ_REV1655_RUNTIME_CLOSED_RESULT);
}

int main(void)
{
    test_gate_failure_precedes_host_mutation();
    test_observe_only_lifecycle();

    CHECK(strcmp(az_rev1655_runtime_result_name(
        AZ_REV1655_RUNTIME_OK), "ok") == 0);
    CHECK(strcmp(az_rev1655_runtime_result_name(
        AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED),
        "thread-startup-rejected") == 0);
    CHECK(strcmp(az_rev1655_runtime_result_name(
        AZ_REV1655_RUNTIME_THREAD_CREATE_FAILED),
        "thread-create-failed") == 0);

    if (failures != 0) {
        fprintf(stderr, "%d Rev1655 runtime assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }

    puts("AuroraAZ Rev1655 observe runtime tests passed");
    return EXIT_SUCCESS;
}
