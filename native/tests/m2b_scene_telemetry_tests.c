#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <auroraaz/m2b_scene_telemetry.h>

typedef struct FakeProbe {
    AzSceneGateDecision decision;
    AzSceneGateStatus status;
    uint8_t raw_result;
    uint32_t probe_calls;
    uint32_t status_calls;
} FakeProbe;

static unsigned int g_failures;

#define CHECK(expression)                                                     \
    do {                                                                      \
        if (!(expression)) {                                                  \
            (void)fprintf(                                                    \
                stderr,                                                       \
                "%s:%d: CHECK failed: %s\n",                              \
                __FILE__,                                                     \
                __LINE__,                                                     \
                #expression);                                                 \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

static uint16_t get_be16(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8u) |
        (uint16_t)bytes[1]);
}

static uint32_t get_be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24u) |
        ((uint32_t)bytes[1] << 16u) |
        ((uint32_t)bytes[2] << 8u) |
        (uint32_t)bytes[3];
}

static void put_be32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24u);
    bytes[1] = (uint8_t)(value >> 16u);
    bytes[2] = (uint8_t)(value >> 8u);
    bytes[3] = (uint8_t)value;
}

static uint32_t test_crc32(const uint8_t *bytes, size_t size)
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
    put_be32(
        record + AZ_M2B_SCENE_OFF_CRC32,
        test_crc32(record, (size_t)AZ_M2B_SCENE_CRC32_PREFIX_SIZE));
}

static void fake_set_decision(
    FakeProbe *fake,
    AzSceneGateReason reason,
    uint8_t allowed,
    uint32_t frame_probe_count)
{
    memset(fake, 0, sizeof(*fake));
    fake->decision.reason = reason;
    fake->decision.cache_head = 0x90001000u;
    fake->decision.main_scene_node = 0x90001020u;
    fake->decision.main_scene_handle = 0x12340001u;
    fake->decision.scanned_nodes = 3u;
    fake->decision.allows_capture = allowed;
    fake->raw_result = allowed;

    fake->status.last_configure_result = AZ_SCENE_GATE_CONFIGURE_OK;
    fake->status.last_reason = reason;
    fake->status.configure_attempts = 1u;
    fake->status.configure_successes = 1u;
    fake->status.probes = frame_probe_count;
    fake->status.allowed = allowed != 0u ? frame_probe_count : 0u;
    fake->status.denied = allowed == 0u ? frame_probe_count : 0u;
    fake->status.main_not_focused =
        reason == AZ_SCENE_GATE_REASON_MAIN_NOT_FOCUSED ?
            frame_probe_count : 0u;
    fake->status.last_cache_head = fake->decision.cache_head;
    fake->status.last_main_scene_node = fake->decision.main_scene_node;
    fake->status.last_main_scene_handle = fake->decision.main_scene_handle;
    fake->status.last_scanned_nodes = fake->decision.scanned_nodes;
    fake->status.configured = 1u;
    fake->status.exact_image_verified = 1u;
    fake->status.signatures_verified = 1u;
}

static uint8_t fake_probe(void *context, AzSceneGateDecision *decision)
{
    FakeProbe *fake = (FakeProbe *)context;

    ++fake->probe_calls;
    *decision = fake->decision;
    return fake->raw_result;
}

static void fake_status(void *context, AzSceneGateStatus *status)
{
    FakeProbe *fake = (FakeProbe *)context;

    ++fake->status_calls;
    *status = fake->status;
}

static AzM2bSceneProbeBindings fake_bindings(FakeProbe *fake)
{
    AzM2bSceneProbeBindings bindings;

    bindings.context = fake;
    bindings.probe = &fake_probe;
    bindings.snapshot_status = &fake_status;
    return bindings;
}

static AzM2bSceneObservation make_observation(
    AzSceneGateReason reason,
    uint8_t allowed,
    uint32_t frame,
    uint8_t system_ui_active)
{
    FakeProbe fake;
    AzM2bSceneProbeBindings bindings;
    AzM2bSceneObservation observation;

    fake_set_decision(&fake, reason, allowed, frame);
    bindings = fake_bindings(&fake);
    CHECK(az_m2b_scene_observe_live(
        &bindings,
        frame,
        system_ui_active,
        &observation) == AZ_M2B_SCENE_TELEMETRY_OK);
    return observation;
}

static uint32_t take_snapshot(
    AzM2bSceneTelemetry *telemetry,
    uint8_t *record)
{
    uint32_t token = 0u;

    CHECK(az_m2b_scene_telemetry_snapshot_be(
        telemetry,
        record,
        AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE,
        &token) == AZ_M2B_SCENE_TELEMETRY_OK);
    return token;
}

static void test_live_probe_is_strict_and_fail_closed(void)
{
    FakeProbe fake;
    AzM2bSceneProbeBindings bindings;
    AzM2bSceneObservation observation;

    fake_set_decision(&fake, AZ_SCENE_GATE_REASON_MAIN_FOCUSED, 1u, 7u);
    bindings = fake_bindings(&fake);
    CHECK(az_m2b_scene_observe_live(
        &bindings, 100u, 0u, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    CHECK(fake.probe_calls == 1u);
    CHECK(fake.status_calls == 1u);
    CHECK(observation.callback_available == 1u);
    CHECK(observation.raw_probe_allowed == 1u);
    CHECK(observation.capture_eligible == 1u);
    CHECK(observation.decision.main_scene_handle == 0x12340001u);

    CHECK(az_m2b_scene_observe_live(
        &bindings, 101u, 1u, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    CHECK(observation.raw_probe_allowed == 1u);
    CHECK(observation.capture_eligible == 0u);

    fake.status.signatures_verified = 0u;
    CHECK(az_m2b_scene_observe_live(
        &bindings, 102u, 0u, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    CHECK(observation.capture_eligible == 0u);

    memset(&observation, 0xA5, sizeof(observation));
    CHECK(az_m2b_scene_observe_live(
        NULL, 103u, 0u, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_PROBE_UNAVAILABLE);
    CHECK(observation.callback_available == 0u);
    CHECK(observation.raw_probe_allowed == 0u);
    CHECK(observation.capture_eligible == 0u);
    CHECK(observation.decision.reason ==
        AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED);
    CHECK(observation.status.last_configure_result ==
        AZ_SCENE_GATE_CONFIGURE_BAD_BINDINGS);
    CHECK(az_m2b_scene_observe_live(NULL, 0u, 0u, NULL) ==
        AZ_M2B_SCENE_TELEMETRY_NULL);
}

static void test_baseline_wire_record_and_retry(void)
{
    AzM2bSceneTelemetry telemetry;
    uint8_t first[AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE];
    uint8_t retry[AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE];
    uint32_t generation = 99u;
    uint32_t token;
    uint32_t retry_token;

    az_m2b_scene_telemetry_init(&telemetry);
    CHECK(az_m2b_scene_telemetry_is_dirty(&telemetry) == 1u);
    token = take_snapshot(&telemetry, first);
    retry_token = take_snapshot(&telemetry, retry);
    CHECK(token == retry_token);
    CHECK(memcmp(first, retry, sizeof(first)) == 0);
    CHECK(memcmp(first, "AZS2", 4u) == 0);
    CHECK(get_be16(first + AZ_M2B_SCENE_OFF_VERSION) ==
        AZ_M2B_SCENE_TELEMETRY_VERSION);
    CHECK(get_be16(first + AZ_M2B_SCENE_OFF_RECORD_SIZE) ==
        AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE);
    CHECK(get_be32(first + AZ_M2B_SCENE_OFF_GENERATION) == 1u);
    CHECK(get_be32(first + AZ_M2B_SCENE_OFF_SAMPLES) == 0u);
    CHECK(first[AZ_M2B_SCENE_OFF_LAST_DECISION_REASON] ==
        (uint8_t)AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED);
    CHECK(first[AZ_M2B_SCENE_OFF_LAST_CONFIGURE_RESULT] ==
        (uint8_t)AZ_SCENE_GATE_CONFIGURE_BAD_BINDINGS);
    CHECK(get_be32(first + AZ_M2B_SCENE_OFF_CRC32) ==
        test_crc32(first, (size_t)AZ_M2B_SCENE_CRC32_PREFIX_SIZE));
    /* Independent zlib/IEEE CRC-32 value for the canonical zero baseline. */
    CHECK(get_be32(first + AZ_M2B_SCENE_OFF_CRC32) == 0xE242CDF9u);
    CHECK(az_m2b_scene_telemetry_validate_record_be(
        first, sizeof(first), &generation) == AZ_M2B_SCENE_TELEMETRY_OK);
    CHECK(generation == 1u);
    CHECK(az_m2b_scene_telemetry_acknowledge(&telemetry, token) == 1u);
    CHECK(az_m2b_scene_telemetry_is_dirty(&telemetry) == 0u);
    CHECK(az_m2b_scene_telemetry_snapshot_be(
        &telemetry, first, sizeof(first), &token) ==
        AZ_M2B_SCENE_TELEMETRY_NO_CHANGE);
}

static void test_hardware_sequence_aggregation(void)
{
    AzM2bSceneTelemetry telemetry;
    AzM2bSceneObservation observation;
    uint8_t record[AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE];
    uint32_t token;

    az_m2b_scene_telemetry_init(&telemetry);
    token = take_snapshot(&telemetry, record);
    CHECK(az_m2b_scene_telemetry_acknowledge(&telemetry, token) == 1u);

    observation = make_observation(
        AZ_SCENE_GATE_REASON_MAIN_FOCUSED, 1u, 10u, 0u);
    CHECK(az_m2b_scene_telemetry_record(&telemetry, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    observation = make_observation(
        AZ_SCENE_GATE_REASON_MAIN_FOCUSED, 1u, 11u, 0u);
    CHECK(az_m2b_scene_telemetry_record(&telemetry, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    observation = make_observation(
        AZ_SCENE_GATE_REASON_MAIN_NOT_FOCUSED, 0u, 12u, 0u);
    CHECK(az_m2b_scene_telemetry_record(&telemetry, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    observation = make_observation(
        AZ_SCENE_GATE_REASON_MAIN_FOCUSED, 1u, 13u, 1u);
    CHECK(az_m2b_scene_telemetry_record(&telemetry, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    (void)take_snapshot(&telemetry, record);

    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_SAMPLES) == 4u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_RAW_ALLOWED) == 3u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_RAW_DENIED) == 1u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_ELIGIBLE) == 2u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_TRANSITIONS) == 2u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_FIRST_FRAME) == 10u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_LAST_FRAME) == 13u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_LAST_TRANSITION_FRAME) == 13u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_UI_ACTIVE_SAMPLES) == 1u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_UI_RAW_ALLOWED) == 1u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_INVALID_SAMPLES) == 0u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_SAFETY_FLAGS) ==
        AZ_M2B_SCENE_SAFETY_SYSTEM_UI_RAW_ALLOW);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_LAST_SAMPLE_SAFETY_FLAGS) ==
        AZ_M2B_SCENE_SAFETY_SYSTEM_UI_RAW_ALLOW);
    CHECK(get_be32(record + AZ_M2B_SCENE_REASON_COUNTER_OFFSET(
        AZ_SCENE_GATE_REASON_MAIN_FOCUSED)) == 3u);
    CHECK(get_be32(record + AZ_M2B_SCENE_REASON_COUNTER_OFFSET(
        AZ_SCENE_GATE_REASON_MAIN_NOT_FOCUSED)) == 1u);
    CHECK(record[AZ_M2B_SCENE_OFF_LAST_ELIGIBLE] == 0u);
    CHECK(record[AZ_M2B_SCENE_OFF_LAST_SYSTEM_UI_ACTIVE] == 1u);
}

static void test_inconsistent_observations_are_visible_and_denied(void)
{
    AzM2bSceneTelemetry telemetry;
    AzM2bSceneObservation observation = make_observation(
        AZ_SCENE_GATE_REASON_MAIN_FOCUSED, 1u, 20u, 0u);
    uint8_t record[AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE];
    uint32_t flags;

    az_m2b_scene_telemetry_init(&telemetry);
    observation.decision.allows_capture = 0u;
    observation.status.configured = 0u;
    observation.capture_eligible = 1u;
    CHECK(az_m2b_scene_telemetry_record(&telemetry, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    (void)take_snapshot(&telemetry, record);
    flags = get_be32(record + AZ_M2B_SCENE_OFF_SAFETY_FLAGS);
    CHECK((flags & AZ_M2B_SCENE_SAFETY_PROBE_DECISION_MISMATCH) != 0u);
    CHECK((flags & AZ_M2B_SCENE_SAFETY_REASON_ALLOW_MISMATCH) != 0u);
    CHECK((flags & AZ_M2B_SCENE_SAFETY_GATE_NOT_VERIFIED) != 0u);
    CHECK((flags & AZ_M2B_SCENE_SAFETY_STATUS_DECISION_MISMATCH) == 0u);
    CHECK((flags & AZ_M2B_SCENE_SAFETY_ELIGIBILITY_MISMATCH) != 0u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_INVALID_SAMPLES) == 1u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_ELIGIBLE) == 0u);
    CHECK(record[AZ_M2B_SCENE_OFF_LAST_ELIGIBLE] == 0u);

    observation = make_observation(
        AZ_SCENE_GATE_REASON_MAIN_FOCUSED, 1u, 21u, 0u);
    observation.status.last_main_scene_handle ^= 1u;
    observation.capture_eligible = 0u;
    CHECK(az_m2b_scene_telemetry_record(&telemetry, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    (void)take_snapshot(&telemetry, record);
    flags = get_be32(record + AZ_M2B_SCENE_OFF_LAST_SAMPLE_SAFETY_FLAGS);
    CHECK((flags & AZ_M2B_SCENE_SAFETY_STATUS_DECISION_MISMATCH) != 0u);
    CHECK(record[AZ_M2B_SCENE_OFF_LAST_ELIGIBLE] == 0u);
}

static void test_invalid_observation_serializes_fail_closed(void)
{
    AzM2bSceneTelemetry telemetry;
    AzM2bSceneObservation observation = make_observation(
        AZ_SCENE_GATE_REASON_MAIN_FOCUSED, 1u, 22u, 0u);
    uint8_t record[AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE];
    uint32_t generation = 0u;
    uint32_t flags;

    az_m2b_scene_telemetry_init(&telemetry);
    observation.decision.reason = (AzSceneGateReason)99;
    observation.decision.allows_capture = 2u;
    observation.status.last_configure_result =
        (AzSceneGateConfigureResult)99;
    observation.status.last_reason = (AzSceneGateReason)99;
    observation.status.configured = 2u;
    observation.status.exact_image_verified = 2u;
    observation.status.signatures_verified = 2u;
    observation.callback_available = 2u;
    observation.raw_probe_allowed = 2u;
    observation.capture_eligible = 1u;
    observation.system_ui_active = 2u;

    CHECK(az_m2b_scene_telemetry_record(&telemetry, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    (void)take_snapshot(&telemetry, record);
    flags = get_be32(record + AZ_M2B_SCENE_OFF_SAFETY_FLAGS);
    CHECK((flags & AZ_M2B_SCENE_SAFETY_CALLBACK_UNAVAILABLE) != 0u);
    CHECK((flags & AZ_M2B_SCENE_SAFETY_INVALID_VALUE) != 0u);
    CHECK((flags & AZ_M2B_SCENE_SAFETY_GATE_NOT_VERIFIED) != 0u);
    CHECK((flags & AZ_M2B_SCENE_SAFETY_ELIGIBILITY_MISMATCH) != 0u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_INVALID_SAMPLES) == 1u);
    CHECK(record[AZ_M2B_SCENE_OFF_LAST_CONFIGURE_RESULT] ==
        (uint8_t)AZ_SCENE_GATE_CONFIGURE_BAD_BINDINGS);
    CHECK(record[AZ_M2B_SCENE_OFF_LAST_DECISION_REASON] ==
        (uint8_t)AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED);
    CHECK(record[AZ_M2B_SCENE_OFF_LAST_STATUS_REASON] ==
        (uint8_t)AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED);
    CHECK(record[AZ_M2B_SCENE_OFF_LAST_CALLBACK_AVAILABLE] == 0u);
    CHECK(record[AZ_M2B_SCENE_OFF_LAST_RAW_PROBE] == 0u);
    CHECK(record[AZ_M2B_SCENE_OFF_LAST_DECISION_ALLOWS] == 0u);
    CHECK(record[AZ_M2B_SCENE_OFF_LAST_ELIGIBLE] == 0u);
    CHECK(record[AZ_M2B_SCENE_OFF_LAST_SYSTEM_UI_ACTIVE] == 0u);
    CHECK(record[AZ_M2B_SCENE_OFF_LAST_STATUS_CONFIGURED] == 0u);
    CHECK(record[AZ_M2B_SCENE_OFF_LAST_EXACT_IMAGE_VERIFIED] == 0u);
    CHECK(record[AZ_M2B_SCENE_OFF_LAST_SIGNATURES_VERIFIED] == 0u);
    CHECK(az_m2b_scene_telemetry_validate_record_be(
        record, sizeof(record), &generation) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
}

static void test_missing_callback_frame_order_and_drop_count(void)
{
    AzM2bSceneTelemetry telemetry;
    AzM2bSceneObservation observation;
    uint8_t record[AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE];

    az_m2b_scene_telemetry_init(&telemetry);
    CHECK(az_m2b_scene_telemetry_probe_frame(
        &telemetry, NULL, 50u, 0u, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_PROBE_UNAVAILABLE);
    CHECK(observation.capture_eligible == 0u);
    CHECK(az_m2b_scene_telemetry_record(&telemetry, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    CHECK(az_m2b_scene_telemetry_update_drops(&telemetry, 3u) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    CHECK(az_m2b_scene_telemetry_update_drops(&telemetry, 3u) ==
        AZ_M2B_SCENE_TELEMETRY_NO_CHANGE);
    (void)take_snapshot(&telemetry, record);

    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_SAMPLES) == 2u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_CALLBACK_UNAVAILABLE) == 2u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_NONMONOTONIC_FRAMES) == 1u);
    CHECK(get_be32(record + AZ_M2B_SCENE_OFF_OBSERVATION_DROPS) == 3u);
    CHECK((get_be32(record + AZ_M2B_SCENE_OFF_SAFETY_FLAGS) &
        AZ_M2B_SCENE_SAFETY_FRAME_NONMONOTONIC) != 0u);
}

static void test_generation_seed_ack_and_slot_selection(void)
{
    AzM2bSceneTelemetry left;
    AzM2bSceneTelemetry right;
    AzM2bSceneObservation observation = make_observation(
        AZ_SCENE_GATE_REASON_MAIN_FOCUSED, 1u, 1u, 0u);
    uint8_t slot_a[AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE];
    uint8_t slot_b[AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE];
    uint8_t selected = 99u;
    uint32_t generation = 0u;
    uint32_t old_token;
    uint32_t token;

    az_m2b_scene_telemetry_init(&left);
    CHECK(az_m2b_scene_telemetry_seed_generation(&left, 40u) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    old_token = take_snapshot(&left, slot_a);
    CHECK(get_be32(slot_a + AZ_M2B_SCENE_OFF_GENERATION) == 41u);
    CHECK(az_m2b_scene_telemetry_record(&left, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    CHECK(az_m2b_scene_telemetry_acknowledge(&left, old_token) == 0u);
    token = take_snapshot(&left, slot_a);
    CHECK(get_be32(slot_a + AZ_M2B_SCENE_OFF_GENERATION) == 42u);
    CHECK(az_m2b_scene_telemetry_acknowledge(&left, token) == 1u);
    CHECK(az_m2b_scene_telemetry_seed_generation(&left, 8u) ==
        AZ_M2B_SCENE_TELEMETRY_INVALID_ARGUMENT);

    az_m2b_scene_telemetry_init(&right);
    CHECK(az_m2b_scene_telemetry_seed_generation(&right, 42u) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    (void)take_snapshot(&right, slot_b);
    CHECK(az_m2b_scene_telemetry_select_newest_be(
        slot_a, sizeof(slot_a), slot_b, sizeof(slot_b),
        &selected, &generation) == AZ_M2B_SCENE_TELEMETRY_OK);
    CHECK(selected == AZ_M2B_SCENE_TELEMETRY_SLOT_B);
    CHECK(generation == 43u);

    slot_b[0] ^= 1u;
    CHECK(az_m2b_scene_telemetry_select_newest_be(
        slot_a, sizeof(slot_a), slot_b, sizeof(slot_b),
        &selected, &generation) == AZ_M2B_SCENE_TELEMETRY_OK);
    CHECK(selected == AZ_M2B_SCENE_TELEMETRY_SLOT_A);
    CHECK(generation == 42u);
    CHECK(az_m2b_scene_telemetry_select_newest_be(
        NULL, 0u, slot_b, sizeof(slot_b), &selected, &generation) ==
        AZ_M2B_SCENE_TELEMETRY_INVALID_RECORD);
}

static void test_record_validation_rejects_corruption(void)
{
    AzM2bSceneTelemetry telemetry;
    AzM2bSceneObservation observation = make_observation(
        AZ_SCENE_GATE_REASON_MAIN_FOCUSED, 1u, 1u, 0u);
    uint8_t good[AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE];
    uint8_t mutated[AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE];
    uint32_t generation = 0u;

    az_m2b_scene_telemetry_init(&telemetry);
    CHECK(az_m2b_scene_telemetry_record(&telemetry, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_OK);
    (void)take_snapshot(&telemetry, good);
    CHECK(az_m2b_scene_telemetry_validate_record_be(
        good, sizeof(good), &generation) == AZ_M2B_SCENE_TELEMETRY_OK);

    memcpy(mutated, good, sizeof(mutated));
    mutated[0] ^= 1u;
    CHECK(az_m2b_scene_telemetry_validate_record_be(
        mutated, sizeof(mutated), &generation) ==
        AZ_M2B_SCENE_TELEMETRY_INVALID_RECORD);

    memcpy(mutated, good, sizeof(mutated));
    mutated[175u] = 1u;
    repair_crc(mutated);
    CHECK(az_m2b_scene_telemetry_validate_record_be(
        mutated, sizeof(mutated), &generation) ==
        AZ_M2B_SCENE_TELEMETRY_INVALID_RECORD);

    memcpy(mutated, good, sizeof(mutated));
    put_be32(mutated + AZ_M2B_SCENE_OFF_SAFETY_FLAGS, 0x80000000u);
    repair_crc(mutated);
    CHECK(az_m2b_scene_telemetry_validate_record_be(
        mutated, sizeof(mutated), &generation) ==
        AZ_M2B_SCENE_TELEMETRY_INVALID_RECORD);

    memcpy(mutated, good, sizeof(mutated));
    put_be32(mutated + AZ_M2B_SCENE_OFF_SAMPLES, 0u);
    repair_crc(mutated);
    CHECK(az_m2b_scene_telemetry_validate_record_be(
        mutated, sizeof(mutated), &generation) ==
        AZ_M2B_SCENE_TELEMETRY_INVALID_RECORD);

    memcpy(mutated, good, sizeof(mutated));
    mutated[AZ_M2B_SCENE_OFF_LAST_SYSTEM_UI_ACTIVE] = 1u;
    repair_crc(mutated);
    CHECK(az_m2b_scene_telemetry_validate_record_be(
        mutated, sizeof(mutated), &generation) ==
        AZ_M2B_SCENE_TELEMETRY_INVALID_RECORD);

    CHECK(az_m2b_scene_telemetry_validate_record_be(
        good, sizeof(good) - 1u, &generation) ==
        AZ_M2B_SCENE_TELEMETRY_BUFFER_TOO_SMALL);
    CHECK(az_m2b_scene_telemetry_validate_record_be(
        good, sizeof(good) + 1u, &generation) ==
        AZ_M2B_SCENE_TELEMETRY_INVALID_RECORD);
    CHECK(az_m2b_scene_telemetry_validate_record_be(
        NULL, sizeof(good), &generation) == AZ_M2B_SCENE_TELEMETRY_NULL);
}

static void test_null_and_small_buffer_contracts(void)
{
    AzM2bSceneTelemetry telemetry;
    AzM2bSceneObservation observation;
    uint8_t record[AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE];
    uint32_t token;

    az_m2b_scene_telemetry_init(NULL);
    az_m2b_scene_telemetry_init(&telemetry);
    CHECK(az_m2b_scene_telemetry_record(NULL, &observation) ==
        AZ_M2B_SCENE_TELEMETRY_NULL);
    CHECK(az_m2b_scene_telemetry_record(&telemetry, NULL) ==
        AZ_M2B_SCENE_TELEMETRY_NULL);
    CHECK(az_m2b_scene_telemetry_update_drops(NULL, 1u) ==
        AZ_M2B_SCENE_TELEMETRY_NULL);
    CHECK(az_m2b_scene_telemetry_snapshot_be(
        &telemetry, record, sizeof(record) - 1u, &token) ==
        AZ_M2B_SCENE_TELEMETRY_BUFFER_TOO_SMALL);
    CHECK(az_m2b_scene_telemetry_snapshot_be(
        NULL, record, sizeof(record), &token) ==
        AZ_M2B_SCENE_TELEMETRY_NULL);
    CHECK(az_m2b_scene_telemetry_acknowledge(NULL, 1u) == 0u);
    CHECK(az_m2b_scene_telemetry_is_dirty(NULL) == 0u);
    CHECK(az_m2b_scene_telemetry_select_newest_be(
        NULL, 0u, NULL, 0u, NULL, &token) ==
        AZ_M2B_SCENE_TELEMETRY_NULL);
}

int main(void)
{
    test_live_probe_is_strict_and_fail_closed();
    test_baseline_wire_record_and_retry();
    test_hardware_sequence_aggregation();
    test_inconsistent_observations_are_visible_and_denied();
    test_invalid_observation_serializes_fail_closed();
    test_missing_callback_frame_order_and_drop_count();
    test_generation_seed_ack_and_slot_selection();
    test_record_validation_rejects_corruption();
    test_null_and_small_buffer_contracts();

    if (g_failures != 0u) {
        (void)fprintf(
            stderr,
            "%u M2b scene telemetry test(s) failed\n",
            g_failures);
        return 1;
    }
    (void)puts("m2b_scene_telemetry_tests: all checks passed");
    return 0;
}
