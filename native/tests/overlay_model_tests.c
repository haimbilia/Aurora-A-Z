#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <auroraaz/overlay_model.h>

static int close_enough(float actual, float expected)
{
    return fabsf(actual - expected) < 0.01f;
}

int main(void)
{
    AzOverlayModel model;

    az_overlay_model_build(
        1280.0f, 720.0f, 1u, 0u, 0u, 0u, 0u, 0.0f, &model);
    if (model.count != 0u) {
        return EXIT_FAILURE;
    }

    az_overlay_model_build(
        1280.0f, 720.0f, 1u, 1u, 0u, 0u, 0u, 0.0f, &model);
    if (model.count != 5u ||
        model.quads[0].layer != AZ_OVERLAY_LAYER_DIM ||
        model.quads[1].layer != AZ_OVERLAY_LAYER_SHADOW ||
        model.quads[2].layer != AZ_OVERLAY_LAYER_ROW ||
        model.quads[3].layer != AZ_OVERLAY_LAYER_SELECTED_SHADOW ||
        model.quads[4].layer != AZ_OVERLAY_LAYER_SELECTED ||
        model.quads[0].color != 0xA0000000u ||
        !close_enough(model.quads[0].width, 1280.0f) ||
        !close_enough(model.quads[0].height, 720.0f) ||
        !close_enough(model.quads[2].x, 141.0f) ||
        !close_enough(model.quads[2].y, 344.0f) ||
        !close_enough(model.quads[2].width, 998.0f) ||
        !close_enough(model.quads[2].height, 33.0f) ||
        !close_enough(model.quads[4].x, 131.76f) ||
        !close_enough(model.quads[4].y, 339.38f) ||
        !close_enough(model.quads[4].width, 84.48f) ||
        !close_enough(model.quads[4].height, 42.24f) ||
        model.quads[4].color != 0xFFFFFFFFu) {
        return EXIT_FAILURE;
    }

    az_overlay_model_build(
        1280.0f, 720.0f, 1u, 1u, 1u, 0u, 0u, 0.0f, &model);
    if (model.count != 5u ||
        !close_enough(model.quads[4].x, 213.78f) ||
        !close_enough(model.quads[4].width, 29.44f)) {
        return EXIT_FAILURE;
    }

    az_overlay_model_build(
        1280.0f, 720.0f, 1u, 0u, 0u, 1u, 1u, 0.5f, &model);
    if (model.count != 3u ||
        model.quads[0].layer != AZ_OVERLAY_LAYER_DIM ||
        model.quads[1].layer != AZ_OVERLAY_LAYER_SELECTED_SHADOW ||
        model.quads[2].layer != AZ_OVERLAY_LAYER_SELECTED ||
        model.quads[0].color != 0x50000000u ||
        model.quads[2].color != 0x80FFFFFFu ||
        !close_enough(model.quads[2].width, 39.79f)) {
        return EXIT_FAILURE;
    }

    az_overlay_model_build(
        640.0f, 480.0f, 1u, 1u, 0u, 0u, 0u, 0.0f, &model);
    if (model.count != 5u ||
        !close_enough(model.quads[2].x, 70.5f) ||
        !close_enough(model.quads[2].y, 234.0f) ||
        !close_enough(model.quads[2].width, 499.0f) ||
        !close_enough(model.quads[2].height, 16.5f)) {
        return EXIT_FAILURE;
    }

    az_overlay_model_build(
        1280.0f, 720.0f, 0u, 1u, 0u, 0u, 0u, 0.0f, &model);
    if (model.count != 0u) {
        return EXIT_FAILURE;
    }

    model.count = 3u;
    az_overlay_model_build(
        1280.0f, 720.0f, 1u, 1u, 0u, 0u, 0u, 0.0f, NULL);
    if (model.count != 3u) {
        return EXIT_FAILURE;
    }

    puts("overlay model tests passed");
    return EXIT_SUCCESS;
}
