#include <auroraaz/input_hook.h>

#define AZ_REV1655_TEXT_BASE 0x82210000u
#define AZ_REV1655_TEXT_SIZE 0x009573DCu
#define AZ_REV1655_INPUT_WRAPPER_OFFSET 0x005F1D90u
#define AZ_REV1655_INPUT_CALL_CONTEXT_OFFSET 0x000013D8u

static const uint8_t k_input_wrapper_signature[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x91, 0x81, 0xFF, 0xF8,
    0x94, 0x21, 0xFF, 0xA0, 0x90, 0x61, 0x00, 0x74,
    0x2B, 0x03, 0x00, 0xFF
};

static const uint8_t k_input_call_context_signature[] = {
    0xFC, 0x00, 0x06, 0x9C, 0xC1, 0xBF, 0x00, 0xDC,
    0x38, 0x80, 0x00, 0xFF, 0x38, 0x60, 0x00, 0xFF
};

static const AzInputHookDescriptor k_rev1655_descriptor = {
    AZ_REV1655_INPUT_WRAPPER_ADDRESS,
    AZ_REV1655_INPUT_CALL_CONTEXT_ADDRESS,
    (uint8_t)sizeof(k_input_wrapper_signature),
    (uint8_t)sizeof(k_input_call_context_signature)
};

static int bytes_equal(
    const uint8_t *actual,
    const uint8_t *expected,
    size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        if (actual[index] != expected[index]) {
            return 0;
        }
    }

    return 1;
}

AzInputHookGateResult az_input_hook_validate_rev1655(
    const uint8_t *text,
    size_t text_size,
    uint32_t text_virtual_address)
{
    if (text == NULL) {
        return AZ_INPUT_HOOK_NULL_TEXT;
    }

    if (text_virtual_address != AZ_REV1655_TEXT_BASE) {
        return AZ_INPUT_HOOK_BAD_TEXT_BASE;
    }

    if (text_size != (size_t)AZ_REV1655_TEXT_SIZE) {
        return AZ_INPUT_HOOK_BAD_TEXT_SIZE;
    }

    if (!bytes_equal(
            text + AZ_REV1655_INPUT_WRAPPER_OFFSET,
            k_input_wrapper_signature,
            sizeof(k_input_wrapper_signature))) {
        return AZ_INPUT_HOOK_BAD_WRAPPER_SIGNATURE;
    }

    if (!bytes_equal(
            text + AZ_REV1655_INPUT_CALL_CONTEXT_OFFSET,
            k_input_call_context_signature,
            sizeof(k_input_call_context_signature))) {
        return AZ_INPUT_HOOK_BAD_CALL_CONTEXT_SIGNATURE;
    }

    return AZ_INPUT_HOOK_REV1655;
}

const AzInputHookDescriptor *az_input_hook_rev1655_descriptor(void)
{
    return &k_rev1655_descriptor;
}

const char *az_input_hook_gate_result_name(AzInputHookGateResult result)
{
    switch (result) {
    case AZ_INPUT_HOOK_REV1655:
        return "rev1655-input-hook";
    case AZ_INPUT_HOOK_NULL_TEXT:
        return "null-text";
    case AZ_INPUT_HOOK_BAD_TEXT_BASE:
        return "bad-text-base";
    case AZ_INPUT_HOOK_BAD_TEXT_SIZE:
        return "bad-text-size";
    case AZ_INPUT_HOOK_BAD_WRAPPER_SIGNATURE:
        return "bad-input-wrapper-signature";
    case AZ_INPUT_HOOK_BAD_CALL_CONTEXT_SIGNATURE:
        return "bad-input-call-context-signature";
    default:
        return "unknown-input-hook";
    }
}
