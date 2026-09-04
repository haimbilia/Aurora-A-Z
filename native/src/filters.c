#include <stddef.h>

#include <auroraaz/filters.h>

static const char *const k_filter_methods[AZ_GLYPH_COUNT] = {
    NULL,
    "NameFilter.Other",
    "NameFilter.A - F.A",
    "NameFilter.A - F.B",
    "NameFilter.A - F.C",
    "NameFilter.A - F.D",
    "NameFilter.A - F.E",
    "NameFilter.A - F.F",
    "NameFilter.G - L.G",
    "NameFilter.G - L.H",
    "NameFilter.G - L.I",
    "NameFilter.G - L.J",
    "NameFilter.G - L.K",
    "NameFilter.G - L.L",
    "NameFilter.M - R.M",
    "NameFilter.M - R.N",
    "NameFilter.M - R.O",
    "NameFilter.M - R.P",
    "NameFilter.M - R.Q",
    "NameFilter.M - R.R",
    "NameFilter.S - X.S",
    "NameFilter.S - X.T",
    "NameFilter.S - X.U",
    "NameFilter.S - X.V",
    "NameFilter.S - X.W",
    "NameFilter.S - X.X",
    "NameFilter.Y - Z.Y",
    "NameFilter.Y - Z.Z"
};

static const char *const k_labels[AZ_GLYPH_COUNT] = {
    "ALL", "#",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L",
    "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X",
    "Y", "Z"
};

static int strings_equal(const char *left, const char *right)
{
    size_t index = 0u;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) {
            return 0;
        }
        ++index;
    }

    return left[index] == right[index] ? 1 : 0;
}

char az_glyph_for_index(uint8_t index)
{
    if (index == AZ_FILTER_OTHER_INDEX) {
        return '#';
    }

    if (index < AZ_FILTER_FIRST_ALPHA_INDEX || index >= AZ_GLYPH_COUNT) {
        return '\0';
    }

    return (char)('A' + (char)(index - AZ_FILTER_FIRST_ALPHA_INDEX));
}

const char *az_label_for_index(uint8_t index)
{
    return index < AZ_GLYPH_COUNT ? k_labels[index] : NULL;
}

const char *az_filter_method_for_index(uint8_t index)
{
    if (index >= AZ_GLYPH_COUNT) {
        return NULL;
    }

    return k_filter_methods[index];
}

uint8_t az_filter_index_for_method(const char *method)
{
    uint8_t index;

    if (method == NULL) {
        return AZ_NO_GLYPH;
    }

    for (index = AZ_FILTER_OTHER_INDEX;
         index < AZ_GLYPH_COUNT;
         ++index) {
        if (strings_equal(method, k_filter_methods[index]) != 0) {
            return index;
        }
    }

    return AZ_NO_GLYPH;
}
