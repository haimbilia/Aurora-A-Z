#include <auroraaz/filters.h>
#include <auroraaz/layout.h>

static const AzVisualStyle k_mockup_style = {
    1280.0f,
    720.0f,
    640.0f,
    34.25f,
    598.0f,
    36.0f,
    2.0f,
    2.0f,
    216u,
    255u,
    176u
};

const AzVisualStyle *az_mockup_visual_style(void)
{
    return &k_mockup_style;
}
float az_glyph_center_x(uint8_t index)
{
    const float middle_index = ((float)AZ_GLYPH_COUNT - 1.0f) * 0.5f;

    if (index >= AZ_GLYPH_COUNT) {
        return k_mockup_style.row_center_x;
    }

    return k_mockup_style.row_center_x +
        (((float)index - middle_index) * k_mockup_style.glyph_pitch);
}
