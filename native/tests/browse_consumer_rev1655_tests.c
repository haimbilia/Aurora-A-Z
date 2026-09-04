#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <auroraaz/browse_consumer_rev1655.h>
#include <auroraaz/filters.h>

#define TEST_ITEM_CAPACITY 8u
#define TEST_OBJECT_BASE 0x00100000u
#define TEST_OBJECT_STRIDE 0x00001000u
#define TEST_NAME_OFFSET 0x35Cu

static int failures = 0;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

typedef struct TestBrowseHost {
    AzRev1655BrowseItem items[TEST_ITEM_CAPACITY];
    const char *names[TEST_ITEM_CAPACITY];
    uint32_t count;
    uint8_t interactive;
    uint8_t addresses_valid;
    uint8_t race_on_second_list;
    uint8_t publish_succeeds;
    AzInputDetourResult take_result;
    uint8_t requested_index;
    uint32_t gcm_marker;
    void *published_gcm;
    uint32_t published_target;
    uint32_t published_count;
    uint32_t take_calls;
    uint32_t finish_calls;
    uint32_t list_calls;
    uint32_t publish_calls;
} TestBrowseHost;

static uint8_t test_interactive(void *context)
{
    const TestBrowseHost *host = (const TestBrowseHost *)context;
    return host->interactive;
}

static uint8_t test_address_range(
    void *context,
    const void *address,
    size_t size)
{
    const TestBrowseHost *host = (const TestBrowseHost *)context;
    (void)size;
    return host->addresses_valid != 0u && address != NULL ? 1u : 0u;
}

static AzInputDetourResult test_take(void *context, uint8_t *index)
{
    TestBrowseHost *host = (TestBrowseHost *)context;
    ++host->take_calls;
    if (host->take_result == AZ_INPUT_DETOUR_OK && index != NULL) {
        *index = host->requested_index;
    }
    return host->take_result;
}

static void test_finish(void *context)
{
    TestBrowseHost *host = (TestBrowseHost *)context;
    ++host->finish_calls;
}

static void *test_gcm(void *context)
{
    TestBrowseHost *host = (TestBrowseHost *)context;
    return &host->gcm_marker;
}

static uint8_t test_active_list(
    void *context,
    AzRev1655BrowseList *list)
{
    TestBrowseHost *host = (TestBrowseHost *)context;
    ++host->list_calls;
    if (list == NULL) {
        return 0u;
    }
    list->begin = host->items;
    list->end = host->items + host->count;
    if (host->race_on_second_list != 0u && host->list_calls == 2u) {
        list->end = host->items + host->count - 1u;
    }
    return 1u;
}

static uint8_t test_string_view(
    void *context,
    const AzRev1655AuroraString *value,
    const char **characters,
    uint32_t *length,
    uint32_t *capacity)
{
    TestBrowseHost *host = (TestBrowseHost *)context;
    uintptr_t address = (uintptr_t)value;
    uint32_t object_address;
    uint32_t slot;
    size_t name_length;

    if (address < TEST_OBJECT_BASE + TEST_NAME_OFFSET ||
        characters == NULL || length == NULL || capacity == NULL) {
        return 0u;
    }
    object_address = (uint32_t)(address - TEST_NAME_OFFSET);
    if (object_address < TEST_OBJECT_BASE ||
        (object_address - TEST_OBJECT_BASE) % TEST_OBJECT_STRIDE != 0u) {
        return 0u;
    }
    slot = (object_address - TEST_OBJECT_BASE) / TEST_OBJECT_STRIDE;
    if (slot >= host->count || host->names[slot] == NULL) {
        return 0u;
    }
    name_length = strlen(host->names[slot]);
    *characters = host->names[slot];
    *length = (uint32_t)name_length;
    *capacity = (uint32_t)name_length;
    return 1u;
}

static uint8_t test_publish(
    void *context,
    void *gcm,
    uint32_t target_index,
    uint32_t item_count)
{
    TestBrowseHost *host = (TestBrowseHost *)context;
    ++host->publish_calls;
    host->published_gcm = gcm;
    host->published_target = target_index;
    host->published_count = item_count;
    return host->publish_succeeds;
}

static void init_host(TestBrowseHost *host)
{
    uint32_t index;

    memset(host, 0, sizeof(*host));
    host->interactive = 1u;
    host->addresses_valid = 1u;
    host->publish_succeeds = 1u;
    host->take_result = AZ_INPUT_DETOUR_OK;
    for (index = 0u; index < TEST_ITEM_CAPACITY; ++index) {
        host->items[index].object_address =
            TEST_OBJECT_BASE + index * TEST_OBJECT_STRIDE;
        host->items[index].owner_address = 0x00200000u + index * 4u;
    }
}

static AzRev1655BrowseResult bind_consumer(
    AzRev1655BrowseConsumer *consumer,
    TestBrowseHost *host)
{
    AzRev1655BrowseHostOps ops;

    memset(&ops, 0, sizeof(ops));
    ops.context = host;
    ops.coverflow_is_interactive = &test_interactive;
    ops.address_range_is_valid = &test_address_range;
    ops.take_request = &test_take;
    ops.finish_request = &test_finish;
    ops.gcm_singleton = &test_gcm;
    ops.active_list = &test_active_list;
    ops.string_view = &test_string_view;
    ops.publish_jump = &test_publish;
    return az_rev1655_browse_consumer_bind(consumer, &ops);
}

static void load_names(TestBrowseHost *host)
{
    host->names[0] = "007 Legends";
    host->names[1] = "Aces of the Galaxy";
    host->names[2] = "borderlands";
    host->names[3] = "Call of Duty";
    host->names[4] = "XBLA Sample";
    host->count = 5u;
}

static void test_alpha_jump_uses_first_match(void)
{
    TestBrowseHost host;
    AzRev1655BrowseConsumer consumer;

    init_host(&host);
    load_names(&host);
    host.requested_index = (uint8_t)(AZ_FILTER_FIRST_ALPHA_INDEX + 1u);
    CHECK(bind_consumer(&consumer, &host) == AZ_REV1655_BROWSE_IDLE);
    CHECK(az_rev1655_browse_consumer_worker_step(&consumer) ==
        AZ_REV1655_BROWSE_JUMP_QUEUED);
    CHECK(host.published_gcm == &host.gcm_marker);
    CHECK(host.published_target == 2u);
    CHECK(host.published_count == 5u);
    CHECK(host.take_calls == 1u);
    CHECK(host.finish_calls == 1u);
}

static void test_other_and_all(void)
{
    TestBrowseHost host;
    AzRev1655BrowseConsumer consumer;

    init_host(&host);
    load_names(&host);
    host.requested_index = AZ_FILTER_OTHER_INDEX;
    CHECK(bind_consumer(&consumer, &host) == AZ_REV1655_BROWSE_IDLE);
    CHECK(az_rev1655_browse_consumer_worker_step(&consumer) ==
        AZ_REV1655_BROWSE_JUMP_QUEUED);
    CHECK(host.published_target == 0u);

    init_host(&host);
    load_names(&host);
    host.requested_index = AZ_FILTER_ALL_INDEX;
    CHECK(bind_consumer(&consumer, &host) == AZ_REV1655_BROWSE_IDLE);
    CHECK(az_rev1655_browse_consumer_worker_step(&consumer) ==
        AZ_REV1655_BROWSE_JUMP_QUEUED);
    CHECK(host.published_target == 0u);
}

static void test_no_match_does_not_move(void)
{
    TestBrowseHost host;
    AzRev1655BrowseConsumer consumer;

    init_host(&host);
    load_names(&host);
    host.requested_index = (uint8_t)(AZ_FILTER_FIRST_ALPHA_INDEX + 25u);
    CHECK(bind_consumer(&consumer, &host) == AZ_REV1655_BROWSE_IDLE);
    CHECK(az_rev1655_browse_consumer_worker_step(&consumer) ==
        AZ_REV1655_BROWSE_NO_MATCH);
    CHECK(host.publish_calls == 0u);
    CHECK(host.finish_calls == 1u);
    CHECK(consumer.no_match_count == 1u);
}

static void test_deferred_request_is_held(void)
{
    TestBrowseHost host;
    AzRev1655BrowseConsumer consumer;

    init_host(&host);
    load_names(&host);
    host.interactive = 0u;
    host.requested_index = AZ_FILTER_OTHER_INDEX;
    CHECK(bind_consumer(&consumer, &host) == AZ_REV1655_BROWSE_IDLE);
    CHECK(az_rev1655_browse_consumer_worker_step(&consumer) ==
        AZ_REV1655_BROWSE_DEFERRED);
    CHECK(host.take_calls == 1u);
    CHECK(host.finish_calls == 0u);

    host.interactive = 1u;
    host.take_result = AZ_INPUT_DETOUR_NO_FILTER;
    CHECK(az_rev1655_browse_consumer_worker_step(&consumer) ==
        AZ_REV1655_BROWSE_JUMP_QUEUED);
    CHECK(host.take_calls == 1u);
    CHECK(host.finish_calls == 1u);
}

static void test_race_and_publish_failure_finish_request(void)
{
    TestBrowseHost host;
    AzRev1655BrowseConsumer consumer;

    init_host(&host);
    load_names(&host);
    host.race_on_second_list = 1u;
    host.requested_index = AZ_FILTER_OTHER_INDEX;
    CHECK(bind_consumer(&consumer, &host) == AZ_REV1655_BROWSE_IDLE);
    CHECK(az_rev1655_browse_consumer_worker_step(&consumer) ==
        AZ_REV1655_BROWSE_RACE);
    CHECK(host.publish_calls == 0u);
    CHECK(host.finish_calls == 1u);

    init_host(&host);
    load_names(&host);
    host.publish_succeeds = 0u;
    host.requested_index = AZ_FILTER_OTHER_INDEX;
    CHECK(bind_consumer(&consumer, &host) == AZ_REV1655_BROWSE_IDLE);
    CHECK(az_rev1655_browse_consumer_worker_step(&consumer) ==
        AZ_REV1655_BROWSE_PUBLISH_FAILED);
    CHECK(host.finish_calls == 1u);
}

static void test_invalid_binding_list_and_cancel(void)
{
    TestBrowseHost host;
    AzRev1655BrowseConsumer consumer;
    AzRev1655BrowseHostOps empty_ops;

    memset(&empty_ops, 0, sizeof(empty_ops));
    CHECK(az_rev1655_browse_consumer_bind(&consumer, &empty_ops) ==
        AZ_REV1655_BROWSE_BAD_BINDINGS);

    init_host(&host);
    load_names(&host);
    host.addresses_valid = 0u;
    host.requested_index = AZ_FILTER_OTHER_INDEX;
    CHECK(bind_consumer(&consumer, &host) == AZ_REV1655_BROWSE_IDLE);
    CHECK(az_rev1655_browse_consumer_worker_step(&consumer) ==
        AZ_REV1655_BROWSE_BAD_LIST);
    CHECK(host.finish_calls == 1u);

    init_host(&host);
    load_names(&host);
    host.interactive = 0u;
    host.requested_index = AZ_FILTER_OTHER_INDEX;
    CHECK(bind_consumer(&consumer, &host) == AZ_REV1655_BROWSE_IDLE);
    CHECK(az_rev1655_browse_consumer_worker_step(&consumer) ==
        AZ_REV1655_BROWSE_DEFERRED);
    CHECK(az_rev1655_browse_consumer_cancel(&consumer) ==
        AZ_REV1655_BROWSE_CANCELLED);
    CHECK(host.finish_calls == 1u);
}

int main(void)
{
    test_alpha_jump_uses_first_match();
    test_other_and_all();
    test_no_match_does_not_move();
    test_deferred_request_is_held();
    test_race_and_publish_failure_finish_request();
    test_invalid_binding_list_and_cancel();

    if (failures != 0) {
        fprintf(stderr, "%d browse consumer assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("AuroraAZ Rev1655 browse consumer tests passed");
    return EXIT_SUCCESS;
}
