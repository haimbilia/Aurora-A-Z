#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <auroraaz/hook_plan.h>

static void test_input_wrapper_plan(void)
{
    AzHookPlan plan;
    uint32_t destination = 0u;
    uint8_t link = 0u;

    assert(az_hook_plan_build(
        0x82801D90u,
        0x82D48000u,
        0x82D48020u,
        0x91D01000u,
        0x7D8802A6u,
        &plan) == AZ_PPC_OK);

    assert(plan.target_address == 0x82801D90u);
    assert(plan.original_instruction == 0x7D8802A6u);
    assert(plan.trampoline[0] == 0x7D8802A6u);

    assert(az_ppc_decode_branch_target(
        plan.target_branch,
        plan.target_address,
        &destination,
        &link) == AZ_PPC_OK);
    assert(destination == plan.relay_address);
    assert(link == 0u);

    assert(az_ppc_decode_branch_target(
        plan.trampoline[1],
        plan.trampoline_address + 4u,
        &destination,
        &link) == AZ_PPC_OK);
    assert(destination == plan.target_address + 4u);
    assert(link == 0u);

    assert(plan.relay[0] == 0x3D6091D0u);
    assert(plan.relay[1] == 0x396B1000u);
    assert(plan.relay[2] == 0x7D6903A6u);
    assert(plan.relay[3] == 0x4E800420u);
}

static void test_render_plan(void)
{
    AzHookPlan plan;

    assert(az_hook_plan_build(
        0x82358A08u,
        0x82D48040u,
        0x82D48060u,
        0x91D02000u,
        0x7D8802A6u,
        &plan) == AZ_PPC_OK);
    assert(plan.trampoline[0] == 0x7D8802A6u);
}

static void test_fail_closed(void)
{
    AzHookPlan plan;

    assert(az_hook_plan_build(
        0x82801D90u,
        0x90000000u,
        0x82D48020u,
        0x91D01000u,
        0x7D8802A6u,
        &plan) == AZ_PPC_OUT_OF_RANGE);
    assert(az_hook_plan_build(
        0x82801D90u,
        0x82D48000u,
        0x82D48020u,
        0x91D01000u,
        0x41820010u,
        &plan) == AZ_PPC_UNSUPPORTED);
    assert(az_hook_plan_build(
        0x82801D90u,
        0x82D48000u,
        0x82D48020u,
        0x91D01000u,
        0x7D8802A6u,
        NULL) == AZ_PPC_NULL);
}

int main(void)
{
    test_input_wrapper_plan();
    test_render_plan();
    test_fail_closed();
    puts("hook plan tests passed");
    return 0;
}
