#include <stddef.h>
#include <stdint.h>

#include <auroraaz/hook_plan.h>

AzPpcResult az_hook_plan_build(
    uint32_t target_address,
    uint32_t relay_address,
    uint32_t trampoline_address,
    uint32_t detour_address,
    uint32_t original_instruction,
    AzHookPlan *plan)
{
    AzPpcResult result;

    if (plan == NULL) {
        return AZ_PPC_NULL;
    }

    result = az_ppc_encode_relative_branch(
        target_address,
        relay_address,
        0u,
        &plan->target_branch);
    if (result != AZ_PPC_OK) {
        return result;
    }

    result = az_ppc_emit_absolute_branch(
        detour_address,
        0u,
        plan->relay);
    if (result != AZ_PPC_OK) {
        return result;
    }

    result = az_ppc_relocate_instruction(
        original_instruction,
        target_address,
        trampoline_address,
        &plan->trampoline[0]);
    if (result != AZ_PPC_OK) {
        return result;
    }

    result = az_ppc_encode_relative_branch(
        trampoline_address + 4u,
        target_address + 4u,
        0u,
        &plan->trampoline[1]);
    if (result != AZ_PPC_OK) {
        return result;
    }

    plan->target_address = target_address;
    plan->relay_address = relay_address;
    plan->trampoline_address = trampoline_address;
    plan->detour_address = detour_address;
    plan->original_instruction = original_instruction;
    return AZ_PPC_OK;
}
