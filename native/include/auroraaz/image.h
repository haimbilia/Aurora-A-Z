#ifndef AURORAAZ_IMAGE_H
#define AURORAAZ_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZ_REV1655_IMAGE_BASE 0x82000000u
#define AZ_REV1655_NT_IMAGE_SIZE 0x00D47E00u
#define AZ_REV1655_FULL_IMAGE_SIZE 0x00BF0000u
#define AZ_REV1655_ENTRY_POINT 0x828050E0u

typedef enum AzImageResult {
    AZ_IMAGE_REV1655 = 0,
    AZ_IMAGE_NULL,
    AZ_IMAGE_BAD_SIZE,
    AZ_IMAGE_BAD_DOS_HEADER,
    AZ_IMAGE_BAD_NT_HEADER,
    AZ_IMAGE_BAD_MACHINE,
    AZ_IMAGE_BAD_OPTIONAL_HEADER,
    AZ_IMAGE_BAD_IDENTITY,
    AZ_IMAGE_BAD_SECTION_TABLE,
    AZ_IMAGE_BAD_TEXT_SECTION
} AzImageResult;

AzImageResult az_locate_rev1655_text(
    const uint8_t *image,
    size_t image_size,
    const uint8_t **out_text,
    size_t *out_text_size);

const char *az_image_result_name(AzImageResult result);

#ifdef __cplusplus
}
#endif

#endif
