#include <stddef.h>
#include <stdint.h>

#include <auroraaz/ppc.h>

#define AZ_PPC_PRIMARY_OPCODE_MASK 0xFC000000u
#define AZ_PPC_CONDITIONAL_BRANCH_OPCODE 0x40000000u
#define AZ_PPC_BRANCH_OPCODE 0x48000000u
#define AZ_PPC_BRANCH_DISPLACEMENT_MASK 0x03FFFFFCu
#define AZ_PPC_BRANCH_SIGN_BIT 0x02000000u
#define AZ_PPC_BRANCH_ABSOLUTE_BIT 0x00000002u
#define AZ_PPC_BRANCH_LINK_BIT 0x00000001u
#define AZ_PPC_BRANCH_MIN (-33554432LL)
#define AZ_PPC_BRANCH_MAX 33554428LL

static uint8_t is_aligned(uint32_t address)
{
    return (uint8_t)((address & 3u) == 0u);
}

AzPpcResult az_ppc_encode_relative_branch(
    uint32_t source_address,
    uint32_t destination_address,
    uint8_t link,
    uint32_t *instruction)
{
    int64_t displacement;

    if (instruction == NULL) {
        return AZ_PPC_NULL;
    }
    if (is_aligned(source_address) == 0u ||
        is_aligned(destination_address) == 0u) {
        return AZ_PPC_UNALIGNED;
    }

    displacement = (int64_t)(uint64_t)destination_address -
        (int64_t)(uint64_t)source_address;
    if (displacement < AZ_PPC_BRANCH_MIN ||
        displacement > AZ_PPC_BRANCH_MAX) {
        return AZ_PPC_OUT_OF_RANGE;
    }

    *instruction = AZ_PPC_BRANCH_OPCODE |
        ((uint32_t)displacement & AZ_PPC_BRANCH_DISPLACEMENT_MASK) |
        (link != 0u ? AZ_PPC_BRANCH_LINK_BIT : 0u);
    return AZ_PPC_OK;
}

AzPpcResult az_ppc_decode_branch_target(
    uint32_t instruction,
    uint32_t instruction_address,
    uint32_t *destination_address,
    uint8_t *link)
{
    int32_t displacement;

    if (destination_address == NULL || link == NULL) {
        return AZ_PPC_NULL;
    }
    if ((instruction & AZ_PPC_PRIMARY_OPCODE_MASK) != AZ_PPC_BRANCH_OPCODE) {
        return AZ_PPC_NOT_BRANCH;
    }
    if (is_aligned(instruction_address) == 0u) {
        return AZ_PPC_UNALIGNED;
    }

    displacement = (int32_t)(instruction & AZ_PPC_BRANCH_DISPLACEMENT_MASK);
    if (((uint32_t)displacement & AZ_PPC_BRANCH_SIGN_BIT) != 0u) {
        displacement |= (int32_t)0xFC000000u;
    }

    if ((instruction & AZ_PPC_BRANCH_ABSOLUTE_BIT) != 0u) {
        *destination_address = (uint32_t)displacement;
    }
    else {
        *destination_address = instruction_address + (uint32_t)displacement;
    }
    *link = (uint8_t)(instruction & AZ_PPC_BRANCH_LINK_BIT);
    return AZ_PPC_OK;
}

AzPpcResult az_ppc_relocate_instruction(
    uint32_t instruction,
    uint32_t source_address,
    uint32_t destination_address,
    uint32_t *relocated_instruction)
{
    uint32_t branch_destination;
    uint8_t link;
    AzPpcResult result;

    if (relocated_instruction == NULL) {
        return AZ_PPC_NULL;
    }
    if (is_aligned(source_address) == 0u ||
        is_aligned(destination_address) == 0u) {
        return AZ_PPC_UNALIGNED;
    }

    if ((instruction & AZ_PPC_PRIMARY_OPCODE_MASK) ==
        AZ_PPC_CONDITIONAL_BRANCH_OPCODE) {
        return AZ_PPC_UNSUPPORTED;
    }

    if ((instruction & AZ_PPC_PRIMARY_OPCODE_MASK) != AZ_PPC_BRANCH_OPCODE ||
        (instruction & AZ_PPC_BRANCH_ABSOLUTE_BIT) != 0u) {
        *relocated_instruction = instruction;
        return AZ_PPC_OK;
    }

    result = az_ppc_decode_branch_target(
        instruction,
        source_address,
        &branch_destination,
        &link);
    if (result != AZ_PPC_OK) {
        return result;
    }

    return az_ppc_encode_relative_branch(
        destination_address,
        branch_destination,
        link,
        relocated_instruction);
}

AzPpcResult az_ppc_emit_absolute_branch(
    uint32_t destination_address,
    uint8_t link,
    uint32_t instructions[AZ_PPC_ABSOLUTE_BRANCH_WORDS])
{
    uint32_t high;
    uint32_t low;

    if (instructions == NULL) {
        return AZ_PPC_NULL;
    }
    if (is_aligned(destination_address) == 0u) {
        return AZ_PPC_UNALIGNED;
    }

    low = destination_address & 0xFFFFu;
    high = (destination_address >> 16) + (low >= 0x8000u ? 1u : 0u);

    instructions[0] = 0x3D600000u | (high & 0xFFFFu); /* lis r11, high */
    instructions[1] = 0x396B0000u | low;              /* addi r11, r11, low */
    instructions[2] = 0x7D6903A6u;                    /* mtctr r11 */
    instructions[3] = link != 0u ? 0x4E800421u : 0x4E800420u;
    return AZ_PPC_OK;
}

const char *az_ppc_result_name(AzPpcResult result)
{
    switch (result) {
    case AZ_PPC_OK:
        return "ok";
    case AZ_PPC_NULL:
        return "null";
    case AZ_PPC_UNALIGNED:
        return "unaligned";
    case AZ_PPC_OUT_OF_RANGE:
        return "out-of-range";
    case AZ_PPC_NOT_BRANCH:
        return "not-branch";
    case AZ_PPC_UNSUPPORTED:
        return "unsupported";
    case AZ_PPC_CAPACITY:
        return "capacity";
    default:
        return "unknown";
    }
}
