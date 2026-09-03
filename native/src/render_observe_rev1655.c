#include <auroraaz/render_observe_rev1655.h>

#include <limits.h>
#include <string.h>

#define AZ_RENDER_OBSERVE_SCOPE_SEAL 0x415A5343u
#define AZ_RENDER_OBSERVE_LOCK_ATTEMPTS 4u

typedef char AzRenderObserveSceneReasonCountMustMatch[
    AZ_RENDER_OBSERVE_REV1655_SCENE_REASON_COUNT ==
        (uint32_t)AZ_SCENE_GATE_REASON_MAIN_FOCUSED + 1u ? 1 : -1];
typedef char AzRenderObserveSceneCountersMustReachCallerAddresses[
    AZ_RENDER_OBSERVE_SCENE_REASON_OFFSET(
        AZ_RENDER_OBSERVE_REV1655_SCENE_REASON_COUNT - 1u) + 4u ==
            AZ_RENDER_OBSERVE_OFF_RENDER_CALLER_ADDRESSES ? 1 : -1];
typedef char AzRenderObserveCallerAddressesMustReachCounts[
    AZ_RENDER_OBSERVE_CALLER_ADDRESS_OFFSET(
        AZ_RENDER_OBSERVE_REV1655_MAX_CALLERS - 1u) + 4u ==
            AZ_RENDER_OBSERVE_OFF_RENDER_CALLER_COUNTS ? 1 : -1];
typedef char AzRenderObserveCallerCountsMustReachOverflow[
    AZ_RENDER_OBSERVE_CALLER_COUNT_OFFSET(
        AZ_RENDER_OBSERVE_REV1655_MAX_CALLERS - 1u) + 2u ==
            AZ_RENDER_OBSERVE_OFF_RENDER_CALLER_OVERFLOW ? 1 : -1];
typedef char AzRenderObserveRecordLayoutMustFit[
    AZ_RENDER_OBSERVE_OFF_CRC32 + 4u ==
        AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE ? 1 : -1];

typedef struct AzRenderObserveSnapshot {
    uint32_t generation;
    uint32_t safety_flags;
    uint32_t contention_drops;
    uint32_t unarmed_events;
    uint32_t invalid_events;
    uint32_t nesting_underflows;
    uint32_t render_menu_enters;
    uint32_t render_menu_exits;
    uint32_t render_menu_successes;
    uint32_t render_menu_nonzero;
    uint32_t font_end_enters;
    uint32_t font_end_exits;
    uint32_t font_expected_callers;
    uint32_t font_unexpected_callers;
    uint32_t render_menu_nesting;
    uint32_t render_menu_max_nesting;
    uint32_t font_end_nesting;
    uint32_t font_end_max_nesting;
    uint32_t cross_nesting;
    uint32_t pointer_anomalies;
    uint32_t device_missing;
    uint32_t scene_samples;
    uint32_t scene_allowed;
    uint32_t scene_denied;
    uint32_t last_render_menu_lr;
    uint32_t last_font_end_lr;
    uint32_t last_manager;
    uint32_t last_success_manager;
    uint32_t last_font;
    uint32_t last_render_device;
    uint32_t last_font_device;
    int32_t last_render_result;
    uint32_t last_scene_reason;
    uint32_t last_cache_head;
    uint32_t last_main_node;
    uint32_t last_main_handle;
    uint32_t last_scanned_nodes;
    uint8_t last_scene_allows;
    uint8_t armed;
    uint8_t exact_image_verified;
    uint32_t scene_reason_counts[
        AZ_RENDER_OBSERVE_REV1655_SCENE_REASON_COUNT];
    uint32_t render_caller_addresses[
        AZ_RENDER_OBSERVE_REV1655_MAX_CALLERS];
    uint32_t render_caller_counts[
        AZ_RENDER_OBSERVE_REV1655_MAX_CALLERS];
    uint32_t render_caller_overflow;
    uint32_t last_unexpected_font_lr;
} AzRenderObserveSnapshot;

static uint32_t increment_saturated_u32(uint32_t value)
{
    return value == UINT32_MAX ? UINT32_MAX : value + 1u;
}

static uint16_t clamp_u16(uint32_t value)
{
    return value > (uint32_t)UINT16_MAX ? UINT16_MAX : (uint16_t)value;
}

static void atomic_increment_saturated(volatile uint32_t *value)
{
    uint32_t attempt;

    for (attempt = 0u; attempt < AZ_RENDER_OBSERVE_LOCK_ATTEMPTS; ++attempt) {
        uint32_t current = __atomic_load_n(value, __ATOMIC_ACQUIRE);
        uint32_t replacement;

        if (current == UINT32_MAX) {
            return;
        }
        replacement = current + 1u;
        if (__atomic_compare_exchange_n(
                value,
                &current,
                replacement,
                0,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE)) {
            return;
        }
    }
}

static void mark_changed(AzRenderObserveRev1655 *state)
{
    uint32_t revision;

    revision = __atomic_add_fetch(
        &state->revision,
        1u,
        __ATOMIC_ACQ_REL);
    if (revision == 0u) {
        __atomic_store_n(&state->revision, 1u, __ATOMIC_RELEASE);
    }
    __atomic_store_n(&state->generation_assigned, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&state->dirty, 1u, __ATOMIC_RELEASE);
}

static void record_contention(AzRenderObserveRev1655 *state)
{
    atomic_increment_saturated(&state->contention_drops);
    (void)__atomic_fetch_or(
        &state->safety_flags,
        AZ_RENDER_OBSERVE_SAFETY_WRITER_CONTENTION,
        __ATOMIC_ACQ_REL);
    mark_changed(state);
}

static uint8_t try_lock(
    AzRenderObserveRev1655 *state,
    uint8_t report_contention)
{
    uint32_t attempt;

    for (attempt = 0u; attempt < AZ_RENDER_OBSERVE_LOCK_ATTEMPTS; ++attempt) {
        uint32_t expected = 0u;

        if (__atomic_compare_exchange_n(
                &state->writer_lock,
                &expected,
                1u,
                0,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE)) {
            return 1u;
        }
    }

    if (report_contention != 0u) {
        record_contention(state);
    }
    return 0u;
}

static void unlock(AzRenderObserveRev1655 *state)
{
    __atomic_store_n(&state->writer_lock, 0u, __ATOMIC_RELEASE);
}

static uint8_t is_armed(const AzRenderObserveRev1655 *state)
{
    return __atomic_load_n(&state->armed, __ATOMIC_ACQUIRE) != 0u ? 1u : 0u;
}

static void add_safety_flag(
    AzRenderObserveRev1655 *state,
    uint32_t flag)
{
    (void)__atomic_fetch_or(
        &state->safety_flags,
        flag,
        __ATOMIC_ACQ_REL);
}

static void clear_scope(AzRenderObserveScope *scope)
{
    if (scope != NULL) {
        memset(scope, 0, sizeof(*scope));
    }
}

static void note_unarmed(AzRenderObserveRev1655 *state)
{
    if (try_lock(state, 1u) == 0u) {
        return;
    }
    state->unarmed_events = increment_saturated_u32(
        state->unarmed_events);
    add_safety_flag(state, AZ_RENDER_OBSERVE_SAFETY_UNARMED_EVENT);
    mark_changed(state);
    unlock(state);
}

static uint8_t caller_lr_is_valid(uint32_t caller_lr)
{
    return caller_lr >= AZ_RENDER_OBSERVE_REV1655_IMAGE_TEXT_BEGIN &&
        caller_lr < AZ_RENDER_OBSERVE_REV1655_IMAGE_TEXT_END &&
        (caller_lr & 3u) == 0u ? 1u : 0u;
}

static uint32_t note_pointer(
    AzRenderObserveRev1655 *state,
    uintptr_t pointer,
    uint8_t allow_null)
{
    if (pointer == (uintptr_t)0u) {
        if (allow_null != 0u) {
            state->device_missing = increment_saturated_u32(
                state->device_missing);
        }
        else {
            state->pointer_anomalies = increment_saturated_u32(
                state->pointer_anomalies);
            add_safety_flag(
                state,
                AZ_RENDER_OBSERVE_SAFETY_INVALID_POINTER);
        }
        return 0u;
    }
    if (pointer > (uintptr_t)UINT32_MAX || (pointer & (uintptr_t)3u) != 0u) {
        state->pointer_anomalies = increment_saturated_u32(
            state->pointer_anomalies);
        add_safety_flag(state, AZ_RENDER_OBSERVE_SAFETY_INVALID_POINTER);
        return 0u;
    }
    return (uint32_t)pointer;
}

static uint32_t note_caller_lr(
    AzRenderObserveRev1655 *state,
    uint32_t caller_lr)
{
    if (caller_lr_is_valid(caller_lr) == 0u) {
        add_safety_flag(state, AZ_RENDER_OBSERVE_SAFETY_INVALID_CALLER_LR);
        return 0u;
    }
    return caller_lr;
}

static void note_render_caller(
    AzRenderObserveRev1655 *state,
    uint32_t caller_lr)
{
    uint32_t slot;

    if (caller_lr == 0u) {
        return;
    }
    for (slot = 0u; slot < AZ_RENDER_OBSERVE_REV1655_MAX_CALLERS; ++slot) {
        if (state->render_caller_addresses[slot] == caller_lr) {
            state->render_caller_counts[slot] = increment_saturated_u32(
                state->render_caller_counts[slot]);
            return;
        }
        if (state->render_caller_addresses[slot] == 0u) {
            state->render_caller_addresses[slot] = caller_lr;
            state->render_caller_counts[slot] = 1u;
            return;
        }
    }
    state->render_caller_overflow = increment_saturated_u32(
        state->render_caller_overflow);
}

static void grow_nesting(
    AzRenderObserveRev1655 *state,
    volatile uint32_t *nesting,
    uint32_t *max_nesting,
    uint32_t overflow_flag,
    AzRenderObserveScope *scope)
{
    uint32_t current;
    uint32_t replacement;

    for (;;) {
        current = __atomic_load_n(nesting, __ATOMIC_ACQUIRE);
        if (current >= AZ_RENDER_OBSERVE_REV1655_MAX_NESTING) {
            add_safety_flag(state, overflow_flag);
            scope->depth_accounted = 0u;
            return;
        }
        replacement = current + 1u;
        if (__atomic_compare_exchange_n(
                nesting,
                &current,
                replacement,
                0,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE)) {
            break;
        }
    }
    if (*max_nesting < replacement) {
        *max_nesting = replacement;
    }
    scope->depth_accounted = 1u;
}

static void shrink_nesting(
    AzRenderObserveRev1655 *state,
    volatile uint32_t *nesting,
    const AzRenderObserveScope *scope)
{
    uint32_t current;

    if (scope->depth_accounted == 0u) {
        return;
    }
    for (;;) {
        current = __atomic_load_n(nesting, __ATOMIC_ACQUIRE);
        if (current == 0u) {
            atomic_increment_saturated(&state->nesting_underflows);
            add_safety_flag(
                state,
                AZ_RENDER_OBSERVE_SAFETY_NESTING_UNDERFLOW);
            return;
        }
        if (__atomic_compare_exchange_n(
                nesting,
                &current,
                current - 1u,
                0,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE)) {
            return;
        }
    }
}

static AzRenderObserveResult close_scope_after_contention(
    AzRenderObserveRev1655 *state,
    volatile uint32_t *nesting,
    AzRenderObserveScope *scope)
{
    /*
     * The scope is stack-local to the intercepted call, so returning with it
     * active would strand the globally visible nesting depth forever.  The
     * depth fields are atomic specifically so this minimal close can proceed
     * without the observational writer lock.  Publish the contention change
     * only after the depth is closed so a snapshot token can never cover an
     * earlier image that omitted the close.
     */
    shrink_nesting(state, nesting, scope);
    scope->active = 0u;
    record_contention(state);
    return AZ_RENDER_OBSERVE_BUSY;
}

static uint8_t scope_is_valid(
    const AzRenderObserveRev1655 *state,
    const AzRenderObserveScope *scope,
    AzRenderObserveScopeKind kind)
{
    return scope != NULL && scope->owner == state &&
        scope->seal == AZ_RENDER_OBSERVE_SCOPE_SEAL &&
        scope->kind == (uint32_t)kind && scope->active == 1u &&
        scope->depth_accounted <= 1u ? 1u : 0u;
}

static AzRenderObserveResult reject_bad_scope(
    AzRenderObserveRev1655 *state)
{
    if (try_lock(state, 1u) == 0u) {
        return AZ_RENDER_OBSERVE_BUSY;
    }
    state->invalid_events = increment_saturated_u32(state->invalid_events);
    add_safety_flag(state, AZ_RENDER_OBSERVE_SAFETY_SCOPE_MISMATCH);
    mark_changed(state);
    unlock(state);
    return AZ_RENDER_OBSERVE_INVALID_SCOPE;
}

void az_render_observe_rev1655_init(
    AzRenderObserveRev1655 *state,
    uint8_t exact_image_verified)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->revision = 1u;
    state->dirty = 1u;
    state->exact_image_verified = exact_image_verified != 0u ? 1u : 0u;
    state->armed = exact_image_verified != 0u ? 1u : 0u;
    state->last_render_result = -1;
    state->last_scene_reason = AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED;
}

AzRenderObserveResult az_render_observe_rev1655_disarm(
    AzRenderObserveRev1655 *state)
{
    if (state == NULL) {
        return AZ_RENDER_OBSERVE_NULL;
    }
    if (try_lock(state, 0u) == 0u) {
        return AZ_RENDER_OBSERVE_BUSY;
    }
    if (is_armed(state) == 0u) {
        unlock(state);
        return AZ_RENDER_OBSERVE_NO_CHANGE;
    }
    __atomic_store_n(&state->armed, 0u, __ATOMIC_RELEASE);
    mark_changed(state);
    unlock(state);
    return AZ_RENDER_OBSERVE_OK;
}

AzRenderObserveResult az_render_observe_render_menu_begin(
    AzRenderObserveRev1655 *state,
    uintptr_t game_content_manager,
    uint32_t caller_lr,
    uintptr_t device,
    AzRenderObserveScope *scope)
{
    uint32_t manager;
    uint32_t normalized_lr;

    if (state == NULL || scope == NULL) {
        return AZ_RENDER_OBSERVE_NULL;
    }
    clear_scope(scope);
    if (is_armed(state) == 0u) {
        note_unarmed(state);
        return AZ_RENDER_OBSERVE_NOT_ARMED;
    }
    if (try_lock(state, 1u) == 0u) {
        return AZ_RENDER_OBSERVE_BUSY;
    }
    if (is_armed(state) == 0u) {
        state->unarmed_events = increment_saturated_u32(
            state->unarmed_events);
        add_safety_flag(state, AZ_RENDER_OBSERVE_SAFETY_UNARMED_EVENT);
        mark_changed(state);
        unlock(state);
        return AZ_RENDER_OBSERVE_NOT_ARMED;
    }

    manager = note_pointer(state, game_content_manager, 0u);
    normalized_lr = note_caller_lr(state, caller_lr);
    state->last_manager = manager;
    state->last_render_menu_lr = normalized_lr;
    state->last_render_device = note_pointer(state, device, 1u);
    state->render_menu_enters = increment_saturated_u32(
        state->render_menu_enters);
    note_render_caller(state, normalized_lr);

    scope->owner = state;
    scope->seal = AZ_RENDER_OBSERVE_SCOPE_SEAL;
    scope->kind = (uint32_t)AZ_RENDER_OBSERVE_SCOPE_RENDER_MENU;
    scope->subject = manager;
    scope->caller_lr = normalized_lr;
    scope->active = 1u;
    grow_nesting(
        state,
        &state->render_menu_nesting,
        &state->render_menu_max_nesting,
        AZ_RENDER_OBSERVE_SAFETY_RENDER_NESTING_OVERFLOW,
        scope);
    mark_changed(state);
    unlock(state);
    return AZ_RENDER_OBSERVE_OK;
}

AzRenderObserveResult az_render_observe_render_menu_end(
    AzRenderObserveRev1655 *state,
    AzRenderObserveScope *scope,
    int32_t render_result,
    uintptr_t device)
{
    if (state == NULL || scope == NULL) {
        return AZ_RENDER_OBSERVE_NULL;
    }
    if (scope_is_valid(
            state,
            scope,
            AZ_RENDER_OBSERVE_SCOPE_RENDER_MENU) == 0u) {
        return reject_bad_scope(state);
    }
    if (try_lock(state, 0u) == 0u) {
        return close_scope_after_contention(
            state,
            &state->render_menu_nesting,
            scope);
    }
    if (scope_is_valid(
            state,
            scope,
            AZ_RENDER_OBSERVE_SCOPE_RENDER_MENU) == 0u) {
        unlock(state);
        return reject_bad_scope(state);
    }

    state->render_menu_exits = increment_saturated_u32(
        state->render_menu_exits);
    state->last_render_result = render_result;
    state->last_manager = scope->subject;
    state->last_render_menu_lr = scope->caller_lr;
    state->last_render_device = note_pointer(state, device, 1u);
    if (render_result == 0) {
        state->render_menu_successes = increment_saturated_u32(
            state->render_menu_successes);
        state->last_success_manager = scope->subject;
    }
    else {
        state->render_menu_nonzero = increment_saturated_u32(
            state->render_menu_nonzero);
    }
    shrink_nesting(state, &state->render_menu_nesting, scope);
    scope->active = 0u;
    mark_changed(state);
    unlock(state);
    return AZ_RENDER_OBSERVE_OK;
}

AzRenderObserveResult az_render_observe_font_end_begin(
    AzRenderObserveRev1655 *state,
    uintptr_t font,
    uint32_t caller_lr,
    uintptr_t device,
    AzRenderObserveScope *scope)
{
    uint32_t normalized_lr;

    if (state == NULL || scope == NULL) {
        return AZ_RENDER_OBSERVE_NULL;
    }
    clear_scope(scope);
    if (is_armed(state) == 0u) {
        note_unarmed(state);
        return AZ_RENDER_OBSERVE_NOT_ARMED;
    }
    if (try_lock(state, 1u) == 0u) {
        return AZ_RENDER_OBSERVE_BUSY;
    }
    if (is_armed(state) == 0u) {
        state->unarmed_events = increment_saturated_u32(
            state->unarmed_events);
        add_safety_flag(state, AZ_RENDER_OBSERVE_SAFETY_UNARMED_EVENT);
        mark_changed(state);
        unlock(state);
        return AZ_RENDER_OBSERVE_NOT_ARMED;
    }

    normalized_lr = note_caller_lr(state, caller_lr);
    state->last_font = note_pointer(state, font, 0u);
    state->last_font_end_lr = normalized_lr;
    state->last_font_device = note_pointer(state, device, 1u);
    state->font_end_enters = increment_saturated_u32(state->font_end_enters);
    if (caller_lr == AZ_RENDER_OBSERVE_REV1655_FONT_END_CALLER_LR) {
        state->font_expected_callers = increment_saturated_u32(
            state->font_expected_callers);
    }
    else {
        state->font_unexpected_callers = increment_saturated_u32(
            state->font_unexpected_callers);
        state->last_unexpected_font_lr = normalized_lr;
        add_safety_flag(
            state,
            AZ_RENDER_OBSERVE_SAFETY_UNEXPECTED_FONT_CALLER);
    }
    if (__atomic_load_n(
            &state->render_menu_nesting,
            __ATOMIC_ACQUIRE) != 0u) {
        state->cross_nesting = increment_saturated_u32(
            state->cross_nesting);
        add_safety_flag(state, AZ_RENDER_OBSERVE_SAFETY_CROSS_NESTING);
    }

    scope->owner = state;
    scope->seal = AZ_RENDER_OBSERVE_SCOPE_SEAL;
    scope->kind = (uint32_t)AZ_RENDER_OBSERVE_SCOPE_FONT_END;
    scope->subject = state->last_font;
    scope->caller_lr = normalized_lr;
    scope->active = 1u;
    grow_nesting(
        state,
        &state->font_end_nesting,
        &state->font_end_max_nesting,
        AZ_RENDER_OBSERVE_SAFETY_FONT_NESTING_OVERFLOW,
        scope);
    mark_changed(state);
    unlock(state);
    return AZ_RENDER_OBSERVE_OK;
}

AzRenderObserveResult az_render_observe_font_end_end(
    AzRenderObserveRev1655 *state,
    AzRenderObserveScope *scope,
    uintptr_t device)
{
    if (state == NULL || scope == NULL) {
        return AZ_RENDER_OBSERVE_NULL;
    }
    if (scope_is_valid(
            state,
            scope,
            AZ_RENDER_OBSERVE_SCOPE_FONT_END) == 0u) {
        return reject_bad_scope(state);
    }
    if (try_lock(state, 0u) == 0u) {
        return close_scope_after_contention(
            state,
            &state->font_end_nesting,
            scope);
    }
    if (scope_is_valid(
            state,
            scope,
            AZ_RENDER_OBSERVE_SCOPE_FONT_END) == 0u) {
        unlock(state);
        return reject_bad_scope(state);
    }

    state->font_end_exits = increment_saturated_u32(state->font_end_exits);
    state->last_font = scope->subject;
    state->last_font_end_lr = scope->caller_lr;
    state->last_font_device = note_pointer(state, device, 1u);
    shrink_nesting(state, &state->font_end_nesting, scope);
    scope->active = 0u;
    mark_changed(state);
    unlock(state);
    return AZ_RENDER_OBSERVE_OK;
}

static uint8_t scene_decision_is_valid(
    const AzSceneGateDecision *decision)
{
    const uint32_t reason = (uint32_t)decision->reason;
    const uint8_t expected_allow =
        decision->reason == AZ_SCENE_GATE_REASON_MAIN_FOCUSED ? 1u : 0u;

    if (reason >= AZ_RENDER_OBSERVE_REV1655_SCENE_REASON_COUNT ||
        decision->allows_capture > 1u ||
        decision->allows_capture != expected_allow ||
        decision->scanned_nodes > AZ_REV1655_SCENE_MAX_CACHE_NODES) {
        return 0u;
    }
    if ((decision->cache_head != 0u &&
            (decision->cache_head & 3u) != 0u) ||
        (decision->main_scene_node != 0u &&
            (decision->main_scene_node & 3u) != 0u)) {
        return 0u;
    }
    return 1u;
}

AzRenderObserveResult az_render_observe_note_scene(
    AzRenderObserveRev1655 *state,
    const AzSceneGateDecision *decision)
{
    const uint32_t reason = decision != NULL ?
        (uint32_t)decision->reason : 0u;

    if (state == NULL || decision == NULL) {
        return AZ_RENDER_OBSERVE_NULL;
    }
    if (is_armed(state) == 0u) {
        note_unarmed(state);
        return AZ_RENDER_OBSERVE_NOT_ARMED;
    }
    if (try_lock(state, 1u) == 0u) {
        return AZ_RENDER_OBSERVE_BUSY;
    }
    if (is_armed(state) == 0u) {
        state->unarmed_events = increment_saturated_u32(
            state->unarmed_events);
        add_safety_flag(state, AZ_RENDER_OBSERVE_SAFETY_UNARMED_EVENT);
        mark_changed(state);
        unlock(state);
        return AZ_RENDER_OBSERVE_NOT_ARMED;
    }
    if (scene_decision_is_valid(decision) == 0u) {
        state->invalid_events = increment_saturated_u32(
            state->invalid_events);
        add_safety_flag(state, AZ_RENDER_OBSERVE_SAFETY_INVALID_SCENE);
        mark_changed(state);
        unlock(state);
        return AZ_RENDER_OBSERVE_INVALID_ARGUMENT;
    }

    state->scene_samples = increment_saturated_u32(state->scene_samples);
    state->scene_reason_counts[reason] = increment_saturated_u32(
        state->scene_reason_counts[reason]);
    if (decision->allows_capture != 0u) {
        state->scene_allowed = increment_saturated_u32(
            state->scene_allowed);
    }
    else {
        state->scene_denied = increment_saturated_u32(state->scene_denied);
    }
    state->last_scene_reason = decision->reason;
    state->last_cache_head = decision->cache_head;
    state->last_main_node = decision->main_scene_node;
    state->last_main_handle = decision->main_scene_handle;
    state->last_scanned_nodes = decision->scanned_nodes;
    state->last_scene_allows = decision->allows_capture;
    mark_changed(state);
    unlock(state);
    return AZ_RENDER_OBSERVE_OK;
}

uint8_t az_render_observe_rev1655_is_dirty(
    const AzRenderObserveRev1655 *state)
{
    return state != NULL &&
        __atomic_load_n(&state->dirty, __ATOMIC_ACQUIRE) != 0u ? 1u : 0u;
}

AzRenderObserveResult az_render_observe_rev1655_seed_generation(
    AzRenderObserveRev1655 *state,
    uint32_t generation)
{
    if (state == NULL) {
        return AZ_RENDER_OBSERVE_NULL;
    }
    if (try_lock(state, 0u) == 0u) {
        return AZ_RENDER_OBSERVE_BUSY;
    }
    if (state->generation != 0u ||
        __atomic_load_n(
            &state->generation_assigned,
            __ATOMIC_ACQUIRE) != 0u) {
        unlock(state);
        return AZ_RENDER_OBSERVE_INVALID_ARGUMENT;
    }
    state->generation = generation;
    unlock(state);
    return AZ_RENDER_OBSERVE_OK;
}

static void capture_snapshot(
    const AzRenderObserveRev1655 *state,
    AzRenderObserveSnapshot *snapshot)
{
    uint32_t index;

    snapshot->generation = state->generation;
    snapshot->safety_flags = __atomic_load_n(
        &state->safety_flags,
        __ATOMIC_ACQUIRE);
    snapshot->contention_drops = __atomic_load_n(
        &state->contention_drops,
        __ATOMIC_ACQUIRE);
    snapshot->unarmed_events = state->unarmed_events;
    snapshot->invalid_events = state->invalid_events;
    snapshot->nesting_underflows = __atomic_load_n(
        &state->nesting_underflows,
        __ATOMIC_ACQUIRE);
    snapshot->render_menu_enters = state->render_menu_enters;
    snapshot->render_menu_exits = state->render_menu_exits;
    snapshot->render_menu_successes = state->render_menu_successes;
    snapshot->render_menu_nonzero = state->render_menu_nonzero;
    snapshot->font_end_enters = state->font_end_enters;
    snapshot->font_end_exits = state->font_end_exits;
    snapshot->font_expected_callers = state->font_expected_callers;
    snapshot->font_unexpected_callers = state->font_unexpected_callers;
    snapshot->render_menu_nesting = __atomic_load_n(
        &state->render_menu_nesting,
        __ATOMIC_ACQUIRE);
    snapshot->render_menu_max_nesting = state->render_menu_max_nesting;
    snapshot->font_end_nesting = __atomic_load_n(
        &state->font_end_nesting,
        __ATOMIC_ACQUIRE);
    snapshot->font_end_max_nesting = state->font_end_max_nesting;
    snapshot->cross_nesting = state->cross_nesting;
    snapshot->pointer_anomalies = state->pointer_anomalies;
    snapshot->device_missing = state->device_missing;
    snapshot->scene_samples = state->scene_samples;
    snapshot->scene_allowed = state->scene_allowed;
    snapshot->scene_denied = state->scene_denied;
    snapshot->last_render_menu_lr = state->last_render_menu_lr;
    snapshot->last_font_end_lr = state->last_font_end_lr;
    snapshot->last_manager = state->last_manager;
    snapshot->last_success_manager = state->last_success_manager;
    snapshot->last_font = state->last_font;
    snapshot->last_render_device = state->last_render_device;
    snapshot->last_font_device = state->last_font_device;
    snapshot->last_render_result = state->last_render_result;
    snapshot->last_scene_reason = (uint32_t)state->last_scene_reason;
    snapshot->last_cache_head = state->last_cache_head;
    snapshot->last_main_node = state->last_main_node;
    snapshot->last_main_handle = state->last_main_handle;
    snapshot->last_scanned_nodes = state->last_scanned_nodes;
    snapshot->last_scene_allows = state->last_scene_allows;
    snapshot->armed = is_armed(state);
    snapshot->exact_image_verified = __atomic_load_n(
        &state->exact_image_verified,
        __ATOMIC_ACQUIRE) != 0u ? 1u : 0u;
    for (index = 0u;
         index < AZ_RENDER_OBSERVE_REV1655_SCENE_REASON_COUNT;
         ++index) {
        snapshot->scene_reason_counts[index] =
            state->scene_reason_counts[index];
    }
    for (index = 0u;
         index < AZ_RENDER_OBSERVE_REV1655_MAX_CALLERS;
         ++index) {
        snapshot->render_caller_addresses[index] =
            state->render_caller_addresses[index];
        snapshot->render_caller_counts[index] =
            state->render_caller_counts[index];
    }
    snapshot->render_caller_overflow = state->render_caller_overflow;
    snapshot->last_unexpected_font_lr = state->last_unexpected_font_lr;
}

static void zero_bytes(uint8_t *bytes, size_t size)
{
    size_t index;

    for (index = 0u; index < size; ++index) {
        bytes[index] = 0u;
    }
}

static void put_be16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value >> 8u);
    destination[1] = (uint8_t)value;
}

static void put_be32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value >> 24u);
    destination[1] = (uint8_t)(value >> 16u);
    destination[2] = (uint8_t)(value >> 8u);
    destination[3] = (uint8_t)value;
}

static uint16_t get_be16(const uint8_t *source)
{
    return (uint16_t)(((uint16_t)source[0] << 8u) |
        (uint16_t)source[1]);
}

static uint32_t get_be32(const uint8_t *source)
{
    return ((uint32_t)source[0] << 24u) |
        ((uint32_t)source[1] << 16u) |
        ((uint32_t)source[2] << 8u) |
        (uint32_t)source[3];
}

static uint32_t crc32_ieee(const uint8_t *bytes, size_t size)
{
    uint32_t crc = UINT32_MAX;
    size_t index;

    for (index = 0u; index < size; ++index) {
        uint32_t bit;

        crc ^= (uint32_t)bytes[index];
        for (bit = 0u; bit < 8u; ++bit) {
            const uint32_t low_bit_mask = (uint32_t)(0u - (crc & 1u));
            crc = (crc >> 1u) ^ (0xEDB88320u & low_bit_mask);
        }
    }
    return ~crc;
}

static void encode_snapshot(
    const AzRenderObserveSnapshot *snapshot,
    uint8_t *record)
{
    uint32_t index;

    zero_bytes(record, AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE);
    record[AZ_RENDER_OBSERVE_OFF_MAGIC + 0u] = (uint8_t)'A';
    record[AZ_RENDER_OBSERVE_OFF_MAGIC + 1u] = (uint8_t)'Z';
    record[AZ_RENDER_OBSERVE_OFF_MAGIC + 2u] = (uint8_t)'R';
    record[AZ_RENDER_OBSERVE_OFF_MAGIC + 3u] = (uint8_t)'O';
    put_be16(record + AZ_RENDER_OBSERVE_OFF_VERSION,
        (uint16_t)AZ_RENDER_OBSERVE_REV1655_VERSION);
    put_be16(record + AZ_RENDER_OBSERVE_OFF_RECORD_SIZE,
        (uint16_t)AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_GENERATION,
        snapshot->generation);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_SAFETY_FLAGS,
        snapshot->safety_flags);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_CONTENTION_DROPS,
        snapshot->contention_drops);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_UNARMED_EVENTS,
        snapshot->unarmed_events);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_INVALID_EVENTS,
        snapshot->invalid_events);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_NESTING_UNDERFLOWS,
        snapshot->nesting_underflows);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_ENTERS,
        snapshot->render_menu_enters);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_EXITS,
        snapshot->render_menu_exits);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_SUCCESSES,
        snapshot->render_menu_successes);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_NONZERO,
        snapshot->render_menu_nonzero);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_FONT_END_ENTERS,
        snapshot->font_end_enters);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_FONT_END_EXITS,
        snapshot->font_end_exits);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_FONT_EXPECTED_CALLERS,
        snapshot->font_expected_callers);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_FONT_UNEXPECTED_CALLERS,
        snapshot->font_unexpected_callers);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_NESTING,
        snapshot->render_menu_nesting);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_MAX_NESTING,
        snapshot->render_menu_max_nesting);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_FONT_END_NESTING,
        snapshot->font_end_nesting);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_FONT_END_MAX_NESTING,
        snapshot->font_end_max_nesting);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_CROSS_NESTING,
        snapshot->cross_nesting);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_POINTER_ANOMALIES,
        snapshot->pointer_anomalies);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_DEVICE_MISSING,
        snapshot->device_missing);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_SCENE_SAMPLES,
        snapshot->scene_samples);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_SCENE_ALLOWED,
        snapshot->scene_allowed);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_SCENE_DENIED,
        snapshot->scene_denied);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_RENDER_MENU_LR,
        snapshot->last_render_menu_lr);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_FONT_END_LR,
        snapshot->last_font_end_lr);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_MANAGER,
        snapshot->last_manager);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_SUCCESS_MANAGER,
        snapshot->last_success_manager);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_FONT,
        snapshot->last_font);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_RENDER_DEVICE,
        snapshot->last_render_device);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_FONT_DEVICE,
        snapshot->last_font_device);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_RENDER_RESULT,
        (uint32_t)snapshot->last_render_result);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_SCENE_REASON,
        snapshot->last_scene_reason);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_CACHE_HEAD,
        snapshot->last_cache_head);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_MAIN_NODE,
        snapshot->last_main_node);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_MAIN_HANDLE,
        snapshot->last_main_handle);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_SCANNED_NODES,
        snapshot->last_scanned_nodes);
    record[AZ_RENDER_OBSERVE_OFF_LAST_SCENE_ALLOWS] =
        snapshot->last_scene_allows;
    record[AZ_RENDER_OBSERVE_OFF_ARMED] = snapshot->armed;
    record[AZ_RENDER_OBSERVE_OFF_EXACT_IMAGE_VERIFIED] =
        snapshot->exact_image_verified;

    for (index = 0u;
         index < AZ_RENDER_OBSERVE_REV1655_SCENE_REASON_COUNT;
         ++index) {
        put_be32(
            record + AZ_RENDER_OBSERVE_SCENE_REASON_OFFSET(index),
            snapshot->scene_reason_counts[index]);
    }
    for (index = 0u;
         index < AZ_RENDER_OBSERVE_REV1655_MAX_CALLERS;
         ++index) {
        put_be32(
            record + AZ_RENDER_OBSERVE_CALLER_ADDRESS_OFFSET(index),
            snapshot->render_caller_addresses[index]);
        put_be16(
            record + AZ_RENDER_OBSERVE_CALLER_COUNT_OFFSET(index),
            clamp_u16(snapshot->render_caller_counts[index]));
    }
    put_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_CALLER_OVERFLOW,
        snapshot->render_caller_overflow);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_UNEXPECTED_FONT_LR,
        snapshot->last_unexpected_font_lr);
    put_be32(record + AZ_RENDER_OBSERVE_OFF_CRC32,
        crc32_ieee(record, AZ_RENDER_OBSERVE_OFF_CRC32));
}

AzRenderObserveResult az_render_observe_rev1655_snapshot_be(
    AzRenderObserveRev1655 *state,
    uint8_t *record,
    size_t record_size,
    uint32_t *revision_token)
{
    AzRenderObserveSnapshot snapshot;
    uint32_t captured_revision;

    if (state == NULL || record == NULL || revision_token == NULL) {
        return AZ_RENDER_OBSERVE_NULL;
    }
    if (record_size < (size_t)AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE) {
        return AZ_RENDER_OBSERVE_BUFFER_TOO_SMALL;
    }
    if (record_size != (size_t)AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE) {
        return AZ_RENDER_OBSERVE_INVALID_ARGUMENT;
    }
    if (try_lock(state, 0u) == 0u) {
        return AZ_RENDER_OBSERVE_BUSY;
    }
    if (__atomic_load_n(&state->dirty, __ATOMIC_ACQUIRE) == 0u) {
        unlock(state);
        return AZ_RENDER_OBSERVE_NO_CHANGE;
    }
    if (__atomic_load_n(
            &state->generation_assigned,
            __ATOMIC_ACQUIRE) == 0u) {
        state->generation += 1u;
        __atomic_store_n(
            &state->generation_assigned,
            1u,
            __ATOMIC_RELEASE);
    }
    /*
     * record_contention() and failed-scope cleanup deliberately remain
     * lockless so a render hook never blocks.  Read the acknowledgement token
     * before copying the state: if such an update starts during the copy, its
     * later revision bump makes this token stale and acknowledge() must fail.
     * Reading the token after the copy could instead cover an update omitted
     * by the copied record and let the worker acknowledge that update away.
     */
    captured_revision = __atomic_load_n(
        &state->revision,
        __ATOMIC_ACQUIRE);
    capture_snapshot(state, &snapshot);
#if defined(AURORAAZ_RENDER_OBSERVE_TEST_HOOKS)
    if (state->test_after_snapshot_capture != NULL) {
        state->test_after_snapshot_capture(state, state->test_hook_context);
    }
#endif
    *revision_token = captured_revision;
    unlock(state);

    encode_snapshot(&snapshot, record);
    return AZ_RENDER_OBSERVE_OK;
}

uint8_t az_render_observe_rev1655_acknowledge(
    AzRenderObserveRev1655 *state,
    uint32_t revision_token)
{
    uint8_t acknowledged = 0u;

    if (state == NULL || try_lock(state, 0u) == 0u) {
        return 0u;
    }
    if (__atomic_load_n(&state->dirty, __ATOMIC_ACQUIRE) != 0u &&
        __atomic_load_n(&state->revision, __ATOMIC_ACQUIRE) ==
            revision_token) {
        __atomic_store_n(&state->dirty, 0u, __ATOMIC_RELEASE);
        acknowledged = 1u;
    }
    unlock(state);
    return acknowledged;
}

static uint8_t bytes_are_zero(
    const uint8_t *record,
    size_t begin,
    size_t end)
{
    size_t index;

    for (index = begin; index < end; ++index) {
        if (record[index] != 0u) {
            return 0u;
        }
    }
    return 1u;
}

static uint8_t encoded_pointer_is_valid(uint32_t pointer)
{
    return pointer == 0u || (pointer & 3u) == 0u ? 1u : 0u;
}

static uint8_t encoded_caller_is_valid(uint32_t caller)
{
    return caller == 0u || caller_lr_is_valid(caller) != 0u ? 1u : 0u;
}

static uint8_t validate_record_semantics(const uint8_t *record)
{
    const uint32_t safety = get_be32(
        record + AZ_RENDER_OBSERVE_OFF_SAFETY_FLAGS);
    const uint32_t render_nesting = get_be32(
        record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_NESTING);
    const uint32_t render_max = get_be32(
        record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_MAX_NESTING);
    const uint32_t font_nesting = get_be32(
        record + AZ_RENDER_OBSERVE_OFF_FONT_END_NESTING);
    const uint32_t font_max = get_be32(
        record + AZ_RENDER_OBSERVE_OFF_FONT_END_MAX_NESTING);
    const uint32_t reason = get_be32(
        record + AZ_RENDER_OBSERVE_OFF_LAST_SCENE_REASON);
    const uint8_t scene_allows =
        record[AZ_RENDER_OBSERVE_OFF_LAST_SCENE_ALLOWS];
    const uint8_t armed = record[AZ_RENDER_OBSERVE_OFF_ARMED];
    const uint8_t exact =
        record[AZ_RENDER_OBSERVE_OFF_EXACT_IMAGE_VERIFIED];
    uint32_t index;

    if ((safety & ~AZ_RENDER_OBSERVE_ALL_SAFETY_FLAGS) != 0u ||
        render_nesting > render_max ||
        render_max > AZ_RENDER_OBSERVE_REV1655_MAX_NESTING ||
        font_nesting > font_max ||
        font_max > AZ_RENDER_OBSERVE_REV1655_MAX_NESTING ||
        reason >= AZ_RENDER_OBSERVE_REV1655_SCENE_REASON_COUNT ||
        scene_allows > 1u || armed > 1u || exact > 1u || armed > exact ||
        record[AZ_RENDER_OBSERVE_OFF_RESERVED_BOOL] != 0u ||
        get_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_SCANNED_NODES) >
            AZ_REV1655_SCENE_MAX_CACHE_NODES ||
        scene_allows !=
            (reason == (uint32_t)AZ_SCENE_GATE_REASON_MAIN_FOCUSED ?
                1u : 0u)) {
        return 0u;
    }

    if (encoded_caller_is_valid(get_be32(
            record + AZ_RENDER_OBSERVE_OFF_LAST_RENDER_MENU_LR)) == 0u ||
        encoded_caller_is_valid(get_be32(
            record + AZ_RENDER_OBSERVE_OFF_LAST_FONT_END_LR)) == 0u ||
        encoded_caller_is_valid(get_be32(
            record + AZ_RENDER_OBSERVE_OFF_LAST_UNEXPECTED_FONT_LR)) == 0u ||
        encoded_pointer_is_valid(get_be32(
            record + AZ_RENDER_OBSERVE_OFF_LAST_MANAGER)) == 0u ||
        encoded_pointer_is_valid(get_be32(
            record + AZ_RENDER_OBSERVE_OFF_LAST_SUCCESS_MANAGER)) == 0u ||
        encoded_pointer_is_valid(get_be32(
            record + AZ_RENDER_OBSERVE_OFF_LAST_FONT)) == 0u ||
        encoded_pointer_is_valid(get_be32(
            record + AZ_RENDER_OBSERVE_OFF_LAST_RENDER_DEVICE)) == 0u ||
        encoded_pointer_is_valid(get_be32(
            record + AZ_RENDER_OBSERVE_OFF_LAST_FONT_DEVICE)) == 0u ||
        encoded_pointer_is_valid(get_be32(
            record + AZ_RENDER_OBSERVE_OFF_LAST_CACHE_HEAD)) == 0u ||
        encoded_pointer_is_valid(get_be32(
            record + AZ_RENDER_OBSERVE_OFF_LAST_MAIN_NODE)) == 0u) {
        return 0u;
    }

    for (index = 0u;
         index < AZ_RENDER_OBSERVE_REV1655_MAX_CALLERS;
         ++index) {
        const uint32_t address = get_be32(
            record + AZ_RENDER_OBSERVE_CALLER_ADDRESS_OFFSET(index));
        const uint16_t count = get_be16(
            record + AZ_RENDER_OBSERVE_CALLER_COUNT_OFFSET(index));

        if (encoded_caller_is_valid(address) == 0u ||
            ((address == 0u) != (count == 0u))) {
            return 0u;
        }
    }
    return bytes_are_zero(
        record,
        AZ_RENDER_OBSERVE_OFF_RESERVED,
        AZ_RENDER_OBSERVE_OFF_CRC32);
}

AzRenderObserveResult az_render_observe_rev1655_validate_record_be(
    const uint8_t *record,
    size_t record_size,
    uint32_t *generation)
{
    if (record == NULL || generation == NULL) {
        return AZ_RENDER_OBSERVE_NULL;
    }
    if (record_size < (size_t)AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE) {
        return AZ_RENDER_OBSERVE_BUFFER_TOO_SMALL;
    }
    if (record_size != (size_t)AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE) {
        return AZ_RENDER_OBSERVE_INVALID_RECORD;
    }
    if (record[AZ_RENDER_OBSERVE_OFF_MAGIC + 0u] != (uint8_t)'A' ||
        record[AZ_RENDER_OBSERVE_OFF_MAGIC + 1u] != (uint8_t)'Z' ||
        record[AZ_RENDER_OBSERVE_OFF_MAGIC + 2u] != (uint8_t)'R' ||
        record[AZ_RENDER_OBSERVE_OFF_MAGIC + 3u] != (uint8_t)'O' ||
        get_be16(record + AZ_RENDER_OBSERVE_OFF_VERSION) !=
            (uint16_t)AZ_RENDER_OBSERVE_REV1655_VERSION ||
        get_be16(record + AZ_RENDER_OBSERVE_OFF_RECORD_SIZE) !=
            (uint16_t)AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE ||
        validate_record_semantics(record) == 0u ||
        get_be32(record + AZ_RENDER_OBSERVE_OFF_CRC32) !=
            crc32_ieee(record, AZ_RENDER_OBSERVE_OFF_CRC32)) {
        return AZ_RENDER_OBSERVE_INVALID_RECORD;
    }
    *generation = get_be32(record + AZ_RENDER_OBSERVE_OFF_GENERATION);
    return AZ_RENDER_OBSERVE_OK;
}

static uint8_t generation_is_newer(
    uint32_t candidate,
    uint32_t reference)
{
    const uint32_t distance = candidate - reference;

    return distance != 0u && distance < 0x80000000u ? 1u : 0u;
}

AzRenderObserveResult az_render_observe_rev1655_select_newest_be(
    const uint8_t *slot_a,
    size_t slot_a_size,
    const uint8_t *slot_b,
    size_t slot_b_size,
    uint8_t *selected_slot,
    uint32_t *generation)
{
    uint32_t generation_a = 0u;
    uint32_t generation_b = 0u;
    const uint8_t valid_a = slot_a != NULL &&
        az_render_observe_rev1655_validate_record_be(
            slot_a,
            slot_a_size,
            &generation_a) == AZ_RENDER_OBSERVE_OK ? 1u : 0u;
    const uint8_t valid_b = slot_b != NULL &&
        az_render_observe_rev1655_validate_record_be(
            slot_b,
            slot_b_size,
            &generation_b) == AZ_RENDER_OBSERVE_OK ? 1u : 0u;

    if (selected_slot == NULL || generation == NULL) {
        return AZ_RENDER_OBSERVE_NULL;
    }
    if (valid_a == 0u && valid_b == 0u) {
        return AZ_RENDER_OBSERVE_INVALID_RECORD;
    }
    if (valid_b != 0u &&
        (valid_a == 0u || generation_is_newer(
            generation_b,
            generation_a) != 0u)) {
        *selected_slot = AZ_RENDER_OBSERVE_REV1655_SLOT_B;
        *generation = generation_b;
    }
    else {
        *selected_slot = AZ_RENDER_OBSERVE_REV1655_SLOT_A;
        *generation = generation_a;
    }
    return AZ_RENDER_OBSERVE_OK;
}

const char *az_render_observe_result_name(AzRenderObserveResult result)
{
    switch (result) {
    case AZ_RENDER_OBSERVE_OK:
        return "ok";
    case AZ_RENDER_OBSERVE_NULL:
        return "null";
    case AZ_RENDER_OBSERVE_NOT_ARMED:
        return "not-armed";
    case AZ_RENDER_OBSERVE_BUSY:
        return "busy";
    case AZ_RENDER_OBSERVE_INVALID_ARGUMENT:
        return "invalid-argument";
    case AZ_RENDER_OBSERVE_INVALID_SCOPE:
        return "invalid-scope";
    case AZ_RENDER_OBSERVE_NO_CHANGE:
        return "no-change";
    case AZ_RENDER_OBSERVE_BUFFER_TOO_SMALL:
        return "buffer-too-small";
    case AZ_RENDER_OBSERVE_INVALID_RECORD:
        return "invalid-record";
    default:
        return "unknown";
    }
}
