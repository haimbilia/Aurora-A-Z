#include <stddef.h>
#include <stdint.h>

#include <auroraaz/filters.h>
#include <auroraaz/glyph_atlas.h>
#include <auroraaz/layout.h>
#include <auroraaz/overlay_model.h>

#define AZ_COLOR_SHADOW_NEAR 0x58000000u
#define AZ_COLOR_SHADOW_FAR 0x78000000u
#define AZ_COLOR_INACTIVE 0xD8F5F5F5u
#define AZ_COLOR_SELECTED_GLOW 0x88FFFFFFu
#define AZ_COLOR_SELECTED 0xFFFFFFFFu
#define AZ_SELECTED_GLOW_SCALE 1.08f

static float minimum(float left, float right)
{
    return left < right ? left : right;
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

    if (model == NULL) {
        return;
    }
    model->count = 0u;

    if (coverflow_visible == 0u ||
        viewport_width <= 0.0f || viewport_height <= 0.0f) {
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

    add_quad(
        model,
        row_x + (shadow_x * 0.5f),
        row_y + (shadow_y * 0.5f),
        row_width,
        row_height,
        0.0f,
        (float)AZ_GLYPH_ATLAS_TEXT_TOP,
        (float)AZ_GLYPH_ATLAS_ROW_ADVANCE,
        (float)(AZ_GLYPH_ATLAS_TEXT_BOTTOM - AZ_GLYPH_ATLAS_TEXT_TOP),
        AZ_COLOR_SHADOW_NEAR,
        AZ_OVERLAY_LAYER_SHADOW_NEAR);
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
        AZ_COLOR_SHADOW_FAR,
        AZ_OVERLAY_LAYER_SHADOW_FAR);
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

    if (selector_active != 0u && selected_index < AZ_GLYPH_COUNT) {
        const AzGlyphAtlasGlyph *glyph =
            &g_az_glyph_atlas_glyphs[selected_index];
        const float glyph_x =
            row_x + ((float)glyph->source_x * scale);
        const float glyph_width = (float)glyph->advance * scale;
        const float glow_width = glyph_width * AZ_SELECTED_GLOW_SCALE;
        const float glow_height = row_height * AZ_SELECTED_GLOW_SCALE;

        add_quad(
            model,
            glyph_x - ((glow_width - glyph_width) * 0.5f),
            row_y - ((glow_height - row_height) * 0.5f),
            glow_width,
            glow_height,
            (float)glyph->source_x,
            (float)AZ_GLYPH_ATLAS_TEXT_TOP,
            (float)glyph->advance,
            (float)(AZ_GLYPH_ATLAS_TEXT_BOTTOM - AZ_GLYPH_ATLAS_TEXT_TOP),
            AZ_COLOR_SELECTED_GLOW,
            AZ_OVERLAY_LAYER_SELECTED_GLOW);

        add_quad(
            model,
            glyph_x,
            row_y,
            glyph_width,
            row_height,
            (float)glyph->source_x,
            (float)AZ_GLYPH_ATLAS_TEXT_TOP,
            (float)glyph->advance,
            (float)(AZ_GLYPH_ATLAS_TEXT_BOTTOM - AZ_GLYPH_ATLAS_TEXT_TOP),
            AZ_COLOR_SELECTED,
            AZ_OVERLAY_LAYER_SELECTED);
    }
}
