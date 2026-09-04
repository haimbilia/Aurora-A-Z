#ifndef AURORAAZ_FILTERS_H
#define AURORAAZ_FILTERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#define AZ_GLYPH_COUNT 28u
#define AZ_NO_GLYPH 0xFFu
#define AZ_FILTER_ALL_INDEX 0u
#define AZ_FILTER_OTHER_INDEX 1u
#define AZ_FILTER_FIRST_ALPHA_INDEX 2u

char az_glyph_for_index(uint8_t index);
const char *az_label_for_index(uint8_t index);
const char *az_filter_method_for_index(uint8_t index);
uint8_t az_filter_index_for_method(const char *method);

#ifdef __cplusplus
}
#endif

#endif
