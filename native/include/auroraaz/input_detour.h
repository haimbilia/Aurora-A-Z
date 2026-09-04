#ifndef AURORAAZ_INPUT_DETOUR_H
#define AURORAAZ_INPUT_DETOUR_H

#include <stdint.h>

#include <auroraaz/input.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Exact Aurora 0.7b.2 Rev1655 addresses. The entry hook must be guarded by
 * az_input_hook_validate_rev1655() before these values are used.
 */
#define AZ_REV1655_INPUT_MAIN_RETURN_ADDRESS 0x822113F8u
#define AZ_REV1655_INPUT_DRAIN_RETURN_ADDRESS 0x82211ED4u
#define AZ_REV1655_INPUT_WRAPPER_CONTINUE_ADDRESS 0x82801D94u
#define AZ_REV1655_INPUT_WRAPPER_FIRST_INSTRUCTION 0x7D8802A6u

#define AZ_REV1655_INPUT_RESULT_SUCCESS 0u
#define AZ_INPUT_DETOUR_OBSERVATION_CAPACITY 32u
#define AZ_INPUT_DETOUR_NO_FILTER_REQUEST 0xFFFFFFFFu

#define AZ_INPUT_VERIFIED_A (1u << 0u)
#define AZ_INPUT_VERIFIED_R3 (1u << 1u)
#define AZ_INPUT_VERIFIED_DPAD_LEFT (1u << 2u)
#define AZ_INPUT_VERIFIED_DPAD_RIGHT (1u << 3u)
#define AZ_INPUT_VERIFIED_LSTICK_LEFT (1u << 4u)
#define AZ_INPUT_VERIFIED_LSTICK_RIGHT (1u << 5u)
#define AZ_INPUT_VERIFIED_REQUIRED \
    (AZ_INPUT_VERIFIED_R3 | \
     AZ_INPUT_VERIFIED_DPAD_LEFT | AZ_INPUT_VERIFIED_DPAD_RIGHT | \
     AZ_INPUT_VERIFIED_LSTICK_LEFT | AZ_INPUT_VERIFIED_LSTICK_RIGHT)

typedef enum AzInputDetourStage {
    AZ_INPUT_DETOUR_OFF = 0,
    AZ_INPUT_DETOUR_OBSERVE,
    AZ_INPUT_DETOUR_CONSUME
} AzInputDetourStage;

typedef enum AzInputDetourResult {
    AZ_INPUT_DETOUR_OK = 0,
    AZ_INPUT_DETOUR_NULL,
    AZ_INPUT_DETOUR_BAD_STAGE,
    AZ_INPUT_DETOUR_NOT_VERIFIED,
    AZ_INPUT_DETOUR_SHUTTING_DOWN,
    AZ_INPUT_DETOUR_NO_OBSERVATION,
    AZ_INPUT_DETOUR_NO_FILTER,
    AZ_INPUT_DETOUR_FILTER_BUSY
} AzInputDetourResult;

/*
 * ABI of Aurora's wrapper at 0x82801D90, not the imported
 * XamInputGetKeystrokeEx function. Aurora converts user_index to the DWORD*
 * required by the import inside the wrapper.
 */
typedef uint32_t (*AzRev1655InputWrapper)(
    uint32_t user_index,
    uint32_t flags,
    AzInputKeystroke *keystroke);

/*
 * Browse-mode scanning happens on the plugin worker. The resulting movement
 * is handed back to Aurora's main input thread through this callback; the
 * callback must revalidate the live GameContentManager before moving.
 */
typedef uint8_t (*AzRev1655BrowseJumpApply)(
    void *context,
    uintptr_t game_content_manager,
    uint32_t target_index,
    uint32_t item_count);
typedef void (*AzRev1655UiTick)(void *context);

typedef struct AzInputDetourObservation {
    uint32_t serial;
    uint32_t input_frame;
    uint32_t caller_return_address;
    AzInputKeystroke keystroke;
    AzInputTranslation translation;
    uint8_t coverflow_active;
    uint8_t would_handle;
    uint8_t consumed;
    uint8_t filter_queued;
    AzInputDetourStage requested_stage;
    AzInputStage effective_stage;
} AzInputDetourObservation;

typedef struct AzInputDetourStatus {
    AzInputDetourStage requested_stage;
    AzInputStage effective_stage;
    uint32_t verified_controls;
    uint32_t input_frame;
    uint32_t main_calls;
    uint32_t successful_keys;
    uint32_t drain_calls;
    uint32_t unknown_caller_calls;
    uint32_t invalid_keystroke_pointers;
    uint32_t reentrant_calls;
    uint32_t observation_drops;
    uint32_t filter_queue_busy;
    uint32_t browse_jump_queued;
    uint32_t browse_jump_applied;
    uint32_t browse_jump_rejected;
    uint32_t in_flight;
    uint32_t consumed_controls;
    uint32_t pending_filter;
    uint8_t image_verified;
    uint8_t input_hook_verified;
    uint8_t render_hook_verified;
    uint8_t filter_consumer_verified;
    uint8_t scene_allows_capture;
    uint8_t filter_in_flight;
    uint8_t browse_jump_pending;
    uint8_t browse_jump_in_flight;
    uint8_t shutdown_requested;
} AzInputDetourStatus;

/*
 * Call only before the input entry hook is installed. Reset is ignored once
 * one-way shutdown has begun; a live instance cannot be reopened in place.
 */
void az_rev1655_input_detour_reset(void);

/*
 * Publish verification/liveness gates. The first three gates plus confirmed
 * controls admit selector ownership. filter_consumer_verified independently
 * admits A/filter requests; while false, A is consumed without leaving the
 * selector or launching a title. Only exact validators and successfully
 * installed hooks may publish their corresponding gates.
 */
void az_rev1655_input_detour_publish_verification(
    uint8_t image_verified,
    uint8_t input_hook_verified,
    uint8_t render_hook_verified,
    uint8_t filter_consumer_verified);

/* Hardware-confirmed controls only; observations do not self-certify keys. */
void az_rev1655_input_detour_confirm_controls(uint32_t verified_controls);

/* A modal/scene probe owns this dynamic gate. It defaults to false. */
void az_rev1655_input_detour_set_scene_allows_capture(uint8_t allowed);

AzInputDetourResult az_rev1655_input_detour_request_stage(
    AzInputDetourStage stage);

/*
 * Atomically closes the stage-control plane and requests OFF. Once shutdown
 * begins, verification publication and later stage requests cannot reopen
 * input until reset() is called for a new, not-yet-installed hook instance.
 */
void az_rev1655_input_detour_begin_shutdown(void);

/*
 * Input-side drain signal only. Module unload additionally requires every
 * resident AzLiveHook admission relay to report az_live_hook_can_unload().
 */
uint8_t az_rev1655_input_detour_shutdown_ready(void);

/*
 * Called after the original RenderMenu returns. The bridge correlates this
 * report with the next main input call; stale or failed reports invalidate the
 * coverflow scope.
 */
void az_rev1655_input_detour_note_render(
    uintptr_t game_content_manager,
    int32_t render_result);

void az_rev1655_input_detour_invalidate_render(void);

/* Worker-thread APIs: neither logging nor filter application runs in-hook. */
AzInputDetourResult az_rev1655_input_detour_take_observation(
    AzInputDetourObservation *observation);

AzInputDetourResult az_rev1655_input_detour_take_filter_request(
    uint8_t *filter_index);

void az_rev1655_input_detour_finish_filter_request(void);

/* Configure before installing the input hook. Passing NULL disables jumps. */
void az_rev1655_input_detour_configure_browse_jump(
    AzRev1655BrowseJumpApply apply,
    void *context);

/* Optional main-input-thread callback configured before hook publication. */
void az_rev1655_input_detour_configure_ui_tick(
    AzRev1655UiTick tick,
    void *context);

/* Worker-thread producer; the callback itself runs on the next main poll. */
uint8_t az_rev1655_input_detour_publish_browse_jump(
    uintptr_t game_content_manager,
    uint32_t target_index,
    uint32_t item_count);

void az_rev1655_input_detour_snapshot_selector(AzSelectorState *selector);
void az_rev1655_input_detour_snapshot_status(AzInputDetourStatus *status);

/*
 * Hook target. The assembly shim captures the unmodified caller LR in r6 and
 * pairs with hook_runtime's resident admission relay (state pointer in r0).
 * Install a non-linking admitted hook only after all expected bytes match.
 */
uint32_t az_rev1655_input_detour_entry(
    uint32_t user_index,
    uint32_t flags,
    AzInputKeystroke *keystroke);

/* Low-linked direct-entry variant; uses the same C bridge without a relay. */
uint32_t az_rev1655_input_direct_detour_entry(
    uint32_t user_index,
    uint32_t flags,
    AzInputKeystroke *keystroke);

/* Exported for the assembly shim; hook installers should use _entry above. */
uint32_t az_rev1655_input_detour_c(
    uint32_t user_index,
    uint32_t flags,
    AzInputKeystroke *keystroke,
    uint32_t caller_return_address);

const char *az_input_detour_result_name(AzInputDetourResult result);

#ifdef __cplusplus
}
#endif

#endif
