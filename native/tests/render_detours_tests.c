#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <auroraaz/render_detours.h>

enum TestEvent {
    EVENT_RENDER_ORIGINAL = 1,
    EVENT_NOTE_OVERLAY,
    EVENT_NOTE_INPUT,
    EVENT_SNAPSHOT_SELECTOR,
    EVENT_SNAPSHOT_STATUS,
    EVENT_TRY_DRAW,
    EVENT_RELEASE_TEXTURE,
    EVENT_FONT_ORIGINAL,
    EVENT_RENDER_FALLBACK,
    EVENT_FONT_FALLBACK
};

#define TEST_EVENT_CAPACITY 64u

static int failures;
static enum TestEvent events[TEST_EVENT_CAPACITY];
static size_t event_count;
static int32_t original_render_result;
static AzOverlayRendererResult note_result;
static AzOverlayRendererResult draw_result;
static AzOverlayRendererResult cleanup_results[2];
static size_t cleanup_result_index;
static void *expected_manager;
static void *expected_font;
static AzOverlayDrawRequest captured_request;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

static void push_event(enum TestEvent event)
{
    if (event_count < TEST_EVENT_CAPACITY) {
        events[event_count] = event;
        ++event_count;
    }
}

static void clear_events(void)
{
    memset(events, 0, sizeof(events));
    event_count = 0u;
}

int32_t az_rev1655_render_menu_original_fallback(
    void *game_content_manager)
{
    CHECK(game_content_manager == expected_manager);
    push_event(EVENT_RENDER_FALLBACK);
    return original_render_result;
}

void az_rev1655_font_end_original_fallback(void *font)
{
    CHECK(font == expected_font);
    push_event(EVENT_FONT_FALLBACK);
}

static int32_t render_original(void *game_content_manager)
{
    CHECK(game_content_manager == expected_manager);
    push_event(EVENT_RENDER_ORIGINAL);
    return original_render_result;
}

static void font_original(void *font)
{
    CHECK(font == expected_font);
    push_event(EVENT_FONT_ORIGINAL);
}

static AzOverlayRendererResult note_overlay(
    AzOverlayRenderer *renderer,
    void *game_content_manager,
    int32_t render_result)
{
    CHECK(renderer != NULL);
    CHECK(game_content_manager == expected_manager);
    CHECK(render_result == original_render_result);
    push_event(EVENT_NOTE_OVERLAY);
    return note_result;
}

static void note_input(
    uintptr_t game_content_manager,
    int32_t render_result)
{
    if (note_result == AZ_OVERLAY_RENDERER_OK) {
        CHECK(game_content_manager == (uintptr_t)expected_manager);
        CHECK(render_result == original_render_result);
    }
    else {
        CHECK(game_content_manager == (uintptr_t)0u);
        CHECK(render_result == -1);
    }
    push_event(EVENT_NOTE_INPUT);
}

static void snapshot_selector(AzSelectorState *selector)
{
    CHECK(selector != NULL);
    selector->mode = AZ_MODE_SELECTING;
    selector->selected_index = 12u;
    selector->applied_index = 3u;
    push_event(EVENT_SNAPSHOT_SELECTOR);
}

static void snapshot_input_status(AzInputDetourStatus *status)
{
    CHECK(status != NULL);
    status->scene_allows_capture = 1u;
    push_event(EVENT_SNAPSHOT_STATUS);
}

static AzOverlayRendererResult try_draw(
    AzOverlayRenderer *renderer,
    const AzOverlayDrawRequest *request)
{
    CHECK(renderer != NULL);
    CHECK(request != NULL);
    if (request != NULL) {
        captured_request = *request;
    }
    push_event(EVENT_TRY_DRAW);
    return draw_result;
}

static AzOverlayRendererResult release_texture(AzOverlayRenderer *renderer)
{
    AzOverlayRendererResult result;

    CHECK(renderer != NULL);
    push_event(EVENT_RELEASE_TEXTURE);
    result = cleanup_results[cleanup_result_index];
    if (cleanup_result_index + 1u <
        sizeof(cleanup_results) / sizeof(cleanup_results[0])) {
        ++cleanup_result_index;
    }
    return result;
}

static AzRev1655RenderDetourBindings make_bindings(
    AzOverlayRenderer *renderer)
{
    AzRev1655RenderDetourBindings bindings;

    memset(&bindings, 0, sizeof(bindings));
    bindings.renderer = renderer;
    bindings.render_menu_original = render_original;
    bindings.font_end_original = font_original;
    bindings.note_overlay = note_overlay;
    bindings.note_input = note_input;
    bindings.try_draw = try_draw;
    bindings.release_texture = release_texture;
    bindings.snapshot_selector = snapshot_selector;
    bindings.snapshot_input_status = snapshot_input_status;
    bindings.viewport_width = 1280.0f;
    bindings.viewport_height = 720.0f;
    return bindings;
}

static void test_unconfigured_fails_closed_to_originals(void)
{
    AzRenderDetourStatus status;

    az_rev1655_render_detours_reset();
    clear_events();
    original_render_result = -17;

    CHECK(az_rev1655_render_menu_detour_c(expected_manager) == -17);
    az_rev1655_font_end_detour_c(expected_font, 0xDEADBEEFu);
    CHECK(event_count == 2u);
    CHECK(events[0] == EVENT_RENDER_FALLBACK);
    CHECK(events[1] == EVENT_FONT_FALLBACK);
    CHECK(az_rev1655_render_detours_request_texture_cleanup() ==
        AZ_RENDER_DETOUR_NOT_CONFIGURED);

    memset(&status, 0, sizeof(status));
    az_rev1655_render_detours_snapshot_status(&status);
    CHECK(status.configured == 0u);
    CHECK(status.render_menu_calls == 1u);
    CHECK(status.render_menu_original_calls == 1u);
    CHECK(status.font_end_calls == 1u);
    CHECK(status.font_end_original_calls == 1u);
}

static void test_configuration_guards(void)
{
    AzOverlayRenderer renderer;
    AzRev1655RenderDetourBindings bindings;

    memset(&renderer, 0, sizeof(renderer));
    az_rev1655_render_detours_reset();
    CHECK(az_rev1655_render_detours_configure(NULL) ==
        AZ_RENDER_DETOUR_NULL);

    memset(&bindings, 0, sizeof(bindings));
    CHECK(az_rev1655_render_detours_configure(&bindings) ==
        AZ_RENDER_DETOUR_BAD_BINDINGS);
    clear_events();
    original_render_result = -23;
    CHECK(az_rev1655_render_menu_detour_c(expected_manager) == -23);
    az_rev1655_font_end_detour_c(expected_font, 0u);
    CHECK(event_count == 2u);
    CHECK(events[0] == EVENT_RENDER_FALLBACK);
    CHECK(events[1] == EVENT_FONT_FALLBACK);

    bindings = make_bindings(&renderer);
    bindings.viewport_width = 0.0f;
    CHECK(az_rev1655_render_detours_configure(&bindings) ==
        AZ_RENDER_DETOUR_BAD_VIEWPORT);

    bindings = make_bindings(&renderer);
    CHECK(az_rev1655_render_detours_configure(&bindings) ==
        AZ_RENDER_DETOUR_OK);
    CHECK(az_rev1655_render_detours_configure(&bindings) ==
        AZ_RENDER_DETOUR_ALREADY_CONFIGURED);
}

static void test_render_then_publish_and_draw_then_end(void)
{
    AzOverlayRenderer renderer;
    AzRev1655RenderDetourBindings bindings;
    AzRenderDetourStatus status;

    memset(&renderer, 0, sizeof(renderer));
    az_rev1655_render_detours_reset();
    bindings = make_bindings(&renderer);
    CHECK(az_rev1655_render_detours_configure(&bindings) ==
        AZ_RENDER_DETOUR_OK);

    clear_events();
    original_render_result = 0;
    note_result = AZ_OVERLAY_RENDERER_OK;
    CHECK(az_rev1655_render_menu_detour_c(expected_manager) == 0);
    CHECK(event_count == 3u);
    CHECK(events[0] == EVENT_RENDER_ORIGINAL);
    CHECK(events[1] == EVENT_NOTE_OVERLAY);
    CHECK(events[2] == EVENT_NOTE_INPUT);

    clear_events();
    memset(&captured_request, 0, sizeof(captured_request));
    draw_result = AZ_OVERLAY_RENDERER_BAD_FONT;
    az_rev1655_font_end_detour_c(
        expected_font,
        AZ_REV1655_FONT_END_CALLER_LR);
    CHECK(event_count == 4u);
    CHECK(events[0] == EVENT_SNAPSHOT_SELECTOR);
    CHECK(events[1] == EVENT_SNAPSHOT_STATUS);
    CHECK(events[2] == EVENT_TRY_DRAW);
    CHECK(events[3] == EVENT_FONT_ORIGINAL);
    CHECK(captured_request.font == expected_font);
    CHECK(captured_request.caller_lr == AZ_REV1655_FONT_END_CALLER_LR);
    CHECK(captured_request.viewport_width == 1280.0f);
    CHECK(captured_request.viewport_height == 720.0f);
    CHECK(captured_request.selector_active == 1u);
    CHECK(captured_request.selected_index == 12u);
    CHECK(captured_request.proven_modal_clear == 1u);

    memset(&status, 0, sizeof(status));
    az_rev1655_render_detours_snapshot_status(&status);
    CHECK(status.render_menu_calls == 1u);
    CHECK(status.render_menu_original_calls == 1u);
    CHECK(status.font_end_calls == 1u);
    CHECK(status.font_end_original_calls == 1u);
    CHECK(status.last_render_menu_result == 0);
    CHECK(status.last_note_result == AZ_OVERLAY_RENDERER_OK);
    CHECK(status.last_draw_result == AZ_OVERLAY_RENDERER_BAD_FONT);
}

static void test_overlay_rejection_invalidates_input_scope(void)
{
    AzOverlayRenderer renderer;
    AzRev1655RenderDetourBindings bindings;
    AzRenderDetourStatus status;

    memset(&renderer, 0, sizeof(renderer));
    az_rev1655_render_detours_reset();
    bindings = make_bindings(&renderer);
    CHECK(az_rev1655_render_detours_configure(&bindings) ==
        AZ_RENDER_DETOUR_OK);

    clear_events();
    original_render_result = 0;
    note_result = AZ_OVERLAY_RENDERER_NO_COVERFLOW;
    CHECK(az_rev1655_render_menu_detour_c(expected_manager) == 0);
    CHECK(event_count == 3u);
    CHECK(events[0] == EVENT_RENDER_ORIGINAL);
    CHECK(events[1] == EVENT_NOTE_OVERLAY);
    CHECK(events[2] == EVENT_NOTE_INPUT);

    az_rev1655_render_detours_snapshot_status(&status);
    CHECK(status.last_note_result == AZ_OVERLAY_RENDERER_NO_COVERFLOW);
}

static void test_cleanup_retries_before_original(void)
{
    AzOverlayRenderer renderer;
    AzRev1655RenderDetourBindings bindings;
    AzRenderDetourStatus status;

    memset(&renderer, 0, sizeof(renderer));
    az_rev1655_render_detours_reset();
    bindings = make_bindings(&renderer);
    CHECK(az_rev1655_render_detours_configure(&bindings) ==
        AZ_RENDER_DETOUR_OK);
    CHECK(az_rev1655_render_detours_request_texture_cleanup() ==
        AZ_RENDER_DETOUR_OK);

    cleanup_results[0] = AZ_OVERLAY_RENDERER_BUSY;
    cleanup_results[1] = AZ_OVERLAY_RENDERER_OK;
    cleanup_result_index = 0u;
    clear_events();
    az_rev1655_font_end_detour_c(
        expected_font,
        AZ_REV1655_FONT_END_CALLER_LR);
    CHECK(event_count == 2u);
    CHECK(events[0] == EVENT_RELEASE_TEXTURE);
    CHECK(events[1] == EVENT_FONT_ORIGINAL);
    az_rev1655_render_detours_snapshot_status(&status);
    CHECK(status.cleanup_requested == 1u);
    CHECK(status.cleanup_complete == 0u);
    CHECK(status.last_cleanup_result == AZ_OVERLAY_RENDERER_BUSY);

    clear_events();
    az_rev1655_font_end_detour_c(
        expected_font,
        AZ_REV1655_FONT_END_CALLER_LR);
    CHECK(event_count == 2u);
    CHECK(events[0] == EVENT_RELEASE_TEXTURE);
    CHECK(events[1] == EVENT_FONT_ORIGINAL);
    az_rev1655_render_detours_snapshot_status(&status);
    CHECK(status.cleanup_complete == 1u);
    CHECK(status.last_cleanup_result == AZ_OVERLAY_RENDERER_OK);

    clear_events();
    az_rev1655_font_end_detour_c(
        expected_font,
        AZ_REV1655_FONT_END_CALLER_LR);
    CHECK(event_count == 1u);
    CHECK(events[0] == EVENT_FONT_ORIGINAL);
    az_rev1655_render_detours_snapshot_status(&status);
    CHECK(status.font_end_calls == 3u);
    CHECK(status.font_end_original_calls == 3u);
}

int main(void)
{
    uint32_t manager_word = 0u;
    uint32_t font_word = 0u;

    expected_manager = &manager_word;
    expected_font = &font_word;

    test_unconfigured_fails_closed_to_originals();
    test_configuration_guards();
    test_render_then_publish_and_draw_then_end();
    test_overlay_rejection_invalidates_input_scope();
    test_cleanup_retries_before_original();

    CHECK(strcmp(
        az_render_detour_result_name(AZ_RENDER_DETOUR_BAD_BINDINGS),
        "bad-bindings") == 0);

    if (failures != 0) {
        fprintf(stderr, "%d render-detour test(s) failed\n", failures);
        return EXIT_FAILURE;
    }

    puts("Rev1655 render detour bridge tests passed");
    return EXIT_SUCCESS;
}
