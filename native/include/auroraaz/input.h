#ifndef AURORAAZ_INPUT_H
#define AURORAAZ_INPUT_H

#include <stdint.h>

#include <auroraaz/selector.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Conventional XDK virtual keys. Hardware logging must confirm these values. */
#define AZ_VK_PAD_A 0x5800u
#define AZ_VK_PAD_B 0x5801u
#define AZ_VK_PAD_RSHOULDER 0x5804u
#define AZ_VK_PAD_DPAD_UP 0x5810u
#define AZ_VK_PAD_DPAD_DOWN 0x5811u
#define AZ_VK_PAD_DPAD_LEFT 0x5812u
#define AZ_VK_PAD_DPAD_RIGHT 0x5813u
#define AZ_VK_PAD_LTHUMB_PRESS 0x5816u
#define AZ_VK_PAD_RTHUMB_PRESS 0x5817u
#define AZ_VK_PAD_LTHUMB_UP 0x5820u
#define AZ_VK_PAD_LTHUMB_DOWN 0x5821u
#define AZ_VK_PAD_LTHUMB_RIGHT 0x5822u
#define AZ_VK_PAD_LTHUMB_LEFT 0x5823u

#define AZ_KEYSTROKE_KEYDOWN 0x0001u
#define AZ_KEYSTROKE_KEYUP 0x0002u
#define AZ_KEYSTROKE_REPEAT 0x0004u

typedef struct AzInputKeystroke {
    uint16_t virtual_key;
    uint16_t unicode;
    uint16_t flags;
    uint8_t user_index;
    uint8_t hid_code;
} AzInputKeystroke;

typedef enum AzInputControl {
    AZ_INPUT_CONTROL_UNKNOWN = 0,
    AZ_INPUT_CONTROL_A,
    AZ_INPUT_CONTROL_RB,
    AZ_INPUT_CONTROL_R3,
    AZ_INPUT_CONTROL_DPAD_LEFT,
    AZ_INPUT_CONTROL_DPAD_RIGHT,
    AZ_INPUT_CONTROL_LSTICK_LEFT,
    AZ_INPUT_CONTROL_LSTICK_RIGHT
} AzInputControl;

typedef enum AzInputEvent {
    AZ_INPUT_EVENT_INVALID = 0,
    AZ_INPUT_EVENT_PRESS,
    AZ_INPUT_EVENT_REPEAT,
    AZ_INPUT_EVENT_RELEASE
} AzInputEvent;

typedef enum AzInputStage {
    AZ_INPUT_STAGE_OBSERVE_ONLY = 0,
    AZ_INPUT_STAGE_CONSUME_VERIFIED
} AzInputStage;

typedef struct AzInputTranslation {
    AzInputControl control;
    AzInputEvent event;
    AzSelectorCommand command;
} AzInputTranslation;

typedef struct AzCoverflowScope {
    uintptr_t game_content_manager;
    uint32_t successful_render_frame;
    uint8_t has_successful_render;
} AzCoverflowScope;

typedef struct AzInputGate {
    uint32_t input_frame;
    uint8_t image_verified;
    uint8_t input_hook_verified;
    uint8_t scene_allows_capture;
} AzInputGate;

typedef struct AzInputRuntime {
    AzSelectorState selector;
    AzCoverflowScope coverflow;
    AzEdgeBehavior edge_behavior;
    AzInputStage stage;
    uint32_t consumed_controls;
} AzInputRuntime;

typedef struct AzInputDecision {
    AzInputTranslation translation;
    AzSelectorResult selector_result;
    uint8_t coverflow_active;
    uint8_t would_handle;
    uint8_t consume;
    uint8_t clear_keystroke;
} AzInputDecision;

void az_input_runtime_init(AzInputRuntime *runtime);
void az_input_set_stage(AzInputRuntime *runtime, AzInputStage stage);

void az_input_scope_note_render(
    AzCoverflowScope *scope,
    uint32_t render_frame,
    uintptr_t game_content_manager,
    int32_t render_result);

void az_input_scope_invalidate(AzCoverflowScope *scope);

uint8_t az_input_scope_is_active(
    const AzCoverflowScope *scope,
    const AzInputGate *gate);

AzInputTranslation az_input_translate(const AzInputKeystroke *keystroke);

AzInputDecision az_input_process(
    AzInputRuntime *runtime,
    const AzInputKeystroke *keystroke,
    const AzInputGate *gate);

void az_input_apply_consumption(
    AzInputKeystroke *keystroke,
    const AzInputDecision *decision);

#ifdef __cplusplus
}
#endif

#endif
