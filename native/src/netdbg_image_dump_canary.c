#if !defined(AURORAAZ_XBOX360)
#error "netdbg_image_dump_canary.c must only be built for Xbox 360"
#endif

#include <stddef.h>
#include <stdint.h>

#include <xecore/xam.h>
#include <xecore/xboxkrnl.h>

#include <auroraaz/compatibility.h>
#include <auroraaz/netdbg_bootstrap.h>
#include <auroraaz/rev1655_hook_gate.h>

#define AZ_IMAGE_BASE 0x82000000u
#define AZ_HEADER_SIZE 0x400u
#define AZ_IAT_RVA 0x400u
#define AZ_IAT_END_RVA 0x9B4u
#define AZ_IAT_SIZE (AZ_IAT_END_RVA - AZ_IAT_RVA)
#define AZ_DUMP_CHUNK_SIZE 0x10000u
#define AZ_DUMP_PHASE_STARTED 1u
#define AZ_DUMP_PHASE_COMPLETE 2u
#define AZ_DUMP_STATUS_HEADER_MAPPED 0x01u
#define AZ_DUMP_STATUS_TEXT_MAPPED 0x02u
#define AZ_DUMP_STATUS_HEADER_WRITTEN 0x04u
#define AZ_DUMP_STATUS_TEXT_WRITTEN 0x08u
#define AZ_DUMP_STATUS_IAT_MAPPED 0x10u
#define AZ_DUMP_STATUS_IAT_WRITTEN 0x20u
#define AZ_DUMP_FORMAT_VERSION 3u

typedef struct AzImageDumpMarker {
    uint8_t magic[4];
    uint32_t version;
    uint32_t record_size;
    uint32_t phase;
    uint32_t status;
    uint32_t header_bytes;
    uint32_t text_bytes;
    uint32_t iat_bytes;
} AzImageDumpMarker;

typedef char AzImageDumpMarkerMustBe32Bytes[
    sizeof(AzImageDumpMarker) == 32u ? 1 : -1];

static uint32_t g_dump_claimed;
/*
 * Executable pages can have a different backing-store view when a kernel
 * write consumes their address directly.  Bounce every byte through this
 * module's ordinary data pages so the dump records the CPU-visible view that
 * the verifier and detours actually read.
 */
static uint8_t g_dump_chunk[AZ_DUMP_CHUNK_SIZE];
static char g_marker_path[] =
    "game:\\Data\\Logs\\AuroraAZ-image-dump.bin";
static char g_header_path[] =
    "game:\\Data\\Logs\\AuroraAZ-live-header.bin";
static char g_text_path[] =
    "game:\\Data\\Logs\\AuroraAZ-live-text.bin";
static char g_iat_path[] =
    "game:\\Data\\Logs\\AuroraAZ-live-iat.bin";

static uint8_t range_is_mapped(const uint8_t *bytes, uint32_t size)
{
    uint32_t offset;

    if (bytes == NULL || size == 0u) {
        return 0u;
    }
    for (offset = 0u; offset < size; offset += 0x1000u) {
        if (!MmIsAddressValid((void *)(bytes + offset))) {
            return 0u;
        }
    }
    return MmIsAddressValid((void *)(bytes + size - 1u)) ? 1u : 0u;
}

static uint8_t write_bytes(
    char *path,
    const uint8_t *bytes,
    uint32_t size,
    uint32_t *completed)
{
    HANDLE file;
    uint32_t offset = 0u;
    uint8_t success = 1u;

    if (completed != NULL) {
        *completed = 0u;
    }
    if (path == NULL || bytes == NULL || size == 0u || completed == NULL) {
        return 0u;
    }

    file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == NULL || file == INVALID_HANDLE_VALUE) {
        return 0u;
    }

    while (offset < size) {
        const volatile uint8_t *source;
        uint32_t copy_index;
        uint32_t written = 0u;
        const uint32_t remaining = size - offset;
        const uint32_t chunk = remaining < AZ_DUMP_CHUNK_SIZE ?
            remaining : AZ_DUMP_CHUNK_SIZE;

        source = (const volatile uint8_t *)(bytes + offset);
        for (copy_index = 0u; copy_index < chunk; ++copy_index) {
            g_dump_chunk[copy_index] = source[copy_index];
        }

        if (WriteFile(
                file,
                g_dump_chunk,
                chunk,
                &written,
                NULL) == 0 ||
            written != chunk) {
            success = 0u;
            break;
        }
        offset += written;
    }

    if (CloseHandle(file) == 0) {
        success = 0u;
    }
    *completed = offset;
    return success;
}

static void write_marker(
    uint32_t phase,
    uint32_t status,
    uint32_t header_bytes,
    uint32_t text_bytes,
    uint32_t iat_bytes)
{
    const AzImageDumpMarker marker = {
        {'A', 'Z', 'I', 'D'},
        AZ_DUMP_FORMAT_VERSION,
        (uint32_t)sizeof(AzImageDumpMarker),
        phase,
        status,
        header_bytes,
        text_bytes,
        iat_bytes
    };
    uint32_t ignored = 0u;

    (void)write_bytes(
        g_marker_path,
        (const uint8_t *)&marker,
        (uint32_t)sizeof(marker),
        &ignored);
}

uint32_t AuroraAZNetDbgBootstrapStart(void)
{
    const uint8_t *const header =
        (const uint8_t *)(uintptr_t)AZ_IMAGE_BASE;
    const uint8_t *const text =
        (const uint8_t *)(uintptr_t)AZ_REV1655_TEXT_BASE;
    const uint8_t *const iat =
        (const uint8_t *)(uintptr_t)(AZ_IMAGE_BASE + AZ_IAT_RVA);
    uint32_t expected = 0u;
    uint32_t status = 0u;
    uint32_t header_bytes = 0u;
    uint32_t text_bytes = 0u;
    uint32_t iat_bytes = 0u;

    if (!__atomic_compare_exchange_n(
            &g_dump_claimed,
            &expected,
            1u,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        return 0u;
    }

    write_marker(AZ_DUMP_PHASE_STARTED, 0u, 0u, 0u, 0u);
    if (range_is_mapped(header, AZ_HEADER_SIZE) != 0u) {
        status |= AZ_DUMP_STATUS_HEADER_MAPPED;
        if (write_bytes(
                g_header_path,
                header,
                AZ_HEADER_SIZE,
                &header_bytes) != 0u) {
            status |= AZ_DUMP_STATUS_HEADER_WRITTEN;
        }
    }
    if (range_is_mapped(iat, AZ_IAT_SIZE) != 0u) {
        status |= AZ_DUMP_STATUS_IAT_MAPPED;
        if (write_bytes(
                g_iat_path,
                iat,
                AZ_IAT_SIZE,
                &iat_bytes) != 0u) {
            status |= AZ_DUMP_STATUS_IAT_WRITTEN;
        }
    }
    if (range_is_mapped(text, AZ_REV1655_TEXT_SIZE) != 0u) {
        status |= AZ_DUMP_STATUS_TEXT_MAPPED;
        if (write_bytes(
                g_text_path,
                text,
                AZ_REV1655_TEXT_SIZE,
                &text_bytes) != 0u) {
            status |= AZ_DUMP_STATUS_TEXT_WRITTEN;
        }
    }
    write_marker(
        AZ_DUMP_PHASE_COMPLETE,
        status,
        header_bytes,
        text_bytes,
        iat_bytes);
    return 0u;
}

int DllMain(void *module, uint32_t reason, void *reserved)
{
    (void)module;
    (void)reason;
    (void)reserved;
    return 1;
}
