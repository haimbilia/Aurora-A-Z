#include <stddef.h>

#include <auroraaz/input.h>

typedef char AzInputKeystroke_must_be_eight_bytes[
    sizeof(AzInputKeystroke) == 8u ? 1 : -1];

#define AZ_CONSUMED_R3 (1u << 1u)
#define AZ_CONSUMED_DPAD_LEFT (1u << 2u)
#define AZ_CONSUMED_DPAD_RIGHT (1u << 3u)
#define AZ_CONSUMED_LSTICK_LEFT (1u << 4u)
#define AZ_CONSUMED_LSTICK_RIGHT (1u << 5u)

static AzSelectorResult selector_unhandled(void)
{
    AzSelectorResult result = { 0u, 0u, AZ_NO_GLYPH };
    return result;
}

static AzInputTranslation unknown_translation(void)
{
    AzInputTranslation translation;

    translation.control = AZ_INPUT_CONTROL_UNKNOWN;
    translation.event = AZ_INPUT_EVENT_INVALID;
    translation.command = AZ_COMMAND_NONE;
    return translation;
}

static AzInputDecision empty_decision(void)
{
    AzInputDecision decision;

    decision.translation = unknown_translation();
    decision.selector_result = selector_unhandled();
    decision.coverflow_active = 0u;
    decision.would_handle = 0u;
    decision.consume = 0u;
    decision.clear_keystroke = 0u;
    return decision;
}

static uint32_t control_mask(AzInputControl control)
{
    switch (control) {
    case AZ_INPUT_CONTROL_A:
        return 0u;
    case AZ_INPUT_CONTROL_R3:
        return AZ_CONSUMED_R3;
    case AZ_INPUT_CONTROL_DPAD_LEFT:
        return AZ_CONSUMED_DPAD_LEFT;
    case AZ_INPUT_CONTROL_DPAD_RIGHT:
        return AZ_CONSUMED_DPAD_RIGHT;
    case AZ_INPUT_CONTROL_LSTICK_LEFT:
        return AZ_CONSUMED_LSTICK_LEFT;
    case AZ_INPUT_CONTROL_LSTICK_RIGHT:
        return AZ_CONSUMED_LSTICK_RIGHT;
    case AZ_INPUT_CONTROL_RB:
    case AZ_INPUT_CONTROL_UNKNOWN:
    default:
        return 0u;
    }
}

static AzInputEvent translate_event(uint16_t flags)
{
    const uint16_t direction_flags =
        (uint16_t)(flags & (AZ_KEYSTROKE_KEYDOWN | AZ_KEYSTROKE_KEYUP));

    if (direction_flags == (AZ_KEYSTROKE_KEYDOWN | AZ_KEYSTROKE_KEYUP)) {
        return AZ_INPUT_EVENT_INVALID;
    }

    if ((flags & AZ_KEYSTROKE_KEYUP) != 0u) {
        return AZ_INPUT_EVENT_RELEASE;
    }

    if ((flags & AZ_KEYSTROKE_REPEAT) != 0u) {
        return AZ_INPUT_EVENT_REPEAT;
    }

    if ((flags & AZ_KEYSTROKE_KEYDOWN) != 0u) {
        return AZ_INPUT_EVENT_PRESS;
    }

    return AZ_INPUT_EVENT_INVALID;
}

static AzSelectorCommand translate_command(
    AzInputControl control,
    AzInputEvent event)
{
    if (event == AZ_INPUT_EVENT_PRESS) {
        switch (control) {
        case AZ_INPUT_CONTROL_A:
            return AZ_COMMAND_NONE;
        case AZ_INPUT_CONTROL_R3:
            return AZ_COMMAND_ENTER;
        case AZ_INPUT_CONTROL_DPAD_LEFT:
        case AZ_INPUT_CONTROL_LSTICK_LEFT:
            return AZ_COMMAND_PREVIOUS;
        case AZ_INPUT_CONTROL_DPAD_RIGHT:
        case AZ_INPUT_CONTROL_LSTICK_RIGHT:
            return AZ_COMMAND_NEXT;
        case AZ_INPUT_CONTROL_RB:
        case AZ_INPUT_CONTROL_UNKNOWN:
        default:
            return AZ_COMMAND_NONE;
        }
    }

    if (event == AZ_INPUT_EVENT_RELEASE &&
        control == AZ_INPUT_CONTROL_R3) {
        return AZ_COMMAND_APPLY;
    }

    if (event == AZ_INPUT_EVENT_REPEAT) {
        switch (control) {
        case AZ_INPUT_CONTROL_DPAD_LEFT:
        case AZ_INPUT_CONTROL_LSTICK_LEFT:
            return AZ_COMMAND_PREVIOUS;
        case AZ_INPUT_CONTROL_DPAD_RIGHT:
        case AZ_INPUT_CONTROL_LSTICK_RIGHT:
            return AZ_COMMAND_NEXT;
        case AZ_INPUT_CONTROL_A:
        case AZ_INPUT_CONTROL_RB:
        case AZ_INPUT_CONTROL_R3:
        case AZ_INPUT_CONTROL_UNKNOWN:
        default:
            return AZ_COMMAND_NONE;
        }
    }

    return AZ_COMMAND_NONE;
}

void az_input_runtime_init(AzInputRuntime *runtime)
{
    if (runtime == NULL) {
        return;
    }

    az_selector_init(&runtime->selector);
    runtime->coverflow.game_content_manager = (uintptr_t)0u;
    runtime->coverflow.successful_render_frame = 0u;
    runtime->coverflow.has_successful_render = 0u;
    runtime->edge_behavior = AZ_EDGE_WRAP;
    runtime->stage = AZ_INPUT_STAGE_OBSERVE_ONLY;
    runtime->consumed_controls = 0u;
}

void az_input_set_stage(AzInputRuntime *runtime, AzInputStage stage)
{
    if (runtime == NULL) {
        return;
    }

    if (stage != AZ_INPUT_STAGE_CONSUME_VERIFIED) {
        runtime->stage = AZ_INPUT_STAGE_OBSERVE_ONLY;
        runtime->consumed_controls = 0u;
        az_selector_leave_coverflow(&runtime->selector);
        return;
    }

    runtime->stage = AZ_INPUT_STAGE_CONSUME_VERIFIED;
}

void az_input_scope_note_render(
    AzCoverflowScope *scope,
    uint32_t render_frame,
    uintptr_t game_content_manager,
    int32_t render_result)
{
    if (scope == NULL) {
        return;
    }

    if (render_result != 0 || game_content_manager == (uintptr_t)0u) {
        az_input_scope_invalidate(scope);
        return;
    }

    scope->game_content_manager = game_content_manager;
    scope->successful_render_frame = render_frame;
    scope->has_successful_render = 1u;
}

void az_input_scope_invalidate(AzCoverflowScope *scope)
{
    if (scope == NULL) {
        return;
    }

    scope->game_content_manager = (uintptr_t)0u;
    scope->successful_render_frame = 0u;
    scope->has_successful_render = 0u;
}

uint8_t az_input_scope_is_active(
    const AzCoverflowScope *scope,
    const AzInputGate *gate)
{
    uint32_t frame_delta;

    if (scope == NULL || gate == NULL) {
        return 0u;
    }

    if (scope->has_successful_render == 0u ||
        scope->game_content_manager == (uintptr_t)0u ||
        gate->image_verified == 0u ||
        gate->input_hook_verified == 0u ||
        gate->scene_allows_capture == 0u) {
        return 0u;
    }

    frame_delta = gate->input_frame - scope->successful_render_frame;
    return frame_delta == 1u ? 1u : 0u;
}

AzInputTranslation az_input_translate(const AzInputKeystroke *keystroke)
{
    AzInputTranslation translation = unknown_translation();

    if (keystroke == NULL) {
        return translation;
    }

    switch (keystroke->virtual_key) {
    case AZ_VK_PAD_A:
        translation.control = AZ_INPUT_CONTROL_A;
        break;
    case AZ_VK_PAD_RSHOULDER:
        translation.control = AZ_INPUT_CONTROL_RB;
        break;
    case AZ_VK_PAD_RTHUMB_PRESS:
        translation.control = AZ_INPUT_CONTROL_R3;
        break;
    case AZ_VK_PAD_DPAD_LEFT:
        translation.control = AZ_INPUT_CONTROL_DPAD_LEFT;
        break;
    case AZ_VK_PAD_DPAD_RIGHT:
        translation.control = AZ_INPUT_CONTROL_DPAD_RIGHT;
        break;
    case AZ_VK_PAD_LTHUMB_LEFT:
        translation.control = AZ_INPUT_CONTROL_LSTICK_LEFT;
        break;
    case AZ_VK_PAD_LTHUMB_RIGHT:
        translation.control = AZ_INPUT_CONTROL_LSTICK_RIGHT;
        break;
    default:
        return translation;
    }

    translation.event = translate_event(keystroke->flags);
    translation.command =
        translate_command(translation.control, translation.event);
    return translation;
}

AzInputDecision az_input_process(
    AzInputRuntime *runtime,
    const AzInputKeystroke *keystroke,
    const AzInputGate *gate)
{
    AzInputDecision decision = empty_decision();
    AzSelectorState candidate_state;
    AzSelectorResult candidate_result;
    uint32_t mask;

    if (runtime == NULL || keystroke == NULL) {
        return decision;
    }

    decision.translation = az_input_translate(keystroke);
    decision.coverflow_active =
        az_input_scope_is_active(&runtime->coverflow, gate);

    if (decision.translation.control == AZ_INPUT_CONTROL_RB) {
        az_selector_leave_coverflow(&runtime->selector);
        return decision;
    }

    mask = control_mask(decision.translation.control);
    if (mask == 0u || decision.translation.event == AZ_INPUT_EVENT_INVALID) {
        if (decision.coverflow_active == 0u) {
            az_selector_leave_coverflow(&runtime->selector);
        }
        return decision;
    }

    if (runtime->stage == AZ_INPUT_STAGE_CONSUME_VERIFIED &&
        (runtime->consumed_controls & mask) != 0u) {
        if (decision.translation.event == AZ_INPUT_EVENT_RELEASE) {
            runtime->consumed_controls &= ~mask;
        }

        decision.consume = 1u;
        decision.clear_keystroke = 1u;

        if (decision.coverflow_active == 0u) {
            az_selector_leave_coverflow(&runtime->selector);
            return decision;
        }

        if (decision.translation.event == AZ_INPUT_EVENT_PRESS &&
            (decision.translation.command == AZ_COMMAND_PREVIOUS ||
             decision.translation.command == AZ_COMMAND_NEXT)) {
            /* Aurora reports a held analog direction as repeated KEYDOWN
             * events, not with XINPUT_KEYSTROKE_REPEAT. Treat subsequent
             * presses for an already-owned direction as repeats. */
            decision.translation.event = AZ_INPUT_EVENT_REPEAT;
        }
        if ((decision.translation.event != AZ_INPUT_EVENT_REPEAT &&
             decision.translation.event != AZ_INPUT_EVENT_RELEASE) ||
            decision.translation.command == AZ_COMMAND_NONE) {
            return decision;
        }
    }

    if (decision.coverflow_active == 0u) {
        az_selector_leave_coverflow(&runtime->selector);
        return decision;
    }

    if (decision.translation.command == AZ_COMMAND_NONE) {
        return decision;
    }

    candidate_state = runtime->selector;
    candidate_result = az_selector_dispatch(
        &candidate_state,
        decision.translation.command,
        runtime->edge_behavior,
        1u);
    decision.selector_result = candidate_result;
    decision.would_handle = candidate_result.handled;

    if (runtime->stage != AZ_INPUT_STAGE_CONSUME_VERIFIED ||
        candidate_result.handled == 0u) {
        return decision;
    }

    runtime->selector = candidate_state;
    if (decision.translation.event != AZ_INPUT_EVENT_RELEASE) {
        runtime->consumed_controls |= mask;
    }
    decision.consume = 1u;
    decision.clear_keystroke = 1u;
    return decision;
}

void az_input_apply_consumption(
    AzInputKeystroke *keystroke,
    const AzInputDecision *decision)
{
    if (keystroke == NULL || decision == NULL ||
        decision->clear_keystroke == 0u) {
        return;
    }

    keystroke->virtual_key = 0u;
    keystroke->unicode = 0u;
    keystroke->flags = 0u;
    keystroke->user_index = 0u;
    keystroke->hid_code = 0u;
}
