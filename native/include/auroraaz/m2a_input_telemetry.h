#ifndef AURORAAZ_M2A_INPUT_TELEMETRY_H
#define AURORAAZ_M2A_INPUT_TELEMETRY_H

#include <stddef.h>
#include <stdint.h>

#include <auroraaz/input_detour.h>
#include <auroraaz/rev1655_runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Worker-owned M2a telemetry. This module performs no file I/O and is never
 * called by the input hook. The worker drains AzInputDetourObservation values,
 * records them here, and alternates complete records between the two paths.
 */
#define AZ_M2A_INPUT_TELEMETRY_SLOT_A_PATH \
    "game:\\Data\\Logs\\AuroraAZ-M2a-input-A.bin"
#define AZ_M2A_INPUT_TELEMETRY_SLOT_B_PATH \
    "game:\\Data\\Logs\\AuroraAZ-M2a-input-B.bin"

#define AZ_M2A_INPUT_TELEMETRY_VERSION 2u
#define AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE 160u
#define AZ_M2A_INPUT_TELEMETRY_SLOT_COUNT 2u
#define AZ_M2A_INPUT_TELEMETRY_SLOT_A 0u
#define AZ_M2A_INPUT_TELEMETRY_SLOT_B 1u
#define AZ_M2A_INPUT_TELEMETRY_CONTROL_COUNT 7u
#define AZ_M2A_INPUT_TELEMETRY_EVENT_COUNT 3u
#define AZ_M2A_INPUT_TELEMETRY_ALL_CONTROLS_MASK 0x0000007Fu
#define AZ_M2A_INPUT_TELEMETRY_ALL_SAFETY_FLAGS 0x0000000Fu

/* Fixed big-endian wire-record offsets. Multi-byte values are always BE. */
#define AZ_M2A_INPUT_TELEMETRY_OFF_MAGIC 0u
#define AZ_M2A_INPUT_TELEMETRY_OFF_VERSION 4u
#define AZ_M2A_INPUT_TELEMETRY_OFF_RECORD_SIZE 6u
#define AZ_M2A_INPUT_TELEMETRY_OFF_GENERATION 8u
#define AZ_M2A_INPUT_TELEMETRY_OFF_RELEVANT_COUNT 12u
#define AZ_M2A_INPUT_TELEMETRY_OFF_SEEN_MASK 16u
#define AZ_M2A_INPUT_TELEMETRY_OFF_PRESS_MASK 20u
#define AZ_M2A_INPUT_TELEMETRY_OFF_REPEAT_MASK 24u
#define AZ_M2A_INPUT_TELEMETRY_OFF_RELEASE_MASK 28u
#define AZ_M2A_INPUT_TELEMETRY_OFF_CONSUMED_MASK 32u
#define AZ_M2A_INPUT_TELEMETRY_OFF_FILTER_QUEUED_MASK 36u
#define AZ_M2A_INPUT_TELEMETRY_OFF_SAFETY_FLAGS 40u
#define AZ_M2A_INPUT_TELEMETRY_OFF_INVALID_EVENT_COUNT 44u
#define AZ_M2A_INPUT_TELEMETRY_OFF_COUNTERS 48u
#define AZ_M2A_INPUT_TELEMETRY_OFF_OBSERVATION_DROPS 90u
#define AZ_M2A_INPUT_TELEMETRY_OFF_RUNTIME_STATE 94u
#define AZ_M2A_INPUT_TELEMETRY_OFF_WORKER_ENTERED 98u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_COMMAND 99u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_SERIAL 100u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_INPUT_FRAME 104u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_CALLER 108u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_VK 112u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_FLAGS 114u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_UNICODE 116u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_CONTROL 118u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_EVENT 119u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_REQUESTED_STAGE 120u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_EFFECTIVE_STAGE 121u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_CONSUMED 122u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_FILTER_QUEUED 123u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_WOULD_HANDLE 124u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_COVERFLOW_ACTIVE 125u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_USER_INDEX 126u
#define AZ_M2A_INPUT_TELEMETRY_OFF_LAST_HID_CODE 127u
#define AZ_M2A_INPUT_TELEMETRY_OFF_CRC32 156u

/*
 * Bytes 128..155 are reserved and must be zero.
 * CRC32 is IEEE CRC-32 over bytes [0, AZ_M2A_INPUT_TELEMETRY_OFF_CRC32).
 */
#define AZ_M2A_INPUT_TELEMETRY_CRC32_PREFIX_SIZE \
    AZ_M2A_INPUT_TELEMETRY_OFF_CRC32

/* Control-major uint16 counters: press, repeat, release for each control. */
#define AZ_M2A_INPUT_TELEMETRY_COUNTER_OFFSET(control_slot, event_slot) \
    (AZ_M2A_INPUT_TELEMETRY_OFF_COUNTERS + \
     ((((uint32_t)(control_slot) * AZ_M2A_INPUT_TELEMETRY_EVENT_COUNT) + \
       (uint32_t)(event_slot)) * 2u))

typedef enum AzM2aInputTelemetryControlSlot {
    AZ_M2A_INPUT_SLOT_A = 0,
    AZ_M2A_INPUT_SLOT_RB,
    AZ_M2A_INPUT_SLOT_R3,
    AZ_M2A_INPUT_SLOT_DPAD_LEFT,
    AZ_M2A_INPUT_SLOT_DPAD_RIGHT,
    AZ_M2A_INPUT_SLOT_LSTICK_LEFT,
    AZ_M2A_INPUT_SLOT_LSTICK_RIGHT
} AzM2aInputTelemetryControlSlot;

typedef enum AzM2aInputTelemetryEventSlot {
    AZ_M2A_INPUT_EVENT_SLOT_PRESS = 0,
    AZ_M2A_INPUT_EVENT_SLOT_REPEAT,
    AZ_M2A_INPUT_EVENT_SLOT_RELEASE
} AzM2aInputTelemetryEventSlot;

typedef enum AzM2aInputTelemetrySafetyFlag {
    AZ_M2A_INPUT_SAFETY_CONSUMED = 1u << 0u,
    AZ_M2A_INPUT_SAFETY_FILTER_QUEUED = 1u << 1u,
    AZ_M2A_INPUT_SAFETY_REQUESTED_NOT_OBSERVE = 1u << 2u,
    AZ_M2A_INPUT_SAFETY_EFFECTIVE_NOT_OBSERVE = 1u << 3u
} AzM2aInputTelemetrySafetyFlag;

typedef enum AzM2aInputTelemetryResult {
    AZ_M2A_INPUT_TELEMETRY_OK = 0,
    AZ_M2A_INPUT_TELEMETRY_NULL,
    AZ_M2A_INPUT_TELEMETRY_IGNORED,
    AZ_M2A_INPUT_TELEMETRY_NO_CHANGE,
    AZ_M2A_INPUT_TELEMETRY_BUFFER_TOO_SMALL,
    AZ_M2A_INPUT_TELEMETRY_INVALID_ARGUMENT,
    AZ_M2A_INPUT_TELEMETRY_INVALID_RECORD
} AzM2aInputTelemetryResult;

typedef struct AzM2aInputTelemetry {
    /* revision is the write-ack token; generation identifies wire records. */
    uint32_t revision;
    uint32_t generation;
    uint32_t relevant_observations;
    uint32_t seen_mask;
    uint32_t press_mask;
    uint32_t repeat_mask;
    uint32_t release_mask;
    uint32_t consumed_mask;
    uint32_t filter_queued_mask;
    uint32_t safety_flags;
    uint32_t invalid_event_count;
    uint32_t observation_drops;
    AzRev1655RuntimeState runtime_state;
    uint16_t counters[AZ_M2A_INPUT_TELEMETRY_CONTROL_COUNT]
        [AZ_M2A_INPUT_TELEMETRY_EVENT_COUNT];
    uint32_t last_serial;
    uint32_t last_input_frame;
    uint32_t last_caller_return_address;
    uint16_t last_virtual_key;
    uint16_t last_flags;
    uint16_t last_unicode;
    uint8_t worker_entered;
    uint8_t last_command;
    uint8_t last_control;
    uint8_t last_event;
    uint8_t last_requested_stage;
    uint8_t last_effective_stage;
    uint8_t last_consumed;
    uint8_t last_filter_queued;
    uint8_t last_would_handle;
    uint8_t last_coverflow_active;
    uint8_t last_user_index;
    uint8_t last_hid_code;
    uint8_t generation_assigned;
    uint8_t dirty;
} AzM2aInputTelemetry;

/* Initializes a dirty baseline so the worker can durably publish zeroes. */
void az_m2a_input_telemetry_init(AzM2aInputTelemetry *telemetry);

/*
 * Worker startup may seed the generation from the newest valid on-disk slot
 * so a record left by an earlier title session cannot outrank this session.
 * This is accepted only before the first snapshot generation is assigned.
 */
AzM2aInputTelemetryResult az_m2a_input_telemetry_seed_generation(
    AzM2aInputTelemetry *telemetry,
    uint32_t generation);

/*
 * Records only the seven declared controls. Unknown controls are ignored and
 * do not make the snapshot dirty. Counters saturate rather than wrap.
 */
AzM2aInputTelemetryResult az_m2a_input_telemetry_record(
    AzM2aInputTelemetry *telemetry,
    const AzInputDetourObservation *observation);

/*
 * Worker-only runtime context. A changed drop counter is independently dirty,
 * so a full observation ring remains visible even if no entry can be drained.
 */
AzM2aInputTelemetryResult az_m2a_input_telemetry_update_runtime(
    AzM2aInputTelemetry *telemetry,
    uint8_t worker_entered,
    AzRev1655RuntimeState runtime_state,
    uint32_t observation_drops);

uint8_t az_m2a_input_telemetry_is_dirty(
    const AzM2aInputTelemetry *telemetry);

/*
 * Encodes exactly one fixed-size, endian-neutral record without clearing
 * dirty. A new generation is assigned once per distinct dirty snapshot;
 * retries produce identical bytes. revision_token identifies that state for
 * acknowledgement and must not be used as a slot index.
 */
AzM2aInputTelemetryResult az_m2a_input_telemetry_snapshot_be(
    AzM2aInputTelemetry *telemetry,
    uint8_t *record,
    size_t record_size,
    uint32_t *revision_token);

/*
 * Call only after the corresponding record was written successfully. A stale
 * token cannot clear a newer change, which also makes failed writes retryable.
 */
uint8_t az_m2a_input_telemetry_acknowledge(
    AzM2aInputTelemetry *telemetry,
    uint32_t revision_token);

/* Validates identity, format invariants, reserved bytes, and the CRC32. */
AzM2aInputTelemetryResult az_m2a_input_telemetry_validate_record_be(
    const uint8_t *record,
    size_t record_size,
    uint32_t *generation);

/*
 * Chooses the newest valid slot using wrap-safe serial-number ordering. NULL
 * or invalid slots are ignored. Equal/ambiguous generations prefer slot A.
 */
AzM2aInputTelemetryResult az_m2a_input_telemetry_select_newest_be(
    const uint8_t *slot_a,
    size_t slot_a_size,
    const uint8_t *slot_b,
    size_t slot_b_size,
    uint8_t *selected_slot,
    uint32_t *generation);

#ifdef __cplusplus
}
#endif

#endif
