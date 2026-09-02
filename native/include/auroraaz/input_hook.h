#ifndef AURORAAZ_INPUT_HOOK_H
#define AURORAAZ_INPUT_HOOK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZ_REV1655_INPUT_WRAPPER_ADDRESS 0x82801D90u
#define AZ_REV1655_INPUT_CALL_CONTEXT_ADDRESS 0x822113D8u

typedef enum AzInputHookGateResult {
    AZ_INPUT_HOOK_REV1655 = 0,
    AZ_INPUT_HOOK_NULL_TEXT,
    AZ_INPUT_HOOK_BAD_TEXT_BASE,
    AZ_INPUT_HOOK_BAD_TEXT_SIZE,
    AZ_INPUT_HOOK_BAD_WRAPPER_SIGNATURE,
    AZ_INPUT_HOOK_BAD_CALL_CONTEXT_SIGNATURE
} AzInputHookGateResult;

typedef struct AzInputHookDescriptor {
    uint32_t wrapper_address;
    uint32_t call_context_address;
    uint8_t wrapper_signature_size;
    uint8_t call_context_signature_size;
} AzInputHookDescriptor;

/*
 * This module only validates the offline hook plan. It never writes a branch,
 * changes page protection, or flushes an instruction cache.
 */
AzInputHookGateResult az_input_hook_validate_rev1655(
    const uint8_t *text,
    size_t text_size,
    uint32_t text_virtual_address);

const AzInputHookDescriptor *az_input_hook_rev1655_descriptor(void);
const char *az_input_hook_gate_result_name(AzInputHookGateResult result);

#ifdef __cplusplus
}
#endif

#endif
