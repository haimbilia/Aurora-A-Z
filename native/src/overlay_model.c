#include <stddef.h>
#include <stdint.h>

#include <auroraaz/filters.h>
#include <auroraaz/glyph_atlas.h>
#include <auroraaz/layout.h>
#include <auroraaz/overlay_model.h>

#define AZ_COLOR_DIM 0xA0000000u
#define AZ_COLOR_SHADOW 0xB0000000u
#define AZ_COLOR_INACTIVE 0xD0E0E0E0u
#define AZ_COLOR_SELECTED 0xFFFFFFFFu
#define AZ_ACTIVE_SELECTED_SCALE 1.28f
#define AZ_EXIT_SELECTED_SCALE 2.18f

static float minimum(float left, float right)
{
    return left < right ? left : right;
}

static float clamp_unit(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static uint32_t with_alpha(uint32_t color, float alpha_scale)
{
    const uint32_t alpha = (uint32_t)(
        ((float)((color >> 24u) & 0xFFu) * clamp_unit(alpha_scale)) +
        0.5f);
    return (color & 0x00FFFFFFu) | (alpha << 24u);
}

static void add_quad(
    AzOverlayModel *model,
    float x,
    float y,
    float width,
    float height,
    float source_x,
    float source_y,
    float source_width,
    float source_height,
    uint32_t color,
    AzOverlayLayer layer)
{
    AzOverlayQuad *quad;

    if (model->count >= AZ_OVERLAY_MAX_QUADS) {
        return;
    }

    quad = &model->quads[model->count];
    quad->x = x;
    quad->y = y;
    quad->width = width;
    quad->height = height;
    quad->source_x = source_x;
    quad->source_y = source_y;
    quad->source_width = source_width;
    quad->source_height = source_height;
    quad->color = color;
    quad->layer = layer;
    ++model->count;
}

void az_overlay_model_build(
    float viewport_width,
    float viewport_height,
    uint8_t coverflow_visible,
    uint8_t selector_active,
    uint8_t selected_index,
    uint8_t exit_animation_active,
    uint8_t exit_animation_index,
    float exit_animation_progress,
    AzOverlayModel *model)
{
    const AzVisualStyle *style;
    float scale;
    float row_width;
    float row_height;
    float row_x;
    float row_y;
    float shadow_x;
    float shadow_y;
    uint8_t display_index;
    float selected_scale;
    float opacity;

    if (model == NULL) {
        return;
    }
    model->count = 0u;

    if (coverflow_visible == 0u ||
        viewport_width <= 0.0f || viewport_height <= 0.0f) {
        return;
    }

    if (selector_active == 0u && exit_animation_active == 0u) {
        return;
    }

    style = az_mockup_visual_style();
    scale = minimum(
        viewport_width / style->logical_width,
        viewport_height / style->logical_height);
    if (scale <= 0.0f) {
        return;
    }

    row_width = (float)AZ_GLYPH_ATLAS_ROW_ADVANCE * scale;
    row_height =
        (float)(AZ_GLYPH_ATLAS_TEXT_BOTTOM - AZ_GLYPH_ATLAS_TEXT_TOP) *
        scale;
    row_x = (viewport_width - row_width) * 0.5f;
    row_y =
        (viewport_height * (style->baseline_y / style->logical_height)) -
        ((float)(AZ_GLYPH_ATLAS_BASELINE - AZ_GLYPH_ATLAS_TEXT_TOP) *
            scale);
    shadow_x = style->shadow_offset_x * scale;
    shadow_y = style->shadow_offset_y * scale;

    opacity = selector_active != 0u ?
        1.0f : 1.0f - clamp_unit(exit_animation_progress);
    if (opacity <= 0.0f) {
        return;
    }

    add_quad(
        model,
        0.0f,
        0.0f,
        viewport_width,
        viewport_height,
        (float)AZ_GLYPH_ATLAS_SOLID_X,
        (float)AZ_GLYPH_ATLAS_SOLID_Y,
        1.0f,
        1.0f,
        with_alpha(AZ_COLOR_DIM, opacity),
        AZ_OVERLAY_LAYER_DIM);

    if (selector_active != 0u) {
        add_quad(
            model,
            row_x + shadow_x,
            row_y + shadow_y,
            row_width,
            row_height,
            0.0f,
            (float)AZ_GLYPH_ATLAS_TEXT_TOP,
            (float)AZ_GLYPH_ATLAS_ROW_ADVANCE,
            (float)(AZ_GLYPH_ATLAS_TEXT_BOTTOM - AZ_GLYPH_ATLAS_TEXT_TOP),
            AZ_COLOR_SHADOW,
            AZ_OVERLAY_LAYER_SHADOW);
        add_quad(
            model,
            row_x,
            row_y,
            row_width,
            row_height,
            0.0f,
            (float)AZ_GLYPH_ATLAS_TEXT_TOP,
            (float)AZ_GLYPH_ATLAS_ROW_ADVANCE,
            (float)(AZ_GLYPH_ATLAS_TEXT_BOTTOM - AZ_GLYPH_ATLAS_TEXT_TOP),
            AZ_COLOR_INACTIVE,
            AZ_OVERLAY_LAYER_ROW);
    }

    display_index = selector_active != 0u ?
        selected_index : exit_animation_index;
    if (display_index < AZ_GLYPH_COUNT) {
        const AzGlyphAtlasGlyph *glyph =
            &g_az_glyph_atlas_glyphs[display_index];
        const float glyph_x =
            row_x + ((float)glyph->source_x * scale);
        const float glyph_width = (float)glyph->advance * scale;
        const float glyph_center_x = glyph_x + (glyph_width * 0.5f);
        const float glyph_center_y = row_y + (row_height * 0.5f);
        float selected_width;
        float selected_height;
        float selected_x;
        float selected_y;

        selected_scale = selector_active != 0u ?
            AZ_ACTIVE_SELECTED_SCALE :
            AZ_ACTIVE_SELECTED_SCALE +
                ((AZ_EXIT_SELECTED_SCALE - AZ_ACTIVE_SELECTED_SCALE) *
                    clamp_unit(exit_animation_progress));
        selected_width = glyph_width * selected_scale;
        selected_height = row_height * selected_scale;
        selected_x = glyph_center_x - (selected_width * 0.5f);
        selected_y = glyph_center_y - (selected_height * 0.5f);

        add_quad(
            model,
            selected_x + shadow_x,
            selected_y + shadow_y,
            selected_width,
            selected_height,
            (float)glyph->source_x,
            (float)AZ_GLYPH_ATLAS_TEXT_TOP,
            (float)glyph->advance,
            (float)(AZ_GLYPH_ATLAS_TEXT_BOTTOM - AZ_GLYPH_ATLAS_TEXT_TOP),
            with_alpha(AZ_COLOR_SHADOW, opacity),
            AZ_OVERLAY_LAYER_SELECTED_SHADOW);
        add_quad(
            model,
            selected_x,
            selected_y,
            selected_width,
            selected_height,
            (float)glyph->source_x,
            (float)AZ_GLYPH_ATLAS_TEXT_TOP,
            (float)glyph->advance,
            (float)(AZ_GLYPH_ATLAS_TEXT_BOTTOM - AZ_GLYPH_ATLAS_TEXT_TOP),
            with_alpha(AZ_COLOR_SELECTED, opacity),
            AZ_OVERLAY_LAYER_SELECTED);
    }
}
