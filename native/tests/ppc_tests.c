#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <auroraaz/ppc.h>

static void test_relative_branch_encoding(void)
{
    uint32_t instruction = 0u;

    assert(az_ppc_encode_relative_branch(
        0x82210000u,
        0x82D48000u,
        0u,
        &instruction) == AZ_PPC_OK);
    assert(instruction == 0x48B38000u);

    assert(az_ppc_encode_relative_branch(
        0x82D48000u,
        0x82210000u,
        1u,
        &instruction) == AZ_PPC_OK);
    assert(instruction == 0x4B4C8001u);

    assert(az_ppc_encode_relative_branch(
        0x10000000u,
        0x12000000u,
        0u,
        &instruction) == AZ_PPC_OUT_OF_RANGE);
    assert(az_ppc_encode_relative_branch(
        0x10000000u,
        0x0DFFFFFCu,
        0u,
        &instruction) == AZ_PPC_OUT_OF_RANGE);
    assert(az_ppc_encode_relative_branch(
        0x10000002u,
        0x10000004u,
        0u,
        &instruction) == AZ_PPC_UNALIGNED);
}

static void test_branch_decode_and_relocation(void)
{
    uint32_t destination = 0u;
    uint32_t relocated = 0u;
    uint8_t link = 0u;

    assert(az_ppc_decode_branch_target(
        0x4860F2BDu,
        0x82358A0Cu,
        &destination,
        &link) == AZ_PPC_OK);
    assert(destination == 0x82967CC8u);
    assert(link == 1u);

    assert(az_ppc_relocate_instruction(
        0x4860F2BDu,
        0x82358A0Cu,
        0x82D48004u,
        &relocated) == AZ_PPC_OK);
    assert(az_ppc_decode_branch_target(
        relocated,
        0x82D48004u,
        &destination,
        &link) == AZ_PPC_OK);
    assert(destination == 0x82967CC8u);
    assert(link == 1u);

    assert(az_ppc_relocate_instruction(
        0x7D8802A6u,
        0x82358A08u,
        0x82D48000u,
        &relocated) == AZ_PPC_OK);
    assert(relocated == 0x7D8802A6u);

    assert(az_ppc_relocate_instruction(
        0x41820010u,
        0x82358A08u,
        0x82D48000u,
        &relocated) == AZ_PPC_UNSUPPORTED);
}

static void test_absolute_branch(void)
{
    uint32_t instructions[AZ_PPC_ABSOLUTE_BRANCH_WORDS] = { 0u };

    assert(az_ppc_emit_absolute_branch(
        0x91D0F234u,
        0u,
        instructions) == AZ_PPC_OK);
    assert(instructions[0] == 0x3D6091D1u);
    assert(instructions[1] == 0x396BF234u);
    assert(instructions[2] == 0x7D6903A6u);
    assert(instructions[3] == 0x4E800420u);

    assert(az_ppc_emit_absolute_branch(
        0x91D01234u,
        1u,
        instructions) == AZ_PPC_OK);
    assert(instructions[0] == 0x3D6091D0u);
    assert(instructions[1] == 0x396B1234u);
    assert(instructions[3] == 0x4E800421u);

    assert(az_ppc_emit_absolute_branch(
        0x91D01235u,
        0u,
        instructions) == AZ_PPC_UNALIGNED);
}

static void test_result_names(void)
{
    assert(strcmp(az_ppc_result_name(AZ_PPC_OK), "ok") == 0);
    assert(strcmp(
        az_ppc_result_name(AZ_PPC_UNSUPPORTED),
        "unsupported") == 0);
}

int main(void)
{
    test_relative_branch_encoding();
    test_branch_decode_and_relocation();
    test_absolute_branch();
    test_result_names();
    puts("ppc hook encoding tests passed");
    return 0;
}
