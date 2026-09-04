#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <auroraaz/render_detours.h>

typedef char AzRenderMenuContinuationMustMatch[
    AZ_REV1655_RENDER_MENU_CONTINUE_ADDRESS ==
        AZ_REV1655_RENDER_MENU_ADDRESS + sizeof(uint32_t) ? 1 : -1];
typedef char AzFontEndContinuationMustMatch[
    AZ_REV1655_FONT_END_CONTINUE_ADDRESS ==
        AZ_REV1655_FONT_END_ADDRESS + sizeof(uint32_t) ? 1 : -1];

#define AZ_EXIT_ANIMATION_FRAMES 18u
#define AZ_SELECTION_ANIMATION_FRAMES 15u

typedef struct AzRenderDetourBridge {
    AzRev1655RenderDetourBindings bindings;
    volatile uint32_t configured;
    volatile uint32_t cleanup_requested;
    volatile uint32_t cleanup_complete;
    volatile uint32_t render_menu_calls;
    volatile uint32_t render_menu_original_calls;
    volatile uint32_t font_end_calls;
    volatile uint32_t font_end_original_calls;
    volatile int32_t last_render_menu_result;
    volatile uint32_t last_note_result;
    volatile uint32_t last_draw_result;
    volatile uint32_t last_cleanup_result;
    uint8_t last_apply_serial;
    uint8_t exit_animation_active;
    uint8_t exit_animation_index;
    uint8_t exit_animation_frame;
    uint8_t selector_was_active;
    uint8_t last_selected_index;
    uint8_t selection_animation_active;
    uint8_t selection_animation_from_index;
    uint8_t selection_animation_frame;
} AzRenderDetourBridge;

static AzRenderDetourBridge g_render_detours;

static uint32_t load_u32(const volatile uint32_t *value)
{
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static void store_u32(volatile uint32_t *value, uint32_t replacement)
{
    __atomic_store_n(value, replacement, __ATOMIC_RELEASE);
}

static void store_i32(volatile int32_t *value, int32_t replacement)
{
    __atomic_store_n(value, replacement, __ATOMIC_RELEASE);
}

static void increment_u32(volatile uint32_t *value)
{
    (void)__atomic_add_fetch(value, 1u, __ATOMIC_ACQ_REL);
}

static uint8_t viewport_is_valid(float value)
{
    return (value > 0.0f &&
        value <= AZ_RENDER_DETOUR_MAX_VIEWPORT_DIMENSION) ? 1u : 0u;
}

static uint8_t callbacks_are_complete(
    const AzRev1655RenderDetourBindings *bindings)
{
    return (bindings->renderer != NULL &&
        bindings->note_overlay != NULL &&
        bindings->note_input != NULL &&
        bindings->try_draw != NULL &&
        bindings->release_texture != NULL &&
        bindings->snapshot_selector != NULL &&
        bindings->snapshot_input_status != NULL) ? 1u : 0u;
}

static AzRev1655RenderMenuOriginalFn select_render_menu_original(void)
{
    if (load_u32(&g_render_detours.configured) != 0u &&
        g_render_detours.bindings.render_menu_original != NULL) {
        return g_render_detours.bindings.render_menu_original;
    }
    return &az_rev1655_render_menu_original_fallback;
}

static AzRev1655FontEndOriginalFn select_font_end_original(void)
{
    if (load_u32(&g_render_detours.configured) != 0u &&
        g_render_detours.bindings.font_end_original != NULL) {
        return g_render_detours.bindings.font_end_original;
    }
    return &az_rev1655_font_end_original_fallback;
}

void az_rev1655_render_detours_reset(void)
{
    /* The caller owns the documented no-live-hook precondition. */
    store_u32(&g_render_detours.configured, 0u);
    memset(&g_render_detours, 0, sizeof(g_render_detours));
    store_i32(&g_render_detours.last_render_menu_result, -1);
    store_u32(
        &g_render_detours.last_note_result,
        (uint32_t)AZ_OVERLAY_RENDERER_NOT_VALIDATED);
    store_u32(
        &g_render_detours.last_draw_result,
        (uint32_t)AZ_OVERLAY_RENDERER_NOT_VALIDATED);
    store_u32(
        &g_render_detours.last_cleanup_result,
        (uint32_t)AZ_OVERLAY_RENDERER_NOT_VALIDATED);
}

AzRenderDetourResult az_rev1655_render_detours_configure(
    const AzRev1655RenderDetourBindings *bindings)
{
    if (bindings == NULL) {
        return AZ_RENDER_DETOUR_NULL;
    }
    if (load_u32(&g_render_detours.configured) != 0u) {
        return AZ_RENDER_DETOUR_ALREADY_CONFIGURED;
    }
    if (callbacks_are_complete(bindings) == 0u) {
        return AZ_RENDER_DETOUR_BAD_BINDINGS;
    }
    if (viewport_is_valid(bindings->viewport_width) == 0u ||
        viewport_is_valid(bindings->viewport_height) == 0u) {
        return AZ_RENDER_DETOUR_BAD_VIEWPORT;
    }

    az_rev1655_render_detours_reset();
    g_render_detours.bindings = *bindings;
    if (g_render_detours.bindings.render_menu_original == NULL) {
        g_render_detours.bindings.render_menu_original =
            &az_rev1655_render_menu_original_fallback;
    }
    if (g_render_detours.bindings.font_end_original == NULL) {
        g_render_detours.bindings.font_end_original =
            &az_rev1655_font_end_original_fallback;
    }
    store_u32(&g_render_detours.configured, 1u);
    return AZ_RENDER_DETOUR_OK;
}

AzRenderDetourResult az_rev1655_render_detours_request_texture_cleanup(void)
{
    if (load_u32(&g_render_detours.configured) == 0u) {
        return AZ_RENDER_DETOUR_NOT_CONFIGURED;
    }

    store_u32(&g_render_detours.cleanup_requested, 1u);
    return AZ_RENDER_DETOUR_OK;
}

void az_rev1655_render_detours_snapshot_status(
    AzRenderDetourStatus *status)
{
    if (status == NULL) {
        return;
    }

    status->render_menu_calls = load_u32(
        &g_render_detours.render_menu_calls);
    status->render_menu_original_calls = load_u32(
        &g_render_detours.render_menu_original_calls);
    status->font_end_calls = load_u32(&g_render_detours.font_end_calls);
    status->font_end_original_calls = load_u32(
        &g_render_detours.font_end_original_calls);
    status->last_render_menu_result = __atomic_load_n(
        &g_render_detours.last_render_menu_result,
        __ATOMIC_ACQUIRE);
    status->last_note_result = (AzOverlayRendererResult)load_u32(
        &g_render_detours.last_note_result);
    status->last_draw_result = (AzOverlayRendererResult)load_u32(
        &g_render_detours.last_draw_result);
    status->last_cleanup_result = (AzOverlayRendererResult)load_u32(
        &g_render_detours.last_cleanup_result);
    status->configured = load_u32(&g_render_detours.configured) != 0u ?
        1u : 0u;
    status->cleanup_requested = load_u32(
        &g_render_detours.cleanup_requested) != 0u ? 1u : 0u;
    status->cleanup_complete = load_u32(
        &g_render_detours.cleanup_complete) != 0u ? 1u : 0u;
}

int32_t az_rev1655_render_menu_detour_c(void *game_content_manager)
{
    const AzRev1655RenderMenuOriginalFn original =
        select_render_menu_original();
    int32_t render_result;

    increment_u32(&g_render_detours.render_menu_calls);
    increment_u32(&g_render_detours.render_menu_original_calls);
    render_result = original(game_content_manager);
    store_i32(&g_render_detours.last_render_menu_result, render_result);

    /* Publish only after Aurora has restored its coverflow render targets. */
    if (load_u32(&g_render_detours.configured) != 0u) {
        const AzOverlayRendererResult note_result =
            g_render_detours.bindings.note_overlay(
                g_render_detours.bindings.renderer,
                game_content_manager,
                render_result);
        store_u32(
            &g_render_detours.last_note_result,
            (uint32_t)note_result);
        if (note_result == AZ_OVERLAY_RENDERER_OK) {
            g_render_detours.bindings.note_input(
                (uintptr_t)game_content_manager,
                render_result);
        }
        else {
            /* Publish an invalidation, never a weaker input-scope token. */
            g_render_detours.bindings.note_input((uintptr_t)0u, -1);
        }
    }

    return render_result;
}

void az_rev1655_font_end_detour_c(void *font, uint32_t caller_lr)
{
    const AzRev1655FontEndOriginalFn original = select_font_end_original();

    increment_u32(&g_render_detours.font_end_calls);
    if (load_u32(&g_render_detours.configured) != 0u) {
        if (load_u32(&g_render_detours.cleanup_requested) != 0u) {
            if (load_u32(&g_render_detours.cleanup_complete) == 0u) {
                const AzOverlayRendererResult cleanup_result =
                    g_render_detours.bindings.release_texture(
                        g_render_detours.bindings.renderer);
                store_u32(
                    &g_render_detours.last_cleanup_result,
                    (uint32_t)cleanup_result);
                if (cleanup_result == AZ_OVERLAY_RENDERER_OK ||
                    cleanup_result == AZ_OVERLAY_RENDERER_NO_DEVICE) {
                    store_u32(&g_render_detours.cleanup_complete, 1u);
                }
            }
        }
        else {
            AzSelectorState selector;
            AzInputDetourStatus input_status;
            AzOverlayDrawRequest request;
            AzOverlayRendererResult draw_result;

            memset(&selector, 0, sizeof(selector));
            memset(&input_status, 0, sizeof(input_status));
            memset(&request, 0, sizeof(request));
            /* Only the recovered final coverflow caller may trigger the XUI
             * focus probe used by the default snapshot callback. */
            if (caller_lr == AZ_REV1655_FONT_END_CALLER_LR) {
                g_render_detours.bindings.snapshot_selector(&selector);
                g_render_detours.bindings.snapshot_input_status(
                    &input_status);
            }

            request.font = font;
            request.caller_lr = caller_lr;
            request.viewport_width =
                g_render_detours.bindings.viewport_width;
            request.viewport_height =
                g_render_detours.bindings.viewport_height;
            request.selector_active = selector.mode == AZ_MODE_SELECTING ?
                1u : 0u;
            request.first_visible_index =
                selector.first_selectable_index < AZ_GLYPH_COUNT ?
                    selector.first_selectable_index : 0u;
            request.selected_index = request.selector_active != 0u ?
                selector.selected_index : request.first_visible_index;
            if (caller_lr == AZ_REV1655_FONT_END_CALLER_LR) {
                if (request.selector_active != 0u) {
                    if (g_render_detours.selector_was_active != 0u &&
                        selector.selected_index !=
                            g_render_detours.last_selected_index &&
                        g_render_detours.last_selected_index >=
                            request.first_visible_index &&
                        g_render_detours.last_selected_index <
                            AZ_GLYPH_COUNT) {
                        g_render_detours.selection_animation_active = 1u;
                        g_render_detours.selection_animation_from_index =
                            g_render_detours.last_selected_index;
                        g_render_detours.selection_animation_frame = 0u;
                    }
                    g_render_detours.selector_was_active = 1u;
                    g_render_detours.last_selected_index =
                        selector.selected_index;
                    g_render_detours.last_apply_serial =
                        selector.apply_serial;
                    g_render_detours.exit_animation_active = 0u;
                    g_render_detours.exit_animation_frame = 0u;

                    if (g_render_detours.selection_animation_active != 0u) {
                        request.selection_animation_active = 1u;
                        request.selection_animation_from_index =
                            g_render_detours.selection_animation_from_index;
                        request.selection_animation_progress =
                            (float)g_render_detours.selection_animation_frame /
                            (float)AZ_SELECTION_ANIMATION_FRAMES;
                        ++g_render_detours.selection_animation_frame;
                        if (g_render_detours.selection_animation_frame >=
                            AZ_SELECTION_ANIMATION_FRAMES) {
                            g_render_detours.selection_animation_active = 0u;
                        }
                    }
                }
                else if (selector.apply_serial !=
                    g_render_detours.last_apply_serial) {
                    g_render_detours.selector_was_active = 0u;
                    g_render_detours.selection_animation_active = 0u;
                    g_render_detours.last_apply_serial =
                        selector.apply_serial;
                    if (selector.applied_index < AZ_GLYPH_COUNT) {
                        g_render_detours.exit_animation_active = 1u;
                        g_render_detours.exit_animation_index =
                            selector.applied_index;
                        g_render_detours.exit_animation_frame = 0u;
                    }
                }
                else {
                    g_render_detours.selector_was_active = 0u;
                    g_render_detours.selection_animation_active = 0u;
                }

                if (g_render_detours.exit_animation_active != 0u) {
                    request.exit_animation_active = 1u;
                    request.exit_animation_index =
                        g_render_detours.exit_animation_index;
                    request.exit_animation_progress =
                        (float)g_render_detours.exit_animation_frame /
                        (float)AZ_EXIT_ANIMATION_FRAMES;
                    ++g_render_detours.exit_animation_frame;
                    if (g_render_detours.exit_animation_frame >=
                        AZ_EXIT_ANIMATION_FRAMES) {
                        g_render_detours.exit_animation_active = 0u;
                    }
                }
            }
            request.proven_modal_clear =
                input_status.scene_allows_capture != 0u ? 1u : 0u;

            draw_result = g_render_detours.bindings.try_draw(
                g_render_detours.bindings.renderer,
                &request);
            store_u32(
                &g_render_detours.last_draw_result,
                (uint32_t)draw_result);
        }
    }

    /* Font::End owns Aurora's state cleanup and must run exactly once. */
    increment_u32(&g_render_detours.font_end_original_calls);
    original(font);
}

const char *az_render_detour_result_name(AzRenderDetourResult result)
{
    switch (result) {
    case AZ_RENDER_DETOUR_OK:
        return "ok";
    case AZ_RENDER_DETOUR_NULL:
        return "null";
    case AZ_RENDER_DETOUR_ALREADY_CONFIGURED:
        return "already-configured";
    case AZ_RENDER_DETOUR_BAD_BINDINGS:
        return "bad-bindings";
    case AZ_RENDER_DETOUR_BAD_VIEWPORT:
        return "bad-viewport";
    case AZ_RENDER_DETOUR_NOT_CONFIGURED:
        return "not-configured";
    default:
        return "unknown";
    }
}
