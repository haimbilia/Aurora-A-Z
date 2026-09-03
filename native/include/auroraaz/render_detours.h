#ifndef AURORAAZ_RENDER_DETOURS_H
#define AURORAAZ_RENDER_DETOURS_H

#include <stdint.h>

#include <auroraaz/hook_runtime.h>
#include <auroraaz/input_detour.h>
#include <auroraaz/overlay_renderer_xbox360.h>

#if AZ_HOOK_RESIDENT_RETURN_ABI_VERSION != 1
#error "render detours require resident-return ABI v1"
#endif

#if AZ_HOOK_RESIDENT_EXIT_ADMISSION_DELTA != 0x20
#error "render detour resident-return layout mismatch"
#endif

#if AZ_HOOK_SLOT_SIZE < 0xA0
#error "render detour resident slot is too small"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Exact Aurora 0.7b.2 Rev1655 entry-hook contracts. */
#define AZ_REV1655_RENDER_MENU_CONTINUE_ADDRESS 0x82358A0Cu
#define AZ_REV1655_FONT_END_CONTINUE_ADDRESS 0x8247E394u
#define AZ_REV1655_RENDER_MENU_FIRST_INSTRUCTION 0x7D8802A6u
#define AZ_REV1655_FONT_END_FIRST_INSTRUCTION 0x7D8802A6u

#define AZ_RENDER_DETOUR_MAX_VIEWPORT_DIMENSION 8192.0f

/*
 * Required hook-runtime ABI v1. The runtime reserves at least 0xA0 bytes per
 * live slot, emits its resident exit epilogue at relay+0x70, stores admission
 * state at relay+0x90, flushes the complete slot before opening admission, and
 * keeps that title-memory slot mapped and unreused until title exit. The entry
 * relay passes the admission address in r0.
 *
 * At tail handoff the shim supplies:
 *   r11 = AzResidentAdmission* (active_entries at +0)
 *   r12 = Aurora's original caller return address
 *   r3  = Aurora's original return value, when the target returns a value
 *   r1  = Aurora's restored stack pointer
 * The resident epilogue decrements active_entries once, restores LR from r12,
 * and returns. This closes the zero-count/module-instruction-fetch race that
 * an in-module decrement cannot close.
 */

typedef int32_t (*AzRev1655RenderMenuOriginalFn)(
    void *game_content_manager);
typedef void (*AzRev1655FontEndOriginalFn)(void *font);

typedef AzOverlayRendererResult (*AzRenderDetourNoteOverlayFn)(
    AzOverlayRenderer *renderer,
    void *game_content_manager,
    int32_t render_result);
typedef void (*AzRenderDetourNoteInputFn)(
    uintptr_t game_content_manager,
    int32_t render_result);
typedef AzOverlayRendererResult (*AzRenderDetourTryDrawFn)(
    AzOverlayRenderer *renderer,
    const AzOverlayDrawRequest *request);
typedef AzOverlayRendererResult (*AzRenderDetourReleaseTextureFn)(
    AzOverlayRenderer *renderer);
typedef void (*AzRenderDetourSnapshotSelectorFn)(
    AzSelectorState *selector);
typedef void (*AzRenderDetourSnapshotInputStatusFn)(
    AzInputDetourStatus *status);

/*
 * Bind once, before either live hook is installed. The four renderer/input
 * callbacks deliberately remain injectable so the complete callback order is
 * host-testable. A null original callback selects the exact assembly fallback
 * trampoline supplied by render_detour_shims.S.
 */
typedef struct AzRev1655RenderDetourBindings {
    AzOverlayRenderer *renderer;
    AzRev1655RenderMenuOriginalFn render_menu_original;
    AzRev1655FontEndOriginalFn font_end_original;
    AzRenderDetourNoteOverlayFn note_overlay;
    AzRenderDetourNoteInputFn note_input;
    AzRenderDetourTryDrawFn try_draw;
    AzRenderDetourReleaseTextureFn release_texture;
    AzRenderDetourSnapshotSelectorFn snapshot_selector;
    AzRenderDetourSnapshotInputStatusFn snapshot_input_status;
    float viewport_width;
    float viewport_height;
} AzRev1655RenderDetourBindings;

typedef enum AzRenderDetourResult {
    AZ_RENDER_DETOUR_OK = 0,
    AZ_RENDER_DETOUR_NULL,
    AZ_RENDER_DETOUR_ALREADY_CONFIGURED,
    AZ_RENDER_DETOUR_BAD_BINDINGS,
    AZ_RENDER_DETOUR_BAD_VIEWPORT,
    AZ_RENDER_DETOUR_NOT_CONFIGURED
} AzRenderDetourResult;

typedef struct AzRenderDetourStatus {
    uint32_t render_menu_calls;
    uint32_t render_menu_original_calls;
    uint32_t font_end_calls;
    uint32_t font_end_original_calls;
    int32_t last_render_menu_result;
    AzOverlayRendererResult last_note_result;
    AzOverlayRendererResult last_draw_result;
    AzOverlayRendererResult last_cleanup_result;
    uint8_t configured;
    uint8_t cleanup_requested;
    uint8_t cleanup_complete;
} AzRenderDetourStatus;

/* Only before installation, or after all three admitted hooks have drained. */
void az_rev1655_render_detours_reset(void);

AzRenderDetourResult az_rev1655_render_detours_configure(
    const AzRev1655RenderDetourBindings *bindings);

/*
 * The lifecycle owner must first call az_overlay_renderer_begin_unload().
 * This call only asks the next Font::End render-thread callback to attempt
 * release_texture(). AZ_OVERLAY_RENDERER_BUSY is retried on a later callback;
 * OK and NO_DEVICE both complete cleanup because the renderer has discarded
 * its atlas pointer in either case.
 */
AzRenderDetourResult az_rev1655_render_detours_request_texture_cleanup(void);

void az_rev1655_render_detours_snapshot_status(
    AzRenderDetourStatus *status);

/*
 * Entry points for non-linking admitted hooks. The resident relay passes the
 * admission-state address in volatile r0. The assembly shim captures the
 * untouched caller LR, restores all ABI state, and tail-hands off while still
 * admitted to the resident-return ABI above. The resident epilogue performs
 * the only active_entries decrement and returns to Aurora.
 */
int32_t az_rev1655_render_menu_detour_entry(void *game_content_manager);
void az_rev1655_font_end_detour_entry(void *font);

/* Low-linked direct-entry variants; no resident admission pointer in r0. */
int32_t az_rev1655_render_menu_direct_detour_entry(
    void *game_content_manager);
void az_rev1655_font_end_direct_detour_entry(void *font);

/* C bridges called by render_detour_shims.S; public for host tests only. */
int32_t az_rev1655_render_menu_detour_c(void *game_content_manager);
void az_rev1655_font_end_detour_c(void *font, uint32_t caller_lr);

/* Dedicated exact-build fallbacks implemented by render_detour_shims.S. */
int32_t az_rev1655_render_menu_original_fallback(
    void *game_content_manager);
void az_rev1655_font_end_original_fallback(void *font);

const char *az_render_detour_result_name(AzRenderDetourResult result);

#ifdef __cplusplus
}
#endif

#endif
