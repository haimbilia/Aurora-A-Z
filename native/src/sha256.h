#ifndef AURORAAZ_SHA256_H
#define AURORAAZ_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct AzSha256Context {
    uint32_t state[8];
    uint64_t total_size;
    uint8_t block[64];
    size_t block_size;
    int failed;
} AzSha256Context;

void az_sha256_init(AzSha256Context *context);
void az_sha256_update(
    AzSha256Context *context,
    const uint8_t *bytes,
    size_t size);
void az_sha256_final(AzSha256Context *context, uint8_t digest[32]);

void az_sha256(const uint8_t *bytes, size_t size, uint8_t digest[32]);

#endif
