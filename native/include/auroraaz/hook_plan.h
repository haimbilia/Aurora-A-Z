#ifndef AURORAAZ_HOOK_PLAN_H
#define AURORAAZ_HOOK_PLAN_H

#include <stdint.h>

#include <auroraaz/ppc.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZ_HOOK_RELAY_WORDS AZ_PPC_ABSOLUTE_BRANCH_WORDS
#define AZ_HOOK_TRAMPOLINE_WORDS 2u

/*
 * A one-instruction hook plan. The target branches atomically to a nearby
 * relay, the relay jumps to the potentially distant C detour, and the
 * trampoline executes the displaced instruction before returning to target+4.
 */
typedef struct AzHookPlan {
    uint32_t target_address;
    uint32_t relay_address;
    uint32_t trampoline_address;
    uint32_t detour_address;
    uint32_t original_instruction;
    uint32_t target_branch;
    uint32_t relay[AZ_HOOK_RELAY_WORDS];
    uint32_t trampoline[AZ_HOOK_TRAMPOLINE_WORDS];
} AzHookPlan;

AzPpcResult az_hook_plan_build(
    uint32_t target_address,
    uint32_t relay_address,
    uint32_t trampoline_address,
    uint32_t detour_address,
    uint32_t original_instruction,
    AzHookPlan *plan);

#ifdef __cplusplus
}
#endif

#endif
