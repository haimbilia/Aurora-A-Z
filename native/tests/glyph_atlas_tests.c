#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <auroraaz/glyph_atlas.h>

static uint32_t fnv1a(const uint8_t *data, size_t size)
{
    size_t index;
    uint32_t hash = 2166136261u;

    for (index = 0u; index < size; ++index) {
        hash ^= (uint32_t)data[index];
        hash *= 16777619u;
    }
    return hash;
}

int main(void)
{
    uint8_t *pixels =
        (uint8_t *)malloc((size_t)AZ_GLYPH_ATLAS_PIXEL_COUNT);
    size_t index;
    size_t nonzero = 0u;
    uint32_t sum = 0u;

    if (pixels == NULL) {
        return EXIT_FAILURE;
    }

    if (az_glyph_atlas_decode(
            pixels,
            (size_t)AZ_GLYPH_ATLAS_PIXEL_COUNT) == 0u ||
        az_glyph_atlas_decode(NULL, AZ_GLYPH_ATLAS_PIXEL_COUNT) != 0u ||
        az_glyph_atlas_decode(
            pixels,
            (size_t)AZ_GLYPH_ATLAS_PIXEL_COUNT - 1u) != 0u) {
        free(pixels);
        return EXIT_FAILURE;
    }

    for (index = 0u; index < AZ_GLYPH_ATLAS_PIXEL_COUNT; ++index) {
        if (pixels[index] != 0u) {
            ++nonzero;
        }
        sum += (uint32_t)pixels[index];
    }

    if (az_glyph_atlas_rle_size() != 13438u ||
        nonzero != 6478u ||
        sum != 1144023u ||
        fnv1a(pixels, AZ_GLYPH_ATLAS_PIXEL_COUNT) != 0xF067A02Eu ||
        g_az_glyph_atlas_glyphs[0].source_x != 0u ||
        g_az_glyph_atlas_glyphs[0].advance != 66u ||
        g_az_glyph_atlas_glyphs[1].source_x != 76u ||
        g_az_glyph_atlas_glyphs[27].source_x != 975u ||
        g_az_glyph_atlas_glyphs[27].advance != 23u ||
        pixels[(AZ_GLYPH_ATLAS_SOLID_Y * AZ_GLYPH_ATLAS_WIDTH) +
            AZ_GLYPH_ATLAS_SOLID_X] != 255u ||
        strcmp(
            az_glyph_atlas_source_font_sha256(),
            "59123D9F5A81091626FB1B37C583510A85DB1296AB794B48309AAAD0410232ED") != 0) {
        free(pixels);
        return EXIT_FAILURE;
    }

    free(pixels);
    puts("glyph atlas tests passed");
    return EXIT_SUCCESS;
}
