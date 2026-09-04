#ifndef AURORAAZ_OPERATION_MODE_H
#define AURORAAZ_OPERATION_MODE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AzOperationMode {
    AZ_OPERATION_MODE_BROWSE = 0,
    AZ_OPERATION_MODE_FILTER = 1
} AzOperationMode;

#define AZ_OPERATION_MODE_CONFIG_VERSION 1u
#define AZ_OPERATION_MODE_CONFIG_MAX_SIZE 64u

AzOperationMode az_operation_mode_sanitize(uint32_t value);
const char *az_operation_mode_name(AzOperationMode mode);

/* Missing, truncated, or unknown state deliberately falls back to Browse. */
AzOperationMode az_operation_mode_parse(
    const uint8_t *bytes,
    size_t size);

size_t az_operation_mode_serialize(
    AzOperationMode mode,
    char *destination,
    size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
