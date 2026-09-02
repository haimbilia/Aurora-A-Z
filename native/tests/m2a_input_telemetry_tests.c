#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <auroraaz/m2a_input_telemetry.h>

static int failures = 0;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

static uint16_t read_be16(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8u) |
        (uint16_t)bytes[1]);
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
    write_be32(
        record + AZ_M2A_INPUT_TELEMETRY_OFF_CRC32,
        test_crc32(
            record,
            (size_t)AZ_M2A_INPUT_TELEMETRY_CRC32_PREFIX_SIZE));
}

static AzInputDetourObservation make_observation(
    AzInputControl control,
    AzInputEvent event,
    AzSelectorCommand command,
    uint16_t virtual_key,
    uint16_t flags)
{
    AzInputDetourObservation observation;

    memset(&observation, 0, sizeof(observation));
    observation.serial = 0x01020304u;
    observation.input_frame = 0x11223344u;
    observation.caller_return_address = 0x822113F8u;
    observation.keystroke.virtual_key = virtual_key;
    observation.keystroke.unicode = 0x3456u;
    observation.keystroke.flags = flags;
    observation.keystroke.user_index = 2u;
    observation.keystroke.hid_code = 3u;
    observation.translation.control = control;
    observation.translation.event = event;
    observation.translation.command = command;
    observation.coverflow_active = 1u;
    observation.would_handle = 0u;
    observation.consumed = 0u;
    observation.filter_queued = 0u;
    observation.requested_stage = AZ_INPUT_DETOUR_OBSERVE;
    observation.effective_stage = AZ_INPUT_STAGE_OBSERVE_ONLY;
    return observation;
}

static uint32_t take_snapshot(
    AzM2aInputTelemetry *telemetry,
    uint8_t *record)
{
    uint32_t token = 0u;

    CHECK(az_m2a_input_telemetry_snapshot_be(
        telemetry,
        record,
        AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE,
        &token) == AZ_M2A_INPUT_TELEMETRY_OK);
    return token;
}

static void acknowledge_baseline(AzM2aInputTelemetry *telemetry)
{
    uint8_t record[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    const uint32_t token = take_snapshot(telemetry, record);

    CHECK(az_m2a_input_telemetry_acknowledge(telemetry, token) == 1u);
}

static void test_baseline_and_irrelevant_inputs(void)
{
    AzM2aInputTelemetry telemetry;
    AzInputDetourObservation observation = make_observation(
        AZ_INPUT_CONTROL_UNKNOWN,
        AZ_INPUT_EVENT_PRESS,
        AZ_COMMAND_NONE,
        0x7777u,
        AZ_KEYSTROKE_KEYDOWN);
    uint8_t record[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint32_t generation = 99u;
    uint32_t token;

    az_m2a_input_telemetry_init(&telemetry);
    CHECK(az_m2a_input_telemetry_is_dirty(&telemetry) == 1u);
    token = take_snapshot(&telemetry, record);
    CHECK(memcmp(record, "AZI2", 4u) == 0);
    CHECK(read_be16(record + AZ_M2A_INPUT_TELEMETRY_OFF_VERSION) ==
        AZ_M2A_INPUT_TELEMETRY_VERSION);
    CHECK(read_be16(record + AZ_M2A_INPUT_TELEMETRY_OFF_RECORD_SIZE) ==
        AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_GENERATION) == 1u);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_RELEVANT_COUNT) ==
        0u);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_OBSERVATION_DROPS) ==
        0u);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_RUNTIME_STATE) ==
        (uint32_t)AZ_REV1655_RUNTIME_STOPPED);
    CHECK(record[AZ_M2A_INPUT_TELEMETRY_OFF_WORKER_ENTERED] == 0u);
    CHECK(record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_COMMAND] ==
        (uint8_t)AZ_COMMAND_NONE);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_CRC32) ==
        test_crc32(
            record,
            (size_t)AZ_M2A_INPUT_TELEMETRY_CRC32_PREFIX_SIZE));
    /* Independent zlib/IEEE CRC-32 value for the canonical zero baseline. */
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_CRC32) ==
        0x68735095u);
    CHECK(az_m2a_input_telemetry_validate_record_be(
        record,
        sizeof(record),
        &generation) == AZ_M2A_INPUT_TELEMETRY_OK);
    CHECK(generation == 1u);
    CHECK(az_m2a_input_telemetry_acknowledge(&telemetry, token) == 1u);

    CHECK(az_m2a_input_telemetry_record(&telemetry, &observation) ==
        AZ_M2A_INPUT_TELEMETRY_IGNORED);
    CHECK(az_m2a_input_telemetry_is_dirty(&telemetry) == 0u);
    CHECK(az_m2a_input_telemetry_snapshot_be(
        &telemetry,
        record,
        sizeof(record),
        &token) == AZ_M2A_INPUT_TELEMETRY_NO_CHANGE);
}

static void test_runtime_context_and_retry_generation(void)
{
    AzM2aInputTelemetry telemetry;
    uint8_t first[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint8_t retry[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint32_t first_token;
    uint32_t retry_token;

    az_m2a_input_telemetry_init(&telemetry);
    first_token = take_snapshot(&telemetry, first);
    retry_token = take_snapshot(&telemetry, retry);
    CHECK(first_token == retry_token);
    CHECK(memcmp(first, retry, sizeof(first)) == 0);
    CHECK(az_m2a_input_telemetry_acknowledge(
        &telemetry,
        first_token) == 1u);

    CHECK(az_m2a_input_telemetry_update_runtime(
        &telemetry,
        7u,
        AZ_REV1655_RUNTIME_RUNNING,
        11u) == AZ_M2A_INPUT_TELEMETRY_OK);
    first_token = take_snapshot(&telemetry, first);
    CHECK(read_be32(first + AZ_M2A_INPUT_TELEMETRY_OFF_GENERATION) == 2u);
    CHECK(first[AZ_M2A_INPUT_TELEMETRY_OFF_WORKER_ENTERED] == 1u);
    CHECK(read_be32(first + AZ_M2A_INPUT_TELEMETRY_OFF_RUNTIME_STATE) ==
        (uint32_t)AZ_REV1655_RUNTIME_RUNNING);
    CHECK(read_be32(first + AZ_M2A_INPUT_TELEMETRY_OFF_OBSERVATION_DROPS) ==
        11u);
    CHECK(az_m2a_input_telemetry_acknowledge(
        &telemetry,
        first_token) == 1u);
    CHECK(az_m2a_input_telemetry_update_runtime(
        &telemetry,
        1u,
        AZ_REV1655_RUNTIME_RUNNING,
        11u) == AZ_M2A_INPUT_TELEMETRY_NO_CHANGE);
    CHECK(az_m2a_input_telemetry_is_dirty(&telemetry) == 0u);
    CHECK(az_m2a_input_telemetry_update_runtime(
        &telemetry,
        1u,
        (AzRev1655RuntimeState)99,
        11u) == AZ_M2A_INPUT_TELEMETRY_INVALID_ARGUMENT);
    CHECK(az_m2a_input_telemetry_is_dirty(&telemetry) == 0u);
}

static void test_duplicate_and_repeat_accumulation(void)
{
    AzM2aInputTelemetry telemetry;
    AzInputDetourObservation observation = make_observation(
        AZ_INPUT_CONTROL_DPAD_RIGHT,
        AZ_INPUT_EVENT_PRESS,
        AZ_COMMAND_NEXT,
        AZ_VK_PAD_DPAD_RIGHT,
        AZ_KEYSTROKE_KEYDOWN);
    uint8_t record[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    const uint32_t bit = 1u << AZ_M2A_INPUT_SLOT_DPAD_RIGHT;

    az_m2a_input_telemetry_init(&telemetry);
    acknowledge_baseline(&telemetry);
    CHECK(az_m2a_input_telemetry_record(&telemetry, &observation) ==
        AZ_M2A_INPUT_TELEMETRY_OK);
    CHECK(az_m2a_input_telemetry_record(&telemetry, &observation) ==
        AZ_M2A_INPUT_TELEMETRY_OK);
    observation.translation.event = AZ_INPUT_EVENT_REPEAT;
    observation.keystroke.flags =
        (uint16_t)(AZ_KEYSTROKE_KEYDOWN | AZ_KEYSTROKE_REPEAT);
    CHECK(az_m2a_input_telemetry_record(&telemetry, &observation) ==
        AZ_M2A_INPUT_TELEMETRY_OK);
    CHECK(az_m2a_input_telemetry_record(&telemetry, &observation) ==
        AZ_M2A_INPUT_TELEMETRY_OK);
    (void)take_snapshot(&telemetry, record);

    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_RELEVANT_COUNT) ==
        4u);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_PRESS_MASK) == bit);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_REPEAT_MASK) == bit);
    CHECK(read_be16(record + AZ_M2A_INPUT_TELEMETRY_COUNTER_OFFSET(
        AZ_M2A_INPUT_SLOT_DPAD_RIGHT,
        AZ_M2A_INPUT_EVENT_SLOT_PRESS)) == 2u);
    CHECK(read_be16(record + AZ_M2A_INPUT_TELEMETRY_COUNTER_OFFSET(
        AZ_M2A_INPUT_SLOT_DPAD_RIGHT,
        AZ_M2A_INPUT_EVENT_SLOT_REPEAT)) == 2u);
    CHECK(record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_COMMAND] ==
        (uint8_t)AZ_COMMAND_NEXT);
}

static void test_all_seven_controls_and_events(void)
{
    static const AzInputControl controls[] = {
        AZ_INPUT_CONTROL_A,
        AZ_INPUT_CONTROL_RB,
        AZ_INPUT_CONTROL_R3,
        AZ_INPUT_CONTROL_DPAD_LEFT,
        AZ_INPUT_CONTROL_DPAD_RIGHT,
        AZ_INPUT_CONTROL_LSTICK_LEFT,
        AZ_INPUT_CONTROL_LSTICK_RIGHT
    };
    static const uint16_t virtual_keys[] = {
        AZ_VK_PAD_A,
        AZ_VK_PAD_RSHOULDER,
        AZ_VK_PAD_RTHUMB_PRESS,
        AZ_VK_PAD_DPAD_LEFT,
        AZ_VK_PAD_DPAD_RIGHT,
        AZ_VK_PAD_LTHUMB_LEFT,
        AZ_VK_PAD_LTHUMB_RIGHT
    };
    static const AzInputEvent events[] = {
        AZ_INPUT_EVENT_PRESS,
        AZ_INPUT_EVENT_REPEAT,
        AZ_INPUT_EVENT_RELEASE
    };
    AzM2aInputTelemetry telemetry;
    uint8_t record[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint32_t generation = 0u;
    uint32_t control;
    uint32_t event;

    az_m2a_input_telemetry_init(&telemetry);
    acknowledge_baseline(&telemetry);
    for (control = 0u;
         control < AZ_M2A_INPUT_TELEMETRY_CONTROL_COUNT;
         ++control) {
        for (event = 0u;
             event < AZ_M2A_INPUT_TELEMETRY_EVENT_COUNT;
             ++event) {
            AzInputDetourObservation observation = make_observation(
                controls[control],
                events[event],
                control == AZ_M2A_INPUT_SLOT_LSTICK_RIGHT ?
                    AZ_COMMAND_NEXT : AZ_COMMAND_NONE,
                virtual_keys[control],
                events[event] == AZ_INPUT_EVENT_RELEASE ?
                    AZ_KEYSTROKE_KEYUP : AZ_KEYSTROKE_KEYDOWN);
            observation.serial = control * 10u + event;
            CHECK(az_m2a_input_telemetry_record(
                &telemetry,
                &observation) == AZ_M2A_INPUT_TELEMETRY_OK);
        }
    }

    (void)take_snapshot(&telemetry, record);
    CHECK(az_m2a_input_telemetry_validate_record_be(
        record,
        sizeof(record),
        &generation) == AZ_M2A_INPUT_TELEMETRY_OK);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_RELEVANT_COUNT) ==
        21u);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_SEEN_MASK) ==
        AZ_M2A_INPUT_TELEMETRY_ALL_CONTROLS_MASK);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_PRESS_MASK) ==
        AZ_M2A_INPUT_TELEMETRY_ALL_CONTROLS_MASK);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_REPEAT_MASK) ==
        AZ_M2A_INPUT_TELEMETRY_ALL_CONTROLS_MASK);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_RELEASE_MASK) ==
        AZ_M2A_INPUT_TELEMETRY_ALL_CONTROLS_MASK);

    for (control = 0u;
         control < AZ_M2A_INPUT_TELEMETRY_CONTROL_COUNT;
         ++control) {
        for (event = 0u;
             event < AZ_M2A_INPUT_TELEMETRY_EVENT_COUNT;
             ++event) {
            CHECK(read_be16(record + AZ_M2A_INPUT_TELEMETRY_COUNTER_OFFSET(
                control,
                event)) == 1u);
        }
    }

    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_LAST_CALLER) ==
        0x822113F8u);
    CHECK(read_be16(record + AZ_M2A_INPUT_TELEMETRY_OFF_LAST_VK) ==
        AZ_VK_PAD_LTHUMB_RIGHT);
    CHECK(read_be16(record + AZ_M2A_INPUT_TELEMETRY_OFF_LAST_FLAGS) ==
        AZ_KEYSTROKE_KEYUP);
    CHECK(read_be16(record + AZ_M2A_INPUT_TELEMETRY_OFF_LAST_UNICODE) ==
        0x3456u);
    CHECK(record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_CONTROL] ==
        (uint8_t)AZ_INPUT_CONTROL_LSTICK_RIGHT);
    CHECK(record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_EVENT] ==
        (uint8_t)AZ_INPUT_EVENT_RELEASE);
    CHECK(record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_COMMAND] ==
        (uint8_t)AZ_COMMAND_NEXT);
}

static void test_rb_and_safety_flags(void)
{
    AzM2aInputTelemetry telemetry;
    AzInputDetourObservation rb = make_observation(
        AZ_INPUT_CONTROL_RB,
        AZ_INPUT_EVENT_PRESS,
        AZ_COMMAND_NONE,
        AZ_VK_PAD_RSHOULDER,
        AZ_KEYSTROKE_KEYDOWN);
    AzInputDetourObservation before = rb;
    AzInputDetourObservation violation = make_observation(
        AZ_INPUT_CONTROL_A,
        AZ_INPUT_EVENT_PRESS,
        AZ_COMMAND_APPLY,
        AZ_VK_PAD_A,
        AZ_KEYSTROKE_KEYDOWN);
    uint8_t record[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];

    az_m2a_input_telemetry_init(&telemetry);
    acknowledge_baseline(&telemetry);
    CHECK(az_m2a_input_telemetry_record(&telemetry, &rb) ==
        AZ_M2A_INPUT_TELEMETRY_OK);
    CHECK(memcmp(&rb, &before, sizeof(rb)) == 0);
    (void)take_snapshot(&telemetry, record);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_SEEN_MASK) ==
        (1u << AZ_M2A_INPUT_SLOT_RB));
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_SAFETY_FLAGS) ==
        0u);

    violation.consumed = 1u;
    violation.filter_queued = 1u;
    violation.requested_stage = AZ_INPUT_DETOUR_CONSUME;
    violation.effective_stage = AZ_INPUT_STAGE_CONSUME_VERIFIED;
    CHECK(az_m2a_input_telemetry_record(&telemetry, &violation) ==
        AZ_M2A_INPUT_TELEMETRY_OK);
    (void)take_snapshot(&telemetry, record);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_CONSUMED_MASK) ==
        (1u << AZ_M2A_INPUT_SLOT_A));
    CHECK(read_be32(
        record + AZ_M2A_INPUT_TELEMETRY_OFF_FILTER_QUEUED_MASK) ==
        (1u << AZ_M2A_INPUT_SLOT_A));
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_SAFETY_FLAGS) ==
        AZ_M2A_INPUT_TELEMETRY_ALL_SAFETY_FLAGS);
}

static void test_dirty_acknowledge_semantics(void)
{
    AzM2aInputTelemetry telemetry;
    AzInputDetourObservation observation = make_observation(
        AZ_INPUT_CONTROL_R3,
        AZ_INPUT_EVENT_PRESS,
        AZ_COMMAND_ENTER,
        AZ_VK_PAD_RTHUMB_PRESS,
        AZ_KEYSTROKE_KEYDOWN);
    uint8_t first[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint8_t second[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint8_t retry[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint32_t first_token;
    uint32_t second_token;

    az_m2a_input_telemetry_init(&telemetry);
    first_token = take_snapshot(&telemetry, first);
    CHECK(az_m2a_input_telemetry_acknowledge(
        &telemetry,
        first_token + 1u) == 0u);
    CHECK(az_m2a_input_telemetry_is_dirty(&telemetry) == 1u);
    CHECK(az_m2a_input_telemetry_acknowledge(
        &telemetry,
        first_token) == 1u);

    CHECK(az_m2a_input_telemetry_record(&telemetry, &observation) ==
        AZ_M2A_INPUT_TELEMETRY_OK);
    first_token = take_snapshot(&telemetry, first);
    CHECK(read_be32(first + AZ_M2A_INPUT_TELEMETRY_OFF_GENERATION) == 2u);
    observation.serial += 1u;
    CHECK(az_m2a_input_telemetry_record(&telemetry, &observation) ==
        AZ_M2A_INPUT_TELEMETRY_OK);
    CHECK(az_m2a_input_telemetry_acknowledge(
        &telemetry,
        first_token) == 0u);
    second_token = take_snapshot(&telemetry, second);
    CHECK(read_be32(second + AZ_M2A_INPUT_TELEMETRY_OFF_GENERATION) == 3u);
    CHECK(second_token != first_token);
    CHECK(take_snapshot(&telemetry, retry) == second_token);
    CHECK(memcmp(second, retry, sizeof(second)) == 0);
    CHECK(az_m2a_input_telemetry_acknowledge(
        &telemetry,
        second_token) == 1u);
    CHECK(az_m2a_input_telemetry_is_dirty(&telemetry) == 0u);
}

static void test_saturating_counters_and_generation_wrap(void)
{
    AzM2aInputTelemetry telemetry;
    AzInputDetourObservation valid = make_observation(
        AZ_INPUT_CONTROL_A,
        AZ_INPUT_EVENT_PRESS,
        AZ_COMMAND_APPLY,
        AZ_VK_PAD_A,
        AZ_KEYSTROKE_KEYDOWN);
    AzInputDetourObservation invalid = valid;
    uint8_t record[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint32_t generation = 1u;

    az_m2a_input_telemetry_init(&telemetry);
    acknowledge_baseline(&telemetry);
    telemetry.relevant_observations = UINT32_MAX;
    telemetry.invalid_event_count = UINT32_MAX;
    telemetry.counters[AZ_M2A_INPUT_SLOT_A]
        [AZ_M2A_INPUT_EVENT_SLOT_PRESS] = UINT16_MAX;
    telemetry.observation_drops = UINT32_MAX;
    telemetry.generation = UINT32_MAX;
    telemetry.revision = UINT32_MAX;
    invalid.translation.event = AZ_INPUT_EVENT_INVALID;
    CHECK(az_m2a_input_telemetry_record(&telemetry, &valid) ==
        AZ_M2A_INPUT_TELEMETRY_OK);
    CHECK(az_m2a_input_telemetry_record(&telemetry, &invalid) ==
        AZ_M2A_INPUT_TELEMETRY_OK);
    CHECK(take_snapshot(&telemetry, record) != 0u);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_GENERATION) == 0u);
    CHECK(read_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_RELEVANT_COUNT) ==
        UINT32_MAX);
    CHECK(read_be32(
        record + AZ_M2A_INPUT_TELEMETRY_OFF_INVALID_EVENT_COUNT) ==
        UINT32_MAX);
    CHECK(read_be32(
        record + AZ_M2A_INPUT_TELEMETRY_OFF_OBSERVATION_DROPS) ==
        UINT32_MAX);
    CHECK(read_be16(record + AZ_M2A_INPUT_TELEMETRY_COUNTER_OFFSET(
        AZ_M2A_INPUT_SLOT_A,
        AZ_M2A_INPUT_EVENT_SLOT_PRESS)) == UINT16_MAX);
    CHECK(az_m2a_input_telemetry_validate_record_be(
        record,
        sizeof(record),
        &generation) == AZ_M2A_INPUT_TELEMETRY_OK);
    CHECK(generation == 0u);
}

static void test_torn_corrupt_and_semantic_rejection(void)
{
    AzM2aInputTelemetry telemetry;
    uint8_t valid[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint8_t damaged[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint32_t generation = 0u;

    az_m2a_input_telemetry_init(&telemetry);
    (void)take_snapshot(&telemetry, valid);
    CHECK(az_m2a_input_telemetry_validate_record_be(
        valid,
        sizeof(valid) - 1u,
        &generation) == AZ_M2A_INPUT_TELEMETRY_BUFFER_TOO_SMALL);

    memset(damaged, 0, sizeof(damaged));
    memcpy(damaged, valid, 100u);
    CHECK(az_m2a_input_telemetry_validate_record_be(
        damaged,
        sizeof(damaged),
        &generation) == AZ_M2A_INPUT_TELEMETRY_INVALID_RECORD);

    memcpy(damaged, valid, sizeof(damaged));
    damaged[AZ_M2A_INPUT_TELEMETRY_OFF_RELEVANT_COUNT] ^= 1u;
    CHECK(az_m2a_input_telemetry_validate_record_be(
        damaged,
        sizeof(damaged),
        &generation) == AZ_M2A_INPUT_TELEMETRY_INVALID_RECORD);

    memcpy(damaged, valid, sizeof(damaged));
    damaged[AZ_M2A_INPUT_TELEMETRY_OFF_CRC32 + 3u] ^= 1u;
    CHECK(az_m2a_input_telemetry_validate_record_be(
        damaged,
        sizeof(damaged),
        &generation) == AZ_M2A_INPUT_TELEMETRY_INVALID_RECORD);

    memcpy(damaged, valid, sizeof(damaged));
    damaged[132] = 1u;
    repair_crc(damaged);
    CHECK(az_m2a_input_telemetry_validate_record_be(
        damaged,
        sizeof(damaged),
        &generation) == AZ_M2A_INPUT_TELEMETRY_INVALID_RECORD);

    memcpy(damaged, valid, sizeof(damaged));
    damaged[AZ_M2A_INPUT_TELEMETRY_OFF_WORKER_ENTERED] = 2u;
    repair_crc(damaged);
    CHECK(az_m2a_input_telemetry_validate_record_be(
        damaged,
        sizeof(damaged),
        &generation) == AZ_M2A_INPUT_TELEMETRY_INVALID_RECORD);

    memcpy(damaged, valid, sizeof(damaged));
    write_be32(
        damaged + AZ_M2A_INPUT_TELEMETRY_OFF_SEEN_MASK,
        0x80000000u);
    repair_crc(damaged);
    CHECK(az_m2a_input_telemetry_validate_record_be(
        damaged,
        sizeof(damaged),
        &generation) == AZ_M2A_INPUT_TELEMETRY_INVALID_RECORD);
}

static void test_dual_slot_selection_and_wrap(void)
{
    AzM2aInputTelemetry telemetry;
    AzInputDetourObservation observation = make_observation(
        AZ_INPUT_CONTROL_R3,
        AZ_INPUT_EVENT_PRESS,
        AZ_COMMAND_ENTER,
        AZ_VK_PAD_RTHUMB_PRESS,
        AZ_KEYSTROKE_KEYDOWN);
    uint8_t slot_a[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint8_t slot_b[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint8_t selected = 99u;
    uint32_t generation = 99u;
    uint32_t token;

    az_m2a_input_telemetry_init(&telemetry);
    token = take_snapshot(&telemetry, slot_a);
    CHECK(az_m2a_input_telemetry_acknowledge(&telemetry, token) == 1u);
    CHECK(az_m2a_input_telemetry_record(&telemetry, &observation) ==
        AZ_M2A_INPUT_TELEMETRY_OK);
    token = take_snapshot(&telemetry, slot_b);
    CHECK(az_m2a_input_telemetry_acknowledge(&telemetry, token) == 1u);
    CHECK(az_m2a_input_telemetry_select_newest_be(
        slot_a,
        sizeof(slot_a),
        slot_b,
        sizeof(slot_b),
        &selected,
        &generation) == AZ_M2A_INPUT_TELEMETRY_OK);
    CHECK(selected == AZ_M2A_INPUT_TELEMETRY_SLOT_B);
    CHECK(generation == 2u);

    slot_b[70] ^= 1u;
    CHECK(az_m2a_input_telemetry_select_newest_be(
        slot_a,
        sizeof(slot_a),
        slot_b,
        sizeof(slot_b),
        &selected,
        &generation) == AZ_M2A_INPUT_TELEMETRY_OK);
    CHECK(selected == AZ_M2A_INPUT_TELEMETRY_SLOT_A);
    CHECK(generation == 1u);
    slot_a[71] ^= 1u;
    CHECK(az_m2a_input_telemetry_select_newest_be(
        slot_a,
        sizeof(slot_a),
        slot_b,
        sizeof(slot_b),
        &selected,
        &generation) == AZ_M2A_INPUT_TELEMETRY_INVALID_RECORD);

    az_m2a_input_telemetry_init(&telemetry);
    telemetry.generation = UINT32_MAX - 1u;
    token = take_snapshot(&telemetry, slot_a);
    CHECK(read_be32(slot_a + AZ_M2A_INPUT_TELEMETRY_OFF_GENERATION) ==
        UINT32_MAX);
    CHECK(az_m2a_input_telemetry_acknowledge(&telemetry, token) == 1u);
    CHECK(az_m2a_input_telemetry_record(&telemetry, &observation) ==
        AZ_M2A_INPUT_TELEMETRY_OK);
    (void)take_snapshot(&telemetry, slot_b);
    CHECK(read_be32(slot_b + AZ_M2A_INPUT_TELEMETRY_OFF_GENERATION) == 0u);
    CHECK(az_m2a_input_telemetry_select_newest_be(
        slot_a,
        sizeof(slot_a),
        slot_b,
        sizeof(slot_b),
        &selected,
        &generation) == AZ_M2A_INPUT_TELEMETRY_OK);
    CHECK(selected == AZ_M2A_INPUT_TELEMETRY_SLOT_B);
    CHECK(generation == 0u);

    CHECK(az_m2a_input_telemetry_select_newest_be(
        NULL,
        0u,
        slot_b,
        sizeof(slot_b),
        &selected,
        &generation) == AZ_M2A_INPUT_TELEMETRY_OK);
    CHECK(selected == AZ_M2A_INPUT_TELEMETRY_SLOT_B);
    memcpy(slot_a, slot_b, sizeof(slot_a));
    CHECK(az_m2a_input_telemetry_select_newest_be(
        slot_a,
        sizeof(slot_a),
        slot_b,
        sizeof(slot_b),
        &selected,
        &generation) == AZ_M2A_INPUT_TELEMETRY_OK);
    CHECK(selected == AZ_M2A_INPUT_TELEMETRY_SLOT_A);
}

static void test_argument_guards(void)
{
    AzM2aInputTelemetry telemetry;
    AzInputDetourObservation observation = make_observation(
        AZ_INPUT_CONTROL_A,
        AZ_INPUT_EVENT_PRESS,
        AZ_COMMAND_APPLY,
        AZ_VK_PAD_A,
        AZ_KEYSTROKE_KEYDOWN);
    uint8_t record[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint32_t token = 0u;
    uint8_t slot = 0u;

    az_m2a_input_telemetry_init(&telemetry);
    az_m2a_input_telemetry_init(NULL);
    CHECK(az_m2a_input_telemetry_record(NULL, &observation) ==
        AZ_M2A_INPUT_TELEMETRY_NULL);
    CHECK(az_m2a_input_telemetry_record(&telemetry, NULL) ==
        AZ_M2A_INPUT_TELEMETRY_NULL);
    CHECK(az_m2a_input_telemetry_update_runtime(
        NULL,
        1u,
        AZ_REV1655_RUNTIME_RUNNING,
        0u) == AZ_M2A_INPUT_TELEMETRY_NULL);
    CHECK(az_m2a_input_telemetry_snapshot_be(
        &telemetry,
        record,
        AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE - 1u,
        &token) == AZ_M2A_INPUT_TELEMETRY_BUFFER_TOO_SMALL);
    CHECK(telemetry.generation == 0u);
    CHECK(az_m2a_input_telemetry_snapshot_be(
        NULL,
        record,
        sizeof(record),
        &token) == AZ_M2A_INPUT_TELEMETRY_NULL);
    CHECK(az_m2a_input_telemetry_validate_record_be(
        NULL,
        sizeof(record),
        &token) == AZ_M2A_INPUT_TELEMETRY_NULL);
    CHECK(az_m2a_input_telemetry_select_newest_be(
        record,
        sizeof(record),
        record,
        sizeof(record),
        NULL,
        &token) == AZ_M2A_INPUT_TELEMETRY_NULL);
    CHECK(az_m2a_input_telemetry_select_newest_be(
        NULL,
        0u,
        NULL,
        0u,
        &slot,
        &token) == AZ_M2A_INPUT_TELEMETRY_INVALID_RECORD);
    CHECK(az_m2a_input_telemetry_acknowledge(NULL, token) == 0u);
    CHECK(az_m2a_input_telemetry_is_dirty(NULL) == 0u);
}

int main(void)
{
    test_baseline_and_irrelevant_inputs();
    test_runtime_context_and_retry_generation();
    test_duplicate_and_repeat_accumulation();
    test_all_seven_controls_and_events();
    test_rb_and_safety_flags();
    test_dirty_acknowledge_semantics();
    test_saturating_counters_and_generation_wrap();
    test_torn_corrupt_and_semantic_rejection();
    test_dual_slot_selection_and_wrap();
    test_argument_guards();

    if (failures != 0) {
        fprintf(stderr, "%d M2a input telemetry test(s) failed\n", failures);
        return 1;
    }
    puts("M2a input telemetry tests passed");
    return 0;
}
