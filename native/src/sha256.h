#ifndef AURORAAZ_SHA256_H
#define AURORAAZ_SHA256_H

#include <stddef.h>
#include <stdint.h>

void az_sha256(const uint8_t *bytes, size_t size, uint8_t digest[32]);

#endif
