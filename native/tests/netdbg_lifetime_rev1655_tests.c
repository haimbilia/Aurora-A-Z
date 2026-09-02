#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <auroraaz/netdbg_lifetime_rev1655.h>

#define FAKE_MAX_REGIONS 12u
#define FAKE_MAX_REGION_SIZE 256u
#define FAKE_WRAPPER_ADDRESS 0x90001000u
#define FAKE_LABEL_ADDRESS 0x90002000u
#define FAKE_IDENTITY_ADDRESS 0x90002100u
#define FAKE_PATH_ADDRESS 0x90002200u
#define FAKE_MODULE_HANDLE 0x91D00000u
#define FAKE_ORDINAL2 0x91D01100u
#define FAKE_ORDINAL3 0x91D01200u
#define FAKE_ORDINAL4 0x91D01300u
#define FAKE_ORDINAL5 0x91D01400u

typedef struct FakeRegion {
    uint32_t base;
    size_t size;
    uint8_t bytes[FAKE_MAX_REGION_SIZE];
} FakeRegion;

typedef struct FakeLifetime {
    FakeRegion regions[FAKE_MAX_REGIONS];
    size_t region_count;
    AzNetDbgLifetimeRev1655Bindings bindings;
    uint32_t lookup_manager;
    uint32_t lookup_wrapper;
    uint32_t module_handle;
    uint32_t reject_address;
    size_t reject_size;
    uint32_t observed_key;
    uint32_t observed_cas_address;
    uint32_t observed_cas_expected;
    uint32_t observed_cas_desired;
    uint32_t lookup_calls;
    uint32_t module_lookup_calls;
    uint32_t cas_calls;
    uint8_t lookup_succeeds;
    uint8_t module_lookup_succeeds;
    uint8_t cas_loses_race;
    uint8_t corrupt_after_cas;
    uint8_t reject_after_cas;
} FakeLifetime;

static const uint8_t g_label_utf16be[] = {
    0x00u, 0x4Eu, 0x00u, 0x65u, 0x00u, 0x74u, 0x00u, 0x77u,
    0x00u, 0x6Fu, 0x00u, 0x72u, 0x00u, 0x6Bu, 0x00u, 0x20u,
    0x00u, 0x44u, 0x00u, 0x65u, 0x00u, 0x62u, 0x00u, 0x75u,
    0x00u, 0x67u, 0x00u, 0x67u, 0x00u, 0x65u, 0x00u, 0x72u,
    0x00u, 0x00u
};

static int g_failures;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            ++g_failures; \
        } \
    } while (0)

static void fixture_abort(const char *message)
{
    fprintf(stderr, "fixture failure: %s\n", message);
    exit(2);
}

static FakeRegion *fake_add_region(
    FakeLifetime *fake,
    uint32_t base,
    size_t size)
{
    FakeRegion *region;

    if (fake == NULL || size == 0u || size > FAKE_MAX_REGION_SIZE ||
        fake->region_count >= FAKE_MAX_REGIONS) {
        fixture_abort("invalid region");
    }
    region = &fake->regions[fake->region_count];
    ++fake->region_count;
    memset(region, 0, sizeof(*region));
    region->base = base;
    region->size = size;
    return region;
}

static FakeRegion *fake_find_region(
    FakeLifetime *fake,
    uintptr_t address,
    size_t size)
{
    size_t index;

    if (fake == NULL || address > (uintptr_t)UINT32_MAX || size == 0u) {
        return NULL;
    }
    for (index = 0u; index < fake->region_count; ++index) {
        FakeRegion *region = &fake->regions[index];
        const uint32_t candidate = (uint32_t)address;

        if (candidate >= region->base) {
            const size_t offset = (size_t)(candidate - region->base);

            if (offset <= region->size && size <= region->size - offset) {
                return region;
            }
        }
    }
    return NULL;
}

static uint8_t ranges_overlap(
    uintptr_t first_address,
    size_t first_size,
    uint32_t second_address,
    size_t second_size)
{
    const uint64_t first_begin = (uint64_t)first_address;
    const uint64_t second_begin = (uint64_t)second_address;
    const uint64_t first_end = first_begin + (uint64_t)first_size;
    const uint64_t second_end = second_begin + (uint64_t)second_size;

    return (first_size != 0u && second_size != 0u &&
        first_begin < second_end && second_begin < first_end) ? 1u : 0u;
}

static uint8_t fake_read_bytes(
    void *context,
    uintptr_t address,
    void *destination,
    size_t size)
{
    FakeLifetime *fake = (FakeLifetime *)context;
    FakeRegion *region;
    size_t offset;

    if (destination == NULL ||
        ranges_overlap(
            address,
            size,
            fake->reject_address,
            fake->reject_size) != 0u) {
        return 0u;
    }
    region = fake_find_region(fake, address, size);
    if (region == NULL) {
        return 0u;
    }
    offset = (size_t)((uint32_t)address - region->base);
    memcpy(destination, &region->bytes[offset], size);
    return 1u;
}

static uint8_t fake_read_u32(
    void *context,
    uintptr_t address,
    uint32_t *value)
{
    if (value == NULL) {
        return 0u;
    }
    return fake_read_bytes(context, address, value, sizeof(*value));
}

static void fake_write_bytes(
    FakeLifetime *fake,
    uint32_t address,
    const void *source,
    size_t size)
{
    FakeRegion *region = fake_find_region(fake, (uintptr_t)address, size);
    size_t offset;

    if (region == NULL || source == NULL) {
        fixture_abort("write outside fake memory");
    }
    offset = (size_t)(address - region->base);
    memcpy(&region->bytes[offset], source, size);
}

static void fake_write_u32(
    FakeLifetime *fake,
    uint32_t address,
    uint32_t value)
{
    fake_write_bytes(fake, address, &value, sizeof(value));
}

static void fake_write_byte(
    FakeLifetime *fake,
    uint32_t address,
    uint8_t value)
{
    fake_write_bytes(fake, address, &value, sizeof(value));
}

static uint32_t fake_get_u32(FakeLifetime *fake, uint32_t address)
{
    uint32_t value = 0u;

    if (fake_read_u32(fake, (uintptr_t)address, &value) == 0u) {
        fixture_abort("read outside fake memory");
    }
    return value;
}

static uint8_t fake_compare_exchange_u32(
    void *context,
    uintptr_t address,
    uint32_t *expected,
    uint32_t desired)
{
    FakeLifetime *fake = (FakeLifetime *)context;
    uint32_t observed;

    ++fake->cas_calls;
    fake->observed_cas_address = (uint32_t)address;
    fake->observed_cas_expected = expected != NULL ? *expected : 0u;
    fake->observed_cas_desired = desired;
    if (expected == NULL ||
        fake_read_u32(fake, address, &observed) == 0u) {
        return 0u;
    }
    if (fake->cas_loses_race != 0u) {
        observed = AZ_REV1655_NETDBG_POLICY_RESIDENT;
        fake_write_u32(fake, (uint32_t)address, observed);
        *expected = observed;
        return 0u;
    }
    if (observed != *expected) {
        *expected = observed;
        return 0u;
    }
    fake_write_u32(fake, (uint32_t)address, desired);
    if (fake->corrupt_after_cas != 0u) {
        fake_write_u32(fake, (uint32_t)address, 5u);
    }
    if (fake->reject_after_cas != 0u) {
        fake->reject_address = (uint32_t)address;
        fake->reject_size = sizeof(uint32_t);
    }
    return 1u;
}

static uint8_t fake_lookup_plugin(
    void *context,
    uint32_t key,
    uint32_t *manager_address,
    uint32_t *wrapper_address)
{
    FakeLifetime *fake = (FakeLifetime *)context;

    ++fake->lookup_calls;
    fake->observed_key = key;
    if (fake->lookup_succeeds == 0u || manager_address == NULL ||
        wrapper_address == NULL) {
        return 0u;
    }
    *manager_address = fake->lookup_manager;
    *wrapper_address = fake->lookup_wrapper;
    return 1u;
}

static uint8_t fake_get_module_handle(
    void *context,
    const char *identity,
    uint32_t *module_handle)
{
    FakeLifetime *fake = (FakeLifetime *)context;

    ++fake->module_lookup_calls;
    if (fake->module_lookup_succeeds == 0u || identity == NULL ||
        strcmp(identity, AZ_REV1655_NETDBG_IDENTITY) != 0 ||
        module_handle == NULL) {
        return 0u;
    }
    *module_handle = fake->module_handle;
    return 1u;
}

static void fake_init(FakeLifetime *fake)
{
    size_t index;
    FakeRegion *region;

    memset(fake, 0, sizeof(*fake));
    fake->lookup_manager = AZ_REV1655_PLUGIN_MANAGER_ADDRESS;
    fake->lookup_wrapper = FAKE_WRAPPER_ADDRESS;
    fake->module_handle = FAKE_MODULE_HANDLE;
    fake->lookup_succeeds = 1u;
    fake->module_lookup_succeeds = 1u;

    for (index = 0u;
         index < az_rev1655_netdbg_lifetime_validation_span_count();
         ++index) {
        AzNetDbgLifetimeValidationSpan span;

        if (az_rev1655_netdbg_lifetime_validation_span(index, &span) == 0u) {
            fixture_abort("missing validation span");
        }
        region = fake_add_region(fake, (uint32_t)span.address, span.size);
        memcpy(region->bytes, span.expected, span.size);
    }

    (void)fake_add_region(
        fake, AZ_REV1655_PLUGIN_MANAGER_ADDRESS, sizeof(uint32_t));
    (void)fake_add_region(fake, AZ_REV1655_NETDBG_VTABLE, 0x24u);
    (void)fake_add_region(fake, FAKE_WRAPPER_ADDRESS, 0xA8u);
    region = fake_add_region(
        fake, FAKE_LABEL_ADDRESS, sizeof(g_label_utf16be));
    memcpy(region->bytes, g_label_utf16be, sizeof(g_label_utf16be));
    region = fake_add_region(
        fake,
        FAKE_IDENTITY_ADDRESS,
        sizeof(AZ_REV1655_NETDBG_IDENTITY));
    memcpy(
        region->bytes,
        AZ_REV1655_NETDBG_IDENTITY,
        sizeof(AZ_REV1655_NETDBG_IDENTITY));
    region = fake_add_region(
        fake, FAKE_PATH_ADDRESS, sizeof(AZ_REV1655_NETDBG_PATH));
    memcpy(
        region->bytes,
        AZ_REV1655_NETDBG_PATH,
        sizeof(AZ_REV1655_NETDBG_PATH));

    fake_write_u32(
        fake,
        AZ_REV1655_PLUGIN_MANAGER_ADDRESS,
        AZ_REV1655_PLUGIN_MANAGER_VTABLE);
    fake_write_u32(
        fake,
        AZ_REV1655_NETDBG_VTABLE + 0x04u,
        AZ_REV1655_NETDBG_RESOLVER_ADDRESS);
    fake_write_u32(
        fake,
        AZ_REV1655_NETDBG_VTABLE + 0x18u,
        AZ_REV1655_MODULE_LOAD_ADDRESS);
    fake_write_u32(
        fake,
        AZ_REV1655_NETDBG_VTABLE + 0x1Cu,
        AZ_REV1655_MODULE_UNLOAD_ADDRESS);

    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_VTABLE_OFFSET,
        AZ_REV1655_NETDBG_VTABLE);

    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_LABEL_OFFSET,
        FAKE_LABEL_ADDRESS);
    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_LABEL_OFFSET + 0x10u,
        (uint32_t)(sizeof(AZ_REV1655_NETDBG_LABEL) - 1u));
    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_LABEL_OFFSET + 0x14u,
        (uint32_t)(sizeof(AZ_REV1655_NETDBG_LABEL) - 1u));

    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_IDENTITY_OFFSET,
        FAKE_IDENTITY_ADDRESS);
    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_IDENTITY_OFFSET + 0x10u,
        (uint32_t)(sizeof(AZ_REV1655_NETDBG_IDENTITY) - 1u));
    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_IDENTITY_OFFSET + 0x14u,
        (uint32_t)(sizeof(AZ_REV1655_NETDBG_IDENTITY) - 1u));

    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_PATH_OFFSET,
        FAKE_PATH_ADDRESS);
    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_PATH_OFFSET + 0x10u,
        (uint32_t)(sizeof(AZ_REV1655_NETDBG_PATH) - 1u));
    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_PATH_OFFSET + 0x14u,
        (uint32_t)(sizeof(AZ_REV1655_NETDBG_PATH) - 1u));

    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_MODE_OFFSET,
        AZ_REV1655_NETDBG_MODE);
    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_POLICY_OFFSET,
        AZ_REV1655_NETDBG_POLICY_UNLOADABLE);
    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_HANDLE_OFFSET,
        FAKE_MODULE_HANDLE);
    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_ORDINAL2_OFFSET,
        FAKE_ORDINAL2);
    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_ORDINAL3_OFFSET,
        FAKE_ORDINAL3);
    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_ORDINAL4_OFFSET,
        FAKE_ORDINAL4);
    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_ORDINAL5_OFFSET,
        FAKE_ORDINAL5);
    fake_write_u32(
        fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_READY_OFFSET,
        1u);

    fake->bindings.context = fake;
    fake->bindings.read_bytes = fake_read_bytes;
    fake->bindings.read_u32 = fake_read_u32;
    fake->bindings.compare_exchange_u32 = fake_compare_exchange_u32;
    fake->bindings.lookup_plugin = fake_lookup_plugin;
    fake->bindings.get_module_handle = fake_get_module_handle;
    fake->bindings.exact_image_verified = 1u;
}

static AzNetDbgLifetimeRev1655Result call_pin(
    FakeLifetime *fake,
    AzNetDbgLifetimeRev1655Status *status)
{
    return az_rev1655_netdbg_lifetime_pin(
        &fake->bindings, FAKE_ORDINAL4, status);
}

static void expect_no_cas(
    FakeLifetime *fake,
    AzNetDbgLifetimeRev1655Result expected)
{
    AzNetDbgLifetimeRev1655Status status;
    const AzNetDbgLifetimeRev1655Result actual = call_pin(fake, &status);

    CHECK(actual == expected);
    CHECK(status.result == expected);
    CHECK(status.pinned_for_title_lifetime == 0u);
    CHECK(fake->cas_calls == 0u);
}

static void test_span_contract(void)
{
    static const uintptr_t expected_addresses[] = {
        0x82227008u,
        0x82227044u,
        0x82227928u,
        0x82389000u
    };
    static const size_t expected_sizes[] = { 32u, 28u, 32u, 36u };
    size_t index;
    AzNetDbgLifetimeValidationSpan span;

    CHECK(az_rev1655_netdbg_lifetime_validation_span_count() == 4u);
    for (index = 0u; index < 4u; ++index) {
        CHECK(az_rev1655_netdbg_lifetime_validation_span(index, &span) != 0u);
        CHECK(span.address == expected_addresses[index]);
        CHECK(span.size == expected_sizes[index]);
        CHECK(span.expected != NULL);
    }
    CHECK(az_rev1655_netdbg_lifetime_validation_span(4u, &span) == 0u);
    CHECK(az_rev1655_netdbg_lifetime_validation_span(0u, NULL) == 0u);
}

static void test_happy_pin(void)
{
    FakeLifetime fake;
    AzNetDbgLifetimeRev1655Status status;

    fake_init(&fake);
    CHECK(call_pin(&fake, &status) == AZ_NETDBG_LIFETIME_OK);
    CHECK(status.result == AZ_NETDBG_LIFETIME_OK);
    CHECK(status.failed_validation_span == UINT32_MAX);
    CHECK(status.manager_address == AZ_REV1655_PLUGIN_MANAGER_ADDRESS);
    CHECK(status.wrapper_address == FAKE_WRAPPER_ADDRESS);
    CHECK(status.wrapper_vtable == AZ_REV1655_NETDBG_VTABLE);
    CHECK(status.module_handle == FAKE_MODULE_HANDLE);
    CHECK(status.wrapper_handle == FAKE_MODULE_HANDLE);
    CHECK(status.mode == AZ_REV1655_NETDBG_MODE);
    CHECK(status.policy_before == AZ_REV1655_NETDBG_POLICY_UNLOADABLE);
    CHECK(status.policy_after == AZ_REV1655_NETDBG_POLICY_RESIDENT);
    CHECK(status.ordinal2 == FAKE_ORDINAL2);
    CHECK(status.ordinal3 == FAKE_ORDINAL3);
    CHECK(status.ordinal4 == FAKE_ORDINAL4);
    CHECK(status.ordinal5 == FAKE_ORDINAL5);
    CHECK(status.ready == 1u);
    CHECK(status.signatures_verified == 1u);
    CHECK(status.object_verified == 1u);
    CHECK(status.strings_verified == 1u);
    CHECK(status.compare_exchange_succeeded == 1u);
    CHECK(status.pinned_for_title_lifetime == 1u);
    CHECK(fake.lookup_calls == 1u);
    CHECK(fake.observed_key == AZ_REV1655_NETDBG_KEY);
    CHECK(fake.module_lookup_calls == 1u);
    CHECK(fake.cas_calls == 1u);
    CHECK(fake.observed_cas_address ==
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_POLICY_OFFSET);
    CHECK(fake.observed_cas_expected ==
        AZ_REV1655_NETDBG_POLICY_UNLOADABLE);
    CHECK(fake.observed_cas_desired == AZ_REV1655_NETDBG_POLICY_RESIDENT);
    CHECK(fake_get_u32(
        &fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_POLICY_OFFSET) ==
        AZ_REV1655_NETDBG_POLICY_RESIDENT);
}

static void test_entry_guards(void)
{
    FakeLifetime fake;
    AzNetDbgLifetimeRev1655Status status;
    AzNetDbgLifetimeRev1655Bindings bindings;

    fake_init(&fake);
    CHECK(az_rev1655_netdbg_lifetime_pin(
        NULL, FAKE_ORDINAL4, &status) == AZ_NETDBG_LIFETIME_NULL);
    CHECK(status.result == AZ_NETDBG_LIFETIME_NULL);
    CHECK(az_rev1655_netdbg_lifetime_pin(
        &fake.bindings, FAKE_ORDINAL4, NULL) == AZ_NETDBG_LIFETIME_NULL);

    bindings = fake.bindings;
    bindings.read_bytes = NULL;
    CHECK(az_rev1655_netdbg_lifetime_pin(
        &bindings, FAKE_ORDINAL4, &status) ==
        AZ_NETDBG_LIFETIME_BAD_BINDINGS);
    CHECK(fake.cas_calls == 0u);

    fake.bindings.exact_image_verified = 0u;
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_IMAGE_UNVERIFIED);

    fake_init(&fake);
    CHECK(az_rev1655_netdbg_lifetime_pin(
        &fake.bindings, 0u, &status) ==
        AZ_NETDBG_LIFETIME_BAD_WRITE_EXPORT);
    CHECK(fake.cas_calls == 0u);
}

static void test_signature_guards(void)
{
    size_t index;

    for (index = 0u;
         index < az_rev1655_netdbg_lifetime_validation_span_count();
         ++index) {
        FakeLifetime fake;
        AzNetDbgLifetimeRev1655Status status;
        AzNetDbgLifetimeValidationSpan span;

        fake_init(&fake);
        CHECK(az_rev1655_netdbg_lifetime_validation_span(index, &span) != 0u);
        fake.reject_address = (uint32_t)span.address;
        fake.reject_size = span.size;
        CHECK(call_pin(&fake, &status) ==
            AZ_NETDBG_LIFETIME_SIGNATURE_UNREADABLE);
        CHECK(status.failed_validation_span == (uint32_t)index);
        CHECK(fake.cas_calls == 0u);

        fake_init(&fake);
        fake_write_byte(
            &fake,
            (uint32_t)span.address,
            (uint8_t)(span.expected[0] ^ 0x01u));
        CHECK(call_pin(&fake, &status) ==
            AZ_NETDBG_LIFETIME_SIGNATURE_MISMATCH);
        CHECK(status.failed_validation_span == (uint32_t)index);
        CHECK(fake.cas_calls == 0u);
    }
}

static void test_lookup_and_type_guards(void)
{
    FakeLifetime fake;
    static const uint32_t vtable_offsets[] = { 0x04u, 0x18u, 0x1Cu };
    size_t index;

    fake_init(&fake);
    fake.lookup_succeeds = 0u;
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_LOOKUP_FAILED);

    fake_init(&fake);
    fake.lookup_manager += 4u;
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_MANAGER);

    fake_init(&fake);
    fake.reject_address = AZ_REV1655_PLUGIN_MANAGER_ADDRESS;
    fake.reject_size = sizeof(uint32_t);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_MANAGER_VTABLE);

    fake_init(&fake);
    fake_write_u32(&fake, AZ_REV1655_PLUGIN_MANAGER_ADDRESS, 0u);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_MANAGER_VTABLE);

    fake_init(&fake);
    fake.lookup_wrapper = 0u;
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_WRAPPER);

    fake_init(&fake);
    fake.lookup_wrapper = FAKE_WRAPPER_ADDRESS + 1u;
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_WRAPPER);

    fake_init(&fake);
    fake.reject_address = FAKE_WRAPPER_ADDRESS;
    fake.reject_size = sizeof(uint32_t);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_WRAPPER_VTABLE);

    fake_init(&fake);
    fake_write_u32(&fake, FAKE_WRAPPER_ADDRESS, 0u);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_WRAPPER_VTABLE);

    for (index = 0u;
         index < sizeof(vtable_offsets) / sizeof(vtable_offsets[0]);
         ++index) {
        fake_init(&fake);
        fake_write_u32(
            &fake,
            AZ_REV1655_NETDBG_VTABLE + vtable_offsets[index],
            0u);
        expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_VTABLE_CONTRACT);

        fake_init(&fake);
        fake.reject_address =
            AZ_REV1655_NETDBG_VTABLE + vtable_offsets[index];
        fake.reject_size = sizeof(uint32_t);
        expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_VTABLE_CONTRACT);
    }
}

static void test_scalar_handle_and_export_guards(void)
{
    FakeLifetime fake;
    static const uint32_t ordinal_offsets[] = {
        AZ_REV1655_NETDBG_ORDINAL2_OFFSET,
        AZ_REV1655_NETDBG_ORDINAL3_OFFSET,
        AZ_REV1655_NETDBG_ORDINAL4_OFFSET,
        AZ_REV1655_NETDBG_ORDINAL5_OFFSET
    };
    size_t index;

    fake_init(&fake);
    fake_write_u32(
        &fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_MODE_OFFSET,
        10u);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_MODE);

    fake_init(&fake);
    fake.reject_address =
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_MODE_OFFSET;
    fake.reject_size = sizeof(uint32_t);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_MODE);

    fake_init(&fake);
    fake_write_u32(
        &fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_POLICY_OFFSET,
        AZ_REV1655_NETDBG_POLICY_RESIDENT);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_POLICY);

    fake_init(&fake);
    fake.reject_address =
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_POLICY_OFFSET;
    fake.reject_size = sizeof(uint32_t);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_POLICY);

    fake_init(&fake);
    fake_write_u32(
        &fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_HANDLE_OFFSET,
        0u);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_HANDLE);

    fake_init(&fake);
    fake.reject_address =
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_HANDLE_OFFSET;
    fake.reject_size = sizeof(uint32_t);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_HANDLE);

    fake_init(&fake);
    fake.module_lookup_succeeds = 0u;
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_MODULE_LOOKUP_FAILED);

    fake_init(&fake);
    fake.module_handle = 0u;
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_MODULE_LOOKUP_FAILED);

    fake_init(&fake);
    fake.module_handle += 0x1000u;
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_HANDLE_MISMATCH);

    for (index = 0u;
         index < sizeof(ordinal_offsets) / sizeof(ordinal_offsets[0]);
         ++index) {
        fake_init(&fake);
        fake_write_u32(
            &fake,
            FAKE_WRAPPER_ADDRESS + ordinal_offsets[index],
            0u);
        expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_EXPORTS);

        fake_init(&fake);
        fake.reject_address =
            FAKE_WRAPPER_ADDRESS + ordinal_offsets[index];
        fake.reject_size = sizeof(uint32_t);
        expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_EXPORTS);
    }

    fake_init(&fake);
    fake_write_u32(
        &fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_ORDINAL4_OFFSET,
        FAKE_ORDINAL4 + 4u);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_WRITE_EXPORT_MISMATCH);

    fake_init(&fake);
    fake_write_u32(
        &fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_READY_OFFSET,
        0u);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_READY);

    fake_init(&fake);
    fake.reject_address =
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_READY_OFFSET;
    fake.reject_size = sizeof(uint32_t);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_BAD_READY);
}

static void test_string_guards(void)
{
    FakeLifetime fake;

    fake_init(&fake);
    fake.reject_address = FAKE_LABEL_ADDRESS;
    fake.reject_size = sizeof(g_label_utf16be);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_LABEL_UNREADABLE);

    fake_init(&fake);
    fake_write_byte(&fake, FAKE_LABEL_ADDRESS + 1u, 0x4Fu);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_LABEL_MISMATCH);

    fake_init(&fake);
    fake_write_u32(
        &fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_LABEL_OFFSET + 0x10u,
        15u);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_LABEL_MISMATCH);

    fake_init(&fake);
    fake.reject_address = FAKE_IDENTITY_ADDRESS;
    fake.reject_size = sizeof(AZ_REV1655_NETDBG_IDENTITY);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_IDENTITY_UNREADABLE);

    fake_init(&fake);
    fake_write_byte(&fake, FAKE_IDENTITY_ADDRESS, (uint8_t)'D');
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_IDENTITY_MISMATCH);

    fake_init(&fake);
    fake_write_u32(
        &fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_IDENTITY_OFFSET + 0x14u,
        1u);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_IDENTITY_MISMATCH);

    fake_init(&fake);
    fake.reject_address = FAKE_PATH_ADDRESS;
    fake.reject_size = sizeof(AZ_REV1655_NETDBG_PATH);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_PATH_UNREADABLE);

    fake_init(&fake);
    fake_write_byte(&fake, FAKE_PATH_ADDRESS, (uint8_t)'G');
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_PATH_MISMATCH);

    fake_init(&fake);
    fake_write_u32(
        &fake,
        FAKE_WRAPPER_ADDRESS + AZ_REV1655_NETDBG_PATH_OFFSET + 0x10u,
        1u);
    expect_no_cas(&fake, AZ_NETDBG_LIFETIME_PATH_MISMATCH);
}

static void test_atomic_fail_closed_paths(void)
{
    FakeLifetime fake;
    AzNetDbgLifetimeRev1655Status status;

    fake_init(&fake);
    fake.cas_loses_race = 1u;
    CHECK(call_pin(&fake, &status) ==
        AZ_NETDBG_LIFETIME_POLICY_CAS_FAILED);
    CHECK(status.object_verified == 1u);
    CHECK(status.strings_verified == 1u);
    CHECK(status.compare_exchange_succeeded == 0u);
    CHECK(status.pinned_for_title_lifetime == 0u);
    CHECK(status.policy_after == AZ_REV1655_NETDBG_POLICY_RESIDENT);
    CHECK(fake.cas_calls == 1u);

    fake_init(&fake);
    fake.corrupt_after_cas = 1u;
    CHECK(call_pin(&fake, &status) ==
        AZ_NETDBG_LIFETIME_POLICY_READBACK_FAILED);
    CHECK(status.compare_exchange_succeeded == 1u);
    CHECK(status.pinned_for_title_lifetime == 0u);
    CHECK(status.policy_after == 5u);
    CHECK(fake.cas_calls == 1u);

    fake_init(&fake);
    fake.reject_after_cas = 1u;
    CHECK(call_pin(&fake, &status) ==
        AZ_NETDBG_LIFETIME_POLICY_READBACK_FAILED);
    CHECK(status.compare_exchange_succeeded == 1u);
    CHECK(status.pinned_for_title_lifetime == 0u);
    CHECK(status.policy_after == 0u);
    CHECK(fake.cas_calls == 1u);
}

static void test_result_names(void)
{
    int value;

    for (value = (int)AZ_NETDBG_LIFETIME_OK;
         value <= (int)AZ_NETDBG_LIFETIME_POLICY_READBACK_FAILED;
         ++value) {
        CHECK(strcmp(
            az_netdbg_lifetime_rev1655_result_name(
                (AzNetDbgLifetimeRev1655Result)value),
            "unknown") != 0);
    }
    CHECK(strcmp(
        az_netdbg_lifetime_rev1655_result_name(
            (AzNetDbgLifetimeRev1655Result)999),
        "unknown") == 0);
}

int main(void)
{
    test_span_contract();
    test_happy_pin();
    test_entry_guards();
    test_signature_guards();
    test_lookup_and_type_guards();
    test_scalar_handle_and_export_guards();
    test_string_guards();
    test_atomic_fail_closed_paths();
    test_result_names();

    if (g_failures != 0) {
        fprintf(stderr, "%d NetDbg lifetime test(s) failed\n", g_failures);
        return 1;
    }
    puts("Aurora Rev1655 NetDbg lifetime pin tests passed");
    return 0;
}
