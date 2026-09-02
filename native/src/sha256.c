#include "sha256.h"

#include <string.h>

static const uint32_t k_round_constants[64] = {
    0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u,
    0x3956C25Bu, 0x59F111F1u, 0x923F82A4u, 0xAB1C5ED5u,
    0xD807AA98u, 0x12835B01u, 0x243185BEu, 0x550C7DC3u,
    0x72BE5D74u, 0x80DEB1FEu, 0x9BDC06A7u, 0xC19BF174u,
    0xE49B69C1u, 0xEFBE4786u, 0x0FC19DC6u, 0x240CA1CCu,
    0x2DE92C6Fu, 0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu,
    0x983E5152u, 0xA831C66Du, 0xB00327C8u, 0xBF597FC7u,
    0xC6E00BF3u, 0xD5A79147u, 0x06CA6351u, 0x14292967u,
    0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu, 0x53380D13u,
    0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u,
    0xA2BFE8A1u, 0xA81A664Bu, 0xC24B8B70u, 0xC76C51A3u,
    0xD192E819u, 0xD6990624u, 0xF40E3585u, 0x106AA070u,
    0x19A4C116u, 0x1E376C08u, 0x2748774Cu, 0x34B0BCB5u,
    0x391C0CB3u, 0x4ED8AA4Au, 0x5B9CCA4Fu, 0x682E6FF3u,
    0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u,
    0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u, 0xC67178F2u
};

static uint32_t rotate_right(uint32_t value, uint32_t count)
{
    return (value >> count) | (value << (32u - count));
}

static uint32_t read_u32_be(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24u) |
        ((uint32_t)bytes[1] << 16u) |
        ((uint32_t)bytes[2] << 8u) |
        (uint32_t)bytes[3];
}

static void write_u32_be(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24u);
    bytes[1] = (uint8_t)(value >> 16u);
    bytes[2] = (uint8_t)(value >> 8u);
    bytes[3] = (uint8_t)value;
}

static void transform(uint32_t state[8], const uint8_t block[64])
{
    uint32_t words[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    size_t index;

    for (index = 0u; index < 16u; ++index) {
        words[index] = read_u32_be(block + (index * 4u));
    }
    for (index = 16u; index < 64u; ++index) {
        const uint32_t x = words[index - 15u];
        const uint32_t y = words[index - 2u];
        const uint32_t sigma0 =
            rotate_right(x, 7u) ^ rotate_right(x, 18u) ^ (x >> 3u);
        const uint32_t sigma1 =
            rotate_right(y, 17u) ^ rotate_right(y, 19u) ^ (y >> 10u);
        words[index] = words[index - 16u] + sigma0 +
            words[index - 7u] + sigma1;
    }

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    for (index = 0u; index < 64u; ++index) {
        const uint32_t sum1 = rotate_right(e, 6u) ^
            rotate_right(e, 11u) ^ rotate_right(e, 25u);
        const uint32_t choose = (e & f) ^ ((~e) & g);
        const uint32_t temporary1 = h + sum1 + choose +
            k_round_constants[index] + words[index];
        const uint32_t sum0 = rotate_right(a, 2u) ^
            rotate_right(a, 13u) ^ rotate_right(a, 22u);
        const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temporary2 = sum0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void az_sha256(const uint8_t *bytes, size_t size, uint8_t digest[32])
{
    uint32_t state[8] = {
        0x6A09E667u, 0xBB67AE85u, 0x3C6EF372u, 0xA54FF53Au,
        0x510E527Fu, 0x9B05688Cu, 0x1F83D9ABu, 0x5BE0CD19u
    };
    uint8_t tail[128];
    size_t remaining = size;
    size_t tail_size;
    size_t index;
    uint64_t bit_length;

    if (digest == NULL) {
        return;
    }
    if (bytes == NULL && size != 0u) {
        memset(digest, 0, 32u);
        return;
    }

    while (remaining >= 64u) {
        transform(state, bytes);
        bytes += 64u;
        remaining -= 64u;
    }

    memset(tail, 0, sizeof(tail));
    if (remaining != 0u) {
        memcpy(tail, bytes, remaining);
    }
    tail[remaining] = 0x80u;
    tail_size = remaining < 56u ? 64u : 128u;
    bit_length = (uint64_t)size * UINT64_C(8);
    for (index = 0u; index < 8u; ++index) {
        tail[tail_size - 1u - index] = (uint8_t)bit_length;
        bit_length >>= 8u;
    }

    transform(state, tail);
    if (tail_size == 128u) {
        transform(state, tail + 64u);
    }

    for (index = 0u; index < 8u; ++index) {
        write_u32_be(digest + (index * 4u), state[index]);
    }
}
