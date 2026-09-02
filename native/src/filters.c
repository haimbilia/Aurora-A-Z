#include <stddef.h>

#include <auroraaz/filters.h>

static const char *const k_filter_methods[AZ_GLYPH_COUNT] = {
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

char az_glyph_for_index(uint8_t index)
{
    if (index == 0u) {
        return '#';
    }

    if (index >= AZ_GLYPH_COUNT) {
        return '\0';
    }

    return (char)('A' + (char)(index - 1u));
}

const char *az_filter_method_for_index(uint8_t index)
{
    if (index >= AZ_GLYPH_COUNT) {
        return NULL;
    }

    return k_filter_methods[index];
}
