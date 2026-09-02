#ifndef AURORAAZ_FILTERS_H
#define AURORAAZ_FILTERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#define AZ_GLYPH_COUNT 27u
#define AZ_NO_GLYPH 0xFFu

char az_glyph_for_index(uint8_t index);
const char *az_filter_method_for_index(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif
