#ifndef AURORAAZ_OVERLAY_MODEL_H
#define AURORAAZ_OVERLAY_MODEL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZ_OVERLAY_MAX_QUADS 13u

typedef enum AzOverlayLayer {
    AZ_OVERLAY_LAYER_DIM = 0,
    AZ_OVERLAY_LAYER_SHADOW,
    AZ_OVERLAY_LAYER_ROW,
    AZ_OVERLAY_LAYER_SELECTED_SHADOW,
    AZ_OVERLAY_LAYER_SELECTED,
    AZ_OVERLAY_LAYER_NOTICE
} AzOverlayLayer;

typedef struct AzOverlayQuad {
    float x;
    float y;
    float width;
    float height;
    float source_x;
    float source_y;
    float source_width;
    float source_height;
    uint32_t color;
    AzOverlayLayer layer;
} AzOverlayQuad;

typedef struct AzOverlayModel {
    AzOverlayQuad quads[AZ_OVERLAY_MAX_QUADS];
    size_t count;
} AzOverlayModel;

/*
 * Build draw data only. The platform renderer owns the D3D texture, state
 * bracket, and actual DrawPrimitiveUP calls.
 */
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
    AzOverlayModel *model);

#ifdef __cplusplus
}
#endif

#endif
