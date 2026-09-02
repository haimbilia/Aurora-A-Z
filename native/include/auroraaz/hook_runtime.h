#ifndef AURORAAZ_HOOK_RUNTIME_H
#define AURORAAZ_HOOK_RUNTIME_H

#define AZ_REV1655_HOOK_ARENA_START 0x82D50000
#define AZ_REV1655_HOOK_ARENA_END 0x84350000
#define AZ_HOOK_ARENA_SIZE 0x1000
#define AZ_HOOK_SLOT_SIZE 0xA0

/*
 * Resident slot layout. The admission state remains in REGION_TITLE memory
 * after a live hook is removed; never free or reuse a live slot. The assembly
 * detour entry receives the admission-state address in volatile r0. Before
 * leaving module text, its assembly shim must restore its stack, put that
 * state address in r11 and Aurora's original return LR in r12, preserve the
 * function result in r3, and branch to the resident exit epilogue. Only the
 * resident epilogue decrements active_entries and returns to Aurora.
 */
#define AZ_HOOK_RESIDENT_RETURN_ABI_VERSION 1
#define AZ_HOOK_TRAMPOLINE_OFFSET 0x60
#define AZ_HOOK_RESIDENT_EXIT_OFFSET 0x70
#define AZ_HOOK_ADMISSION_OFFSET 0x90
#define AZ_HOOK_RESIDENT_EXIT_ADMISSION_DELTA \
    (AZ_HOOK_ADMISSION_OFFSET - AZ_HOOK_RESIDENT_EXIT_OFFSET)
#define AZ_HOOK_ADMISSION_ACTIVE_OFFSET 0
#define AZ_HOOK_ADMISSION_ACCEPTING_OFFSET 4

#ifndef __ASSEMBLER__

#include <stdint.h>

#include <auroraaz/hook_plan.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AzHookRuntimeResult {
    AZ_HOOK_RUNTIME_OK = 0,
    AZ_HOOK_RUNTIME_NULL,
    AZ_HOOK_RUNTIME_NO_NEAR_MEMORY,
    AZ_HOOK_RUNTIME_ARENA_FULL,
    AZ_HOOK_RUNTIME_BAD_TARGET,
    AZ_HOOK_RUNTIME_TARGET_CHANGED,
    AZ_HOOK_RUNTIME_PLAN_FAILED,
    AZ_HOOK_RUNTIME_QUIESCING,
    AZ_HOOK_RUNTIME_NOT_INSTALLED
} AzHookRuntimeResult;

typedef struct AzHookArena {
    uintptr_t base;
    uint32_t size;
    uint32_t used;
} AzHookArena;

typedef struct AzLiveHook {
    AzHookPlan plan;
    uintptr_t admission_address;
    uint32_t old_protect;
    uint8_t installed;
    uint8_t target_restored;
} AzLiveHook;

/* Reserve one executable title-memory page in relative-branch reach. */
AzHookRuntimeResult az_hook_arena_create_rev1655(AzHookArena *arena);

/* Only call before any install attempt has reserved a resident slot. */
AzHookRuntimeResult az_hook_arena_release_uninstalled(AzHookArena *arena);

AzHookRuntimeResult az_live_hook_install(
    AzHookArena *arena,
    uint32_t target_address,
    uint32_t expected_instruction,
    const void *detour,
    AzLiveHook *hook);

AzHookRuntimeResult az_live_hook_remove(AzLiveHook *hook);

/*
 * Removal is deliberately non-blocking. AZ_HOOK_RUNTIME_QUIESCING means the
 * target instruction is already restored and admission is closed, but one or
 * more detours that entered while admission was open still need to return.
 * Retry removal from a normal worker thread. Do not unload the module until it
 * returns AZ_HOOK_RUNTIME_OK.
 */
uint32_t az_live_hook_active_entries(const AzLiveHook *hook);
uint8_t az_live_hook_accepting(const AzLiveHook *hook);
uint8_t az_live_hook_can_unload(const AzLiveHook *hook);

void *az_live_hook_trampoline(const AzLiveHook *hook);
const char *az_hook_runtime_result_name(AzHookRuntimeResult result);

#ifdef __cplusplus
}
#endif

#endif /* !__ASSEMBLER__ */

#endif
