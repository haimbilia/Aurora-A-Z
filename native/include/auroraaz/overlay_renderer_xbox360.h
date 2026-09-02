#ifndef AURORAAZ_OVERLAY_RENDERER_XBOX360_H
#define AURORAAZ_OVERLAY_RENDERER_XBOX360_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZ_REV1655_RENDER_MENU_ADDRESS 0x82358A08u
#define AZ_REV1655_FONT_END_ADDRESS 0x8247E390u
#define AZ_REV1655_FONT_END_CALLER_LR 0x82211844u
#define AZ_REV1655_DEVICE_SLOT_ADDRESS 0x82BC6BD8u

#define AZ_OVERLAY_TEXTURE_RETRY_EPOCHS 120u

typedef int (*AzOverlayAddressValidFn)(void *address);
typedef int32_t (*AzOverlaySystemUiActiveFn)(void);

typedef void *(*AzOverlayCreateTextureFn)(
    uint32_t width,
    uint32_t height,
    uint32_t arg5,
    uint32_t arg6,
    uint32_t arg7,
    uint32_t format,
    uint32_t arg9,
    uint32_t arg10);

typedef struct AzOverlayLockedRect {
    uint32_t pitch;
    void *bits;
} AzOverlayLockedRect;

typedef int32_t (*AzOverlayLockTextureFn)(
    void *texture,
    uint32_t level,
    AzOverlayLockedRect *locked,
    const void *rect,
    uint32_t flags);
typedef int32_t (*AzOverlayUnlockTextureFn)(
    void *texture,
    uint32_t level);
typedef void (*AzOverlaySetTextureFn)(
    void *device,
    uint32_t stage,
    void *texture,
    uint32_t fetch_flags);
typedef void (*AzOverlaySetVertexShaderConstantFFn)(
    void *device,
    uint32_t start_register,
    const float *vectors,
    uint32_t vector4_count);
typedef void (*AzOverlayDrawPrimitiveUpFn)(
    void *device,
    uint32_t primitive_type,
    uint32_t vertex_count,
    const void *vertices,
    uint32_t stride);
typedef uint32_t (*AzOverlayReleaseResourceFn)(void *resource);

typedef struct AzOverlayRendererBindings {
    AzOverlayAddressValidFn is_address_valid;
    AzOverlaySystemUiActiveFn is_system_ui_active;
    const volatile uint32_t *device_slot;
    AzOverlayCreateTextureFn create_texture;
    AzOverlayLockTextureFn lock_texture;
    AzOverlayUnlockTextureFn unlock_texture;
    AzOverlaySetTextureFn set_texture;
    AzOverlaySetVertexShaderConstantFFn set_vs_constant_f;
    AzOverlayDrawPrimitiveUpFn draw_primitive_up;
    AzOverlayReleaseResourceFn release_resource;
} AzOverlayRendererBindings;

typedef struct AzOverlayRenderer {
    AzOverlayRendererBindings bindings;
    volatile uint32_t render_menu_epoch;
    volatile uint32_t drawn_epoch;
    volatile uint32_t in_flight;
    volatile uint32_t drawing;
    void *last_game_content_manager;
    void *atlas_texture;
    void *atlas_device;
    uint32_t texture_failure_epoch;
    uint8_t texture_failure_pending;
    uint8_t texture_ready;
    uint8_t unloading;
    uint8_t rev1655_validated;
} AzOverlayRenderer;

typedef struct AzOverlayDrawRequest {
    void *font;
    uint32_t caller_lr;
    float viewport_width;
    float viewport_height;
    uint8_t selector_active;
    uint8_t selected_index;
    /* Must come from a proven Aurora scene/modal predicate. */
    uint8_t proven_modal_clear;
} AzOverlayDrawRequest;

typedef struct AzOverlayFontVertex {
    float x;
    float y;
    int16_t u;
    int16_t v;
    uint32_t channel_selector;
} AzOverlayFontVertex;

typedef enum AzOverlayRendererResult {
    AZ_OVERLAY_RENDERER_OK = 0,
    AZ_OVERLAY_RENDERER_NULL,
    AZ_OVERLAY_RENDERER_BAD_IMAGE,
    AZ_OVERLAY_RENDERER_BAD_SIGNATURE,
    AZ_OVERLAY_RENDERER_NOT_VALIDATED,
    AZ_OVERLAY_RENDERER_UNLOADING,
    AZ_OVERLAY_RENDERER_BUSY,
    AZ_OVERLAY_RENDERER_BAD_CALLER,
    AZ_OVERLAY_RENDERER_BAD_FONT,
    AZ_OVERLAY_RENDERER_NO_COVERFLOW,
    AZ_OVERLAY_RENDERER_SYSTEM_UI,
    AZ_OVERLAY_RENDERER_MODAL_UNKNOWN,
    AZ_OVERLAY_RENDERER_NO_DEVICE,
    AZ_OVERLAY_RENDERER_TEXTURE_COOLDOWN,
    AZ_OVERLAY_RENDERER_TEXTURE_CREATE_FAILED,
    AZ_OVERLAY_RENDERER_TEXTURE_LOCK_FAILED,
    AZ_OVERLAY_RENDERER_TEXTURE_UPLOAD_FAILED,
    AZ_OVERLAY_RENDERER_BAD_REQUEST,
    AZ_OVERLAY_RENDERER_BAD_QUAD,
    AZ_OVERLAY_RENDERER_DRAWN
} AzOverlayRendererResult;

/*
 * Initializes only the renderer state and exact Rev1655 call bindings. It
 * installs no hook. image_hash_allowlisted must be set only after the caller
 * has matched the complete Aurora Rev1655 image hash.
 */
AzOverlayRendererResult az_overlay_renderer_init_rev1655(
    AzOverlayRenderer *renderer,
    const uint8_t *text,
    size_t text_size,
    uint32_t text_virtual_address,
    uint8_t image_hash_allowlisted,
    AzOverlayAddressValidFn is_address_valid,
    AzOverlaySystemUiActiveFn is_system_ui_active);

/*
 * Call after the original RenderMenu trampoline. This routine only validates
 * the manager and advances a visibility epoch; it never allocates or draws.
 * It participates in in_flight accounting so teardown can wait for both hook
 * callbacks after their entry branches have been removed.
 */
AzOverlayRendererResult az_overlay_renderer_note_render_menu(
    AzOverlayRenderer *renderer,
    void *game_content_manager,
    int32_t render_result);

/*
 * Call immediately before the original ATG Font::End trampoline. The caller
 * still owns invoking that trampoline exactly once, regardless of this
 * routine's result.
 */
AzOverlayRendererResult az_overlay_renderer_try_draw(
    AzOverlayRenderer *renderer,
    const AzOverlayDrawRequest *request);

/* Begin teardown only after hook removal has started. */
void az_overlay_renderer_begin_unload(AzOverlayRenderer *renderer);

/*
 * Release on the render thread after both hooks are removed and in_flight is
 * zero. The hook owner must separately guarantee that no already-branched
 * entry shim can call either callback after this check. A stale/invalid
 * texture is dropped rather than dereferenced.
 */
AzOverlayRendererResult az_overlay_renderer_release_texture(
    AzOverlayRenderer *renderer);

uint32_t az_overlay_renderer_in_flight(const AzOverlayRenderer *renderer);
const char *az_overlay_renderer_result_name(AzOverlayRendererResult result);

#ifdef __cplusplus
}
#endif

#endif
