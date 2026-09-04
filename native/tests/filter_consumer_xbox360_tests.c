#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <auroraaz/filter_consumer_xbox360.h>
#include <auroraaz/filters.h>
#include <auroraaz/image.h>

#include "../src/rev1655_hook_gate_private.h"

#define TEST_WORKER_THREAD 0x1655u
#define TEST_MAX_STRINGS 96u
#define TEST_MAX_VECTORS 8u
#define TEST_MAX_RAW_FILTERS 16u
#define TEST_CAPTURE_TEXT 512u
#define TEST_REV1655_TEXT_RVA 0x00210000u
#define TEST_REV1655_THUNK_OFFSET 0x00955DFCu
#define TEST_REV1655_THUNK_COUNT 350u
#define TEST_REV1655_THUNK_SIZE 16u

static int failures = 0;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

typedef struct TestStringRecord {
    AzRev1655AuroraString *owner;
    AzRev1655ActiveAggregateD0 *aggregate_owner;
    char text[AZ_REV1655_FILTER_MAX_TEXT + 1u];
    uint32_t length;
    uint32_t capacity;
    uint8_t active;
} TestStringRecord;

typedef struct TestVectorRecord {
    AzRev1655AuroraStringVector *owner;
    AzRev1655ActiveAggregateD0 *aggregate_owner;
    AzRev1655AuroraString elements[TEST_MAX_RAW_FILTERS];
    uint32_t count;
    uint8_t active;
} TestVectorRecord;

typedef struct TestScheduledCapture {
    uint32_t flags;
    uint32_t quickview_id;
    uint32_t include_hidden;
    uint32_t favorites_only;
    uint32_t sort_behavior;
    uint32_t content_context_id;
    uint32_t raw_count;
    char sort_method[TEST_CAPTURE_TEXT];
    char compiled[TEST_CAPTURE_TEXT];
    char search[TEST_CAPTURE_TEXT];
    char context0[TEST_CAPTURE_TEXT];
    char context1[TEST_CAPTURE_TEXT];
    char raw[TEST_MAX_RAW_FILTERS][TEST_CAPTURE_TEXT];
} TestScheduledCapture;

typedef struct TestHost {
    uint8_t gcm[0x200];
    uint32_t registry;
    uint32_t current_thread_id;
    uint8_t affinity_verified;
    uint8_t coverflow_interactive;
    uint8_t queue_idle;
    uint8_t addresses_valid;
    AzInputDetourResult take_result;
    uint8_t requested_index;
    int32_t schedule_result;
    int32_t registry_failure_index;
    uint8_t fail_copy;
    uint8_t fail_construct;
    uint8_t fail_assign;
    uint8_t fail_push;
    uint8_t malformed_string;
    uint32_t initial_quickview_id;
    uint32_t initial_include_hidden;
    uint32_t initial_favorites_only;
    uint32_t initial_sort_behavior;
    uint32_t initial_content_context_id;
    const char *initial_sort_method;
    const char *initial_compiled;
    const char *initial_search;
    const char *initial_context0;
    const char *initial_context1;
    const char *initial_raw[TEST_MAX_RAW_FILTERS];
    uint32_t initial_raw_count;
    TestStringRecord strings[TEST_MAX_STRINGS];
    TestVectorRecord vectors[TEST_MAX_VECTORS];
    TestScheduledCapture scheduled;
    uint32_t registry_calls;
    uint32_t copy_calls;
    uint32_t destroy_calls;
    uint32_t construct_calls;
    uint32_t assign_calls;
    uint32_t push_calls;
    uint32_t erase_calls;
    uint32_t schedule_calls;
    uint32_t take_calls;
    uint32_t finish_calls;
} TestHost;

typedef struct TestImportResolver {
    uint32_t targets[TEST_REV1655_THUNK_COUNT];
    AzRev1655ImportLibrary libraries[TEST_REV1655_THUNK_COUNT];
    uint16_t ordinals[TEST_REV1655_THUNK_COUNT];
    size_t fail_index;
    size_t calls;
} TestImportResolver;

static TestImportResolver g_test_import_resolver;
static AzRev1655ImportResolver g_test_import_resolver_api;

static const uint8_t k_xex_sha256[32] = {
    0x58, 0x3B, 0xCD, 0x44, 0x2D, 0x80, 0x17, 0xD6,
    0xFC, 0xB2, 0x64, 0x5B, 0x93, 0xCD, 0xA9, 0x87,
    0xF4, 0xC0, 0xA4, 0x3A, 0x68, 0x8B, 0x65, 0x2D,
    0x73, 0x64, 0xCC, 0xAE, 0xDA, 0xEE, 0xFA, 0x9F
};

static const uint8_t k_pe_sha256[32] = {
    0x5B, 0xB5, 0xBA, 0xF8, 0xDF, 0x4C, 0xCB, 0x19,
    0x72, 0x41, 0xB3, 0x49, 0x35, 0xEB, 0x40, 0x0F,
    0x36, 0xC8, 0xC2, 0x06, 0x48, 0xCC, 0x07, 0x4E,
    0x2C, 0x30, 0xFA, 0x80, 0xAD, 0xD3, 0x7E, 0x3C
};

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
        (uint16_t)((uint16_t)bytes[1] << 8u));
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8u) |
        ((uint32_t)bytes[2] << 16u) |
        ((uint32_t)bytes[3] << 24u);
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

static int range_fits(size_t offset, size_t length, size_t total)
{
    return offset <= total && length <= total - offset;
}

static uint8_t *load_pe_as_image(const char *path)
{
    FILE *file;
    long file_length;
    uint8_t *raw = NULL;
    uint8_t *image = NULL;
    size_t raw_size;
    size_t pe_offset;
    size_t optional_offset;
    size_t section_table_offset;
    size_t headers_size;
    uint16_t section_count;
    uint16_t optional_size;
    uint16_t section_index;

    file = fopen(path, "rb");
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0) {
        if (file != NULL) {
            fclose(file);
        }
        return NULL;
    }
    file_length = ftell(file);
    if (file_length <= 0L || fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    raw_size = (size_t)file_length;
    raw = (uint8_t *)malloc(raw_size);
    if (raw == NULL || fread(raw, 1u, raw_size, file) != raw_size) {
        free(raw);
        fclose(file);
        return NULL;
    }
    fclose(file);

    if (!range_fits(0u, 0x40u, raw_size)) {
        free(raw);
        return NULL;
    }
    pe_offset = (size_t)read_u32_le(raw + 0x3Cu);
    if (!range_fits(pe_offset, 24u, raw_size)) {
        free(raw);
        return NULL;
    }
    section_count = read_u16_le(raw + pe_offset + 6u);
    optional_size = read_u16_le(raw + pe_offset + 20u);
    optional_offset = pe_offset + 24u;
    if (!range_fits(optional_offset, (size_t)optional_size, raw_size)) {
        free(raw);
        return NULL;
    }
    headers_size = (size_t)read_u32_le(raw + optional_offset + 60u);
    section_table_offset = optional_offset + (size_t)optional_size;
    if (!range_fits(0u, headers_size, raw_size) ||
        !range_fits(section_table_offset,
            (size_t)section_count * 40u, raw_size)) {
        free(raw);
        return NULL;
    }

    image = (uint8_t *)calloc(AZ_REV1655_NT_IMAGE_SIZE, 1u);
    if (image == NULL) {
        free(raw);
        return NULL;
    }
    memcpy(image, raw, headers_size);

    for (section_index = 0u; section_index < section_count; ++section_index) {
        const uint8_t *section = raw + section_table_offset +
            ((size_t)section_index * 40u);
        const size_t virtual_address =
            (size_t)read_u32_le(section + 12u);
        const size_t raw_section_size =
            (size_t)read_u32_le(section + 16u);
        const size_t raw_offset = (size_t)read_u32_le(section + 20u);

        if (!range_fits(raw_offset, raw_section_size, raw_size) ||
            !range_fits(virtual_address, raw_section_size,
                AZ_REV1655_NT_IMAGE_SIZE)) {
            free(image);
            free(raw);
            return NULL;
        }
        memcpy(image + virtual_address, raw + raw_offset, raw_section_size);
    }

    free(raw);
    return image;
}

static int test_resolve_import(
    void *context,
    AzRev1655ImportLibrary library,
    uint16_t ordinal,
    size_t thunk_index,
    uint32_t *out_target)
{
    TestImportResolver *resolver = (TestImportResolver *)context;

    if (resolver == NULL || out_target == NULL ||
        thunk_index >= (size_t)TEST_REV1655_THUNK_COUNT ||
        thunk_index == resolver->fail_index ||
        library != resolver->libraries[thunk_index] ||
        ordinal != resolver->ordinals[thunk_index]) {
        return 0;
    }
    ++resolver->calls;
    *out_target = resolver->targets[thunk_index];
    return 1;
}

static int prepare_loaded_image_and_resolver(uint8_t *image)
{
    uint8_t *thunks;
    size_t index;

    if (image == NULL) {
        return 0;
    }
    memset(&g_test_import_resolver, 0, sizeof(g_test_import_resolver));
    g_test_import_resolver.fail_index =
        (size_t)TEST_REV1655_THUNK_COUNT;
    thunks = image + TEST_REV1655_TEXT_RVA + TEST_REV1655_THUNK_OFFSET;

    for (index = 0u;
         index < (size_t)TEST_REV1655_THUNK_COUNT;
         ++index) {
        uint8_t *thunk = thunks + index * (size_t)TEST_REV1655_THUNK_SIZE;
        AzRev1655ImportDescriptor descriptor;
        uint32_t identity;
        const uint32_t target = 0x81004000u +
            (uint32_t)index * 0x00000104u;
        const uint32_t low = target & 0xFFFFu;
        const uint32_t adjusted_high =
            ((target >> 16u) + (low >= 0x8000u ? 1u : 0u)) & 0xFFFFu;

        if (!az_rev1655_hook_gate_import_descriptor(index, &descriptor)) {
            return 0;
        }
        identity = ((uint32_t)descriptor.library << 16u) |
            (uint32_t)descriptor.ordinal;

        if (read_u32_be(thunk) != (0x01000000u | identity) ||
            read_u32_be(thunk + 4u) != (0x02000000u | identity) ||
            read_u32_be(thunk + 8u) != 0x7D6903A6u ||
            read_u32_be(thunk + 12u) != 0x4E800420u) {
            return 0;
        }

        g_test_import_resolver.libraries[index] = descriptor.library;
        g_test_import_resolver.ordinals[index] = descriptor.ordinal;
        g_test_import_resolver.targets[index] = target;
        write_u32_be(thunk, 0x3D600000u | adjusted_high);
        write_u32_be(thunk + 4u, 0x396B0000u | low);
    }

    g_test_import_resolver_api.resolve = &test_resolve_import;
    g_test_import_resolver_api.context = &g_test_import_resolver;
    return 1;
}

static void fill_provenance(AzRev1655FilterProvenance *provenance)
{
    az_rev1655_filter_consumer_exact_provenance(provenance);
    CHECK(memcmp(provenance->aurora_xex_sha256,
        k_xex_sha256, sizeof(k_xex_sha256)) == 0);
    CHECK(memcmp(provenance->extracted_pe_sha256,
        k_pe_sha256, sizeof(k_pe_sha256)) == 0);
}

static void test_host_init(TestHost *host)
{
    memset(host, 0, sizeof(*host));
    host->registry = 0xA55AA55Au;
    host->current_thread_id = TEST_WORKER_THREAD;
    host->affinity_verified = 1u;
    host->coverflow_interactive = 1u;
    host->queue_idle = 1u;
    host->addresses_valid = 1u;
    host->take_result = AZ_INPUT_DETOUR_NO_FILTER;
    host->requested_index = AZ_NO_GLYPH;
    host->schedule_result = 0;
    host->registry_failure_index = -1;
    host->initial_quickview_id = 1u;
    host->initial_include_hidden = 1u;
    host->initial_favorites_only = 1u;
    host->initial_sort_behavior = 0x10000u;
    host->initial_content_context_id = 0x1234u;
    host->initial_sort_method = "Title Name";
    host->initial_compiled = "return true";
    host->initial_search = "galaxy";
    host->initial_context0 = "context-zero";
    host->initial_context1 = "context-one";
}

static TestStringRecord *find_string_record(
    TestHost *host,
    const AzRev1655AuroraString *owner)
{
    uint32_t index;

    for (index = 0u; index < TEST_MAX_STRINGS; ++index) {
        if (host->strings[index].active != 0u &&
            host->strings[index].owner == owner) {
            return &host->strings[index];
        }
    }
    return NULL;
}

static TestStringRecord *add_string_record(
    TestHost *host,
    AzRev1655AuroraString *owner,
    AzRev1655ActiveAggregateD0 *aggregate_owner,
    const char *text)
{
    uint32_t index;
    size_t length;

    if (text == NULL) {
        return NULL;
    }
    length = strlen(text);
    if (length > AZ_REV1655_FILTER_MAX_TEXT) {
        return NULL;
    }

    for (index = 0u; index < TEST_MAX_STRINGS; ++index) {
        TestStringRecord *record = &host->strings[index];
        if (record->active == 0u) {
            memset(record, 0, sizeof(*record));
            record->owner = owner;
            record->aggregate_owner = aggregate_owner;
            memcpy(record->text, text, length + 1u);
            record->length = (uint32_t)length;
            record->capacity = length < 0x10u ? 0x0Fu : (uint32_t)length;
            record->active = 1u;
            return record;
        }
    }
    return NULL;
}

static TestVectorRecord *find_vector_record(
    TestHost *host,
    const AzRev1655AuroraStringVector *owner)
{
    uint32_t index;

    for (index = 0u; index < TEST_MAX_VECTORS; ++index) {
        if (host->vectors[index].active != 0u &&
            host->vectors[index].owner == owner) {
            return &host->vectors[index];
        }
    }
    return NULL;
}

static TestVectorRecord *add_vector_record(
    TestHost *host,
    AzRev1655AuroraStringVector *owner,
    AzRev1655ActiveAggregateD0 *aggregate_owner)
{
    uint32_t index;

    for (index = 0u; index < TEST_MAX_VECTORS; ++index) {
        TestVectorRecord *record = &host->vectors[index];
        if (record->active == 0u) {
            memset(record, 0, sizeof(*record));
            record->owner = owner;
            record->aggregate_owner = aggregate_owner;
            record->active = 1u;
            return record;
        }
    }
    return NULL;
}

static uint32_t active_string_count(const TestHost *host)
{
    uint32_t index;
    uint32_t count = 0u;

    for (index = 0u; index < TEST_MAX_STRINGS; ++index) {
        if (host->strings[index].active != 0u) {
            ++count;
        }
    }
    return count;
}

static uint32_t test_current_thread_id(void *context)
{
    return ((TestHost *)context)->current_thread_id;
}

static uint8_t test_worker_affinity_verified(
    void *context,
    uint32_t worker_thread_id)
{
    TestHost *host = (TestHost *)context;
    return (host->affinity_verified != 0u &&
        worker_thread_id == TEST_WORKER_THREAD) ? 1u : 0u;
}

static uint8_t test_coverflow_is_interactive(void *context)
{
    return ((TestHost *)context)->coverflow_interactive;
}

static uint8_t test_filter_queue_is_idle(void *context)
{
    return ((TestHost *)context)->queue_idle;
}

static uint8_t test_address_range_is_valid(
    void *context,
    const void *address,
    size_t size)
{
    TestHost *host = (TestHost *)context;
    return host->addresses_valid != 0u && address != NULL && size != 0u ?
        1u : 0u;
}

static AzInputDetourResult test_take_filter_request(
    void *context,
    uint8_t *filter_index)
{
    TestHost *host = (TestHost *)context;
    ++host->take_calls;
    if (host->take_result == AZ_INPUT_DETOUR_OK && filter_index != NULL) {
        *filter_index = host->requested_index;
    }
    return host->take_result;
}

static void test_finish_filter_request(void *context)
{
    ++((TestHost *)context)->finish_calls;
}

static void *test_gcm_singleton(void *context)
{
    return ((TestHost *)context)->gcm;
}

static void *test_registry_singleton(void *context)
{
    return &((TestHost *)context)->registry;
}

static uint8_t test_registry_lookup(
    void *context,
    void *registry,
    uint32_t registry_type,
    const char *identifier)
{
    TestHost *host = (TestHost *)context;
    const uint8_t index = az_filter_index_for_method(identifier);

    ++host->registry_calls;
    if (registry != &host->registry || registry_type != 0u ||
        index == AZ_NO_GLYPH ||
        (int32_t)index == host->registry_failure_index) {
        return 0u;
    }
    return 1u;
}

static uint8_t test_copy_active_aggregate(
    void *context,
    AzRev1655ActiveAggregateD0 *destination,
    const void *gcm_plus_60,
    uint32_t staging_selector)
{
    TestHost *host = (TestHost *)context;
    TestVectorRecord *vector;
    uint32_t index;

    ++host->copy_calls;
    if (host->fail_copy != 0u || destination == NULL ||
        gcm_plus_60 != host->gcm + 0x60u || staging_selector != 0u) {
        return 0u;
    }

    memset(destination, 0, sizeof(*destination));
    destination->work.include_hidden = host->initial_include_hidden;
    destination->work.favorites_only = host->initial_favorites_only;
    destination->work.sort_behavior = host->initial_sort_behavior;
    destination->work.content_context_id = host->initial_content_context_id;
    destination->work.quickview_id = host->initial_quickview_id;
    if (add_string_record(host, &destination->work.sort_method,
            destination, host->initial_sort_method) == NULL ||
        add_string_record(host, &destination->work.compiled_quickview_filter,
            destination, host->initial_compiled) == NULL ||
        add_string_record(host, &destination->work.search_text,
            destination, host->initial_search) == NULL ||
        add_string_record(host, &destination->context.values[0],
            destination, host->initial_context0) == NULL ||
        add_string_record(host, &destination->context.values[1],
            destination, host->initial_context1) == NULL) {
        return 0u;
    }

    vector = add_vector_record(
        host, &destination->work.additional_filter_ids, destination);
    if (vector == NULL || host->initial_raw_count > TEST_MAX_RAW_FILTERS) {
        return 0u;
    }
    for (index = 0u; index < host->initial_raw_count; ++index) {
        if (add_string_record(host, &vector->elements[index], destination,
                host->initial_raw[index]) == NULL) {
            return 0u;
        }
        ++vector->count;
    }
    if (host->malformed_string != 0u) {
        TestStringRecord *record = find_string_record(
            host, &destination->work.compiled_quickview_filter);
        if (record != NULL) {
            record->capacity = 0u;
        }
    }
    return 1u;
}

static void test_destroy_active_aggregate(
    void *context,
    AzRev1655ActiveAggregateD0 *aggregate)
{
    TestHost *host = (TestHost *)context;
    uint32_t index;

    ++host->destroy_calls;
    for (index = 0u; index < TEST_MAX_STRINGS; ++index) {
        if (host->strings[index].active != 0u &&
            host->strings[index].aggregate_owner == aggregate) {
            host->strings[index].active = 0u;
        }
    }
    for (index = 0u; index < TEST_MAX_VECTORS; ++index) {
        if (host->vectors[index].active != 0u &&
            host->vectors[index].aggregate_owner == aggregate) {
            host->vectors[index].active = 0u;
        }
    }
}

static uint8_t test_string_view(
    void *context,
    const AzRev1655AuroraString *value,
    const char **characters,
    uint32_t *length,
    uint32_t *capacity)
{
    TestStringRecord *record = find_string_record((TestHost *)context, value);
    if (record == NULL || characters == NULL || length == NULL ||
        capacity == NULL) {
        return 0u;
    }
    *characters = record->text;
    *length = record->length;
    *capacity = record->capacity;
    return 1u;
}

static uint8_t test_string_construct_cstring(
    void *context,
    AzRev1655AuroraString *destination,
    const char *source)
{
    TestHost *host = (TestHost *)context;
    ++host->construct_calls;
    if (host->fail_construct != 0u) {
        return 0u;
    }
    return add_string_record(host, destination, NULL, source) != NULL ? 1u : 0u;
}

static uint8_t test_string_assign_bytes(
    void *context,
    AzRev1655AuroraString *destination,
    const char *source,
    uint32_t length)
{
    TestHost *host = (TestHost *)context;
    TestStringRecord *record = find_string_record(host, destination);

    ++host->assign_calls;
    if (host->fail_assign != 0u || record == NULL || source == NULL ||
        length > AZ_REV1655_FILTER_MAX_TEXT) {
        return 0u;
    }
    memcpy(record->text, source, length);
    record->text[length] = '\0';
    record->length = length;
    record->capacity = length < 0x10u ? 0x0Fu : length;
    return 1u;
}

static void test_string_destroy(
    void *context,
    AzRev1655AuroraString *value)
{
    TestStringRecord *record = find_string_record((TestHost *)context, value);
    if (record != NULL) {
        record->active = 0u;
    }
}

static uint8_t test_vector_count(
    void *context,
    const AzRev1655AuroraStringVector *vector,
    uint32_t *count)
{
    TestVectorRecord *record = find_vector_record((TestHost *)context, vector);
    if (record == NULL || count == NULL) {
        return 0u;
    }
    *count = record->count;
    return 1u;
}

static AzRev1655AuroraString *test_vector_at(
    void *context,
    AzRev1655AuroraStringVector *vector,
    uint32_t index)
{
    TestVectorRecord *record = find_vector_record((TestHost *)context, vector);
    if (record == NULL || index >= record->count) {
        return NULL;
    }
    return &record->elements[index];
}

static uint8_t test_vector_push_back(
    void *context,
    AzRev1655AuroraStringVector *vector,
    const AzRev1655AuroraString *value)
{
    TestHost *host = (TestHost *)context;
    TestVectorRecord *vector_record = find_vector_record(host, vector);
    TestStringRecord *source_record = find_string_record(host, value);
    AzRev1655AuroraString *destination;

    ++host->push_calls;
    if (host->fail_push != 0u || vector_record == NULL ||
        source_record == NULL || vector_record->count >= TEST_MAX_RAW_FILTERS) {
        return 0u;
    }
    destination = &vector_record->elements[vector_record->count];
    if (add_string_record(host, destination,
            vector_record->aggregate_owner, source_record->text) == NULL) {
        return 0u;
    }
    ++vector_record->count;
    return 1u;
}

static uint8_t test_vector_erase(
    void *context,
    AzRev1655AuroraStringVector *vector,
    uint32_t index)
{
    TestHost *host = (TestHost *)context;
    TestVectorRecord *record = find_vector_record(host, vector);
    uint32_t current;

    ++host->erase_calls;
    if (record == NULL || index >= record->count) {
        return 0u;
    }
    for (current = index; current + 1u < record->count; ++current) {
        TestStringRecord *destination = find_string_record(
            host, &record->elements[current]);
        TestStringRecord *source = find_string_record(
            host, &record->elements[current + 1u]);
        if (destination == NULL || source == NULL) {
            return 0u;
        }
        memcpy(destination->text, source->text, source->length + 1u);
        destination->length = source->length;
        destination->capacity = source->capacity;
    }
    {
        TestStringRecord *last = find_string_record(
            host, &record->elements[record->count - 1u]);
        if (last == NULL) {
            return 0u;
        }
        last->active = 0u;
    }
    --record->count;
    return 1u;
}

static void capture_string(
    TestHost *host,
    const AzRev1655AuroraString *source,
    char *destination,
    size_t capacity)
{
    TestStringRecord *record = find_string_record(host, source);
    size_t length;

    CHECK(record != NULL);
    if (record == NULL || capacity == 0u) {
        return;
    }
    length = record->length;
    if (length >= capacity) {
        length = capacity - 1u;
    }
    memcpy(destination, record->text, length);
    destination[length] = '\0';
}

static int32_t test_schedule_filter(
    void *context,
    void *gcm_plus_8,
    const AzRev1655FilterContext38 *filter_context,
    const AzRev1655FilterWork74 *work,
    uint32_t flags)
{
    TestHost *host = (TestHost *)context;
    TestVectorRecord *vector;
    uint32_t index;

    ++host->schedule_calls;
    CHECK(gcm_plus_8 == host->gcm + 8u);
    CHECK(filter_context != NULL);
    CHECK(work != NULL);
    memset(&host->scheduled, 0, sizeof(host->scheduled));
    host->scheduled.flags = flags;
    host->scheduled.quickview_id = work->quickview_id;
    host->scheduled.include_hidden = work->include_hidden;
    host->scheduled.favorites_only = work->favorites_only;
    host->scheduled.sort_behavior = work->sort_behavior;
    host->scheduled.content_context_id = work->content_context_id;
    capture_string(host, &work->sort_method,
        host->scheduled.sort_method, sizeof(host->scheduled.sort_method));
    capture_string(host, &work->compiled_quickview_filter,
        host->scheduled.compiled, sizeof(host->scheduled.compiled));
    capture_string(host, &work->search_text,
        host->scheduled.search, sizeof(host->scheduled.search));
    capture_string(host, &filter_context->values[0],
        host->scheduled.context0, sizeof(host->scheduled.context0));
    capture_string(host, &filter_context->values[1],
        host->scheduled.context1, sizeof(host->scheduled.context1));

    vector = find_vector_record(host, &work->additional_filter_ids);
    CHECK(vector != NULL);
    if (vector != NULL) {
        host->scheduled.raw_count = vector->count;
        for (index = 0u; index < vector->count; ++index) {
            capture_string(host, &vector->elements[index],
                host->scheduled.raw[index],
                sizeof(host->scheduled.raw[index]));
        }
    }
    return host->schedule_result;
}

static void make_host_ops(TestHost *host, AzRev1655FilterHostOps *ops)
{
    memset(ops, 0, sizeof(*ops));
    ops->context = host;
    az_rev1655_filter_consumer_exact_entrypoints(&ops->entrypoints);
    ops->current_thread_id = &test_current_thread_id;
    ops->worker_affinity_verified = &test_worker_affinity_verified;
    ops->coverflow_is_interactive = &test_coverflow_is_interactive;
    ops->filter_queue_is_demonstrably_idle = &test_filter_queue_is_idle;
    ops->address_range_is_valid = &test_address_range_is_valid;
    ops->take_filter_request = &test_take_filter_request;
    ops->finish_filter_request = &test_finish_filter_request;
    ops->gcm_singleton = &test_gcm_singleton;
    ops->registry_singleton = &test_registry_singleton;
    ops->registry_lookup = &test_registry_lookup;
    ops->copy_active_aggregate = &test_copy_active_aggregate;
    ops->destroy_active_aggregate = &test_destroy_active_aggregate;
    ops->string_view = &test_string_view;
    ops->string_construct_cstring = &test_string_construct_cstring;
    ops->string_assign_bytes = &test_string_assign_bytes;
    ops->string_destroy = &test_string_destroy;
    ops->vector_count = &test_vector_count;
    ops->vector_at = &test_vector_at;
    ops->vector_push_back = &test_vector_push_back;
    ops->vector_erase = &test_vector_erase;
    ops->schedule_filter = &test_schedule_filter;
}

static AzRev1655FilterConsumerResult bind_consumer(
    AzRev1655FilterConsumer *consumer,
    TestHost *host,
    const AzRev1655LoadedImage *image)
{
    AzRev1655FilterProvenance provenance;
    AzRev1655FilterHostOps ops;

    fill_provenance(&provenance);
    make_host_ops(host, &ops);
    return az_rev1655_filter_consumer_bind_with_import_resolver(
        consumer,
        image,
        &g_test_import_resolver_api,
        &provenance,
        TEST_WORKER_THREAD,
        &ops);
}

static void bind_and_probe(
    AzRev1655FilterConsumer *consumer,
    TestHost *host,
    const AzRev1655LoadedImage *image)
{
    CHECK(bind_consumer(consumer, host, image) ==
        AZ_REV1655_FILTER_CONSUMER_IDLE);
    CHECK(az_rev1655_filter_consumer_worker_probe(consumer) ==
        AZ_REV1655_FILTER_CONSUMER_IDLE);
    CHECK(active_string_count(host) == 0u);
}

static void assert_preserved_state(const TestHost *host)
{
    CHECK(host->scheduled.flags == AZ_REV1655_FILTER_FLAGS_ADDITIONAL);
    CHECK(host->scheduled.quickview_id == host->initial_quickview_id);
    CHECK(host->scheduled.include_hidden == host->initial_include_hidden);
    CHECK(host->scheduled.favorites_only == host->initial_favorites_only);
    CHECK(host->scheduled.sort_behavior == host->initial_sort_behavior);
    CHECK(host->scheduled.content_context_id == host->initial_content_context_id);
    CHECK(strcmp(host->scheduled.sort_method, host->initial_sort_method) == 0);
    CHECK(strcmp(host->scheduled.search, host->initial_search) == 0);
    CHECK(strcmp(host->scheduled.context0, host->initial_context0) == 0);
    CHECK(strcmp(host->scheduled.context1, host->initial_context1) == 0);
}

static void test_binding_and_exact_gates(const AzRev1655LoadedImage *image)
{
    TestHost host;
    AzRev1655FilterConsumer consumer;
    AzRev1655FilterHostOps ops;
    AzRev1655FilterProvenance provenance;
    AzRev1655LoadedImage wrong_image;
    const AzRev1655HookPermit *permit = NULL;
    TestImportResolver bad_resolver_context;
    AzRev1655ImportResolver bad_resolver;

    test_host_init(&host);
    fill_provenance(&provenance);
    make_host_ops(&host, &ops);
    CHECK(az_rev1655_filter_consumer_bind_with_import_resolver(
        &consumer, image, &g_test_import_resolver_api, &provenance,
        TEST_WORKER_THREAD, &ops) ==
        AZ_REV1655_FILTER_CONSUMER_IDLE);
    CHECK(az_rev1655_hook_gate_validate_with_import_resolver(
        image, &g_test_import_resolver_api, &permit) ==
        AZ_REV1655_HOOK_GATE_OK);
    CHECK(permit != NULL);
    CHECK(az_rev1655_filter_consumer_bind_with_validated_permit(
        &consumer, image, permit, &provenance,
        TEST_WORKER_THREAD, &ops) ==
        AZ_REV1655_FILTER_CONSUMER_IDLE);

    /* The legacy API and every missing/untrusted resolver path fail closed. */
    CHECK(az_rev1655_filter_consumer_bind(
        &consumer, image, &provenance, TEST_WORKER_THREAD, &ops) ==
        AZ_REV1655_FILTER_CONSUMER_BAD_IMAGE);
    CHECK(consumer.bound == 0u);
    CHECK(az_rev1655_filter_consumer_bind_with_import_resolver(
        &consumer, image, NULL, &provenance, TEST_WORKER_THREAD, &ops) ==
        AZ_REV1655_FILTER_CONSUMER_BAD_IMAGE);
    CHECK(consumer.bound == 0u);
    bad_resolver = g_test_import_resolver_api;
    bad_resolver.resolve = NULL;
    CHECK(az_rev1655_filter_consumer_bind_with_import_resolver(
        &consumer, image, &bad_resolver, &provenance,
        TEST_WORKER_THREAD, &ops) ==
        AZ_REV1655_FILTER_CONSUMER_BAD_IMAGE);
    CHECK(consumer.bound == 0u);

    bad_resolver_context = g_test_import_resolver;
    bad_resolver_context.fail_index = 17u;
    bad_resolver.resolve = &test_resolve_import;
    bad_resolver.context = &bad_resolver_context;
    CHECK(az_rev1655_filter_consumer_bind_with_import_resolver(
        &consumer, image, &bad_resolver, &provenance,
        TEST_WORKER_THREAD, &ops) ==
        AZ_REV1655_FILTER_CONSUMER_BAD_IMAGE);
    CHECK(consumer.bound == 0u);
    bad_resolver_context.fail_index =
        (size_t)TEST_REV1655_THUNK_COUNT;
    bad_resolver_context.targets[23] += 4u;
    CHECK(az_rev1655_filter_consumer_bind_with_import_resolver(
        &consumer, image, &bad_resolver, &provenance,
        TEST_WORKER_THREAD, &ops) ==
        AZ_REV1655_FILTER_CONSUMER_BAD_IMAGE);
    CHECK(consumer.bound == 0u);

    provenance.aurora_xex_sha256[0] ^= 1u;
    CHECK(az_rev1655_filter_consumer_bind_with_import_resolver(
        &consumer, image, &g_test_import_resolver_api, &provenance,
        TEST_WORKER_THREAD, &ops) ==
        AZ_REV1655_FILTER_CONSUMER_BAD_PROVENANCE);
    provenance.aurora_xex_sha256[0] ^= 1u;

    wrong_image = *image;
    wrong_image.virtual_address += 4u;
    CHECK(az_rev1655_filter_consumer_bind_with_validated_permit(
        &consumer, &wrong_image, permit, &provenance,
        TEST_WORKER_THREAD, &ops) ==
        AZ_REV1655_FILTER_CONSUMER_BAD_IMAGE);
    CHECK(az_rev1655_filter_consumer_bind_with_import_resolver(
        &consumer, &wrong_image, &g_test_import_resolver_api, &provenance,
        TEST_WORKER_THREAD, &ops) ==
        AZ_REV1655_FILTER_CONSUMER_BAD_IMAGE);

    ++ops.entrypoints.scheduler;
    CHECK(az_rev1655_filter_consumer_bind_with_import_resolver(
        &consumer, image, &g_test_import_resolver_api, &provenance,
        TEST_WORKER_THREAD, &ops) ==
        AZ_REV1655_FILTER_CONSUMER_BAD_BINDINGS);
    --ops.entrypoints.scheduler;
    ops.registry_lookup = NULL;
    CHECK(az_rev1655_filter_consumer_bind_with_import_resolver(
        &consumer, image, &g_test_import_resolver_api, &provenance,
        TEST_WORKER_THREAD, &ops) ==
        AZ_REV1655_FILTER_CONSUMER_BAD_BINDINGS);

    CHECK(az_rev1655_filter_consumer_bind_with_import_resolver(
        NULL, image, &g_test_import_resolver_api, &provenance,
        TEST_WORKER_THREAD, &ops) ==
        AZ_REV1655_FILTER_CONSUMER_NULL_ARGUMENT);
    CHECK(az_rev1655_filter_consumer_bind_with_import_resolver(
        &consumer, image, &g_test_import_resolver_api, &provenance,
        0u, &ops) ==
        AZ_REV1655_FILTER_CONSUMER_NULL_ARGUMENT);
}

static void test_all_canonical_methods(const AzRev1655LoadedImage *image)
{
    TestHost host;
    AzRev1655FilterConsumer consumer;
    uint8_t index;

    test_host_init(&host);
    bind_and_probe(&consumer, &host, image);
    host.take_result = AZ_INPUT_DETOUR_OK;

    for (index = AZ_FILTER_OTHER_INDEX;
         index < AZ_GLYPH_COUNT;
         ++index) {
        host.requested_index = index;
        CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
            AZ_REV1655_FILTER_CONSUMER_SCHEDULED);
        CHECK(host.scheduled.raw_count == 1u);
        CHECK(strcmp(host.scheduled.raw[0],
            az_filter_method_for_index(index)) == 0);
        CHECK(host.scheduled.flags == AZ_REV1655_FILTER_FLAGS_ADDITIONAL);
        CHECK(active_string_count(&host) == 0u);
    }
    CHECK(host.schedule_calls == AZ_GLYPH_COUNT - 1u);
    CHECK(host.finish_calls == AZ_GLYPH_COUNT - 1u);
}

static void test_worker_probe(const AzRev1655LoadedImage *image)
{
    TestHost host;
    AzRev1655FilterConsumer consumer;
    AzRev1655FilterConsumerStatus status;

    test_host_init(&host);
    CHECK(bind_consumer(&consumer, &host, image) ==
        AZ_REV1655_FILTER_CONSUMER_IDLE);

    host.current_thread_id = TEST_WORKER_THREAD + 1u;
    CHECK(az_rev1655_filter_consumer_worker_probe(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_NOT_WORKER);
    CHECK(host.registry_calls == 0u);
    host.current_thread_id = TEST_WORKER_THREAD;
    host.affinity_verified = 0u;
    CHECK(az_rev1655_filter_consumer_worker_probe(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_DEFERRED);
    CHECK(host.copy_calls == 0u);
    host.affinity_verified = 1u;
    CHECK(az_rev1655_filter_consumer_worker_probe(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_IDLE);
    CHECK(host.registry_calls == AZ_GLYPH_COUNT - 1u);
    CHECK(host.copy_calls == 1u);
    CHECK(host.destroy_calls == 1u);
    CHECK(active_string_count(&host) == 0u);

    az_rev1655_filter_consumer_snapshot_status(&consumer, &status);
    CHECK(status.bound == 1u);
    CHECK(status.runtime_verified == 1u);
    CHECK(status.probe_count == 1u);
    CHECK(status.deferred_count == 1u);
}

static void test_probe_fail_closed(const AzRev1655LoadedImage *image)
{
    TestHost host;
    AzRev1655FilterConsumer consumer;

    test_host_init(&host);
    host.registry_failure_index = 13;
    CHECK(bind_consumer(&consumer, &host, image) ==
        AZ_REV1655_FILTER_CONSUMER_IDLE);
    CHECK(az_rev1655_filter_consumer_worker_probe(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_REGISTRY_MISSING);
    CHECK(consumer.disabled == 1u);

    test_host_init(&host);
    host.initial_quickview_id = 0u;
    CHECK(bind_consumer(&consumer, &host, image) ==
        AZ_REV1655_FILTER_CONSUMER_IDLE);
    CHECK(az_rev1655_filter_consumer_worker_probe(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_SNAPSHOT_INVALID);
    CHECK(host.destroy_calls == 1u);
    CHECK(active_string_count(&host) == 0u);

    test_host_init(&host);
    host.malformed_string = 1u;
    CHECK(bind_consumer(&consumer, &host, image) ==
        AZ_REV1655_FILTER_CONSUMER_IDLE);
    CHECK(az_rev1655_filter_consumer_worker_probe(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_SNAPSHOT_INVALID);
    CHECK(active_string_count(&host) == 0u);
}

static void test_append_filter(const AzRev1655LoadedImage *image)
{
    TestHost host;
    AzRev1655FilterConsumer consumer;

    test_host_init(&host);
    host.initial_raw[0] = "ContentType.Xbox360";
    host.initial_raw_count = 1u;
    bind_and_probe(&consumer, &host, image);
    host.take_result = AZ_INPUT_DETOUR_OK;
    host.requested_index = 2u;

    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_SCHEDULED);
    CHECK(host.take_calls == 1u);
    CHECK(host.finish_calls == 1u);
    CHECK(host.construct_calls == 1u);
    CHECK(host.push_calls == 1u);
    CHECK(host.assign_calls == 0u);
    CHECK(host.schedule_calls == 1u);
    CHECK(host.scheduled.raw_count == 2u);
    CHECK(strcmp(host.scheduled.raw[0], "ContentType.Xbox360") == 0);
    CHECK(strcmp(host.scheduled.raw[1], "NameFilter.A - F.A") == 0);
    CHECK(strcmp(host.scheduled.compiled, host.initial_compiled) == 0);
    assert_preserved_state(&host);
    CHECK(active_string_count(&host) == 0u);
}

static void test_replace_raw_filter(const AzRev1655LoadedImage *image)
{
    TestHost host;
    AzRev1655FilterConsumer consumer;

    test_host_init(&host);
    host.initial_raw[0] = "NameFilter.M - R.M";
    host.initial_raw[1] = "ContentType.Xbox360";
    host.initial_raw_count = 2u;
    bind_and_probe(&consumer, &host, image);
    host.take_result = AZ_INPUT_DETOUR_OK;
    host.requested_index = 27u;

    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_SCHEDULED);
    CHECK(host.construct_calls == 0u);
    CHECK(host.push_calls == 0u);
    CHECK(host.assign_calls == 1u);
    CHECK(host.scheduled.raw_count == 2u);
    CHECK(strcmp(host.scheduled.raw[0], "NameFilter.Y - Z.Z") == 0);
    CHECK(strcmp(host.scheduled.raw[1], "ContentType.Xbox360") == 0);
    assert_preserved_state(&host);
    CHECK(active_string_count(&host) == 0u);
}

static void test_replace_compiled_filter(const AzRev1655LoadedImage *image)
{
    TestHost host;
    AzRev1655FilterConsumer consumer;

    test_host_init(&host);
    host.initial_compiled =
        "before and GameListFilterCategories[\"NameFilter\"]"
        "[\"A - F\"][\"A\"](Content) and after";
    bind_and_probe(&consumer, &host, image);
    host.take_result = AZ_INPUT_DETOUR_OK;
    host.requested_index = 1u;

    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_SCHEDULED);
    CHECK(host.assign_calls == 1u);
    CHECK(host.push_calls == 0u);
    CHECK(strcmp(host.scheduled.compiled,
        "before and GameListFilterCategories[\"NameFilter\"]"
        "[\"Other\"](Content) and after") == 0);
    CHECK(host.scheduled.raw_count == 0u);
    assert_preserved_state(&host);
    CHECK(active_string_count(&host) == 0u);
}

static void test_alphabetical_all_preserves_quickview(
    const AzRev1655LoadedImage *image)
{
    TestHost host;
    AzRev1655FilterConsumer consumer;

    test_host_init(&host);
    host.initial_quickview_id = 17u;
    host.initial_raw[0] = "NameFilter.A - F.A";
    host.initial_raw[1] = "ContentType.XBLA";
    host.initial_raw_count = 2u;
    bind_and_probe(&consumer, &host, image);
    host.take_result = AZ_INPUT_DETOUR_OK;
    host.requested_index = AZ_FILTER_ALL_INDEX;

    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_SCHEDULED);
    CHECK(host.erase_calls == 1u);
    CHECK(host.assign_calls == 0u);
    CHECK(host.scheduled.raw_count == 1u);
    CHECK(strcmp(host.scheduled.raw[0], "ContentType.XBLA") == 0);
    CHECK(strcmp(host.scheduled.compiled, host.initial_compiled) == 0);
    assert_preserved_state(&host);
    CHECK(active_string_count(&host) == 0u);

    test_host_init(&host);
    host.initial_quickview_id = 23u;
    host.initial_compiled =
        "ContentType.XBLA(Content) and "
        "GameListFilterCategories[\"NameFilter\"]"
        "[\"M - R\"][\"M\"](Content) and Favorites(Content)";
    bind_and_probe(&consumer, &host, image);
    host.take_result = AZ_INPUT_DETOUR_OK;
    host.requested_index = AZ_FILTER_ALL_INDEX;

    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_SCHEDULED);
    CHECK(host.erase_calls == 0u);
    CHECK(host.assign_calls == 1u);
    CHECK(strcmp(host.scheduled.compiled,
        "ContentType.XBLA(Content) and Favorites(Content)") == 0);
    CHECK(host.scheduled.raw_count == 0u);
    assert_preserved_state(&host);
    CHECK(active_string_count(&host) == 0u);

    test_host_init(&host);
    host.initial_quickview_id = 29u;
    host.initial_raw[0] = "ContentType.XBLA";
    host.initial_raw_count = 1u;
    bind_and_probe(&consumer, &host, image);
    host.take_result = AZ_INPUT_DETOUR_OK;
    host.requested_index = AZ_FILTER_ALL_INDEX;

    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_SCHEDULED);
    CHECK(host.erase_calls == 0u);
    CHECK(host.assign_calls == 0u);
    CHECK(host.scheduled.raw_count == 1u);
    CHECK(strcmp(host.scheduled.raw[0], "ContentType.XBLA") == 0);
    assert_preserved_state(&host);
    CHECK(active_string_count(&host) == 0u);
}

static void run_ambiguous_case(
    const AzRev1655LoadedImage *image,
    const char *compiled,
    const char *raw0,
    const char *raw1)
{
    TestHost host;
    AzRev1655FilterConsumer consumer;

    test_host_init(&host);
    host.initial_compiled = compiled;
    if (raw0 != NULL) {
        host.initial_raw[host.initial_raw_count++] = raw0;
    }
    if (raw1 != NULL) {
        host.initial_raw[host.initial_raw_count++] = raw1;
    }
    bind_and_probe(&consumer, &host, image);
    host.take_result = AZ_INPUT_DETOUR_OK;
    host.requested_index = 2u;
    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_AMBIGUOUS_NAME_FILTER);
    CHECK(host.schedule_calls == 0u);
    CHECK(host.finish_calls == 1u);
    CHECK(host.destroy_calls == 2u);
    CHECK(consumer.disabled == 0u);
    CHECK(active_string_count(&host) == 0u);
}

static void test_ambiguous_filters(const AzRev1655LoadedImage *image)
{
    run_ambiguous_case(image, "return true",
        "NameFilter.A - F.A", "NameFilter.A - F.B");
    run_ambiguous_case(image,
        "GameListFilterCategories[\"NameFilter\"][\"Other\"](Content)",
        "NameFilter.A - F.A", NULL);
    run_ambiguous_case(image, "return true", "NameFilter.Bad", NULL);
    run_ambiguous_case(image,
        "GameListFilterCategories[\"NameFilter\"][\"Bogus\"](Content)",
        NULL, NULL);
    run_ambiguous_case(image,
        "GameListFilterCategories[\"NameFilter\"][\"Other\"](Content) and "
        "GameListFilterCategories[\"NameFilter\"][\"A - F\"]"
        "[\"A\"](Content)",
        NULL, NULL);
    run_ambiguous_case(image,
        "\"GameListFilterCategories[\"NameFilter\"]"
        "[\"Other\"](Content)\"",
        NULL, NULL);
}

static void test_defer_worker_only_and_cancel(
    const AzRev1655LoadedImage *image)
{
    TestHost host;
    AzRev1655FilterConsumer consumer;

    test_host_init(&host);
    bind_and_probe(&consumer, &host, image);
    host.take_result = AZ_INPUT_DETOUR_OK;
    host.requested_index = 3u;
    host.queue_idle = 0u;

    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_DEFERRED);
    CHECK(host.take_calls == 1u);
    CHECK(host.finish_calls == 0u);
    CHECK(consumer.request_held == 1u);
    host.current_thread_id = TEST_WORKER_THREAD + 1u;
    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_NOT_WORKER);
    CHECK(host.take_calls == 1u);
    CHECK(host.finish_calls == 0u);
    CHECK(az_rev1655_filter_consumer_worker_cancel(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_NOT_WORKER);
    host.current_thread_id = TEST_WORKER_THREAD;
    CHECK(az_rev1655_filter_consumer_worker_cancel(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_CANCELLED);
    CHECK(host.finish_calls == 1u);
    CHECK(consumer.request_held == 0u);

    host.requested_index = 4u;
    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_DEFERRED);
    CHECK(host.take_calls == 2u);
    host.queue_idle = 1u;
    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_SCHEDULED);
    CHECK(host.take_calls == 2u);
    CHECK(host.finish_calls == 2u);
    CHECK(host.schedule_calls == 1u);
}

static void test_failure_cleanup(const AzRev1655LoadedImage *image)
{
    TestHost host;
    AzRev1655FilterConsumer consumer;

    test_host_init(&host);
    bind_and_probe(&consumer, &host, image);
    host.take_result = AZ_INPUT_DETOUR_OK;
    host.requested_index = 5u;
    host.schedule_result = 1;
    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_SCHEDULE_FAILED);
    CHECK(host.finish_calls == 1u);
    CHECK(active_string_count(&host) == 0u);

    test_host_init(&host);
    bind_and_probe(&consumer, &host, image);
    host.take_result = AZ_INPUT_DETOUR_OK;
    host.requested_index = 5u;
    host.fail_push = 1u;
    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_MUTATION_FAILED);
    CHECK(host.finish_calls == 1u);
    CHECK(host.destroy_calls == 2u);
    CHECK(active_string_count(&host) == 0u);

    test_host_init(&host);
    bind_and_probe(&consumer, &host, image);
    host.initial_quickview_id = 0u;
    host.take_result = AZ_INPUT_DETOUR_OK;
    host.requested_index = 5u;
    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_SNAPSHOT_INVALID);
    CHECK(consumer.disabled == 1u);
    CHECK(host.finish_calls == 1u);
    CHECK(active_string_count(&host) == 0u);

    test_host_init(&host);
    bind_and_probe(&consumer, &host, image);
    host.take_result = AZ_INPUT_DETOUR_OK;
    host.requested_index = AZ_GLYPH_COUNT;
    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_BAD_FILTER_INDEX);
    CHECK(consumer.disabled == 1u);
    CHECK(host.finish_calls == 1u);
}

static void test_input_and_result_surface(const AzRev1655LoadedImage *image)
{
    TestHost host;
    AzRev1655FilterConsumer consumer;

    test_host_init(&host);
    memset(&consumer, 0, sizeof(consumer));
    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) !=
        AZ_REV1655_FILTER_CONSUMER_SCHEDULED);
    bind_and_probe(&consumer, &host, image);
    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_IDLE);
    host.take_result = AZ_INPUT_DETOUR_FILTER_BUSY;
    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_INPUT_BUSY);
    host.take_result = AZ_INPUT_DETOUR_NULL;
    CHECK(az_rev1655_filter_consumer_worker_step(&consumer) ==
        AZ_REV1655_FILTER_CONSUMER_INPUT_ERROR);
    CHECK(strcmp(az_rev1655_filter_consumer_result_name(
        AZ_REV1655_FILTER_CONSUMER_SCHEDULED), "scheduled") == 0);
    CHECK(strcmp(az_rev1655_filter_consumer_result_name(
        (AzRev1655FilterConsumerResult)99),
        "unknown-filter-consumer-result") == 0);
}

int main(int argc, char **argv)
{
    uint8_t *image_bytes;
    AzRev1655LoadedImage image;

    if (argc < 2) {
        printf("Rev1655 fixture path not supplied; exact filter-consumer "
            "tests skipped\n");
        return EXIT_SUCCESS;
    }
    image_bytes = load_pe_as_image(argv[1]);
    if (image_bytes == NULL) {
        printf("Rev1655 fixture unavailable at %s; exact "
            "filter-consumer tests skipped\n", argv[1]);
        return EXIT_SUCCESS;
    }
    if (!prepare_loaded_image_and_resolver(image_bytes)) {
        fprintf(stderr, "Rev1655 fixture import table was not canonical\n");
        free(image_bytes);
        return EXIT_FAILURE;
    }
    image.bytes = image_bytes;
    image.size = AZ_REV1655_NT_IMAGE_SIZE;
    image.virtual_address = AZ_REV1655_IMAGE_BASE;

    test_binding_and_exact_gates(&image);
    test_worker_probe(&image);
    test_probe_fail_closed(&image);
    test_all_canonical_methods(&image);
    test_append_filter(&image);
    test_replace_raw_filter(&image);
    test_replace_compiled_filter(&image);
    test_alphabetical_all_preserves_quickview(&image);
    test_ambiguous_filters(&image);
    test_defer_worker_only_and_cancel(&image);
    test_failure_cleanup(&image);
    test_input_and_result_surface(&image);

    free(image_bytes);
    if (failures != 0) {
        fprintf(stderr, "%d filter-consumer assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("Rev1655 fail-closed filter-consumer tests passed");
    return EXIT_SUCCESS;
}
