#include <auroraaz/render_observe_rev1655.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define TEST_MANAGER 0x90001000u
#define TEST_FONT 0x90002000u
#define TEST_DEVICE 0x90003000u
#define TEST_RENDER_CALLER 0x82300004u

static unsigned int g_failures;

#define CHECK(expression)                                                     \
    do {                                                                      \
        if (!(expression)) {                                                  \
            (void)fprintf(                                                    \
                stderr,                                                       \
                "%s:%d: CHECK failed: %s\n",                                \
                __FILE__,                                                     \
                __LINE__,                                                     \
                #expression);                                                 \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

static uint16_t read_be16(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8u) | bytes[1]);
}

static uint32_t read_be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24u) |
        ((uint32_t)bytes[1] << 16u) |
        ((uint32_t)bytes[2] << 8u) |
        (uint32_t)bytes[3];
}

static void write_be32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24u);
    bytes[1] = (uint8_t)(value >> 16u);
    bytes[2] = (uint8_t)(value >> 8u);
    bytes[3] = (uint8_t)value;
}

static uint32_t crc32_ieee(const uint8_t *bytes, size_t size)
{
    uint32_t crc = UINT32_MAX;
    size_t index;

    for (index = 0u; index < size; ++index) {
        uint32_t bit;

        crc ^= (uint32_t)bytes[index];
        for (bit = 0u; bit < 8u; ++bit) {
            const uint32_t mask = (uint32_t)(0u - (crc & 1u));
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static void repair_crc(uint8_t *record)
{
    write_be32(
        record + AZ_RENDER_OBSERVE_OFF_CRC32,
        crc32_ieee(record, AZ_RENDER_OBSERVE_OFF_CRC32));
}

static uint32_t take_snapshot(
    AzRenderObserveRev1655 *state,
    uint8_t *record)
{
    uint32_t token = 0u;

    CHECK(az_render_observe_rev1655_snapshot_be(
        state,
        record,
        AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE,
        &token) == AZ_RENDER_OBSERVE_OK);
    return token;
}

static AzSceneGateDecision make_scene(
    AzSceneGateReason reason,
    uint8_t allows_capture)
{
    AzSceneGateDecision decision;

    memset(&decision, 0, sizeof(decision));
    decision.reason = reason;
    decision.cache_head = 0x91000000u;
    decision.main_scene_node = 0x91000020u;
    decision.main_scene_handle = 0x12340001u;
    decision.scanned_nodes = 7u;
    decision.allows_capture = allows_capture;
    return decision;
}

#if defined(AURORAAZ_RENDER_OBSERVE_TEST_HOOKS)
typedef struct SnapshotContentionRace {
    uint32_t calls;
    AzRenderObserveResult injected_result;
    uint8_t injected_scope_active;
} SnapshotContentionRace;

static void inject_contention_after_snapshot_capture(
    AzRenderObserveRev1655 *state,
    void *context)
{
    SnapshotContentionRace *race = (SnapshotContentionRace *)context;
    AzRenderObserveScope scope;

    /* One shot: the retry snapshot must be allowed to complete normally. */
    state->test_after_snapshot_capture = NULL;
    race->calls += 1u;
    race->injected_result = az_render_observe_render_menu_begin(
        state,
        (uintptr_t)TEST_MANAGER,
        TEST_RENDER_CALLER,
        (uintptr_t)TEST_DEVICE,
        &scope);
    race->injected_scope_active = scope.active;
}
#endif

static void test_unverified_stage_fails_closed(void)
{
    AzRenderObserveRev1655 state;
    AzRenderObserveScope scope;
    uint8_t record[AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE];
    uint32_t generation = 0u;
    uint32_t token;

    az_render_observe_rev1655_init(&state, 0u);
    memset(&scope, 0xA5, sizeof(scope));
    CHECK(az_render_observe_render_menu_begin(
        &state,
        (uintptr_t)TEST_MANAGER,
        TEST_RENDER_CALLER,
        (uintptr_t)TEST_DEVICE,
        &scope) == AZ_RENDER_OBSERVE_NOT_ARMED);
    CHECK(scope.active == 0u);
    CHECK(scope.owner == NULL);
    token = take_snapshot(&state, record);
    CHECK(memcmp(record + AZ_RENDER_OBSERVE_OFF_MAGIC, "AZRO", 4u) == 0);
    CHECK(read_be16(record + AZ_RENDER_OBSERVE_OFF_VERSION) ==
        AZ_RENDER_OBSERVE_REV1655_VERSION);
    CHECK(read_be16(record + AZ_RENDER_OBSERVE_OFF_RECORD_SIZE) ==
        AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE);
    CHECK(record[AZ_RENDER_OBSERVE_OFF_ARMED] == 0u);
    CHECK(record[AZ_RENDER_OBSERVE_OFF_EXACT_IMAGE_VERIFIED] == 0u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_UNARMED_EVENTS) == 1u);
    CHECK((read_be32(record + AZ_RENDER_OBSERVE_OFF_SAFETY_FLAGS) &
        AZ_RENDER_OBSERVE_SAFETY_UNARMED_EVENT) != 0u);
    CHECK(az_render_observe_rev1655_validate_record_be(
        record,
        sizeof(record),
        &generation) == AZ_RENDER_OBSERVE_OK);
    CHECK(generation == 1u);
    CHECK(az_render_observe_rev1655_acknowledge(&state, token) == 1u);
    CHECK(az_render_observe_rev1655_is_dirty(&state) == 0u);
}

static void test_happy_render_font_and_scene_observation(void)
{
    AzRenderObserveRev1655 state;
    AzRenderObserveScope render_scope;
    AzRenderObserveScope font_scope;
    AzSceneGateDecision focused = make_scene(
        AZ_SCENE_GATE_REASON_MAIN_FOCUSED,
        1u);
    AzSceneGateDecision modal = make_scene(
        AZ_SCENE_GATE_REASON_MAIN_NOT_FOCUSED,
        0u);
    const AzSceneGateDecision focused_before = focused;
    uint8_t record[AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE];
    uint32_t generation = 0u;

    az_render_observe_rev1655_init(&state, 1u);
    CHECK(az_render_observe_render_menu_begin(
        &state,
        (uintptr_t)TEST_MANAGER,
        TEST_RENDER_CALLER,
        (uintptr_t)TEST_DEVICE,
        &render_scope) == AZ_RENDER_OBSERVE_OK);
    CHECK(render_scope.active == 1u);
    CHECK(az_render_observe_render_menu_end(
        &state,
        &render_scope,
        0,
        (uintptr_t)TEST_DEVICE) == AZ_RENDER_OBSERVE_OK);
    CHECK(render_scope.active == 0u);
    CHECK(az_render_observe_font_end_begin(
        &state,
        (uintptr_t)TEST_FONT,
        AZ_RENDER_OBSERVE_REV1655_FONT_END_CALLER_LR,
        (uintptr_t)TEST_DEVICE,
        &font_scope) == AZ_RENDER_OBSERVE_OK);
    CHECK(az_render_observe_font_end_end(
        &state,
        &font_scope,
        (uintptr_t)TEST_DEVICE) == AZ_RENDER_OBSERVE_OK);
    CHECK(az_render_observe_note_scene(&state, &focused) ==
        AZ_RENDER_OBSERVE_OK);
    CHECK(memcmp(&focused, &focused_before, sizeof(focused)) == 0);
    CHECK(az_render_observe_note_scene(&state, &modal) ==
        AZ_RENDER_OBSERVE_OK);

    (void)take_snapshot(&state, record);
    CHECK(az_render_observe_rev1655_validate_record_be(
        record,
        sizeof(record),
        &generation) == AZ_RENDER_OBSERVE_OK);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_SAFETY_FLAGS) == 0u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_ENTERS) == 1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_EXITS) == 1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_SUCCESSES) ==
        1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_NONZERO) ==
        0u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_FONT_END_ENTERS) == 1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_FONT_END_EXITS) == 1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_FONT_EXPECTED_CALLERS) ==
        1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_FONT_UNEXPECTED_CALLERS) ==
        0u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_NESTING) == 0u);
    CHECK(read_be32(
        record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_MAX_NESTING) == 1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_FONT_END_NESTING) == 0u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_FONT_END_MAX_NESTING) ==
        1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_RENDER_MENU_LR) ==
        TEST_RENDER_CALLER);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_FONT_END_LR) ==
        AZ_RENDER_OBSERVE_REV1655_FONT_END_CALLER_LR);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_MANAGER) ==
        TEST_MANAGER);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_SUCCESS_MANAGER) ==
        TEST_MANAGER);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_FONT) == TEST_FONT);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_RENDER_DEVICE) ==
        TEST_DEVICE);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_LAST_FONT_DEVICE) ==
        TEST_DEVICE);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_SCENE_SAMPLES) == 2u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_SCENE_ALLOWED) == 1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_SCENE_DENIED) == 1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_SCENE_REASON_OFFSET(
        AZ_SCENE_GATE_REASON_MAIN_FOCUSED)) == 1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_SCENE_REASON_OFFSET(
        AZ_SCENE_GATE_REASON_MAIN_NOT_FOCUSED)) == 1u);
    CHECK(record[AZ_RENDER_OBSERVE_OFF_LAST_SCENE_ALLOWS] == 0u);
    CHECK(record[AZ_RENDER_OBSERVE_OFF_ARMED] == 1u);
    CHECK(record[AZ_RENDER_OBSERVE_OFF_EXACT_IMAGE_VERIFIED] == 1u);
}

static void test_nesting_and_bounded_caller_samples(void)
{
    AzRenderObserveRev1655 state;
    AzRenderObserveScope render_scopes[17];
    AzRenderObserveScope font_scope;
    uint8_t record[AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE];
    uint32_t index;
    uint32_t safety;

    az_render_observe_rev1655_init(&state, 1u);
    for (index = 0u; index < 17u; ++index) {
        const uint32_t caller = TEST_RENDER_CALLER + index * 4u;
        CHECK(az_render_observe_render_menu_begin(
            &state,
            (uintptr_t)(TEST_MANAGER + index * 4u),
            caller,
            (uintptr_t)TEST_DEVICE,
            &render_scopes[index]) == AZ_RENDER_OBSERVE_OK);
    }
    CHECK(az_render_observe_font_end_begin(
        &state,
        (uintptr_t)TEST_FONT,
        0x82211848u,
        (uintptr_t)TEST_DEVICE,
        &font_scope) == AZ_RENDER_OBSERVE_OK);
    CHECK(az_render_observe_font_end_end(
        &state,
        &font_scope,
        (uintptr_t)TEST_DEVICE) == AZ_RENDER_OBSERVE_OK);
    for (index = 17u; index > 0u; --index) {
        CHECK(az_render_observe_render_menu_end(
            &state,
            &render_scopes[index - 1u],
            index == 1u ? 0 : -1,
            (uintptr_t)TEST_DEVICE) == AZ_RENDER_OBSERVE_OK);
    }

    (void)take_snapshot(&state, record);
    safety = read_be32(record + AZ_RENDER_OBSERVE_OFF_SAFETY_FLAGS);
    CHECK((safety & AZ_RENDER_OBSERVE_SAFETY_RENDER_NESTING_OVERFLOW) != 0u);
    CHECK((safety & AZ_RENDER_OBSERVE_SAFETY_CROSS_NESTING) != 0u);
    CHECK((safety & AZ_RENDER_OBSERVE_SAFETY_UNEXPECTED_FONT_CALLER) != 0u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_ENTERS) ==
        17u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_EXITS) ==
        17u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_NESTING) == 0u);
    CHECK(read_be32(
        record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_MAX_NESTING) ==
        AZ_RENDER_OBSERVE_REV1655_MAX_NESTING);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_CROSS_NESTING) == 1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_CALLER_OVERFLOW) ==
        13u);
    CHECK(read_be32(
        record + AZ_RENDER_OBSERVE_OFF_LAST_UNEXPECTED_FONT_LR) ==
        0x82211848u);
    for (index = 0u;
         index < AZ_RENDER_OBSERVE_REV1655_MAX_CALLERS;
         ++index) {
        CHECK(read_be32(record + AZ_RENDER_OBSERVE_CALLER_ADDRESS_OFFSET(
            index)) == TEST_RENDER_CALLER + index * 4u);
        CHECK(read_be16(record + AZ_RENDER_OBSERVE_CALLER_COUNT_OFFSET(
            index)) == 1u);
    }
}

static void test_invalid_inputs_scopes_and_scene_fail_closed(void)
{
    AzRenderObserveRev1655 state;
    AzRenderObserveScope scope;
    AzSceneGateDecision invalid_scene = make_scene(
        (AzSceneGateReason)99,
        0u);
    AzSceneGateDecision inconsistent_scene = make_scene(
        AZ_SCENE_GATE_REASON_MAIN_NOT_FOCUSED,
        1u);
    uint8_t record[AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE];
    uint32_t safety;

    az_render_observe_rev1655_init(&state, 1u);
    CHECK(az_render_observe_render_menu_begin(
        &state,
        (uintptr_t)0u,
        3u,
        (uintptr_t)(TEST_DEVICE + 1u),
        &scope) == AZ_RENDER_OBSERVE_OK);
    state.render_menu_nesting = 0u;
    CHECK(az_render_observe_render_menu_end(
        &state,
        &scope,
        7,
        (uintptr_t)0u) == AZ_RENDER_OBSERVE_OK);
    CHECK(az_render_observe_render_menu_end(
        &state,
        &scope,
        7,
        (uintptr_t)0u) == AZ_RENDER_OBSERVE_INVALID_SCOPE);
    CHECK(az_render_observe_note_scene(&state, &invalid_scene) ==
        AZ_RENDER_OBSERVE_INVALID_ARGUMENT);
    CHECK(az_render_observe_note_scene(&state, &inconsistent_scene) ==
        AZ_RENDER_OBSERVE_INVALID_ARGUMENT);

    (void)take_snapshot(&state, record);
    safety = read_be32(record + AZ_RENDER_OBSERVE_OFF_SAFETY_FLAGS);
    CHECK((safety & AZ_RENDER_OBSERVE_SAFETY_INVALID_POINTER) != 0u);
    CHECK((safety & AZ_RENDER_OBSERVE_SAFETY_INVALID_CALLER_LR) != 0u);
    CHECK((safety & AZ_RENDER_OBSERVE_SAFETY_NESTING_UNDERFLOW) != 0u);
    CHECK((safety & AZ_RENDER_OBSERVE_SAFETY_SCOPE_MISMATCH) != 0u);
    CHECK((safety & AZ_RENDER_OBSERVE_SAFETY_INVALID_SCENE) != 0u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_POINTER_ANOMALIES) == 2u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_DEVICE_MISSING) == 1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_NESTING_UNDERFLOWS) == 1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_INVALID_EVENTS) == 3u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_SCENE_SAMPLES) == 0u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_NONZERO) ==
        1u);
}

static void test_contention_is_bounded_and_visible(void)
{
    AzRenderObserveRev1655 state;
    AzRenderObserveScope scope;
    uint8_t record[AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE];
    uint32_t token = 0u;

    az_render_observe_rev1655_init(&state, 1u);
    state.writer_lock = 1u;
    CHECK(az_render_observe_render_menu_begin(
        &state,
        (uintptr_t)TEST_MANAGER,
        TEST_RENDER_CALLER,
        (uintptr_t)TEST_DEVICE,
        &scope) == AZ_RENDER_OBSERVE_BUSY);
    CHECK(scope.active == 0u);
    CHECK(az_render_observe_rev1655_snapshot_be(
        &state,
        record,
        sizeof(record),
        &token) == AZ_RENDER_OBSERVE_BUSY);
    state.writer_lock = 0u;
    (void)take_snapshot(&state, record);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_CONTENTION_DROPS) == 1u);
    CHECK((read_be32(record + AZ_RENDER_OBSERVE_OFF_SAFETY_FLAGS) &
        AZ_RENDER_OBSERVE_SAFETY_WRITER_CONTENTION) != 0u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_ENTERS) == 0u);
}

static void test_contended_ends_close_their_scopes(void)
{
    AzRenderObserveRev1655 state;
    AzRenderObserveScope scope;
    uint8_t record[AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE];

    az_render_observe_rev1655_init(&state, 1u);
    CHECK(az_render_observe_render_menu_begin(
        &state,
        (uintptr_t)TEST_MANAGER,
        TEST_RENDER_CALLER,
        (uintptr_t)TEST_DEVICE,
        &scope) == AZ_RENDER_OBSERVE_OK);
    CHECK(state.render_menu_nesting == 1u);
    state.writer_lock = 1u;
    CHECK(az_render_observe_render_menu_end(
        &state,
        &scope,
        0,
        (uintptr_t)TEST_DEVICE) == AZ_RENDER_OBSERVE_BUSY);
    CHECK(scope.active == 0u);
    CHECK(state.render_menu_nesting == 0u);
    state.writer_lock = 0u;
    (void)take_snapshot(&state, record);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_ENTERS) == 1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_EXITS) == 0u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_NESTING) == 0u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_CONTENTION_DROPS) == 1u);

    az_render_observe_rev1655_init(&state, 1u);
    CHECK(az_render_observe_font_end_begin(
        &state,
        (uintptr_t)TEST_FONT,
        AZ_RENDER_OBSERVE_REV1655_FONT_END_CALLER_LR,
        (uintptr_t)TEST_DEVICE,
        &scope) == AZ_RENDER_OBSERVE_OK);
    CHECK(state.font_end_nesting == 1u);
    state.writer_lock = 1u;
    CHECK(az_render_observe_font_end_end(
        &state,
        &scope,
        (uintptr_t)TEST_DEVICE) == AZ_RENDER_OBSERVE_BUSY);
    CHECK(scope.active == 0u);
    CHECK(state.font_end_nesting == 0u);
    state.writer_lock = 0u;
    (void)take_snapshot(&state, record);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_FONT_END_ENTERS) == 1u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_FONT_END_EXITS) == 0u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_FONT_END_NESTING) == 0u);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_CONTENTION_DROPS) == 1u);
    CHECK((read_be32(record + AZ_RENDER_OBSERVE_OFF_SAFETY_FLAGS) &
        AZ_RENDER_OBSERVE_SAFETY_WRITER_CONTENTION) != 0u);
}

#if defined(AURORAAZ_RENDER_OBSERVE_TEST_HOOKS)
static void test_snapshot_cannot_acknowledge_an_omitted_contention(void)
{
    AzRenderObserveRev1655 state;
    SnapshotContentionRace race;
    uint8_t before[AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE];
    uint8_t after[AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE];
    uint32_t before_token;
    uint32_t after_token;

    memset(&race, 0, sizeof(race));
    az_render_observe_rev1655_init(&state, 1u);
    state.test_after_snapshot_capture =
        inject_contention_after_snapshot_capture;
    state.test_hook_context = &race;

    before_token = take_snapshot(&state, before);
    CHECK(race.calls == 1u);
    CHECK(race.injected_result == AZ_RENDER_OBSERVE_BUSY);
    CHECK(race.injected_scope_active == 0u);
    CHECK(read_be32(before + AZ_RENDER_OBSERVE_OFF_CONTENTION_DROPS) == 0u);
    CHECK((read_be32(before + AZ_RENDER_OBSERVE_OFF_SAFETY_FLAGS) &
        AZ_RENDER_OBSERVE_SAFETY_WRITER_CONTENTION) == 0u);

    /* The injected update was omitted, so its older token must not clear it. */
    CHECK(az_render_observe_rev1655_acknowledge(
        &state,
        before_token) == 0u);
    CHECK(az_render_observe_rev1655_is_dirty(&state) == 1u);

    after_token = take_snapshot(&state, after);
    CHECK(after_token != before_token);
    CHECK(read_be32(after + AZ_RENDER_OBSERVE_OFF_CONTENTION_DROPS) == 1u);
    CHECK((read_be32(after + AZ_RENDER_OBSERVE_OFF_SAFETY_FLAGS) &
        AZ_RENDER_OBSERVE_SAFETY_WRITER_CONTENTION) != 0u);
    CHECK(az_render_observe_rev1655_acknowledge(
        &state,
        after_token) == 1u);
    CHECK(az_render_observe_rev1655_is_dirty(&state) == 0u);
}
#endif

static void test_snapshot_retry_ack_and_dual_slot_recovery(void)
{
    AzRenderObserveRev1655 state;
    AzRenderObserveScope scope;
    uint8_t slot_a[AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE];
    uint8_t slot_b[AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE];
    uint8_t retry[AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE];
    uint8_t selected = 99u;
    uint32_t generation = 0u;
    uint32_t first_token;
    uint32_t retry_token;
    uint32_t second_token;

    az_render_observe_rev1655_init(&state, 1u);
    CHECK(az_render_observe_rev1655_seed_generation(&state, 41u) ==
        AZ_RENDER_OBSERVE_OK);
    first_token = take_snapshot(&state, slot_a);
    CHECK(read_be32(slot_a + AZ_RENDER_OBSERVE_OFF_GENERATION) == 42u);
    retry_token = take_snapshot(&state, retry);
    CHECK(retry_token == first_token);
    CHECK(memcmp(slot_a, retry, sizeof(slot_a)) == 0);
    CHECK(az_render_observe_rev1655_acknowledge(
        &state,
        first_token + 1u) == 0u);
    CHECK(az_render_observe_rev1655_acknowledge(&state, first_token) == 1u);
    CHECK(az_render_observe_rev1655_snapshot_be(
        &state,
        retry,
        sizeof(retry),
        &retry_token) == AZ_RENDER_OBSERVE_NO_CHANGE);

    CHECK(az_render_observe_render_menu_begin(
        &state,
        (uintptr_t)TEST_MANAGER,
        TEST_RENDER_CALLER,
        (uintptr_t)TEST_DEVICE,
        &scope) == AZ_RENDER_OBSERVE_OK);
    CHECK(az_render_observe_render_menu_end(
        &state,
        &scope,
        0,
        (uintptr_t)TEST_DEVICE) == AZ_RENDER_OBSERVE_OK);
    second_token = take_snapshot(&state, slot_b);
    CHECK(read_be32(slot_b + AZ_RENDER_OBSERVE_OFF_GENERATION) == 43u);
    CHECK(az_render_observe_rev1655_acknowledge(&state, first_token) == 0u);
    CHECK(az_render_observe_rev1655_select_newest_be(
        slot_a,
        sizeof(slot_a),
        slot_b,
        sizeof(slot_b),
        &selected,
        &generation) == AZ_RENDER_OBSERVE_OK);
    CHECK(selected == AZ_RENDER_OBSERVE_REV1655_SLOT_B);
    CHECK(generation == 43u);
    CHECK(az_render_observe_rev1655_acknowledge(&state, second_token) == 1u);

    slot_b[AZ_RENDER_OBSERVE_OFF_LAST_MANAGER] ^= 1u;
    CHECK(az_render_observe_rev1655_select_newest_be(
        slot_a,
        sizeof(slot_a),
        slot_b,
        sizeof(slot_b),
        &selected,
        &generation) == AZ_RENDER_OBSERVE_OK);
    CHECK(selected == AZ_RENDER_OBSERVE_REV1655_SLOT_A);

    az_render_observe_rev1655_init(&state, 1u);
    CHECK(az_render_observe_rev1655_seed_generation(&state, UINT32_MAX) ==
        AZ_RENDER_OBSERVE_OK);
    (void)take_snapshot(&state, slot_b);
    CHECK(read_be32(slot_b + AZ_RENDER_OBSERVE_OFF_GENERATION) == 0u);
}

static void test_record_corruption_and_semantic_rejection(void)
{
    AzRenderObserveRev1655 state;
    uint8_t valid[AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE];
    uint8_t damaged[AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE];
    uint8_t oversized[AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE + 1u];
    uint32_t generation = 0u;
    uint32_t token = 0u;

    az_render_observe_rev1655_init(&state, 1u);
    (void)take_snapshot(&state, valid);
    CHECK(az_render_observe_rev1655_validate_record_be(
        valid,
        sizeof(valid) - 1u,
        &generation) == AZ_RENDER_OBSERVE_BUFFER_TOO_SMALL);
    CHECK(az_render_observe_rev1655_validate_record_be(
        valid,
        sizeof(valid),
        &generation) == AZ_RENDER_OBSERVE_OK);

    memcpy(damaged, valid, sizeof(damaged));
    damaged[AZ_RENDER_OBSERVE_OFF_RENDER_MENU_ENTERS] ^= 1u;
    CHECK(az_render_observe_rev1655_validate_record_be(
        damaged,
        sizeof(damaged),
        &generation) == AZ_RENDER_OBSERVE_INVALID_RECORD);

    memcpy(damaged, valid, sizeof(damaged));
    damaged[AZ_RENDER_OBSERVE_OFF_RESERVED] = 1u;
    repair_crc(damaged);
    CHECK(az_render_observe_rev1655_validate_record_be(
        damaged,
        sizeof(damaged),
        &generation) == AZ_RENDER_OBSERVE_INVALID_RECORD);

    memcpy(damaged, valid, sizeof(damaged));
    damaged[AZ_RENDER_OBSERVE_OFF_ARMED] = 2u;
    repair_crc(damaged);
    CHECK(az_render_observe_rev1655_validate_record_be(
        damaged,
        sizeof(damaged),
        &generation) == AZ_RENDER_OBSERVE_INVALID_RECORD);

    memcpy(damaged, valid, sizeof(damaged));
    write_be32(
        damaged + AZ_RENDER_OBSERVE_CALLER_ADDRESS_OFFSET(0u),
        TEST_RENDER_CALLER);
    repair_crc(damaged);
    CHECK(az_render_observe_rev1655_validate_record_be(
        damaged,
        sizeof(damaged),
        &generation) == AZ_RENDER_OBSERVE_INVALID_RECORD);

    memset(oversized, 0, sizeof(oversized));
    CHECK(az_render_observe_rev1655_snapshot_be(
        &state,
        oversized,
        sizeof(oversized),
        &token) == AZ_RENDER_OBSERVE_INVALID_ARGUMENT);
}

static void test_saturation_disarm_and_argument_guards(void)
{
    AzRenderObserveRev1655 state;
    AzRenderObserveScope scope;
    AzSceneGateDecision focused = make_scene(
        AZ_SCENE_GATE_REASON_MAIN_FOCUSED,
        1u);
    uint8_t record[AZ_RENDER_OBSERVE_REV1655_RECORD_SIZE];
    uint8_t selected = 0u;
    uint32_t generation = 0u;
    uint32_t token = 0u;

    az_render_observe_rev1655_init(NULL, 1u);
    az_render_observe_rev1655_init(&state, 1u);
    state.render_menu_enters = UINT32_MAX;
    state.render_caller_addresses[0] = TEST_RENDER_CALLER;
    state.render_caller_counts[0] = UINT32_MAX;
    state.scene_samples = UINT32_MAX;
    state.scene_reason_counts[AZ_SCENE_GATE_REASON_MAIN_FOCUSED] = UINT32_MAX;
    CHECK(az_render_observe_render_menu_begin(
        &state,
        (uintptr_t)TEST_MANAGER,
        TEST_RENDER_CALLER,
        (uintptr_t)TEST_DEVICE,
        &scope) == AZ_RENDER_OBSERVE_OK);
    CHECK(az_render_observe_render_menu_end(
        &state,
        &scope,
        0,
        (uintptr_t)TEST_DEVICE) == AZ_RENDER_OBSERVE_OK);
    CHECK(az_render_observe_note_scene(&state, &focused) ==
        AZ_RENDER_OBSERVE_OK);
    (void)take_snapshot(&state, record);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_RENDER_MENU_ENTERS) ==
        UINT32_MAX);
    CHECK(read_be16(record + AZ_RENDER_OBSERVE_CALLER_COUNT_OFFSET(0u)) ==
        UINT16_MAX);
    CHECK(read_be32(record + AZ_RENDER_OBSERVE_OFF_SCENE_SAMPLES) ==
        UINT32_MAX);

    CHECK(az_render_observe_rev1655_disarm(&state) == AZ_RENDER_OBSERVE_OK);
    CHECK(az_render_observe_rev1655_disarm(&state) ==
        AZ_RENDER_OBSERVE_NO_CHANGE);
    CHECK(az_render_observe_font_end_begin(
        &state,
        (uintptr_t)TEST_FONT,
        AZ_RENDER_OBSERVE_REV1655_FONT_END_CALLER_LR,
        (uintptr_t)TEST_DEVICE,
        &scope) == AZ_RENDER_OBSERVE_NOT_ARMED);

    CHECK(az_render_observe_rev1655_disarm(NULL) == AZ_RENDER_OBSERVE_NULL);
    CHECK(az_render_observe_render_menu_begin(
        NULL,
        0u,
        0u,
        0u,
        &scope) == AZ_RENDER_OBSERVE_NULL);
    CHECK(az_render_observe_render_menu_begin(
        &state,
        0u,
        0u,
        0u,
        NULL) == AZ_RENDER_OBSERVE_NULL);
    CHECK(az_render_observe_render_menu_end(
        NULL,
        &scope,
        0,
        0u) == AZ_RENDER_OBSERVE_NULL);
    CHECK(az_render_observe_font_end_begin(
        NULL,
        0u,
        0u,
        0u,
        &scope) == AZ_RENDER_OBSERVE_NULL);
    CHECK(az_render_observe_font_end_end(
        NULL,
        &scope,
        0u) == AZ_RENDER_OBSERVE_NULL);
    CHECK(az_render_observe_note_scene(NULL, &focused) ==
        AZ_RENDER_OBSERVE_NULL);
    CHECK(az_render_observe_note_scene(&state, NULL) ==
        AZ_RENDER_OBSERVE_NULL);
    CHECK(az_render_observe_rev1655_seed_generation(NULL, 1u) ==
        AZ_RENDER_OBSERVE_NULL);
    CHECK(az_render_observe_rev1655_seed_generation(&state, 1u) ==
        AZ_RENDER_OBSERVE_INVALID_ARGUMENT);
    CHECK(az_render_observe_rev1655_snapshot_be(
        NULL,
        record,
        sizeof(record),
        &token) == AZ_RENDER_OBSERVE_NULL);
    CHECK(az_render_observe_rev1655_snapshot_be(
        &state,
        record,
        sizeof(record) - 1u,
        &token) == AZ_RENDER_OBSERVE_BUFFER_TOO_SMALL);
    CHECK(az_render_observe_rev1655_validate_record_be(
        NULL,
        sizeof(record),
        &generation) == AZ_RENDER_OBSERVE_NULL);
    CHECK(az_render_observe_rev1655_acknowledge(NULL, token) == 0u);
    CHECK(az_render_observe_rev1655_is_dirty(NULL) == 0u);
    CHECK(az_render_observe_rev1655_select_newest_be(
        NULL,
        0u,
        NULL,
        0u,
        &selected,
        &generation) == AZ_RENDER_OBSERVE_INVALID_RECORD);
    CHECK(az_render_observe_rev1655_select_newest_be(
        record,
        sizeof(record),
        record,
        sizeof(record),
        NULL,
        &generation) == AZ_RENDER_OBSERVE_NULL);
    CHECK(strcmp(
        az_render_observe_result_name(AZ_RENDER_OBSERVE_BUSY),
        "busy") == 0);
    CHECK(strcmp(
        az_render_observe_result_name((AzRenderObserveResult)99),
        "unknown") == 0);
}

int main(void)
{
    test_unverified_stage_fails_closed();
    test_happy_render_font_and_scene_observation();
    test_nesting_and_bounded_caller_samples();
    test_invalid_inputs_scopes_and_scene_fail_closed();
    test_contention_is_bounded_and_visible();
    test_contended_ends_close_their_scopes();
#if defined(AURORAAZ_RENDER_OBSERVE_TEST_HOOKS)
    test_snapshot_cannot_acknowledge_an_omitted_contention();
#endif
    test_snapshot_retry_ack_and_dual_slot_recovery();
    test_record_corruption_and_semantic_rejection();
    test_saturation_disarm_and_argument_guards();

    if (g_failures != 0u) {
        (void)fprintf(
            stderr,
            "%u render-observe test(s) failed\n",
            g_failures);
        return 1;
    }
    (void)puts("render_observe_rev1655_tests: all checks passed");
    return 0;
}
