#include <stdio.h>
#include <string.h>

#include <auroraaz/module_registry_injection.h>

#define MANAGER AZ_REV1655_PLUGIN_MANAGER_ADDRESS
#define SOURCE 0x2000u
#define CLONE 0x3000u
#define HANDLE 0x4000u

typedef struct FakeMemory {
    uint32_t manager_vtable;
    uint32_t source_vtable;
    uint32_t source_handle;
    uint32_t source_ready;
    uint32_t clone_handle;
    uint32_t clone_policy;
    uint32_t clone_ready;
    uint32_t registered_clone;
    uint8_t allocated;
    uint8_t constructed;
    uint8_t labelled;
    uint8_t resolved;
    uint8_t inserted;
} FakeMemory;

static FakeMemory fake;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed: %s (%d)\n", #condition, __LINE__); \
        return 1; \
    } \
} while (0)

static uint8_t read_u32(uint32_t address, uint32_t *value)
{
    if (value == NULL) {
        return 0u;
    }
    if (address == MANAGER) {
        *value = fake.manager_vtable;
    } else if (address == SOURCE) {
        *value = fake.source_vtable;
    } else if (address == SOURCE + AZ_REV1655_WRAPPER_HANDLE_OFFSET) {
        *value = fake.source_handle;
    } else if (address == SOURCE + AZ_REV1655_WRAPPER_READY_OFFSET) {
        *value = fake.source_ready;
    } else if (address == CLONE + AZ_REV1655_WRAPPER_READY_OFFSET) {
        *value = fake.clone_ready;
    } else {
        return 0u;
    }
    return 1u;
}

static uint8_t write_u32(uint32_t address, uint32_t value)
{
    if (address == CLONE + AZ_REV1655_WRAPPER_HANDLE_OFFSET) {
        fake.clone_handle = value;
    } else if (address == CLONE + AZ_REV1655_WRAPPER_POLICY_OFFSET) {
        fake.clone_policy = value;
    } else {
        return 0u;
    }
    return 1u;
}

static uint32_t lookup(uint32_t manager, uint32_t key)
{
    if (manager != MANAGER) {
        return 0u;
    }
    if (key == AZ_REV1655_NETDBG_SOURCE_KEY) {
        return SOURCE;
    }
    return key == AZ_REV1655_AZ_MODULE_KEY ? fake.registered_clone : 0u;
}

static uint32_t allocate(uint32_t bytes)
{
    fake.allocated = bytes == AZ_REV1655_NETDBG_WRAPPER_SIZE ? 1u : 0u;
    return CLONE;
}

static void construct_netdbg(uint32_t wrapper)
{
    fake.constructed = wrapper == CLONE ? 1u : 0u;
}

static void resolve_netdbg(uint32_t wrapper)
{
    fake.resolved = wrapper == CLONE ? 1u : 0u;
    if (fake.resolved != 0u && fake.clone_handle == HANDLE) {
        fake.clone_ready = 1u;
    }
}

static uint8_t write_label(uint32_t wrapper)
{
    fake.labelled = wrapper == CLONE ? 1u : 0u;
    return fake.labelled;
}

static uint32_t create_hint(uint32_t registry, const AzModuleRegistryPair *pair)
{
    return registry == MANAGER + 4u && pair != NULL &&
        pair->key == AZ_REV1655_AZ_MODULE_KEY && pair->value == CLONE ?
        0x5000u : 0u;
}

static void insert(AzModuleRegistryPair *pair, uint32_t registry, uint32_t hint)
{
    if (pair != NULL && registry == MANAGER + 4u && hint == 0x5000u) {
        fake.registered_clone = pair->value;
        fake.inserted = 1u;
    }
}

static AzModuleRegistryBindings bindings = {
    read_u32, write_u32, lookup, allocate, construct_netdbg, resolve_netdbg,
    write_label, create_hint, insert
};

static void reset_fake(void)
{
    memset(&fake, 0, sizeof(fake));
    fake.manager_vtable = AZ_REV1655_PLUGIN_MANAGER_VTABLE;
    fake.source_vtable = AZ_REV1655_NETDBG_WRAPPER_VTABLE;
    fake.source_handle = HANDLE;
    fake.source_ready = 1u;
}

static int test_registers_distinct_entry(void)
{
    uint32_t wrapper = 0u;
    reset_fake();
    CHECK(az_module_registry_register_aurora_az(
              &bindings, 0x1000u, &wrapper) == AZ_MODULE_REGISTRY_BAD_MANAGER);
    CHECK(az_module_registry_register_aurora_az(
              &bindings, AZ_REV1655_PLUGIN_MANAGER_ADDRESS, &wrapper) ==
          AZ_MODULE_REGISTRY_OK);
    CHECK(wrapper == CLONE);
    CHECK(fake.allocated != 0u && fake.constructed != 0u);
    CHECK(fake.labelled != 0u && fake.resolved != 0u && fake.inserted != 0u);
    CHECK(fake.clone_handle == HANDLE);
    CHECK(fake.clone_policy == AZ_REV1655_WRAPPER_RESIDENT_POLICY);
    return 0;
}

static int test_rejects_unready_source(void)
{
    reset_fake();
    fake.source_ready = 0u;
    CHECK(az_module_registry_register_aurora_az(
              &bindings, AZ_REV1655_PLUGIN_MANAGER_ADDRESS, NULL) ==
          AZ_MODULE_REGISTRY_BAD_SOURCE);
    CHECK(fake.allocated == 0u);
    return 0;
}

static int test_is_idempotent(void)
{
    uint32_t wrapper = 0u;
    reset_fake();
    fake.registered_clone = CLONE;
    CHECK(az_module_registry_register_aurora_az(
              &bindings, AZ_REV1655_PLUGIN_MANAGER_ADDRESS, &wrapper) ==
          AZ_MODULE_REGISTRY_ALREADY_PRESENT);
    CHECK(wrapper == CLONE);
    CHECK(fake.allocated == 0u);
    return 0;
}

int main(void)
{
    if (test_registers_distinct_entry() != 0 ||
        test_rejects_unready_source() != 0 ||
        test_is_idempotent() != 0) {
        return 1;
    }
    puts("AuroraAZ module registry injection tests passed");
    return 0;
}
