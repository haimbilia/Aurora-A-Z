#include <string.h>

#include <auroraaz/browse_consumer_rev1655.h>
#include <auroraaz/filters.h>

#define AZ_REV1655_CONTENT_NAME_OFFSET 0x35Cu
#define AZ_REV1655_CONTENT_NAME_SPAN \
    (AZ_REV1655_CONTENT_NAME_OFFSET + AZ_REV1655_AURORA_STRING_SIZE)

static uint8_t host_is_complete(const AzRev1655BrowseHostOps *host)
{
    return host != NULL &&
        host->coverflow_is_interactive != NULL &&
        host->address_range_is_valid != NULL &&
        host->take_request != NULL &&
        host->finish_request != NULL &&
        host->gcm_singleton != NULL &&
        host->active_list != NULL &&
        host->string_view != NULL &&
        host->publish_jump != NULL ? 1u : 0u;
}

static uint8_t item_matches(
    AzRev1655BrowseConsumer *consumer,
    const AzRev1655BrowseItem *item,
    uint8_t requested_index)
{
    const AzRev1655AuroraString *name;
    const char *characters = NULL;
    uint32_t length = 0u;
    uint32_t capacity = 0u;
    unsigned char first;
    uint8_t alpha_index;

    if (item == NULL || item->object_address == 0u ||
        item->object_address > UINT32_MAX -
            AZ_REV1655_CONTENT_NAME_SPAN ||
        consumer->host.address_range_is_valid(
            consumer->host.context,
            (const void *)(uintptr_t)item->object_address,
            AZ_REV1655_CONTENT_NAME_SPAN) == 0u) {
        return 0u;
    }
    name = (const AzRev1655AuroraString *)(uintptr_t)(
        item->object_address + AZ_REV1655_CONTENT_NAME_OFFSET);
    if (consumer->host.string_view(
            consumer->host.context,
            name,
            &characters,
            &length,
            &capacity) == 0u ||
        capacity < length ||
        (length != 0u && (characters == NULL ||
         consumer->host.address_range_is_valid(
            consumer->host.context, characters, length) == 0u))) {
        return 0u;
    }

    first = length == 0u ? 0u : (unsigned char)characters[0];
    if (first >= (unsigned char)'a' && first <= (unsigned char)'z') {
        first = (unsigned char)(first - (unsigned char)'a' +
            (unsigned char)'A');
    }
    if (first >= (unsigned char)'A' && first <= (unsigned char)'Z') {
        alpha_index = (uint8_t)(
            (uint32_t)AZ_FILTER_FIRST_ALPHA_INDEX +
            (uint32_t)(first - (unsigned char)'A'));
        return requested_index == alpha_index ? 1u : 0u;
    }
    return requested_index == AZ_FILTER_OTHER_INDEX ? 1u : 0u;
}

static uint8_t list_is_valid(
    AzRev1655BrowseConsumer *consumer,
    const AzRev1655BrowseList *list,
    uint32_t *count)
{
    uintptr_t begin;
    uintptr_t end;
    uintptr_t bytes;

    if (consumer == NULL || list == NULL || count == NULL) {
        return 0u;
    }
    begin = (uintptr_t)list->begin;
    end = (uintptr_t)list->end;
    if (begin == 0u || end < begin ||
        (begin & (sizeof(uint32_t) - 1u)) != 0u) {
        return 0u;
    }
    bytes = end - begin;
    if ((bytes % sizeof(AzRev1655BrowseItem)) != 0u ||
        bytes / sizeof(AzRev1655BrowseItem) >
            AZ_REV1655_BROWSE_MAX_ITEMS) {
        return 0u;
    }
    if (bytes != 0u && consumer->host.address_range_is_valid(
            consumer->host.context, list->begin, (size_t)bytes) == 0u) {
        return 0u;
    }
    *count = (uint32_t)(bytes / sizeof(AzRev1655BrowseItem));
    return 1u;
}

AzRev1655BrowseResult az_rev1655_browse_consumer_bind(
    AzRev1655BrowseConsumer *consumer,
    const AzRev1655BrowseHostOps *host)
{
    if (consumer == NULL || host_is_complete(host) == 0u) {
        return AZ_REV1655_BROWSE_BAD_BINDINGS;
    }
    memset(consumer, 0, sizeof(*consumer));
    consumer->host = *host;
    consumer->held_index = AZ_NO_GLYPH;
    consumer->bound = 1u;
    return AZ_REV1655_BROWSE_IDLE;
}

AzRev1655BrowseResult az_rev1655_browse_consumer_worker_step(
    AzRev1655BrowseConsumer *consumer)
{
    AzRev1655BrowseList before;
    AzRev1655BrowseList after;
    AzInputDetourResult input_result;
    void *gcm;
    uint32_t count;
    uint32_t index;
    uint32_t target = UINT32_MAX;
    AzRev1655BrowseResult result;

    if (consumer == NULL || consumer->bound == 0u ||
        consumer->disabled != 0u) {
        return AZ_REV1655_BROWSE_BAD_BINDINGS;
    }
    if (consumer->request_held == 0u) {
        input_result = consumer->host.take_request(
            consumer->host.context, &consumer->held_index);
        if (input_result == AZ_INPUT_DETOUR_NO_FILTER) {
            return AZ_REV1655_BROWSE_IDLE;
        }
        if (input_result == AZ_INPUT_DETOUR_FILTER_BUSY) {
            return AZ_REV1655_BROWSE_INPUT_BUSY;
        }
        if (input_result != AZ_INPUT_DETOUR_OK) {
            ++consumer->rejected_count;
            return AZ_REV1655_BROWSE_BAD_REQUEST;
        }
        consumer->request_held = 1u;
    }
    if (consumer->host.coverflow_is_interactive(
            consumer->host.context) == 0u) {
        return AZ_REV1655_BROWSE_DEFERRED;
    }
    if (consumer->held_index >= AZ_GLYPH_COUNT) {
        result = AZ_REV1655_BROWSE_BAD_REQUEST;
        goto finish;
    }
    gcm = consumer->host.gcm_singleton(consumer->host.context);
    if (gcm == NULL || consumer->host.active_list(
            consumer->host.context, &before) == 0u ||
        list_is_valid(consumer, &before, &count) == 0u || count == 0u) {
        result = AZ_REV1655_BROWSE_BAD_LIST;
        goto finish;
    }

    if (consumer->held_index == AZ_FILTER_ALL_INDEX) {
        target = 0u;
    }
    else {
        for (index = 0u; index < count; ++index) {
            if (item_matches(
                    consumer, &before.begin[index],
                    consumer->held_index) != 0u) {
                target = index;
                break;
            }
        }
    }

    if (consumer->host.active_list(
            consumer->host.context, &after) == 0u ||
        before.begin != after.begin || before.end != after.end) {
        result = AZ_REV1655_BROWSE_RACE;
        goto finish;
    }
    if (target == UINT32_MAX) {
        ++consumer->no_match_count;
        result = AZ_REV1655_BROWSE_NO_MATCH;
        goto finish;
    }
    if (consumer->host.publish_jump(
            consumer->host.context, gcm, target, count) == 0u) {
        result = AZ_REV1655_BROWSE_PUBLISH_FAILED;
        goto finish;
    }
    ++consumer->queued_count;
    result = AZ_REV1655_BROWSE_JUMP_QUEUED;

finish:
    if (result != AZ_REV1655_BROWSE_JUMP_QUEUED &&
        result != AZ_REV1655_BROWSE_NO_MATCH) {
        ++consumer->rejected_count;
    }
    consumer->host.finish_request(consumer->host.context);
    consumer->request_held = 0u;
    consumer->held_index = AZ_NO_GLYPH;
    return result;
}

AzRev1655BrowseResult az_rev1655_browse_consumer_cancel(
    AzRev1655BrowseConsumer *consumer)
{
    if (consumer == NULL || consumer->bound == 0u) {
        return AZ_REV1655_BROWSE_BAD_BINDINGS;
    }
    if (consumer->request_held != 0u) {
        consumer->host.finish_request(consumer->host.context);
    }
    consumer->request_held = 0u;
    consumer->held_index = AZ_NO_GLYPH;
    consumer->disabled = 1u;
    return AZ_REV1655_BROWSE_CANCELLED;
}

const char *az_rev1655_browse_result_name(AzRev1655BrowseResult result)
{
    switch (result) {
    case AZ_REV1655_BROWSE_JUMP_QUEUED: return "jump-queued";
    case AZ_REV1655_BROWSE_IDLE: return "idle";
    case AZ_REV1655_BROWSE_DEFERRED: return "deferred";
    case AZ_REV1655_BROWSE_NO_MATCH: return "no-match";
    case AZ_REV1655_BROWSE_INPUT_BUSY: return "input-busy";
    case AZ_REV1655_BROWSE_BAD_REQUEST: return "bad-request";
    case AZ_REV1655_BROWSE_BAD_BINDINGS: return "bad-bindings";
    case AZ_REV1655_BROWSE_BAD_LIST: return "bad-list";
    case AZ_REV1655_BROWSE_RACE: return "race";
    case AZ_REV1655_BROWSE_PUBLISH_FAILED: return "publish-failed";
    case AZ_REV1655_BROWSE_CANCELLED: return "cancelled";
    default: return "unknown";
    }
}
