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

    az_overlay_model_build(1280.0f, 720.0f, 1u, 0u, 0u, &model);
    if (model.count != 2u ||
        model.quads[0].layer != AZ_OVERLAY_LAYER_SHADOW ||
        model.quads[1].layer != AZ_OVERLAY_LAYER_ROW ||
        !close_enough(model.quads[1].x, 179.0f) ||
        !close_enough(model.quads[1].y, 570.0f) ||
        !close_enough(model.quads[1].width, 922.0f) ||
        !close_enough(model.quads[1].height, 33.0f) ||
        !close_enough(model.quads[0].x, 181.0f) ||
        !close_enough(model.quads[0].y, 572.0f)) {
        return EXIT_FAILURE;
    }

    az_overlay_model_build(1280.0f, 720.0f, 1u, 1u, 0u, &model);
    if (model.count != 3u ||
        model.quads[2].layer != AZ_OVERLAY_LAYER_SELECTED ||
        !close_enough(model.quads[2].x, 179.0f) ||
        !close_enough(model.quads[2].width, 23.0f)) {
        return EXIT_FAILURE;
    }

    az_overlay_model_build(1280.0f, 720.0f, 1u, 1u, 1u, &model);
    if (model.count != 3u ||
        !close_enough(model.quads[2].x, 212.0f) ||
        !close_enough(model.quads[2].width, 24.0f)) {
        return EXIT_FAILURE;
    }

    az_overlay_model_build(640.0f, 480.0f, 1u, 0u, 0u, &model);
    if (model.count != 2u ||
        !close_enough(model.quads[1].x, 89.5f) ||
        !close_enough(model.quads[1].width, 461.0f) ||
        !close_enough(model.quads[1].height, 16.5f)) {
        return EXIT_FAILURE;
    }

    az_overlay_model_build(1280.0f, 720.0f, 1u, 1u, 27u, &model);
    if (model.count != 2u) {
        return EXIT_FAILURE;
    }

    az_overlay_model_build(1280.0f, 720.0f, 0u, 0u, 0u, &model);
    if (model.count != 0u) {
        return EXIT_FAILURE;
    }

    model.count = 3u;
    az_overlay_model_build(1280.0f, 720.0f, 1u, 0u, 0u, NULL);
    if (model.count != 3u) {
        return EXIT_FAILURE;
    }

    puts("overlay model tests passed");
    return EXIT_SUCCESS;
}
