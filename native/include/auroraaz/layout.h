#ifndef AURORAAZ_LAYOUT_H
#define AURORAAZ_LAYOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct AzVisualStyle {
    float logical_width;
    float logical_height;
    float row_center_x;
    float glyph_pitch;
    float baseline_y;
    float em_size;
    float shadow_offset_x;
    float shadow_offset_y;
    uint8_t inactive_alpha;
    uint8_t selected_alpha;
    uint8_t shadow_alpha;
} AzVisualStyle;

const AzVisualStyle *az_mockup_visual_style(void);
float az_glyph_center_x(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif
