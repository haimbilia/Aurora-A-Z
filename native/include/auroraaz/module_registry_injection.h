#ifndef AURORAAZ_MODULE_REGISTRY_INJECTION_H
#define AURORAAZ_MODULE_REGISTRY_INJECTION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Aurora Rev1655 owns a fixed PluginManager registry at startup.  The
 * functions below model the exact late-registration sequence used by its
 * constructor so a separate Aurora A-Z module entry can be added without
 * relabelling the Network Debugger entry that bootstraps this XEX.
 *
 * This is deliberately an address-based, injected ABI.  It is valid only
 * after the image/revision gate has accepted the exact Rev1655 executable.
 */
#define AZ_REV1655_PLUGIN_MANAGER_ADDRESS 0x82BC3860u
#define AZ_REV1655_PLUGIN_MANAGER_VTABLE 0x821425BCu
#define AZ_REV1655_NETDBG_WRAPPER_VTABLE 0x821425C0u
#define AZ_REV1655_NETDBG_WRAPPER_SIZE 0xA8u
#define AZ_REV1655_NETDBG_SOURCE_KEY 7u
#define AZ_REV1655_AZ_MODULE_KEY 8u
#define AZ_REV1655_WRAPPER_HANDLE_OFFSET 0x60u
#define AZ_REV1655_WRAPPER_POLICY_OFFSET 0x5Cu
#define AZ_REV1655_WRAPPER_READY_OFFSET 0xA4u
#define AZ_REV1655_WRAPPER_RESIDENT_POLICY 3u
#define AZ_REV1655_MAP_CREATE_HINT_ADDRESS 0x8238E6F0u
#define AZ_REV1655_MAP_INSERT_ADDRESS 0x82227638u
#define AZ_REV1655_NETDBG_CONSTRUCT_ADDRESS 0x8238E848u
#define AZ_REV1655_NETDBG_RESOLVE_ADDRESS 0x82389650u
#define AZ_REV1655_ALLOCATE_ADDRESS 0x8295DC10u

typedef struct AzModuleRegistryPair {
    uint32_t key;
    uint32_t value;
} AzModuleRegistryPair;

typedef uint8_t (*AzModuleRegistryReadU32Fn)(
    uint32_t address,
    uint32_t *value);
typedef uint8_t (*AzModuleRegistryWriteU32Fn)(
    uint32_t address,
    uint32_t value);
typedef uint32_t (*AzModuleRegistryLookupFn)(
    uint32_t manager,
    uint32_t key);
typedef uint32_t (*AzModuleRegistryAllocateFn)(uint32_t bytes);
typedef void (*AzModuleRegistryConstructNetDbgFn)(uint32_t wrapper);
typedef void (*AzModuleRegistryResolveNetDbgFn)(uint32_t wrapper);
typedef uint8_t (*AzModuleRegistryWriteLabelFn)(uint32_t wrapper);
typedef uint32_t (*AzModuleRegistryCreateHintFn)(
    uint32_t registry,
    const AzModuleRegistryPair *pair);
typedef void (*AzModuleRegistryInsertFn)(
    AzModuleRegistryPair *pair,
    uint32_t registry,
    uint32_t hint);

typedef struct AzModuleRegistryBindings {
    AzModuleRegistryReadU32Fn read_u32;
    AzModuleRegistryWriteU32Fn write_u32;
    AzModuleRegistryLookupFn lookup;
    AzModuleRegistryAllocateFn allocate;
    AzModuleRegistryConstructNetDbgFn construct_netdbg;
    AzModuleRegistryResolveNetDbgFn resolve_netdbg;
    AzModuleRegistryWriteLabelFn write_label;
    AzModuleRegistryCreateHintFn create_hint;
    AzModuleRegistryInsertFn insert;
} AzModuleRegistryBindings;

typedef enum AzModuleRegistryResult {
    AZ_MODULE_REGISTRY_OK = 0,
    AZ_MODULE_REGISTRY_ALREADY_PRESENT,
    AZ_MODULE_REGISTRY_BAD_BINDINGS,
    AZ_MODULE_REGISTRY_BAD_MANAGER,
    AZ_MODULE_REGISTRY_SOURCE_MISSING,
    AZ_MODULE_REGISTRY_BAD_SOURCE,
    AZ_MODULE_REGISTRY_ALLOC_FAILED,
    AZ_MODULE_REGISTRY_LABEL_FAILED,
    AZ_MODULE_REGISTRY_RESOLVE_FAILED,
    AZ_MODULE_REGISTRY_INSERT_FAILED
} AzModuleRegistryResult;

/* Registers one resident Aurora A-Z wrapper under key 8.  It never loads a
 * second XEX: it reuses the already-live key-7 NetDbg module handle and only
 * resolves the proven key-7 ordinal ABI into the new wrapper. */
AzModuleRegistryResult az_module_registry_register_aurora_az(
    const AzModuleRegistryBindings *bindings,
    uint32_t manager_address,
    uint32_t *registered_wrapper);

const char *az_module_registry_result_name(AzModuleRegistryResult result);

#if defined(AURORAAZ_XBOX360)
/* Uses only the verified Rev1655 addresses above. Call only after
 * az_rev1655_runtime_pin_module has accepted and pinned key 7. */
AzModuleRegistryResult az_rev1655_module_registry_register_default(
    uint32_t *registered_wrapper);
#endif

#ifdef __cplusplus
}
#endif

#endif
