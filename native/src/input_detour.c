#if !defined(AURORAAZ_XBOX360)
#error "input_detour.c must only be built for the Xbox 360 target"
#endif

#include <stddef.h>
#include <stdint.h>

#include <xecore/xboxkrnl.h>

#include <auroraaz/filters.h>
#include <auroraaz/input.h>
#include <auroraaz/input_detour.h>

#define AZ_RENDER_RESULT_SUCCESS 0
#define AZ_RENDER_SNAPSHOT_ATTEMPTS 3u
#define AZ_INPUT_LIFECYCLE_STAGE_MASK 0x000000FFu
#define AZ_INPUT_LIFECYCLE_RESETTING 0x40000000u
#define AZ_INPUT_LIFECYCLE_SHUTDOWN 0x80000000u
#define AZ_INPUT_LIFECYCLE_CONTROL_MASK \
    (AZ_INPUT_LIFECYCLE_RESETTING | AZ_INPUT_LIFECYCLE_SHUTDOWN)

typedef char AzObservationCapacityMustBePowerOfTwo[
    (AZ_INPUT_DETOUR_OBSERVATION_CAPACITY != 0u &&
     (AZ_INPUT_DETOUR_OBSERVATION_CAPACITY &
      (AZ_INPUT_DETOUR_OBSERVATION_CAPACITY - 1u)) == 0u) ? 1 : -1];
typedef char AzInputKeystrokeSizeMustMatchXdk[
    sizeof(AzInputKeystroke) == 8u ? 1 : -1];
typedef char AzInputKeystrokeVirtualKeyOffsetMustMatchXdk[
    offsetof(AzInputKeystroke, virtual_key) == 0u ? 1 : -1];
typedef char AzInputKeystrokeUnicodeOffsetMustMatchXdk[
    offsetof(AzInputKeystroke, unicode) == 2u ? 1 : -1];
typedef char AzInputKeystrokeFlagsOffsetMustMatchXdk[
    offsetof(AzInputKeystroke, flags) == 4u ? 1 : -1];
typedef char AzInputKeystrokeUserIndexOffsetMustMatchXdk[
    offsetof(AzInputKeystroke, user_index) == 6u ? 1 : -1];
typedef char AzInputKeystrokeHidCodeOffsetMustMatchXdk[
    offsetof(AzInputKeystroke, hid_code) == 7u ? 1 : -1];

typedef uint32_t (*AzRev1655InputFallback)(
    uint32_t user_index,
    uint32_t flags,
    AzInputKeystroke *keystroke);

extern uint32_t az_rev1655_input_original_fallback(
    uint32_t user_index,
    uint32_t flags,
    AzInputKeystroke *keystroke);

typedef struct AzRenderPublication {
    uint32_t serial;
    uint32_t input_frame;
    uint32_t game_content_manager;
    int32_t result;
} AzRenderPublication;

typedef struct AzInputDetourBridge {
    AzInputRuntime runtime;
    AzInputDetourObservation observations[
        AZ_INPUT_DETOUR_OBSERVATION_CAPACITY];
    uint32_t last_render_serial;
    volatile uint32_t lifecycle;
    volatile uint32_t verified_controls;
    volatile uint32_t image_verified;
    volatile uint32_t input_hook_verified;
    volatile uint32_t render_hook_verified;
    volatile uint32_t filter_consumer_verified;
    volatile uint32_t scene_allows_capture;
    volatile uint32_t input_frame;
    volatile uint32_t render_write_lock;
    volatile uint32_t render_sequence;
    volatile uint32_t render_serial_counter;
    volatile uint32_t render_serial;
    volatile uint32_t render_input_frame;
    volatile uint32_t render_game_content_manager;
    volatile int32_t render_result;
    volatile uint32_t observation_head;
    volatile uint32_t observation_tail;
    volatile uint32_t observation_serial;
    volatile uint32_t pending_filter;
    volatile uint32_t filter_in_flight;
    volatile uint32_t selector_snapshot;
    volatile uint32_t effective_stage_snapshot;
    volatile uint32_t consumed_controls_snapshot;
    volatile uint32_t process_guard;
    volatile uint32_t in_flight;
    volatile uint32_t main_calls;
    volatile uint32_t successful_keys;
    volatile uint32_t drain_calls;
    volatile uint32_t unknown_caller_calls;
    volatile uint32_t invalid_keystroke_pointers;
    volatile uint32_t reentrant_calls;
    volatile uint32_t observation_drops;
    volatile uint32_t filter_queue_busy;
    AzRev1655BrowseJumpApply browse_jump_apply;
    void *browse_jump_context;
    AzRev1655UiTick ui_tick;
    void *ui_tick_context;
    AzRev1655UiInput ui_input;
    void *ui_input_context;
    volatile uint32_t browse_jump_pending;
    volatile uint32_t browse_jump_in_flight;
    volatile uint32_t browse_jump_gcm;
    volatile uint32_t browse_jump_target;
    volatile uint32_t browse_jump_count;
    volatile uint32_t browse_jump_queued;
    volatile uint32_t browse_jump_applied;
    volatile uint32_t browse_jump_rejected;
} AzInputDetourBridge;

static AzInputDetourBridge g_input_bridge;

static uint32_t load_u32(const volatile uint32_t *value)
{
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static void store_u32(volatile uint32_t *value, uint32_t replacement)
{
    __atomic_store_n(value, replacement, __ATOMIC_RELEASE);
}

static uint32_t increment_u32(volatile uint32_t *value)
{
    return __atomic_add_fetch(value, 1u, __ATOMIC_ACQ_REL);
}

static uint32_t pack_selector(const AzSelectorState *selector)
{
    return ((uint32_t)selector->mode & 0xFFu) |
        ((uint32_t)selector->selected_index << 8u) |
        ((uint32_t)selector->applied_index << 16u) |
        ((uint32_t)selector->apply_serial << 24u);
}

static void publish_selector(void)
{
    store_u32(
        &g_input_bridge.selector_snapshot,
        pack_selector(&g_input_bridge.runtime.selector));
}

static uint8_t bool_from_atomic(const volatile uint32_t *value)
{
    return load_u32(value) != 0u ? 1u : 0u;
}

static AzInputDetourStage requested_stage_from_lifecycle(void)
{
    return (AzInputDetourStage)(
        load_u32(&g_input_bridge.lifecycle) &
        AZ_INPUT_LIFECYCLE_STAGE_MASK);
}

static uint8_t shutdown_is_requested(void)
{
    return (load_u32(&g_input_bridge.lifecycle) &
        AZ_INPUT_LIFECYCLE_SHUTDOWN) != 0u ? 1u : 0u;
}

static uint8_t address_range_is_valid(const void *address, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)address;
    const uintptr_t start = (uintptr_t)address;

    if (bytes == NULL || size == 0u ||
        start > UINTPTR_MAX - (size - 1u)) {
        return 0u;
    }

    if (!MmIsAddressValid((void *)bytes) ||
        !MmIsAddressValid((void *)(bytes + size - 1u))) {
        return 0u;
    }

    return 1u;
}

static uint8_t selector_prerequisites_are_verified(void)
{
    const uint32_t controls = load_u32(&g_input_bridge.verified_controls);

    if (shutdown_is_requested() != 0u ||
        bool_from_atomic(&g_input_bridge.image_verified) == 0u ||
        bool_from_atomic(&g_input_bridge.input_hook_verified) == 0u ||
        bool_from_atomic(&g_input_bridge.render_hook_verified) == 0u ||
        (controls & AZ_INPUT_VERIFIED_REQUIRED) !=
            AZ_INPUT_VERIFIED_REQUIRED) {
        return 0u;
    }

    return 1u;
}

static uint8_t filter_is_busy(void)
{
    return (load_u32(&g_input_bridge.pending_filter) !=
                AZ_INPUT_DETOUR_NO_FILTER_REQUEST ||
            load_u32(&g_input_bridge.filter_in_flight) != 0u) ? 1u : 0u;
}

static void apply_pending_browse_jump(void)
{
    AzRev1655BrowseJumpApply apply;
    void *context;
    uintptr_t gcm;
    uint32_t target;
    uint32_t count;

    uint32_t ready = 1u;

    if (!__atomic_compare_exchange_n(
            &g_input_bridge.browse_jump_pending,
            &ready,
            0u,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        return;
    }
    if (shutdown_is_requested() != 0u) {
        (void)increment_u32(&g_input_bridge.browse_jump_rejected);
        return;
    }

    apply = g_input_bridge.browse_jump_apply;
    context = g_input_bridge.browse_jump_context;
    gcm = (uintptr_t)load_u32(&g_input_bridge.browse_jump_gcm);
    target = load_u32(&g_input_bridge.browse_jump_target);
    count = load_u32(&g_input_bridge.browse_jump_count);
    if (apply == NULL || gcm == (uintptr_t)0u || count == 0u ||
        target >= count) {
        (void)increment_u32(&g_input_bridge.browse_jump_rejected);
        return;
    }

    store_u32(&g_input_bridge.browse_jump_in_flight, 1u);
    if (apply(context, gcm, target, count) != 0u) {
        (void)increment_u32(&g_input_bridge.browse_jump_applied);
    }
    else {
        (void)increment_u32(&g_input_bridge.browse_jump_rejected);
    }
    store_u32(&g_input_bridge.browse_jump_in_flight, 0u);
}

static uint8_t snapshot_render(AzRenderPublication *publication)
{
    uint32_t attempt;

    if (publication == NULL) {
        return 0u;
    }

    for (attempt = 0u; attempt < AZ_RENDER_SNAPSHOT_ATTEMPTS; ++attempt) {
        uint32_t before = load_u32(&g_input_bridge.render_sequence);
        uint32_t after;

        if ((before & 1u) != 0u) {
            continue;
        }

        publication->serial = load_u32(&g_input_bridge.render_serial);
        publication->input_frame =
            load_u32(&g_input_bridge.render_input_frame);
        publication->game_content_manager =
            load_u32(&g_input_bridge.render_game_content_manager);
        publication->result = __atomic_load_n(
            &g_input_bridge.render_result,
            __ATOMIC_ACQUIRE);

        after = load_u32(&g_input_bridge.render_sequence);
        if (before == after && (after & 1u) == 0u) {
            return 1u;
        }
    }

    return 0u;
}

static void update_coverflow_scope(uint32_t input_frame)
{
    AzRenderPublication publication;

    if (bool_from_atomic(&g_input_bridge.render_hook_verified) == 0u ||
        snapshot_render(&publication) == 0u ||
        publication.serial == 0u ||
        publication.serial == g_input_bridge.last_render_serial) {
        az_input_scope_invalidate(&g_input_bridge.runtime.coverflow);
        return;
    }

    g_input_bridge.last_render_serial = publication.serial;
    if (publication.input_frame + 1u != input_frame ||
        publication.result != AZ_RENDER_RESULT_SUCCESS ||
        publication.game_content_manager == 0u) {
        az_input_scope_invalidate(&g_input_bridge.runtime.coverflow);
        return;
    }

    az_input_scope_note_render(
        &g_input_bridge.runtime.coverflow,
        publication.input_frame,
        (uintptr_t)publication.game_content_manager,
        publication.result);
}

static AzInputStage select_effective_stage(AzInputDetourStage requested)
{
    if (g_input_bridge.runtime.stage == AZ_INPUT_STAGE_CONSUME_VERIFIED &&
        g_input_bridge.runtime.consumed_controls != 0u) {
        return AZ_INPUT_STAGE_CONSUME_VERIFIED;
    }

    if (requested == AZ_INPUT_DETOUR_CONSUME &&
        selector_prerequisites_are_verified() != 0u) {
        return AZ_INPUT_STAGE_CONSUME_VERIFIED;
    }

    return AZ_INPUT_STAGE_OBSERVE_ONLY;
}

static void enqueue_observation(
    const AzInputKeystroke *original_key,
    uint32_t input_frame,
    uint32_t caller_return_address,
    AzInputDetourStage requested_stage,
    AzInputStage effective_stage,
    const AzInputDecision *decision,
    uint8_t filter_queued)
{
    uint32_t head = load_u32(&g_input_bridge.observation_head);
    const uint32_t tail = load_u32(&g_input_bridge.observation_tail);
    AzInputDetourObservation *observation;

    if (head - tail >= AZ_INPUT_DETOUR_OBSERVATION_CAPACITY) {
        (void)increment_u32(&g_input_bridge.observation_drops);
        return;
    }

    observation = &g_input_bridge.observations[
        head & (AZ_INPUT_DETOUR_OBSERVATION_CAPACITY - 1u)];
    observation->serial = increment_u32(&g_input_bridge.observation_serial);
    observation->input_frame = input_frame;
    observation->caller_return_address = caller_return_address;
    observation->keystroke = *original_key;
    observation->translation = decision->translation;
    observation->coverflow_active = decision->coverflow_active;
    observation->would_handle = decision->would_handle;
    observation->consumed = decision->consume;
    observation->filter_queued = filter_queued;
    observation->requested_stage = requested_stage;
    observation->effective_stage = effective_stage;

    ++head;
    store_u32(&g_input_bridge.observation_head, head);
}

void az_rev1655_input_detour_reset(void)
{
    uint32_t index;
    uint32_t lifecycle = load_u32(&g_input_bridge.lifecycle);
    uint32_t reset_marker;

    for (;;) {
        if ((lifecycle & (AZ_INPUT_LIFECYCLE_SHUTDOWN |
                          AZ_INPUT_LIFECYCLE_RESETTING)) != 0u) {
            return;
        }
        reset_marker = AZ_INPUT_LIFECYCLE_RESETTING;
        if (__atomic_compare_exchange_n(
                &g_input_bridge.lifecycle,
                &lifecycle,
                reset_marker,
                0,
                __ATOMIC_SEQ_CST,
                __ATOMIC_ACQUIRE)) {
            break;
        }
    }

    az_input_runtime_init(&g_input_bridge.runtime);
    for (index = 0u; index < AZ_INPUT_DETOUR_OBSERVATION_CAPACITY; ++index) {
        AzInputDetourObservation *observation =
            &g_input_bridge.observations[index];
        observation->serial = 0u;
        observation->input_frame = 0u;
        observation->caller_return_address = 0u;
        observation->keystroke.virtual_key = 0u;
        observation->keystroke.unicode = 0u;
        observation->keystroke.flags = 0u;
        observation->keystroke.user_index = 0u;
        observation->keystroke.hid_code = 0u;
        observation->translation.control = AZ_INPUT_CONTROL_UNKNOWN;
        observation->translation.event = AZ_INPUT_EVENT_INVALID;
        observation->translation.command = AZ_COMMAND_NONE;
        observation->coverflow_active = 0u;
        observation->would_handle = 0u;
        observation->consumed = 0u;
        observation->filter_queued = 0u;
        observation->requested_stage = AZ_INPUT_DETOUR_OFF;
        observation->effective_stage = AZ_INPUT_STAGE_OBSERVE_ONLY;
    }

    g_input_bridge.last_render_serial = 0u;
    store_u32(&g_input_bridge.verified_controls, 0u);
    store_u32(&g_input_bridge.image_verified, 0u);
    store_u32(&g_input_bridge.input_hook_verified, 0u);
    store_u32(&g_input_bridge.render_hook_verified, 0u);
    store_u32(&g_input_bridge.filter_consumer_verified, 0u);
    store_u32(&g_input_bridge.scene_allows_capture, 0u);
    store_u32(&g_input_bridge.input_frame, 0u);
    store_u32(&g_input_bridge.render_write_lock, 0u);
    store_u32(&g_input_bridge.render_sequence, 0u);
    store_u32(&g_input_bridge.render_serial_counter, 0u);
    store_u32(&g_input_bridge.render_serial, 0u);
    store_u32(&g_input_bridge.render_input_frame, 0u);
    store_u32(&g_input_bridge.render_game_content_manager, 0u);
    __atomic_store_n(
        &g_input_bridge.render_result,
        (int32_t)-1,
        __ATOMIC_RELEASE);
    store_u32(&g_input_bridge.observation_head, 0u);
    store_u32(&g_input_bridge.observation_tail, 0u);
    store_u32(&g_input_bridge.observation_serial, 0u);
    store_u32(
        &g_input_bridge.pending_filter,
        AZ_INPUT_DETOUR_NO_FILTER_REQUEST);
    store_u32(&g_input_bridge.filter_in_flight, 0u);
    publish_selector();
    store_u32(
        &g_input_bridge.effective_stage_snapshot,
        AZ_INPUT_STAGE_OBSERVE_ONLY);
    store_u32(&g_input_bridge.consumed_controls_snapshot, 0u);
    store_u32(&g_input_bridge.process_guard, 0u);
    store_u32(&g_input_bridge.in_flight, 0u);
    store_u32(&g_input_bridge.main_calls, 0u);
    store_u32(&g_input_bridge.successful_keys, 0u);
    store_u32(&g_input_bridge.drain_calls, 0u);
    store_u32(&g_input_bridge.unknown_caller_calls, 0u);
    store_u32(&g_input_bridge.invalid_keystroke_pointers, 0u);
    store_u32(&g_input_bridge.reentrant_calls, 0u);
    store_u32(&g_input_bridge.observation_drops, 0u);
    store_u32(&g_input_bridge.filter_queue_busy, 0u);
    g_input_bridge.browse_jump_apply = NULL;
    g_input_bridge.browse_jump_context = NULL;
    g_input_bridge.ui_tick = NULL;
    g_input_bridge.ui_tick_context = NULL;
    g_input_bridge.ui_input = NULL;
    g_input_bridge.ui_input_context = NULL;
    store_u32(&g_input_bridge.browse_jump_pending, 0u);
    store_u32(&g_input_bridge.browse_jump_in_flight, 0u);
    store_u32(&g_input_bridge.browse_jump_gcm, 0u);
    store_u32(&g_input_bridge.browse_jump_target, 0u);
    store_u32(&g_input_bridge.browse_jump_count, 0u);
    store_u32(&g_input_bridge.browse_jump_queued, 0u);
    store_u32(&g_input_bridge.browse_jump_applied, 0u);
    store_u32(&g_input_bridge.browse_jump_rejected, 0u);

    lifecycle = AZ_INPUT_LIFECYCLE_RESETTING;
    (void)__atomic_compare_exchange_n(
        &g_input_bridge.lifecycle,
        &lifecycle,
        (uint32_t)AZ_INPUT_DETOUR_OFF,
        0,
        __ATOMIC_SEQ_CST,
        __ATOMIC_ACQUIRE);
}

void az_rev1655_input_detour_publish_verification(
    uint8_t image_verified,
    uint8_t input_hook_verified,
    uint8_t render_hook_verified,
    uint8_t filter_consumer_verified)
{
    if (shutdown_is_requested() != 0u) {
        image_verified = 0u;
        input_hook_verified = 0u;
        render_hook_verified = 0u;
        filter_consumer_verified = 0u;
    }

    store_u32(
        &g_input_bridge.image_verified,
        image_verified != 0u ? 1u : 0u);
    store_u32(
        &g_input_bridge.input_hook_verified,
        input_hook_verified != 0u ? 1u : 0u);
    store_u32(
        &g_input_bridge.render_hook_verified,
        render_hook_verified != 0u ? 1u : 0u);
    store_u32(
        &g_input_bridge.filter_consumer_verified,
        filter_consumer_verified != 0u ? 1u : 0u);

    /* Close a publication that raced the one-way shutdown store. */
    if (shutdown_is_requested() != 0u) {
        store_u32(&g_input_bridge.image_verified, 0u);
        store_u32(&g_input_bridge.input_hook_verified, 0u);
        store_u32(&g_input_bridge.render_hook_verified, 0u);
        store_u32(&g_input_bridge.filter_consumer_verified, 0u);
    }
}

void az_rev1655_input_detour_confirm_controls(uint32_t verified_controls)
{
    if (shutdown_is_requested() != 0u) {
        verified_controls = 0u;
    }
    store_u32(
        &g_input_bridge.verified_controls,
        verified_controls & AZ_INPUT_VERIFIED_REQUIRED);
    if (shutdown_is_requested() != 0u) {
        store_u32(&g_input_bridge.verified_controls, 0u);
    }
}

void az_rev1655_input_detour_set_scene_allows_capture(uint8_t allowed)
{
    if (shutdown_is_requested() != 0u) {
        allowed = 0u;
    }
    store_u32(
        &g_input_bridge.scene_allows_capture,
        allowed != 0u ? 1u : 0u);
    if (shutdown_is_requested() != 0u) {
        store_u32(&g_input_bridge.scene_allows_capture, 0u);
    }
}

AzInputDetourResult az_rev1655_input_detour_request_stage(
    AzInputDetourStage stage)
{
    uint32_t lifecycle;
    uint32_t replacement;
    AzInputDetourStage published_stage = stage;
    AzInputDetourResult result = AZ_INPUT_DETOUR_OK;

    if (stage != AZ_INPUT_DETOUR_OFF &&
        stage != AZ_INPUT_DETOUR_OBSERVE &&
        stage != AZ_INPUT_DETOUR_CONSUME) {
        (void)__atomic_fetch_and(
            &g_input_bridge.lifecycle,
            AZ_INPUT_LIFECYCLE_CONTROL_MASK,
            __ATOMIC_SEQ_CST);
        return AZ_INPUT_DETOUR_BAD_STAGE;
    }

    if (stage == AZ_INPUT_DETOUR_OBSERVE &&
        (bool_from_atomic(&g_input_bridge.image_verified) == 0u ||
         bool_from_atomic(&g_input_bridge.input_hook_verified) == 0u)) {
        published_stage = AZ_INPUT_DETOUR_OFF;
        result = AZ_INPUT_DETOUR_NOT_VERIFIED;
    }

    if (stage == AZ_INPUT_DETOUR_CONSUME &&
        selector_prerequisites_are_verified() == 0u) {
        const uint8_t observe_verified =
            (bool_from_atomic(&g_input_bridge.image_verified) != 0u &&
             bool_from_atomic(&g_input_bridge.input_hook_verified) != 0u) ?
                1u : 0u;
        published_stage = observe_verified != 0u ?
            AZ_INPUT_DETOUR_OBSERVE : AZ_INPUT_DETOUR_OFF;
        result = AZ_INPUT_DETOUR_NOT_VERIFIED;
    }

    lifecycle = load_u32(&g_input_bridge.lifecycle);
    for (;;) {
        if ((lifecycle & AZ_INPUT_LIFECYCLE_RESETTING) != 0u) {
            return AZ_INPUT_DETOUR_NOT_VERIFIED;
        }
        if ((lifecycle & AZ_INPUT_LIFECYCLE_SHUTDOWN) != 0u) {
            published_stage = AZ_INPUT_DETOUR_OFF;
            if (stage != AZ_INPUT_DETOUR_OFF) {
                result = AZ_INPUT_DETOUR_SHUTTING_DOWN;
            }
        }

        replacement =
            (lifecycle & AZ_INPUT_LIFECYCLE_SHUTDOWN) |
            (uint32_t)published_stage;
        if (__atomic_compare_exchange_n(
                &g_input_bridge.lifecycle,
                &lifecycle,
                replacement,
                0,
                __ATOMIC_SEQ_CST,
                __ATOMIC_ACQUIRE)) {
            return result;
        }
    }
}

void az_rev1655_input_detour_begin_shutdown(void)
{
    store_u32(
        &g_input_bridge.lifecycle,
        AZ_INPUT_LIFECYCLE_SHUTDOWN | AZ_INPUT_DETOUR_OFF);
    store_u32(&g_input_bridge.scene_allows_capture, 0u);
    store_u32(&g_input_bridge.verified_controls, 0u);
    store_u32(&g_input_bridge.image_verified, 0u);
    store_u32(&g_input_bridge.input_hook_verified, 0u);
    store_u32(&g_input_bridge.render_hook_verified, 0u);
    store_u32(&g_input_bridge.filter_consumer_verified, 0u);
    store_u32(&g_input_bridge.browse_jump_pending, 0u);
}

uint8_t az_rev1655_input_detour_shutdown_ready(void)
{
    if (shutdown_is_requested() == 0u ||
        requested_stage_from_lifecycle() != AZ_INPUT_DETOUR_OFF ||
        load_u32(&g_input_bridge.in_flight) != 0u ||
        load_u32(&g_input_bridge.process_guard) != 0u ||
        load_u32(&g_input_bridge.consumed_controls_snapshot) != 0u ||
        load_u32(&g_input_bridge.pending_filter) !=
            AZ_INPUT_DETOUR_NO_FILTER_REQUEST ||
        load_u32(&g_input_bridge.filter_in_flight) != 0u ||
        load_u32(&g_input_bridge.browse_jump_pending) != 0u ||
        load_u32(&g_input_bridge.browse_jump_in_flight) != 0u ||
        load_u32(&g_input_bridge.render_write_lock) != 0u) {
        return 0u;
    }

    return 1u;
}

void az_rev1655_input_detour_note_render(
    uintptr_t game_content_manager,
    int32_t render_result)
{
    uint32_t expected = 0u;
    uint32_t serial;

    /* A new render token invalidates the previous frame's scene decision.
     * The exact final Font::End callback republishes the matching decision. */
    store_u32(&g_input_bridge.scene_allows_capture, 0u);

    if (shutdown_is_requested() != 0u ||
        !__atomic_compare_exchange_n(
            &g_input_bridge.render_write_lock,
            &expected,
            1u,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        return;
    }

    (void)increment_u32(&g_input_bridge.render_sequence);
    serial = increment_u32(&g_input_bridge.render_serial_counter);
    store_u32(&g_input_bridge.render_serial, serial);
    store_u32(
        &g_input_bridge.render_input_frame,
        load_u32(&g_input_bridge.input_frame));
    store_u32(
        &g_input_bridge.render_game_content_manager,
        (uint32_t)game_content_manager);
    __atomic_store_n(
        &g_input_bridge.render_result,
        render_result,
        __ATOMIC_RELEASE);
    (void)increment_u32(&g_input_bridge.render_sequence);
    store_u32(&g_input_bridge.render_write_lock, 0u);
}

void az_rev1655_input_detour_invalidate_render(void)
{
    az_rev1655_input_detour_note_render((uintptr_t)0u, (int32_t)-1);
}

AzInputDetourResult az_rev1655_input_detour_take_observation(
    AzInputDetourObservation *observation)
{
    uint32_t tail;
    uint32_t head;

    if (observation == NULL) {
        return AZ_INPUT_DETOUR_NULL;
    }

    tail = load_u32(&g_input_bridge.observation_tail);
    head = load_u32(&g_input_bridge.observation_head);
    if (tail == head) {
        return AZ_INPUT_DETOUR_NO_OBSERVATION;
    }

    *observation = g_input_bridge.observations[
        tail & (AZ_INPUT_DETOUR_OBSERVATION_CAPACITY - 1u)];
    store_u32(&g_input_bridge.observation_tail, tail + 1u);
    return AZ_INPUT_DETOUR_OK;
}

AzInputDetourResult az_rev1655_input_detour_take_filter_request(
    uint8_t *filter_index)
{
    uint32_t request;
    uint32_t expected = 0u;

    if (filter_index == NULL) {
        return AZ_INPUT_DETOUR_NULL;
    }

    if (!__atomic_compare_exchange_n(
            &g_input_bridge.filter_in_flight,
            &expected,
            1u,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        return AZ_INPUT_DETOUR_FILTER_BUSY;
    }

    request = __atomic_exchange_n(
        &g_input_bridge.pending_filter,
        AZ_INPUT_DETOUR_NO_FILTER_REQUEST,
        __ATOMIC_ACQ_REL);
    if (request >= AZ_GLYPH_COUNT) {
        store_u32(&g_input_bridge.filter_in_flight, 0u);
        return AZ_INPUT_DETOUR_NO_FILTER;
    }

    *filter_index = (uint8_t)request;
    return AZ_INPUT_DETOUR_OK;
}

void az_rev1655_input_detour_finish_filter_request(void)
{
    store_u32(&g_input_bridge.filter_in_flight, 0u);
}

void az_rev1655_input_detour_configure_browse_jump(
    AzRev1655BrowseJumpApply apply,
    void *context)
{
    if (shutdown_is_requested() != 0u ||
        load_u32(&g_input_bridge.browse_jump_pending) != 0u ||
        load_u32(&g_input_bridge.browse_jump_in_flight) != 0u) {
        return;
    }
    g_input_bridge.browse_jump_context = context;
    g_input_bridge.browse_jump_apply = apply;
}

void az_rev1655_input_detour_configure_ui_tick(
    AzRev1655UiTick tick,
    void *context)
{
    if (shutdown_is_requested() != 0u ||
        load_u32(&g_input_bridge.in_flight) != 0u) {
        return;
    }
    g_input_bridge.ui_tick_context = context;
    g_input_bridge.ui_tick = tick;
}

void az_rev1655_input_detour_configure_ui_input(
    AzRev1655UiInput input,
    void *context)
{
    if (shutdown_is_requested() != 0u ||
        load_u32(&g_input_bridge.in_flight) != 0u) {
        return;
    }
    g_input_bridge.ui_input_context = context;
    g_input_bridge.ui_input = input;
}

uint8_t az_rev1655_input_detour_publish_browse_jump(
    uintptr_t game_content_manager,
    uint32_t target_index,
    uint32_t item_count)
{
    uint32_t expected = 0u;

    if (shutdown_is_requested() != 0u ||
        g_input_bridge.browse_jump_apply == NULL ||
        game_content_manager == (uintptr_t)0u ||
        game_content_manager > (uintptr_t)UINT32_MAX ||
        item_count == 0u || target_index >= item_count) {
        (void)increment_u32(&g_input_bridge.browse_jump_rejected);
        return 0u;
    }

    if (!__atomic_compare_exchange_n(
            &g_input_bridge.browse_jump_pending,
            &expected,
            2u,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        (void)increment_u32(&g_input_bridge.browse_jump_rejected);
        return 0u;
    }
    store_u32(
        &g_input_bridge.browse_jump_gcm,
        (uint32_t)game_content_manager);
    store_u32(&g_input_bridge.browse_jump_target, target_index);
    store_u32(&g_input_bridge.browse_jump_count, item_count);
    if (shutdown_is_requested() != 0u) {
        store_u32(&g_input_bridge.browse_jump_pending, 0u);
        (void)increment_u32(&g_input_bridge.browse_jump_rejected);
        return 0u;
    }
    store_u32(&g_input_bridge.browse_jump_pending, 1u);
    (void)increment_u32(&g_input_bridge.browse_jump_queued);
    return 1u;
}

void az_rev1655_input_detour_snapshot_selector(AzSelectorState *selector)
{
    uint32_t packed;

    if (selector == NULL) {
        return;
    }

    packed = load_u32(&g_input_bridge.selector_snapshot);
    selector->mode = (AzSelectorMode)(packed & 0xFFu);
    selector->selected_index = (uint8_t)((packed >> 8u) & 0xFFu);
    selector->applied_index = (uint8_t)((packed >> 16u) & 0xFFu);
    selector->apply_serial = (uint8_t)((packed >> 24u) & 0xFFu);
    selector->selection_changed = 0u;
    if (requested_stage_from_lifecycle() !=
        AZ_INPUT_DETOUR_CONSUME) {
        selector->mode = AZ_MODE_COVERFLOW;
        selector->selected_index = 0u;
    }
}

void az_rev1655_input_detour_snapshot_status(AzInputDetourStatus *status)
{
    if (status == NULL) {
        return;
    }

    status->requested_stage = requested_stage_from_lifecycle();
    status->effective_stage = (AzInputStage)load_u32(
        &g_input_bridge.effective_stage_snapshot);
    status->verified_controls = load_u32(&g_input_bridge.verified_controls);
    status->input_frame = load_u32(&g_input_bridge.input_frame);
    status->main_calls = load_u32(&g_input_bridge.main_calls);
    status->successful_keys = load_u32(&g_input_bridge.successful_keys);
    status->drain_calls = load_u32(&g_input_bridge.drain_calls);
    status->unknown_caller_calls =
        load_u32(&g_input_bridge.unknown_caller_calls);
    status->invalid_keystroke_pointers =
        load_u32(&g_input_bridge.invalid_keystroke_pointers);
    status->reentrant_calls = load_u32(&g_input_bridge.reentrant_calls);
    status->observation_drops =
        load_u32(&g_input_bridge.observation_drops);
    status->filter_queue_busy = load_u32(&g_input_bridge.filter_queue_busy);
    status->browse_jump_queued =
        load_u32(&g_input_bridge.browse_jump_queued);
    status->browse_jump_applied =
        load_u32(&g_input_bridge.browse_jump_applied);
    status->browse_jump_rejected =
        load_u32(&g_input_bridge.browse_jump_rejected);
    status->in_flight = load_u32(&g_input_bridge.in_flight);
    status->consumed_controls = load_u32(
        &g_input_bridge.consumed_controls_snapshot);
    status->pending_filter = load_u32(&g_input_bridge.pending_filter);
    status->image_verified = bool_from_atomic(&g_input_bridge.image_verified);
    status->input_hook_verified =
        bool_from_atomic(&g_input_bridge.input_hook_verified);
    status->render_hook_verified =
        bool_from_atomic(&g_input_bridge.render_hook_verified);
    status->filter_consumer_verified =
        bool_from_atomic(&g_input_bridge.filter_consumer_verified);
    status->scene_allows_capture =
        bool_from_atomic(&g_input_bridge.scene_allows_capture);
    status->filter_in_flight =
        bool_from_atomic(&g_input_bridge.filter_in_flight);
    status->browse_jump_pending =
        bool_from_atomic(&g_input_bridge.browse_jump_pending);
    status->browse_jump_in_flight =
        bool_from_atomic(&g_input_bridge.browse_jump_in_flight);
    status->shutdown_requested = shutdown_is_requested();
}

uint32_t az_rev1655_input_detour_c(
    uint32_t user_index,
    uint32_t flags,
    AzInputKeystroke *keystroke,
    uint32_t caller_return_address)
{
    const AzRev1655InputFallback original =
        &az_rev1655_input_original_fallback;
    uint32_t result;
    uint32_t expected;
    uint32_t input_frame;
    AzInputDetourStage requested_stage;
    AzInputStage effective_stage;
    AzInputGate gate;
    AzInputKeystroke original_key;
    AzInputDecision decision;
    uint8_t filter_queued = 0u;
    uint8_t capture_prerequisites;

    (void)increment_u32(&g_input_bridge.in_flight);
    result = original(user_index, flags, keystroke);

    if (caller_return_address == AZ_REV1655_INPUT_DRAIN_RETURN_ADDRESS) {
        (void)increment_u32(&g_input_bridge.drain_calls);
        (void)__atomic_sub_fetch(
            &g_input_bridge.in_flight,
            1u,
            __ATOMIC_ACQ_REL);
        return result;
    }

    if (caller_return_address != AZ_REV1655_INPUT_MAIN_RETURN_ADDRESS) {
        (void)increment_u32(&g_input_bridge.unknown_caller_calls);
        (void)__atomic_sub_fetch(
            &g_input_bridge.in_flight,
            1u,
            __ATOMIC_ACQ_REL);
        return result;
    }

    (void)increment_u32(&g_input_bridge.main_calls);
    input_frame = increment_u32(&g_input_bridge.input_frame);
    update_coverflow_scope(input_frame);
    apply_pending_browse_jump();
    if (shutdown_is_requested() == 0u && g_input_bridge.ui_tick != NULL) {
        g_input_bridge.ui_tick(g_input_bridge.ui_tick_context);
    }

    if (result != AZ_REV1655_INPUT_RESULT_SUCCESS) {
        (void)__atomic_sub_fetch(
            &g_input_bridge.in_flight,
            1u,
            __ATOMIC_ACQ_REL);
        return result;
    }

    if (address_range_is_valid(keystroke, sizeof(*keystroke)) == 0u) {
        (void)increment_u32(&g_input_bridge.invalid_keystroke_pointers);
        (void)__atomic_sub_fetch(
            &g_input_bridge.in_flight,
            1u,
            __ATOMIC_ACQ_REL);
        return result;
    }

    expected = 0u;
    if (!__atomic_compare_exchange_n(
            &g_input_bridge.process_guard,
            &expected,
            1u,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        (void)increment_u32(&g_input_bridge.reentrant_calls);
        (void)__atomic_sub_fetch(
            &g_input_bridge.in_flight,
            1u,
            __ATOMIC_ACQ_REL);
        return result;
    }

    original_key = *keystroke;
    (void)increment_u32(&g_input_bridge.successful_keys);
    if (shutdown_is_requested() == 0u &&
        g_input_bridge.ui_input != NULL &&
        g_input_bridge.ui_input(
            g_input_bridge.ui_input_context, &original_key) != 0u) {
        keystroke->virtual_key = 0u;
        keystroke->unicode = 0u;
        keystroke->flags = 0u;
        keystroke->user_index = 0u;
        keystroke->hid_code = 0u;
        store_u32(&g_input_bridge.process_guard, 0u);
        (void)__atomic_sub_fetch(
            &g_input_bridge.in_flight,
            1u,
            __ATOMIC_ACQ_REL);
        return result;
    }
    requested_stage = requested_stage_from_lifecycle();
    if (shutdown_is_requested() != 0u) {
        requested_stage = AZ_INPUT_DETOUR_OFF;
    }
    if (requested_stage != AZ_INPUT_DETOUR_OFF &&
        (bool_from_atomic(&g_input_bridge.image_verified) == 0u ||
         bool_from_atomic(&g_input_bridge.input_hook_verified) == 0u)) {
        requested_stage = AZ_INPUT_DETOUR_OFF;
    }
    capture_prerequisites =
        (requested_stage == AZ_INPUT_DETOUR_CONSUME &&
         selector_prerequisites_are_verified() != 0u) ? 1u : 0u;
    effective_stage = select_effective_stage(requested_stage);
    if (g_input_bridge.runtime.stage != effective_stage) {
        az_input_set_stage(&g_input_bridge.runtime, effective_stage);
    }
    store_u32(
        &g_input_bridge.effective_stage_snapshot,
        (uint32_t)effective_stage);

    gate.input_frame = input_frame;
    gate.image_verified = bool_from_atomic(&g_input_bridge.image_verified);
    gate.input_hook_verified =
        bool_from_atomic(&g_input_bridge.input_hook_verified);
    gate.scene_allows_capture =
        bool_from_atomic(&g_input_bridge.scene_allows_capture);
    if (effective_stage == AZ_INPUT_STAGE_CONSUME_VERIFIED &&
        capture_prerequisites == 0u) {
        gate.scene_allows_capture = 0u;
    }

    if (g_input_bridge.runtime.selector.mode == AZ_MODE_COVERFLOW &&
        original_key.virtual_key == AZ_VK_PAD_RTHUMB_PRESS &&
        (filter_is_busy() != 0u ||
         bool_from_atomic(
            &g_input_bridge.filter_consumer_verified) == 0u)) {
        gate.scene_allows_capture = 0u;
    }

    decision = az_input_process(
        &g_input_bridge.runtime,
        &original_key,
        &gate);

    if (decision.consume != 0u &&
        decision.selector_result.request_filter != 0u) {
        uint32_t no_request = AZ_INPUT_DETOUR_NO_FILTER_REQUEST;
        uint32_t filter_index =
            (uint32_t)decision.selector_result.filter_index;

        if (bool_from_atomic(
                &g_input_bridge.filter_consumer_verified) == 0u) {
            /* R3 release always closes the transient overlay. If filtering
             * was revoked while R3 was held, consume the owned release but
             * drop the request rather than leaking input into Aurora. */
            decision.selector_result.request_filter = 0u;
            decision.selector_result.filter_index = AZ_NO_GLYPH;
        }
        else if (load_u32(&g_input_bridge.filter_in_flight) == 0u &&
            filter_index < AZ_GLYPH_COUNT &&
            __atomic_compare_exchange_n(
                &g_input_bridge.pending_filter,
                &no_request,
                filter_index,
                0,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE)) {
            filter_queued = 1u;
        }
        else {
            decision.selector_result.request_filter = 0u;
            decision.selector_result.filter_index = AZ_NO_GLYPH;
            (void)increment_u32(&g_input_bridge.filter_queue_busy);
        }
    }

    az_input_apply_consumption(keystroke, &decision);
    publish_selector();
    store_u32(
        &g_input_bridge.consumed_controls_snapshot,
        g_input_bridge.runtime.consumed_controls);

    if (requested_stage != AZ_INPUT_DETOUR_OFF) {
        enqueue_observation(
            &original_key,
            input_frame,
            caller_return_address,
            requested_stage,
            effective_stage,
            &decision,
            filter_queued);
    }

    store_u32(&g_input_bridge.process_guard, 0u);
    (void)__atomic_sub_fetch(
        &g_input_bridge.in_flight,
        1u,
        __ATOMIC_ACQ_REL);
    return result;
}

const char *az_input_detour_result_name(AzInputDetourResult result)
{
    switch (result) {
    case AZ_INPUT_DETOUR_OK:
        return "ok";
    case AZ_INPUT_DETOUR_NULL:
        return "null";
    case AZ_INPUT_DETOUR_BAD_STAGE:
        return "bad-stage";
    case AZ_INPUT_DETOUR_NOT_VERIFIED:
        return "not-verified";
    case AZ_INPUT_DETOUR_SHUTTING_DOWN:
        return "shutting-down";
    case AZ_INPUT_DETOUR_NO_OBSERVATION:
        return "no-observation";
    case AZ_INPUT_DETOUR_NO_FILTER:
        return "no-filter";
    case AZ_INPUT_DETOUR_FILTER_BUSY:
        return "filter-busy";
    default:
        return "unknown";
    }
}
