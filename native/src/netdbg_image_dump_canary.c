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
#define AZ_DUMP_STATUS_RESOLVER_CAPTURED 0x40u
#define AZ_DUMP_STATUS_RESOLVER_WRITTEN 0x80u
#define AZ_DUMP_FORMAT_VERSION 4u

#define AZ_RESOLVER_FORMAT_VERSION 4u
#define AZ_RESOLVER_MODULE_COUNT 4u
#define AZ_RESOLVER_SLOT_COUNT 2u
#define AZ_RESOLVER_STATUS_NOT_CALLED 0xFFFFFFFFu
#define AZ_RESOLVER_STATUS_CAPTURE_COMPLETE 0x80000000u
#define AZ_RESOLVER_STATUS_MODULE_QUERIED(index) (1u << (index))
#define AZ_RESOLVER_STATUS_MODULE_FOUND(index) (1u << ((index) + 4u))
#define AZ_RESOLVER_STATUS_SLOT_THUNK_MAPPED(index) (1u << ((index) * 5u + 8u))
#define AZ_RESOLVER_STATUS_SLOT_THUNK_DECODED(index) (1u << ((index) * 5u + 9u))
#define AZ_RESOLVER_STATUS_SLOT_IAT_MAPPED(index) (1u << ((index) * 5u + 10u))
#define AZ_RESOLVER_STATUS_SLOT_PROCEDURE_QUERIED(index) \
    (1u << ((index) * 5u + 11u))
#define AZ_RESOLVER_STATUS_SLOT_OWNER_QUERIED(index) \
    (1u << ((index) * 5u + 12u))

#define AZ_XAM_MODULE_INDEX 0u
#define AZ_XBOXKRNL_MODULE_INDEX 1u
#define AZ_LAUNCH_MODULE_INDEX 2u
#define AZ_NOVA_MODULE_INDEX 3u

#define AZ_SLOT_60_ORDINAL 0x217u
#define AZ_SLOT_60_THUNK_VA 0x82B661BCu
#define AZ_SLOT_60_IAT_RVA 0x4F0u
#define AZ_SLOT_65_ORDINAL 0x1FCu
#define AZ_SLOT_65_THUNK_VA 0x82B6620Cu
#define AZ_SLOT_65_IAT_RVA 0x504u

/*
 * OpenXeChain's xboxkrnl import library exports ordinal 412, but its current
 * public header has no declaration for it.  The loader entry is deliberately
 * opaque: this probe records its handle and never dereferences it.
 */
struct _LDR_DATA_TABLE_ENTRY;
extern void *XexPcToFileHeader(
    void *program_counter,
    struct _LDR_DATA_TABLE_ENTRY **out_ldr_entry);

typedef struct AzImageDumpMarker {
    uint8_t magic[4];
    uint32_t version;
    uint32_t record_size;
    uint32_t phase;
    uint32_t status;
    uint32_t header_bytes;
    uint32_t text_bytes;
    uint32_t iat_bytes;
    uint32_t resolver_bytes;
} AzImageDumpMarker;

typedef struct AzResolverModuleRecord {
    uint8_t tag[4];
    uint32_t query_status;
    uint32_t handle;
} AzResolverModuleRecord;

typedef struct AzResolverSlotRecord {
    uint32_t ordinal;
    uint32_t thunk_va;
    uint32_t iat_rva;
    uint32_t thunk_word_0;
    uint32_t thunk_word_1;
    uint32_t decoded_target;
    uint32_t iat_value;
    uint32_t procedure_status;
    uint32_t procedure_target;
    uint32_t pc_header;
    uint32_t owner_ldr;
} AzResolverSlotRecord;

typedef struct AzResolverEvidence {
    /* Xbox 360 writes every uint32_t below in native big-endian order. */
    uint8_t magic[4];
    uint32_t version;
    uint32_t record_size;
    uint32_t status;
    uint32_t module_count;
    uint32_t slot_count;
    AzResolverModuleRecord modules[AZ_RESOLVER_MODULE_COUNT];
    AzResolverSlotRecord slots[AZ_RESOLVER_SLOT_COUNT];
} AzResolverEvidence;

typedef char AzImageDumpMarkerMustBe36Bytes[
    sizeof(AzImageDumpMarker) == 36u ? 1 : -1];
typedef char AzResolverModuleRecordMustBe12Bytes[
    sizeof(AzResolverModuleRecord) == 12u ? 1 : -1];
typedef char AzResolverSlotRecordMustBe44Bytes[
    sizeof(AzResolverSlotRecord) == 44u ? 1 : -1];
typedef char AzResolverEvidenceMustBe160Bytes[
    sizeof(AzResolverEvidence) == 160u ? 1 : -1];
typedef char AzResolverModulesMustStartAtOffset24[
    offsetof(AzResolverEvidence, modules) == 24u ? 1 : -1];
typedef char AzResolverSlotsMustStartAtOffset72[
    offsetof(AzResolverEvidence, slots) == 72u ? 1 : -1];

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
static char g_resolver_path[] =
    "game:\\Data\\Logs\\AuroraAZ-live-resolver.bin";
static AzResolverEvidence g_resolver_evidence = {
    {'A', 'Z', 'R', 'E'},
    AZ_RESOLVER_FORMAT_VERSION,
    (uint32_t)sizeof(AzResolverEvidence),
    0u,
    AZ_RESOLVER_MODULE_COUNT,
    AZ_RESOLVER_SLOT_COUNT,
    {
        {{'X', 'A', 'M', ' '}, AZ_RESOLVER_STATUS_NOT_CALLED, 0u},
        {{'K', 'R', 'N', 'L'}, AZ_RESOLVER_STATUS_NOT_CALLED, 0u},
        {{'L', 'N', 'C', 'H'}, AZ_RESOLVER_STATUS_NOT_CALLED, 0u},
        {{'N', 'O', 'V', 'A'}, AZ_RESOLVER_STATUS_NOT_CALLED, 0u}
    },
    {
        {
            AZ_SLOT_60_ORDINAL,
            AZ_SLOT_60_THUNK_VA,
            AZ_SLOT_60_IAT_RVA,
            0u,
            0u,
            0u,
            0u,
            AZ_RESOLVER_STATUS_NOT_CALLED,
            0u,
            0u,
            0u
        },
        {
            AZ_SLOT_65_ORDINAL,
            AZ_SLOT_65_THUNK_VA,
            AZ_SLOT_65_IAT_RVA,
            0u,
            0u,
            0u,
            0u,
            AZ_RESOLVER_STATUS_NOT_CALLED,
            0u,
            0u,
            0u
        }
    }
};

static const char *const g_module_identities[AZ_RESOLVER_MODULE_COUNT] = {
    "xam.xex",
    "xboxkrnl.exe",
    "launch.xex",
    "Nova.xex"
};

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

static uint32_t pointer_value(const void *pointer)
{
    return (uint32_t)(uintptr_t)pointer;
}

static uint8_t decode_live_thunk(
    AzResolverSlotRecord *slot)
{
    const volatile uint32_t *words;
    uint32_t high;
    uint32_t low;

    if (slot == NULL ||
        range_is_mapped(
            (const uint8_t *)(uintptr_t)slot->thunk_va,
            16u) == 0u) {
        return 0u;
    }

    words = (const volatile uint32_t *)(uintptr_t)slot->thunk_va;
    slot->thunk_word_0 = words[0];
    slot->thunk_word_1 = words[1];
    if ((slot->thunk_word_0 & 0xFFFF0000u) != 0x3D600000u ||
        (slot->thunk_word_1 & 0xFFFF0000u) != 0x396B0000u ||
        words[2] != 0x7D6903A6u ||
        words[3] != 0x4E800420u) {
        return 1u;
    }

    high = (slot->thunk_word_0 & 0xFFFFu) << 16u;
    low = slot->thunk_word_1 & 0xFFFFu;
    if ((low & 0x8000u) != 0u) {
        high -= 0x10000u;
    }
    slot->decoded_target = high + low;
    return 2u;
}

static void capture_resolver_evidence(void)
{
    uint32_t module_index;
    uint32_t slot_index;

    for (module_index = 0u;
         module_index < AZ_RESOLVER_MODULE_COUNT;
         ++module_index) {
        HMODULE handle = NULL;
        const NTSTATUS result = XexGetModuleHandle(
            g_module_identities[module_index],
            &handle);

        g_resolver_evidence.status |=
            AZ_RESOLVER_STATUS_MODULE_QUERIED(module_index);
        g_resolver_evidence.modules[module_index].query_status =
            (uint32_t)result;
        g_resolver_evidence.modules[module_index].handle =
            pointer_value(handle);
        if (!FAILED(result) && handle != NULL) {
            g_resolver_evidence.status |=
                AZ_RESOLVER_STATUS_MODULE_FOUND(module_index);
        }
    }

    for (slot_index = 0u;
         slot_index < AZ_RESOLVER_SLOT_COUNT;
         ++slot_index) {
        AzResolverSlotRecord *const slot =
            &g_resolver_evidence.slots[slot_index];
        const uint8_t thunk_result = decode_live_thunk(slot);
        const uintptr_t iat_address =
            (uintptr_t)AZ_IMAGE_BASE + (uintptr_t)slot->iat_rva;

        if (thunk_result != 0u) {
            g_resolver_evidence.status |=
                AZ_RESOLVER_STATUS_SLOT_THUNK_MAPPED(slot_index);
        }
        if (thunk_result == 2u) {
            g_resolver_evidence.status |=
                AZ_RESOLVER_STATUS_SLOT_THUNK_DECODED(slot_index);
        }

        if (range_is_mapped(
                (const uint8_t *)iat_address,
                (uint32_t)sizeof(uint32_t)) != 0u) {
            slot->iat_value =
                *(const volatile uint32_t *)iat_address;
            g_resolver_evidence.status |=
                AZ_RESOLVER_STATUS_SLOT_IAT_MAPPED(slot_index);
        }

        if (g_resolver_evidence.modules[AZ_XAM_MODULE_INDEX].handle != 0u) {
            void *procedure = NULL;
            const NTSTATUS procedure_result = XexGetProcedureAddress(
                (HMODULE)(uintptr_t)
                    g_resolver_evidence.modules[AZ_XAM_MODULE_INDEX].handle,
                slot->ordinal,
                &procedure);

            slot->procedure_status = (uint32_t)procedure_result;
            slot->procedure_target = pointer_value(procedure);
            g_resolver_evidence.status |=
                AZ_RESOLVER_STATUS_SLOT_PROCEDURE_QUERIED(slot_index);
        }

        if (slot->decoded_target != 0u &&
            (slot->decoded_target & 3u) == 0u &&
            range_is_mapped(
                (const uint8_t *)(uintptr_t)slot->decoded_target,
                (uint32_t)sizeof(uint32_t)) != 0u) {
            struct _LDR_DATA_TABLE_ENTRY *owner = NULL;
            void *const pc_header = XexPcToFileHeader(
                (void *)(uintptr_t)slot->decoded_target,
                &owner);

            slot->pc_header = pointer_value(pc_header);
            slot->owner_ldr = pointer_value(owner);
            g_resolver_evidence.status |=
                AZ_RESOLVER_STATUS_SLOT_OWNER_QUERIED(slot_index);
        }
    }

    g_resolver_evidence.status |= AZ_RESOLVER_STATUS_CAPTURE_COMPLETE;
}

static void write_marker(
    uint32_t phase,
    uint32_t status,
    uint32_t header_bytes,
    uint32_t text_bytes,
    uint32_t iat_bytes,
    uint32_t resolver_bytes)
{
    const AzImageDumpMarker marker = {
        {'A', 'Z', 'I', 'D'},
        AZ_DUMP_FORMAT_VERSION,
        (uint32_t)sizeof(AzImageDumpMarker),
        phase,
        status,
        header_bytes,
        text_bytes,
        iat_bytes,
        resolver_bytes
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
    uint32_t resolver_bytes = 0u;

    if (!__atomic_compare_exchange_n(
            &g_dump_claimed,
            &expected,
            1u,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        return 0u;
    }

    write_marker(AZ_DUMP_PHASE_STARTED, 0u, 0u, 0u, 0u, 0u);
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
    capture_resolver_evidence();
    status |= AZ_DUMP_STATUS_RESOLVER_CAPTURED;
    if (write_bytes(
            g_resolver_path,
            (const uint8_t *)&g_resolver_evidence,
            (uint32_t)sizeof(g_resolver_evidence),
            &resolver_bytes) != 0u) {
        status |= AZ_DUMP_STATUS_RESOLVER_WRITTEN;
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
        iat_bytes,
        resolver_bytes);
    return 0u;
}

int DllMain(void *module, uint32_t reason, void *reserved)
{
    (void)module;
    (void)reason;
    (void)reserved;
    return 1;
}
