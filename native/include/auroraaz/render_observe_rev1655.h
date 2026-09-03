#ifndef AURORAAZ_RENDER_OBSERVE_REV1655_H
#define AURORAAZ_RENDER_OBSERVE_REV1655_H

#include <stddef.h>
#include <stdint.h>

#include <auroraaz/scene_gate_rev1655.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Hardware stage M2b is observation only. It records values already present
 * at the two render hooks; it has no D3D callback, target-memory write, draw,
 * allocation, or file-I/O API. A worker may persist snapshots separately.
 */
#define AZ_RENDER_OBSERVE_REV1655_SLOT_A_PATH \
    "game:\\Data\\Logs\\AuroraAZ-M2b-render-A.bin"
#define AZ_RENDER_OBSERVE_REV1655_SLOT_B_PATH \
    "game:\\Data\\Logs\\AuroraAZ-M2b-render-B.bin"

#define AZ_RENDER_OBSERVE_REV1655_VERSION 1u
#define AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE 256u
#define AZ_RENDER_OBSERVE_REV1655_SLOT_A 0u
#define AZ_RENDER_OBSERVE_REV1655_SLOT_B 1u
#define AZ_RENDER_OBSERVE_REV1655_MAX_NESTING 16u
#define AZ_RENDER_OBSERVE_REV1655_MAX_CALLERS 4u
#define AZ_RENDER_OBSERVE_REV1655_SCENE_REASON_COUNT 13u
#define AZ_RENDER_OBSERVE_REV1655_FONT_END_CALLER_LR 0x82211844u
#define AZ_RENDER_OBSERVE_REV1655_IMAGE_TEXT_BEGIN 0x82210000u
#define AZ_RENDER_OBSERVE_REV1655_IMAGE_TEXT_END 0x82B673DCu

/* Fixed big-endian wire record. Multi-byte values are always BE. */
#define AZ_RENDER_OBSERVE_OFF_MAGIC 0u
#define AZ_RENDER_OBSERVE_OFF_VERSION 4u
#define AZ_RENDER_OBSERVE_OFF_RECORD_SIZE 6u
#define AZ_RENDER_OBSERVE_OFF_GENERATION 8u
#define AZ_RENDER_OBSERVE_OFF_SAFETY_FLAGS 12u
#define AZ_RENDER_OBSERVE_OFF_CONTENTION_DROPS 16u
#define AZ_RENDER_OBSERVE_OFF_UNARMED_EVENTS 20u
#define AZ_RENDER_OBSERVE_OFF_INVALID_EVENTS 24u
#define AZ_RENDER_OBSERVE_OFF_NESTING_UNDERFLOWS 28u
#define AZ_RENDER_OBSERVE_OFF_RENDER_MENU_ENTERS 32u
#define AZ_RENDER_OBSERVE_OFF_RENDER_MENU_EXITS 36u
#define AZ_RENDER_OBSERVE_OFF_RENDER_MENU_SUCCESSES 40u
#define AZ_RENDER_OBSERVE_OFF_RENDER_MENU_NONZERO 44u
#define AZ_RENDER_OBSERVE_OFF_FONT_END_ENTERS 48u
#define AZ_RENDER_OBSERVE_OFF_FONT_END_EXITS 52u
#define AZ_RENDER_OBSERVE_OFF_FONT_EXPECTED_CALLERS 56u
#define AZ_RENDER_OBSERVE_OFF_FONT_UNEXPECTED_CALLERS 60u
#define AZ_RENDER_OBSERVE_OFF_RENDER_MENU_NESTING 64u
#define AZ_RENDER_OBSERVE_OFF_RENDER_MENU_MAX_NESTING 68u
#define AZ_RENDER_OBSERVE_OFF_FONT_END_NESTING 72u
#define AZ_RENDER_OBSERVE_OFF_FONT_END_MAX_NESTING 76u
#define AZ_RENDER_OBSERVE_OFF_CROSS_NESTING 80u
#define AZ_RENDER_OBSERVE_OFF_POINTER_ANOMALIES 84u
#define AZ_RENDER_OBSERVE_OFF_DEVICE_MISSING 88u
#define AZ_RENDER_OBSERVE_OFF_SCENE_SAMPLES 92u
#define AZ_RENDER_OBSERVE_OFF_SCENE_ALLOWED 96u
#define AZ_RENDER_OBSERVE_OFF_SCENE_DENIED 100u
#define AZ_RENDER_OBSERVE_OFF_LAST_RENDER_MENU_LR 104u
#define AZ_RENDER_OBSERVE_OFF_LAST_FONT_END_LR 108u
#define AZ_RENDER_OBSERVE_OFF_LAST_MANAGER 112u
#define AZ_RENDER_OBSERVE_OFF_LAST_SUCCESS_MANAGER 116u
#define AZ_RENDER_OBSERVE_OFF_LAST_FONT 120u
#define AZ_RENDER_OBSERVE_OFF_LAST_RENDER_DEVICE 124u
#define AZ_RENDER_OBSERVE_OFF_LAST_FONT_DEVICE 128u
#define AZ_RENDER_OBSERVE_OFF_LAST_RENDER_RESULT 132u
#define AZ_RENDER_OBSERVE_OFF_LAST_SCENE_REASON 136u
#define AZ_RENDER_OBSERVE_OFF_LAST_CACHE_HEAD 140u
#define AZ_RENDER_OBSERVE_OFF_LAST_MAIN_NODE 144u
#define AZ_RENDER_OBSERVE_OFF_LAST_MAIN_HANDLE 148u
#define AZ_RENDER_OBSERVE_OFF_LAST_SCANNED_NODES 152u
#define AZ_RENDER_OBSERVE_OFF_LAST_SCENE_ALLOWS 156u
#define AZ_RENDER_OBSERVE_OFF_ARMED 157u
#define AZ_RENDER_OBSERVE_OFF_EXACT_IMAGE_VERIFIED 158u
#define AZ_RENDER_OBSERVE_OFF_RESERVED_BOOL 159u
#define AZ_RENDER_OBSERVE_OFF_SCENE_REASON_COUNTS 160u
#define AZ_RENDER_OBSERVE_OFF_RENDER_CALLER_ADDRESSES 212u
#define AZ_RENDER_OBSERVE_OFF_RENDER_CALLER_COUNTS 228u
#define AZ_RENDER_OBSERVE_OFF_RENDER_CALLER_OVERFLOW 236u
#define AZ_RENDER_OBSERVE_OFF_LAST_UNEXPECTED_FONT_LR 240u
#define AZ_RENDER_OBSERVE_OFF_RESERVED 244u
#define AZ_RENDER_OBSERVE_OFF_CRC32 252u

#define AZ_RENDER_OBSERVE_SCENE_REASON_OFFSET(reason) \
    (AZ_RENDER_OBSERVE_OFF_SCENE_REASON_COUNTS + \
     (uint32_t)(reason) * 4u)
#define AZ_RENDER_OBSERVE_CALLER_ADDRESS_OFFSET(slot) \
    (AZ_RENDER_OBSERVE_OFF_RENDER_CALLER_ADDRESSES + \
     (uint32_t)(slot) * 4u)
#define AZ_RENDER_OBSERVE_CALLER_COUNT_OFFSET(slot) \
    (AZ_RENDER_OBSERVE_OFF_RENDER_CALLER_COUNTS + \
     (uint32_t)(slot) * 2u)

typedef enum AzRenderObserveSafetyFlag {
    AZ_RENDER_OBSERVE_SAFETY_INVALID_POINTER = 1u << 0u,
    AZ_RENDER_OBSERVE_SAFETY_INVALID_CALLER_LR = 1u << 1u,
    AZ_RENDER_OBSERVE_SAFETY_UNEXPECTED_FONT_CALLER = 1u << 2u,
    AZ_RENDER_OBSERVE_SAFETY_RENDER_NESTING_OVERFLOW = 1u << 3u,
    AZ_RENDER_OBSERVE_SAFETY_FONT_NESTING_OVERFLOW = 1u << 4u,
    AZ_RENDER_OBSERVE_SAFETY_NESTING_UNDERFLOW = 1u << 5u,
    AZ_RENDER_OBSERVE_SAFETY_CROSS_NESTING = 1u << 6u,
    AZ_RENDER_OBSERVE_SAFETY_WRITER_CONTENTION = 1u << 7u,
    AZ_RENDER_OBSERVE_SAFETY_INVALID_SCENE = 1u << 8u,
    AZ_RENDER_OBSERVE_SAFETY_SCOPE_MISMATCH = 1u << 9u,
    AZ_RENDER_OBSERVE_SAFETY_UNARMED_EVENT = 1u << 10u
} AzRenderObserveSafetyFlag;

#define AZ_RENDER_OBSERVE_ALL_SAFETY_FLAGS 0x000007FFu

typedef enum AzRenderObserveResult {
    AZ_RENDER_OBSERVE_OK = 0,
    AZ_RENDER_OBSERVE_NULL,
    AZ_RENDER_OBSERVE_NOT_ARMED,
    AZ_RENDER_OBSERVE_BUSY,
    AZ_RENDER_OBSERVE_INVALID_ARGUMENT,
    AZ_RENDER_OBSERVE_INVALID_SCOPE,
    AZ_RENDER_OBSERVE_NO_CHANGE,
    AZ_RENDER_OBSERVE_BUFFER_TOO_SMALL,
    AZ_RENDER_OBSERVE_INVALID_RECORD
} AzRenderObserveResult;

typedef enum AzRenderObserveScopeKind {
    AZ_RENDER_OBSERVE_SCOPE_NONE = 0,
    AZ_RENDER_OBSERVE_SCOPE_RENDER_MENU = 1,
    AZ_RENDER_OBSERVE_SCOPE_FONT_END = 2
} AzRenderObserveScopeKind;

typedef struct AzRenderObserveRev1655 AzRenderObserveRev1655;

#if defined(AURORAAZ_RENDER_OBSERVE_TEST_HOOKS)
/* Deterministic race injection for the isolated host tests only. */
typedef void (*AzRenderObserveTestHookFn)(
    AzRenderObserveRev1655 *state,
    void *context);
#endif

/* Stack-local pairing token. Callers must treat every member as opaque. */
typedef struct AzRenderObserveScope {
    const AzRenderObserveRev1655 *owner;
    uint32_t seal;
    uint32_t kind;
    uint32_t subject;
    uint32_t caller_lr;
    uint8_t active;
    uint8_t depth_accounted;
} AzRenderObserveScope;

/*
 * Public only so the runtime can reserve static storage. All live fields are
 * owned by this module and must not be read directly outside host tests.
 */
struct AzRenderObserveRev1655 {
    volatile uint32_t writer_lock;
    volatile uint32_t revision;
    uint32_t generation;
    volatile uint32_t generation_assigned;
    volatile uint32_t dirty;
    volatile uint32_t exact_image_verified;
    volatile uint32_t armed;
    volatile uint32_t safety_flags;
    volatile uint32_t contention_drops;
    uint32_t unarmed_events;
    uint32_t invalid_events;
    volatile uint32_t nesting_underflows;
    uint32_t render_menu_enters;
    uint32_t render_menu_exits;
    uint32_t render_menu_successes;
    uint32_t render_menu_nonzero;
    uint32_t font_end_enters;
    uint32_t font_end_exits;
    uint32_t font_expected_callers;
    uint32_t font_unexpected_callers;
    volatile uint32_t render_menu_nesting;
    uint32_t render_menu_max_nesting;
    volatile uint32_t font_end_nesting;
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
    AzSceneGateReason last_scene_reason;
    uint32_t last_cache_head;
    uint32_t last_main_node;
    uint32_t last_main_handle;
    uint32_t last_scanned_nodes;
    uint8_t last_scene_allows;
    uint32_t scene_reason_counts[
        AZ_RENDER_OBSERVE_REV1655_SCENE_REASON_COUNT];
    uint32_t render_caller_addresses[
        AZ_RENDER_OBSERVE_REV1655_MAX_CALLERS];
    uint32_t render_caller_counts[
        AZ_RENDER_OBSERVE_REV1655_MAX_CALLERS];
    uint32_t render_caller_overflow;
    uint32_t last_unexpected_font_lr;
#if defined(AURORAAZ_RENDER_OBSERVE_TEST_HOOKS)
    AzRenderObserveTestHookFn test_after_snapshot_capture;
    void *test_hook_context;
#endif
};

/* exact_image_verified must come from the existing Rev1655 hook permit. */
void az_render_observe_rev1655_init(
    AzRenderObserveRev1655 *state,
    uint8_t exact_image_verified);

/* Prevents new scopes. Already-admitted scopes may still close cleanly. */
AzRenderObserveResult az_render_observe_rev1655_disarm(
    AzRenderObserveRev1655 *state);

AzRenderObserveResult az_render_observe_render_menu_begin(
    AzRenderObserveRev1655 *state,
    uintptr_t game_content_manager,
    uint32_t caller_lr,
    uintptr_t device,
    AzRenderObserveScope *scope);

AzRenderObserveResult az_render_observe_render_menu_end(
    AzRenderObserveRev1655 *state,
    AzRenderObserveScope *scope,
    int32_t render_result,
    uintptr_t device);

AzRenderObserveResult az_render_observe_font_end_begin(
    AzRenderObserveRev1655 *state,
    uintptr_t font,
    uint32_t caller_lr,
    uintptr_t device,
    AzRenderObserveScope *scope);

AzRenderObserveResult az_render_observe_font_end_end(
    AzRenderObserveRev1655 *state,
    AzRenderObserveScope *scope,
    uintptr_t device);

/*
 * An end call that accepts a valid scope but returns BUSY has still closed the
 * scope and released its accounted nesting depth. Its detailed completion
 * event was dropped and is represented by contention_drops/WRITER_CONTENTION;
 * it must not be retried.
 */

/* Worker-side periodic scene observation; never invoke this from a hook. */
AzRenderObserveResult az_render_observe_note_scene(
    AzRenderObserveRev1655 *state,
    const AzSceneGateDecision *decision);

uint8_t az_render_observe_rev1655_is_dirty(
    const AzRenderObserveRev1655 *state);

AzRenderObserveResult az_render_observe_rev1655_seed_generation(
    AzRenderObserveRev1655 *state,
    uint32_t generation);

AzRenderObserveResult az_render_observe_rev1655_snapshot_be(
    AzRenderObserveRev1655 *state,
    uint8_t *record,
    size_t record_size,
    uint32_t *revision_token);

/*
 * Returns zero if any event began publishing after the snapshot token was
 * bound. In that case dirty remains set and the worker must snapshot again.
 */
uint8_t az_render_observe_rev1655_acknowledge(
    AzRenderObserveRev1655 *state,
    uint32_t revision_token);

AzRenderObserveResult az_render_observe_rev1655_validate_record_be(
    const uint8_t *record,
    size_t record_size,
    uint32_t *generation);

AzRenderObserveResult az_render_observe_rev1655_select_newest_be(
    const uint8_t *slot_a,
    size_t slot_a_size,
    const uint8_t *slot_b,
    size_t slot_b_size,
    uint8_t *selected_slot,
    uint32_t *generation);

const char *az_render_observe_result_name(AzRenderObserveResult result);

#ifdef __cplusplus
}
#endif

#endif
