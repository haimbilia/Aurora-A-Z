#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>

#if !defined(MAP_FIXED_NOREPLACE)
#define MAP_FIXED_NOREPLACE MAP_FIXED
#endif

#define MEM_COMMIT 0x00001000u
#define MEM_RESERVE 0x00002000u
#define MEM_RELEASE 0x00008000u
#define PAGE_EXECUTE_READWRITE 0x00000040u
#endif

#include <auroraaz/hook_runtime.h>
#include <auroraaz/ppc.h>

#define TEST_TARGET_ADDRESS 0x82C00000u
#define TEST_SECOND_TARGET_ADDRESS 0x82C10000u
#define TEST_DETOUR_ADDRESS 0x91D02000u
#define TEST_ORIGINAL_INSTRUCTION 0x60000000u
#define TEST_OLD_PROTECT 0x00000020u
#define TEST_ALLOCATION_GRANULARITY 0x00010000u
#define TEST_STATUS_SUCCESS 0x00000000u
#define TEST_STATUS_UNSUCCESSFUL 0xC0000001u
#define TEST_DEBUG_MEMORY_FALSE 0
#define TEST_EVENT_CAPACITY 2048u
#define TEST_MAPPING_CAPACITY 64u
#define TEST_ALLOCATION_STEP_CAPACITY 16u
#define TEST_ADMITTED_RELAY_WORDS 21u
#define TEST_RESIDENT_EXIT_WORDS 7u

typedef enum TestEventType {
    TEST_EVENT_ALLOCATE = 1,
    TEST_EVENT_FREE,
    TEST_EVENT_QUERY_PROTECT,
    TEST_EVENT_SET_PROTECT,
    TEST_EVENT_SWEEP_DCACHE,
    TEST_EVENT_SWEEP_ICACHE
} TestEventType;

typedef struct TestEvent {
    TestEventType type;
    uintptr_t address;
    uint32_t size;
    uint32_t value;
    int region;
} TestEvent;

typedef enum TestAllocationOutcome {
    TEST_ALLOCATE_FAIL = 0,
    TEST_ALLOCATE_EXACT,
    TEST_ALLOCATE_WRONG_BASE,
    TEST_ALLOCATE_TOO_SMALL,
    TEST_ALLOCATE_OVERSIZE
} TestAllocationOutcome;

typedef struct TestMapping {
    void *base;
    size_t size;
    uint8_t live;
} TestMapping;

static int failures;
static TestEvent events[TEST_EVENT_CAPACITY];
static size_t event_count;
static TestMapping mappings[TEST_MAPPING_CAPACITY];
static TestAllocationOutcome allocation_steps[
    TEST_ALLOCATION_STEP_CAPACITY];
static size_t allocation_step_count;
static size_t allocation_call_count;
static TestAllocationOutcome default_allocation_outcome;
static uint32_t free_status;
static uintptr_t valid_target_address;
static uint8_t target_is_valid;
static uint32_t query_protect_result;
static uint8_t mutate_on_next_write_enable;
static uint32_t mutate_on_next_write_value;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

static void push_event(
    TestEventType type,
    uintptr_t address,
    uint32_t size,
    uint32_t value,
    int region)
{
    if (event_count >= TEST_EVENT_CAPACITY) {
        CHECK(event_count < TEST_EVENT_CAPACITY);
        return;
    }

    events[event_count].type = type;
    events[event_count].address = address;
    events[event_count].size = size;
    events[event_count].value = value;
    events[event_count].region = region;
    ++event_count;
}

static void clear_events(void)
{
    memset(events, 0, sizeof(events));
    event_count = 0u;
}

static void register_mapping(void *base, size_t size)
{
    size_t index;

    if (base == NULL) {
        return;
    }
    for (index = 0u; index < TEST_MAPPING_CAPACITY; ++index) {
        if (mappings[index].live == 0u) {
            mappings[index].base = base;
            mappings[index].size = size;
            mappings[index].live = 1u;
            return;
        }
    }
    CHECK(0);
}

static int release_mapping(void *base)
{
    size_t index;

    for (index = 0u; index < TEST_MAPPING_CAPACITY; ++index) {
        if (mappings[index].live != 0u && mappings[index].base == base) {
#if defined(_WIN32)
            const int released =
                VirtualFree(base, 0u, MEM_RELEASE) != 0 ? 1 : 0;
#else
            const int released =
                munmap(base, mappings[index].size) == 0 ? 1 : 0;
#endif

            if (released == 0) {
                return 0;
            }
            mappings[index].base = NULL;
            mappings[index].size = 0u;
            mappings[index].live = 0u;
            return 1;
        }
    }
    CHECK(0);
    return 0;
}

static void release_all_test_mappings(void)
{
    size_t index;

    for (index = 0u; index < TEST_MAPPING_CAPACITY; ++index) {
        if (mappings[index].live != 0u) {
            CHECK(release_mapping(mappings[index].base) != 0);
        }
    }
}

static void *map_exact(uint32_t address, size_t size)
{
    void *requested = (void *)(uintptr_t)address;
#if defined(_WIN32)
    void *mapped = VirtualAlloc(
        requested,
        size,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_EXECUTE_READWRITE);
#else
    void *mapped = mmap(
        requested,
        size,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
        -1,
        0);

    if (mapped == MAP_FAILED) {
        mapped = NULL;
    }
#endif

    CHECK(mapped == requested);
    if (mapped != NULL) {
        register_mapping(mapped, size);
    }
    return mapped;
}

static volatile uint32_t *map_target(
    uint32_t address,
    uint32_t instruction)
{
    volatile uint32_t *target = (volatile uint32_t *)map_exact(
        address,
        AZ_HOOK_ARENA_SIZE);

    if (target != NULL) {
        *target = instruction;
        valid_target_address = (uintptr_t)address;
        target_is_valid = 1u;
    }
    return target;
}

static void reset_stubs(void)
{
    release_all_test_mappings();
    clear_events();
    memset(allocation_steps, 0, sizeof(allocation_steps));
    allocation_step_count = 0u;
    allocation_call_count = 0u;
    default_allocation_outcome = TEST_ALLOCATE_FAIL;
    free_status = TEST_STATUS_SUCCESS;
    valid_target_address = (uintptr_t)0u;
    target_is_valid = 0u;
    query_protect_result = TEST_OLD_PROTECT;
    mutate_on_next_write_enable = 0u;
    mutate_on_next_write_value = 0u;
}

static void set_allocation_steps(
    const TestAllocationOutcome *steps,
    size_t count)
{
    CHECK(count <= TEST_ALLOCATION_STEP_CAPACITY);
    if (count > TEST_ALLOCATION_STEP_CAPACITY) {
        count = TEST_ALLOCATION_STEP_CAPACITY;
    }
    if (count != 0u) {
        memcpy(allocation_steps, steps, count * sizeof(steps[0]));
    }
    allocation_step_count = count;
    allocation_call_count = 0u;
}

static TestAllocationOutcome next_allocation_outcome(void)
{
    TestAllocationOutcome outcome = default_allocation_outcome;

    if (allocation_call_count < allocation_step_count) {
        outcome = allocation_steps[allocation_call_count];
    }
    ++allocation_call_count;
    return outcome;
}

uint32_t NtAllocateVirtualMemory(
    void **base_address_ptr,
    uint32_t *region_size_ptr,
    uint32_t alloc_type,
    uint32_t protect_bits,
    int region)
{
    TestAllocationOutcome outcome;
    uintptr_t requested_address;
    uint32_t requested_size;
    uint32_t mapped_address;
    uint32_t returned_size;
    void *mapped;

    CHECK(base_address_ptr != NULL);
    CHECK(region_size_ptr != NULL);
    if (base_address_ptr == NULL || region_size_ptr == NULL) {
        return TEST_STATUS_UNSUCCESSFUL;
    }

    requested_address = (uintptr_t)*base_address_ptr;
    requested_size = *region_size_ptr;
    push_event(
        TEST_EVENT_ALLOCATE,
        requested_address,
        requested_size,
        alloc_type | protect_bits,
        region);
    CHECK(alloc_type == (MEM_RESERVE | MEM_COMMIT));
    CHECK(protect_bits == PAGE_EXECUTE_READWRITE);
    CHECK(region == TEST_DEBUG_MEMORY_FALSE);

    outcome = next_allocation_outcome();
    if (outcome == TEST_ALLOCATE_FAIL) {
        return TEST_STATUS_UNSUCCESSFUL;
    }

    mapped_address = (uint32_t)requested_address;
    returned_size = requested_size;
    if (outcome == TEST_ALLOCATE_WRONG_BASE) {
        mapped_address += TEST_ALLOCATION_GRANULARITY;
    }
    else if (outcome == TEST_ALLOCATE_TOO_SMALL) {
        returned_size = AZ_HOOK_ARENA_SIZE - 1u;
    }
    else if (outcome == TEST_ALLOCATE_OVERSIZE) {
        returned_size = AZ_HOOK_ARENA_SIZE * 2u;
    }

    mapped = map_exact(mapped_address, (size_t)returned_size);
    if (mapped == NULL) {
        return TEST_STATUS_UNSUCCESSFUL;
    }
    *base_address_ptr = mapped;
    *region_size_ptr = returned_size;
    return TEST_STATUS_SUCCESS;
}

uint32_t NtFreeVirtualMemory(
    void **base_address_ptr,
    uint32_t *region_size_ptr,
    uint32_t free_type,
    int region)
{
    void *base;

    CHECK(base_address_ptr != NULL);
    CHECK(region_size_ptr != NULL);
    if (base_address_ptr == NULL || region_size_ptr == NULL) {
        return TEST_STATUS_UNSUCCESSFUL;
    }

    base = *base_address_ptr;
    push_event(
        TEST_EVENT_FREE,
        (uintptr_t)base,
        *region_size_ptr,
        free_type,
        region);
    CHECK(*region_size_ptr == 0u);
    CHECK(free_type == MEM_RELEASE);
    CHECK(region == TEST_DEBUG_MEMORY_FALSE);

    if (free_status != TEST_STATUS_SUCCESS) {
        return free_status;
    }
    CHECK(base != NULL);
    if (base != NULL) {
        CHECK(release_mapping(base) != 0);
    }
    return TEST_STATUS_SUCCESS;
}

bool MmIsAddressValid(void *address)
{
    return target_is_valid != 0u &&
        (uintptr_t)address == valid_target_address;
}

uint32_t MmQueryAddressProtect(void *base_address)
{
    push_event(
        TEST_EVENT_QUERY_PROTECT,
        (uintptr_t)base_address,
        0u,
        query_protect_result,
        0);
    return query_protect_result;
}

void MmSetAddressProtect(
    void *base_address,
    uint32_t region_size,
    uint32_t protect_bits)
{
    push_event(
        TEST_EVENT_SET_PROTECT,
        (uintptr_t)base_address,
        region_size,
        protect_bits,
        0);
    CHECK(region_size == sizeof(uint32_t));

    if (protect_bits == PAGE_EXECUTE_READWRITE &&
        mutate_on_next_write_enable != 0u) {
        volatile uint32_t *target = (volatile uint32_t *)base_address;

        *target = mutate_on_next_write_value;
        mutate_on_next_write_enable = 0u;
    }
}

void KeSweepDcacheRange(void *address, uint32_t size)
{
    push_event(
        TEST_EVENT_SWEEP_DCACHE,
        (uintptr_t)address,
        size,
        0u,
        0);
}

void KeSweepIcacheRange(void *address, uint32_t size)
{
    push_event(
        TEST_EVENT_SWEEP_ICACHE,
        (uintptr_t)address,
        size,
        0u,
        0);
}

static AzHookRuntimeResult create_arena(AzHookArena *arena)
{
    const TestAllocationOutcome steps[] = { TEST_ALLOCATE_EXACT };

    set_allocation_steps(steps, sizeof(steps) / sizeof(steps[0]));
    return az_hook_arena_create_rev1655(arena);
}

static AzHookRuntimeResult install_default(
    AzHookArena *arena,
    AzLiveHook *hook)
{
    return az_live_hook_install(
        arena,
        TEST_TARGET_ADDRESS,
        TEST_ORIGINAL_INSTRUCTION,
        (const void *)(uintptr_t)TEST_DETOUR_ADDRESS,
        hook);
}

static volatile uint32_t *hook_admission_words(const AzLiveHook *hook)
{
    return (volatile uint32_t *)hook->admission_address;
}

static void check_hook_cleared(const AzLiveHook *hook)
{
    size_t index;

    CHECK(hook->plan.target_address == 0u);
    CHECK(hook->plan.relay_address == 0u);
    CHECK(hook->plan.trampoline_address == 0u);
    CHECK(hook->plan.detour_address == 0u);
    CHECK(hook->plan.original_instruction == 0u);
    CHECK(hook->plan.target_branch == 0u);
    for (index = 0u; index < AZ_HOOK_RELAY_WORDS; ++index) {
        CHECK(hook->plan.relay[index] == 0u);
    }
    for (index = 0u; index < AZ_HOOK_TRAMPOLINE_WORDS; ++index) {
        CHECK(hook->plan.trampoline[index] == 0u);
    }
    CHECK(hook->admission_address == (uintptr_t)0u);
    CHECK(hook->old_protect == 0u);
    CHECK(hook->installed == 0u);
    CHECK(hook->target_restored == 0u);
}

static size_t count_events(TestEventType type)
{
    size_t index;
    size_t count = 0u;

    for (index = 0u; index < event_count; ++index) {
        if (events[index].type == type) {
            ++count;
        }
    }
    return count;
}

static void check_event(
    size_t index,
    TestEventType type,
    uintptr_t address,
    uint32_t size,
    uint32_t value)
{
    CHECK(index < event_count);
    if (index >= event_count) {
        return;
    }
    CHECK(events[index].type == type);
    CHECK(events[index].address == address);
    CHECK(events[index].size == size);
    CHECK(events[index].value == value);
}

static void test_arena_argument_and_exhaustion_paths(void)
{
    AzHookArena arena;
    size_t expected_attempts;

    reset_stubs();
    CHECK(az_hook_arena_create_rev1655(NULL) == AZ_HOOK_RUNTIME_NULL);
    CHECK(event_count == 0u);

    memset(&arena, 0xA5, sizeof(arena));
    CHECK(az_hook_arena_create_rev1655(&arena) ==
        AZ_HOOK_RUNTIME_NO_NEAR_MEMORY);
    CHECK(arena.base == (uintptr_t)0u);
    CHECK(arena.size == 0u);
    CHECK(arena.used == 0u);
    expected_attempts = (size_t)(
        (AZ_REV1655_HOOK_ARENA_END - AZ_HOOK_ARENA_SIZE -
            AZ_REV1655_HOOK_ARENA_START) /
        TEST_ALLOCATION_GRANULARITY) + 1u;
    CHECK(allocation_call_count == expected_attempts);
    CHECK(count_events(TEST_EVENT_ALLOCATE) == expected_attempts);
    CHECK(count_events(TEST_EVENT_FREE) == 0u);
    CHECK(events[0].address == AZ_REV1655_HOOK_ARENA_START);
    CHECK(events[event_count - 1u].address ==
        AZ_REV1655_HOOK_ARENA_START +
            (expected_attempts - 1u) * TEST_ALLOCATION_GRANULARITY);
}

static void test_arena_rejects_bad_allocations_then_accepts(void)
{
    const TestAllocationOutcome steps[] = {
        TEST_ALLOCATE_FAIL,
        TEST_ALLOCATE_WRONG_BASE,
        TEST_ALLOCATE_TOO_SMALL,
        TEST_ALLOCATE_OVERSIZE
    };
    AzHookArena arena;

    reset_stubs();
    set_allocation_steps(steps, sizeof(steps) / sizeof(steps[0]));
    CHECK(az_hook_arena_create_rev1655(&arena) == AZ_HOOK_RUNTIME_OK);
    CHECK(allocation_call_count == 4u);
    CHECK(arena.base ==
        (uintptr_t)(AZ_REV1655_HOOK_ARENA_START +
            3u * TEST_ALLOCATION_GRANULARITY));
    CHECK(arena.size == AZ_HOOK_ARENA_SIZE * 2u);
    CHECK(arena.used == 0u);
    CHECK(count_events(TEST_EVENT_ALLOCATE) == 4u);
    CHECK(count_events(TEST_EVENT_FREE) == 2u);
    CHECK(events[2].type == TEST_EVENT_FREE);
    CHECK(events[2].address ==
        AZ_REV1655_HOOK_ARENA_START +
            2u * TEST_ALLOCATION_GRANULARITY);
    CHECK(events[4].type == TEST_EVENT_FREE);
    CHECK(events[4].address ==
        AZ_REV1655_HOOK_ARENA_START +
            2u * TEST_ALLOCATION_GRANULARITY);

    clear_events();
    CHECK(az_hook_arena_release_uninstalled(&arena) ==
        AZ_HOOK_RUNTIME_OK);
    CHECK(arena.base == (uintptr_t)0u);
    CHECK(arena.size == 0u);
    CHECK(arena.used == 0u);
    CHECK(event_count == 1u);
    CHECK(events[0].type == TEST_EVENT_FREE);
}

static void test_arena_release_contract(void)
{
    AzHookArena arena;

    reset_stubs();
    CHECK(az_hook_arena_release_uninstalled(NULL) ==
        AZ_HOOK_RUNTIME_NULL);

    memset(&arena, 0, sizeof(arena));
    CHECK(az_hook_arena_release_uninstalled(&arena) ==
        AZ_HOOK_RUNTIME_OK);
    CHECK(event_count == 0u);

    CHECK(create_arena(&arena) == AZ_HOOK_RUNTIME_OK);
    arena.used = AZ_HOOK_SLOT_SIZE;
    clear_events();
    CHECK(az_hook_arena_release_uninstalled(&arena) ==
        AZ_HOOK_RUNTIME_TARGET_CHANGED);
    CHECK(arena.base != (uintptr_t)0u);
    CHECK(event_count == 0u);

    arena.used = 0u;
    free_status = TEST_STATUS_UNSUCCESSFUL;
    CHECK(az_hook_arena_release_uninstalled(&arena) ==
        AZ_HOOK_RUNTIME_OK);
    CHECK(arena.base == (uintptr_t)0u);
    CHECK(arena.size == 0u);
    CHECK(arena.used == 0u);
    CHECK(count_events(TEST_EVENT_FREE) == 1u);
}

static void test_install_argument_and_arena_guards(void)
{
    AzHookArena arena;
    AzLiveHook hook;

    reset_stubs();
    memset(&arena, 0, sizeof(arena));
    memset(&hook, 0xA5, sizeof(hook));
    CHECK(az_live_hook_install(
        NULL,
        TEST_TARGET_ADDRESS,
        TEST_ORIGINAL_INSTRUCTION,
        (const void *)(uintptr_t)TEST_DETOUR_ADDRESS,
        &hook) == AZ_HOOK_RUNTIME_NULL);
    CHECK(az_live_hook_install(
        &arena,
        TEST_TARGET_ADDRESS,
        TEST_ORIGINAL_INSTRUCTION,
        NULL,
        &hook) == AZ_HOOK_RUNTIME_NULL);
    CHECK(az_live_hook_install(
        &arena,
        TEST_TARGET_ADDRESS,
        TEST_ORIGINAL_INSTRUCTION,
        (const void *)(uintptr_t)TEST_DETOUR_ADDRESS,
        NULL) == AZ_HOOK_RUNTIME_NULL);

    CHECK(az_live_hook_install(
        &arena,
        TEST_TARGET_ADDRESS,
        TEST_ORIGINAL_INSTRUCTION,
        (const void *)(uintptr_t)TEST_DETOUR_ADDRESS,
        &hook) == AZ_HOOK_RUNTIME_ARENA_FULL);
    check_hook_cleared(&hook);

    arena.base = AZ_REV1655_HOOK_ARENA_START;
    arena.size = AZ_HOOK_SLOT_SIZE - 1u;
    arena.used = 0u;
    memset(&hook, 0xA5, sizeof(hook));
    CHECK(install_default(&arena, &hook) == AZ_HOOK_RUNTIME_ARENA_FULL);
    check_hook_cleared(&hook);

    arena.size = AZ_HOOK_ARENA_SIZE;
    arena.used = arena.size - AZ_HOOK_SLOT_SIZE + 1u;
    memset(&hook, 0xA5, sizeof(hook));
    CHECK(install_default(&arena, &hook) == AZ_HOOK_RUNTIME_ARENA_FULL);
    check_hook_cleared(&hook);
    CHECK(event_count == 0u);
}

static void test_install_target_and_plan_failures_are_non_mutating(void)
{
    AzHookArena arena;
    AzLiveHook hook;
    volatile uint32_t *target;

    reset_stubs();
    CHECK(create_arena(&arena) == AZ_HOOK_RUNTIME_OK);
    clear_events();
    memset(&hook, 0xA5, sizeof(hook));
    CHECK(az_live_hook_install(
        &arena,
        TEST_TARGET_ADDRESS + 2u,
        TEST_ORIGINAL_INSTRUCTION,
        (const void *)(uintptr_t)TEST_DETOUR_ADDRESS,
        &hook) == AZ_HOOK_RUNTIME_BAD_TARGET);
    check_hook_cleared(&hook);
    CHECK(arena.used == 0u);
    CHECK(event_count == 0u);

    memset(&hook, 0xA5, sizeof(hook));
    CHECK(install_default(&arena, &hook) == AZ_HOOK_RUNTIME_BAD_TARGET);
    check_hook_cleared(&hook);
    CHECK(arena.used == 0u);
    CHECK(event_count == 0u);

    target = map_target(TEST_TARGET_ADDRESS, 0x60000001u);
    CHECK(target != NULL);
    memset(&hook, 0xA5, sizeof(hook));
    CHECK(install_default(&arena, &hook) ==
        AZ_HOOK_RUNTIME_TARGET_CHANGED);
    check_hook_cleared(&hook);
    CHECK(arena.used == 0u);
    CHECK(event_count == 0u);

    *target = TEST_ORIGINAL_INSTRUCTION;
    memset(&hook, 0xA5, sizeof(hook));
    CHECK(az_live_hook_install(
        &arena,
        TEST_TARGET_ADDRESS,
        TEST_ORIGINAL_INSTRUCTION,
        (const void *)(uintptr_t)(TEST_DETOUR_ADDRESS + 2u),
        &hook) == AZ_HOOK_RUNTIME_PLAN_FAILED);
    check_hook_cleared(&hook);
    CHECK(arena.used == 0u);
    CHECK(event_count == 0u);

    *target = 0x40000000u;
    memset(&hook, 0xA5, sizeof(hook));
    CHECK(az_live_hook_install(
        &arena,
        TEST_TARGET_ADDRESS,
        0x40000000u,
        (const void *)(uintptr_t)TEST_DETOUR_ADDRESS,
        &hook) == AZ_HOOK_RUNTIME_PLAN_FAILED);
    check_hook_cleared(&hook);
    CHECK(arena.used == 0u);
    CHECK(event_count == 0u);

    arena.base = (uintptr_t)0x10000000u;
    arena.size = AZ_HOOK_ARENA_SIZE;
    arena.used = 0u;
    *target = TEST_ORIGINAL_INSTRUCTION;
    memset(&hook, 0xA5, sizeof(hook));
    CHECK(install_default(&arena, &hook) == AZ_HOOK_RUNTIME_PLAN_FAILED);
    check_hook_cleared(&hook);
    CHECK(arena.used == 0u);
    CHECK(event_count == 0u);
}

static void test_successful_install_emits_resident_code(void)
{
    static const uint32_t expected_exit[TEST_RESIDENT_EXIT_WORDS] = {
        0x7D405828u,
        0x394AFFFFu,
        0x7D40592Du,
        0x4082FFF4u,
        0x7C0004ACu,
        0x7D8803A6u,
        0x4E800020u
    };
    AzHookArena arena;
    AzLiveHook hook;
    volatile uint32_t *target;
    const uint32_t *relay;
    const uint32_t *trampoline;
    const uint32_t *resident_exit;
    volatile uint32_t *admission;
    uint32_t detour_branch[AZ_PPC_ABSOLUTE_BRANCH_WORDS];
    uint32_t expected_tail;
    size_t index;
    uintptr_t slot;

    reset_stubs();
    CHECK(create_arena(&arena) == AZ_HOOK_RUNTIME_OK);
    target = map_target(TEST_TARGET_ADDRESS, TEST_ORIGINAL_INSTRUCTION);
    CHECK(target != NULL);
    slot = arena.base;
    clear_events();
    memset(&hook, 0xA5, sizeof(hook));

    CHECK(install_default(&arena, &hook) == AZ_HOOK_RUNTIME_OK);
    CHECK(arena.used == AZ_HOOK_SLOT_SIZE);
    CHECK(hook.plan.target_address == TEST_TARGET_ADDRESS);
    CHECK(hook.plan.relay_address == (uint32_t)slot);
    CHECK(hook.plan.trampoline_address ==
        (uint32_t)slot + AZ_HOOK_TRAMPOLINE_OFFSET);
    CHECK(hook.plan.detour_address == TEST_DETOUR_ADDRESS);
    CHECK(hook.plan.original_instruction == TEST_ORIGINAL_INSTRUCTION);
    CHECK(*target == hook.plan.target_branch);
    CHECK(hook.admission_address ==
        slot + AZ_HOOK_ADMISSION_OFFSET);
    CHECK(hook.old_protect == TEST_OLD_PROTECT);
    CHECK(hook.installed == 1u);
    CHECK(hook.target_restored == 0u);

    relay = (const uint32_t *)slot;
    CHECK(relay[0] == (0x3D600000u |
        (((uint32_t)hook.admission_address >> 16u) & 0xFFFFu)));
    CHECK(relay[1] == (0x616B0000u |
        ((uint32_t)hook.admission_address & 0xFFFFu)));
    CHECK(relay[2] == 0x7D805828u);
    CHECK(relay[3] == 0x398C0001u);
    CHECK(relay[4] == 0x7D80592Du);
    CHECK(relay[5] == 0x4082FFF4u);
    CHECK(relay[6] == 0x7C0004ACu);
    CHECK(relay[7] == 0x818B0004u);
    CHECK(relay[8] == 0x2C0C0000u);
    CHECK(relay[9] == 0x41820018u);
    CHECK(relay[10] == 0x7D605B78u);
    CHECK(az_ppc_emit_absolute_branch(
        TEST_DETOUR_ADDRESS,
        0u,
        detour_branch) == AZ_PPC_OK);
    for (index = 0u; index < AZ_PPC_ABSOLUTE_BRANCH_WORDS; ++index) {
        CHECK(relay[11u + index] == detour_branch[index]);
    }
    CHECK(relay[15] == 0x7D805828u);
    CHECK(relay[16] == 0x398CFFFFu);
    CHECK(relay[17] == 0x7D80592Du);
    CHECK(relay[18] == 0x4082FFF4u);
    CHECK(relay[19] == 0x7C0004ACu);
    CHECK(az_ppc_encode_relative_branch(
        (uint32_t)slot + 20u * (uint32_t)sizeof(uint32_t),
        (uint32_t)slot + AZ_HOOK_TRAMPOLINE_OFFSET,
        0u,
        &expected_tail) == AZ_PPC_OK);
    CHECK(relay[20] == expected_tail);

    trampoline = (const uint32_t *)(
        slot + AZ_HOOK_TRAMPOLINE_OFFSET);
    for (index = 0u; index < AZ_HOOK_TRAMPOLINE_WORDS; ++index) {
        CHECK(trampoline[index] == hook.plan.trampoline[index]);
    }
    resident_exit = (const uint32_t *)(
        slot + AZ_HOOK_RESIDENT_EXIT_OFFSET);
    for (index = 0u; index < TEST_RESIDENT_EXIT_WORDS; ++index) {
        CHECK(resident_exit[index] == expected_exit[index]);
    }
    /* addi treats RA=0 as the literal value zero. Keep both operands on the
     * same nonzero volatile scratch register so this is a real decrement. */
    CHECK(((resident_exit[1] >> 26u) & 0x3Fu) == 14u);
    CHECK(((resident_exit[1] >> 21u) & 0x1Fu) == 10u);
    CHECK(((resident_exit[1] >> 16u) & 0x1Fu) == 10u);
    CHECK((resident_exit[1] & 0xFFFFu) == 0xFFFFu);
    admission = hook_admission_words(&hook);
    CHECK(admission[AZ_HOOK_ADMISSION_ACTIVE_OFFSET /
        sizeof(uint32_t)] == 0u);
    CHECK(admission[AZ_HOOK_ADMISSION_ACCEPTING_OFFSET /
        sizeof(uint32_t)] == 1u);

    CHECK(event_count == 7u);
    check_event(0u, TEST_EVENT_SWEEP_DCACHE, slot,
        AZ_HOOK_SLOT_SIZE, 0u);
    check_event(1u, TEST_EVENT_SWEEP_ICACHE, slot,
        AZ_HOOK_SLOT_SIZE, 0u);
    check_event(2u, TEST_EVENT_QUERY_PROTECT, TEST_TARGET_ADDRESS,
        0u, TEST_OLD_PROTECT);
    check_event(3u, TEST_EVENT_SET_PROTECT, TEST_TARGET_ADDRESS,
        sizeof(uint32_t), PAGE_EXECUTE_READWRITE);
    check_event(4u, TEST_EVENT_SWEEP_DCACHE, TEST_TARGET_ADDRESS,
        sizeof(uint32_t), 0u);
    check_event(5u, TEST_EVENT_SWEEP_ICACHE, TEST_TARGET_ADDRESS,
        sizeof(uint32_t), 0u);
    check_event(6u, TEST_EVENT_SET_PROTECT, TEST_TARGET_ADDRESS,
        sizeof(uint32_t), TEST_OLD_PROTECT);

    CHECK(az_live_hook_active_entries(&hook) == 0u);
    CHECK(az_live_hook_accepting(&hook) == 1u);
    CHECK(az_live_hook_can_unload(&hook) == 0u);
    CHECK(az_live_hook_trampoline(&hook) ==
        (void *)(uintptr_t)hook.plan.trampoline_address);
}

static void test_install_cas_failure_closes_and_reserves_slot(void)
{
    AzHookArena arena;
    AzLiveHook hook;
    volatile uint32_t *target;
    volatile uint32_t *resident_admission;
    uintptr_t slot;

    reset_stubs();
    CHECK(create_arena(&arena) == AZ_HOOK_RUNTIME_OK);
    target = map_target(TEST_TARGET_ADDRESS, TEST_ORIGINAL_INSTRUCTION);
    CHECK(target != NULL);
    slot = arena.base;
    mutate_on_next_write_enable = 1u;
    mutate_on_next_write_value = 0x60000001u;
    clear_events();
    memset(&hook, 0xA5, sizeof(hook));

    CHECK(install_default(&arena, &hook) ==
        AZ_HOOK_RUNTIME_TARGET_CHANGED);
    CHECK(*target == 0x60000001u);
    CHECK(arena.used == AZ_HOOK_SLOT_SIZE);
    check_hook_cleared(&hook);
    resident_admission = (volatile uint32_t *)(
        slot + AZ_HOOK_ADMISSION_OFFSET);
    CHECK(resident_admission[0] == 0u);
    CHECK(resident_admission[1] == 0u);
    CHECK(event_count == 5u);
    check_event(0u, TEST_EVENT_SWEEP_DCACHE, slot,
        AZ_HOOK_SLOT_SIZE, 0u);
    check_event(1u, TEST_EVENT_SWEEP_ICACHE, slot,
        AZ_HOOK_SLOT_SIZE, 0u);
    check_event(2u, TEST_EVENT_QUERY_PROTECT, TEST_TARGET_ADDRESS,
        0u, TEST_OLD_PROTECT);
    check_event(3u, TEST_EVENT_SET_PROTECT, TEST_TARGET_ADDRESS,
        sizeof(uint32_t), PAGE_EXECUTE_READWRITE);
    check_event(4u, TEST_EVENT_SET_PROTECT, TEST_TARGET_ADDRESS,
        sizeof(uint32_t), TEST_OLD_PROTECT);

    *target = TEST_ORIGINAL_INSTRUCTION;
    clear_events();
    CHECK(install_default(&arena, &hook) == AZ_HOOK_RUNTIME_OK);
    CHECK(hook.plan.relay_address ==
        (uint32_t)slot + AZ_HOOK_SLOT_SIZE);
    CHECK(arena.used == 2u * AZ_HOOK_SLOT_SIZE);
    CHECK(az_live_hook_remove(&hook) == AZ_HOOK_RUNTIME_OK);
}

static void test_arena_capacity_and_permanent_reservations(void)
{
    AzHookArena arena;
    AzLiveHook hook;
    volatile uint32_t *target;
    uint32_t slot_count;
    uint32_t index;

    reset_stubs();
    CHECK(create_arena(&arena) == AZ_HOOK_RUNTIME_OK);
    target = map_target(TEST_TARGET_ADDRESS, TEST_ORIGINAL_INSTRUCTION);
    CHECK(target != NULL);
    slot_count = arena.size / AZ_HOOK_SLOT_SIZE;
    CHECK(slot_count == 25u);

    for (index = 0u; index < slot_count; ++index) {
        CHECK(install_default(&arena, &hook) == AZ_HOOK_RUNTIME_OK);
        CHECK(hook.plan.relay_address ==
            (uint32_t)arena.base + index * AZ_HOOK_SLOT_SIZE);
        CHECK(az_live_hook_remove(&hook) == AZ_HOOK_RUNTIME_OK);
        CHECK(*target == TEST_ORIGINAL_INSTRUCTION);
    }
    CHECK(arena.used == slot_count * AZ_HOOK_SLOT_SIZE);
    memset(&hook, 0xA5, sizeof(hook));
    CHECK(install_default(&arena, &hook) == AZ_HOOK_RUNTIME_ARENA_FULL);
    check_hook_cleared(&hook);
    CHECK(arena.used == slot_count * AZ_HOOK_SLOT_SIZE);
    CHECK(az_hook_arena_release_uninstalled(&arena) ==
        AZ_HOOK_RUNTIME_TARGET_CHANGED);
}

static void test_remove_guards_and_success(void)
{
    AzHookArena arena;
    AzLiveHook hook;
    AzLiveHook empty_hook;
    volatile uint32_t *target;

    reset_stubs();
    memset(&empty_hook, 0, sizeof(empty_hook));
    CHECK(az_live_hook_remove(NULL) == AZ_HOOK_RUNTIME_NULL);
    CHECK(az_live_hook_remove(&empty_hook) ==
        AZ_HOOK_RUNTIME_NOT_INSTALLED);
    CHECK(event_count == 0u);

    CHECK(create_arena(&arena) == AZ_HOOK_RUNTIME_OK);
    target = map_target(TEST_TARGET_ADDRESS, TEST_ORIGINAL_INSTRUCTION);
    CHECK(target != NULL);
    CHECK(install_default(&arena, &hook) == AZ_HOOK_RUNTIME_OK);
    clear_events();

    CHECK(az_live_hook_remove(&hook) == AZ_HOOK_RUNTIME_OK);
    CHECK(*target == TEST_ORIGINAL_INSTRUCTION);
    CHECK(hook.installed == 0u);
    CHECK(hook.target_restored == 1u);
    CHECK(az_live_hook_accepting(&hook) == 0u);
    CHECK(az_live_hook_active_entries(&hook) == 0u);
    CHECK(az_live_hook_can_unload(&hook) == 1u);
    CHECK(az_live_hook_trampoline(&hook) == NULL);
    CHECK(event_count == 4u);
    check_event(0u, TEST_EVENT_SET_PROTECT, TEST_TARGET_ADDRESS,
        sizeof(uint32_t), PAGE_EXECUTE_READWRITE);
    check_event(1u, TEST_EVENT_SWEEP_DCACHE, TEST_TARGET_ADDRESS,
        sizeof(uint32_t), 0u);
    check_event(2u, TEST_EVENT_SWEEP_ICACHE, TEST_TARGET_ADDRESS,
        sizeof(uint32_t), 0u);
    check_event(3u, TEST_EVENT_SET_PROTECT, TEST_TARGET_ADDRESS,
        sizeof(uint32_t), TEST_OLD_PROTECT);
    CHECK(az_live_hook_remove(&hook) ==
        AZ_HOOK_RUNTIME_NOT_INSTALLED);
    CHECK(event_count == 4u);
}

static void test_remove_quiesces_without_repatching(void)
{
    AzHookArena arena;
    AzLiveHook hook;
    volatile uint32_t *target;
    volatile uint32_t *admission;

    reset_stubs();
    CHECK(create_arena(&arena) == AZ_HOOK_RUNTIME_OK);
    target = map_target(TEST_TARGET_ADDRESS, TEST_ORIGINAL_INSTRUCTION);
    CHECK(target != NULL);
    CHECK(install_default(&arena, &hook) == AZ_HOOK_RUNTIME_OK);
    admission = hook_admission_words(&hook);
    __atomic_store_n(&admission[0], 2u, __ATOMIC_SEQ_CST);
    clear_events();

    CHECK(az_live_hook_remove(&hook) == AZ_HOOK_RUNTIME_QUIESCING);
    CHECK(*target == TEST_ORIGINAL_INSTRUCTION);
    CHECK(hook.installed == 1u);
    CHECK(hook.target_restored == 1u);
    CHECK(az_live_hook_accepting(&hook) == 0u);
    CHECK(az_live_hook_active_entries(&hook) == 2u);
    CHECK(az_live_hook_can_unload(&hook) == 0u);
    CHECK(event_count == 4u);

    clear_events();
    CHECK(az_live_hook_remove(&hook) == AZ_HOOK_RUNTIME_QUIESCING);
    CHECK(event_count == 0u);
    __atomic_store_n(&admission[0], 0u, __ATOMIC_SEQ_CST);
    CHECK(az_live_hook_remove(&hook) == AZ_HOOK_RUNTIME_OK);
    CHECK(event_count == 0u);
    CHECK(hook.installed == 0u);
    CHECK(hook.target_restored == 1u);
    CHECK(az_live_hook_can_unload(&hook) == 1u);
}

static void test_remove_invalid_and_changed_target_are_retryable(void)
{
    AzHookArena arena;
    AzLiveHook hook;
    volatile uint32_t *target;
    uint32_t installed_branch;

    reset_stubs();
    CHECK(create_arena(&arena) == AZ_HOOK_RUNTIME_OK);
    target = map_target(TEST_TARGET_ADDRESS, TEST_ORIGINAL_INSTRUCTION);
    CHECK(target != NULL);
    CHECK(install_default(&arena, &hook) == AZ_HOOK_RUNTIME_OK);
    installed_branch = hook.plan.target_branch;
    target_is_valid = 0u;
    clear_events();

    CHECK(az_live_hook_remove(&hook) == AZ_HOOK_RUNTIME_BAD_TARGET);
    CHECK(az_live_hook_accepting(&hook) == 0u);
    CHECK(hook.installed == 1u);
    CHECK(hook.target_restored == 0u);
    CHECK(*target == installed_branch);
    CHECK(event_count == 0u);

    target_is_valid = 1u;
    mutate_on_next_write_enable = 1u;
    mutate_on_next_write_value = 0x60000002u;
    CHECK(az_live_hook_remove(&hook) ==
        AZ_HOOK_RUNTIME_TARGET_CHANGED);
    CHECK(*target == 0x60000002u);
    CHECK(hook.installed == 1u);
    CHECK(hook.target_restored == 0u);
    CHECK(az_live_hook_accepting(&hook) == 0u);
    CHECK(event_count == 2u);
    check_event(0u, TEST_EVENT_SET_PROTECT, TEST_TARGET_ADDRESS,
        sizeof(uint32_t), PAGE_EXECUTE_READWRITE);
    check_event(1u, TEST_EVENT_SET_PROTECT, TEST_TARGET_ADDRESS,
        sizeof(uint32_t), TEST_OLD_PROTECT);

    *target = installed_branch;
    clear_events();
    CHECK(az_live_hook_remove(&hook) == AZ_HOOK_RUNTIME_OK);
    CHECK(*target == TEST_ORIGINAL_INSTRUCTION);
    CHECK(hook.target_restored == 1u);
    CHECK(hook.installed == 0u);
    CHECK(event_count == 4u);
}

static void test_observers_fail_closed_for_invalid_hooks(void)
{
    AzLiveHook hook;
    volatile uint32_t admission[2];

    reset_stubs();
    memset(&hook, 0, sizeof(hook));
    CHECK(az_live_hook_active_entries(NULL) == 0u);
    CHECK(az_live_hook_accepting(NULL) == 0u);
    CHECK(az_live_hook_can_unload(NULL) == 0u);
    CHECK(az_live_hook_trampoline(NULL) == NULL);
    CHECK(az_live_hook_active_entries(&hook) == 0u);
    CHECK(az_live_hook_accepting(&hook) == 0u);
    CHECK(az_live_hook_can_unload(&hook) == 0u);
    CHECK(az_live_hook_trampoline(&hook) == NULL);

    admission[0] = 7u;
    admission[1] = 3u;
    hook.admission_address = (uintptr_t)admission;
    hook.installed = 1u;
    hook.plan.trampoline_address = 0x12345678u;
    CHECK(az_live_hook_active_entries(&hook) == 7u);
    CHECK(az_live_hook_accepting(&hook) == 1u);
    CHECK(az_live_hook_can_unload(&hook) == 0u);
    CHECK(az_live_hook_trampoline(&hook) ==
        (void *)(uintptr_t)0x12345678u);

    admission[0] = 0u;
    admission[1] = 0u;
    hook.target_restored = 1u;
    CHECK(az_live_hook_can_unload(&hook) == 1u);
}

static void test_second_target_and_exact_slot_boundary(void)
{
    AzHookArena arena;
    AzLiveHook hook;
    volatile uint32_t *target;

    reset_stubs();
    CHECK(create_arena(&arena) == AZ_HOOK_RUNTIME_OK);
    target = map_target(TEST_SECOND_TARGET_ADDRESS,
        TEST_ORIGINAL_INSTRUCTION);
    CHECK(target != NULL);
    arena.used = arena.size - AZ_HOOK_SLOT_SIZE;
    CHECK(az_live_hook_install(
        &arena,
        TEST_SECOND_TARGET_ADDRESS,
        TEST_ORIGINAL_INSTRUCTION,
        (const void *)(uintptr_t)TEST_DETOUR_ADDRESS,
        &hook) == AZ_HOOK_RUNTIME_OK);
    CHECK(arena.used == arena.size);
    CHECK(az_live_hook_remove(&hook) == AZ_HOOK_RUNTIME_OK);
    memset(&hook, 0xA5, sizeof(hook));
    CHECK(az_live_hook_install(
        &arena,
        TEST_SECOND_TARGET_ADDRESS,
        TEST_ORIGINAL_INSTRUCTION,
        (const void *)(uintptr_t)TEST_DETOUR_ADDRESS,
        &hook) == AZ_HOOK_RUNTIME_ARENA_FULL);
    check_hook_cleared(&hook);
}

static void test_result_names(void)
{
    static const struct {
        AzHookRuntimeResult result;
        const char *name;
    } cases[] = {
        { AZ_HOOK_RUNTIME_OK, "ok" },
        { AZ_HOOK_RUNTIME_NULL, "null" },
        { AZ_HOOK_RUNTIME_NO_NEAR_MEMORY, "no-near-memory" },
        { AZ_HOOK_RUNTIME_ARENA_FULL, "arena-full" },
        { AZ_HOOK_RUNTIME_BAD_TARGET, "bad-target" },
        { AZ_HOOK_RUNTIME_TARGET_CHANGED, "target-changed" },
        { AZ_HOOK_RUNTIME_PLAN_FAILED, "plan-failed" },
        { AZ_HOOK_RUNTIME_QUIESCING, "quiescing" },
        { AZ_HOOK_RUNTIME_NOT_INSTALLED, "not-installed" }
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        CHECK(strcmp(
            az_hook_runtime_result_name(cases[index].result),
            cases[index].name) == 0);
    }
    CHECK(strcmp(
        az_hook_runtime_result_name((AzHookRuntimeResult)999),
        "unknown") == 0);
}

int main(void)
{
    test_arena_argument_and_exhaustion_paths();
    test_arena_rejects_bad_allocations_then_accepts();
    test_arena_release_contract();
    test_install_argument_and_arena_guards();
    test_install_target_and_plan_failures_are_non_mutating();
    test_successful_install_emits_resident_code();
    test_install_cas_failure_closes_and_reserves_slot();
    test_arena_capacity_and_permanent_reservations();
    test_remove_guards_and_success();
    test_remove_quiesces_without_repatching();
    test_remove_invalid_and_changed_target_are_retryable();
    test_observers_fail_closed_for_invalid_hooks();
    test_second_target_and_exact_slot_boundary();
    test_result_names();
    reset_stubs();

    if (failures != 0) {
        fprintf(stderr, "%d hook runtime assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }

    puts("AuroraAZ hook runtime host tests passed");
    return EXIT_SUCCESS;
}
