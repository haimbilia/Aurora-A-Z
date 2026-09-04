#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <auroraaz/input.h>
#include <auroraaz/input_detour.h>
#include <auroraaz/hook_runtime.h>

static int failures = 0;
static uint32_t original_result = AZ_REV1655_INPUT_RESULT_SUCCESS;
static uint32_t browse_apply_calls = 0u;
static uintptr_t browse_apply_gcm = (uintptr_t)0u;
static uint32_t browse_apply_target = 0u;
static uint32_t browse_apply_count = 0u;
static uint8_t browse_apply_result = 1u;
static uint32_t ui_tick_calls = 0u;
static uint32_t ui_input_calls = 0u;
static uint16_t ui_input_owned_key = 0u;

static uint32_t dispatch_main(AzInputKeystroke *key);
static void release_main(uint16_t virtual_key);

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

bool MmIsAddressValid(void *address)
{
    return address != NULL;
}

uint32_t az_rev1655_input_original_fallback(
    uint32_t user_index,
    uint32_t flags,
    AzInputKeystroke *keystroke)
{
    (void)user_index;
    (void)flags;
    (void)keystroke;
    return original_result;
}

static uint8_t apply_browse_jump(
    void *context,
    uintptr_t game_content_manager,
    uint32_t target_index,
    uint32_t item_count)
{
    CHECK(context == (void *)(uintptr_t)0x1655u);
    ++browse_apply_calls;
    browse_apply_gcm = game_content_manager;
    browse_apply_target = target_index;
    browse_apply_count = item_count;
    return browse_apply_result;
}

static void ui_tick(void *context)
{
    CHECK(context == (void *)(uintptr_t)0x55AAu);
    ++ui_tick_calls;
}

static uint8_t ui_input(void *context, const AzInputKeystroke *key)
{
    CHECK(context == (void *)(uintptr_t)0xAA55u);
    ++ui_input_calls;
    return key != NULL && key->virtual_key == ui_input_owned_key ? 1u : 0u;
}

static AzInputKeystroke make_key(uint16_t virtual_key, uint16_t flags)
{
    AzInputKeystroke key;

    key.virtual_key = virtual_key;
    key.unicode = 0x1234u;
    key.flags = flags;
    key.user_index = 1u;
    key.hid_code = 2u;
    return key;
}

static void arm_next_input(void)
{
    az_rev1655_input_detour_note_render((uintptr_t)0x82345678u, 0);
    /* Models the exact final Font::End scene probe after RenderMenu. */
    az_rev1655_input_detour_set_scene_allows_capture(1u);
}

static void test_selector_requires_filter_worker(void)
{
    AzInputKeystroke key;
    AzSelectorState selector;
    AzInputDetourStatus status;
    uint8_t filter_index = AZ_NO_GLYPH;

    az_rev1655_input_detour_reset();
    az_rev1655_input_detour_publish_verification(1u, 1u, 1u, 0u);
    az_rev1655_input_detour_confirm_controls(AZ_INPUT_VERIFIED_REQUIRED);
    CHECK(az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_CONSUME) == AZ_INPUT_DETOUR_OK);

    arm_next_input();
    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);
    CHECK(key.virtual_key == AZ_VK_PAD_RTHUMB_PRESS);
    az_rev1655_input_detour_snapshot_selector(&selector);
    CHECK(selector.mode == AZ_MODE_COVERFLOW);

    arm_next_input();
    key = make_key(AZ_VK_PAD_A, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);
    CHECK(key.virtual_key == AZ_VK_PAD_A);
    CHECK(az_rev1655_input_detour_take_filter_request(&filter_index) ==
        AZ_INPUT_DETOUR_NO_FILTER);

    az_rev1655_input_detour_snapshot_status(&status);
    CHECK(status.filter_consumer_verified == 0u);
    CHECK(status.pending_filter == AZ_INPUT_DETOUR_NO_FILTER_REQUEST);
    CHECK(status.filter_in_flight == 0u);
}

static uint32_t dispatch_main(AzInputKeystroke *key)
{
    return az_rev1655_input_detour_c(
        0xFFu,
        0xFFu,
        key,
        AZ_REV1655_INPUT_MAIN_RETURN_ADDRESS);
}

static void release_main(uint16_t virtual_key)
{
    AzInputKeystroke key;

    arm_next_input();
    key = make_key(virtual_key, AZ_KEYSTROKE_KEYUP);
    CHECK(dispatch_main(&key) == AZ_REV1655_INPUT_RESULT_SUCCESS);
    CHECK(key.virtual_key == 0u);
}

static void test_stage_gates_and_observe(void)
{
    AzInputKeystroke key;
    AzInputDetourObservation observation;
    AzInputDetourStatus status;

    az_rev1655_input_detour_reset();
    CHECK(az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_OBSERVE) == AZ_INPUT_DETOUR_NOT_VERIFIED);

    az_rev1655_input_detour_publish_verification(1u, 1u, 0u, 0u);
    CHECK(az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_OBSERVE) == AZ_INPUT_DETOUR_OK);

    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYDOWN);
    CHECK(dispatch_main(&key) == AZ_REV1655_INPUT_RESULT_SUCCESS);
    CHECK(key.virtual_key == AZ_VK_PAD_RTHUMB_PRESS);
    CHECK(az_rev1655_input_detour_take_observation(&observation) ==
        AZ_INPUT_DETOUR_OK);
    CHECK(observation.translation.control == AZ_INPUT_CONTROL_R3);
    CHECK(observation.consumed == 0u);
    CHECK(observation.caller_return_address ==
        AZ_REV1655_INPUT_MAIN_RETURN_ADDRESS);

    az_rev1655_input_detour_snapshot_status(&status);
    CHECK(status.main_calls == 1u);
    CHECK(status.successful_keys == 1u);
    CHECK(status.requested_stage == AZ_INPUT_DETOUR_OBSERVE);
    CHECK(status.effective_stage == AZ_INPUT_STAGE_OBSERVE_ONLY);
    CHECK(status.consumed_controls == 0u);

    CHECK(az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_CONSUME) == AZ_INPUT_DETOUR_NOT_VERIFIED);
}

static void test_verified_flow_and_filter_queue(void)
{
    AzInputKeystroke key;
    AzSelectorState selector;
    AzInputDetourStatus status;
    uint8_t filter_index = AZ_NO_GLYPH;

    az_rev1655_input_detour_reset();
    az_rev1655_input_detour_publish_verification(1u, 1u, 1u, 1u);
    az_rev1655_input_detour_confirm_controls(AZ_INPUT_VERIFIED_REQUIRED);
    az_rev1655_input_detour_set_scene_allows_capture(1u);
    CHECK(az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_CONSUME) == AZ_INPUT_DETOUR_OK);

    arm_next_input();
    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYDOWN);
    CHECK(dispatch_main(&key) == AZ_REV1655_INPUT_RESULT_SUCCESS);
    CHECK(key.virtual_key == 0u);
    az_rev1655_input_detour_snapshot_selector(&selector);
    CHECK(selector.mode == AZ_MODE_SELECTING);
    CHECK(selector.selected_index == 0u);

    release_main(AZ_VK_PAD_RTHUMB_PRESS);
    az_rev1655_input_detour_snapshot_selector(&selector);
    CHECK(selector.mode == AZ_MODE_COVERFLOW);
    CHECK(selector.applied_index == AZ_NO_GLYPH);
    CHECK(az_rev1655_input_detour_take_filter_request(&filter_index) ==
        AZ_INPUT_DETOUR_NO_FILTER);

    arm_next_input();
    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);
    CHECK(key.virtual_key == 0u);

    arm_next_input();
    key = make_key(AZ_VK_PAD_DPAD_RIGHT, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);
    CHECK(key.virtual_key == 0u);
    az_rev1655_input_detour_snapshot_selector(&selector);
    CHECK(selector.selected_index == 1u);
    release_main(AZ_VK_PAD_DPAD_RIGHT);

    release_main(AZ_VK_PAD_RTHUMB_PRESS);
    az_rev1655_input_detour_snapshot_selector(&selector);
    CHECK(selector.mode == AZ_MODE_COVERFLOW);
    CHECK(selector.applied_index == 1u);

    arm_next_input();
    key = make_key(AZ_VK_PAD_A, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);
    CHECK(key.virtual_key == AZ_VK_PAD_A);

    CHECK(az_rev1655_input_detour_take_filter_request(&filter_index) ==
        AZ_INPUT_DETOUR_OK);
    CHECK(filter_index == 1u);
    CHECK(az_rev1655_input_detour_take_filter_request(&filter_index) ==
        AZ_INPUT_DETOUR_FILTER_BUSY);
    az_rev1655_input_detour_finish_filter_request();
    CHECK(az_rev1655_input_detour_take_filter_request(&filter_index) ==
        AZ_INPUT_DETOUR_NO_FILTER);

    az_rev1655_input_detour_snapshot_status(&status);
    CHECK(status.consumed_controls == 0u);
    CHECK(status.filter_in_flight == 0u);
    CHECK(status.pending_filter == AZ_INPUT_DETOUR_NO_FILTER_REQUEST);
    CHECK(status.observation_drops == 0u);
}

static void test_rb_and_shutdown_drain(void)
{
    AzInputKeystroke key;
    AzSelectorState selector;
    AzInputDetourStatus status;

    az_rev1655_input_detour_reset();
    az_rev1655_input_detour_publish_verification(1u, 1u, 1u, 1u);
    az_rev1655_input_detour_confirm_controls(AZ_INPUT_VERIFIED_REQUIRED);
    az_rev1655_input_detour_set_scene_allows_capture(1u);
    CHECK(az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_CONSUME) == AZ_INPUT_DETOUR_OK);

    arm_next_input();
    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);

    arm_next_input();
    key = make_key(AZ_VK_PAD_RSHOULDER, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);
    CHECK(key.virtual_key == AZ_VK_PAD_RSHOULDER);
    az_rev1655_input_detour_snapshot_selector(&selector);
    CHECK(selector.mode == AZ_MODE_COVERFLOW);
    release_main(AZ_VK_PAD_RTHUMB_PRESS);

    arm_next_input();
    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);

    arm_next_input();
    key = make_key(AZ_VK_PAD_DPAD_RIGHT, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);
    CHECK(key.virtual_key == 0u);

    CHECK(az_rev1655_input_detour_request_stage(AZ_INPUT_DETOUR_OFF) ==
        AZ_INPUT_DETOUR_OK);
    arm_next_input();
    key = make_key(AZ_VK_PAD_DPAD_LEFT, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);
    CHECK(key.virtual_key == AZ_VK_PAD_DPAD_LEFT);

    release_main(AZ_VK_PAD_DPAD_RIGHT);
    release_main(AZ_VK_PAD_RTHUMB_PRESS);
    az_rev1655_input_detour_snapshot_status(&status);
    CHECK(status.consumed_controls == 0u);

    key = make_key(AZ_VK_PAD_A, AZ_KEYSTROKE_KEYDOWN);
    (void)az_rev1655_input_detour_c(
        0xFFu,
        0xFFu,
        &key,
        AZ_REV1655_INPUT_DRAIN_RETURN_ADDRESS);
    CHECK(key.virtual_key == AZ_VK_PAD_A);
    az_rev1655_input_detour_snapshot_status(&status);
    CHECK(status.drain_calls == 1u);
}

static void test_revoked_verification_only_drains(void)
{
    AzInputKeystroke key;
    AzInputDetourStatus status;

    az_rev1655_input_detour_reset();
    az_rev1655_input_detour_publish_verification(1u, 1u, 1u, 1u);
    az_rev1655_input_detour_confirm_controls(AZ_INPUT_VERIFIED_REQUIRED);
    az_rev1655_input_detour_set_scene_allows_capture(1u);
    CHECK(az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_CONSUME) == AZ_INPUT_DETOUR_OK);

    arm_next_input();
    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);

    arm_next_input();
    key = make_key(AZ_VK_PAD_DPAD_RIGHT, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);
    CHECK(key.virtual_key == 0u);

    az_rev1655_input_detour_publish_verification(1u, 1u, 0u, 1u);
    arm_next_input();
    key = make_key(AZ_VK_PAD_DPAD_LEFT, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);
    CHECK(key.virtual_key == AZ_VK_PAD_DPAD_LEFT);

    release_main(AZ_VK_PAD_DPAD_RIGHT);
    release_main(AZ_VK_PAD_RTHUMB_PRESS);
    az_rev1655_input_detour_snapshot_status(&status);
    CHECK(status.consumed_controls == 0u);
}

static void test_shutdown_is_one_way_and_drains_owned_key(void)
{
    AzInputKeystroke key;
    AzInputDetourStatus status;
    uint8_t filter_index = AZ_NO_GLYPH;

    az_rev1655_input_detour_reset();
    az_rev1655_input_detour_publish_verification(1u, 1u, 1u, 1u);
    az_rev1655_input_detour_confirm_controls(AZ_INPUT_VERIFIED_REQUIRED);
    az_rev1655_input_detour_set_scene_allows_capture(1u);
    az_rev1655_input_detour_configure_browse_jump(
        &apply_browse_jump,
        (void *)(uintptr_t)0x1655u);
    CHECK(az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_CONSUME) == AZ_INPUT_DETOUR_OK);
    CHECK(az_rev1655_input_detour_shutdown_ready() == 0u);

    arm_next_input();
    key = make_key(AZ_VK_PAD_RTHUMB_PRESS, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);

    arm_next_input();
    key = make_key(AZ_VK_PAD_DPAD_RIGHT, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);
    CHECK(key.virtual_key == 0u);

    /* R3 release commits while the D-pad release remains owned. Keep both
     * the filter request and that owned release pending across shutdown. */
    release_main(AZ_VK_PAD_RTHUMB_PRESS);

    CHECK(az_rev1655_input_detour_publish_browse_jump(
        (uintptr_t)0x82345678u, 0u, 1u) == 1u);

    az_rev1655_input_detour_begin_shutdown();
    CHECK(az_rev1655_input_detour_shutdown_ready() == 0u);
    CHECK(az_rev1655_input_detour_publish_browse_jump(
        (uintptr_t)0x82345678u, 0u, 1u) == 0u);

    CHECK(az_rev1655_input_detour_take_filter_request(&filter_index) ==
        AZ_INPUT_DETOUR_OK);
    CHECK(filter_index == 1u);
    CHECK(az_rev1655_input_detour_shutdown_ready() == 0u);
    az_rev1655_input_detour_finish_filter_request();
    CHECK(az_rev1655_input_detour_shutdown_ready() == 0u);

    /* Publication/setter races after shutdown cannot reopen consumption. */
    az_rev1655_input_detour_publish_verification(1u, 1u, 1u, 1u);
    az_rev1655_input_detour_confirm_controls(AZ_INPUT_VERIFIED_REQUIRED);
    az_rev1655_input_detour_set_scene_allows_capture(1u);
    CHECK(az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_OBSERVE) == AZ_INPUT_DETOUR_SHUTTING_DOWN);
    CHECK(az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_CONSUME) == AZ_INPUT_DETOUR_SHUTTING_DOWN);
    CHECK(az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_OFF) == AZ_INPUT_DETOUR_OK);

    /* reset() is an initialization operation, not a shutdown escape hatch. */
    az_rev1655_input_detour_reset();
    CHECK(az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_OBSERVE) == AZ_INPUT_DETOUR_SHUTTING_DOWN);

    arm_next_input();
    key = make_key(AZ_VK_PAD_DPAD_LEFT, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);
    CHECK(key.virtual_key == AZ_VK_PAD_DPAD_LEFT);

    /* The release paired with a previously consumed press still drains. */
    release_main(AZ_VK_PAD_DPAD_RIGHT);
    CHECK(az_rev1655_input_detour_shutdown_ready() == 1u);

    az_rev1655_input_detour_snapshot_status(&status);
    CHECK(status.requested_stage == AZ_INPUT_DETOUR_OFF);
    CHECK(status.shutdown_requested == 1u);
    CHECK(status.image_verified == 0u);
    CHECK(status.input_hook_verified == 0u);
    CHECK(status.render_hook_verified == 0u);
    CHECK(status.filter_consumer_verified == 0u);
    CHECK(status.scene_allows_capture == 0u);
    CHECK(status.verified_controls == 0u);
    CHECK(status.in_flight == 0u);
    CHECK(status.consumed_controls == 0u);
}

static void test_invalid_pointer_range_fails_closed(void)
{
    AzInputDetourStatus status;
    AzInputKeystroke *wrapped = (AzInputKeystroke *)(
        UINTPTR_MAX - (uintptr_t)3u);

    az_rev1655_input_detour_reset();
    az_rev1655_input_detour_publish_verification(1u, 1u, 0u, 0u);
    CHECK(az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_OBSERVE) == AZ_INPUT_DETOUR_OK);
    CHECK(dispatch_main(wrapped) == AZ_REV1655_INPUT_RESULT_SUCCESS);
    az_rev1655_input_detour_snapshot_status(&status);
    CHECK(status.invalid_keystroke_pointers == 1u);
    CHECK(status.in_flight == 0u);
}

static void test_resident_return_layout_contract(void)
{
    CHECK(AZ_HOOK_RESIDENT_RETURN_ABI_VERSION == 1);
    CHECK(AZ_HOOK_TRAMPOLINE_OFFSET == 0x60);
    CHECK(AZ_HOOK_RESIDENT_EXIT_OFFSET == 0x70);
    CHECK(AZ_HOOK_ADMISSION_OFFSET == 0x90);
    CHECK(AZ_HOOK_RESIDENT_EXIT_ADMISSION_DELTA == 0x20);
    CHECK(AZ_HOOK_ADMISSION_OFFSET + 8 <= AZ_HOOK_SLOT_SIZE);
}

static void test_browse_jump_runs_once_on_main_thread_poll(void)
{
    AzInputKeystroke key;
    AzInputDetourStatus status;

    az_rev1655_input_detour_reset();
    browse_apply_calls = 0u;
    browse_apply_gcm = (uintptr_t)0u;
    browse_apply_target = 0u;
    browse_apply_count = 0u;
    browse_apply_result = 1u;
    az_rev1655_input_detour_configure_browse_jump(
        &apply_browse_jump,
        (void *)(uintptr_t)0x1655u);

    CHECK(az_rev1655_input_detour_publish_browse_jump(
        (uintptr_t)0x82345678u, 43u, 100u) == 1u);
    CHECK(az_rev1655_input_detour_publish_browse_jump(
        (uintptr_t)0x82345678u, 44u, 100u) == 0u);

    /* Aurora can report no keystroke on this poll; browse movement still
     * belongs on the main input thread and must not wait for a key event. */
    original_result = 1u;
    key = make_key(0u, 0u);
    (void)dispatch_main(&key);
    original_result = AZ_REV1655_INPUT_RESULT_SUCCESS;

    CHECK(browse_apply_calls == 1u);
    CHECK(browse_apply_gcm == (uintptr_t)0x82345678u);
    CHECK(browse_apply_target == 43u);
    CHECK(browse_apply_count == 100u);
    (void)dispatch_main(&key);
    CHECK(browse_apply_calls == 1u);

    az_rev1655_input_detour_snapshot_status(&status);
    CHECK(status.browse_jump_queued == 1u);
    CHECK(status.browse_jump_applied == 1u);
    CHECK(status.browse_jump_rejected == 1u);
    CHECK(status.browse_jump_pending == 0u);
    CHECK(status.browse_jump_in_flight == 0u);
}

static void test_browse_jump_validation(void)
{
    AzInputDetourStatus status;

    az_rev1655_input_detour_reset();
    az_rev1655_input_detour_configure_browse_jump(
        &apply_browse_jump,
        (void *)(uintptr_t)0x1655u);
    CHECK(az_rev1655_input_detour_publish_browse_jump(
        (uintptr_t)0u, 0u, 1u) == 0u);
    CHECK(az_rev1655_input_detour_publish_browse_jump(
        (uintptr_t)0x82345678u, 1u, 1u) == 0u);
    az_rev1655_input_detour_snapshot_status(&status);
    CHECK(status.browse_jump_pending == 0u);
}

static void test_ui_tick_runs_only_on_main_poll(void)
{
    AzInputKeystroke key = make_key(0u, 0u);

    az_rev1655_input_detour_reset();
    ui_tick_calls = 0u;
    az_rev1655_input_detour_configure_ui_tick(
        &ui_tick, (void *)(uintptr_t)0x55AAu);
    (void)dispatch_main(&key);
    CHECK(ui_tick_calls == 1u);
    (void)az_rev1655_input_detour_c(
        0u, 0u, &key, AZ_REV1655_INPUT_DRAIN_RETURN_ADDRESS);
    CHECK(ui_tick_calls == 1u);
}

static void test_ui_input_can_own_main_keystroke(void)
{
    AzInputKeystroke key;

    az_rev1655_input_detour_reset();
    ui_input_calls = 0u;
    ui_input_owned_key = AZ_VK_PAD_A;
    az_rev1655_input_detour_configure_ui_input(
        &ui_input, (void *)(uintptr_t)0xAA55u);

    key = make_key(AZ_VK_PAD_A, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);
    CHECK(ui_input_calls == 1u);
    CHECK(key.virtual_key == 0u);

    key = make_key(AZ_VK_PAD_DPAD_LEFT, AZ_KEYSTROKE_KEYDOWN);
    (void)dispatch_main(&key);
    CHECK(ui_input_calls == 2u);
    CHECK(key.virtual_key == AZ_VK_PAD_DPAD_LEFT);

    (void)az_rev1655_input_detour_c(
        0u, 0u, &key, AZ_REV1655_INPUT_DRAIN_RETURN_ADDRESS);
    CHECK(ui_input_calls == 2u);
}

int main(void)
{
    test_stage_gates_and_observe();
    test_verified_flow_and_filter_queue();
    test_selector_requires_filter_worker();
    test_rb_and_shutdown_drain();
    test_revoked_verification_only_drains();
    test_invalid_pointer_range_fails_closed();
    test_resident_return_layout_contract();
    test_browse_jump_runs_once_on_main_thread_poll();
    test_browse_jump_validation();
    test_ui_tick_runs_only_on_main_poll();
    test_ui_input_can_own_main_keystroke();
    /* One-way shutdown must be the final test that resets global state. */
    test_shutdown_is_one_way_and_drains_owned_key();

    if (failures != 0) {
        fprintf(stderr, "%d input detour assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }

    puts("AuroraAZ Xbox input detour tests passed");
    return EXIT_SUCCESS;
}
