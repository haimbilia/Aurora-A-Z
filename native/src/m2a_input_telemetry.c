#include <auroraaz/m2a_input_telemetry.h>

#include <limits.h>

typedef char AzM2aInputTelemetryControlCountMustBeSeven[
    AZ_M2A_INPUT_TELEMETRY_CONTROL_COUNT == 7u ? 1 : -1];
typedef char AzM2aInputTelemetryCountersMustEndAt90[
    AZ_M2A_INPUT_TELEMETRY_COUNTER_OFFSET(6u, 2u) + 2u == 90u ? 1 : -1];
typedef char AzM2aInputTelemetryRecordLayoutMustFit[
    AZ_M2A_INPUT_TELEMETRY_OFF_CRC32 + 4u ==
        AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE ? 1 : -1];

static void zero_bytes(uint8_t *bytes, size_t size)
{
    size_t index;

    for (index = 0u; index < size; ++index) {
        bytes[index] = 0u;
    }
}

static uint8_t bytes_are_zero(
    const uint8_t *bytes,
    size_t begin,
    size_t end)
{
    size_t index;

    for (index = begin; index < end; ++index) {
        if (bytes[index] != 0u) {
            return 0u;
        }
    }
    return 1u;
}

static uint32_t increment_saturated_u32(uint32_t value)
{
    return value == UINT32_MAX ? UINT32_MAX : value + 1u;
}

static uint16_t increment_saturated_u16(uint16_t value)
{
    return value == UINT16_MAX ? UINT16_MAX : (uint16_t)(value + 1u);
}

static void bump_revision(AzM2aInputTelemetry *telemetry)
{
    ++telemetry->revision;
    if (telemetry->revision == 0u) {
        telemetry->revision = 1u;
    }
    telemetry->generation_assigned = 0u;
    telemetry->dirty = 1u;
}

static uint8_t control_slot(
    AzInputControl control,
    AzM2aInputTelemetryControlSlot *slot)
{
    if (slot == NULL) {
        return 0u;
    }

    switch (control) {
    case AZ_INPUT_CONTROL_A:
        *slot = AZ_M2A_INPUT_SLOT_A;
        return 1u;
    case AZ_INPUT_CONTROL_RB:
        *slot = AZ_M2A_INPUT_SLOT_RB;
        return 1u;
    case AZ_INPUT_CONTROL_R3:
        *slot = AZ_M2A_INPUT_SLOT_R3;
        return 1u;
    case AZ_INPUT_CONTROL_DPAD_LEFT:
        *slot = AZ_M2A_INPUT_SLOT_DPAD_LEFT;
        return 1u;
    case AZ_INPUT_CONTROL_DPAD_RIGHT:
        *slot = AZ_M2A_INPUT_SLOT_DPAD_RIGHT;
        return 1u;
    case AZ_INPUT_CONTROL_LSTICK_LEFT:
        *slot = AZ_M2A_INPUT_SLOT_LSTICK_LEFT;
        return 1u;
    case AZ_INPUT_CONTROL_LSTICK_RIGHT:
        *slot = AZ_M2A_INPUT_SLOT_LSTICK_RIGHT;
        return 1u;
    case AZ_INPUT_CONTROL_UNKNOWN:
    default:
        return 0u;
    }
}

static uint8_t event_slot(
    AzInputEvent event,
    AzM2aInputTelemetryEventSlot *slot)
{
    if (slot == NULL) {
        return 0u;
    }

    switch (event) {
    case AZ_INPUT_EVENT_PRESS:
        *slot = AZ_M2A_INPUT_EVENT_SLOT_PRESS;
        return 1u;
    case AZ_INPUT_EVENT_REPEAT:
        *slot = AZ_M2A_INPUT_EVENT_SLOT_REPEAT;
        return 1u;
    case AZ_INPUT_EVENT_RELEASE:
        *slot = AZ_M2A_INPUT_EVENT_SLOT_RELEASE;
        return 1u;
    case AZ_INPUT_EVENT_INVALID:
    default:
        return 0u;
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
            const uint32_t low_bit_mask =
                (uint32_t)(0u - (crc & 1u));
            crc = (crc >> 1u) ^ (0xEDB88320u & low_bit_mask);
        }
    }
    return ~crc;
}

static uint8_t generation_is_newer(
    uint32_t candidate,
    uint32_t reference)
{
    const uint32_t distance = candidate - reference;

    return distance != 0u && distance < 0x80000000u ? 1u : 0u;
}

void az_m2a_input_telemetry_init(AzM2aInputTelemetry *telemetry)
{
    uint32_t control;
    uint32_t event;

    if (telemetry == NULL) {
        return;
    }

    telemetry->revision = 1u;
    telemetry->generation = 0u;
    telemetry->relevant_observations = 0u;
    telemetry->seen_mask = 0u;
    telemetry->press_mask = 0u;
    telemetry->repeat_mask = 0u;
    telemetry->release_mask = 0u;
    telemetry->consumed_mask = 0u;
    telemetry->filter_queued_mask = 0u;
    telemetry->safety_flags = 0u;
    telemetry->invalid_event_count = 0u;
    telemetry->observation_drops = 0u;
    telemetry->runtime_state = AZ_REV1655_RUNTIME_STOPPED;
    for (control = 0u;
         control < AZ_M2A_INPUT_TELEMETRY_CONTROL_COUNT;
         ++control) {
        for (event = 0u;
             event < AZ_M2A_INPUT_TELEMETRY_EVENT_COUNT;
             ++event) {
            telemetry->counters[control][event] = 0u;
        }
    }
    telemetry->last_serial = 0u;
    telemetry->last_input_frame = 0u;
    telemetry->last_caller_return_address = 0u;
    telemetry->last_virtual_key = 0u;
    telemetry->last_flags = 0u;
    telemetry->last_unicode = 0u;
    telemetry->worker_entered = 0u;
    telemetry->last_command = (uint8_t)AZ_COMMAND_NONE;
    telemetry->last_control = (uint8_t)AZ_INPUT_CONTROL_UNKNOWN;
    telemetry->last_event = (uint8_t)AZ_INPUT_EVENT_INVALID;
    telemetry->last_requested_stage = (uint8_t)AZ_INPUT_DETOUR_OFF;
    telemetry->last_effective_stage =
        (uint8_t)AZ_INPUT_STAGE_OBSERVE_ONLY;
    telemetry->last_consumed = 0u;
    telemetry->last_filter_queued = 0u;
    telemetry->last_would_handle = 0u;
    telemetry->last_coverflow_active = 0u;
    telemetry->last_user_index = 0u;
    telemetry->last_hid_code = 0u;
    telemetry->generation_assigned = 0u;
    telemetry->dirty = 1u;
}

AzM2aInputTelemetryResult az_m2a_input_telemetry_record(
    AzM2aInputTelemetry *telemetry,
    const AzInputDetourObservation *observation)
{
    AzM2aInputTelemetryControlSlot control;
    AzM2aInputTelemetryEventSlot event;
    uint32_t control_mask;

    if (telemetry == NULL || observation == NULL) {
        return AZ_M2A_INPUT_TELEMETRY_NULL;
    }
    if (!control_slot(observation->translation.control, &control)) {
        return AZ_M2A_INPUT_TELEMETRY_IGNORED;
    }

    control_mask = 1u << (uint32_t)control;
    telemetry->relevant_observations = increment_saturated_u32(
        telemetry->relevant_observations);
    telemetry->seen_mask |= control_mask;

    if (event_slot(observation->translation.event, &event)) {
        telemetry->counters[(uint32_t)control][(uint32_t)event] =
            increment_saturated_u16(
                telemetry->counters[(uint32_t)control][(uint32_t)event]);
        if (event == AZ_M2A_INPUT_EVENT_SLOT_PRESS) {
            telemetry->press_mask |= control_mask;
        } else if (event == AZ_M2A_INPUT_EVENT_SLOT_REPEAT) {
            telemetry->repeat_mask |= control_mask;
        } else {
            telemetry->release_mask |= control_mask;
        }
    } else {
        telemetry->invalid_event_count = increment_saturated_u32(
            telemetry->invalid_event_count);
    }

    if (observation->consumed != 0u) {
        telemetry->consumed_mask |= control_mask;
        telemetry->safety_flags |= AZ_M2A_INPUT_SAFETY_CONSUMED;
    }
    if (observation->filter_queued != 0u) {
        telemetry->filter_queued_mask |= control_mask;
        telemetry->safety_flags |= AZ_M2A_INPUT_SAFETY_FILTER_QUEUED;
    }
    if (observation->requested_stage != AZ_INPUT_DETOUR_OBSERVE) {
        telemetry->safety_flags |=
            AZ_M2A_INPUT_SAFETY_REQUESTED_NOT_OBSERVE;
    }
    if (observation->effective_stage != AZ_INPUT_STAGE_OBSERVE_ONLY) {
        telemetry->safety_flags |=
            AZ_M2A_INPUT_SAFETY_EFFECTIVE_NOT_OBSERVE;
    }

    telemetry->last_serial = observation->serial;
    telemetry->last_input_frame = observation->input_frame;
    telemetry->last_caller_return_address =
        observation->caller_return_address;
    telemetry->last_virtual_key = observation->keystroke.virtual_key;
    telemetry->last_flags = observation->keystroke.flags;
    telemetry->last_unicode = observation->keystroke.unicode;
    telemetry->last_command =
        (uint8_t)observation->translation.command;
    telemetry->last_control = (uint8_t)observation->translation.control;
    telemetry->last_event = (uint8_t)observation->translation.event;
    telemetry->last_requested_stage =
        (uint8_t)observation->requested_stage;
    telemetry->last_effective_stage =
        (uint8_t)observation->effective_stage;
    telemetry->last_consumed = observation->consumed != 0u ? 1u : 0u;
    telemetry->last_filter_queued =
        observation->filter_queued != 0u ? 1u : 0u;
    telemetry->last_would_handle =
        observation->would_handle != 0u ? 1u : 0u;
    telemetry->last_coverflow_active =
        observation->coverflow_active != 0u ? 1u : 0u;
    telemetry->last_user_index = observation->keystroke.user_index;
    telemetry->last_hid_code = observation->keystroke.hid_code;
    bump_revision(telemetry);
    return AZ_M2A_INPUT_TELEMETRY_OK;
}

AzM2aInputTelemetryResult az_m2a_input_telemetry_update_runtime(
    AzM2aInputTelemetry *telemetry,
    uint8_t worker_entered,
    AzRev1655RuntimeState runtime_state,
    uint32_t observation_drops)
{
    const uint8_t normalized_worker = worker_entered != 0u ? 1u : 0u;

    if (telemetry == NULL) {
        return AZ_M2A_INPUT_TELEMETRY_NULL;
    }
    if ((uint32_t)runtime_state > (uint32_t)AZ_REV1655_RUNTIME_CLOSED) {
        return AZ_M2A_INPUT_TELEMETRY_INVALID_ARGUMENT;
    }
    if (telemetry->worker_entered == normalized_worker &&
        telemetry->runtime_state == runtime_state &&
        telemetry->observation_drops == observation_drops) {
        return AZ_M2A_INPUT_TELEMETRY_NO_CHANGE;
    }

    telemetry->worker_entered = normalized_worker;
    telemetry->runtime_state = runtime_state;
    telemetry->observation_drops = observation_drops;
    bump_revision(telemetry);
    return AZ_M2A_INPUT_TELEMETRY_OK;
}

uint8_t az_m2a_input_telemetry_is_dirty(
    const AzM2aInputTelemetry *telemetry)
{
    return telemetry != NULL && telemetry->dirty != 0u ? 1u : 0u;
}

AzM2aInputTelemetryResult az_m2a_input_telemetry_snapshot_be(
    AzM2aInputTelemetry *telemetry,
    uint8_t *record,
    size_t record_size,
    uint32_t *revision_token)
{
    uint32_t control;
    uint32_t event;

    if (telemetry == NULL || record == NULL || revision_token == NULL) {
        return AZ_M2A_INPUT_TELEMETRY_NULL;
    }
    if (record_size < (size_t)AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE) {
        return AZ_M2A_INPUT_TELEMETRY_BUFFER_TOO_SMALL;
    }
    if (telemetry->dirty == 0u) {
        return AZ_M2A_INPUT_TELEMETRY_NO_CHANGE;
    }

    if (telemetry->generation_assigned == 0u) {
        ++telemetry->generation;
        telemetry->generation_assigned = 1u;
    }

    zero_bytes(record, (size_t)AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE);
    record[0] = (uint8_t)'A';
    record[1] = (uint8_t)'Z';
    record[2] = (uint8_t)'I';
    record[3] = (uint8_t)'2';
    put_be16(
        record + AZ_M2A_INPUT_TELEMETRY_OFF_VERSION,
        (uint16_t)AZ_M2A_INPUT_TELEMETRY_VERSION);
    put_be16(
        record + AZ_M2A_INPUT_TELEMETRY_OFF_RECORD_SIZE,
        (uint16_t)AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE);
    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_GENERATION,
        telemetry->generation);
    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_RELEVANT_COUNT,
        telemetry->relevant_observations);
    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_SEEN_MASK,
        telemetry->seen_mask);
    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_PRESS_MASK,
        telemetry->press_mask);
    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_REPEAT_MASK,
        telemetry->repeat_mask);
    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_RELEASE_MASK,
        telemetry->release_mask);
    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_CONSUMED_MASK,
        telemetry->consumed_mask);
    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_FILTER_QUEUED_MASK,
        telemetry->filter_queued_mask);
    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_SAFETY_FLAGS,
        telemetry->safety_flags);
    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_INVALID_EVENT_COUNT,
        telemetry->invalid_event_count);
    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_OBSERVATION_DROPS,
        telemetry->observation_drops);
    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_RUNTIME_STATE,
        (uint32_t)telemetry->runtime_state);
    record[AZ_M2A_INPUT_TELEMETRY_OFF_WORKER_ENTERED] =
        telemetry->worker_entered;
    record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_COMMAND] =
        telemetry->last_command;

    for (control = 0u;
         control < AZ_M2A_INPUT_TELEMETRY_CONTROL_COUNT;
         ++control) {
        for (event = 0u;
             event < AZ_M2A_INPUT_TELEMETRY_EVENT_COUNT;
             ++event) {
            const uint32_t offset =
                AZ_M2A_INPUT_TELEMETRY_COUNTER_OFFSET(control, event);
            put_be16(record + offset, telemetry->counters[control][event]);
        }
    }

    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_LAST_SERIAL,
        telemetry->last_serial);
    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_LAST_INPUT_FRAME,
        telemetry->last_input_frame);
    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_LAST_CALLER,
        telemetry->last_caller_return_address);
    put_be16(record + AZ_M2A_INPUT_TELEMETRY_OFF_LAST_VK,
        telemetry->last_virtual_key);
    put_be16(record + AZ_M2A_INPUT_TELEMETRY_OFF_LAST_FLAGS,
        telemetry->last_flags);
    put_be16(record + AZ_M2A_INPUT_TELEMETRY_OFF_LAST_UNICODE,
        telemetry->last_unicode);
    record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_CONTROL] =
        telemetry->last_control;
    record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_EVENT] = telemetry->last_event;
    record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_REQUESTED_STAGE] =
        telemetry->last_requested_stage;
    record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_EFFECTIVE_STAGE] =
        telemetry->last_effective_stage;
    record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_CONSUMED] =
        telemetry->last_consumed;
    record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_FILTER_QUEUED] =
        telemetry->last_filter_queued;
    record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_WOULD_HANDLE] =
        telemetry->last_would_handle;
    record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_COVERFLOW_ACTIVE] =
        telemetry->last_coverflow_active;
    record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_USER_INDEX] =
        telemetry->last_user_index;
    record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_HID_CODE] =
        telemetry->last_hid_code;
    put_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_CRC32,
        crc32_ieee(
            record,
            (size_t)AZ_M2A_INPUT_TELEMETRY_CRC32_PREFIX_SIZE));

    *revision_token = telemetry->revision;
    return AZ_M2A_INPUT_TELEMETRY_OK;
}

uint8_t az_m2a_input_telemetry_acknowledge(
    AzM2aInputTelemetry *telemetry,
    uint32_t revision_token)
{
    if (telemetry == NULL || telemetry->dirty == 0u ||
        revision_token != telemetry->revision) {
        return 0u;
    }

    telemetry->dirty = 0u;
    return 1u;
}

AzM2aInputTelemetryResult az_m2a_input_telemetry_validate_record_be(
    const uint8_t *record,
    size_t record_size,
    uint32_t *generation)
{
    uint32_t seen_mask;
    uint32_t event_masks;
    uint32_t consumed_mask;
    uint32_t filter_queued_mask;
    uint32_t safety_flags;

    if (record == NULL || generation == NULL) {
        return AZ_M2A_INPUT_TELEMETRY_NULL;
    }
    if (record_size < (size_t)AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE) {
        return AZ_M2A_INPUT_TELEMETRY_BUFFER_TOO_SMALL;
    }
    if (record[0] != (uint8_t)'A' || record[1] != (uint8_t)'Z' ||
        record[2] != (uint8_t)'I' || record[3] != (uint8_t)'2' ||
        get_be16(record + AZ_M2A_INPUT_TELEMETRY_OFF_VERSION) !=
            (uint16_t)AZ_M2A_INPUT_TELEMETRY_VERSION ||
        get_be16(record + AZ_M2A_INPUT_TELEMETRY_OFF_RECORD_SIZE) !=
            (uint16_t)AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE ||
        get_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_CRC32) !=
            crc32_ieee(
                record,
                (size_t)AZ_M2A_INPUT_TELEMETRY_CRC32_PREFIX_SIZE)) {
        return AZ_M2A_INPUT_TELEMETRY_INVALID_RECORD;
    }
    if (!bytes_are_zero(
            record,
            128u,
            (size_t)AZ_M2A_INPUT_TELEMETRY_OFF_CRC32)) {
        return AZ_M2A_INPUT_TELEMETRY_INVALID_RECORD;
    }

    seen_mask = get_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_SEEN_MASK);
    event_masks =
        get_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_PRESS_MASK) |
        get_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_REPEAT_MASK) |
        get_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_RELEASE_MASK);
    consumed_mask = get_be32(
        record + AZ_M2A_INPUT_TELEMETRY_OFF_CONSUMED_MASK);
    filter_queued_mask = get_be32(
        record + AZ_M2A_INPUT_TELEMETRY_OFF_FILTER_QUEUED_MASK);
    safety_flags = get_be32(
        record + AZ_M2A_INPUT_TELEMETRY_OFF_SAFETY_FLAGS);
    if (((seen_mask | event_masks | consumed_mask | filter_queued_mask) &
         ~AZ_M2A_INPUT_TELEMETRY_ALL_CONTROLS_MASK) != 0u ||
        (event_masks & ~seen_mask) != 0u ||
        (consumed_mask & ~seen_mask) != 0u ||
        (filter_queued_mask & ~seen_mask) != 0u ||
        (safety_flags & ~AZ_M2A_INPUT_TELEMETRY_ALL_SAFETY_FLAGS) != 0u ||
        ((consumed_mask != 0u) !=
         ((safety_flags & AZ_M2A_INPUT_SAFETY_CONSUMED) != 0u)) ||
        ((filter_queued_mask != 0u) !=
         ((safety_flags & AZ_M2A_INPUT_SAFETY_FILTER_QUEUED) != 0u))) {
        return AZ_M2A_INPUT_TELEMETRY_INVALID_RECORD;
    }
    if (get_be32(record + AZ_M2A_INPUT_TELEMETRY_OFF_RUNTIME_STATE) >
            (uint32_t)AZ_REV1655_RUNTIME_CLOSED ||
        record[AZ_M2A_INPUT_TELEMETRY_OFF_WORKER_ENTERED] > 1u ||
        record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_COMMAND] >
            (uint8_t)AZ_COMMAND_APPLY ||
        record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_CONTROL] >
            (uint8_t)AZ_INPUT_CONTROL_LSTICK_RIGHT ||
        record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_EVENT] >
            (uint8_t)AZ_INPUT_EVENT_RELEASE ||
        record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_REQUESTED_STAGE] >
            (uint8_t)AZ_INPUT_DETOUR_CONSUME ||
        record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_EFFECTIVE_STAGE] >
            (uint8_t)AZ_INPUT_STAGE_CONSUME_VERIFIED ||
        record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_CONSUMED] > 1u ||
        record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_FILTER_QUEUED] > 1u ||
        record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_WOULD_HANDLE] > 1u ||
        record[AZ_M2A_INPUT_TELEMETRY_OFF_LAST_COVERFLOW_ACTIVE] > 1u) {
        return AZ_M2A_INPUT_TELEMETRY_INVALID_RECORD;
    }

    *generation = get_be32(
        record + AZ_M2A_INPUT_TELEMETRY_OFF_GENERATION);
    return AZ_M2A_INPUT_TELEMETRY_OK;
}

AzM2aInputTelemetryResult az_m2a_input_telemetry_select_newest_be(
    const uint8_t *slot_a,
    size_t slot_a_size,
    const uint8_t *slot_b,
    size_t slot_b_size,
    uint8_t *selected_slot,
    uint32_t *generation)
{
    uint32_t generation_a = 0u;
    uint32_t generation_b = 0u;
    uint8_t valid_a;
    uint8_t valid_b;

    if (selected_slot == NULL || generation == NULL) {
        return AZ_M2A_INPUT_TELEMETRY_NULL;
    }

    valid_a = slot_a != NULL &&
        az_m2a_input_telemetry_validate_record_be(
            slot_a,
            slot_a_size,
            &generation_a) == AZ_M2A_INPUT_TELEMETRY_OK ? 1u : 0u;
    valid_b = slot_b != NULL &&
        az_m2a_input_telemetry_validate_record_be(
            slot_b,
            slot_b_size,
            &generation_b) == AZ_M2A_INPUT_TELEMETRY_OK ? 1u : 0u;
    if (valid_a == 0u && valid_b == 0u) {
        return AZ_M2A_INPUT_TELEMETRY_INVALID_RECORD;
    }
    if (valid_a == 0u ||
        (valid_b != 0u && generation_is_newer(
            generation_b,
            generation_a))) {
        *selected_slot = (uint8_t)AZ_M2A_INPUT_TELEMETRY_SLOT_B;
        *generation = generation_b;
    } else {
        *selected_slot = (uint8_t)AZ_M2A_INPUT_TELEMETRY_SLOT_A;
        *generation = generation_a;
    }
    return AZ_M2A_INPUT_TELEMETRY_OK;
}
