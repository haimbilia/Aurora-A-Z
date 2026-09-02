#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(AURORAAZ_XBOX360)
#include <xecore/xboxkrnl.h>
#endif

#include <auroraaz/scene_gate_rev1655.h>

#define AZ_MAIN_SCENE_BASENAME "Aurora_Main.xur"
#define AZ_MAIN_SCENE_BASENAME_LENGTH 15u
#define AZ_SCENE_GATE_VALIDATION_SPAN_COUNT 7u

typedef struct AzSceneCacheNodeSnapshot {
    uint32_t path;
    uint32_t handle;
    uint32_t acquired;
    uint32_t next;
} AzSceneCacheNodeSnapshot;

typedef struct AzSceneGateState {
    AzSceneGateRev1655Bindings bindings;
    volatile uint32_t configured;
    volatile uint32_t exact_image_verified;
    volatile uint32_t signatures_verified;
    volatile uint32_t last_configure_result;
    volatile uint32_t last_reason;
    volatile uint32_t configure_attempts;
    volatile uint32_t configure_successes;
    volatile uint32_t static_validation_failures;
    volatile uint32_t probes;
    volatile uint32_t allowed;
    volatile uint32_t denied;
    volatile uint32_t manager_unavailable;
    volatile uint32_t memory_read_failures;
    volatile uint32_t cache_changed;
    volatile uint32_t cache_cycles;
    volatile uint32_t cache_limits;
    volatile uint32_t path_failures;
    volatile uint32_t main_missing;
    volatile uint32_t main_duplicate;
    volatile uint32_t main_not_acquired;
    volatile uint32_t invalid_handles;
    volatile uint32_t main_not_focused;
    volatile uint32_t last_cache_head;
    volatile uint32_t last_main_scene_node;
    volatile uint32_t last_main_scene_handle;
    volatile uint32_t last_scanned_nodes;
} AzSceneGateState;

static const uint8_t g_app_manager_accessor_signature[] = {
    0x3Du, 0x60u, 0x82u, 0xBCu,
    0x38u, 0x6Bu, 0xFFu, 0xF8u
};

static const uint8_t g_cache_layout_signature[] = {
    0x83u, 0xFFu, 0x00u, 0x28u,
    0x48u, 0x00u, 0x00u, 0x28u,
    0x7Fu, 0xA3u, 0xEBu, 0x78u,
    0x80u, 0x9Fu, 0x00u, 0x00u,
    0x48u, 0x73u, 0xABu, 0x69u,
    0x2Cu, 0x03u, 0x00u, 0x00u,
    0x40u, 0x82u, 0x00u, 0x10u,
    0x81u, 0x7Fu, 0x00u, 0x08u,
    0x2Fu, 0x0Bu, 0x00u, 0x00u,
    0x41u, 0x9Au, 0x00u, 0x24u,
    0x83u, 0xFFu, 0x00u, 0x0Cu,
    0x2Bu, 0x1Fu, 0x00u, 0x00u
};

static const uint8_t g_cache_handle_signature[] = {
    0x39u, 0x60u, 0x00u, 0x01u,
    0x83u, 0xBFu, 0x00u, 0x04u
};

static const uint8_t g_handle_valid_signature[] = {
    0x54u, 0x6Bu, 0x04u, 0x3Eu,
    0x2Bu, 0x03u, 0x00u, 0x00u,
    0x41u, 0x9Au, 0x00u, 0x60u,
    0x3Du, 0x40u, 0x82u, 0xBBu,
    0x55u, 0x6Bu, 0x04u, 0x3Eu,
    0x39u, 0x4Au, 0xF8u, 0xB8u,
    0x81u, 0x2Au, 0x02u, 0x20u,
    0x7Fu, 0x0Bu, 0x48u, 0x40u
};

static const uint8_t g_has_focus_entry_signature[] = {
    0x7Du, 0x88u, 0x02u, 0xA6u,
    0x48u, 0x14u, 0x63u, 0x51u,
    0x94u, 0x21u, 0xFFu, 0x90u,
    0x3Du, 0x60u, 0x82u, 0xBBu,
    0x7Cu, 0x7Du, 0x1Bu, 0x78u,
    0x83u, 0xEBu, 0x0Fu, 0x28u
};

static const uint8_t g_has_focus_descendant_signature[] = {
    0x4Bu, 0xFEu, 0xE0u, 0xE1u,
    0x7Fu, 0x1Fu, 0x18u, 0x40u,
    0x41u, 0x9Au, 0x00u, 0x1Cu,
    0x38u, 0xA0u, 0x00u, 0x00u,
    0x7Fu, 0xE4u, 0xFBu, 0x78u,
    0x7Fu, 0xA3u, 0xEBu, 0x78u,
    0x4Bu, 0xFFu, 0xDEu, 0x01u,
    0x2Cu, 0x03u, 0x00u, 0x00u,
    0x41u, 0x82u, 0x00u, 0x0Cu
};

/* PPC target memory contains XUI paths as big-endian UTF-16 code units. */
static const uint8_t g_main_scene_literal[] = {
    0x00u, 'A', 0x00u, 'u', 0x00u, 'r', 0x00u, 'o',
    0x00u, 'r', 0x00u, 'a', 0x00u, '_', 0x00u, 'M',
    0x00u, 'a', 0x00u, 'i', 0x00u, 'n', 0x00u, '.',
    0x00u, 'x', 0x00u, 'u', 0x00u, 'r', 0x00u, 0x00u
};

static const AzSceneGateValidationSpan g_validation_spans[
    AZ_SCENE_GATE_VALIDATION_SPAN_COUNT] = {
    { 0x82212194u, g_app_manager_accessor_signature,
      sizeof(g_app_manager_accessor_signature) },
    { 0x82225540u, g_cache_layout_signature,
      sizeof(g_cache_layout_signature) },
    { 0x82225588u, g_cache_handle_signature,
      sizeof(g_cache_handle_signature) },
    { AZ_REV1655_XUI_HANDLE_IS_VALID_ADDRESS, g_handle_valid_signature,
      sizeof(g_handle_valid_signature) },
    { AZ_REV1655_XUI_ELEMENT_HAS_FOCUS_ADDRESS,
      g_has_focus_entry_signature, sizeof(g_has_focus_entry_signature) },
    { 0x82821998u, g_has_focus_descendant_signature,
      sizeof(g_has_focus_descendant_signature) },
    { 0x821208B8u, g_main_scene_literal, sizeof(g_main_scene_literal) }
};

static AzSceneGateState g_scene_gate;

static uint32_t load_u32(const volatile uint32_t *value)
{
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static void store_u32(volatile uint32_t *value, uint32_t replacement)
{
    __atomic_store_n(value, replacement, __ATOMIC_RELEASE);
}

static void increment_u32(volatile uint32_t *value)
{
    (void)__atomic_add_fetch(value, 1u, __ATOMIC_ACQ_REL);
}

static uint8_t bindings_are_complete(
    const AzSceneGateRev1655Bindings *bindings)
{
    return (bindings != NULL &&
        bindings->read_bytes != NULL &&
        bindings->read_u32 != NULL &&
        bindings->xui_handle_is_valid != NULL &&
        bindings->xui_element_has_focus != NULL) ? 1u : 0u;
}

static uint8_t validate_signatures(
    const AzSceneGateRev1655Bindings *bindings)
{
    uint8_t actual[sizeof(g_cache_layout_signature)];
    size_t index;

    for (index = 0u; index < AZ_SCENE_GATE_VALIDATION_SPAN_COUNT; ++index) {
        const AzSceneGateValidationSpan *span = &g_validation_spans[index];

        if (span->size > sizeof(actual) ||
            bindings->read_bytes(
                bindings->context,
                span->address,
                actual,
                span->size) == 0u ||
            memcmp(actual, span->expected, span->size) != 0) {
            return 0u;
        }
    }
    return 1u;
}

static uint8_t read_target_u32(uintptr_t address, uint32_t *value)
{
    if (value == NULL ||
        g_scene_gate.bindings.read_u32(
            g_scene_gate.bindings.context,
            address,
            value) == 0u) {
        return 0u;
    }
    return 1u;
}

static uint8_t read_node(
    uint32_t address,
    AzSceneCacheNodeSnapshot *node)
{
    if (node == NULL || address == 0u || address > UINT32_MAX - 0x0Cu) {
        return 0u;
    }

    return (read_target_u32(
                (uintptr_t)address +
                    AZ_REV1655_SCENE_CACHE_NODE_PATH_OFFSET,
                &node->path) != 0u &&
            read_target_u32(
                (uintptr_t)address +
                    AZ_REV1655_SCENE_CACHE_NODE_HANDLE_OFFSET,
                &node->handle) != 0u &&
            read_target_u32(
                (uintptr_t)address +
                    AZ_REV1655_SCENE_CACHE_NODE_ACQUIRED_OFFSET,
                &node->acquired) != 0u &&
            read_target_u32(
                (uintptr_t)address +
                    AZ_REV1655_SCENE_CACHE_NODE_NEXT_OFFSET,
                &node->next) != 0u) ? 1u : 0u;
}

static uint8_t nodes_are_equal(
    const AzSceneCacheNodeSnapshot *left,
    const AzSceneCacheNodeSnapshot *right)
{
    return (left->path == right->path &&
        left->handle == right->handle &&
        left->acquired == right->acquired &&
        left->next == right->next) ? 1u : 0u;
}

static uint8_t path_is_main_scene(uint32_t path_address, uint8_t *is_main)
{
    uint16_t path[AZ_REV1655_SCENE_MAX_PATH_CODE_UNITS];
    size_t length;

    if (is_main == NULL || path_address == 0u ||
        (path_address & 1u) != 0u ||
        path_address > UINT32_MAX -
            (AZ_REV1655_SCENE_MAX_PATH_CODE_UNITS * 2u - 1u)) {
        return 0u;
    }
    *is_main = 0u;

    for (length = 0u;
         length < AZ_REV1655_SCENE_MAX_PATH_CODE_UNITS;
         ++length) {
        uint8_t encoded[2];

        if (g_scene_gate.bindings.read_bytes(
                g_scene_gate.bindings.context,
                (uintptr_t)path_address + length * 2u,
                encoded,
                sizeof(encoded)) == 0u) {
            return 0u;
        }
        path[length] = (uint16_t)(
            ((uint16_t)encoded[0] << 8u) | encoded[1]);
        if (path[length] == '\0') {
            break;
        }
    }

    if (length == AZ_REV1655_SCENE_MAX_PATH_CODE_UNITS) {
        return 0u;
    }
    if (length < AZ_MAIN_SCENE_BASENAME_LENGTH) {
        return 1u;
    }

    {
        size_t index;

        for (index = 0u; index < AZ_MAIN_SCENE_BASENAME_LENGTH; ++index) {
            if (path[length - AZ_MAIN_SCENE_BASENAME_LENGTH + index] !=
                (uint16_t)(uint8_t)AZ_MAIN_SCENE_BASENAME[index]) {
                return 1u;
            }
        }
    }

    if (length == AZ_MAIN_SCENE_BASENAME_LENGTH) {
        *is_main = 1u;
    }
    else {
        const uint16_t separator =
            path[length - AZ_MAIN_SCENE_BASENAME_LENGTH - 1u];
        if (separator == (uint16_t)'/' || separator == (uint16_t)'\\' ||
            separator == (uint16_t)':' || separator == (uint16_t)'#') {
            *is_main = 1u;
        }
    }
    return 1u;
}

static void initialize_decision(AzSceneGateDecision *decision)
{
    memset(decision, 0, sizeof(*decision));
    decision->reason = AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED;
}

static void publish_decision(const AzSceneGateDecision *decision)
{
    store_u32(&g_scene_gate.last_reason, (uint32_t)decision->reason);
    store_u32(&g_scene_gate.last_cache_head, decision->cache_head);
    store_u32(
        &g_scene_gate.last_main_scene_node,
        decision->main_scene_node);
    store_u32(
        &g_scene_gate.last_main_scene_handle,
        decision->main_scene_handle);
    store_u32(&g_scene_gate.last_scanned_nodes, decision->scanned_nodes);

    if (decision->allows_capture != 0u) {
        increment_u32(&g_scene_gate.allowed);
        return;
    }

    increment_u32(&g_scene_gate.denied);
    switch (decision->reason) {
    case AZ_SCENE_GATE_REASON_MANAGER_UNAVAILABLE:
        increment_u32(&g_scene_gate.manager_unavailable);
        break;
    case AZ_SCENE_GATE_REASON_MEMORY_UNREADABLE:
        increment_u32(&g_scene_gate.memory_read_failures);
        break;
    case AZ_SCENE_GATE_REASON_CACHE_CHANGED:
        increment_u32(&g_scene_gate.cache_changed);
        break;
    case AZ_SCENE_GATE_REASON_CACHE_CYCLE:
        increment_u32(&g_scene_gate.cache_cycles);
        break;
    case AZ_SCENE_GATE_REASON_CACHE_LIMIT:
        increment_u32(&g_scene_gate.cache_limits);
        break;
    case AZ_SCENE_GATE_REASON_PATH_INVALID:
        increment_u32(&g_scene_gate.path_failures);
        break;
    case AZ_SCENE_GATE_REASON_MAIN_NOT_FOUND:
        increment_u32(&g_scene_gate.main_missing);
        break;
    case AZ_SCENE_GATE_REASON_MAIN_DUPLICATE:
        increment_u32(&g_scene_gate.main_duplicate);
        break;
    case AZ_SCENE_GATE_REASON_MAIN_NOT_ACQUIRED:
        increment_u32(&g_scene_gate.main_not_acquired);
        break;
    case AZ_SCENE_GATE_REASON_HANDLE_INVALID:
        increment_u32(&g_scene_gate.invalid_handles);
        break;
    case AZ_SCENE_GATE_REASON_MAIN_NOT_FOCUSED:
        increment_u32(&g_scene_gate.main_not_focused);
        break;
    default:
        break;
    }
}

void az_rev1655_scene_gate_reset(void)
{
    memset(&g_scene_gate, 0, sizeof(g_scene_gate));
    store_u32(
        &g_scene_gate.last_configure_result,
        (uint32_t)AZ_SCENE_GATE_CONFIGURE_BAD_BINDINGS);
    store_u32(
        &g_scene_gate.last_reason,
        (uint32_t)AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED);
}

AzSceneGateConfigureResult az_rev1655_scene_gate_configure(
    const AzSceneGateRev1655Bindings *bindings)
{
    AzSceneGateConfigureResult result;

    increment_u32(&g_scene_gate.configure_attempts);
    if (bindings == NULL) {
        result = AZ_SCENE_GATE_CONFIGURE_NULL;
    }
    else if (load_u32(&g_scene_gate.configured) != 0u) {
        result = AZ_SCENE_GATE_CONFIGURE_ALREADY_CONFIGURED;
    }
    else if (bindings_are_complete(bindings) == 0u) {
        result = AZ_SCENE_GATE_CONFIGURE_BAD_BINDINGS;
    }
    else if (bindings->exact_image_verified == 0u) {
        result = AZ_SCENE_GATE_CONFIGURE_IMAGE_UNVERIFIED;
    }
    else if (validate_signatures(bindings) == 0u) {
        increment_u32(&g_scene_gate.static_validation_failures);
        result = AZ_SCENE_GATE_CONFIGURE_SIGNATURE_MISMATCH;
    }
    else {
        g_scene_gate.bindings = *bindings;
        store_u32(&g_scene_gate.exact_image_verified, 1u);
        store_u32(&g_scene_gate.signatures_verified, 1u);
        store_u32(&g_scene_gate.configured, 1u);
        increment_u32(&g_scene_gate.configure_successes);
        result = AZ_SCENE_GATE_CONFIGURE_OK;
    }

    store_u32(&g_scene_gate.last_configure_result, (uint32_t)result);
    return result;
}

uint8_t az_rev1655_scene_gate_probe(AzSceneGateDecision *decision)
{
    AzSceneGateDecision local_decision;
    AzSceneCacheNodeSnapshot main_snapshot;
    uint32_t visited[AZ_REV1655_SCENE_MAX_CACHE_NODES];
    uint32_t manager_vtable;
    uint32_t manager_vtable_after;
    uint32_t cache_head;
    uint32_t cache_head_after;
    uint32_t current;
    uint32_t main_count = 0u;

    initialize_decision(&local_decision);
    memset(&main_snapshot, 0, sizeof(main_snapshot));
    increment_u32(&g_scene_gate.probes);

    if (load_u32(&g_scene_gate.configured) == 0u ||
        load_u32(&g_scene_gate.exact_image_verified) == 0u ||
        load_u32(&g_scene_gate.signatures_verified) == 0u) {
        goto finished;
    }

    if (read_target_u32(
            AZ_REV1655_SCENE_APP_MANAGER_ADDRESS,
            &manager_vtable) == 0u) {
        local_decision.reason = AZ_SCENE_GATE_REASON_MEMORY_UNREADABLE;
        goto finished;
    }
    if (manager_vtable != AZ_REV1655_SCENE_APP_MANAGER_VTABLE) {
        local_decision.reason = AZ_SCENE_GATE_REASON_MANAGER_UNAVAILABLE;
        goto finished;
    }
    if (read_target_u32(
            AZ_REV1655_SCENE_CACHE_HEAD_ADDRESS,
            &cache_head) == 0u) {
        local_decision.reason = AZ_SCENE_GATE_REASON_MEMORY_UNREADABLE;
        goto finished;
    }

    local_decision.cache_head = cache_head;
    current = cache_head;
    while (current != 0u) {
        AzSceneCacheNodeSnapshot before;
        AzSceneCacheNodeSnapshot after;
        uint8_t is_main;
        uint32_t index;

        if (local_decision.scanned_nodes >=
            AZ_REV1655_SCENE_MAX_CACHE_NODES) {
            local_decision.reason = AZ_SCENE_GATE_REASON_CACHE_LIMIT;
            goto finished;
        }
        for (index = 0u; index < local_decision.scanned_nodes; ++index) {
            if (visited[index] == current) {
                local_decision.reason = AZ_SCENE_GATE_REASON_CACHE_CYCLE;
                goto finished;
            }
        }
        visited[local_decision.scanned_nodes] = current;
        ++local_decision.scanned_nodes;

        if (read_node(current, &before) == 0u) {
            local_decision.reason = AZ_SCENE_GATE_REASON_MEMORY_UNREADABLE;
            goto finished;
        }
        if (path_is_main_scene(before.path, &is_main) == 0u) {
            local_decision.reason = AZ_SCENE_GATE_REASON_PATH_INVALID;
            goto finished;
        }
        if (read_node(current, &after) == 0u) {
            local_decision.reason = AZ_SCENE_GATE_REASON_MEMORY_UNREADABLE;
            goto finished;
        }
        if (nodes_are_equal(&before, &after) == 0u) {
            local_decision.reason = AZ_SCENE_GATE_REASON_CACHE_CHANGED;
            goto finished;
        }

        if (is_main != 0u) {
            ++main_count;
            if (main_count != 1u) {
                local_decision.reason = AZ_SCENE_GATE_REASON_MAIN_DUPLICATE;
                goto finished;
            }
            local_decision.main_scene_node = current;
            local_decision.main_scene_handle = before.handle;
            main_snapshot = before;
        }
        current = before.next;
    }

    if (read_target_u32(
            AZ_REV1655_SCENE_CACHE_HEAD_ADDRESS,
            &cache_head_after) == 0u ||
        read_target_u32(
            AZ_REV1655_SCENE_APP_MANAGER_ADDRESS,
            &manager_vtable_after) == 0u) {
        local_decision.reason = AZ_SCENE_GATE_REASON_MEMORY_UNREADABLE;
        goto finished;
    }
    if (cache_head_after != cache_head ||
        manager_vtable_after != manager_vtable) {
        local_decision.reason = AZ_SCENE_GATE_REASON_CACHE_CHANGED;
        goto finished;
    }
    if (main_count == 0u) {
        local_decision.reason = AZ_SCENE_GATE_REASON_MAIN_NOT_FOUND;
        goto finished;
    }
    if (main_snapshot.acquired != 1u || main_snapshot.handle == 0u) {
        local_decision.reason = AZ_SCENE_GATE_REASON_MAIN_NOT_ACQUIRED;
        goto finished;
    }
    if (g_scene_gate.bindings.xui_handle_is_valid(
            g_scene_gate.bindings.context,
            main_snapshot.handle) != 1) {
        local_decision.reason = AZ_SCENE_GATE_REASON_HANDLE_INVALID;
        goto finished;
    }
    if (g_scene_gate.bindings.xui_element_has_focus(
            g_scene_gate.bindings.context,
            main_snapshot.handle) != 1) {
        local_decision.reason = AZ_SCENE_GATE_REASON_MAIN_NOT_FOCUSED;
        goto finished;
    }

    {
        AzSceneCacheNodeSnapshot final_snapshot;

        if (read_node(
                local_decision.main_scene_node,
                &final_snapshot) == 0u ||
            read_target_u32(
                AZ_REV1655_SCENE_CACHE_HEAD_ADDRESS,
                &cache_head_after) == 0u) {
            local_decision.reason = AZ_SCENE_GATE_REASON_MEMORY_UNREADABLE;
            goto finished;
        }
        if (nodes_are_equal(&main_snapshot, &final_snapshot) == 0u ||
            cache_head_after != cache_head) {
            local_decision.reason = AZ_SCENE_GATE_REASON_CACHE_CHANGED;
            goto finished;
        }
    }

    local_decision.reason = AZ_SCENE_GATE_REASON_MAIN_FOCUSED;
    local_decision.allows_capture = 1u;

finished:
    publish_decision(&local_decision);
    if (decision != NULL) {
        *decision = local_decision;
    }
    return local_decision.allows_capture;
}

void az_rev1655_scene_gate_snapshot_status(AzSceneGateStatus *status)
{
    if (status == NULL) {
        return;
    }

    status->last_configure_result = (AzSceneGateConfigureResult)load_u32(
        &g_scene_gate.last_configure_result);
    status->last_reason = (AzSceneGateReason)load_u32(
        &g_scene_gate.last_reason);
    status->configure_attempts = load_u32(&g_scene_gate.configure_attempts);
    status->configure_successes = load_u32(&g_scene_gate.configure_successes);
    status->static_validation_failures = load_u32(
        &g_scene_gate.static_validation_failures);
    status->probes = load_u32(&g_scene_gate.probes);
    status->allowed = load_u32(&g_scene_gate.allowed);
    status->denied = load_u32(&g_scene_gate.denied);
    status->manager_unavailable = load_u32(
        &g_scene_gate.manager_unavailable);
    status->memory_read_failures = load_u32(
        &g_scene_gate.memory_read_failures);
    status->cache_changed = load_u32(&g_scene_gate.cache_changed);
    status->cache_cycles = load_u32(&g_scene_gate.cache_cycles);
    status->cache_limits = load_u32(&g_scene_gate.cache_limits);
    status->path_failures = load_u32(&g_scene_gate.path_failures);
    status->main_missing = load_u32(&g_scene_gate.main_missing);
    status->main_duplicate = load_u32(&g_scene_gate.main_duplicate);
    status->main_not_acquired = load_u32(
        &g_scene_gate.main_not_acquired);
    status->invalid_handles = load_u32(&g_scene_gate.invalid_handles);
    status->main_not_focused = load_u32(&g_scene_gate.main_not_focused);
    status->last_cache_head = load_u32(&g_scene_gate.last_cache_head);
    status->last_main_scene_node = load_u32(
        &g_scene_gate.last_main_scene_node);
    status->last_main_scene_handle = load_u32(
        &g_scene_gate.last_main_scene_handle);
    status->last_scanned_nodes = load_u32(
        &g_scene_gate.last_scanned_nodes);
    status->configured = load_u32(&g_scene_gate.configured) != 0u ? 1u : 0u;
    status->exact_image_verified = load_u32(
        &g_scene_gate.exact_image_verified) != 0u ? 1u : 0u;
    status->signatures_verified = load_u32(
        &g_scene_gate.signatures_verified) != 0u ? 1u : 0u;
}

size_t az_rev1655_scene_gate_validation_span_count(void)
{
    return AZ_SCENE_GATE_VALIDATION_SPAN_COUNT;
}

uint8_t az_rev1655_scene_gate_validation_span(
    size_t index,
    AzSceneGateValidationSpan *span)
{
    if (span == NULL || index >= AZ_SCENE_GATE_VALIDATION_SPAN_COUNT) {
        return 0u;
    }
    *span = g_validation_spans[index];
    return 1u;
}

const char *az_scene_gate_configure_result_name(
    AzSceneGateConfigureResult result)
{
    switch (result) {
    case AZ_SCENE_GATE_CONFIGURE_OK:
        return "ok";
    case AZ_SCENE_GATE_CONFIGURE_NULL:
        return "null";
    case AZ_SCENE_GATE_CONFIGURE_ALREADY_CONFIGURED:
        return "already-configured";
    case AZ_SCENE_GATE_CONFIGURE_BAD_BINDINGS:
        return "bad-bindings";
    case AZ_SCENE_GATE_CONFIGURE_IMAGE_UNVERIFIED:
        return "image-unverified";
    case AZ_SCENE_GATE_CONFIGURE_SIGNATURE_MISMATCH:
        return "signature-mismatch";
    default:
        return "unknown";
    }
}

const char *az_scene_gate_reason_name(AzSceneGateReason reason)
{
    switch (reason) {
    case AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED:
        return "static-not-verified";
    case AZ_SCENE_GATE_REASON_MANAGER_UNAVAILABLE:
        return "manager-unavailable";
    case AZ_SCENE_GATE_REASON_MEMORY_UNREADABLE:
        return "memory-unreadable";
    case AZ_SCENE_GATE_REASON_CACHE_CHANGED:
        return "cache-changed";
    case AZ_SCENE_GATE_REASON_CACHE_CYCLE:
        return "cache-cycle";
    case AZ_SCENE_GATE_REASON_CACHE_LIMIT:
        return "cache-limit";
    case AZ_SCENE_GATE_REASON_PATH_INVALID:
        return "path-invalid";
    case AZ_SCENE_GATE_REASON_MAIN_NOT_FOUND:
        return "main-not-found";
    case AZ_SCENE_GATE_REASON_MAIN_DUPLICATE:
        return "main-duplicate";
    case AZ_SCENE_GATE_REASON_MAIN_NOT_ACQUIRED:
        return "main-not-acquired";
    case AZ_SCENE_GATE_REASON_HANDLE_INVALID:
        return "handle-invalid";
    case AZ_SCENE_GATE_REASON_MAIN_NOT_FOCUSED:
        return "main-not-focused";
    case AZ_SCENE_GATE_REASON_MAIN_FOCUSED:
        return "main-focused";
    default:
        return "unknown";
    }
}

#if defined(AURORAAZ_XBOX360)
typedef int32_t (*AzSceneGateXuiFunction)(uint32_t object_handle);

static uint8_t xbox_address_range_is_valid(uintptr_t address, size_t size)
{
    if (address == 0u || size == 0u ||
        address > UINTPTR_MAX - (size - 1u)) {
        return 0u;
    }
    return (MmIsAddressValid((void *)address) &&
        MmIsAddressValid((void *)(address + size - 1u))) ? 1u : 0u;
}

static uint8_t xbox_read_bytes(
    void *context,
    uintptr_t address,
    void *destination,
    size_t size)
{
    (void)context;
    if (destination == NULL ||
        xbox_address_range_is_valid(address, size) == 0u) {
        return 0u;
    }
    memcpy(destination, (const void *)address, size);
    return 1u;
}

static uint8_t xbox_read_u32(
    void *context,
    uintptr_t address,
    uint32_t *value)
{
    (void)context;
    if (value == NULL || (address & 3u) != 0u ||
        xbox_address_range_is_valid(address, sizeof(*value)) == 0u) {
        return 0u;
    }
    *value = *(const volatile uint32_t *)address;
    return 1u;
}

static int32_t xbox_handle_is_valid(void *context, uint32_t object_handle)
{
    const AzSceneGateXuiFunction function =
        (AzSceneGateXuiFunction)(uintptr_t)
            AZ_REV1655_XUI_HANDLE_IS_VALID_ADDRESS;
    (void)context;
    return function(object_handle);
}

static int32_t xbox_element_has_focus(void *context, uint32_t object_handle)
{
    const AzSceneGateXuiFunction function =
        (AzSceneGateXuiFunction)(uintptr_t)
            AZ_REV1655_XUI_ELEMENT_HAS_FOCUS_ADDRESS;
    (void)context;
    return function(object_handle);
}

AzSceneGateConfigureResult az_rev1655_scene_gate_configure_default(
    uint8_t exact_image_verified)
{
    AzSceneGateRev1655Bindings bindings;

    memset(&bindings, 0, sizeof(bindings));
    bindings.read_bytes = &xbox_read_bytes;
    bindings.read_u32 = &xbox_read_u32;
    bindings.xui_handle_is_valid = &xbox_handle_is_valid;
    bindings.xui_element_has_focus = &xbox_element_has_focus;
    bindings.exact_image_verified = exact_image_verified != 0u ? 1u : 0u;
    return az_rev1655_scene_gate_configure(&bindings);
}
#endif
