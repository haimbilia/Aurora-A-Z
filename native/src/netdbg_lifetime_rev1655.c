#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(AURORAAZ_XBOX360)
#include <xecore/xboxkrnl.h>
#endif

#include <auroraaz/netdbg_lifetime_rev1655.h>

#define AZ_NETDBG_VALIDATION_SPAN_COUNT 4u
#define AZ_NETDBG_STRING_INLINE_CAPACITY 16u
#define AZ_NETDBG_WSTRING_INLINE_CAPACITY 8u
#define AZ_NETDBG_STRING_SIZE_OFFSET 0x10u
#define AZ_NETDBG_STRING_CAPACITY_OFFSET 0x14u
#define AZ_NETDBG_NO_FAILED_SPAN UINT32_MAX

typedef enum AzExactStringResult {
    AZ_EXACT_STRING_OK = 0,
    AZ_EXACT_STRING_UNREADABLE,
    AZ_EXACT_STRING_MISMATCH
} AzExactStringResult;

/* Target-native UTF-16BE, including the terminator. */
static const uint8_t g_netdbg_label_utf16be[] = {
    0x00u, 0x4Eu, 0x00u, 0x65u, 0x00u, 0x74u, 0x00u, 0x77u,
    0x00u, 0x6Fu, 0x00u, 0x72u, 0x00u, 0x6Bu, 0x00u, 0x20u,
    0x00u, 0x44u, 0x00u, 0x65u, 0x00u, 0x62u, 0x00u, 0x75u,
    0x00u, 0x67u, 0x00u, 0x67u, 0x00u, 0x65u, 0x00u, 0x72u,
    0x00u, 0x00u
};

static const uint8_t g_manager_get_entry_signature[] = {
    0x7Du, 0x88u, 0x02u, 0xA6u,
    0x91u, 0x81u, 0xFFu, 0xF8u,
    0xFBu, 0xE1u, 0xFFu, 0xF0u,
    0x3Bu, 0xE1u, 0xFFu, 0xA0u,
    0x94u, 0x21u, 0xFFu, 0xA0u,
    0x3Du, 0x40u, 0x82u, 0xBCu,
    0x81u, 0x6Au, 0x38u, 0x5Cu,
    0x55u, 0x69u, 0x07u, 0xFFu
};

static const uint8_t g_manager_get_return_signature[] = {
    0x3Du, 0x60u, 0x82u, 0xBCu,
    0x38u, 0x6Bu, 0x38u, 0x60u,
    0x38u, 0x3Fu, 0x00u, 0x60u,
    0x81u, 0x81u, 0xFFu, 0xF8u,
    0x7Du, 0x88u, 0x03u, 0xA6u,
    0xEBu, 0xE1u, 0xFFu, 0xF0u,
    0x4Eu, 0x80u, 0x00u, 0x20u
};

static const uint8_t g_manager_lookup_entry_signature[] = {
    0x7Du, 0x88u, 0x02u, 0xA6u,
    0x91u, 0x81u, 0xFFu, 0xF8u,
    0xFBu, 0xC1u, 0xFFu, 0xE8u,
    0xFBu, 0xE1u, 0xFFu, 0xF0u,
    0x94u, 0x21u, 0xFFu, 0x90u,
    0x83u, 0xE3u, 0x00u, 0x08u,
    0x3Bu, 0xC3u, 0x00u, 0x04u,
    0x81u, 0x7Fu, 0x00u, 0x04u
};

static const uint8_t g_unload_policy_signature[] = {
    0x81u, 0x7Fu, 0x00u, 0x5Cu,
    0x55u, 0x6Bu, 0x07u, 0xBDu,
    0x40u, 0x82u, 0x00u, 0xACu,
    0x81u, 0x7Fu, 0x00u, 0x58u,
    0x71u, 0x6Au, 0x00u, 0x09u,
    0x2Bu, 0x0Au, 0x00u, 0x09u,
    0x40u, 0x9Au, 0x00u, 0x10u,
    0x80u, 0x7Fu, 0x00u, 0x60u,
    0x48u, 0x47u, 0x8Bu, 0x69u
};

static const AzNetDbgLifetimeValidationSpan g_validation_spans[
    AZ_NETDBG_VALIDATION_SPAN_COUNT] = {
    { AZ_REV1655_PLUGIN_MANAGER_GET_ADDRESS,
      g_manager_get_entry_signature, sizeof(g_manager_get_entry_signature) },
    { 0x82227044u, g_manager_get_return_signature,
      sizeof(g_manager_get_return_signature) },
    { AZ_REV1655_PLUGIN_MANAGER_LOOKUP_ADDRESS,
      g_manager_lookup_entry_signature,
      sizeof(g_manager_lookup_entry_signature) },
    { 0x82389000u, g_unload_policy_signature,
      sizeof(g_unload_policy_signature) }
};

static void clear_status(AzNetDbgLifetimeRev1655Status *status)
{
    if (status != NULL) {
        memset(status, 0, sizeof(*status));
        status->failed_validation_span = AZ_NETDBG_NO_FAILED_SPAN;
        status->result = AZ_NETDBG_LIFETIME_NULL;
    }
}

static AzNetDbgLifetimeRev1655Result finish(
    AzNetDbgLifetimeRev1655Status *status,
    AzNetDbgLifetimeRev1655Result result)
{
    if (status != NULL) {
        status->result = result;
    }
    return result;
}

static uint8_t bindings_are_complete(
    const AzNetDbgLifetimeRev1655Bindings *bindings)
{
    return (bindings != NULL &&
        bindings->read_bytes != NULL &&
        bindings->read_u32 != NULL &&
        bindings->compare_exchange_u32 != NULL &&
        bindings->lookup_plugin != NULL &&
        bindings->get_module_handle != NULL) ? 1u : 0u;
}

static uint8_t address_range_fits(uint32_t address, uint32_t size)
{
    return (size != 0u && address != 0u &&
        address <= UINT32_MAX - (size - 1u)) ? 1u : 0u;
}

static uint8_t read_u32(
    const AzNetDbgLifetimeRev1655Bindings *bindings,
    uint32_t address,
    uint32_t *value)
{
    if (value == NULL || address_range_fits(address, 4u) == 0u ||
        (address & 3u) != 0u) {
        return 0u;
    }
    return bindings->read_u32(
        bindings->context, (uintptr_t)address, value);
}

static uint8_t validate_signatures(
    const AzNetDbgLifetimeRev1655Bindings *bindings,
    AzNetDbgLifetimeRev1655Status *status)
{
    uint8_t actual[sizeof(g_unload_policy_signature)];
    size_t index;

    for (index = 0u; index < AZ_NETDBG_VALIDATION_SPAN_COUNT; ++index) {
        const AzNetDbgLifetimeValidationSpan *span =
            &g_validation_spans[index];

        if (span->size > sizeof(actual) ||
            bindings->read_bytes(
                bindings->context,
                span->address,
                actual,
                span->size) == 0u) {
            status->failed_validation_span = (uint32_t)index;
            status->result = AZ_NETDBG_LIFETIME_SIGNATURE_UNREADABLE;
            return 0u;
        }
        if (memcmp(actual, span->expected, span->size) != 0) {
            status->failed_validation_span = (uint32_t)index;
            status->result = AZ_NETDBG_LIFETIME_SIGNATURE_MISMATCH;
            return 0u;
        }
    }

    status->signatures_verified = 1u;
    return 1u;
}

static AzExactStringResult read_exact_string(
    const AzNetDbgLifetimeRev1655Bindings *bindings,
    uint32_t string_address,
    const char *expected)
{
    uint32_t size;
    uint32_t capacity;
    uint32_t storage_address;
    size_t expected_size;
    char actual[sizeof(AZ_REV1655_NETDBG_PATH)];

    expected_size = strlen(expected);
    if (expected_size + 1u > sizeof(actual) ||
        address_range_fits(
            string_address,
            AZ_NETDBG_STRING_CAPACITY_OFFSET + 4u) == 0u ||
        read_u32(
            bindings,
            string_address + AZ_NETDBG_STRING_SIZE_OFFSET,
            &size) == 0u ||
        read_u32(
            bindings,
            string_address + AZ_NETDBG_STRING_CAPACITY_OFFSET,
            &capacity) == 0u) {
        return AZ_EXACT_STRING_UNREADABLE;
    }
    if (size != (uint32_t)expected_size || capacity < size) {
        return AZ_EXACT_STRING_MISMATCH;
    }

    if (capacity < AZ_NETDBG_STRING_INLINE_CAPACITY) {
        storage_address = string_address;
    } else if (read_u32(bindings, string_address, &storage_address) == 0u) {
        return AZ_EXACT_STRING_UNREADABLE;
    }
    if (address_range_fits(
            storage_address, (uint32_t)expected_size + 1u) == 0u ||
        bindings->read_bytes(
            bindings->context,
            (uintptr_t)storage_address,
            actual,
            expected_size + 1u) == 0u) {
        return AZ_EXACT_STRING_UNREADABLE;
    }
    if (memcmp(actual, expected, expected_size) != 0 ||
        actual[expected_size] != '\0') {
        return AZ_EXACT_STRING_MISMATCH;
    }
    return AZ_EXACT_STRING_OK;
}

static AzExactStringResult read_exact_label(
    const AzNetDbgLifetimeRev1655Bindings *bindings,
    uint32_t string_address)
{
    uint32_t size;
    uint32_t capacity;
    uint32_t storage_address;
    uint8_t actual[sizeof(g_netdbg_label_utf16be)];
    const uint32_t expected_units =
        (uint32_t)((sizeof(g_netdbg_label_utf16be) / 2u) - 1u);

    if (address_range_fits(
            string_address,
            AZ_NETDBG_STRING_CAPACITY_OFFSET + 4u) == 0u ||
        read_u32(
            bindings,
            string_address + AZ_NETDBG_STRING_SIZE_OFFSET,
            &size) == 0u ||
        read_u32(
            bindings,
            string_address + AZ_NETDBG_STRING_CAPACITY_OFFSET,
            &capacity) == 0u) {
        return AZ_EXACT_STRING_UNREADABLE;
    }
    if (size != expected_units || capacity < size) {
        return AZ_EXACT_STRING_MISMATCH;
    }

    if (capacity < AZ_NETDBG_WSTRING_INLINE_CAPACITY) {
        storage_address = string_address;
    } else if (read_u32(bindings, string_address, &storage_address) == 0u) {
        return AZ_EXACT_STRING_UNREADABLE;
    }
    if (address_range_fits(
            storage_address,
            (uint32_t)sizeof(g_netdbg_label_utf16be)) == 0u ||
        bindings->read_bytes(
            bindings->context,
            (uintptr_t)storage_address,
            actual,
            sizeof(actual)) == 0u) {
        return AZ_EXACT_STRING_UNREADABLE;
    }
    if (memcmp(
            actual,
            g_netdbg_label_utf16be,
            sizeof(g_netdbg_label_utf16be)) != 0) {
        return AZ_EXACT_STRING_MISMATCH;
    }
    return AZ_EXACT_STRING_OK;
}

static AzNetDbgLifetimeRev1655Result validate_vtable_contract(
    const AzNetDbgLifetimeRev1655Bindings *bindings)
{
    uint32_t resolver;
    uint32_t load;
    uint32_t unload;

    if (read_u32(
            bindings,
            AZ_REV1655_NETDBG_VTABLE + 0x04u,
            &resolver) == 0u ||
        read_u32(
            bindings,
            AZ_REV1655_NETDBG_VTABLE + 0x18u,
            &load) == 0u ||
        read_u32(
            bindings,
            AZ_REV1655_NETDBG_VTABLE + 0x1Cu,
            &unload) == 0u ||
        resolver != AZ_REV1655_NETDBG_RESOLVER_ADDRESS ||
        load != AZ_REV1655_MODULE_LOAD_ADDRESS ||
        unload != AZ_REV1655_MODULE_UNLOAD_ADDRESS) {
        return AZ_NETDBG_LIFETIME_BAD_VTABLE_CONTRACT;
    }
    return AZ_NETDBG_LIFETIME_OK;
}

AzNetDbgLifetimeRev1655Result az_rev1655_netdbg_lifetime_pin(
    const AzNetDbgLifetimeRev1655Bindings *bindings,
    uint32_t expected_ordinal4_export,
    AzNetDbgLifetimeRev1655Status *status)
{
    AzNetDbgLifetimeRev1655Result result;
    AzExactStringResult string_result;
    uint32_t manager_vtable;
    uint32_t expected_policy;
    uint32_t readback_policy = 0u;

    clear_status(status);
    if (bindings == NULL || status == NULL) {
        return finish(status, AZ_NETDBG_LIFETIME_NULL);
    }
    if (bindings_are_complete(bindings) == 0u) {
        return finish(status, AZ_NETDBG_LIFETIME_BAD_BINDINGS);
    }
    if (bindings->exact_image_verified == 0u) {
        return finish(status, AZ_NETDBG_LIFETIME_IMAGE_UNVERIFIED);
    }
    if (expected_ordinal4_export == 0u) {
        return finish(status, AZ_NETDBG_LIFETIME_BAD_WRITE_EXPORT);
    }
    if (validate_signatures(bindings, status) == 0u) {
        return status->result;
    }

    if (bindings->lookup_plugin(
            bindings->context,
            AZ_REV1655_NETDBG_KEY,
            &status->manager_address,
            &status->wrapper_address) == 0u) {
        return finish(status, AZ_NETDBG_LIFETIME_LOOKUP_FAILED);
    }
    if (status->manager_address != AZ_REV1655_PLUGIN_MANAGER_ADDRESS) {
        return finish(status, AZ_NETDBG_LIFETIME_BAD_MANAGER);
    }
    if (read_u32(
            bindings,
            status->manager_address,
            &manager_vtable) == 0u ||
        manager_vtable != AZ_REV1655_PLUGIN_MANAGER_VTABLE) {
        return finish(status, AZ_NETDBG_LIFETIME_BAD_MANAGER_VTABLE);
    }
    if (address_range_fits(status->wrapper_address, 0xA8u) == 0u ||
        (status->wrapper_address & 3u) != 0u) {
        return finish(status, AZ_NETDBG_LIFETIME_BAD_WRAPPER);
    }
    if (read_u32(
            bindings,
            status->wrapper_address + AZ_REV1655_NETDBG_VTABLE_OFFSET,
            &status->wrapper_vtable) == 0u ||
        status->wrapper_vtable != AZ_REV1655_NETDBG_VTABLE) {
        return finish(status, AZ_NETDBG_LIFETIME_BAD_WRAPPER_VTABLE);
    }
    result = validate_vtable_contract(bindings);
    if (result != AZ_NETDBG_LIFETIME_OK) {
        return finish(status, result);
    }
    if (read_u32(
            bindings,
            status->wrapper_address + AZ_REV1655_NETDBG_MODE_OFFSET,
            &status->mode) == 0u ||
        status->mode != AZ_REV1655_NETDBG_MODE) {
        return finish(status, AZ_NETDBG_LIFETIME_BAD_MODE);
    }
    if (read_u32(
            bindings,
            status->wrapper_address + AZ_REV1655_NETDBG_POLICY_OFFSET,
            &status->policy_before) == 0u ||
        status->policy_before != AZ_REV1655_NETDBG_POLICY_UNLOADABLE) {
        return finish(status, AZ_NETDBG_LIFETIME_BAD_POLICY);
    }
    if (read_u32(
            bindings,
            status->wrapper_address + AZ_REV1655_NETDBG_HANDLE_OFFSET,
            &status->wrapper_handle) == 0u ||
        status->wrapper_handle == 0u) {
        return finish(status, AZ_NETDBG_LIFETIME_BAD_HANDLE);
    }
    if (bindings->get_module_handle(
            bindings->context,
            AZ_REV1655_NETDBG_IDENTITY,
            &status->module_handle) == 0u ||
        status->module_handle == 0u) {
        return finish(status, AZ_NETDBG_LIFETIME_MODULE_LOOKUP_FAILED);
    }
    if (status->module_handle != status->wrapper_handle) {
        return finish(status, AZ_NETDBG_LIFETIME_HANDLE_MISMATCH);
    }
    if (read_u32(
            bindings,
            status->wrapper_address + AZ_REV1655_NETDBG_ORDINAL2_OFFSET,
            &status->ordinal2) == 0u ||
        read_u32(
            bindings,
            status->wrapper_address + AZ_REV1655_NETDBG_ORDINAL3_OFFSET,
            &status->ordinal3) == 0u ||
        read_u32(
            bindings,
            status->wrapper_address + AZ_REV1655_NETDBG_ORDINAL4_OFFSET,
            &status->ordinal4) == 0u ||
        read_u32(
            bindings,
            status->wrapper_address + AZ_REV1655_NETDBG_ORDINAL5_OFFSET,
            &status->ordinal5) == 0u ||
        status->ordinal2 == 0u || status->ordinal3 == 0u ||
        status->ordinal4 == 0u || status->ordinal5 == 0u) {
        return finish(status, AZ_NETDBG_LIFETIME_BAD_EXPORTS);
    }
    if (status->ordinal4 != expected_ordinal4_export) {
        return finish(status, AZ_NETDBG_LIFETIME_WRITE_EXPORT_MISMATCH);
    }
    if (read_u32(
            bindings,
            status->wrapper_address + AZ_REV1655_NETDBG_READY_OFFSET,
            &status->ready) == 0u ||
        status->ready != 1u) {
        return finish(status, AZ_NETDBG_LIFETIME_BAD_READY);
    }

    string_result = read_exact_label(
        bindings,
        status->wrapper_address + AZ_REV1655_NETDBG_LABEL_OFFSET);
    if (string_result != AZ_EXACT_STRING_OK) {
        return finish(
            status,
            string_result == AZ_EXACT_STRING_UNREADABLE ?
                AZ_NETDBG_LIFETIME_LABEL_UNREADABLE :
                AZ_NETDBG_LIFETIME_LABEL_MISMATCH);
    }
    string_result = read_exact_string(
        bindings,
        status->wrapper_address + AZ_REV1655_NETDBG_IDENTITY_OFFSET,
        AZ_REV1655_NETDBG_IDENTITY);
    if (string_result != AZ_EXACT_STRING_OK) {
        return finish(
            status,
            string_result == AZ_EXACT_STRING_UNREADABLE ?
                AZ_NETDBG_LIFETIME_IDENTITY_UNREADABLE :
                AZ_NETDBG_LIFETIME_IDENTITY_MISMATCH);
    }
    string_result = read_exact_string(
        bindings,
        status->wrapper_address + AZ_REV1655_NETDBG_PATH_OFFSET,
        AZ_REV1655_NETDBG_PATH);
    if (string_result != AZ_EXACT_STRING_OK) {
        return finish(
            status,
            string_result == AZ_EXACT_STRING_UNREADABLE ?
                AZ_NETDBG_LIFETIME_PATH_UNREADABLE :
                AZ_NETDBG_LIFETIME_PATH_MISMATCH);
    }
    status->strings_verified = 1u;
    status->object_verified = 1u;

    expected_policy = AZ_REV1655_NETDBG_POLICY_UNLOADABLE;
    if (bindings->compare_exchange_u32(
            bindings->context,
            (uintptr_t)(status->wrapper_address +
                AZ_REV1655_NETDBG_POLICY_OFFSET),
            &expected_policy,
            AZ_REV1655_NETDBG_POLICY_RESIDENT) == 0u) {
        status->policy_after = expected_policy;
        return finish(status, AZ_NETDBG_LIFETIME_POLICY_CAS_FAILED);
    }
    status->compare_exchange_succeeded = 1u;
    if (read_u32(
            bindings,
            status->wrapper_address + AZ_REV1655_NETDBG_POLICY_OFFSET,
            &readback_policy) == 0u ||
        readback_policy != AZ_REV1655_NETDBG_POLICY_RESIDENT) {
        status->policy_after = readback_policy;
        return finish(status, AZ_NETDBG_LIFETIME_POLICY_READBACK_FAILED);
    }

    status->policy_after = readback_policy;
    status->pinned_for_title_lifetime = 1u;
    return finish(status, AZ_NETDBG_LIFETIME_OK);
}

size_t az_rev1655_netdbg_lifetime_validation_span_count(void)
{
    return AZ_NETDBG_VALIDATION_SPAN_COUNT;
}

uint8_t az_rev1655_netdbg_lifetime_validation_span(
    size_t index,
    AzNetDbgLifetimeValidationSpan *span)
{
    if (span == NULL || index >= AZ_NETDBG_VALIDATION_SPAN_COUNT) {
        return 0u;
    }
    *span = g_validation_spans[index];
    return 1u;
}

const char *az_netdbg_lifetime_rev1655_result_name(
    AzNetDbgLifetimeRev1655Result result)
{
    switch (result) {
    case AZ_NETDBG_LIFETIME_OK:
        return "ok";
    case AZ_NETDBG_LIFETIME_NULL:
        return "null";
    case AZ_NETDBG_LIFETIME_BAD_BINDINGS:
        return "bad-bindings";
    case AZ_NETDBG_LIFETIME_IMAGE_UNVERIFIED:
        return "image-unverified";
    case AZ_NETDBG_LIFETIME_BAD_WRITE_EXPORT:
        return "bad-write-export";
    case AZ_NETDBG_LIFETIME_SIGNATURE_UNREADABLE:
        return "signature-unreadable";
    case AZ_NETDBG_LIFETIME_SIGNATURE_MISMATCH:
        return "signature-mismatch";
    case AZ_NETDBG_LIFETIME_LOOKUP_FAILED:
        return "lookup-failed";
    case AZ_NETDBG_LIFETIME_BAD_MANAGER:
        return "bad-manager";
    case AZ_NETDBG_LIFETIME_BAD_MANAGER_VTABLE:
        return "bad-manager-vtable";
    case AZ_NETDBG_LIFETIME_BAD_WRAPPER:
        return "bad-wrapper";
    case AZ_NETDBG_LIFETIME_BAD_WRAPPER_VTABLE:
        return "bad-wrapper-vtable";
    case AZ_NETDBG_LIFETIME_BAD_VTABLE_CONTRACT:
        return "bad-vtable-contract";
    case AZ_NETDBG_LIFETIME_BAD_MODE:
        return "bad-mode";
    case AZ_NETDBG_LIFETIME_BAD_POLICY:
        return "bad-policy";
    case AZ_NETDBG_LIFETIME_BAD_HANDLE:
        return "bad-handle";
    case AZ_NETDBG_LIFETIME_MODULE_LOOKUP_FAILED:
        return "module-lookup-failed";
    case AZ_NETDBG_LIFETIME_HANDLE_MISMATCH:
        return "handle-mismatch";
    case AZ_NETDBG_LIFETIME_BAD_EXPORTS:
        return "bad-exports";
    case AZ_NETDBG_LIFETIME_WRITE_EXPORT_MISMATCH:
        return "write-export-mismatch";
    case AZ_NETDBG_LIFETIME_BAD_READY:
        return "bad-ready";
    case AZ_NETDBG_LIFETIME_LABEL_UNREADABLE:
        return "label-unreadable";
    case AZ_NETDBG_LIFETIME_LABEL_MISMATCH:
        return "label-mismatch";
    case AZ_NETDBG_LIFETIME_IDENTITY_UNREADABLE:
        return "identity-unreadable";
    case AZ_NETDBG_LIFETIME_IDENTITY_MISMATCH:
        return "identity-mismatch";
    case AZ_NETDBG_LIFETIME_PATH_UNREADABLE:
        return "path-unreadable";
    case AZ_NETDBG_LIFETIME_PATH_MISMATCH:
        return "path-mismatch";
    case AZ_NETDBG_LIFETIME_POLICY_CAS_FAILED:
        return "policy-cas-failed";
    case AZ_NETDBG_LIFETIME_POLICY_READBACK_FAILED:
        return "policy-readback-failed";
    default:
        return "unknown";
    }
}

#if defined(AURORAAZ_XBOX360)

static uint8_t default_range_is_readable(uintptr_t address, size_t size)
{
    uintptr_t current;
    uintptr_t final;

    if (address == (uintptr_t)0u || size == 0u ||
        address > UINT32_MAX || size - 1u > UINT32_MAX - address) {
        return 0u;
    }
    final = address + size - 1u;
    current = address;
    for (;;) {
        if (!MmIsAddressValid((void *)current)) {
            return 0u;
        }
        if (current == final) {
            break;
        }
        current = (current & ~(uintptr_t)0xFFFu) + 0x1000u;
        if (current > final) {
            current = final;
        }
    }
    return 1u;
}

static uint8_t default_read_bytes(
    void *context,
    uintptr_t address,
    void *destination,
    size_t size)
{
    size_t index;
    const volatile uint8_t *source;
    uint8_t *output;

    (void)context;
    if (destination == NULL ||
        default_range_is_readable(address, size) == 0u) {
        return 0u;
    }
    source = (const volatile uint8_t *)address;
    output = (uint8_t *)destination;
    for (index = 0u; index < size; ++index) {
        output[index] = source[index];
    }
    return 1u;
}

static uint8_t default_read_u32(
    void *context,
    uintptr_t address,
    uint32_t *value)
{
    (void)context;
    if (value == NULL || (address & 3u) != 0u ||
        default_range_is_readable(address, sizeof(*value)) == 0u) {
        return 0u;
    }
    *value = __atomic_load_n(
        (const volatile uint32_t *)address, __ATOMIC_ACQUIRE);
    return 1u;
}

static uint8_t default_compare_exchange_u32(
    void *context,
    uintptr_t address,
    uint32_t *expected,
    uint32_t desired)
{
    (void)context;
    if (expected == NULL || (address & 3u) != 0u ||
        default_range_is_readable(address, sizeof(*expected)) == 0u) {
        return 0u;
    }
    return __atomic_compare_exchange_n(
        (volatile uint32_t *)address,
        expected,
        desired,
        0,
        __ATOMIC_ACQ_REL,
        __ATOMIC_ACQUIRE) ? 1u : 0u;
}

static uint8_t default_lookup_plugin(
    void *context,
    uint32_t key,
    uint32_t *manager_address,
    uint32_t *wrapper_address)
{
    typedef void *(*ManagerGetFn)(void);
    typedef void *(*ManagerLookupFn)(void *, uint32_t);
    ManagerGetFn manager_get;
    ManagerLookupFn manager_lookup;
    void *manager;
    void *wrapper;

    (void)context;
    if (manager_address == NULL || wrapper_address == NULL) {
        return 0u;
    }
    manager_get = (ManagerGetFn)(uintptr_t)
        AZ_REV1655_PLUGIN_MANAGER_GET_ADDRESS;
    manager_lookup = (ManagerLookupFn)(uintptr_t)
        AZ_REV1655_PLUGIN_MANAGER_LOOKUP_ADDRESS;
    manager = manager_get();
    if (manager == NULL) {
        return 0u;
    }
    wrapper = manager_lookup(manager, key);
    if (wrapper == NULL || (uintptr_t)manager > UINT32_MAX ||
        (uintptr_t)wrapper > UINT32_MAX) {
        return 0u;
    }
    *manager_address = (uint32_t)(uintptr_t)manager;
    *wrapper_address = (uint32_t)(uintptr_t)wrapper;
    return 1u;
}

static uint8_t default_get_module_handle(
    void *context,
    const char *identity,
    uint32_t *module_handle)
{
    HMODULE handle = NULL;
    NTSTATUS result;

    (void)context;
    if (identity == NULL || module_handle == NULL) {
        return 0u;
    }
    result = XexGetModuleHandle(identity, &handle);
    if (FAILED(result) || handle == NULL || (uintptr_t)handle > UINT32_MAX) {
        return 0u;
    }
    *module_handle = (uint32_t)(uintptr_t)handle;
    return 1u;
}

AzNetDbgLifetimeRev1655Result az_rev1655_netdbg_lifetime_pin_default(
    uint8_t exact_image_verified,
    uint32_t expected_ordinal4_export,
    AzNetDbgLifetimeRev1655Status *status)
{
    const AzNetDbgLifetimeRev1655Bindings bindings = {
        NULL,
        default_read_bytes,
        default_read_u32,
        default_compare_exchange_u32,
        default_lookup_plugin,
        default_get_module_handle,
        exact_image_verified
    };

    return az_rev1655_netdbg_lifetime_pin(
        &bindings, expected_ordinal4_export, status);
}

#endif
