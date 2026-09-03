#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <auroraaz/glyph_atlas.h>
#include <auroraaz/overlay_renderer_xbox360.h>

#define TEST_MANAGER_SIZE 0x2260u
#define TEST_MANAGER_RENDER_BYTE_OFFSET 0x225Du
#define TEST_MANAGER_RESOURCE_1_OFFSET 0x2224u
#define TEST_MANAGER_RESOURCE_2_OFFSET 0x222Cu
#define TEST_MANAGER_RESOURCE_3_OFFSET 0x2234u
#define TEST_FONT_SIZE 0xBCu
#define TEST_FONT_BEGIN_COUNT_OFFSET 0xB4u
#define TEST_FONT_TEXTURE_OFFSET 0xB8u

static union {
    uint32_t alignment;
    uint8_t bytes[TEST_MANAGER_SIZE];
} g_manager_storage;
static union {
    uint32_t alignment;
    uint8_t bytes[TEST_FONT_SIZE];
} g_font_storage;
#define g_manager (g_manager_storage.bytes)
#define g_font (g_font_storage.bytes)
static uint32_t g_texture_words[4];
static uint32_t
    g_texture_pixels[AZ_GLYPH_ATLAS_HEIGHT][AZ_GLYPH_ATLAS_WIDTH];
static volatile uint32_t g_device_slot = 0x12340000u;
static AzOverlayFontVertex g_draw_vertices[3][4];
static uint32_t g_create_count;
static uint32_t g_lock_count;
static uint32_t g_unlock_count;
static uint32_t g_set_texture_count;
static uint32_t g_set_constant_count;
static uint32_t g_draw_count;
static uint32_t g_release_count;
static int32_t g_system_ui_active;

static int close_enough(float actual, float expected)
{
    return fabsf(actual - expected) < 0.0001f;
}

static void store_u32(uint8_t *destination, uint32_t offset, uint32_t value)
{
    memcpy(destination + offset, &value, sizeof(value));
}

static int address_is_valid(void *address)
{
    return address != NULL;
}

static int32_t system_ui_is_active(void)
{
    return g_system_ui_active;
}

static void *create_texture(
    uint32_t width,
    uint32_t height,
    uint32_t arg5,
    uint32_t arg6,
    uint32_t arg7,
    uint32_t format,
    uint32_t arg9,
    uint32_t arg10)
{
    if (width != AZ_GLYPH_ATLAS_WIDTH ||
        height != AZ_GLYPH_ATLAS_HEIGHT ||
        arg5 != 1u || arg6 != 1u || arg7 != 0u ||
        format != 0x18280086u || arg9 != 1u || arg10 != 3u) {
        return NULL;
    }

    ++g_create_count;
    return g_texture_words;
}

static int32_t lock_texture(
    void *texture,
    uint32_t level,
    AzOverlayLockedRect *locked,
    const void *rect,
    uint32_t flags)
{
    if (texture != g_texture_words || level != 0u ||
        locked == NULL || rect != NULL || flags != 0u) {
        return -1;
    }

    ++g_lock_count;
    locked->pitch = (uint32_t)sizeof(g_texture_pixels[0]);
    locked->bits = g_texture_pixels;
    return 0;
}

static int32_t unlock_texture(void *texture, uint32_t level)
{
    if (texture != g_texture_words || level != 0u) {
        return -1;
    }

    ++g_unlock_count;
    return 0;
}

static void set_texture(
    void *device,
    uint32_t stage,
    void *texture,
    uint32_t flags)
{
    if ((uintptr_t)device == (uintptr_t)g_device_slot &&
        stage == 0u && texture == g_texture_words &&
        flags == 0x80000000u) {
        ++g_set_texture_count;
    }
}

static void set_vs_constant_f(
    void *device,
    uint32_t start_register,
    const float *vectors,
    uint32_t count,
    uint64_t dirty_block_mask)
{
    if ((uintptr_t)device == (uintptr_t)g_device_slot &&
        (start_register == 1u || start_register == 2u) &&
        vectors != NULL && count == 1u &&
        dirty_block_mask == 0x8000000000000000ULL) {
        ++g_set_constant_count;
    }
}

static void draw_primitive_up(
    void *device,
    uint32_t primitive_type,
    uint32_t vertex_count,
    const void *vertices,
    uint32_t stride)
{
    if ((uintptr_t)device != (uintptr_t)g_device_slot ||
        primitive_type != 5u || vertex_count != 4u ||
        vertices == NULL || stride != sizeof(AzOverlayFontVertex) ||
        g_draw_count >= 3u) {
        return;
    }

    memcpy(
        g_draw_vertices[g_draw_count],
        vertices,
        sizeof(g_draw_vertices[g_draw_count]));
    ++g_draw_count;
}

static uint32_t release_resource(void *resource)
{
    if (resource == g_texture_words) {
        ++g_release_count;
    }
    return 0u;
}

static void initialize_renderer(AzOverlayRenderer *renderer)
{
    memset(renderer, 0, sizeof(*renderer));
    renderer->bindings.is_address_valid = address_is_valid;
    renderer->bindings.is_system_ui_active = system_ui_is_active;
    renderer->bindings.device_slot = &g_device_slot;
    renderer->bindings.create_texture = create_texture;
    renderer->bindings.lock_texture = lock_texture;
    renderer->bindings.unlock_texture = unlock_texture;
    renderer->bindings.set_texture = set_texture;
    renderer->bindings.set_vs_constant_f = set_vs_constant_f;
    renderer->bindings.draw_primitive_up = draw_primitive_up;
    renderer->bindings.release_resource = release_resource;
    renderer->rev1655_validated = 1u;
}

static void initialize_host_objects(void)
{
    memset(g_manager, 0, sizeof(g_manager));
    memset(g_font, 0, sizeof(g_font));
    g_manager[TEST_MANAGER_RENDER_BYTE_OFFSET] = 1u;
    store_u32(g_manager, TEST_MANAGER_RESOURCE_1_OFFSET, 1u);
    store_u32(g_manager, TEST_MANAGER_RESOURCE_2_OFFSET, 1u);
    store_u32(g_manager, TEST_MANAGER_RESOURCE_3_OFFSET, 1u);
    store_u32(g_font, TEST_FONT_BEGIN_COUNT_OFFSET, 1u);
    store_u32(g_font, TEST_FONT_TEXTURE_OFFSET, 1u);
}

static void reset_draw_capture(void)
{
    memset(g_draw_vertices, 0, sizeof(g_draw_vertices));
    g_set_texture_count = 0u;
    g_set_constant_count = 0u;
    g_draw_count = 0u;
}

int main(void)
{
    AzOverlayRenderer renderer;
    AzOverlayRenderer invalid_renderer;
    AzOverlayDrawRequest request;
    uint32_t row;
    uint32_t column;
    int found_alpha = 0;

    initialize_renderer(&renderer);
    initialize_host_objects();

    request.font = g_font;
    request.caller_lr = AZ_REV1655_FONT_END_CALLER_LR;
    request.viewport_width = 1280.0f;
    request.viewport_height = 720.0f;
    request.selector_active = 1u;
    request.selected_index = 0u;
    request.proven_modal_clear = 0u;

    if (az_overlay_renderer_note_render_menu(
            &renderer,
            g_manager,
            0) != AZ_OVERLAY_RENDERER_OK ||
        az_overlay_renderer_try_draw(
            &renderer,
            &request) != AZ_OVERLAY_RENDERER_MODAL_UNKNOWN ||
        az_overlay_renderer_try_draw(
            &renderer,
            &request) != AZ_OVERLAY_RENDERER_NO_COVERFLOW) {
        return EXIT_FAILURE;
    }

    request.proven_modal_clear = 1u;
    if (az_overlay_renderer_note_render_menu(
            &renderer,
            g_manager,
            0) != AZ_OVERLAY_RENDERER_OK ||
        az_overlay_renderer_try_draw(
            &renderer,
            &request) != AZ_OVERLAY_RENDERER_DRAWN ||
        g_create_count != 1u || g_lock_count != 1u ||
        g_unlock_count != 1u || g_set_texture_count != 3u ||
        g_set_constant_count != 6u || g_draw_count != 3u) {
        return EXIT_FAILURE;
    }

    if (!close_enough(g_draw_vertices[1][0].x, 179.0f) ||
        !close_enough(g_draw_vertices[1][0].y, 569.0f) ||
        g_draw_vertices[1][0].u != 0 ||
        g_draw_vertices[1][0].v != 20 ||
        g_draw_vertices[1][2].u != 922 ||
        g_draw_vertices[1][2].v != 53 ||
        g_draw_vertices[1][3].u != 0 ||
        g_draw_vertices[1][3].v != 53 ||
        g_draw_vertices[2][0].u != 0 ||
        g_draw_vertices[2][2].u != 25 ||
        g_draw_vertices[2][3].u != 0) {
        return EXIT_FAILURE;
    }

    request.proven_modal_clear = 2u;
    if (az_overlay_renderer_note_render_menu(
            &renderer,
            g_manager,
            0) != AZ_OVERLAY_RENDERER_OK ||
        az_overlay_renderer_try_draw(
            &renderer,
            &request) != AZ_OVERLAY_RENDERER_BAD_REQUEST ||
        az_overlay_renderer_in_flight(&renderer) != 0u ||
        g_draw_count != 3u) {
        return EXIT_FAILURE;
    }

    request.proven_modal_clear = 1u;
    request.selected_index = AZ_GLYPH_ATLAS_GLYPH_COUNT;
    if (az_overlay_renderer_note_render_menu(
            &renderer,
            g_manager,
            0) != AZ_OVERLAY_RENDERER_OK ||
        az_overlay_renderer_try_draw(
            &renderer,
            &request) != AZ_OVERLAY_RENDERER_BAD_REQUEST ||
        az_overlay_renderer_try_draw(
            &renderer,
            &request) != AZ_OVERLAY_RENDERER_NO_COVERFLOW ||
        g_draw_count != 3u ||
        strcmp(
            az_overlay_renderer_result_name(
                AZ_OVERLAY_RENDERER_BAD_REQUEST),
            "bad-request") != 0) {
        return EXIT_FAILURE;
    }
    request.selected_index = 0u;

    if (az_overlay_renderer_note_render_menu(
            &renderer,
            g_manager + 1u,
            0) != AZ_OVERLAY_RENDERER_NO_COVERFLOW ||
        az_overlay_renderer_in_flight(&renderer) != 0u) {
        return EXIT_FAILURE;
    }

    reset_draw_capture();
    request.viewport_width = 640.0f;
    request.viewport_height = 480.0f;
    request.selector_active = 0u;
    if (az_overlay_renderer_note_render_menu(
            &renderer,
            g_manager,
            0) != AZ_OVERLAY_RENDERER_OK ||
        az_overlay_renderer_try_draw(
            &renderer,
            &request) != AZ_OVERLAY_RENDERER_OK ||
        g_set_texture_count != 0u ||
        g_set_constant_count != 0u ||
        g_draw_count != 0u) {
        return EXIT_FAILURE;
    }

    request.viewport_width = 1280.0f;
    request.viewport_height = 720.0f;
    request.selector_active = 1u;

    for (row = 0u; row < AZ_GLYPH_ATLAS_HEIGHT; ++row) {
        for (column = 0u; column < AZ_GLYPH_ATLAS_WIDTH; ++column) {
            const uint32_t pixel = g_texture_pixels[row][column];
            if ((pixel & 0x00FFFFFFu) != 0x00FFFFFFu) {
                return EXIT_FAILURE;
            }
            if ((pixel >> 24u) != 0u) {
                found_alpha = 1;
            }
        }
    }
    if (!found_alpha) {
        return EXIT_FAILURE;
    }

    g_system_ui_active = 1;
    if (az_overlay_renderer_note_render_menu(
            &renderer,
            g_manager,
            0) != AZ_OVERLAY_RENDERER_OK ||
        az_overlay_renderer_try_draw(
            &renderer,
            &request) != AZ_OVERLAY_RENDERER_SYSTEM_UI ||
        az_overlay_renderer_try_draw(
            &renderer,
            &request) != AZ_OVERLAY_RENDERER_NO_COVERFLOW) {
        return EXIT_FAILURE;
    }
    g_system_ui_active = 0;

    az_overlay_renderer_begin_unload(&renderer);
    if (az_overlay_renderer_note_render_menu(
            &renderer,
            g_manager,
            0) != AZ_OVERLAY_RENDERER_UNLOADING ||
        az_overlay_renderer_try_draw(
            &renderer,
            &request) != AZ_OVERLAY_RENDERER_UNLOADING ||
        az_overlay_renderer_in_flight(&renderer) != 0u ||
        az_overlay_renderer_release_texture(
            &renderer) != AZ_OVERLAY_RENDERER_OK ||
        g_release_count != 1u) {
        return EXIT_FAILURE;
    }

    memset(&invalid_renderer, 0, sizeof(invalid_renderer));
    az_overlay_renderer_begin_unload(&invalid_renderer);
    if (az_overlay_renderer_release_texture(
            &invalid_renderer) != AZ_OVERLAY_RENDERER_NOT_VALIDATED) {
        return EXIT_FAILURE;
    }

    puts("Xbox 360 overlay renderer tests passed");
    return EXIT_SUCCESS;
}
