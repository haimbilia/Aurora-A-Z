#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <auroraaz/scene_gate_rev1655.h>

#define TEST_NODE_CAPACITY (AZ_REV1655_SCENE_MAX_CACHE_NODES + 1u)
#define TEST_PATH_CAPACITY TEST_NODE_CAPACITY
#define TEST_NODE_BASE 0x90001000u
#define TEST_NODE_STRIDE 0x20u
#define TEST_PATH_BASE 0x91000000u
#define TEST_PATH_STRIDE 0x400u
#define TEST_MAIN_HANDLE 0x12340001u

typedef struct FakeNode {
    uint32_t address;
    uint32_t path;
    uint32_t handle;
    uint32_t acquired;
    uint32_t next;
} FakeNode;

typedef struct FakePath {
    uint32_t address;
    const char *value;
    uint8_t readable;
} FakePath;

typedef struct FakeScene {
    FakeNode nodes[TEST_NODE_CAPACITY];
    FakePath paths[TEST_PATH_CAPACITY];
    uint32_t node_count;
    uint32_t path_count;
    uint32_t manager_vtable;
    uint32_t cache_head;
    uint32_t expected_handle;
    int32_t handle_valid;
    int32_t has_focus;
    uintptr_t unreadable_u32_address;
    uintptr_t mutate_u32_address;
    uint32_t mutate_u32_after_reads;
    uint32_t mutate_u32_value;
    uint32_t mutate_u32_reads;
    uint8_t corrupt_signature;
    uint8_t unterminated_path;
} FakeScene;

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

static void fake_init(FakeScene *fake)
{
    memset(fake, 0, sizeof(*fake));
    fake->manager_vtable = AZ_REV1655_SCENE_APP_MANAGER_VTABLE;
    fake->expected_handle = TEST_MAIN_HANDLE;
    fake->handle_valid = 1;
    fake->has_focus = 1;
}

static uint32_t fake_add_path(FakeScene *fake, const char *value)
{
    FakePath *path;

    CHECK(fake->path_count < TEST_PATH_CAPACITY);
    path = &fake->paths[fake->path_count];
    path->address = TEST_PATH_BASE + fake->path_count * TEST_PATH_STRIDE;
    path->value = value;
    path->readable = 1u;
    ++fake->path_count;
    return path->address;
}

static uint32_t fake_add_node(
    FakeScene *fake,
    const char *path_value,
    uint32_t handle,
    uint32_t acquired)
{
    FakeNode *node;

    CHECK(fake->node_count < TEST_NODE_CAPACITY);
    node = &fake->nodes[fake->node_count];
    node->address = TEST_NODE_BASE + fake->node_count * TEST_NODE_STRIDE;
    node->path = fake_add_path(fake, path_value);
    node->handle = handle;
    node->acquired = acquired;
    node->next = 0u;
    if (fake->node_count != 0u) {
        fake->nodes[fake->node_count - 1u].next = node->address;
    }
    else {
        fake->cache_head = node->address;
    }
    ++fake->node_count;
    return node->address;
}

static void fake_add_happy_main(FakeScene *fake)
{
    (void)fake_add_node(
        fake,
        "memory://40200030,4F0B6D#Aurora_Main.xur",
        TEST_MAIN_HANDLE,
        1u);
}

static uint8_t fake_read_signature(
    FakeScene *fake,
    uintptr_t address,
    void *destination,
    size_t size)
{
    const size_t span_count =
        az_rev1655_scene_gate_validation_span_count();
    size_t index;

    for (index = 0u; index < span_count; ++index) {
        AzSceneGateValidationSpan span;

        if (az_rev1655_scene_gate_validation_span(index, &span) != 1u) {
            return 0u;
        }
        if (address == span.address && size == span.size) {
            memcpy(destination, span.expected, size);
            if (fake->corrupt_signature != 0u && index == 0u) {
                ((uint8_t *)destination)[0] ^= 0x01u;
            }
            return 1u;
        }
    }
    return 0u;
}

static uint8_t fake_read_bytes(
    void *context,
    uintptr_t address,
    void *destination,
    size_t size)
{
    FakeScene *fake = (FakeScene *)context;
    uint32_t index;

    if (destination == NULL || size == 0u) {
        return 0u;
    }
    if (fake_read_signature(fake, address, destination, size) != 0u) {
        return 1u;
    }

    for (index = 0u; index < fake->path_count; ++index) {
        const FakePath *path = &fake->paths[index];
        const size_t length = strlen(path->value);
        const uintptr_t relative = address - path->address;
        size_t code_unit_index;
        uint16_t code_unit;

        if (path->readable == 0u || address < path->address || size != 2u ||
            (relative & 1u) != 0u ||
            relative / 2u >= AZ_REV1655_SCENE_MAX_PATH_CODE_UNITS) {
            continue;
        }
        code_unit_index = (size_t)(relative / 2u);
        if (code_unit_index < length) {
            code_unit = (uint16_t)(uint8_t)path->value[code_unit_index];
        }
        else if (fake->unterminated_path != 0u) {
            code_unit = (uint16_t)'X';
        }
        else if (code_unit_index == length) {
            code_unit = 0u;
        }
        else {
            continue;
        }
        ((uint8_t *)destination)[0] = (uint8_t)(code_unit >> 8u);
        ((uint8_t *)destination)[1] = (uint8_t)code_unit;
        return 1u;
    }
    return 0u;
}

static uint8_t fake_lookup_u32(
    FakeScene *fake,
    uintptr_t address,
    uint32_t *value)
{
    uint32_t index;

    if (address == AZ_REV1655_SCENE_APP_MANAGER_ADDRESS) {
        *value = fake->manager_vtable;
        return 1u;
    }
    if (address == AZ_REV1655_SCENE_CACHE_HEAD_ADDRESS) {
        *value = fake->cache_head;
        return 1u;
    }
    for (index = 0u; index < fake->node_count; ++index) {
        const FakeNode *node = &fake->nodes[index];

        if (address == node->address +
                AZ_REV1655_SCENE_CACHE_NODE_PATH_OFFSET) {
            *value = node->path;
            return 1u;
        }
        if (address == node->address +
                AZ_REV1655_SCENE_CACHE_NODE_HANDLE_OFFSET) {
            *value = node->handle;
            return 1u;
        }
        if (address == node->address +
                AZ_REV1655_SCENE_CACHE_NODE_ACQUIRED_OFFSET) {
            *value = node->acquired;
            return 1u;
        }
        if (address == node->address +
                AZ_REV1655_SCENE_CACHE_NODE_NEXT_OFFSET) {
            *value = node->next;
            return 1u;
        }
    }
    return 0u;
}

static uint8_t fake_read_u32(
    void *context,
    uintptr_t address,
    uint32_t *value)
{
    FakeScene *fake = (FakeScene *)context;

    if (value == NULL || address == fake->unreadable_u32_address) {
        return 0u;
    }
    if (address == fake->mutate_u32_address) {
        ++fake->mutate_u32_reads;
        if (fake->mutate_u32_reads > fake->mutate_u32_after_reads) {
            *value = fake->mutate_u32_value;
            return 1u;
        }
    }
    return fake_lookup_u32(fake, address, value);
}

static int32_t fake_handle_is_valid(void *context, uint32_t handle)
{
    FakeScene *fake = (FakeScene *)context;
    return handle == fake->expected_handle ? fake->handle_valid : 0;
}

static int32_t fake_has_focus(void *context, uint32_t handle)
{
    FakeScene *fake = (FakeScene *)context;
    return handle == fake->expected_handle ? fake->has_focus : 0;
}

static AzSceneGateRev1655Bindings fake_bindings(
    FakeScene *fake,
    uint8_t exact_image_verified)
{
    AzSceneGateRev1655Bindings bindings;

    memset(&bindings, 0, sizeof(bindings));
    bindings.context = fake;
    bindings.read_bytes = &fake_read_bytes;
    bindings.read_u32 = &fake_read_u32;
    bindings.xui_handle_is_valid = &fake_handle_is_valid;
    bindings.xui_element_has_focus = &fake_has_focus;
    bindings.exact_image_verified = exact_image_verified;
    return bindings;
}

static void configure_happy(FakeScene *fake)
{
    const AzSceneGateRev1655Bindings bindings = fake_bindings(fake, 1u);
    az_rev1655_scene_gate_reset();
    CHECK(az_rev1655_scene_gate_configure(&bindings) ==
        AZ_SCENE_GATE_CONFIGURE_OK);
}

static AzSceneGateDecision run_probe(FakeScene *fake)
{
    AzSceneGateDecision decision;
    (void)fake;
    memset(&decision, 0xA5, sizeof(decision));
    (void)az_rev1655_scene_gate_probe(&decision);
    return decision;
}

static void test_validation_contract(void)
{
    AzSceneGateValidationSpan span;
    FakeScene fake;
    AzSceneGateRev1655Bindings bindings;
    size_t index;

    CHECK(az_rev1655_scene_gate_validation_span_count() == 7u);
    for (index = 0u;
         index < az_rev1655_scene_gate_validation_span_count();
         ++index) {
        CHECK(az_rev1655_scene_gate_validation_span(index, &span) == 1u);
        CHECK(span.address != 0u);
        CHECK(span.expected != NULL);
        CHECK(span.size != 0u);
    }
    CHECK(az_rev1655_scene_gate_validation_span(index, &span) == 0u);
    CHECK(az_rev1655_scene_gate_validation_span(0u, NULL) == 0u);

    az_rev1655_scene_gate_reset();
    CHECK(az_rev1655_scene_gate_configure(NULL) ==
        AZ_SCENE_GATE_CONFIGURE_NULL);

    fake_init(&fake);
    bindings = fake_bindings(&fake, 1u);
    bindings.xui_element_has_focus = NULL;
    CHECK(az_rev1655_scene_gate_configure(&bindings) ==
        AZ_SCENE_GATE_CONFIGURE_BAD_BINDINGS);

    bindings = fake_bindings(&fake, 0u);
    CHECK(az_rev1655_scene_gate_configure(&bindings) ==
        AZ_SCENE_GATE_CONFIGURE_IMAGE_UNVERIFIED);

    fake.corrupt_signature = 1u;
    bindings = fake_bindings(&fake, 1u);
    CHECK(az_rev1655_scene_gate_configure(&bindings) ==
        AZ_SCENE_GATE_CONFIGURE_SIGNATURE_MISMATCH);
    fake.corrupt_signature = 0u;
    CHECK(az_rev1655_scene_gate_configure(&bindings) ==
        AZ_SCENE_GATE_CONFIGURE_OK);
    CHECK(az_rev1655_scene_gate_configure(&bindings) ==
        AZ_SCENE_GATE_CONFIGURE_ALREADY_CONFIGURED);
}

static void test_unconfigured_fails_closed(void)
{
    AzSceneGateDecision decision;
    AzSceneGateStatus status;

    az_rev1655_scene_gate_reset();
    CHECK(az_rev1655_scene_gate_probe(&decision) == 0u);
    CHECK(decision.allows_capture == 0u);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_STATIC_NOT_VERIFIED);
    az_rev1655_scene_gate_snapshot_status(&status);
    CHECK(status.probes == 1u);
    CHECK(status.denied == 1u);
    CHECK(status.configured == 0u);
}

static void test_main_focus_allows_capture(void)
{
    AzSceneGateDecision decision;
    AzSceneGateStatus status;
    FakeScene fake;

    fake_init(&fake);
    (void)fake_add_node(&fake, "skin://Default/Other.xur", 0x100u, 1u);
    fake_add_happy_main(&fake);
    configure_happy(&fake);

    CHECK(az_rev1655_scene_gate_probe(&decision) == 1u);
    CHECK(decision.allows_capture == 1u);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_MAIN_FOCUSED);
    CHECK(decision.cache_head == fake.nodes[0].address);
    CHECK(decision.main_scene_node == fake.nodes[1].address);
    CHECK(decision.main_scene_handle == TEST_MAIN_HANDLE);
    CHECK(decision.scanned_nodes == 2u);
    CHECK(az_rev1655_scene_gate_probe(NULL) == 1u);

    az_rev1655_scene_gate_snapshot_status(&status);
    CHECK(status.probes == 2u);
    CHECK(status.allowed == 2u);
    CHECK(status.denied == 0u);
    CHECK(status.last_reason == AZ_SCENE_GATE_REASON_MAIN_FOCUSED);
    CHECK(status.last_main_scene_handle == TEST_MAIN_HANDLE);
    CHECK(status.configured == 1u);
    CHECK(status.exact_image_verified == 1u);
    CHECK(status.signatures_verified == 1u);
}

static void test_modal_focus_denies_capture(void)
{
    AzSceneGateDecision decision;
    AzSceneGateStatus status;
    FakeScene fake;

    fake_init(&fake);
    fake_add_happy_main(&fake);
    fake.has_focus = 0;
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.allows_capture == 0u);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_MAIN_NOT_FOCUSED);
    az_rev1655_scene_gate_snapshot_status(&status);
    CHECK(status.main_not_focused == 1u);
}

static void test_manager_and_memory_fail_closed(void)
{
    AzSceneGateDecision decision;
    AzSceneGateStatus status;
    FakeScene fake;

    fake_init(&fake);
    fake_add_happy_main(&fake);
    fake.manager_vtable ^= 4u;
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_MANAGER_UNAVAILABLE);
    CHECK(decision.allows_capture == 0u);
    az_rev1655_scene_gate_snapshot_status(&status);
    CHECK(status.manager_unavailable == 1u);
    CHECK(status.memory_read_failures == 0u);

    fake_init(&fake);
    fake_add_happy_main(&fake);
    fake.unreadable_u32_address = AZ_REV1655_SCENE_CACHE_HEAD_ADDRESS;
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_MEMORY_UNREADABLE);
    az_rev1655_scene_gate_snapshot_status(&status);
    CHECK(status.memory_read_failures == 1u);
}

static void test_path_and_lookup_fail_closed(void)
{
    AzSceneGateDecision decision;
    FakeScene fake;

    fake_init(&fake);
    (void)fake_add_node(&fake, "Aurora_Main.xur.backup", TEST_MAIN_HANDLE, 1u);
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_MAIN_NOT_FOUND);

    fake_init(&fake);
    (void)fake_add_node(&fake, "NotAurora_Main.xur", TEST_MAIN_HANDLE, 1u);
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_MAIN_NOT_FOUND);

    fake_init(&fake);
    (void)fake_add_node(&fake, "Aurora_Main.xur", TEST_MAIN_HANDLE, 1u);
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_MAIN_FOCUSED);

    fake_init(&fake);
    fake_add_happy_main(&fake);
    fake.paths[0].readable = 0u;
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_PATH_INVALID);

    fake_init(&fake);
    fake_add_happy_main(&fake);
    fake.unterminated_path = 1u;
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_PATH_INVALID);
}

static void test_duplicate_cycle_and_limit_fail_closed(void)
{
    AzSceneGateDecision decision;
    FakeScene fake;
    uint32_t index;

    fake_init(&fake);
    fake_add_happy_main(&fake);
    (void)fake_add_node(
        &fake,
        "skin://Another/Aurora_Main.xur",
        TEST_MAIN_HANDLE,
        1u);
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_MAIN_DUPLICATE);

    fake_init(&fake);
    (void)fake_add_node(&fake, "One.xur", 1u, 1u);
    (void)fake_add_node(&fake, "Two.xur", 2u, 1u);
    fake.nodes[1].next = fake.nodes[0].address;
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_CACHE_CYCLE);

    fake_init(&fake);
    for (index = 0u; index < TEST_NODE_CAPACITY; ++index) {
        (void)fake_add_node(&fake, "Other.xur", index + 1u, 1u);
    }
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_CACHE_LIMIT);
    CHECK(decision.scanned_nodes == AZ_REV1655_SCENE_MAX_CACHE_NODES);
}

static void test_acquisition_and_handle_fail_closed(void)
{
    AzSceneGateDecision decision;
    FakeScene fake;

    fake_init(&fake);
    (void)fake_add_node(&fake, "Aurora_Main.xur", TEST_MAIN_HANDLE, 0u);
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_MAIN_NOT_ACQUIRED);

    fake_init(&fake);
    (void)fake_add_node(&fake, "Aurora_Main.xur", 0u, 1u);
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_MAIN_NOT_ACQUIRED);

    fake_init(&fake);
    fake_add_happy_main(&fake);
    fake.handle_valid = 0;
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_HANDLE_INVALID);
}

static void test_concurrent_changes_fail_closed(void)
{
    AzSceneGateDecision decision;
    FakeScene fake;

    fake_init(&fake);
    fake_add_happy_main(&fake);
    fake.mutate_u32_address = fake.nodes[0].address +
        AZ_REV1655_SCENE_CACHE_NODE_NEXT_OFFSET;
    fake.mutate_u32_after_reads = 1u;
    fake.mutate_u32_value = 0x90009900u;
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_CACHE_CHANGED);

    fake_init(&fake);
    fake_add_happy_main(&fake);
    fake.mutate_u32_address = AZ_REV1655_SCENE_CACHE_HEAD_ADDRESS;
    fake.mutate_u32_after_reads = 1u;
    fake.mutate_u32_value = 0u;
    configure_happy(&fake);
    decision = run_probe(&fake);
    CHECK(decision.reason == AZ_SCENE_GATE_REASON_CACHE_CHANGED);
}

static void test_names_and_null_status(void)
{
    CHECK(strcmp(
        az_scene_gate_configure_result_name(AZ_SCENE_GATE_CONFIGURE_OK),
        "ok") == 0);
    CHECK(strcmp(
        az_scene_gate_configure_result_name((AzSceneGateConfigureResult)99),
        "unknown") == 0);
    CHECK(strcmp(
        az_scene_gate_reason_name(AZ_SCENE_GATE_REASON_MAIN_FOCUSED),
        "main-focused") == 0);
    CHECK(strcmp(
        az_scene_gate_reason_name((AzSceneGateReason)99),
        "unknown") == 0);
    az_rev1655_scene_gate_snapshot_status(NULL);
}

int main(void)
{
    test_validation_contract();
    test_unconfigured_fails_closed();
    test_main_focus_allows_capture();
    test_modal_focus_denies_capture();
    test_manager_and_memory_fail_closed();
    test_path_and_lookup_fail_closed();
    test_duplicate_cycle_and_limit_fail_closed();
    test_acquisition_and_handle_fail_closed();
    test_concurrent_changes_fail_closed();
    test_names_and_null_status();

    if (g_failures != 0u) {
        (void)fprintf(stderr, "%u scene-gate test(s) failed\n", g_failures);
        return 1;
    }
    (void)puts("scene_gate_rev1655_tests: all checks passed");
    return 0;
}
