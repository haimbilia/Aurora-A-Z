#include <auroraaz/m2b_scene_telemetry.h>

#include <limits.h>
#include <string.h>

typedef char AzM2bSceneReasonCountMustMatch[
    AZ_SCENE_GATE_REASON_MAIN_FOCUSED + 1 == AZ_M2B_SCENE_REASON_COUNT ?
        1 : -1];
typedef char AzM2bSceneCountersMustEndAt164[
    AZ_M2B_SCENE_REASON_COUNTER_OFFSET(12u) + 4u == 164u ? 1 : -1];
typedef char AzM2bSceneRecordLayoutMustFit[
    AZ_M2B_SCENE_OFF_CRC32 + 4u == AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE ?
        1 : -1];

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
            const uint32_t mask = (uint32_t)(0u - (crc & 1u));
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static uint32_t increment_saturated_u32(uint32_t value)
{
    return value == UINT32_MAX ? UINT32_MAX : value + 1u;
}

static uint8_t generation_is_newer(uint32_t candidate, uint32_t reference)
{
    const uint32_t distance = candidate - reference;

    return distance != 0u && distance < 0x80000000u ? 1u : 0u;
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

static void bump_revision(AzM2bSceneTelemetry *telemetry)
{
    ++telemetry->revision;
    if (telemetry->revision == 0u) {
        telemetry->revision = 1u;
    }
    telemetry->generation_assigned = 0u;
    telemetry->dirty = 1u;
}

static uint8_t reason_is_valid(AzSceneGateReason reason)
{
    return (uint32_t)reason < AZ_M2B_SCENE_REASON_COUNT ? 1u : 0u;
}

static uint8_t configure_result_is_valid(AzSceneGateConfigureResult result)
{
    return (uint32_t)result <=
        (uint32_t)AZ_SCENE_GATE_CONFIGURE_SIGNATURE_MISMATCH ? 1u : 0u;
}

static uint8_t normalized_boolean(uint8_t value)
{
    return value == 1u ? 1u : 0u;
}

static AzSceneGateReason normalized_reason(AzSceneGateReason reason)
{
    return reason_is_valid(reason) != 0u ? reason :
        AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED;
}

static AzSceneGateConfigureResult normalized_configure_result(
    AzSceneGateConfigureResult result)
{
    return configure_result_is_valid(result) != 0u ? result :
        AZ_SCENE_GATE_CONFIGURE_BAD_BINDINGS;
}

static uint8_t status_booleans_are_valid(const AzSceneGateStatus *status)
{
    return status->configured <= 1u &&
        status->exact_image_verified <= 1u &&
        status->signatures_verified <= 1u ? 1u : 0u;
}

static uint8_t status_matches_decision(
    const AzSceneGateStatus *status,
    const AzSceneGateDecision *decision)
{
    return status->last_reason == decision->reason &&
        status->last_cache_head == decision->cache_head &&
        status->last_main_scene_node == decision->main_scene_node &&
        status->last_main_scene_handle == decision->main_scene_handle &&
        status->last_scanned_nodes == decision->scanned_nodes ? 1u : 0u;
}

static uint8_t strict_capture_eligibility(
    const AzM2bSceneObservation *observation)
{
    if (observation->callback_available != 1u ||
        observation->raw_probe_allowed != 1u ||
        observation->decision.allows_capture != 1u ||
        observation->decision.reason != AZ_SCENE_GATE_REASON_MAIN_FOCUSED ||
        observation->system_ui_active != 0u ||
        observation->status.configured != 1u ||
        observation->status.exact_image_verified != 1u ||
        observation->status.signatures_verified != 1u ||
        observation->status.last_configure_result !=
            AZ_SCENE_GATE_CONFIGURE_OK ||
        status_matches_decision(
            &observation->status,
            &observation->decision) == 0u) {
        return 0u;
    }
    return 1u;
}

static void initialize_fail_closed_observation(
    uint32_t frame_sequence,
    uint8_t system_ui_active,
    AzM2bSceneObservation *observation)
{
    memset(observation, 0, sizeof(*observation));
    observation->frame_sequence = frame_sequence;
    observation->system_ui_active = system_ui_active;
    observation->decision.reason =
        AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED;
    observation->status.last_configure_result =
        AZ_SCENE_GATE_CONFIGURE_BAD_BINDINGS;
    observation->status.last_reason =
        AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED;
}

AzM2bSceneTelemetryResult az_m2b_scene_observe_live(
    const AzM2bSceneProbeBindings *bindings,
    uint32_t frame_sequence,
    uint8_t system_ui_active,
    AzM2bSceneObservation *observation)
{
    uint8_t raw_result;

    if (observation == NULL) {
        return AZ_M2B_SCENE_TELEMETRY_NULL;
    }
    initialize_fail_closed_observation(
        frame_sequence,
        system_ui_active,
        observation);
    if (bindings == NULL || bindings->probe == NULL ||
        bindings->snapshot_status == NULL) {
        return AZ_M2B_SCENE_TELEMETRY_PROBE_UNAVAILABLE;
    }

    observation->callback_available = 1u;
    raw_result = bindings->probe(bindings->context, &observation->decision);
    observation->raw_probe_allowed = raw_result;
    bindings->snapshot_status(bindings->context, &observation->status);
    observation->capture_eligible = strict_capture_eligibility(observation);
    return AZ_M2B_SCENE_TELEMETRY_OK;
}

void az_m2b_scene_telemetry_init(AzM2bSceneTelemetry *telemetry)
{
    if (telemetry == NULL) {
        return;
    }

    memset(telemetry, 0, sizeof(*telemetry));
    telemetry->revision = 1u;
    telemetry->last_decision.reason =
        AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED;
    telemetry->last_status.last_configure_result =
        AZ_SCENE_GATE_CONFIGURE_BAD_BINDINGS;
    telemetry->last_status.last_reason =
        AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED;
    telemetry->dirty = 1u;
}

AzM2bSceneTelemetryResult az_m2b_scene_telemetry_seed_generation(
    AzM2bSceneTelemetry *telemetry,
    uint32_t generation)
{
    if (telemetry == NULL) {
        return AZ_M2B_SCENE_TELEMETRY_NULL;
    }
    if (telemetry->generation_assigned != 0u ||
        telemetry->generation != 0u) {
        return AZ_M2B_SCENE_TELEMETRY_INVALID_ARGUMENT;
    }
    telemetry->generation = generation;
    return AZ_M2B_SCENE_TELEMETRY_OK;
}

static uint32_t observation_safety_flags(
    const AzM2bSceneObservation *observation)
{
    uint32_t flags = 0u;
    const uint8_t valid_reason = reason_is_valid(
        observation->decision.reason);
    const uint8_t valid_status_reason = reason_is_valid(
        observation->status.last_reason);

    if (observation->callback_available != 1u) {
        flags |= AZ_M2B_SCENE_SAFETY_CALLBACK_UNAVAILABLE;
    }
    if (observation->callback_available > 1u ||
        observation->raw_probe_allowed > 1u ||
        observation->decision.allows_capture > 1u ||
        observation->capture_eligible > 1u ||
        observation->system_ui_active > 1u ||
        valid_reason == 0u || valid_status_reason == 0u ||
        configure_result_is_valid(
            observation->status.last_configure_result) == 0u ||
        status_booleans_are_valid(&observation->status) == 0u) {
        flags |= AZ_M2B_SCENE_SAFETY_INVALID_VALUE;
    }
    if ((observation->raw_probe_allowed == 1u) !=
        (observation->decision.allows_capture == 1u)) {
        flags |= AZ_M2B_SCENE_SAFETY_PROBE_DECISION_MISMATCH;
    }
    if (valid_reason != 0u &&
        ((observation->decision.reason ==
            AZ_SCENE_GATE_REASON_MAIN_FOCUSED) !=
         (observation->decision.allows_capture == 1u))) {
        flags |= AZ_M2B_SCENE_SAFETY_REASON_ALLOW_MISMATCH;
    }
    if (observation->status.configured != 1u ||
        observation->status.exact_image_verified != 1u ||
        observation->status.signatures_verified != 1u ||
        observation->status.last_configure_result !=
            AZ_SCENE_GATE_CONFIGURE_OK) {
        flags |= AZ_M2B_SCENE_SAFETY_GATE_NOT_VERIFIED;
    }
    if (valid_reason != 0u && valid_status_reason != 0u &&
        status_matches_decision(
            &observation->status,
            &observation->decision) == 0u) {
        flags |= AZ_M2B_SCENE_SAFETY_STATUS_DECISION_MISMATCH;
    }
    if (observation->system_ui_active == 1u &&
        observation->raw_probe_allowed == 1u) {
        flags |= AZ_M2B_SCENE_SAFETY_SYSTEM_UI_RAW_ALLOW;
    }
    if ((strict_capture_eligibility(observation) != 0u) !=
        (observation->capture_eligible == 1u)) {
        flags |= AZ_M2B_SCENE_SAFETY_ELIGIBILITY_MISMATCH;
    }
    return flags;
}

AzM2bSceneTelemetryResult az_m2b_scene_telemetry_record(
    AzM2bSceneTelemetry *telemetry,
    const AzM2bSceneObservation *observation)
{
    uint32_t flags;
    uint8_t raw_allowed;
    uint8_t eligible;
    uint8_t reason_index;

    if (telemetry == NULL || observation == NULL) {
        return AZ_M2B_SCENE_TELEMETRY_NULL;
    }

    flags = observation_safety_flags(observation);
    if (telemetry->have_last_sample != 0u &&
        generation_is_newer(
            observation->frame_sequence,
            telemetry->last_frame) == 0u) {
        flags |= AZ_M2B_SCENE_SAFETY_FRAME_NONMONOTONIC;
    }
    raw_allowed = observation->callback_available == 1u &&
        observation->raw_probe_allowed == 1u ? 1u : 0u;
    eligible = strict_capture_eligibility(observation);
    reason_index = reason_is_valid(observation->decision.reason) != 0u ?
        (uint8_t)observation->decision.reason :
        (uint8_t)AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED;

    if (telemetry->have_last_sample == 0u) {
        telemetry->first_frame = observation->frame_sequence;
    }
    else if (telemetry->last_decision.reason !=
            observation->decision.reason ||
        telemetry->last_raw_probe_allowed != raw_allowed ||
        telemetry->last_capture_eligible != eligible ||
        telemetry->last_system_ui_active !=
            (observation->system_ui_active == 1u ? 1u : 0u)) {
        telemetry->transitions = increment_saturated_u32(
            telemetry->transitions);
        telemetry->last_transition_frame = observation->frame_sequence;
    }

    telemetry->samples = increment_saturated_u32(telemetry->samples);
    if (raw_allowed != 0u) {
        telemetry->raw_allowed = increment_saturated_u32(
            telemetry->raw_allowed);
    }
    else {
        telemetry->raw_denied = increment_saturated_u32(
            telemetry->raw_denied);
    }
    if (eligible != 0u) {
        telemetry->eligible = increment_saturated_u32(telemetry->eligible);
    }
    if ((flags & AZ_M2B_SCENE_INVALID_SAMPLE_FLAGS) != 0u) {
        telemetry->invalid_samples = increment_saturated_u32(
            telemetry->invalid_samples);
    }
    if (observation->system_ui_active == 1u) {
        telemetry->ui_active_samples = increment_saturated_u32(
            telemetry->ui_active_samples);
        if (raw_allowed != 0u) {
            telemetry->ui_raw_allowed = increment_saturated_u32(
                telemetry->ui_raw_allowed);
        }
    }
    if ((flags & AZ_M2B_SCENE_SAFETY_FRAME_NONMONOTONIC) != 0u) {
        telemetry->nonmonotonic_frames = increment_saturated_u32(
            telemetry->nonmonotonic_frames);
    }
    if ((flags & AZ_M2B_SCENE_SAFETY_CALLBACK_UNAVAILABLE) != 0u) {
        telemetry->callback_unavailable = increment_saturated_u32(
            telemetry->callback_unavailable);
    }
    telemetry->reason_counters[reason_index] = increment_saturated_u32(
        telemetry->reason_counters[reason_index]);
    if (observation->decision.scanned_nodes >
        telemetry->max_scanned_nodes) {
        telemetry->max_scanned_nodes = observation->decision.scanned_nodes;
    }
    telemetry->safety_flags |= flags;
    telemetry->last_sample_safety_flags = flags;
    telemetry->last_frame = observation->frame_sequence;
    telemetry->last_decision = observation->decision;
    telemetry->last_status = observation->status;
    telemetry->last_callback_available =
        observation->callback_available == 1u ? 1u : 0u;
    telemetry->last_raw_probe_allowed = raw_allowed;
    telemetry->last_capture_eligible = eligible;
    telemetry->last_system_ui_active =
        observation->system_ui_active == 1u ? 1u : 0u;
    telemetry->have_last_sample = 1u;
    bump_revision(telemetry);
    return AZ_M2B_SCENE_TELEMETRY_OK;
}

AzM2bSceneTelemetryResult az_m2b_scene_telemetry_probe_frame(
    AzM2bSceneTelemetry *telemetry,
    const AzM2bSceneProbeBindings *bindings,
    uint32_t frame_sequence,
    uint8_t system_ui_active,
    AzM2bSceneObservation *observation)
{
    AzM2bSceneObservation local;
    AzM2bSceneTelemetryResult probe_result;
    AzM2bSceneTelemetryResult record_result;

    if (telemetry == NULL) {
        return AZ_M2B_SCENE_TELEMETRY_NULL;
    }
    probe_result = az_m2b_scene_observe_live(
        bindings,
        frame_sequence,
        system_ui_active,
        &local);
    record_result = az_m2b_scene_telemetry_record(telemetry, &local);
    if (observation != NULL) {
        *observation = local;
    }
    if (record_result != AZ_M2B_SCENE_TELEMETRY_OK) {
        return record_result;
    }
    return probe_result;
}

AzM2bSceneTelemetryResult az_m2b_scene_telemetry_update_drops(
    AzM2bSceneTelemetry *telemetry,
    uint32_t observation_drops)
{
    if (telemetry == NULL) {
        return AZ_M2B_SCENE_TELEMETRY_NULL;
    }
    if (telemetry->observation_drops == observation_drops) {
        return AZ_M2B_SCENE_TELEMETRY_NO_CHANGE;
    }
    telemetry->observation_drops = observation_drops;
    bump_revision(telemetry);
    return AZ_M2B_SCENE_TELEMETRY_OK;
}

uint8_t az_m2b_scene_telemetry_is_dirty(
    const AzM2bSceneTelemetry *telemetry)
{
    return telemetry != NULL && telemetry->dirty != 0u ? 1u : 0u;
}

AzM2bSceneTelemetryResult az_m2b_scene_telemetry_snapshot_be(
    AzM2bSceneTelemetry *telemetry,
    uint8_t *record,
    size_t record_size,
    uint32_t *revision_token)
{
    uint32_t reason;

    if (telemetry == NULL || record == NULL || revision_token == NULL) {
        return AZ_M2B_SCENE_TELEMETRY_NULL;
    }
    if (record_size < (size_t)AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE) {
        return AZ_M2B_SCENE_TELEMETRY_BUFFER_TOO_SMALL;
    }
    if (telemetry->dirty == 0u) {
        return AZ_M2B_SCENE_TELEMETRY_NO_CHANGE;
    }
    if (telemetry->generation_assigned == 0u) {
        ++telemetry->generation;
        telemetry->generation_assigned = 1u;
    }

    memset(record, 0, (size_t)AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE);
    record[0] = (uint8_t)'A';
    record[1] = (uint8_t)'Z';
    record[2] = (uint8_t)'S';
    record[3] = (uint8_t)'2';
    put_be16(record + AZ_M2B_SCENE_OFF_VERSION,
        (uint16_t)AZ_M2B_SCENE_TELEMETRY_VERSION);
    put_be16(record + AZ_M2B_SCENE_OFF_RECORD_SIZE,
        (uint16_t)AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE);
    put_be32(record + AZ_M2B_SCENE_OFF_GENERATION,
        telemetry->generation);
    put_be32(record + AZ_M2B_SCENE_OFF_SAMPLES, telemetry->samples);
    put_be32(record + AZ_M2B_SCENE_OFF_RAW_ALLOWED,
        telemetry->raw_allowed);
    put_be32(record + AZ_M2B_SCENE_OFF_RAW_DENIED,
        telemetry->raw_denied);
    put_be32(record + AZ_M2B_SCENE_OFF_ELIGIBLE, telemetry->eligible);
    put_be32(record + AZ_M2B_SCENE_OFF_TRANSITIONS,
        telemetry->transitions);
    put_be32(record + AZ_M2B_SCENE_OFF_SAFETY_FLAGS,
        telemetry->safety_flags);
    put_be32(record + AZ_M2B_SCENE_OFF_INVALID_SAMPLES,
        telemetry->invalid_samples);
    put_be32(record + AZ_M2B_SCENE_OFF_UI_ACTIVE_SAMPLES,
        telemetry->ui_active_samples);
    put_be32(record + AZ_M2B_SCENE_OFF_UI_RAW_ALLOWED,
        telemetry->ui_raw_allowed);
    put_be32(record + AZ_M2B_SCENE_OFF_NONMONOTONIC_FRAMES,
        telemetry->nonmonotonic_frames);
    put_be32(record + AZ_M2B_SCENE_OFF_CALLBACK_UNAVAILABLE,
        telemetry->callback_unavailable);
    put_be32(record + AZ_M2B_SCENE_OFF_FIRST_FRAME,
        telemetry->first_frame);
    put_be32(record + AZ_M2B_SCENE_OFF_LAST_FRAME,
        telemetry->last_frame);
    put_be32(record + AZ_M2B_SCENE_OFF_LAST_TRANSITION_FRAME,
        telemetry->last_transition_frame);
    put_be32(record + AZ_M2B_SCENE_OFF_MAX_SCANNED_NODES,
        telemetry->max_scanned_nodes);
    put_be32(record + AZ_M2B_SCENE_OFF_LAST_CACHE_HEAD,
        telemetry->last_decision.cache_head);
    put_be32(record + AZ_M2B_SCENE_OFF_LAST_MAIN_NODE,
        telemetry->last_decision.main_scene_node);
    put_be32(record + AZ_M2B_SCENE_OFF_LAST_MAIN_HANDLE,
        telemetry->last_decision.main_scene_handle);
    put_be32(record + AZ_M2B_SCENE_OFF_LAST_SCANNED_NODES,
        telemetry->last_decision.scanned_nodes);
    put_be32(record + AZ_M2B_SCENE_OFF_LAST_STATUS_PROBES,
        telemetry->last_status.probes);
    put_be32(record + AZ_M2B_SCENE_OFF_LAST_STATUS_ALLOWED,
        telemetry->last_status.allowed);
    put_be32(record + AZ_M2B_SCENE_OFF_LAST_STATUS_DENIED,
        telemetry->last_status.denied);
    put_be32(record + AZ_M2B_SCENE_OFF_LAST_CONFIGURE_ATTEMPTS,
        telemetry->last_status.configure_attempts);
    put_be32(record + AZ_M2B_SCENE_OFF_LAST_CONFIGURE_SUCCESSES,
        telemetry->last_status.configure_successes);
    put_be32(record + AZ_M2B_SCENE_OFF_LAST_STATIC_FAILURES,
        telemetry->last_status.static_validation_failures);
    for (reason = 0u; reason < AZ_M2B_SCENE_REASON_COUNT; ++reason) {
        put_be32(record + AZ_M2B_SCENE_REASON_COUNTER_OFFSET(reason),
            telemetry->reason_counters[reason]);
    }
    /*
     * Preserve malformed callback evidence in the safety flags while keeping
     * the wire record itself decodable and strictly fail-closed.
     */
    record[AZ_M2B_SCENE_OFF_LAST_CONFIGURE_RESULT] =
        (uint8_t)normalized_configure_result(
            telemetry->last_status.last_configure_result);
    record[AZ_M2B_SCENE_OFF_LAST_DECISION_REASON] =
        (uint8_t)normalized_reason(telemetry->last_decision.reason);
    record[AZ_M2B_SCENE_OFF_LAST_STATUS_REASON] =
        (uint8_t)normalized_reason(telemetry->last_status.last_reason);
    record[AZ_M2B_SCENE_OFF_LAST_CALLBACK_AVAILABLE] =
        telemetry->last_callback_available;
    record[AZ_M2B_SCENE_OFF_LAST_RAW_PROBE] =
        telemetry->last_raw_probe_allowed;
    record[AZ_M2B_SCENE_OFF_LAST_DECISION_ALLOWS] =
        normalized_boolean(telemetry->last_decision.allows_capture);
    record[AZ_M2B_SCENE_OFF_LAST_ELIGIBLE] =
        telemetry->last_capture_eligible;
    record[AZ_M2B_SCENE_OFF_LAST_SYSTEM_UI_ACTIVE] =
        telemetry->last_system_ui_active;
    record[AZ_M2B_SCENE_OFF_LAST_STATUS_CONFIGURED] =
        normalized_boolean(telemetry->last_status.configured);
    record[AZ_M2B_SCENE_OFF_LAST_EXACT_IMAGE_VERIFIED] =
        normalized_boolean(telemetry->last_status.exact_image_verified);
    record[AZ_M2B_SCENE_OFF_LAST_SIGNATURES_VERIFIED] =
        normalized_boolean(telemetry->last_status.signatures_verified);
    put_be32(record + AZ_M2B_SCENE_OFF_MANAGER_UNAVAILABLE,
        telemetry->last_status.manager_unavailable);
    put_be32(record + AZ_M2B_SCENE_OFF_MEMORY_READ_FAILURES,
        telemetry->last_status.memory_read_failures);
    put_be32(record + AZ_M2B_SCENE_OFF_CACHE_CHANGED,
        telemetry->last_status.cache_changed);
    put_be32(record + AZ_M2B_SCENE_OFF_CACHE_CYCLES,
        telemetry->last_status.cache_cycles);
    put_be32(record + AZ_M2B_SCENE_OFF_CACHE_LIMITS,
        telemetry->last_status.cache_limits);
    put_be32(record + AZ_M2B_SCENE_OFF_PATH_FAILURES,
        telemetry->last_status.path_failures);
    put_be32(record + AZ_M2B_SCENE_OFF_MAIN_MISSING,
        telemetry->last_status.main_missing);
    put_be32(record + AZ_M2B_SCENE_OFF_MAIN_DUPLICATE,
        telemetry->last_status.main_duplicate);
    put_be32(record + AZ_M2B_SCENE_OFF_MAIN_NOT_ACQUIRED,
        telemetry->last_status.main_not_acquired);
    put_be32(record + AZ_M2B_SCENE_OFF_INVALID_HANDLES,
        telemetry->last_status.invalid_handles);
    put_be32(record + AZ_M2B_SCENE_OFF_MAIN_NOT_FOCUSED,
        telemetry->last_status.main_not_focused);
    put_be32(record + AZ_M2B_SCENE_OFF_OBSERVATION_DROPS,
        telemetry->observation_drops);
    put_be32(record + AZ_M2B_SCENE_OFF_LAST_SAMPLE_SAFETY_FLAGS,
        telemetry->last_sample_safety_flags);
    put_be32(record + AZ_M2B_SCENE_OFF_CRC32,
        crc32_ieee(record, (size_t)AZ_M2B_SCENE_CRC32_PREFIX_SIZE));

    *revision_token = telemetry->revision;
    return AZ_M2B_SCENE_TELEMETRY_OK;
}

uint8_t az_m2b_scene_telemetry_acknowledge(
    AzM2bSceneTelemetry *telemetry,
    uint32_t revision_token)
{
    if (telemetry == NULL || telemetry->dirty == 0u ||
        revision_token != telemetry->revision) {
        return 0u;
    }
    telemetry->dirty = 0u;
    return 1u;
}

AzM2bSceneTelemetryResult az_m2b_scene_telemetry_validate_record_be(
    const uint8_t *record,
    size_t record_size,
    uint32_t *generation)
{
    uint32_t samples;
    uint32_t raw_allowed;
    uint32_t raw_denied;
    uint32_t eligible;
    uint32_t transitions;
    uint32_t safety_flags;
    uint32_t ui_active;
    uint32_t ui_raw_allowed;
    uint64_t reason_total = 0u;
    uint32_t reason;

    if (record == NULL || generation == NULL) {
        return AZ_M2B_SCENE_TELEMETRY_NULL;
    }
    if (record_size != (size_t)AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE) {
        return record_size < (size_t)AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE ?
            AZ_M2B_SCENE_TELEMETRY_BUFFER_TOO_SMALL :
            AZ_M2B_SCENE_TELEMETRY_INVALID_RECORD;
    }
    if (record[0] != (uint8_t)'A' || record[1] != (uint8_t)'Z' ||
        record[2] != (uint8_t)'S' || record[3] != (uint8_t)'2' ||
        get_be16(record + AZ_M2B_SCENE_OFF_VERSION) !=
            (uint16_t)AZ_M2B_SCENE_TELEMETRY_VERSION ||
        get_be16(record + AZ_M2B_SCENE_OFF_RECORD_SIZE) !=
            (uint16_t)AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE ||
        get_be32(record + AZ_M2B_SCENE_OFF_CRC32) !=
            crc32_ieee(record, (size_t)AZ_M2B_SCENE_CRC32_PREFIX_SIZE) ||
        record[175u] != 0u ||
        bytes_are_zero(record, 228u, AZ_M2B_SCENE_OFF_CRC32) == 0u) {
        return AZ_M2B_SCENE_TELEMETRY_INVALID_RECORD;
    }

    samples = get_be32(record + AZ_M2B_SCENE_OFF_SAMPLES);
    raw_allowed = get_be32(record + AZ_M2B_SCENE_OFF_RAW_ALLOWED);
    raw_denied = get_be32(record + AZ_M2B_SCENE_OFF_RAW_DENIED);
    eligible = get_be32(record + AZ_M2B_SCENE_OFF_ELIGIBLE);
    transitions = get_be32(record + AZ_M2B_SCENE_OFF_TRANSITIONS);
    safety_flags = get_be32(record + AZ_M2B_SCENE_OFF_SAFETY_FLAGS);
    ui_active = get_be32(record + AZ_M2B_SCENE_OFF_UI_ACTIVE_SAMPLES);
    ui_raw_allowed = get_be32(record + AZ_M2B_SCENE_OFF_UI_RAW_ALLOWED);
    for (reason = 0u; reason < AZ_M2B_SCENE_REASON_COUNT; ++reason) {
        const uint32_t count = get_be32(
            record + AZ_M2B_SCENE_REASON_COUNTER_OFFSET(reason));
        if (count > samples) {
            return AZ_M2B_SCENE_TELEMETRY_INVALID_RECORD;
        }
        reason_total += count;
    }

    if ((safety_flags & ~AZ_M2B_SCENE_ALL_SAFETY_FLAGS) != 0u ||
        (get_be32(record + AZ_M2B_SCENE_OFF_LAST_SAMPLE_SAFETY_FLAGS) &
            ~AZ_M2B_SCENE_ALL_SAFETY_FLAGS) != 0u ||
        raw_allowed > samples || raw_denied > samples ||
        eligible > raw_allowed || ui_active > samples ||
        ui_raw_allowed > ui_active || ui_raw_allowed > raw_allowed ||
        get_be32(record + AZ_M2B_SCENE_OFF_INVALID_SAMPLES) > samples ||
        get_be32(record + AZ_M2B_SCENE_OFF_NONMONOTONIC_FRAMES) > samples ||
        get_be32(record + AZ_M2B_SCENE_OFF_CALLBACK_UNAVAILABLE) > samples ||
        (samples != UINT32_MAX &&
            ((uint64_t)raw_allowed + (uint64_t)raw_denied !=
                (uint64_t)samples ||
             reason_total != (uint64_t)samples ||
             (samples == 0u ? transitions != 0u :
                transitions >= samples))) ||
        record[AZ_M2B_SCENE_OFF_LAST_CONFIGURE_RESULT] >
            (uint8_t)AZ_SCENE_GATE_CONFIGURE_SIGNATURE_MISMATCH ||
        record[AZ_M2B_SCENE_OFF_LAST_DECISION_REASON] >=
            (uint8_t)AZ_M2B_SCENE_REASON_COUNT ||
        record[AZ_M2B_SCENE_OFF_LAST_STATUS_REASON] >=
            (uint8_t)AZ_M2B_SCENE_REASON_COUNT ||
        record[AZ_M2B_SCENE_OFF_LAST_CALLBACK_AVAILABLE] > 1u ||
        record[AZ_M2B_SCENE_OFF_LAST_RAW_PROBE] > 1u ||
        record[AZ_M2B_SCENE_OFF_LAST_DECISION_ALLOWS] > 1u ||
        record[AZ_M2B_SCENE_OFF_LAST_ELIGIBLE] > 1u ||
        record[AZ_M2B_SCENE_OFF_LAST_SYSTEM_UI_ACTIVE] > 1u ||
        record[AZ_M2B_SCENE_OFF_LAST_STATUS_CONFIGURED] > 1u ||
        record[AZ_M2B_SCENE_OFF_LAST_EXACT_IMAGE_VERIFIED] > 1u ||
        record[AZ_M2B_SCENE_OFF_LAST_SIGNATURES_VERIFIED] > 1u) {
        return AZ_M2B_SCENE_TELEMETRY_INVALID_RECORD;
    }
    if (record[AZ_M2B_SCENE_OFF_LAST_ELIGIBLE] != 0u &&
        (record[AZ_M2B_SCENE_OFF_LAST_CALLBACK_AVAILABLE] != 1u ||
         record[AZ_M2B_SCENE_OFF_LAST_RAW_PROBE] != 1u ||
         record[AZ_M2B_SCENE_OFF_LAST_DECISION_ALLOWS] != 1u ||
         record[AZ_M2B_SCENE_OFF_LAST_DECISION_REASON] !=
            (uint8_t)AZ_SCENE_GATE_REASON_MAIN_FOCUSED ||
         record[AZ_M2B_SCENE_OFF_LAST_SYSTEM_UI_ACTIVE] != 0u ||
         record[AZ_M2B_SCENE_OFF_LAST_CONFIGURE_RESULT] !=
            (uint8_t)AZ_SCENE_GATE_CONFIGURE_OK ||
         record[AZ_M2B_SCENE_OFF_LAST_STATUS_CONFIGURED] != 1u ||
         record[AZ_M2B_SCENE_OFF_LAST_EXACT_IMAGE_VERIFIED] != 1u ||
         record[AZ_M2B_SCENE_OFF_LAST_SIGNATURES_VERIFIED] != 1u)) {
        return AZ_M2B_SCENE_TELEMETRY_INVALID_RECORD;
    }

    *generation = get_be32(record + AZ_M2B_SCENE_OFF_GENERATION);
    return AZ_M2B_SCENE_TELEMETRY_OK;
}

AzM2bSceneTelemetryResult az_m2b_scene_telemetry_select_newest_be(
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
        return AZ_M2B_SCENE_TELEMETRY_NULL;
    }
    valid_a = slot_a != NULL &&
        az_m2b_scene_telemetry_validate_record_be(
            slot_a, slot_a_size, &generation_a) ==
            AZ_M2B_SCENE_TELEMETRY_OK ? 1u : 0u;
    valid_b = slot_b != NULL &&
        az_m2b_scene_telemetry_validate_record_be(
            slot_b, slot_b_size, &generation_b) ==
            AZ_M2B_SCENE_TELEMETRY_OK ? 1u : 0u;
    if (valid_a == 0u && valid_b == 0u) {
        return AZ_M2B_SCENE_TELEMETRY_INVALID_RECORD;
    }
    if (valid_a == 0u ||
        (valid_b != 0u && generation_is_newer(
            generation_b, generation_a) != 0u)) {
        *selected_slot = (uint8_t)AZ_M2B_SCENE_TELEMETRY_SLOT_B;
        *generation = generation_b;
    }
    else {
        *selected_slot = (uint8_t)AZ_M2B_SCENE_TELEMETRY_SLOT_A;
        *generation = generation_a;
    }
    return AZ_M2B_SCENE_TELEMETRY_OK;
}
