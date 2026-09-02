#ifndef AURORAAZ_NETDBG_LIFETIME_REV1655_H
#define AURORAAZ_NETDBG_LIFETIME_REV1655_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Exact Aurora 0.7b.2 Rev1655 addresses. */
#define AZ_REV1655_PLUGIN_MANAGER_GET_ADDRESS 0x82227008u
#define AZ_REV1655_PLUGIN_MANAGER_LOOKUP_ADDRESS 0x82227928u
#define AZ_REV1655_PLUGIN_MANAGER_ADDRESS 0x82BC3860u
#define AZ_REV1655_PLUGIN_MANAGER_VTABLE 0x821425BCu
#define AZ_REV1655_NETDBG_VTABLE 0x821425C0u
#define AZ_REV1655_NETDBG_RESOLVER_ADDRESS 0x82389650u
#define AZ_REV1655_MODULE_LOAD_ADDRESS 0x82388B50u
#define AZ_REV1655_MODULE_UNLOAD_ADDRESS 0x82388FC0u

#define AZ_REV1655_NETDBG_KEY 7u
#define AZ_REV1655_NETDBG_MODE 9u
#define AZ_REV1655_NETDBG_POLICY_UNLOADABLE 1u
#define AZ_REV1655_NETDBG_POLICY_RESIDENT 3u

#define AZ_REV1655_NETDBG_VTABLE_OFFSET 0x00u
#define AZ_REV1655_NETDBG_LABEL_OFFSET 0x04u
#define AZ_REV1655_NETDBG_IDENTITY_OFFSET 0x20u
#define AZ_REV1655_NETDBG_PATH_OFFSET 0x3Cu
#define AZ_REV1655_NETDBG_MODE_OFFSET 0x58u
#define AZ_REV1655_NETDBG_POLICY_OFFSET 0x5Cu
#define AZ_REV1655_NETDBG_HANDLE_OFFSET 0x60u
#define AZ_REV1655_NETDBG_ORDINAL2_OFFSET 0x94u
#define AZ_REV1655_NETDBG_ORDINAL3_OFFSET 0x98u
#define AZ_REV1655_NETDBG_ORDINAL4_OFFSET 0x9Cu
#define AZ_REV1655_NETDBG_ORDINAL5_OFFSET 0xA0u
#define AZ_REV1655_NETDBG_READY_OFFSET 0xA4u

#define AZ_REV1655_NETDBG_IDENTITY "dll.aurora.netdbg"
#define AZ_REV1655_NETDBG_PATH "game:\\Plugins\\NetDbgDll.xex"
#define AZ_REV1655_NETDBG_LABEL "Network Debugger"

typedef uint8_t (*AzNetDbgLifetimeReadBytesFn)(
    void *context,
    uintptr_t address,
    void *destination,
    size_t size);

/* Reads a target-native 32-bit value, not a byte-order-neutral file word. */
typedef uint8_t (*AzNetDbgLifetimeReadU32Fn)(
    void *context,
    uintptr_t address,
    uint32_t *value);

/*
 * Implements an atomic compare-exchange on target memory. On failure it must
 * replace *expected with the observed value, matching C atomic semantics.
 */
typedef uint8_t (*AzNetDbgLifetimeCompareExchangeU32Fn)(
    void *context,
    uintptr_t address,
    uint32_t *expected,
    uint32_t desired);

/* Uses the exact PluginManager getter and key lookup contract. */
typedef uint8_t (*AzNetDbgLifetimeLookupPluginFn)(
    void *context,
    uint32_t key,
    uint32_t *manager_address,
    uint32_t *wrapper_address);

typedef uint8_t (*AzNetDbgLifetimeGetModuleHandleFn)(
    void *context,
    const char *identity,
    uint32_t *module_handle);

typedef struct AzNetDbgLifetimeRev1655Bindings {
    void *context;
    AzNetDbgLifetimeReadBytesFn read_bytes;
    AzNetDbgLifetimeReadU32Fn read_u32;
    AzNetDbgLifetimeCompareExchangeU32Fn compare_exchange_u32;
    AzNetDbgLifetimeLookupPluginFn lookup_plugin;
    AzNetDbgLifetimeGetModuleHandleFn get_module_handle;
    uint8_t exact_image_verified;
} AzNetDbgLifetimeRev1655Bindings;

typedef struct AzNetDbgLifetimeValidationSpan {
    uintptr_t address;
    const uint8_t *expected;
    size_t size;
} AzNetDbgLifetimeValidationSpan;

typedef enum AzNetDbgLifetimeRev1655Result {
    AZ_NETDBG_LIFETIME_OK = 0,
    AZ_NETDBG_LIFETIME_NULL,
    AZ_NETDBG_LIFETIME_BAD_BINDINGS,
    AZ_NETDBG_LIFETIME_IMAGE_UNVERIFIED,
    AZ_NETDBG_LIFETIME_BAD_WRITE_EXPORT,
    AZ_NETDBG_LIFETIME_SIGNATURE_UNREADABLE,
    AZ_NETDBG_LIFETIME_SIGNATURE_MISMATCH,
    AZ_NETDBG_LIFETIME_LOOKUP_FAILED,
    AZ_NETDBG_LIFETIME_BAD_MANAGER,
    AZ_NETDBG_LIFETIME_BAD_MANAGER_VTABLE,
    AZ_NETDBG_LIFETIME_BAD_WRAPPER,
    AZ_NETDBG_LIFETIME_BAD_WRAPPER_VTABLE,
    AZ_NETDBG_LIFETIME_BAD_VTABLE_CONTRACT,
    AZ_NETDBG_LIFETIME_BAD_MODE,
    AZ_NETDBG_LIFETIME_BAD_POLICY,
    AZ_NETDBG_LIFETIME_BAD_HANDLE,
    AZ_NETDBG_LIFETIME_MODULE_LOOKUP_FAILED,
    AZ_NETDBG_LIFETIME_HANDLE_MISMATCH,
    AZ_NETDBG_LIFETIME_BAD_EXPORTS,
    AZ_NETDBG_LIFETIME_WRITE_EXPORT_MISMATCH,
    AZ_NETDBG_LIFETIME_BAD_READY,
    AZ_NETDBG_LIFETIME_LABEL_UNREADABLE,
    AZ_NETDBG_LIFETIME_LABEL_MISMATCH,
    AZ_NETDBG_LIFETIME_IDENTITY_UNREADABLE,
    AZ_NETDBG_LIFETIME_IDENTITY_MISMATCH,
    AZ_NETDBG_LIFETIME_PATH_UNREADABLE,
    AZ_NETDBG_LIFETIME_PATH_MISMATCH,
    AZ_NETDBG_LIFETIME_POLICY_CAS_FAILED,
    AZ_NETDBG_LIFETIME_POLICY_READBACK_FAILED
} AzNetDbgLifetimeRev1655Result;

typedef struct AzNetDbgLifetimeRev1655Status {
    AzNetDbgLifetimeRev1655Result result;
    uint32_t failed_validation_span;
    uint32_t manager_address;
    uint32_t wrapper_address;
    uint32_t wrapper_vtable;
    uint32_t module_handle;
    uint32_t wrapper_handle;
    uint32_t mode;
    uint32_t policy_before;
    uint32_t policy_after;
    uint32_t ordinal2;
    uint32_t ordinal3;
    uint32_t ordinal4;
    uint32_t ordinal5;
    uint32_t ready;
    uint8_t signatures_verified;
    uint8_t object_verified;
    uint8_t strings_verified;
    uint8_t compare_exchange_succeeded;
    uint8_t pinned_for_title_lifetime;
} AzNetDbgLifetimeRev1655Status;

/*
 * Validates the exact code and live key-7 object before atomically changing
 * only wrapper+0x5C from policy 1 to policy 3. Policy 3 makes Aurora perform
 * logical detach/reattach without XexUnloadImage. There is intentionally no
 * API to restore policy 1 during the title lifetime.
 */
AzNetDbgLifetimeRev1655Result az_rev1655_netdbg_lifetime_pin(
    const AzNetDbgLifetimeRev1655Bindings *bindings,
    uint32_t expected_ordinal4_export,
    AzNetDbgLifetimeRev1655Status *status);

size_t az_rev1655_netdbg_lifetime_validation_span_count(void);
uint8_t az_rev1655_netdbg_lifetime_validation_span(
    size_t index,
    AzNetDbgLifetimeValidationSpan *span);

const char *az_netdbg_lifetime_rev1655_result_name(
    AzNetDbgLifetimeRev1655Result result);

#if defined(AURORAAZ_XBOX360)
/* Uses the exact Rev1655 functions and Xbox kernel module lookup. */
AzNetDbgLifetimeRev1655Result az_rev1655_netdbg_lifetime_pin_default(
    uint8_t exact_image_verified,
    uint32_t expected_ordinal4_export,
    AzNetDbgLifetimeRev1655Status *status);
#endif

#ifdef __cplusplus
}
#endif

#endif
