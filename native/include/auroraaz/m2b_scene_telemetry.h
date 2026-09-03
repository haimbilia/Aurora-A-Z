#ifndef AURORAAZ_M2B_SCENE_TELEMETRY_H
#define AURORAAZ_M2B_SCENE_TELEMETRY_H

#include <stddef.h>
#include <stdint.h>

#include <auroraaz/scene_gate_rev1655.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * M2b is a diagnostic-only hardware gate.  It observes the existing read-only
 * Rev1655 scene predicate and never publishes a capture decision to input,
 * render, selector, or filter code.
 *
 * Live hooks may create AzM2bSceneObservation values once per frame and put
 * them in a bounded SPSC queue.  A worker owns AzM2bSceneTelemetry, drains the
 * queue, and performs file I/O outside this module.  The convenience
 * probe-and-record API is only for a single-threaded/polling lab integration.
 */
#define AZ_M2B_SCENE_TELEMETRY_SLOT_A_PATH \
    "game:\\Data\\Logs\\AuroraAZ-M2b-scene-A.bin"
#define AZ_M2B_SCENE_TELEMETRY_SLOT_B_PATH \
    "game:\\Data\\Logs\\AuroraAZ-M2b-scene-B.bin"

#define AZ_M2B_SCENE_TELEMETRY_VERSION 1u
#define AZ_M2B_SCENE_TELEMETRY_RECORD_SIZE 256u
#define AZ_M2B_SCENE_TELEMETRY_SLOT_A 0u
#define AZ_M2B_SCENE_TELEMETRY_SLOT_B 1u
#define AZ_M2B_SCENE_REASON_COUNT 13u

/* Fixed big-endian wire-record offsets. Multi-byte values are always BE. */
#define AZ_M2B_SCENE_OFF_MAGIC 0u
#define AZ_M2B_SCENE_OFF_VERSION 4u
#define AZ_M2B_SCENE_OFF_RECORD_SIZE 6u
#define AZ_M2B_SCENE_OFF_GENERATION 8u
#define AZ_M2B_SCENE_OFF_SAMPLES 12u
#define AZ_M2B_SCENE_OFF_RAW_ALLOWED 16u
#define AZ_M2B_SCENE_OFF_RAW_DENIED 20u
#define AZ_M2B_SCENE_OFF_ELIGIBLE 24u
#define AZ_M2B_SCENE_OFF_TRANSITIONS 28u
#define AZ_M2B_SCENE_OFF_SAFETY_FLAGS 32u
#define AZ_M2B_SCENE_OFF_INVALID_SAMPLES 36u
#define AZ_M2B_SCENE_OFF_UI_ACTIVE_SAMPLES 40u
#define AZ_M2B_SCENE_OFF_UI_RAW_ALLOWED 44u
#define AZ_M2B_SCENE_OFF_NONMONOTONIC_FRAMES 48u
#define AZ_M2B_SCENE_OFF_CALLBACK_UNAVAILABLE 52u
#define AZ_M2B_SCENE_OFF_FIRST_FRAME 56u
#define AZ_M2B_SCENE_OFF_LAST_FRAME 60u
#define AZ_M2B_SCENE_OFF_LAST_TRANSITION_FRAME 64u
#define AZ_M2B_SCENE_OFF_MAX_SCANNED_NODES 68u
#define AZ_M2B_SCENE_OFF_LAST_CACHE_HEAD 72u
#define AZ_M2B_SCENE_OFF_LAST_MAIN_NODE 76u
#define AZ_M2B_SCENE_OFF_LAST_MAIN_HANDLE 80u
#define AZ_M2B_SCENE_OFF_LAST_SCANNED_NODES 84u
#define AZ_M2B_SCENE_OFF_LAST_STATUS_PROBES 88u
#define AZ_M2B_SCENE_OFF_LAST_STATUS_ALLOWED 92u
#define AZ_M2B_SCENE_OFF_LAST_STATUS_DENIED 96u
#define AZ_M2B_SCENE_OFF_LAST_CONFIGURE_ATTEMPTS 100u
#define AZ_M2B_SCENE_OFF_LAST_CONFIGURE_SUCCESSES 104u
#define AZ_M2B_SCENE_OFF_LAST_STATIC_FAILURES 108u
#define AZ_M2B_SCENE_OFF_REASON_COUNTERS 112u
#define AZ_M2B_SCENE_OFF_LAST_CONFIGURE_RESULT 164u
#define AZ_M2B_SCENE_OFF_LAST_DECISION_REASON 165u
#define AZ_M2B_SCENE_OFF_LAST_STATUS_REASON 166u
#define AZ_M2B_SCENE_OFF_LAST_CALLBACK_AVAILABLE 167u
#define AZ_M2B_SCENE_OFF_LAST_RAW_PROBE 168u
#define AZ_M2B_SCENE_OFF_LAST_DECISION_ALLOWS 169u
#define AZ_M2B_SCENE_OFF_LAST_ELIGIBLE 170u
#define AZ_M2B_SCENE_OFF_LAST_SYSTEM_UI_ACTIVE 171u
#define AZ_M2B_SCENE_OFF_LAST_STATUS_CONFIGURED 172u
#define AZ_M2B_SCENE_OFF_LAST_EXACT_IMAGE_VERIFIED 173u
#define AZ_M2B_SCENE_OFF_LAST_SIGNATURES_VERIFIED 174u
#define AZ_M2B_SCENE_OFF_MANAGER_UNAVAILABLE 176u
#define AZ_M2B_SCENE_OFF_MEMORY_READ_FAILURES 180u
#define AZ_M2B_SCENE_OFF_CACHE_CHANGED 184u
#define AZ_M2B_SCENE_OFF_CACHE_CYCLES 188u
#define AZ_M2B_SCENE_OFF_CACHE_LIMITS 192u
#define AZ_M2B_SCENE_OFF_PATH_FAILURES 196u
#define AZ_M2B_SCENE_OFF_MAIN_MISSING 200u
#define AZ_M2B_SCENE_OFF_MAIN_DUPLICATE 204u
#define AZ_M2B_SCENE_OFF_MAIN_NOT_ACQUIRED 208u
#define AZ_M2B_SCENE_OFF_INVALID_HANDLES 212u
#define AZ_M2B_SCENE_OFF_MAIN_NOT_FOCUSED 216u
#define AZ_M2B_SCENE_OFF_OBSERVATION_DROPS 220u
#define AZ_M2B_SCENE_OFF_LAST_SAMPLE_SAFETY_FLAGS 224u
#define AZ_M2B_SCENE_OFF_CRC32 252u
#define AZ_M2B_SCENE_CRC32_PREFIX_SIZE AZ_M2B_SCENE_OFF_CRC32

#define AZ_M2B_SCENE_REASON_COUNTER_OFFSET(reason) \
    (AZ_M2B_SCENE_OFF_REASON_COUNTERS + ((uint32_t)(reason) * 4u))

typedef enum AzM2bSceneSafetyFlag {
    AZ_M2B_SCENE_SAFETY_CALLBACK_UNAVAILABLE = 1u << 0u,
    AZ_M2B_SCENE_SAFETY_INVALID_VALUE = 1u << 1u,
    AZ_M2B_SCENE_SAFETY_PROBE_DECISION_MISMATCH = 1u << 2u,
    AZ_M2B_SCENE_SAFETY_REASON_ALLOW_MISMATCH = 1u << 3u,
    AZ_M2B_SCENE_SAFETY_GATE_NOT_VERIFIED = 1u << 4u,
    AZ_M2B_SCENE_SAFETY_STATUS_DECISION_MISMATCH = 1u << 5u,
    AZ_M2B_SCENE_SAFETY_SYSTEM_UI_RAW_ALLOW = 1u << 6u,
    AZ_M2B_SCENE_SAFETY_FRAME_NONMONOTONIC = 1u << 7u,
    AZ_M2B_SCENE_SAFETY_ELIGIBILITY_MISMATCH = 1u << 8u
} AzM2bSceneSafetyFlag;

#define AZ_M2B_SCENE_ALL_SAFETY_FLAGS 0x000001FFu
#define AZ_M2B_SCENE_INVALID_SAMPLE_FLAGS \
    (AZ_M2B_SCENE_SAFETY_CALLBACK_UNAVAILABLE | \
     AZ_M2B_SCENE_SAFETY_INVALID_VALUE | \
     AZ_M2B_SCENE_SAFETY_PROBE_DECISION_MISMATCH | \
     AZ_M2B_SCENE_SAFETY_REASON_ALLOW_MISMATCH | \
     AZ_M2B_SCENE_SAFETY_GATE_NOT_VERIFIED | \
     AZ_M2B_SCENE_SAFETY_STATUS_DECISION_MISMATCH | \
     AZ_M2B_SCENE_SAFETY_ELIGIBILITY_MISMATCH)

typedef enum AzM2bSceneTelemetryResult {
    AZ_M2B_SCENE_TELEMETRY_OK = 0,
    AZ_M2B_SCENE_TELEMETRY_NULL,
    AZ_M2B_SCENE_TELEMETRY_PROBE_UNAVAILABLE,
    AZ_M2B_SCENE_TELEMETRY_NO_CHANGE,
    AZ_M2B_SCENE_TELEMETRY_BUFFER_TOO_SMALL,
    AZ_M2B_SCENE_TELEMETRY_INVALID_ARGUMENT,
    AZ_M2B_SCENE_TELEMETRY_INVALID_RECORD
} AzM2bSceneTelemetryResult;

typedef uint8_t (*AzM2bSceneProbeFn)(
    void *context,
    AzSceneGateDecision *decision);

typedef void (*AzM2bSceneStatusFn)(
    void *context,
    AzSceneGateStatus *status);

typedef struct AzM2bSceneProbeBindings {
    void *context;
    AzM2bSceneProbeFn probe;
    AzM2bSceneStatusFn snapshot_status;
} AzM2bSceneProbeBindings;

typedef struct AzM2bSceneObservation {
    uint32_t frame_sequence;
    AzSceneGateDecision decision;
    AzSceneGateStatus status;
    uint8_t callback_available;
    uint8_t raw_probe_allowed;
    uint8_t capture_eligible;
    uint8_t system_ui_active;
} AzM2bSceneObservation;

typedef struct AzM2bSceneTelemetry {
    uint32_t revision;
    uint32_t generation;
    uint32_t samples;
    uint32_t raw_allowed;
    uint32_t raw_denied;
    uint32_t eligible;
    uint32_t transitions;
    uint32_t safety_flags;
    uint32_t invalid_samples;
    uint32_t ui_active_samples;
    uint32_t ui_raw_allowed;
    uint32_t nonmonotonic_frames;
    uint32_t callback_unavailable;
    uint32_t first_frame;
    uint32_t last_frame;
    uint32_t last_transition_frame;
    uint32_t max_scanned_nodes;
    uint32_t reason_counters[AZ_M2B_SCENE_REASON_COUNT];
    uint32_t observation_drops;
    uint32_t last_sample_safety_flags;
    AzSceneGateDecision last_decision;
    AzSceneGateStatus last_status;
    uint8_t last_callback_available;
    uint8_t last_raw_probe_allowed;
    uint8_t last_capture_eligible;
    uint8_t last_system_ui_active;
    uint8_t have_last_sample;
    uint8_t generation_assigned;
    uint8_t dirty;
} AzM2bSceneTelemetry;

/*
 * Calls only the supplied read-only scene probe and status snapshot.  Missing
 * callbacks and inconsistent results produce a synthetic fail-closed
 * observation.  The caller must not use this M2b diagnostic result to enable
 * input consumption.
 */
AzM2bSceneTelemetryResult az_m2b_scene_observe_live(
    const AzM2bSceneProbeBindings *bindings,
    uint32_t frame_sequence,
    uint8_t system_ui_active,
    AzM2bSceneObservation *observation);

void az_m2b_scene_telemetry_init(AzM2bSceneTelemetry *telemetry);

AzM2bSceneTelemetryResult az_m2b_scene_telemetry_seed_generation(
    AzM2bSceneTelemetry *telemetry,
    uint32_t generation);

/* Worker-owned aggregation of a bounded observation copied from live code. */
AzM2bSceneTelemetryResult az_m2b_scene_telemetry_record(
    AzM2bSceneTelemetry *telemetry,
    const AzM2bSceneObservation *observation);

/* Convenience for a single-threaded/polling lab build only. */
AzM2bSceneTelemetryResult az_m2b_scene_telemetry_probe_frame(
    AzM2bSceneTelemetry *telemetry,
    const AzM2bSceneProbeBindings *bindings,
    uint32_t frame_sequence,
    uint8_t system_ui_active,
    AzM2bSceneObservation *observation);

AzM2bSceneTelemetryResult az_m2b_scene_telemetry_update_drops(
    AzM2bSceneTelemetry *telemetry,
    uint32_t observation_drops);

uint8_t az_m2b_scene_telemetry_is_dirty(
    const AzM2bSceneTelemetry *telemetry);

AzM2bSceneTelemetryResult az_m2b_scene_telemetry_snapshot_be(
    AzM2bSceneTelemetry *telemetry,
    uint8_t *record,
    size_t record_size,
    uint32_t *revision_token);

uint8_t az_m2b_scene_telemetry_acknowledge(
    AzM2bSceneTelemetry *telemetry,
    uint32_t revision_token);

AzM2bSceneTelemetryResult az_m2b_scene_telemetry_validate_record_be(
    const uint8_t *record,
    size_t record_size,
    uint32_t *generation);

AzM2bSceneTelemetryResult az_m2b_scene_telemetry_select_newest_be(
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
