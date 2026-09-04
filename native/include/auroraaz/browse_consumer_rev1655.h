#ifndef AURORAAZ_BROWSE_CONSUMER_REV1655_H
#define AURORAAZ_BROWSE_CONSUMER_REV1655_H

#include <stddef.h>
#include <stdint.h>

#include <auroraaz/filter_consumer_xbox360.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZ_REV1655_BROWSE_MAX_ITEMS 16384u

typedef struct AzRev1655BrowseItem {
    uint32_t object_address;
    uint32_t owner_address;
} AzRev1655BrowseItem;

typedef struct AzRev1655BrowseList {
    const AzRev1655BrowseItem *begin;
    const AzRev1655BrowseItem *end;
} AzRev1655BrowseList;

typedef struct AzRev1655BrowseHostOps {
    void *context;
    uint8_t (*coverflow_is_interactive)(void *context);
    uint8_t (*address_range_is_valid)(
        void *context,
        const void *address,
        size_t size);
    AzInputDetourResult (*take_request)(void *context, uint8_t *index);
    void (*finish_request)(void *context);
    void *(*gcm_singleton)(void *context);
    uint8_t (*active_list)(
        void *context,
        AzRev1655BrowseList *list);
    uint8_t (*string_view)(
        void *context,
        const AzRev1655AuroraString *value,
        const char **characters,
        uint32_t *length,
        uint32_t *capacity);
    uint8_t (*publish_jump)(
        void *context,
        void *gcm,
        uint32_t target_index,
        uint32_t item_count);
} AzRev1655BrowseHostOps;

typedef enum AzRev1655BrowseResult {
    AZ_REV1655_BROWSE_JUMP_QUEUED = 0,
    AZ_REV1655_BROWSE_IDLE,
    AZ_REV1655_BROWSE_DEFERRED,
    AZ_REV1655_BROWSE_NO_MATCH,
    AZ_REV1655_BROWSE_INPUT_BUSY,
    AZ_REV1655_BROWSE_BAD_REQUEST,
    AZ_REV1655_BROWSE_BAD_BINDINGS,
    AZ_REV1655_BROWSE_BAD_LIST,
    AZ_REV1655_BROWSE_RACE,
    AZ_REV1655_BROWSE_PUBLISH_FAILED,
    AZ_REV1655_BROWSE_CANCELLED
} AzRev1655BrowseResult;

typedef struct AzRev1655BrowseConsumer {
    AzRev1655BrowseHostOps host;
    uint8_t bound;
    uint8_t disabled;
    uint8_t request_held;
    uint8_t held_index;
    uint32_t queued_count;
    uint32_t no_match_count;
    uint32_t rejected_count;
} AzRev1655BrowseConsumer;

AzRev1655BrowseResult az_rev1655_browse_consumer_bind(
    AzRev1655BrowseConsumer *consumer,
    const AzRev1655BrowseHostOps *host);

AzRev1655BrowseResult az_rev1655_browse_consumer_worker_step(
    AzRev1655BrowseConsumer *consumer);

AzRev1655BrowseResult az_rev1655_browse_consumer_cancel(
    AzRev1655BrowseConsumer *consumer);

const char *az_rev1655_browse_result_name(AzRev1655BrowseResult result);

#ifdef __cplusplus
}
#endif

#endif
