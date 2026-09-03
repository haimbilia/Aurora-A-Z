#if !defined(AURORAAZ_XBOX360)
#error "hook_runtime.c must only be built for the Xbox 360 target"
#endif

#include <stddef.h>
#include <stdint.h>

#include <xecore/xboxkrnl.h>

#include <auroraaz/hook_plan.h>
#include <auroraaz/hook_runtime.h>

#define AZ_ADMITTED_RELAY_WORDS 21u
#define AZ_RESIDENT_EXIT_WORDS 7u
#define AZ_EMBEDDED_ARENA_PAGE_SIZE 0x10000u

/*
 * NtAllocateVirtualMemory cannot create pages in the loader-owned 0x8xxxxxxx
 * XEX range on retail kernels.  The release image is therefore linked at the
 * first free 64-KiB boundary after Aurora Rev1655 and carries its own page for
 * relays and trampolines.  DllMain pins the module before this page is used,
 * so its lifetime is the title lifetime required by the resident-return ABI.
 */
#if defined(_MSC_VER)
#pragma section(".azhook", read, write, execute)
__declspec(allocate(".azhook")) __declspec(align(4096))
#elif defined(__clang__) || defined(__GNUC__)
__attribute__((section(".azhook"), used, aligned(4096)))
#endif
static uint8_t g_auroraaz_hook_arena_storage[AZ_HOOK_ARENA_SIZE] = {0};
static AzHookArenaDiagnostics g_arena_diagnostics = {
    0u, 0u, 0u, 0u, 0u, 0u, 0u};

typedef struct AzResidentAdmission {
    volatile uint32_t active_entries;
    volatile uint32_t accepting;
} AzResidentAdmission;

typedef char AzAdmissionActiveOffsetMustMatch[
    offsetof(AzResidentAdmission, active_entries) ==
        AZ_HOOK_ADMISSION_ACTIVE_OFFSET ? 1 : -1];
typedef char AzAdmissionAcceptingOffsetMustMatch[
    offsetof(AzResidentAdmission, accepting) ==
        AZ_HOOK_ADMISSION_ACCEPTING_OFFSET ? 1 : -1];
typedef char AzAdmittedRelayMustFit[
    AZ_ADMITTED_RELAY_WORDS * sizeof(uint32_t) <=
        AZ_HOOK_TRAMPOLINE_OFFSET ? 1 : -1];
typedef char AzResidentStateMustFit[
    AZ_HOOK_ADMISSION_OFFSET + sizeof(AzResidentAdmission) <=
        AZ_HOOK_SLOT_SIZE ? 1 : -1];
typedef char AzResidentExitMustFit[
    AZ_HOOK_RESIDENT_EXIT_OFFSET +
        AZ_RESIDENT_EXIT_WORDS * sizeof(uint32_t) <=
        AZ_HOOK_ADMISSION_OFFSET ? 1 : -1];
typedef char AzArenaMustFitItsXexPage[
    AZ_HOOK_ARENA_SIZE <= AZ_EMBEDDED_ARENA_PAGE_SIZE ? 1 : -1];

/* xecorelib exports these ordinals but does not currently declare them. */
extern void KeSweepDcacheRange(void *address, uint32_t size);
extern void KeSweepIcacheRange(void *address, uint32_t size);

static void clear_arena(AzHookArena *arena)
{
    arena->base = (uintptr_t)0u;
    arena->size = 0u;
    arena->used = 0u;
}

static void clear_hook(AzLiveHook *hook)
{
    size_t index;

    hook->plan.target_address = 0u;
    hook->plan.relay_address = 0u;
    hook->plan.trampoline_address = 0u;
    hook->plan.detour_address = 0u;
    hook->plan.original_instruction = 0u;
    hook->plan.target_branch = 0u;
    for (index = 0u; index < AZ_HOOK_RELAY_WORDS; ++index) {
        hook->plan.relay[index] = 0u;
    }
    for (index = 0u; index < AZ_HOOK_TRAMPOLINE_WORDS; ++index) {
        hook->plan.trampoline[index] = 0u;
    }
    hook->admission_address = (uintptr_t)0u;
    hook->old_protect = 0u;
    hook->installed = 0u;
    hook->target_restored = 0u;
    hook->direct = 0u;
}

static uintptr_t embedded_arena_base(void)
{
    return (uintptr_t)&g_auroraaz_hook_arena_storage[0];
}

static uint8_t embedded_arena_is_usable(void)
{
    const uintptr_t base = embedded_arena_base();
    uint32_t failures = 0u;

    g_arena_diagnostics.embedded_base = (uint32_t)base;
    if (base > (uintptr_t)UINT32_MAX) {
        failures |= AZ_HOOK_ARENA_DIAG_BASE_ABOVE_32BIT;
    }
    if ((base & (AZ_HOOK_ARENA_SIZE - 1u)) != 0u) {
        failures |= AZ_HOOK_ARENA_DIAG_BASE_UNALIGNED;
    }
    if (base < (uintptr_t)AZ_REV1655_HOOK_ARENA_START) {
        failures |= AZ_HOOK_ARENA_DIAG_BASE_BELOW_RANGE;
    }
    if (base > (uintptr_t)(
            AZ_REV1655_HOOK_ARENA_END - AZ_HOOK_ARENA_SIZE)) {
        failures |= AZ_HOOK_ARENA_DIAG_BASE_ABOVE_RANGE;
    }
    if (!MmIsAddressValid((void *)base)) {
        failures |= AZ_HOOK_ARENA_DIAG_START_INVALID;
    }
    if (!MmIsAddressValid(
            (void *)(base + AZ_HOOK_ARENA_SIZE - 1u))) {
        failures |= AZ_HOOK_ARENA_DIAG_END_INVALID;
    }
    g_arena_diagnostics.validation_failures = failures;
    return (uint8_t)(failures == 0u);
}

AzHookArenaDiagnostics az_hook_arena_diagnostics(void)
{
    return g_arena_diagnostics;
}

#if defined(AURORAAZ_HOOK_RUNTIME_TEST_ALLOCATOR)

#define AZ_ALLOCATION_GRANULARITY 0x10000u

static void release_test_allocation(void *base)
{
    void *release_base = base;
    uint32_t release_size = 0u;

    if (release_base != NULL) {
        (void)NtFreeVirtualMemory(
            &release_base,
            &release_size,
            MEM_RELEASE,
            /* The final kernel ABI argument is DebugMemory, despite the
             * REGION type used by xecorelib's declaration.  Retail title
             * allocations must pass FALSE (REGION_AUTO == 0). */
            REGION_AUTO);
    }
}

#endif

static void flush_code(void *address, uint32_t size)
{
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    KeSweepDcacheRange(address, size);
    KeSweepIcacheRange(address, size);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

static void write_words(
    volatile uint32_t *destination,
    const uint32_t *source,
    size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        destination[index] = source[index];
    }
}

static uint32_t load_u32(const volatile uint32_t *value)
{
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static void store_u32(volatile uint32_t *value, uint32_t replacement)
{
    __atomic_store_n(value, replacement, __ATOMIC_SEQ_CST);
}

static AzResidentAdmission *hook_admission(const AzLiveHook *hook)
{
    if (hook == NULL || hook->admission_address == (uintptr_t)0u) {
        return NULL;
    }

    return (AzResidentAdmission *)hook->admission_address;
}

/*
 * Emit a resident admission relay using only volatile r0/r11/r12, CR0 and
 * CTR. It increments active_entries before observing accepting. An admitted
 * detour receives the state pointer in r0; a closed entry decrements locally
 * and executes the resident trampoline without touching module code/data.
 */
static AzPpcResult build_admitted_relay(
    uint32_t relay_address,
    uint32_t admission_address,
    uint32_t detour_address,
    uint32_t trampoline_address,
    uint32_t relay[AZ_ADMITTED_RELAY_WORDS])
{
    uint32_t detour_branch[AZ_PPC_ABSOLUTE_BRANCH_WORDS];
    AzPpcResult result;

    if (relay == NULL) {
        return AZ_PPC_NULL;
    }

    result = az_ppc_emit_absolute_branch(
        detour_address,
        0u,
        detour_branch);
    if (result != AZ_PPC_OK) {
        return result;
    }

    relay[0] = 0x3D600000u | ((admission_address >> 16u) & 0xFFFFu);
    relay[1] = 0x616B0000u | (admission_address & 0xFFFFu);
    relay[2] = 0x7D805828u; /* lwarx r12, 0, r11 */
    relay[3] = 0x398C0001u; /* addi r12, r12, 1 */
    relay[4] = 0x7D80592Du; /* stwcx. r12, 0, r11 */
    relay[5] = 0x4082FFF4u; /* bne relay[2] */
    relay[6] = 0x7C0004ACu; /* sync */
    relay[7] = 0x818B0004u; /* lwz r12, 4(r11) */
    relay[8] = 0x2C0C0000u; /* cmpwi r12, 0 */
    relay[9] = 0x41820018u; /* beq relay[15] */
    relay[10] = 0x7D605B78u; /* mr r0, r11 */
    relay[11] = detour_branch[0];
    relay[12] = detour_branch[1];
    relay[13] = detour_branch[2];
    relay[14] = detour_branch[3];
    relay[15] = 0x7D805828u; /* lwarx r12, 0, r11 */
    relay[16] = 0x398CFFFFu; /* addi r12, r12, -1 */
    relay[17] = 0x7D80592Du; /* stwcx. r12, 0, r11 */
    relay[18] = 0x4082FFF4u; /* bne relay[15] */
    relay[19] = 0x7C0004ACu; /* sync */
    result = az_ppc_encode_relative_branch(
        relay_address + 20u * (uint32_t)sizeof(uint32_t),
        trampoline_address,
        0u,
        &relay[20]);
    return result;
}

/*
 * Resident detour exit ABI:
 *   r3  = function result to return unchanged
 *   r11 = resident admission-state address
 *   r12 = Aurora's original caller LR
 *   r1  = Aurora caller's original stack pointer
 *
 * The active count reaches zero inside resident code. No module instruction
 * or module data is accessed after the successful stwcx.
 */
static void build_resident_exit(
    uint32_t epilogue[AZ_RESIDENT_EXIT_WORDS])
{
    epilogue[0] = 0x7D405828u; /* lwarx r10, 0, r11 */
    epilogue[1] = 0x394AFFFFu; /* addi r10, r10, -1 */
    epilogue[2] = 0x7D40592Du; /* stwcx. r10, 0, r11 */
    epilogue[3] = 0x4082FFF4u; /* bne epilogue[0] */
    epilogue[4] = 0x7C0004ACu; /* sync */
    epilogue[5] = 0x7D8803A6u; /* mtlr r12 */
    epilogue[6] = 0x4E800020u; /* blr */
}

AzHookRuntimeResult az_hook_arena_create_rev1655(AzHookArena *arena)
{
    const uintptr_t resident_base = embedded_arena_base();

    g_arena_diagnostics.embedded_base = (uint32_t)resident_base;
    g_arena_diagnostics.validation_failures = 0u;
    g_arena_diagnostics.protection_before = 0u;
    g_arena_diagnostics.protection_after = 0u;

    if (arena == NULL) {
        return AZ_HOOK_RUNTIME_NULL;
    }
    clear_arena(arena);

    if (embedded_arena_is_usable() != 0u) {
        uint32_t protection;

        g_arena_diagnostics.protection_before =
            MmQueryAddressProtect((void *)resident_base);
        MmSetAddressProtect(
            (void *)resident_base,
            /* Low-address XEX images use 64-KiB pages. Protect the complete
             * isolated .azhook page, while exposing only the first 4 KiB to
             * the hook allocator. */
            AZ_EMBEDDED_ARENA_PAGE_SIZE,
            PAGE_EXECUTE_READWRITE);
        protection = MmQueryAddressProtect((void *)resident_base);
        g_arena_diagnostics.protection_after = protection;
        if ((protection & 0xFFu) != PAGE_EXECUTE_READWRITE) {
            g_arena_diagnostics.validation_failures |=
                AZ_HOOK_ARENA_DIAG_PROTECT_MISMATCH;
            return AZ_HOOK_RUNTIME_NO_NEAR_MEMORY;
        }
        arena->base = resident_base;
        arena->size = AZ_HOOK_ARENA_SIZE;
        arena->used = 0u;
        return AZ_HOOK_RUNTIME_OK;
    }

#if defined(AURORAAZ_HOOK_RUNTIME_TEST_ALLOCATOR)
    /* Host-only fallback retained for allocator fault-injection coverage. */
    {
        uint32_t candidate;
        for (candidate = AZ_REV1655_HOOK_ARENA_START;
             candidate <= AZ_REV1655_HOOK_ARENA_END - AZ_HOOK_ARENA_SIZE;
             candidate += AZ_ALLOCATION_GRANULARITY) {
            void *requested = (void *)(uintptr_t)candidate;
            uint32_t requested_size = AZ_HOOK_ARENA_SIZE;
            NTSTATUS status = NtAllocateVirtualMemory(
                &requested,
                &requested_size,
                MEM_RESERVE | MEM_COMMIT,
                PAGE_EXECUTE_READWRITE,
                /* Final ABI argument is DebugMemory=FALSE. */
                REGION_AUTO);

            if (FAILED(status)) {
                continue;
            }

            if ((uintptr_t)requested != (uintptr_t)candidate ||
                requested_size < AZ_HOOK_ARENA_SIZE) {
                release_test_allocation(requested);
                continue;
            }

            arena->base = (uintptr_t)requested;
            arena->size = requested_size;
            arena->used = 0u;
            return AZ_HOOK_RUNTIME_OK;
        }
    }
#endif

    return AZ_HOOK_RUNTIME_NO_NEAR_MEMORY;
}

AzHookRuntimeResult az_hook_arena_release_uninstalled(AzHookArena *arena)
{
    if (arena == NULL) {
        return AZ_HOOK_RUNTIME_NULL;
    }
    if (arena->base == (uintptr_t)0u) {
        return AZ_HOOK_RUNTIME_OK;
    }
    if (arena->used != 0u) {
        return AZ_HOOK_RUNTIME_TARGET_CHANGED;
    }

    if (arena->base == embedded_arena_base()) {
        clear_arena(arena);
        return AZ_HOOK_RUNTIME_OK;
    }

#if defined(AURORAAZ_HOOK_RUNTIME_TEST_ALLOCATOR)
    release_test_allocation((void *)arena->base);
    clear_arena(arena);
    return AZ_HOOK_RUNTIME_OK;
#else
    return AZ_HOOK_RUNTIME_TARGET_CHANGED;
#endif
}

AzHookRuntimeResult az_live_hook_install(
    AzHookArena *arena,
    uint32_t target_address,
    uint32_t expected_instruction,
    const void *detour,
    AzLiveHook *hook)
{
    uintptr_t slot;
    uint32_t relay_address;
    uint32_t trampoline_address;
    uint32_t admission_address;
    uint32_t admitted_relay[AZ_ADMITTED_RELAY_WORDS];
    uint32_t resident_exit[AZ_RESIDENT_EXIT_WORDS];
    AzResidentAdmission *admission;
    AzPpcResult plan_result;
    volatile uint32_t *target;
    uint32_t compare;
    int exchanged;

    if (arena == NULL || detour == NULL || hook == NULL) {
        return AZ_HOOK_RUNTIME_NULL;
    }
    clear_hook(hook);

    if (arena->base == (uintptr_t)0u ||
        arena->size < AZ_HOOK_SLOT_SIZE ||
        arena->used > arena->size - AZ_HOOK_SLOT_SIZE) {
        return AZ_HOOK_RUNTIME_ARENA_FULL;
    }

    target = (volatile uint32_t *)(uintptr_t)target_address;
    if ((target_address & 3u) != 0u ||
        !MmIsAddressValid((void *)(uintptr_t)target_address)) {
        return AZ_HOOK_RUNTIME_BAD_TARGET;
    }
    if (__atomic_load_n(target, __ATOMIC_ACQUIRE) != expected_instruction) {
        return AZ_HOOK_RUNTIME_TARGET_CHANGED;
    }

    slot = arena->base + (uintptr_t)arena->used;
    relay_address = (uint32_t)slot;
    trampoline_address = relay_address + AZ_HOOK_TRAMPOLINE_OFFSET;
    admission_address = relay_address + AZ_HOOK_ADMISSION_OFFSET;

    plan_result = az_hook_plan_build(
        target_address,
        relay_address,
        trampoline_address,
        (uint32_t)(uintptr_t)detour,
        expected_instruction,
        &hook->plan);
    if (plan_result != AZ_PPC_OK) {
        clear_hook(hook);
        return AZ_HOOK_RUNTIME_PLAN_FAILED;
    }

    plan_result = build_admitted_relay(
        relay_address,
        admission_address,
        (uint32_t)(uintptr_t)detour,
        trampoline_address,
        admitted_relay);
    if (plan_result != AZ_PPC_OK) {
        clear_hook(hook);
        return AZ_HOOK_RUNTIME_PLAN_FAILED;
    }
    build_resident_exit(resident_exit);

    admission = (AzResidentAdmission *)(uintptr_t)admission_address;
    store_u32(&admission->active_entries, 0u);
    store_u32(&admission->accepting, 0u);

    write_words(
        (volatile uint32_t *)(uintptr_t)relay_address,
        admitted_relay,
        AZ_ADMITTED_RELAY_WORDS);
    write_words(
        (volatile uint32_t *)(uintptr_t)trampoline_address,
        hook->plan.trampoline,
        AZ_HOOK_TRAMPOLINE_WORDS);
    write_words(
        (volatile uint32_t *)(
            slot + (uintptr_t)AZ_HOOK_RESIDENT_EXIT_OFFSET),
        resident_exit,
        AZ_RESIDENT_EXIT_WORDS);
    flush_code((void *)slot, AZ_HOOK_SLOT_SIZE);

    /* Permanently reserve the slot before any target can reach it. Even an
     * install CAS failure deliberately leaks this small resident slot rather
     * than permitting concurrent lifecycle code to reclaim or reuse it. */
    arena->used += AZ_HOOK_SLOT_SIZE;
    hook->admission_address = (uintptr_t)admission_address;
    store_u32(&admission->accepting, 1u);

    hook->old_protect =
        MmQueryAddressProtect((void *)(uintptr_t)target_address);
    MmSetAddressProtect(
        (void *)(uintptr_t)target_address,
        (uint32_t)sizeof(uint32_t),
        PAGE_EXECUTE_READWRITE);

    compare = expected_instruction;
    exchanged = __atomic_compare_exchange_n(
        target,
        &compare,
        hook->plan.target_branch,
        0,
        __ATOMIC_SEQ_CST,
        __ATOMIC_SEQ_CST);
    if (exchanged == 0) {
        store_u32(&admission->accepting, 0u);
        MmSetAddressProtect(
            (void *)(uintptr_t)target_address,
            (uint32_t)sizeof(uint32_t),
            hook->old_protect);
        clear_hook(hook);
        return AZ_HOOK_RUNTIME_TARGET_CHANGED;
    }

    flush_code(
        (void *)(uintptr_t)target_address,
        (uint32_t)sizeof(uint32_t));
    MmSetAddressProtect(
        (void *)(uintptr_t)target_address,
        (uint32_t)sizeof(uint32_t),
        hook->old_protect);

    hook->installed = 1u;
    hook->target_restored = 0u;
    hook->direct = 0u;
    return AZ_HOOK_RUNTIME_OK;
}

AzHookRuntimeResult az_live_hook_install_direct(
    uint32_t target_address,
    uint32_t expected_instruction,
    const void *detour,
    AzLiveHook *hook)
{
    volatile uint32_t *target;
    AzPpcResult branch_result;
    uint32_t write_protection;
    uint32_t compare;
    int exchanged;

    if (detour == NULL || hook == NULL) {
        return AZ_HOOK_RUNTIME_NULL;
    }
    clear_hook(hook);

    target = (volatile uint32_t *)(uintptr_t)target_address;
    if ((target_address & 3u) != 0u ||
        !MmIsAddressValid((void *)(uintptr_t)target_address)) {
        return AZ_HOOK_RUNTIME_BAD_TARGET;
    }
    if ((uintptr_t)detour > (uintptr_t)UINT32_MAX ||
        (((uintptr_t)detour & (uintptr_t)3u) != (uintptr_t)0u)) {
        return AZ_HOOK_RUNTIME_PLAN_FAILED;
    }
    if (__atomic_load_n(target, __ATOMIC_ACQUIRE) != expected_instruction) {
        return AZ_HOOK_RUNTIME_TARGET_CHANGED;
    }

    branch_result = az_ppc_encode_relative_branch(
        target_address,
        (uint32_t)(uintptr_t)detour,
        0u,
        &hook->plan.target_branch);
    if (branch_result != AZ_PPC_OK) {
        clear_hook(hook);
        return AZ_HOOK_RUNTIME_PLAN_FAILED;
    }
    hook->plan.target_address = target_address;
    hook->plan.detour_address = (uint32_t)(uintptr_t)detour;
    hook->plan.original_instruction = expected_instruction;

    hook->old_protect =
        MmQueryAddressProtect((void *)(uintptr_t)target_address);
    g_arena_diagnostics.target_address = target_address;
    g_arena_diagnostics.target_protection_before = hook->old_protect;
    g_arena_diagnostics.target_protection_after = 0u;
    MmSetAddressProtect(
        (void *)(uintptr_t)target_address,
        (uint32_t)sizeof(uint32_t),
        PAGE_EXECUTE_READWRITE);
    write_protection =
        MmQueryAddressProtect((void *)(uintptr_t)target_address);
    g_arena_diagnostics.target_protection_after = write_protection;
    /*
     * On the RGH/freeBOOT retail environment MmQueryAddressProtect keeps
     * reporting the signed image's loader metadata (execute/read) after
     * MmSetAddressProtect, while direct title-code stores remain enabled by
     * the console patch set. The compare/exchange below is still the
     * authoritative fail-closed write: it patches only the exact expected
     * instruction and detects every competing change.
     */
    compare = expected_instruction;
    exchanged = __atomic_compare_exchange_n(
        target,
        &compare,
        hook->plan.target_branch,
        0,
        __ATOMIC_SEQ_CST,
        __ATOMIC_SEQ_CST);
    if (exchanged == 0) {
        MmSetAddressProtect(
            (void *)(uintptr_t)target_address,
            (uint32_t)sizeof(uint32_t),
            hook->old_protect);
        clear_hook(hook);
        return AZ_HOOK_RUNTIME_TARGET_CHANGED;
    }

    flush_code(
        (void *)(uintptr_t)target_address,
        (uint32_t)sizeof(uint32_t));
    MmSetAddressProtect(
        (void *)(uintptr_t)target_address,
        (uint32_t)sizeof(uint32_t),
        hook->old_protect);
    hook->installed = 1u;
    hook->target_restored = 0u;
    hook->direct = 1u;
    return AZ_HOOK_RUNTIME_OK;
}

AzHookRuntimeResult az_live_hook_remove(AzLiveHook *hook)
{
    volatile uint32_t *target;
    AzResidentAdmission *admission;
    uint32_t compare;
    int exchanged;

    if (hook == NULL) {
        return AZ_HOOK_RUNTIME_NULL;
    }
    if (hook->installed == 0u) {
        return AZ_HOOK_RUNTIME_NOT_INSTALLED;
    }

    admission = hook_admission(hook);
    if (hook->direct != 0u) {
        target = (volatile uint32_t *)(uintptr_t)hook->plan.target_address;
        if (!MmIsAddressValid(
                (void *)(uintptr_t)hook->plan.target_address)) {
            return AZ_HOOK_RUNTIME_BAD_TARGET;
        }
        MmSetAddressProtect(
            (void *)(uintptr_t)hook->plan.target_address,
            (uint32_t)sizeof(uint32_t),
            PAGE_EXECUTE_READWRITE);
        compare = hook->plan.target_branch;
        exchanged = __atomic_compare_exchange_n(
            target,
            &compare,
            hook->plan.original_instruction,
            0,
            __ATOMIC_SEQ_CST,
            __ATOMIC_SEQ_CST);
        if (exchanged == 0) {
            MmSetAddressProtect(
                (void *)(uintptr_t)hook->plan.target_address,
                (uint32_t)sizeof(uint32_t),
                hook->old_protect);
            return AZ_HOOK_RUNTIME_TARGET_CHANGED;
        }
        flush_code(
            (void *)(uintptr_t)hook->plan.target_address,
            (uint32_t)sizeof(uint32_t));
        MmSetAddressProtect(
            (void *)(uintptr_t)hook->plan.target_address,
            (uint32_t)sizeof(uint32_t),
            hook->old_protect);
        hook->target_restored = 1u;
        hook->installed = 0u;
        return AZ_HOOK_RUNTIME_OK;
    }
    if (admission == NULL) {
        return AZ_HOOK_RUNTIME_NULL;
    }

    /* Close admission before restoring the target. This store participates in
     * the relay's sync/atomic ordering; no later relay entry can branch into
     * the module. */
    store_u32(&admission->accepting, 0u);

    if (hook->target_restored != 0u) {
        if (load_u32(&admission->active_entries) != 0u) {
            return AZ_HOOK_RUNTIME_QUIESCING;
        }
        hook->installed = 0u;
        return AZ_HOOK_RUNTIME_OK;
    }

    target = (volatile uint32_t *)(uintptr_t)hook->plan.target_address;
    if (!MmIsAddressValid((void *)(uintptr_t)hook->plan.target_address)) {
        return AZ_HOOK_RUNTIME_BAD_TARGET;
    }

    MmSetAddressProtect(
        (void *)(uintptr_t)hook->plan.target_address,
        (uint32_t)sizeof(uint32_t),
        PAGE_EXECUTE_READWRITE);
    compare = hook->plan.target_branch;
    exchanged = __atomic_compare_exchange_n(
        target,
        &compare,
        hook->plan.original_instruction,
        0,
        __ATOMIC_SEQ_CST,
        __ATOMIC_SEQ_CST);
    if (exchanged == 0) {
        MmSetAddressProtect(
            (void *)(uintptr_t)hook->plan.target_address,
            (uint32_t)sizeof(uint32_t),
            hook->old_protect);
        return AZ_HOOK_RUNTIME_TARGET_CHANGED;
    }

    flush_code(
        (void *)(uintptr_t)hook->plan.target_address,
        (uint32_t)sizeof(uint32_t));
    MmSetAddressProtect(
        (void *)(uintptr_t)hook->plan.target_address,
        (uint32_t)sizeof(uint32_t),
        hook->old_protect);
    hook->target_restored = 1u;
    if (load_u32(&admission->active_entries) != 0u) {
        return AZ_HOOK_RUNTIME_QUIESCING;
    }
    hook->installed = 0u;
    return AZ_HOOK_RUNTIME_OK;
}

uint32_t az_live_hook_active_entries(const AzLiveHook *hook)
{
    AzResidentAdmission *admission = hook_admission(hook);

    if (admission == NULL) {
        return 0u;
    }
    return load_u32(&admission->active_entries);
}

uint8_t az_live_hook_accepting(const AzLiveHook *hook)
{
    AzResidentAdmission *admission = hook_admission(hook);

    if (admission == NULL) {
        return 0u;
    }
    return load_u32(&admission->accepting) != 0u ? 1u : 0u;
}

uint8_t az_live_hook_can_unload(const AzLiveHook *hook)
{
    AzResidentAdmission *admission = hook_admission(hook);

    if (hook != NULL && hook->direct != 0u) {
        return (hook->target_restored != 0u &&
            hook->installed == 0u) ? 1u : 0u;
    }
    if (hook == NULL || admission == NULL ||
        hook->target_restored == 0u ||
        load_u32(&admission->accepting) != 0u ||
        load_u32(&admission->active_entries) != 0u) {
        return 0u;
    }
    return 1u;
}

void *az_live_hook_trampoline(const AzLiveHook *hook)
{
    if (hook == NULL || hook->installed == 0u) {
        return NULL;
    }
    return (void *)(uintptr_t)hook->plan.trampoline_address;
}

const char *az_hook_runtime_result_name(AzHookRuntimeResult result)
{
    switch (result) {
    case AZ_HOOK_RUNTIME_OK:
        return "ok";
    case AZ_HOOK_RUNTIME_NULL:
        return "null";
    case AZ_HOOK_RUNTIME_NO_NEAR_MEMORY:
        return "no-near-memory";
    case AZ_HOOK_RUNTIME_ARENA_FULL:
        return "arena-full";
    case AZ_HOOK_RUNTIME_BAD_TARGET:
        return "bad-target";
    case AZ_HOOK_RUNTIME_TARGET_CHANGED:
        return "target-changed";
    case AZ_HOOK_RUNTIME_PLAN_FAILED:
        return "plan-failed";
    case AZ_HOOK_RUNTIME_QUIESCING:
        return "quiescing";
    case AZ_HOOK_RUNTIME_NOT_INSTALLED:
        return "not-installed";
    case AZ_HOOK_RUNTIME_PROTECT_FAILED:
        return "protect-failed";
    default:
        return "unknown";
    }
}
