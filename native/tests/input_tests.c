#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <auroraaz/input.h>
#include <auroraaz/input_hook.h>

#define REV1655_TEXT_BASE 0x82210000u
#define REV1655_TEXT_SIZE 0x009573DCu
#define INPUT_WRAPPER_OFFSET 0x005F1D90u
#define INPUT_CALL_CONTEXT_OFFSET 0x000013D8u

static int failures = 0;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

static const uint8_t k_input_wrapper_signature[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x91, 0x81, 0xFF, 0xF8,
    0x94, 0x21, 0xFF, 0xA0, 0x90, 0x61, 0x00, 0x74,
    0x2B, 0x03, 0x00, 0xFF
};

static const uint8_t k_input_call_context_signature[] = {
    0xFC, 0x00, 0x06, 0x9C, 0xC1, 0xBF, 0x00, 0xDC,
    0x38, 0x80, 0x00, 0xFF, 0x38, 0x60, 0x00, 0xFF
};

static AzInputKeystroke make_key(uint16_t virtual_key, uint16_t flags)
{
    AzInputKeystroke key;

    key.virtual_key = virtual_key;
    key.unicode = 0x1234u;
    key.flags = flags;
    key.user_index = 2u;
    key.hid_code = 3u;
    return key;
}

static AzInputGate valid_gate(uint32_t frame)
{
    AzInputGate gate;

    gate.input_frame = frame;
    gate.image_verified = 1u;
    gate.input_hook_verified = 1u;
    gate.scene_allows_capture = 1u;
    return gate;
}

static void arm_scope(AzInputRuntime *runtime, uint32_t render_frame)
{
    az_input_scope_note_render(
        &runtime->coverflow,
        render_frame,
        (uintptr_t)0x82345678u,
        0);
}

static void release_key(
    AzInputRuntime *runtime,
    uint16_t virtual_key,
    const AzInputGate *gate)
{
    AzInputKeystroke key = make_key(virtual_key, AZ_KEYSTROKE_KEYUP);
    AzInputDecision decision = az_input_process(runtime, &key, gate);

    CHECK(decision.consume == 1u);
    CHECK(decision.clear_keystroke == 1u);
}

static void test_translation(void)
{
    AzInputKeystroke key;
    AzInputTranslation translation;

    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYDOWN);
    translation = az_input_translate(&key);
    CHECK(translation.control == AZ_INPUT_CONTROL_R3);
    CHECK(translation.event == AZ_INPUT_EVENT_PRESS);
    CHECK(translation.command == AZ_COMMAND_ENTER);

    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYUP);
    translation = az_input_translate(&key);
    CHECK(translation.control == AZ_INPUT_CONTROL_R3);
    CHECK(translation.event == AZ_INPUT_EVENT_RELEASE);
    CHECK(translation.command == AZ_COMMAND_APPLY);

    key = make_key(AZ_VK_PAD_DPAD_LEFT, AZ_KEYSTROKE_KEYDOWN);
    translation = az_input_translate(&key);
    CHECK(translation.control == AZ_INPUT_CONTROL_DPAD_LEFT);
    CHECK(translation.command == AZ_COMMAND_PREVIOUS);

    key = make_key(AZ_VK_PAD_DPAD_RIGHT,
        (uint16_t)(AZ_KEYSTROKE_KEYDOWN | AZ_KEYSTROKE_REPEAT));
    translation = az_input_translate(&key);
    CHECK(translation.control == AZ_INPUT_CONTROL_DPAD_RIGHT);
    CHECK(translation.event == AZ_INPUT_EVENT_REPEAT);
    CHECK(translation.command == AZ_COMMAND_NEXT);

    key = make_key(AZ_VK_PAD_LTHUMB_LEFT, AZ_KEYSTROKE_REPEAT);
    translation = az_input_translate(&key);
    CHECK(translation.control == AZ_INPUT_CONTROL_LSTICK_LEFT);
    CHECK(translation.event == AZ_INPUT_EVENT_REPEAT);
    CHECK(translation.command == AZ_COMMAND_PREVIOUS);

    key = make_key(AZ_VK_PAD_LTHUMB_RIGHT, AZ_KEYSTROKE_KEYUP);
    translation = az_input_translate(&key);
    CHECK(translation.control == AZ_INPUT_CONTROL_LSTICK_RIGHT);
    CHECK(translation.event == AZ_INPUT_EVENT_RELEASE);
    CHECK(translation.command == AZ_COMMAND_NONE);

    key = make_key(AZ_VK_PAD_A, AZ_KEYSTROKE_KEYDOWN);
    translation = az_input_translate(&key);
    CHECK(translation.control == AZ_INPUT_CONTROL_A);
    CHECK(translation.command == AZ_COMMAND_NONE);

    key = make_key(AZ_VK_PAD_RSHOULDER, AZ_KEYSTROKE_KEYDOWN);
    translation = az_input_translate(&key);
    CHECK(translation.control == AZ_INPUT_CONTROL_RB);
    CHECK(translation.command == AZ_COMMAND_NONE);

    key = make_key(0x7777u, AZ_KEYSTROKE_KEYDOWN);
    translation = az_input_translate(&key);
    CHECK(translation.control == AZ_INPUT_CONTROL_UNKNOWN);
    CHECK(translation.event == AZ_INPUT_EVENT_INVALID);
    CHECK(translation.command == AZ_COMMAND_NONE);

    key = make_key(AZ_VK_PAD_A,
        (uint16_t)(AZ_KEYSTROKE_KEYDOWN | AZ_KEYSTROKE_KEYUP));
    translation = az_input_translate(&key);
    CHECK(translation.control == AZ_INPUT_CONTROL_A);
    CHECK(translation.event == AZ_INPUT_EVENT_INVALID);
    CHECK(translation.command == AZ_COMMAND_NONE);

    translation = az_input_translate(NULL);
    CHECK(translation.control == AZ_INPUT_CONTROL_UNKNOWN);
}

static void test_coverflow_scope(void)
{
    AzCoverflowScope scope = { (uintptr_t)0u, 0u, 0u };
    AzInputGate gate = valid_gate(101u);

    CHECK(az_input_scope_is_active(NULL, &gate) == 0u);
    CHECK(az_input_scope_is_active(&scope, NULL) == 0u);

    az_input_scope_note_render(&scope, 100u, (uintptr_t)0x1234u, 0);
    CHECK(az_input_scope_is_active(&scope, &gate) == 1u);

    gate.input_frame = 100u;
    CHECK(az_input_scope_is_active(&scope, &gate) == 0u);
    gate.input_frame = 102u;
    CHECK(az_input_scope_is_active(&scope, &gate) == 0u);
    gate.input_frame = 101u;

    gate.image_verified = 0u;
    CHECK(az_input_scope_is_active(&scope, &gate) == 0u);
    gate.image_verified = 1u;
    gate.input_hook_verified = 0u;
    CHECK(az_input_scope_is_active(&scope, &gate) == 0u);
    gate.input_hook_verified = 1u;
    gate.scene_allows_capture = 0u;
    CHECK(az_input_scope_is_active(&scope, &gate) == 0u);

    gate = valid_gate(0u);
    az_input_scope_note_render(
        &scope, UINT32_MAX, (uintptr_t)0x1234u, 0);
    CHECK(az_input_scope_is_active(&scope, &gate) == 1u);

    az_input_scope_note_render(&scope, 0u, (uintptr_t)0x1234u, -1);
    CHECK(scope.has_successful_render == 0u);
    az_input_scope_note_render(&scope, 0u, (uintptr_t)0u, 0);
    CHECK(scope.has_successful_render == 0u);

    az_input_scope_note_render(&scope, 9u, (uintptr_t)0x1234u, 0);
    az_input_scope_invalidate(&scope);
    CHECK(scope.game_content_manager == (uintptr_t)0u);
    CHECK(scope.has_successful_render == 0u);
}

static void test_observe_stage(void)
{
    AzInputRuntime runtime;
    AzInputGate gate = valid_gate(11u);
    AzInputKeystroke key =
        make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYDOWN);
    AzInputDecision decision;

    az_input_runtime_init(&runtime);
    arm_scope(&runtime, 10u);
    decision = az_input_process(&runtime, &key, &gate);

    CHECK(decision.coverflow_active == 1u);
    CHECK(decision.would_handle == 1u);
    CHECK(decision.consume == 0u);
    CHECK(decision.clear_keystroke == 0u);
    CHECK(runtime.selector.mode == AZ_MODE_COVERFLOW);
    CHECK(runtime.consumed_controls == 0u);
}

static void test_verified_selector_flow(void)
{
    AzInputRuntime runtime;
    AzInputGate gate = valid_gate(51u);
    AzInputKeystroke key;
    AzInputKeystroke original;
    AzInputDecision decision;

    az_input_runtime_init(&runtime);
    az_input_set_stage(&runtime, AZ_INPUT_STAGE_CONSUME_VERIFIED);
    arm_scope(&runtime, 50u);

    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYDOWN);
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 1u);
    CHECK(runtime.selector.mode == AZ_MODE_SELECTING);
    CHECK(runtime.selector.selected_index == 0u);
    az_input_apply_consumption(&key, &decision);
    CHECK(key.virtual_key == 0u);
    CHECK(key.unicode == 0u);
    CHECK(key.flags == 0u);
    CHECK(key.user_index == 0u);
    CHECK(key.hid_code == 0u);

    key = make_key(AZ_VK_PAD_DPAD_RIGHT, AZ_KEYSTROKE_KEYDOWN);
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 1u);
    CHECK(runtime.selector.selected_index == 1u);

    key.flags = (uint16_t)(AZ_KEYSTROKE_KEYDOWN | AZ_KEYSTROKE_REPEAT);
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 1u);
    CHECK(runtime.selector.selected_index == 2u);
    release_key(&runtime, AZ_VK_PAD_DPAD_RIGHT, &gate);

    key = make_key(AZ_VK_PAD_LTHUMB_RIGHT, AZ_KEYSTROKE_KEYDOWN);
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 1u);
    CHECK(runtime.selector.selected_index == 3u);
    release_key(&runtime, AZ_VK_PAD_LTHUMB_RIGHT, &gate);

    key = make_key(AZ_VK_PAD_DPAD_LEFT, AZ_KEYSTROKE_KEYDOWN);
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 1u);
    CHECK(runtime.selector.selected_index == 2u);
    release_key(&runtime, AZ_VK_PAD_DPAD_LEFT, &gate);

    key = make_key(AZ_VK_PAD_LTHUMB_LEFT, AZ_KEYSTROKE_KEYDOWN);
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 1u);
    CHECK(runtime.selector.selected_index == 1u);
    release_key(&runtime, AZ_VK_PAD_LTHUMB_LEFT, &gate);

    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYUP);
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 1u);
    CHECK(decision.selector_result.request_filter == 1u);
    CHECK(decision.selector_result.filter_index == 1u);
    CHECK(runtime.selector.mode == AZ_MODE_COVERFLOW);
    CHECK(runtime.selector.applied_index == 1u);
    CHECK(runtime.consumed_controls == 0u);

    original = make_key(AZ_VK_PAD_A, AZ_KEYSTROKE_KEYDOWN);
    key = original;
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 0u);
    CHECK(decision.selector_result.request_filter == 0u);
    az_input_apply_consumption(&key, &decision);
    CHECK(memcmp(&key, &original, sizeof(key)) == 0);
}

static void test_rb_is_never_consumed(void)
{
    AzInputRuntime runtime;
    AzInputGate gate = valid_gate(71u);
    AzInputKeystroke key;
    AzInputKeystroke original;
    AzInputDecision decision;

    az_input_runtime_init(&runtime);
    az_input_set_stage(&runtime, AZ_INPUT_STAGE_CONSUME_VERIFIED);
    arm_scope(&runtime, 70u);

    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYDOWN);
    (void)az_input_process(&runtime, &key, &gate);
    CHECK(runtime.selector.mode == AZ_MODE_SELECTING);

    original = make_key(AZ_VK_PAD_RSHOULDER, AZ_KEYSTROKE_KEYDOWN);
    key = original;
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.translation.control == AZ_INPUT_CONTROL_RB);
    CHECK(decision.consume == 0u);
    CHECK(decision.clear_keystroke == 0u);
    CHECK(runtime.selector.mode == AZ_MODE_COVERFLOW);
    az_input_apply_consumption(&key, &decision);
    CHECK(memcmp(&key, &original, sizeof(key)) == 0);

    key.flags = AZ_KEYSTROKE_KEYUP;
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 0u);
}

static void test_fail_closed_and_release_drain(void)
{
    AzInputRuntime runtime;
    AzInputGate gate = valid_gate(91u);
    AzInputKeystroke key;
    AzInputDecision decision;

    az_input_runtime_init(&runtime);
    az_input_set_stage(&runtime, AZ_INPUT_STAGE_CONSUME_VERIFIED);
    arm_scope(&runtime, 90u);

    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYDOWN);
    (void)az_input_process(&runtime, &key, &gate);

    key = make_key(AZ_VK_PAD_DPAD_RIGHT, AZ_KEYSTROKE_KEYDOWN);
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 1u);

    gate.scene_allows_capture = 0u;
    key.flags = AZ_KEYSTROKE_REPEAT;
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.coverflow_active == 0u);
    CHECK(decision.consume == 1u);
    CHECK(runtime.selector.mode == AZ_MODE_COVERFLOW);

    key.flags = AZ_KEYSTROKE_KEYUP;
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 1u);

    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYUP);
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 1u);
    CHECK(decision.selector_result.request_filter == 0u);

    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYDOWN);
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 0u);
    CHECK(runtime.selector.mode == AZ_MODE_COVERFLOW);

    gate.scene_allows_capture = 1u;
    gate.input_frame = 92u;
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 0u);

    gate.input_frame = 91u;
    gate.image_verified = 0u;
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 0u);
    gate.image_verified = 1u;
    gate.input_hook_verified = 0u;
    decision = az_input_process(&runtime, &key, &gate);
    CHECK(decision.consume == 0u);

    az_input_set_stage(&runtime, (AzInputStage)99);
    CHECK(runtime.stage == AZ_INPUT_STAGE_OBSERVE_ONLY);
    CHECK(runtime.consumed_controls == 0u);
}

static void test_input_hook_gate(void)
{
    uint8_t *text = (uint8_t *)calloc(REV1655_TEXT_SIZE, 1u);
    const AzInputHookDescriptor *descriptor;

    CHECK(text != NULL);
    if (text == NULL) {
        return;
    }

    memcpy(
        text + INPUT_WRAPPER_OFFSET,
        k_input_wrapper_signature,
        sizeof(k_input_wrapper_signature));
    memcpy(
        text + INPUT_CALL_CONTEXT_OFFSET,
        k_input_call_context_signature,
        sizeof(k_input_call_context_signature));

    CHECK(az_input_hook_validate_rev1655(
        text, REV1655_TEXT_SIZE, REV1655_TEXT_BASE) ==
        AZ_INPUT_HOOK_REV1655);
    CHECK(az_input_hook_validate_rev1655(
        NULL, REV1655_TEXT_SIZE, REV1655_TEXT_BASE) ==
        AZ_INPUT_HOOK_NULL_TEXT);
    CHECK(az_input_hook_validate_rev1655(
        text, REV1655_TEXT_SIZE, REV1655_TEXT_BASE + 4u) ==
        AZ_INPUT_HOOK_BAD_TEXT_BASE);
    CHECK(az_input_hook_validate_rev1655(
        text, REV1655_TEXT_SIZE - 1u, REV1655_TEXT_BASE) ==
        AZ_INPUT_HOOK_BAD_TEXT_SIZE);

    text[INPUT_WRAPPER_OFFSET] ^= 1u;
    CHECK(az_input_hook_validate_rev1655(
        text, REV1655_TEXT_SIZE, REV1655_TEXT_BASE) ==
        AZ_INPUT_HOOK_BAD_WRAPPER_SIGNATURE);
    text[INPUT_WRAPPER_OFFSET] ^= 1u;

    text[INPUT_CALL_CONTEXT_OFFSET] ^= 1u;
    CHECK(az_input_hook_validate_rev1655(
        text, REV1655_TEXT_SIZE, REV1655_TEXT_BASE) ==
        AZ_INPUT_HOOK_BAD_CALL_CONTEXT_SIGNATURE);

    descriptor = az_input_hook_rev1655_descriptor();
    CHECK(descriptor != NULL);
    CHECK(descriptor->wrapper_address == AZ_REV1655_INPUT_WRAPPER_ADDRESS);
    CHECK(descriptor->call_context_address ==
        AZ_REV1655_INPUT_CALL_CONTEXT_ADDRESS);
    CHECK(descriptor->wrapper_signature_size == 20u);
    CHECK(descriptor->call_context_signature_size == 16u);

    CHECK(strcmp(
        az_input_hook_gate_result_name(AZ_INPUT_HOOK_REV1655),
        "rev1655-input-hook") == 0);
    CHECK(strcmp(
        az_input_hook_gate_result_name(
            AZ_INPUT_HOOK_BAD_WRAPPER_SIGNATURE),
        "bad-input-wrapper-signature") == 0);
    CHECK(strcmp(
        az_input_hook_gate_result_name((AzInputHookGateResult)99),
        "unknown-input-hook") == 0);

    free(text);
}

int main(void)
{
    test_translation();
    test_coverflow_scope();
    test_observe_stage();
    test_verified_selector_flow();
    test_rb_is_never_consumed();
    test_fail_closed_and_release_drain();
    test_input_hook_gate();

    if (failures != 0) {
        fprintf(stderr, "%d input test assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }

    puts("AuroraAZ input policy tests passed");
    return EXIT_SUCCESS;
}
