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
#define AZ_SELECTED_GAP_PADDING 2.0f

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

static uint32_t interpolate_color(
    uint32_t from,
    uint32_t to,
    float progress)
{
    uint32_t result = 0u;
    uint32_t shift;

    progress = clamp_unit(progress);
    for (shift = 0u; shift < 32u; shift += 8u) {
        const float from_channel = (float)((from >> shift) & 0xFFu);
        const float to_channel = (float)((to >> shift) & 0xFFu);
        const uint32_t channel = (uint32_t)(
            from_channel + ((to_channel - from_channel) * progress) +
            0.5f);
        result |= (channel & 0xFFu) << shift;
    }
    return result;
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

static void add_row_segment(
    AzOverlayModel *model,
    float row_x,
    float row_y,
    float scale,
    float source_start,
    float source_end,
    float offset_x,
    float offset_y,
    uint32_t color,
    AzOverlayLayer layer)
{
    if (!(source_end > source_start)) {
        return;
    }

    add_quad(
        model,
        row_x + (source_start * scale) + offset_x,
        row_y + offset_y,
        (source_end - source_start) * scale,
        (float)(AZ_GLYPH_ATLAS_TEXT_BOTTOM - AZ_GLYPH_ATLAS_TEXT_TOP) *
            scale,
        source_start,
        (float)AZ_GLYPH_ATLAS_TEXT_TOP,
        source_end - source_start,
        (float)(AZ_GLYPH_ATLAS_TEXT_BOTTOM - AZ_GLYPH_ATLAS_TEXT_TOP),
        color,
        layer);
}

static void add_highlight_glyph(
    AzOverlayModel *model,
    const AzGlyphAtlasGlyph *glyph,
    float row_x,
    float row_y,
    float source_start,
    float scale,
    float row_height,
    float highlight_scale,
    float shadow_x,
    float shadow_y,
    uint32_t color,
    uint32_t shadow_color)
{
    const float glyph_x = row_x +
        (((float)glyph->source_x - source_start) * scale);
    const float glyph_width = (float)glyph->advance * scale;
    const float glyph_center_x = glyph_x + (glyph_width * 0.5f);
    const float glyph_center_y = row_y + (row_height * 0.5f);
    const float selected_width = glyph_width * highlight_scale;
    const float selected_height = row_height * highlight_scale;
    const float selected_x = glyph_center_x - (selected_width * 0.5f);
    const float selected_y = glyph_center_y - (selected_height * 0.5f);

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
        shadow_color,
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
        color,
        AZ_OVERLAY_LAYER_SELECTED);
}

void az_overlay_model_build(
    float viewport_width,
    float viewport_height,
    uint8_t coverflow_visible,
    uint8_t selector_active,
    uint8_t first_visible_index,
    uint8_t selected_index,
    uint8_t selection_animation_active,
    uint8_t selection_animation_from_index,
    float selection_animation_progress,
    uint8_t exit_animation_active,
    uint8_t exit_animation_index,
    float exit_animation_progress,
    AzOverlayModel *model)
{
    const AzVisualStyle *style;
    float scale;
    float source_start;
    float source_end;
    float row_width;
    float row_height;
    float row_x;
    float row_y;
    float shadow_x;
    float shadow_y;
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

    if (first_visible_index >= AZ_GLYPH_COUNT) {
        return;
    }
    source_start = (float)
        g_az_glyph_atlas_glyphs[first_visible_index].source_x;
    source_end = (float)AZ_GLYPH_ATLAS_ROW_ADVANCE;
    row_width = (source_end - source_start) * scale;
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

    if (selector_active != 0u && selected_index >= first_visible_index &&
        selected_index < AZ_GLYPH_COUNT) {
        const AzGlyphAtlasGlyph *selected =
            &g_az_glyph_atlas_glyphs[selected_index];
        const uint8_t animate =
            selection_animation_active != 0u &&
            selection_animation_from_index >= first_visible_index &&
            selection_animation_from_index < AZ_GLYPH_COUNT &&
            selection_animation_from_index != selected_index ? 1u : 0u;
        const AzGlyphAtlasGlyph *from = animate != 0u ?
            &g_az_glyph_atlas_glyphs[selection_animation_from_index] :
            selected;
        float gap_start[2];
        float gap_end[2];
        uint8_t gap_count = animate != 0u ? 2u : 1u;
        uint8_t gap_index;
        uint8_t pass;
        float cursor;
        float progress = clamp_unit(selection_animation_progress);
        float eased = progress * progress * (3.0f - (2.0f * progress));

        gap_start[0] = (float)selected->source_x - AZ_SELECTED_GAP_PADDING;
        gap_end[0] = (float)selected->source_x +
            (float)selected->advance + AZ_SELECTED_GAP_PADDING;
        if (animate != 0u) {
            gap_start[1] = (float)from->source_x - AZ_SELECTED_GAP_PADDING;
            gap_end[1] = (float)from->source_x +
                (float)from->advance + AZ_SELECTED_GAP_PADDING;
            if (gap_start[1] < gap_start[0]) {
                const float swap_start = gap_start[0];
                const float swap_end = gap_end[0];
                gap_start[0] = gap_start[1];
                gap_end[0] = gap_end[1];
                gap_start[1] = swap_start;
                gap_end[1] = swap_end;
            }
            if (gap_start[1] <= gap_end[0]) {
                if (gap_end[1] > gap_end[0]) {
                    gap_end[0] = gap_end[1];
                }
                gap_count = 1u;
            }
        }
        for (gap_index = 0u; gap_index < gap_count; ++gap_index) {
            if (gap_start[gap_index] < source_start) {
                gap_start[gap_index] = source_start;
            }
            if (gap_end[gap_index] > source_end) {
                gap_end[gap_index] = source_end;
            }
        }

        for (pass = 0u; pass < 2u; ++pass) {
            cursor = source_start;
            for (gap_index = 0u; gap_index < gap_count; ++gap_index) {
                add_row_segment(
                    model, row_x - (source_start * scale), row_y, scale,
                    cursor, gap_start[gap_index],
                    pass == 0u ? shadow_x : 0.0f,
                    pass == 0u ? shadow_y : 0.0f,
                    pass == 0u ? AZ_COLOR_SHADOW : AZ_COLOR_INACTIVE,
                    pass == 0u ? AZ_OVERLAY_LAYER_SHADOW :
                        AZ_OVERLAY_LAYER_ROW);
                cursor = gap_end[gap_index];
            }
            add_row_segment(
                model, row_x - (source_start * scale), row_y, scale,
                cursor, source_end,
                pass == 0u ? shadow_x : 0.0f,
                pass == 0u ? shadow_y : 0.0f,
                pass == 0u ? AZ_COLOR_SHADOW : AZ_COLOR_INACTIVE,
                pass == 0u ? AZ_OVERLAY_LAYER_SHADOW :
                    AZ_OVERLAY_LAYER_ROW);
        }

        if (animate != 0u) {
            add_highlight_glyph(
                model, from, row_x, row_y, source_start, scale, row_height,
                AZ_ACTIVE_SELECTED_SCALE +
                    ((1.0f - AZ_ACTIVE_SELECTED_SCALE) * eased),
                shadow_x, shadow_y,
                interpolate_color(AZ_COLOR_SELECTED, AZ_COLOR_INACTIVE, eased),
                AZ_COLOR_SHADOW);
        }
        add_highlight_glyph(
            model, selected, row_x, row_y, source_start, scale, row_height,
            animate != 0u ?
                1.0f + ((AZ_ACTIVE_SELECTED_SCALE - 1.0f) * eased) :
                AZ_ACTIVE_SELECTED_SCALE,
            shadow_x, shadow_y,
            animate != 0u ?
                interpolate_color(AZ_COLOR_INACTIVE, AZ_COLOR_SELECTED, eased) :
                AZ_COLOR_SELECTED,
            AZ_COLOR_SHADOW);
    }
    else if (selector_active == 0u &&
        exit_animation_index >= first_visible_index &&
        exit_animation_index < AZ_GLYPH_COUNT) {
        const AzGlyphAtlasGlyph *glyph =
            &g_az_glyph_atlas_glyphs[exit_animation_index];
        const float progress = clamp_unit(exit_animation_progress);

        add_highlight_glyph(
            model, glyph, row_x, row_y, source_start, scale, row_height,
            AZ_ACTIVE_SELECTED_SCALE +
                ((AZ_EXIT_SELECTED_SCALE - AZ_ACTIVE_SELECTED_SCALE) *
                    progress),
            shadow_x, shadow_y,
            with_alpha(AZ_COLOR_SELECTED, opacity),
            with_alpha(AZ_COLOR_SHADOW, opacity));
    }
}
