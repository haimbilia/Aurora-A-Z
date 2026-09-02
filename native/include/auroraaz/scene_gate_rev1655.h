#ifndef AURORAAZ_SCENE_GATE_REV1655_H
#define AURORAAZ_SCENE_GATE_REV1655_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Exact Aurora 0.7b.2 Rev1655 addresses. */
#define AZ_REV1655_SCENE_APP_MANAGER_ADDRESS 0x82BBFFF8u
#define AZ_REV1655_SCENE_APP_MANAGER_VTABLE 0x8211F980u
#define AZ_REV1655_SCENE_CACHE_HEAD_ADDRESS 0x82BC006Cu
#define AZ_REV1655_SCENE_CACHE_NODE_PATH_OFFSET 0x00u
#define AZ_REV1655_SCENE_CACHE_NODE_HANDLE_OFFSET 0x04u
#define AZ_REV1655_SCENE_CACHE_NODE_ACQUIRED_OFFSET 0x08u
#define AZ_REV1655_SCENE_CACHE_NODE_NEXT_OFFSET 0x0Cu
#define AZ_REV1655_XUI_HANDLE_IS_VALID_ADDRESS 0x8280F448u
#define AZ_REV1655_XUI_ELEMENT_HAS_FOCUS_ADDRESS 0x82821978u

#define AZ_REV1655_SCENE_MAX_CACHE_NODES 256u
#define AZ_REV1655_SCENE_MAX_PATH_CODE_UNITS 512u

typedef uint8_t (*AzSceneGateReadBytesFn)(
    void *context,
    uintptr_t address,
    void *destination,
    size_t size);

/* Read a target-native 32-bit scalar, not a byte-order-neutral file word. */
typedef uint8_t (*AzSceneGateReadU32Fn)(
    void *context,
    uintptr_t address,
    uint32_t *value);

typedef int32_t (*AzSceneGateXuiPredicateFn)(
    void *context,
    uint32_t object_handle);

typedef struct AzSceneGateRev1655Bindings {
    void *context;
    AzSceneGateReadBytesFn read_bytes;
    AzSceneGateReadU32Fn read_u32;
    AzSceneGateXuiPredicateFn xui_handle_is_valid;
    AzSceneGateXuiPredicateFn xui_element_has_focus;
    uint8_t exact_image_verified;
} AzSceneGateRev1655Bindings;

typedef struct AzSceneGateValidationSpan {
    uintptr_t address;
    const uint8_t *expected;
    size_t size;
} AzSceneGateValidationSpan;

typedef enum AzSceneGateConfigureResult {
    AZ_SCENE_GATE_CONFIGURE_OK = 0,
    AZ_SCENE_GATE_CONFIGURE_NULL,
    AZ_SCENE_GATE_CONFIGURE_ALREADY_CONFIGURED,
    AZ_SCENE_GATE_CONFIGURE_BAD_BINDINGS,
    AZ_SCENE_GATE_CONFIGURE_IMAGE_UNVERIFIED,
    AZ_SCENE_GATE_CONFIGURE_SIGNATURE_MISMATCH
} AzSceneGateConfigureResult;

typedef enum AzSceneGateReason {
    AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED = 0,
    AZ_SCENE_GATE_REASON_MANAGER_UNAVAILABLE,
    AZ_SCENE_GATE_REASON_MEMORY_UNREADABLE,
    AZ_SCENE_GATE_REASON_CACHE_CHANGED,
    AZ_SCENE_GATE_REASON_CACHE_CYCLE,
    AZ_SCENE_GATE_REASON_CACHE_LIMIT,
    AZ_SCENE_GATE_REASON_PATH_INVALID,
    AZ_SCENE_GATE_REASON_MAIN_NOT_FOUND,
    AZ_SCENE_GATE_REASON_MAIN_DUPLICATE,
    AZ_SCENE_GATE_REASON_MAIN_NOT_ACQUIRED,
    AZ_SCENE_GATE_REASON_HANDLE_INVALID,
    AZ_SCENE_GATE_REASON_MAIN_NOT_FOCUSED,
    AZ_SCENE_GATE_REASON_MAIN_FOCUSED
} AzSceneGateReason;

typedef struct AzSceneGateDecision {
    AzSceneGateReason reason;
    uint32_t cache_head;
    uint32_t main_scene_node;
    uint32_t main_scene_handle;
    uint32_t scanned_nodes;
    uint8_t allows_capture;
} AzSceneGateDecision;

typedef struct AzSceneGateStatus {
    AzSceneGateConfigureResult last_configure_result;
    AzSceneGateReason last_reason;
    uint32_t configure_attempts;
    uint32_t configure_successes;
    uint32_t static_validation_failures;
    uint32_t probes;
    uint32_t allowed;
    uint32_t denied;
    uint32_t manager_unavailable;
    uint32_t memory_read_failures;
    uint32_t cache_changed;
    uint32_t cache_cycles;
    uint32_t cache_limits;
    uint32_t path_failures;
    uint32_t main_missing;
    uint32_t main_duplicate;
    uint32_t main_not_acquired;
    uint32_t invalid_handles;
    uint32_t main_not_focused;
    uint32_t last_cache_head;
    uint32_t last_main_scene_node;
    uint32_t last_main_scene_handle;
    uint32_t last_scanned_nodes;
    uint8_t configured;
    uint8_t exact_image_verified;
    uint8_t signatures_verified;
} AzSceneGateStatus;

/* Call only before the scene gate is published to live hooks. */
void az_rev1655_scene_gate_reset(void);

AzSceneGateConfigureResult az_rev1655_scene_gate_configure(
    const AzSceneGateRev1655Bindings *bindings);

/*
 * Read-only, fail-closed probe. Capture is allowed only when the cached
 * Aurora_Main.xur scene has a valid XUI handle and owns current focus.
 */
uint8_t az_rev1655_scene_gate_probe(AzSceneGateDecision *decision);

void az_rev1655_scene_gate_snapshot_status(AzSceneGateStatus *status);

size_t az_rev1655_scene_gate_validation_span_count(void);
uint8_t az_rev1655_scene_gate_validation_span(
    size_t index,
    AzSceneGateValidationSpan *span);

const char *az_scene_gate_configure_result_name(
    AzSceneGateConfigureResult result);
const char *az_scene_gate_reason_name(AzSceneGateReason reason);

#if defined(AURORAAZ_XBOX360)
/* Uses MmIsAddressValid and the exact in-image XUI predicates above. */
AzSceneGateConfigureResult az_rev1655_scene_gate_configure_default(
    uint8_t exact_image_verified);
#endif

#ifdef __cplusplus
}
#endif

#endif
