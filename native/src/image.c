#include <stddef.h>
#include <stdint.h>

#include <auroraaz/compatibility.h>
#include <auroraaz/image.h>

#define AZ_DOS_LFANEW_OFFSET 0x3Cu
#define AZ_PE_FILE_HEADER_SIZE 20u
#define AZ_PE_SECTION_HEADER_SIZE 40u
#define AZ_REV1655_PE_OFFSET 0xF8u
#define AZ_REV1655_MACHINE_POWERPC 0x01F2u
#define AZ_REV1655_SECTION_COUNT 9u
#define AZ_REV1655_OPTIONAL_SIZE 0xE0u
#define AZ_REV1655_OPTIONAL_MAGIC 0x010Bu
#define AZ_REV1655_ENTRY_RVA 0x008050E0u
#define AZ_REV1655_TEXT_RVA 0x00210000u

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
        (uint16_t)((uint16_t)bytes[1] << 8u));
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8u) |
        ((uint32_t)bytes[2] << 16u) |
        ((uint32_t)bytes[3] << 24u);
}

static int range_fits(size_t offset, size_t length, size_t total)
{
    return offset <= total && length <= total - offset;
}

static int is_text_name(const uint8_t *name)
{
    return name[0] == (uint8_t)'.' &&
        name[1] == (uint8_t)'t' &&
        name[2] == (uint8_t)'e' &&
        name[3] == (uint8_t)'x' &&
        name[4] == (uint8_t)'t' &&
        name[5] == 0u &&
        name[6] == 0u &&
        name[7] == 0u;
}

AzImageResult az_locate_rev1655_text(
    const uint8_t *image,
    size_t image_size,
    const uint8_t **out_text,
    size_t *out_text_size)
{
    size_t pe_offset;
    size_t optional_offset;
    size_t section_offset;
    uint16_t section_count;
    uint16_t optional_size;
    uint16_t section_index;

    if (out_text != NULL) {
        *out_text = NULL;
    }
    if (out_text_size != NULL) {
        *out_text_size = 0u;
    }

    if (image == NULL) {
        return AZ_IMAGE_NULL;
    }
    if (image_size != (size_t)AZ_REV1655_NT_IMAGE_SIZE) {
        return AZ_IMAGE_BAD_SIZE;
    }
    if (!range_fits(0u, 0x40u, image_size) ||
        image[0] != (uint8_t)'M' || image[1] != (uint8_t)'Z') {
        return AZ_IMAGE_BAD_DOS_HEADER;
    }

    pe_offset = (size_t)read_u32_le(image + AZ_DOS_LFANEW_OFFSET);
    if (pe_offset != AZ_REV1655_PE_OFFSET ||
        !range_fits(pe_offset, 24u, image_size) ||
        image[pe_offset] != (uint8_t)'P' ||
        image[pe_offset + 1u] != (uint8_t)'E' ||
        image[pe_offset + 2u] != 0u || image[pe_offset + 3u] != 0u) {
        return AZ_IMAGE_BAD_NT_HEADER;
    }

    if (read_u16_le(image + pe_offset + 4u) !=
        AZ_REV1655_MACHINE_POWERPC) {
        return AZ_IMAGE_BAD_MACHINE;
    }

    section_count = read_u16_le(image + pe_offset + 6u);
    optional_size = read_u16_le(image + pe_offset + 20u);
    if (section_count != AZ_REV1655_SECTION_COUNT ||
        optional_size != AZ_REV1655_OPTIONAL_SIZE) {
        return AZ_IMAGE_BAD_OPTIONAL_HEADER;
    }

    optional_offset = pe_offset + 4u + AZ_PE_FILE_HEADER_SIZE;
    if (!range_fits(optional_offset, (size_t)optional_size, image_size) ||
        read_u16_le(image + optional_offset) !=
            AZ_REV1655_OPTIONAL_MAGIC ||
        read_u32_le(image + optional_offset + 16u) !=
            AZ_REV1655_ENTRY_RVA ||
        read_u32_le(image + optional_offset + 28u) !=
            AZ_REV1655_IMAGE_BASE ||
        read_u32_le(image + optional_offset + 56u) !=
            AZ_REV1655_NT_IMAGE_SIZE ||
        read_u32_le(image + optional_offset + 60u) != 0x400u) {
        return AZ_IMAGE_BAD_IDENTITY;
    }

    section_offset = optional_offset + (size_t)optional_size;
    if (!range_fits(
            section_offset,
            (size_t)section_count * AZ_PE_SECTION_HEADER_SIZE,
            image_size)) {
        return AZ_IMAGE_BAD_SECTION_TABLE;
    }

    for (section_index = 0u; section_index < section_count; ++section_index) {
        const uint8_t *section = image + section_offset +
            ((size_t)section_index * AZ_PE_SECTION_HEADER_SIZE);

        if (is_text_name(section)) {
            const uint32_t virtual_size = read_u32_le(section + 8u);
            const uint32_t virtual_address = read_u32_le(section + 12u);

            if (virtual_address != AZ_REV1655_TEXT_RVA ||
                virtual_size != AZ_REV1655_TEXT_SIZE ||
                !range_fits(
                    (size_t)virtual_address,
                    (size_t)virtual_size,
                    image_size)) {
                return AZ_IMAGE_BAD_TEXT_SECTION;
            }

            if (out_text != NULL) {
                *out_text = image + virtual_address;
            }
            if (out_text_size != NULL) {
                *out_text_size = (size_t)virtual_size;
            }
            return AZ_IMAGE_REV1655;
        }
    }

    return AZ_IMAGE_BAD_TEXT_SECTION;
}

const char *az_image_result_name(AzImageResult result)
{
    switch (result) {
    case AZ_IMAGE_REV1655:
        return "rev1655-image";
    case AZ_IMAGE_NULL:
        return "null-image";
    case AZ_IMAGE_BAD_SIZE:
        return "bad-image-size";
    case AZ_IMAGE_BAD_DOS_HEADER:
        return "bad-dos-header";
    case AZ_IMAGE_BAD_NT_HEADER:
        return "bad-nt-header";
    case AZ_IMAGE_BAD_MACHINE:
        return "bad-machine";
    case AZ_IMAGE_BAD_OPTIONAL_HEADER:
        return "bad-optional-header";
    case AZ_IMAGE_BAD_IDENTITY:
        return "bad-image-identity";
    case AZ_IMAGE_BAD_SECTION_TABLE:
        return "bad-section-table";
    case AZ_IMAGE_BAD_TEXT_SECTION:
        return "bad-text-section";
    default:
        return "unknown-image";
    }
}
