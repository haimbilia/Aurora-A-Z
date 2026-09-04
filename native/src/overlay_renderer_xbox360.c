#if !defined(AURORAAZ_XBOX360)
#error "overlay_renderer_xbox360.c must only be built for the Xbox 360 target"
#endif

#include <stddef.h>
#include <stdint.h>

#include <auroraaz/compatibility.h>
#include <auroraaz/filters.h>
#include <auroraaz/glyph_atlas.h>
#include <auroraaz/overlay_model.h>
#include <auroraaz/overlay_renderer_xbox360.h>

#define AZ_REV1655_MANAGER_RENDER_BYTE_OFFSET 0x225Du
#define AZ_REV1655_MANAGER_RESOURCE_1_OFFSET 0x2224u
#define AZ_REV1655_MANAGER_RESOURCE_2_OFFSET 0x222Cu
#define AZ_REV1655_MANAGER_RESOURCE_3_OFFSET 0x2234u
#define AZ_REV1655_FONT_BEGIN_COUNT_OFFSET 0xB4u
#define AZ_REV1655_FONT_TEXTURE_OFFSET 0xB8u

#define AZ_TEXTURE_FORMAT_A8R8G8B8 0x18280086u
#define AZ_TEXTURE_FETCH_FLAGS 0x80000000u
#define AZ_VERTEX_CONSTANT_BLOCK_0_DIRTY 0x8000000000000000ULL
#define AZ_TRIANGLE_STRIP 5u
#define AZ_QUAD_VERTEX_COUNT 4u
#define AZ_MAX_VIEWPORT_DIMENSION 8192.0f
#define AZ_MAX_LOCK_PITCH 0x00100000u
#define AZ_LOGICAL_VIEWPORT_WIDTH 1280.0f
#define AZ_LOGICAL_VIEWPORT_HEIGHT 720.0f
#define AZ_LOGICAL_ROW_Y_CORRECTION (-1.0f)
#define AZ_SELECTED_CROP_PADDING 2.0f

#define AZ_ATLAS_DECODE_UNINITIALIZED 0u
#define AZ_ATLAS_DECODE_IN_PROGRESS 1u
#define AZ_ATLAS_DECODE_READY 2u
#define AZ_ATLAS_DECODE_FAILED 3u

typedef char AzOverlayFontVertexSize[
    sizeof(AzOverlayFontVertex) == 16u ? 1 : -1];
typedef char AzOverlayFontVertexXOffset[
    offsetof(AzOverlayFontVertex, x) == 0u ? 1 : -1];
typedef char AzOverlayFontVertexYOffset[
    offsetof(AzOverlayFontVertex, y) == 4u ? 1 : -1];
typedef char AzOverlayFontVertexUOffset[
    offsetof(AzOverlayFontVertex, u) == 8u ? 1 : -1];
typedef char AzOverlayFontVertexVOffset[
    offsetof(AzOverlayFontVertex, v) == 10u ? 1 : -1];
typedef char AzOverlayFontVertexChannelOffset[
    offsetof(AzOverlayFontVertex, channel_selector) == 12u ? 1 : -1];
typedef char AzOverlayGlyphCountMatch[
    AZ_GLYPH_COUNT == AZ_GLYPH_ATLAS_GLYPH_COUNT ? 1 : -1];

typedef struct AzRequiredWindow {
    uint32_t offset;
    const uint8_t *bytes;
    size_t size;
} AzRequiredWindow;

static const uint8_t k_final_call_context[] = {
    0x48u, 0x26u, 0xC8u, 0x89u, 0x7Fu, 0xC3u, 0xF3u, 0x78u,
    0x48u, 0x26u, 0xCBu, 0x51u, 0x81u, 0x7Fu, 0x00u, 0xBCu,
    0x39u, 0x40u, 0x00u, 0x00u
};
static const uint8_t k_render_menu_entry[] = {
    0x7Du, 0x88u, 0x02u, 0xA6u, 0x48u, 0x60u, 0xF2u, 0xBDu,
    0xDBu, 0xC1u, 0xFFu, 0xC8u, 0xDBu, 0xE1u, 0xFFu, 0xD0u
};
static const uint8_t k_shader_initializer[] = {
    0x7Du, 0x88u, 0x02u, 0xA6u, 0x48u, 0x4Eu, 0x9Fu, 0xB1u,
    0x94u, 0x21u, 0xFFu, 0x60u, 0x3Du, 0x60u, 0x82u, 0xBCu
};
static const uint8_t k_font_begin[] = {
    0x7Du, 0x88u, 0x02u, 0xA6u, 0x48u, 0x4Eu, 0x9Cu, 0x09u,
    0xDBu, 0xA1u, 0xFFu, 0xC8u, 0xDBu, 0xC1u, 0xFFu, 0xD0u
};
static const uint8_t k_font_end[] = {
    0x7Du, 0x88u, 0x02u, 0xA6u, 0x48u, 0x4Eu, 0x99u, 0x35u,
    0x94u, 0x21u, 0xFFu, 0x80u, 0x81u, 0x63u, 0x00u, 0xB4u
};
static const uint8_t k_texture_unlock[] = {
    0x81u, 0x63u, 0x00u, 0x30u, 0x81u, 0x43u, 0x00u, 0x20u,
    0x55u, 0x65u, 0x00u, 0x26u, 0x55u, 0x44u, 0x00u, 0x26u,
    0x48u, 0x00u, 0x86u, 0x58u
};
static const uint8_t k_texture_lock[] = {
    0x7Cu, 0xE9u, 0x3Bu, 0x78u, 0x7Cu, 0xC8u, 0x33u, 0x78u,
    0x7Cu, 0xA7u, 0x2Bu, 0x78u, 0x7Cu, 0x86u, 0x23u, 0x78u
};
static const uint8_t k_texture_create[] = {
    0x7Du, 0x88u, 0x02u, 0xA6u, 0x48u, 0x1Fu, 0x59u, 0xADu,
    0x94u, 0x21u, 0xFFu, 0x20u, 0x7Cu, 0x7Cu, 0x1Bu, 0x78u
};
static const uint8_t k_texture_bind[] = {
    0x7Du, 0x88u, 0x02u, 0xA6u, 0x48u, 0x1Fu, 0x56u, 0x25u,
    0x94u, 0x21u, 0xFFu, 0x40u, 0x39u, 0x64u, 0x0Cu, 0x9Eu
};
static const uint8_t k_vs_constants[] = {
    0x39u, 0x44u, 0x00u, 0x78u, 0x7Cu, 0xABu, 0x2Bu, 0x78u,
    0x55u, 0x4Au, 0x20u, 0x36u, 0x7Cu, 0xC9u, 0x33u, 0x78u
};
static const uint8_t k_transient_draw[] = {
    0x7Du, 0x88u, 0x02u, 0xA6u, 0x48u, 0x1Eu, 0x37u, 0xA5u,
    0x94u, 0x21u, 0xFFu, 0x80u, 0x7Cu, 0xDCu, 0x33u, 0x78u
};

static const AzRequiredWindow k_required_windows[] = {
    { 0x00001838u, k_final_call_context, sizeof(k_final_call_context) },
    { 0x00148A08u, k_render_menu_entry, sizeof(k_render_menu_entry) },
    { 0x0026DD10u, k_shader_initializer, sizeof(k_shader_initializer) },
    { 0x0026E0C0u, k_font_begin, sizeof(k_font_begin) },
    { 0x0026E390u, k_font_end, sizeof(k_font_end) },
    { 0x005606B8u, k_texture_unlock, sizeof(k_texture_unlock) },
    { 0x005622E0u, k_texture_lock, sizeof(k_texture_lock) },
    { 0x00562300u, k_texture_create, sizeof(k_texture_create) },
    { 0x00562680u, k_texture_bind, sizeof(k_texture_bind) },
    { 0x0056EC20u, k_vs_constants, sizeof(k_vs_constants) },
    { 0x00574520u, k_transient_draw, sizeof(k_transient_draw) }
};

static uint8_t g_atlas_decode_scratch[AZ_GLYPH_ATLAS_PIXEL_COUNT];
static volatile uint32_t g_atlas_decode_state =
    AZ_ATLAS_DECODE_UNINITIALIZED;

static int bytes_equal(
    const uint8_t *actual,
    const uint8_t *expected,
    size_t size)
{
    size_t index;

    for (index = 0u; index < size; ++index) {
        if (actual[index] != expected[index]) {
            return 0;
        }
    }

    return 1;
}

static void clear_bindings(AzOverlayRendererBindings *bindings)
{
    bindings->is_address_valid = NULL;
    bindings->is_system_ui_active = NULL;
    bindings->device_slot = NULL;
    bindings->create_texture = NULL;
    bindings->lock_texture = NULL;
    bindings->unlock_texture = NULL;
    bindings->set_texture = NULL;
    bindings->set_vs_constant_f = NULL;
    bindings->draw_primitive_up = NULL;
    bindings->release_resource = NULL;
}

static void clear_renderer(AzOverlayRenderer *renderer)
{
    clear_bindings(&renderer->bindings);
    renderer->render_menu_epoch = 0u;
    renderer->drawn_epoch = 0u;
    renderer->in_flight = 0u;
    renderer->drawing = 0u;
    renderer->last_game_content_manager = NULL;
    renderer->atlas_texture = NULL;
    renderer->atlas_device = NULL;
    renderer->texture_failure_epoch = 0u;
    renderer->texture_failure_pending = 0u;
    renderer->texture_ready = 0u;
    renderer->unloading = 0u;
    renderer->rev1655_validated = 0u;
}

static int valid_range(
    const AzOverlayRenderer *renderer,
    const void *address,
    size_t size)
{
    const uintptr_t first = (uintptr_t)address;
    uintptr_t last;

    if (renderer == NULL ||
        renderer->bindings.is_address_valid == NULL ||
        address == NULL || size == 0u) {
        return 0;
    }
    if (first > UINTPTR_MAX - (size - 1u)) {
        return 0;
    }
    last = first + size - 1u;

    return renderer->bindings.is_address_valid((void *)first) != 0 &&
        renderer->bindings.is_address_valid((void *)last) != 0;
}

static int is_u32_aligned(const void *address)
{
    return (((uintptr_t)address) & (sizeof(uint32_t) - 1u)) == 0u;
}

static uint32_t read_u32(const void *base, uint32_t offset)
{
    const volatile uint32_t *value =
        (const volatile uint32_t *)((const uint8_t *)base + offset);
    return *value;
}

static uint8_t read_u8(const void *base, uint32_t offset)
{
    const volatile uint8_t *value =
        (const volatile uint8_t *)((const uint8_t *)base + offset);
    return *value;
}

static int bindings_complete(const AzOverlayRendererBindings *bindings)
{
    return bindings != NULL &&
        bindings->is_address_valid != NULL &&
        bindings->is_system_ui_active != NULL &&
        bindings->device_slot != NULL &&
        bindings->create_texture != NULL &&
        bindings->lock_texture != NULL &&
        bindings->unlock_texture != NULL &&
        bindings->set_texture != NULL &&
        bindings->set_vs_constant_f != NULL &&
        bindings->draw_primitive_up != NULL &&
        bindings->release_resource != NULL;
}

static void bind_rev1655_functions(
    AzOverlayRendererBindings *bindings,
    AzOverlayAddressValidFn is_address_valid,
    AzOverlaySystemUiActiveFn is_system_ui_active)
{
    bindings->is_address_valid = is_address_valid;
    bindings->is_system_ui_active = is_system_ui_active;
    bindings->device_slot =
        (const volatile uint32_t *)(uintptr_t)
            AZ_REV1655_DEVICE_SLOT_ADDRESS;
    bindings->create_texture =
        (AzOverlayCreateTextureFn)(uintptr_t)0x82772300u;
    bindings->lock_texture =
        (AzOverlayLockTextureFn)(uintptr_t)0x827722E0u;
    bindings->unlock_texture =
        (AzOverlayUnlockTextureFn)(uintptr_t)0x827706B8u;
    bindings->set_texture =
        (AzOverlaySetTextureFn)(uintptr_t)0x82772680u;
    bindings->set_vs_constant_f =
        (AzOverlaySetVertexShaderConstantFFn)(uintptr_t)0x8277EC20u;
    bindings->draw_primitive_up =
        (AzOverlayDrawPrimitiveUpFn)(uintptr_t)0x82784520u;
    bindings->release_resource =
        (AzOverlayReleaseResourceFn)(uintptr_t)0x82779DE0u;
}

static AzOverlayRendererResult validate_required_windows(
    const uint8_t *text,
    size_t text_size,
    uint32_t text_virtual_address,
    uint8_t image_hash_allowlisted,
    AzOverlayAddressValidFn is_address_valid)
{
    size_t index;

    if (text == NULL ||
        image_hash_allowlisted == 0u ||
        is_address_valid == NULL ||
        text_virtual_address != AZ_REV1655_TEXT_BASE ||
        text_size != (size_t)AZ_REV1655_TEXT_SIZE) {
        return AZ_OVERLAY_RENDERER_BAD_IMAGE;
    }

    for (index = 0u;
         index < sizeof(k_required_windows) / sizeof(k_required_windows[0]);
         ++index) {
        const AzRequiredWindow *window = &k_required_windows[index];
        const uint8_t *window_address;

        if ((size_t)window->offset > text_size ||
            window->size > text_size - (size_t)window->offset) {
            return AZ_OVERLAY_RENDERER_BAD_SIGNATURE;
        }

        window_address = text + window->offset;
        if (is_address_valid((void *)window_address) == 0 ||
            is_address_valid(
                (void *)(window_address + window->size - 1u)) == 0 ||
            !bytes_equal(
                window_address,
                window->bytes,
                window->size)) {
            return AZ_OVERLAY_RENDERER_BAD_SIGNATURE;
        }
    }

    return AZ_OVERLAY_RENDERER_OK;
}

static void discard_texture(AzOverlayRenderer *renderer, uint8_t release)
{
    void *texture = renderer->atlas_texture;

    renderer->atlas_texture = NULL;
    renderer->atlas_device = NULL;
    renderer->texture_ready = 0u;

    if (release != 0u && texture != NULL && is_u32_aligned(texture) &&
        valid_range(renderer, texture, sizeof(uint32_t))) {
        (void)renderer->bindings.release_resource(texture);
    }
}

static int ensure_atlas_decoded(void)
{
    uint32_t state = __atomic_load_n(
        &g_atlas_decode_state,
        __ATOMIC_ACQUIRE);
    uint32_t expected;
    uint8_t decoded;

    if (state == AZ_ATLAS_DECODE_READY) {
        return 1;
    }
    if (state == AZ_ATLAS_DECODE_FAILED) {
        return 0;
    }

    expected = AZ_ATLAS_DECODE_UNINITIALIZED;
    if (!__atomic_compare_exchange_n(
            &g_atlas_decode_state,
            &expected,
            AZ_ATLAS_DECODE_IN_PROGRESS,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        /* Never spin in Aurora's final render pass. */
        return expected == AZ_ATLAS_DECODE_READY;
    }

    decoded = az_glyph_atlas_decode(
        g_atlas_decode_scratch,
        sizeof(g_atlas_decode_scratch));
    __atomic_store_n(
        &g_atlas_decode_state,
        decoded != 0u ? AZ_ATLAS_DECODE_READY : AZ_ATLAS_DECODE_FAILED,
        __ATOMIC_RELEASE);
    return decoded != 0u;
}

static int lock_rows_are_valid(
    const AzOverlayRenderer *renderer,
    const AzOverlayLockedRect *locked)
{
    uint32_t row;

    if (locked->bits == NULL ||
        locked->pitch < AZ_GLYPH_ATLAS_WIDTH * sizeof(uint32_t) ||
        locked->pitch > AZ_MAX_LOCK_PITCH ||
        (locked->pitch & 3u) != 0u ||
        (((uintptr_t)locked->bits) & 3u) != 0u) {
        return 0;
    }

    for (row = 0u; row < AZ_GLYPH_ATLAS_HEIGHT; ++row) {
        const uintptr_t row_offset = (uintptr_t)row * locked->pitch;
        uintptr_t row_address;

        if ((uintptr_t)locked->bits > UINTPTR_MAX - row_offset) {
            return 0;
        }
        row_address = (uintptr_t)locked->bits + row_offset;

        if (!valid_range(
                renderer,
                (const void *)row_address,
                AZ_GLYPH_ATLAS_WIDTH * sizeof(uint32_t))) {
            return 0;
        }
    }

    return 1;
}

static AzOverlayRendererResult upload_texture(
    AzOverlayRenderer *renderer,
    void *texture)
{
    AzOverlayLockedRect locked;
    uint32_t row;

    locked.pitch = 0u;
    locked.bits = NULL;

    if (!ensure_atlas_decoded()) {
        return AZ_OVERLAY_RENDERER_TEXTURE_UPLOAD_FAILED;
    }

    /* Aurora's own CPU-upload caller ignores r3 from these forwarding
     * wrappers. Their return register is unspecified; the authoritative
     * success contract is the validated locked pitch/pointer they publish. */
    (void)renderer->bindings.lock_texture(
        texture,
        0u,
        &locked,
        NULL,
        0u);

    if (!lock_rows_are_valid(renderer, &locked)) {
        (void)renderer->bindings.unlock_texture(texture, 0u);
        return AZ_OVERLAY_RENDERER_TEXTURE_UPLOAD_FAILED;
    }

    for (row = 0u; row < AZ_GLYPH_ATLAS_HEIGHT; ++row) {
        const uint8_t *source =
            g_atlas_decode_scratch +
            ((size_t)row * AZ_GLYPH_ATLAS_WIDTH);
        uint32_t *destination =
            (uint32_t *)((uint8_t *)locked.bits +
                ((size_t)row * locked.pitch));
        uint32_t column;

        for (column = 0u; column < AZ_GLYPH_ATLAS_WIDTH; ++column) {
            destination[column] =
                ((uint32_t)source[column] << 24u) | 0x00FFFFFFu;
        }
    }

    (void)renderer->bindings.unlock_texture(texture, 0u);

    return AZ_OVERLAY_RENDERER_OK;
}

static AzOverlayRendererResult ensure_texture(
    AzOverlayRenderer *renderer,
    void *device,
    uint32_t epoch)
{
    void *texture;
    AzOverlayRendererResult upload_result;

    if (renderer->atlas_device != device) {
        discard_texture(renderer, 1u);
        renderer->texture_failure_pending = 0u;
    }

    if (renderer->texture_ready != 0u &&
        renderer->atlas_texture != NULL) {
        if (is_u32_aligned(renderer->atlas_texture) &&
            valid_range(
                renderer,
                renderer->atlas_texture,
                sizeof(uint32_t))) {
            return AZ_OVERLAY_RENDERER_OK;
        }
        discard_texture(renderer, 0u);
    }

    if (renderer->texture_failure_pending != 0u &&
        epoch - renderer->texture_failure_epoch <
            AZ_OVERLAY_TEXTURE_RETRY_EPOCHS) {
        return AZ_OVERLAY_RENDERER_TEXTURE_COOLDOWN;
    }

    texture = renderer->bindings.create_texture(
        AZ_GLYPH_ATLAS_WIDTH,
        AZ_GLYPH_ATLAS_HEIGHT,
        1u,
        1u,
        0u,
        AZ_TEXTURE_FORMAT_A8R8G8B8,
        1u,
        3u);
    if (texture == NULL || !is_u32_aligned(texture) ||
        !valid_range(renderer, texture, sizeof(uint32_t))) {
        renderer->texture_failure_epoch = epoch;
        renderer->texture_failure_pending = 1u;
        return AZ_OVERLAY_RENDERER_TEXTURE_CREATE_FAILED;
    }

    upload_result = upload_texture(renderer, texture);
    if (upload_result != AZ_OVERLAY_RENDERER_OK) {
        if (is_u32_aligned(texture) &&
            valid_range(renderer, texture, sizeof(uint32_t))) {
            (void)renderer->bindings.release_resource(texture);
        }
        renderer->texture_failure_epoch = epoch;
        renderer->texture_failure_pending = 1u;
        return upload_result;
    }

    renderer->atlas_texture = texture;
    renderer->atlas_device = device;
    renderer->texture_failure_epoch = 0u;
    renderer->texture_failure_pending = 0u;
    renderer->texture_ready = 1u;
    return AZ_OVERLAY_RENDERER_OK;
}

static int quad_is_valid(
    const AzOverlayDrawRequest *request,
    const AzOverlayQuad *quad)
{
    if (!(quad->width > 0.0f) || !(quad->height > 0.0f) ||
        !(quad->source_x >= 0.0f) || !(quad->source_y >= 0.0f) ||
        !(quad->source_width > 0.0f) ||
        !(quad->source_height > 0.0f) ||
        quad->source_x + quad->source_width >
            (float)AZ_GLYPH_ATLAS_WIDTH ||
        quad->source_y + quad->source_height >
            (float)AZ_GLYPH_ATLAS_HEIGHT ||
        !(quad->x >= 0.0f) || !(quad->y >= 0.0f) ||
        quad->x + quad->width > request->viewport_width ||
        quad->y + quad->height > request->viewport_height) {
        return 0;
    }

    return 1;
}

static void color_to_float4(uint32_t color, float *rgba)
{
    const float scale = 1.0f / 255.0f;

    rgba[0] = (float)((color >> 16u) & 0xFFu) * scale;
    rgba[1] = (float)((color >> 8u) & 0xFFu) * scale;
    rgba[2] = (float)(color & 0xFFu) * scale;
    rgba[3] = (float)((color >> 24u) & 0xFFu) * scale;
}

static void expand_selected_crop(AzOverlayQuad *quad)
{
    const float available_left = quad->source_x;
    const float available_right =
        (float)AZ_GLYPH_ATLAS_WIDTH -
        (quad->source_x + quad->source_width);
    const float padding_left =
        available_left < AZ_SELECTED_CROP_PADDING ?
            available_left : AZ_SELECTED_CROP_PADDING;
    const float padding_right =
        available_right < AZ_SELECTED_CROP_PADDING ?
            available_right : AZ_SELECTED_CROP_PADDING;
    const float destination_scale = quad->width / quad->source_width;

    quad->x -= padding_left * destination_scale;
    quad->width += (padding_left + padding_right) * destination_scale;
    quad->source_x -= padding_left;
    quad->source_width += padding_left + padding_right;
}

static int prepare_model(
    const AzOverlayDrawRequest *request,
    AzOverlayModel *model)
{
    const float width_scale =
        request->viewport_width / AZ_LOGICAL_VIEWPORT_WIDTH;
    const float height_scale =
        request->viewport_height / AZ_LOGICAL_VIEWPORT_HEIGHT;
    const float scale =
        width_scale < height_scale ? width_scale : height_scale;
    const float origin_x =
        (request->viewport_width -
            (AZ_LOGICAL_VIEWPORT_WIDTH * scale)) * 0.5f;
    const float origin_y =
        (request->viewport_height -
            (AZ_LOGICAL_VIEWPORT_HEIGHT * scale)) * 0.5f;
    const size_t expected_count =
        request->selector_active != 0u ? 5u : 3u;
    size_t index;

    az_overlay_model_build(
        AZ_LOGICAL_VIEWPORT_WIDTH,
        AZ_LOGICAL_VIEWPORT_HEIGHT,
        1u,
        request->selector_active,
        request->selected_index,
        request->exit_animation_active,
        request->exit_animation_index,
        request->exit_animation_progress,
        model);

    if (model->count != expected_count ||
        model->quads[0].layer != AZ_OVERLAY_LAYER_DIM ||
        model->quads[expected_count - 2u].layer !=
            AZ_OVERLAY_LAYER_SELECTED_SHADOW ||
        model->quads[expected_count - 1u].layer !=
            AZ_OVERLAY_LAYER_SELECTED ||
        (request->selector_active != 0u &&
            (model->quads[1].layer != AZ_OVERLAY_LAYER_SHADOW ||
             model->quads[2].layer != AZ_OVERLAY_LAYER_ROW))) {
        return 0;
    }

    expand_selected_crop(&model->quads[expected_count - 2u]);
    expand_selected_crop(&model->quads[expected_count - 1u]);

    for (index = 0u; index < model->count; ++index) {
        AzOverlayQuad *quad = &model->quads[index];

        if (quad->layer == AZ_OVERLAY_LAYER_DIM) {
            quad->x = 0.0f;
            quad->y = 0.0f;
            quad->width = request->viewport_width;
            quad->height = request->viewport_height;
        }
        else {
            quad->x = origin_x + (quad->x * scale);
            quad->y = origin_y +
                ((quad->y + AZ_LOGICAL_ROW_Y_CORRECTION) * scale);
            quad->width *= scale;
            quad->height *= scale;
        }

        if (!quad_is_valid(request, quad)) {
            return 0;
        }
    }

    return 1;
}

static AzOverlayRendererResult draw_quad(
    AzOverlayRenderer *renderer,
    void *device,
    const AzOverlayDrawRequest *request,
    const AzOverlayQuad *quad)
{
    AzOverlayFontVertex vertices[AZ_QUAD_VERTEX_COUNT];
    float rgba[4];
    const float tex_scale[4] = {
        1.0f / (float)AZ_GLYPH_ATLAS_WIDTH,
        1.0f / (float)AZ_GLYPH_ATLAS_HEIGHT,
        0.0f,
        0.0f
    };
    const float left = quad->x;
    const float top = quad->y;
    const float right = quad->x + quad->width;
    const float bottom = quad->y + quad->height;
    int16_t source_left;
    int16_t source_top;
    int16_t source_right;
    int16_t source_bottom;

    if (!quad_is_valid(request, quad)) {
        return AZ_OVERLAY_RENDERER_BAD_QUAD;
    }

    source_left = (int16_t)quad->source_x;
    source_top = (int16_t)quad->source_y;
    source_right = (int16_t)(quad->source_x + quad->source_width);
    source_bottom = (int16_t)(quad->source_y + quad->source_height);

    vertices[0].x = left;
    vertices[0].y = top;
    vertices[0].u = source_left;
    vertices[0].v = source_top;
    vertices[0].channel_selector = 0u;

    vertices[1].x = right;
    vertices[1].y = top;
    vertices[1].u = source_right;
    vertices[1].v = source_top;
    vertices[1].channel_selector = 0u;

    vertices[2].x = right;
    vertices[2].y = bottom;
    vertices[2].u = source_right;
    vertices[2].v = source_bottom;
    vertices[2].channel_selector = 0u;

    vertices[3].x = left;
    vertices[3].y = bottom;
    vertices[3].u = source_left;
    vertices[3].v = source_bottom;
    vertices[3].channel_selector = 0u;

    color_to_float4(quad->color, rgba);
    renderer->bindings.set_texture(
        device,
        0u,
        renderer->atlas_texture,
        AZ_TEXTURE_FETCH_FLAGS);
    renderer->bindings.set_vs_constant_f(
        device,
        2u,
        tex_scale,
        1u,
        AZ_VERTEX_CONSTANT_BLOCK_0_DIRTY);
    renderer->bindings.set_vs_constant_f(
        device,
        1u,
        rgba,
        1u,
        AZ_VERTEX_CONSTANT_BLOCK_0_DIRTY);
    renderer->bindings.draw_primitive_up(
        device,
        AZ_TRIANGLE_STRIP,
        AZ_QUAD_VERTEX_COUNT,
        vertices,
        (uint32_t)sizeof(AzOverlayFontVertex));
    return AZ_OVERLAY_RENDERER_OK;
}

static AzOverlayRendererResult draw_model(
    AzOverlayRenderer *renderer,
    void *device,
    const AzOverlayDrawRequest *request)
{
    AzOverlayModel model;
    size_t index;

    /* Preflight the complete row before the first render-state mutation. */
    if (!prepare_model(request, &model)) {
        return AZ_OVERLAY_RENDERER_BAD_QUAD;
    }

    for (index = 0u; index < model.count; ++index) {
        const AzOverlayRendererResult result =
            draw_quad(renderer, device, request, &model.quads[index]);
        if (result != AZ_OVERLAY_RENDERER_OK) {
            return result;
        }
    }

    return AZ_OVERLAY_RENDERER_DRAWN;
}

AzOverlayRendererResult az_overlay_renderer_init_rev1655(
    AzOverlayRenderer *renderer,
    const uint8_t *text,
    size_t text_size,
    uint32_t text_virtual_address,
    uint8_t image_hash_allowlisted,
    AzOverlayAddressValidFn is_address_valid,
    AzOverlaySystemUiActiveFn is_system_ui_active)
{
    AzOverlayRendererResult result;

    if (renderer == NULL) {
        return AZ_OVERLAY_RENDERER_NULL;
    }
    clear_renderer(renderer);

    if (is_address_valid == NULL || is_system_ui_active == NULL) {
        return AZ_OVERLAY_RENDERER_NULL;
    }

    result = validate_required_windows(
        text,
        text_size,
        text_virtual_address,
        image_hash_allowlisted,
        is_address_valid);
    if (result != AZ_OVERLAY_RENDERER_OK) {
        return result;
    }

    bind_rev1655_functions(
        &renderer->bindings,
        is_address_valid,
        is_system_ui_active);
    if (!bindings_complete(&renderer->bindings)) {
        clear_renderer(renderer);
        return AZ_OVERLAY_RENDERER_NULL;
    }

    renderer->rev1655_validated = 1u;
    return AZ_OVERLAY_RENDERER_OK;
}

AzOverlayRendererResult az_overlay_renderer_note_render_menu(
    AzOverlayRenderer *renderer,
    void *game_content_manager,
    int32_t render_result)
{
    AzOverlayRendererResult result = AZ_OVERLAY_RENDERER_OK;
    uint32_t epoch;

    if (renderer == NULL) {
        return AZ_OVERLAY_RENDERER_NULL;
    }

    (void)__atomic_add_fetch(&renderer->in_flight, 1u, __ATOMIC_ACQ_REL);
    if (game_content_manager == NULL) {
        result = AZ_OVERLAY_RENDERER_NULL;
        goto done;
    }
    if (renderer->rev1655_validated == 0u ||
        !bindings_complete(&renderer->bindings)) {
        result = AZ_OVERLAY_RENDERER_NOT_VALIDATED;
        goto done;
    }
    if (__atomic_load_n(&renderer->unloading, __ATOMIC_ACQUIRE) != 0u) {
        result = AZ_OVERLAY_RENDERER_UNLOADING;
        goto done;
    }
    if (!is_u32_aligned(game_content_manager) || render_result != 0 ||
        !valid_range(
            renderer,
            game_content_manager,
            AZ_REV1655_MANAGER_RENDER_BYTE_OFFSET + sizeof(uint8_t)) ||
        read_u8(
            game_content_manager,
            AZ_REV1655_MANAGER_RENDER_BYTE_OFFSET) == 0u ||
        read_u32(
            game_content_manager,
            AZ_REV1655_MANAGER_RESOURCE_1_OFFSET) == 0u ||
        read_u32(
            game_content_manager,
            AZ_REV1655_MANAGER_RESOURCE_2_OFFSET) == 0u ||
        read_u32(
            game_content_manager,
            AZ_REV1655_MANAGER_RESOURCE_3_OFFSET) == 0u) {
        result = AZ_OVERLAY_RENDERER_NO_COVERFLOW;
        goto done;
    }

    renderer->last_game_content_manager = game_content_manager;
    epoch = __atomic_add_fetch(
        &renderer->render_menu_epoch,
        1u,
        __ATOMIC_RELEASE);
    if (epoch == 0u) {
        __atomic_store_n(&renderer->render_menu_epoch, 1u, __ATOMIC_RELEASE);
        __atomic_store_n(&renderer->drawn_epoch, 0u, __ATOMIC_RELEASE);
    }

done:
    (void)__atomic_sub_fetch(&renderer->in_flight, 1u, __ATOMIC_ACQ_REL);
    return result;
}

AzOverlayRendererResult az_overlay_renderer_try_draw(
    AzOverlayRenderer *renderer,
    const AzOverlayDrawRequest *request)
{
    AzOverlayRendererResult result = AZ_OVERLAY_RENDERER_NOT_VALIDATED;
    uint32_t epoch;
    void *device;

    if (renderer == NULL || request == NULL) {
        return AZ_OVERLAY_RENDERER_NULL;
    }

    (void)__atomic_add_fetch(&renderer->in_flight, 1u, __ATOMIC_ACQ_REL);
    if (__atomic_load_n(&renderer->unloading, __ATOMIC_ACQUIRE) != 0u) {
        result = AZ_OVERLAY_RENDERER_UNLOADING;
        goto done;
    }
    if (__atomic_exchange_n(&renderer->drawing, 1u, __ATOMIC_ACQ_REL) != 0u) {
        result = AZ_OVERLAY_RENDERER_BUSY;
        goto done;
    }
    if (renderer->rev1655_validated == 0u ||
        !bindings_complete(&renderer->bindings)) {
        goto draw_done;
    }
    if (request->caller_lr != AZ_REV1655_FONT_END_CALLER_LR) {
        result = AZ_OVERLAY_RENDERER_BAD_CALLER;
        goto draw_done;
    }
    if (!is_u32_aligned(request->font) ||
        !valid_range(
            renderer,
            request->font,
            AZ_REV1655_FONT_TEXTURE_OFFSET + sizeof(uint32_t)) ||
        read_u32(request->font, AZ_REV1655_FONT_BEGIN_COUNT_OFFSET) != 1u ||
        read_u32(request->font, AZ_REV1655_FONT_TEXTURE_OFFSET) == 0u) {
        result = AZ_OVERLAY_RENDERER_BAD_FONT;
        goto draw_done;
    }

    epoch = __atomic_load_n(
        &renderer->render_menu_epoch,
        __ATOMIC_ACQUIRE);
    if (epoch == 0u || epoch == __atomic_load_n(
            &renderer->drawn_epoch,
            __ATOMIC_ACQUIRE)) {
        result = AZ_OVERLAY_RENDERER_NO_COVERFLOW;
        goto draw_done;
    }

    /* Never carry a visibility token across a suppressed final pass. */
    __atomic_store_n(&renderer->drawn_epoch, epoch, __ATOMIC_RELEASE);

    if (renderer->bindings.is_system_ui_active() != 0) {
        result = AZ_OVERLAY_RENDERER_SYSTEM_UI;
        goto draw_done;
    }
    if (request->proven_modal_clear == 0u) {
        result = AZ_OVERLAY_RENDERER_MODAL_UNKNOWN;
        goto draw_done;
    }
    if (request->proven_modal_clear != 1u ||
        request->selector_active > 1u ||
        request->exit_animation_active > 1u ||
        (request->selector_active != 0u &&
            request->exit_animation_active != 0u) ||
        (request->selector_active == 1u &&
            request->selected_index >= AZ_GLYPH_COUNT) ||
        (request->exit_animation_active == 1u &&
            (request->exit_animation_index >= AZ_GLYPH_COUNT ||
             !(request->exit_animation_progress >= 0.0f) ||
             !(request->exit_animation_progress < 1.0f)))) {
        result = AZ_OVERLAY_RENDERER_BAD_REQUEST;
        goto draw_done;
    }
    if (request->selector_active == 0u &&
        request->exit_animation_active == 0u) {
        result = AZ_OVERLAY_RENDERER_OK;
        goto draw_done;
    }
    if (!(request->viewport_width > 0.0f) ||
        !(request->viewport_height > 0.0f) ||
        !(request->viewport_width <= AZ_MAX_VIEWPORT_DIMENSION) ||
        !(request->viewport_height <= AZ_MAX_VIEWPORT_DIMENSION)) {
        result = AZ_OVERLAY_RENDERER_BAD_QUAD;
        goto draw_done;
    }
    if (!valid_range(
            renderer,
            (const void *)renderer->bindings.device_slot,
            sizeof(uint32_t))) {
        result = AZ_OVERLAY_RENDERER_NO_DEVICE;
        goto draw_done;
    }

    device = (void *)(uintptr_t)(*renderer->bindings.device_slot);
    if (!is_u32_aligned(device) ||
        !valid_range(renderer, device, sizeof(uint32_t))) {
        result = AZ_OVERLAY_RENDERER_NO_DEVICE;
        goto draw_done;
    }

    result = ensure_texture(renderer, device, epoch);
    if (result != AZ_OVERLAY_RENDERER_OK) {
        goto draw_done;
    }

    result = draw_model(renderer, device, request);

draw_done:
    __atomic_store_n(&renderer->drawing, 0u, __ATOMIC_RELEASE);
done:
    (void)__atomic_sub_fetch(&renderer->in_flight, 1u, __ATOMIC_ACQ_REL);
    return result;
}

void az_overlay_renderer_begin_unload(AzOverlayRenderer *renderer)
{
    if (renderer != NULL) {
        __atomic_store_n(&renderer->unloading, 1u, __ATOMIC_RELEASE);
    }
}

AzOverlayRendererResult az_overlay_renderer_release_texture(
    AzOverlayRenderer *renderer)
{
    if (renderer == NULL) {
        return AZ_OVERLAY_RENDERER_NULL;
    }
    if (__atomic_load_n(&renderer->unloading, __ATOMIC_ACQUIRE) == 0u) {
        return AZ_OVERLAY_RENDERER_UNLOADING;
    }
    if (renderer->rev1655_validated == 0u ||
        !bindings_complete(&renderer->bindings)) {
        return AZ_OVERLAY_RENDERER_NOT_VALIDATED;
    }
    if (__atomic_load_n(&renderer->in_flight, __ATOMIC_ACQUIRE) != 0u ||
        __atomic_load_n(&renderer->drawing, __ATOMIC_ACQUIRE) != 0u) {
        return AZ_OVERLAY_RENDERER_BUSY;
    }
    if (renderer->atlas_texture != NULL &&
        (!is_u32_aligned(renderer->atlas_texture) ||
            !valid_range(
                renderer,
                renderer->atlas_texture,
                sizeof(uint32_t)))) {
        discard_texture(renderer, 0u);
        return AZ_OVERLAY_RENDERER_NO_DEVICE;
    }

    discard_texture(renderer, 1u);
    return AZ_OVERLAY_RENDERER_OK;
}

uint32_t az_overlay_renderer_in_flight(const AzOverlayRenderer *renderer)
{
    if (renderer == NULL) {
        return 0u;
    }
    return __atomic_load_n(&renderer->in_flight, __ATOMIC_ACQUIRE);
}

const char *az_overlay_renderer_result_name(AzOverlayRendererResult result)
{
    switch (result) {
    case AZ_OVERLAY_RENDERER_OK:
        return "ok";
    case AZ_OVERLAY_RENDERER_NULL:
        return "null";
    case AZ_OVERLAY_RENDERER_BAD_IMAGE:
        return "bad-image";
    case AZ_OVERLAY_RENDERER_BAD_SIGNATURE:
        return "bad-signature";
    case AZ_OVERLAY_RENDERER_NOT_VALIDATED:
        return "not-validated";
    case AZ_OVERLAY_RENDERER_UNLOADING:
        return "unloading";
    case AZ_OVERLAY_RENDERER_BUSY:
        return "busy";
    case AZ_OVERLAY_RENDERER_BAD_CALLER:
        return "bad-caller";
    case AZ_OVERLAY_RENDERER_BAD_FONT:
        return "bad-font";
    case AZ_OVERLAY_RENDERER_NO_COVERFLOW:
        return "no-coverflow";
    case AZ_OVERLAY_RENDERER_SYSTEM_UI:
        return "system-ui";
    case AZ_OVERLAY_RENDERER_MODAL_UNKNOWN:
        return "modal-unknown";
    case AZ_OVERLAY_RENDERER_NO_DEVICE:
        return "no-device";
    case AZ_OVERLAY_RENDERER_TEXTURE_COOLDOWN:
        return "texture-cooldown";
    case AZ_OVERLAY_RENDERER_TEXTURE_CREATE_FAILED:
        return "texture-create-failed";
    case AZ_OVERLAY_RENDERER_TEXTURE_LOCK_FAILED:
        return "texture-lock-failed";
    case AZ_OVERLAY_RENDERER_TEXTURE_UPLOAD_FAILED:
        return "texture-upload-failed";
    case AZ_OVERLAY_RENDERER_BAD_REQUEST:
        return "bad-request";
    case AZ_OVERLAY_RENDERER_BAD_QUAD:
        return "bad-quad";
    case AZ_OVERLAY_RENDERER_DRAWN:
        return "drawn";
    default:
        return "unknown";
    }
}
