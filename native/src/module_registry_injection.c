#include <stddef.h>

#include <auroraaz/module_registry_injection.h>

static uint8_t bindings_are_valid(const AzModuleRegistryBindings *bindings)
{
    return bindings != NULL &&
        bindings->read_u32 != NULL &&
        bindings->write_u32 != NULL &&
        bindings->lookup != NULL &&
        bindings->allocate != NULL &&
        bindings->construct_netdbg != NULL &&
        bindings->resolve_netdbg != NULL &&
        bindings->write_label != NULL &&
        bindings->create_hint != NULL &&
        bindings->insert != NULL;
}

AzModuleRegistryResult az_module_registry_register_aurora_az(
    const AzModuleRegistryBindings *bindings,
    uint32_t manager_address,
    uint32_t *registered_wrapper)
{
    AzModuleRegistryPair pair;
    uint32_t manager_vtable;
    uint32_t source_wrapper;
    uint32_t source_vtable;
    uint32_t source_handle;
    uint32_t source_ready;
    uint32_t wrapper;
    uint32_t wrapper_ready;
    uint32_t hint;

    if (registered_wrapper != NULL) {
        *registered_wrapper = 0u;
    }
    if (bindings_are_valid(bindings) == 0u) {
        return AZ_MODULE_REGISTRY_BAD_BINDINGS;
    }
    if (manager_address != AZ_REV1655_PLUGIN_MANAGER_ADDRESS ||
        bindings->read_u32(manager_address, &manager_vtable) == 0u ||
        manager_vtable != AZ_REV1655_PLUGIN_MANAGER_VTABLE) {
        return AZ_MODULE_REGISTRY_BAD_MANAGER;
    }

    wrapper = bindings->lookup(manager_address, AZ_REV1655_AZ_MODULE_KEY);
    if (wrapper != 0u) {
        if (registered_wrapper != NULL) {
            *registered_wrapper = wrapper;
        }
        return AZ_MODULE_REGISTRY_ALREADY_PRESENT;
    }

    source_wrapper = bindings->lookup(
        manager_address, AZ_REV1655_NETDBG_SOURCE_KEY);
    if (source_wrapper == 0u) {
        return AZ_MODULE_REGISTRY_SOURCE_MISSING;
    }
    if (bindings->read_u32(source_wrapper, &source_vtable) == 0u ||
        bindings->read_u32(
            source_wrapper + AZ_REV1655_WRAPPER_HANDLE_OFFSET,
            &source_handle) == 0u ||
        bindings->read_u32(
            source_wrapper + AZ_REV1655_WRAPPER_READY_OFFSET,
            &source_ready) == 0u ||
        source_vtable != AZ_REV1655_NETDBG_WRAPPER_VTABLE ||
        source_handle == 0u || source_ready == 0u) {
        return AZ_MODULE_REGISTRY_BAD_SOURCE;
    }

    wrapper = bindings->allocate(AZ_REV1655_NETDBG_WRAPPER_SIZE);
    if (wrapper == 0u) {
        return AZ_MODULE_REGISTRY_ALLOC_FAILED;
    }
    bindings->construct_netdbg(wrapper);
    if (bindings->write_label(wrapper) == 0u ||
        bindings->write_u32(
            wrapper + AZ_REV1655_WRAPPER_POLICY_OFFSET,
            AZ_REV1655_WRAPPER_RESIDENT_POLICY) == 0u) {
        return AZ_MODULE_REGISTRY_LABEL_FAILED;
    }
    if (bindings->write_u32(
            wrapper + AZ_REV1655_WRAPPER_HANDLE_OFFSET,
            source_handle) == 0u) {
        return AZ_MODULE_REGISTRY_RESOLVE_FAILED;
    }
    bindings->resolve_netdbg(wrapper);
    if (bindings->read_u32(
            wrapper + AZ_REV1655_WRAPPER_READY_OFFSET,
            &wrapper_ready) == 0u || wrapper_ready == 0u) {
        return AZ_MODULE_REGISTRY_RESOLVE_FAILED;
    }

    pair.key = AZ_REV1655_AZ_MODULE_KEY;
    pair.value = wrapper;
    hint = bindings->create_hint(manager_address + 4u, &pair);
    if (hint == 0u) {
        return AZ_MODULE_REGISTRY_INSERT_FAILED;
    }
    bindings->insert(&pair, manager_address + 4u, hint);
    if (bindings->lookup(manager_address, AZ_REV1655_AZ_MODULE_KEY) != wrapper) {
        return AZ_MODULE_REGISTRY_INSERT_FAILED;
    }
    if (registered_wrapper != NULL) {
        *registered_wrapper = wrapper;
    }
    return AZ_MODULE_REGISTRY_OK;
}

const char *az_module_registry_result_name(AzModuleRegistryResult result)
{
    switch (result) {
    case AZ_MODULE_REGISTRY_OK:
        return "ok";
    case AZ_MODULE_REGISTRY_ALREADY_PRESENT:
        return "already-present";
    case AZ_MODULE_REGISTRY_BAD_BINDINGS:
        return "bad-bindings";
    case AZ_MODULE_REGISTRY_BAD_MANAGER:
        return "bad-manager";
    case AZ_MODULE_REGISTRY_SOURCE_MISSING:
        return "source-missing";
    case AZ_MODULE_REGISTRY_BAD_SOURCE:
        return "bad-source";
    case AZ_MODULE_REGISTRY_ALLOC_FAILED:
        return "alloc-failed";
    case AZ_MODULE_REGISTRY_LABEL_FAILED:
        return "label-failed";
    case AZ_MODULE_REGISTRY_RESOLVE_FAILED:
        return "resolve-failed";
    case AZ_MODULE_REGISTRY_INSERT_FAILED:
        return "insert-failed";
    default:
        return "unknown";
    }
}
