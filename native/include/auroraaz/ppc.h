#ifndef AURORAAZ_PPC_H
#define AURORAAZ_PPC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZ_PPC_ABSOLUTE_BRANCH_WORDS 4u

typedef enum AzPpcResult {
    AZ_PPC_OK = 0,
    AZ_PPC_NULL,
    AZ_PPC_UNALIGNED,
    AZ_PPC_OUT_OF_RANGE,
    AZ_PPC_NOT_BRANCH,
    AZ_PPC_UNSUPPORTED,
    AZ_PPC_CAPACITY
} AzPpcResult;

/*
 * Encode a PowerPC "b"/"bl" instruction. Both addresses must be four-byte
 * aligned and the destination must be within the signed 26-bit branch range.
 */
AzPpcResult az_ppc_encode_relative_branch(
    uint32_t source_address,
    uint32_t destination_address,
    uint8_t link,
    uint32_t *instruction);

/* Resolve an unconditional PowerPC branch, including absolute branches. */
AzPpcResult az_ppc_decode_branch_target(
    uint32_t instruction,
    uint32_t instruction_address,
    uint32_t *destination_address,
    uint8_t *link);

/*
 * Relocate one instruction from one address to another. Position-independent
 * instructions are copied. Relative unconditional branches are re-encoded so
 * they retain the same destination. Relative conditional branches fail closed
 * because the exact Rev1655 hook prologues do not require them.
 */
AzPpcResult az_ppc_relocate_instruction(
    uint32_t instruction,
    uint32_t source_address,
    uint32_t destination_address,
    uint32_t *relocated_instruction);

/*
 * Emit an address-independent four-instruction jump through volatile r11 and
 * CTR. The sequence may optionally link with bctrl.
 */
AzPpcResult az_ppc_emit_absolute_branch(
    uint32_t destination_address,
    uint8_t link,
    uint32_t instructions[AZ_PPC_ABSOLUTE_BRANCH_WORDS]);

const char *az_ppc_result_name(AzPpcResult result);

#ifdef __cplusplus
}
#endif

#endif
