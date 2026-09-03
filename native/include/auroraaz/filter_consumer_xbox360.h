#ifndef AURORAAZ_FILTER_CONSUMER_XBOX360_H
#define AURORAAZ_FILTER_CONSUMER_XBOX360_H

#include <stddef.h>
#include <stdint.h>

#include <auroraaz/input_detour.h>
#include <auroraaz/rev1655_hook_gate.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZ_REV1655_AURORA_STRING_SIZE 0x1Cu
#define AZ_REV1655_WORK74_SIZE 0x74u
#define AZ_REV1655_CONTEXT38_SIZE 0x38u
#define AZ_REV1655_ACTIVE_AGGREGATE_SIZE 0xD0u
#define AZ_REV1655_FILTER_FLAGS_ADDITIONAL 0x08u
#define AZ_REV1655_FILTER_MAX_TEXT 4096u
#define AZ_REV1655_FILTER_MAX_VECTOR_ITEMS 128u

typedef union AzRev1655AuroraString {
    uint8_t storage[AZ_REV1655_AURORA_STRING_SIZE];
    uint32_t alignment;
} AzRev1655AuroraString;

typedef struct AzRev1655AuroraStringVector {
    uint32_t begin_address;
    uint32_t end_address;
    uint32_t capacity_address;
} AzRev1655AuroraStringVector;

typedef struct AzRev1655FilterWork74 {
    uint32_t include_hidden;
    uint32_t favorites_only;
    uint32_t sort_behavior;
    uint32_t content_context_id;
    uint32_t quickview_id;
    AzRev1655AuroraString sort_method;
    AzRev1655AuroraString compiled_quickview_filter;
    AzRev1655AuroraString search_text;
    AzRev1655AuroraStringVector additional_filter_ids;
} AzRev1655FilterWork74;

typedef struct AzRev1655FilterContext38 {
    AzRev1655AuroraString values[2];
} AzRev1655FilterContext38;

typedef struct AzRev1655ActiveAggregateD0 {
    AzRev1655FilterWork74 work;
    uint8_t unknown_74_to_77[4];
    AzRev1655FilterContext38 context;
    uint8_t unknown_b0_to_cf[0x20];
} AzRev1655ActiveAggregateD0;

typedef struct AzRev1655FilterProvenance {
    uint8_t aurora_xex_sha256[32];
    uint8_t extracted_pe_sha256[32];
} AzRev1655FilterProvenance;

/*
 * These are metadata as well as documentation: bind rejects an adapter unless
 * every Aurora target address is the reviewed Rev1655 address. The callbacks
 * below may be small ABI adapters, but they may not redirect the host calls.
 */
typedef struct AzRev1655FilterEntrypoints {
    uint32_t gcm_singleton;
    uint32_t copy_active_aggregate;
    uint32_t destroy_active_aggregate;
    uint32_t string_construct_cstring;
    uint32_t string_assign_bytes;
    uint32_t string_lifecycle;
    uint32_t vector_push_back;
    uint32_t registry_singleton;
    uint32_t registry_lookup;
    uint32_t scheduler;
} AzRev1655FilterEntrypoints;

typedef struct AzRev1655FilterHostOps {
    void *context;
    AzRev1655FilterEntrypoints entrypoints;

    uint32_t (*current_thread_id)(void *context);
    uint8_t (*worker_affinity_verified)(
        void *context,
        uint32_t worker_thread_id);
    uint8_t (*coverflow_is_interactive)(void *context);
    uint8_t (*filter_queue_is_demonstrably_idle)(void *context);
    uint8_t (*address_range_is_valid)(
        void *context,
        const void *address,
        size_t size);

    AzInputDetourResult (*take_filter_request)(
        void *context,
        uint8_t *filter_index);
    void (*finish_filter_request)(void *context);

    void *(*gcm_singleton)(void *context);
    void *(*registry_singleton)(void *context);
    uint8_t (*registry_lookup)(
        void *context,
        void *registry,
        uint32_t registry_type,
        const char *identifier);

    uint8_t (*copy_active_aggregate)(
        void *context,
        AzRev1655ActiveAggregateD0 *destination,
        const void *gcm_plus_60,
        uint32_t staging_selector);
    void (*destroy_active_aggregate)(
        void *context,
        AzRev1655ActiveAggregateD0 *aggregate);

    uint8_t (*string_view)(
        void *context,
        const AzRev1655AuroraString *value,
        const char **characters,
        uint32_t *length,
        uint32_t *capacity);
    uint8_t (*string_construct_cstring)(
        void *context,
        AzRev1655AuroraString *destination,
        const char *source);
    uint8_t (*string_assign_bytes)(
        void *context,
        AzRev1655AuroraString *destination,
        const char *source,
        uint32_t length);
    void (*string_destroy)(
        void *context,
        AzRev1655AuroraString *value);

    uint8_t (*vector_count)(
        void *context,
        const AzRev1655AuroraStringVector *vector,
        uint32_t *count);
    AzRev1655AuroraString *(*vector_at)(
        void *context,
        AzRev1655AuroraStringVector *vector,
        uint32_t index);
    uint8_t (*vector_push_back)(
        void *context,
        AzRev1655AuroraStringVector *vector,
        const AzRev1655AuroraString *value);

    int32_t (*schedule_filter)(
        void *context,
        void *gcm_plus_8,
        const AzRev1655FilterContext38 *filter_context,
        const AzRev1655FilterWork74 *work,
        uint32_t flags);
} AzRev1655FilterHostOps;

typedef enum AzRev1655FilterConsumerResult {
    AZ_REV1655_FILTER_CONSUMER_SCHEDULED = 0,
    AZ_REV1655_FILTER_CONSUMER_IDLE,
    AZ_REV1655_FILTER_CONSUMER_DEFERRED,
    AZ_REV1655_FILTER_CONSUMER_NOT_WORKER,
    AZ_REV1655_FILTER_CONSUMER_NOT_BOUND,
    AZ_REV1655_FILTER_CONSUMER_NOT_RUNTIME_VERIFIED,
    AZ_REV1655_FILTER_CONSUMER_DISABLED,
    AZ_REV1655_FILTER_CONSUMER_NULL_ARGUMENT,
    AZ_REV1655_FILTER_CONSUMER_BAD_PROVENANCE,
    AZ_REV1655_FILTER_CONSUMER_BAD_IMAGE,
    AZ_REV1655_FILTER_CONSUMER_BAD_HELPER_SIGNATURE,
    AZ_REV1655_FILTER_CONSUMER_BAD_BINDINGS,
    AZ_REV1655_FILTER_CONSUMER_INPUT_BUSY,
    AZ_REV1655_FILTER_CONSUMER_INPUT_ERROR,
    AZ_REV1655_FILTER_CONSUMER_BAD_FILTER_INDEX,
    AZ_REV1655_FILTER_CONSUMER_REGISTRY_MISSING,
    AZ_REV1655_FILTER_CONSUMER_SNAPSHOT_INVALID,
    AZ_REV1655_FILTER_CONSUMER_AMBIGUOUS_NAME_FILTER,
    AZ_REV1655_FILTER_CONSUMER_MUTATION_FAILED,
    AZ_REV1655_FILTER_CONSUMER_SCHEDULE_FAILED,
    AZ_REV1655_FILTER_CONSUMER_CANCELLED
} AzRev1655FilterConsumerResult;

typedef struct AzRev1655FilterConsumerStatus {
    uint32_t worker_thread_id;
    uint32_t probe_count;
    uint32_t scheduled_count;
    uint32_t rejected_count;
    uint32_t deferred_count;
    uint8_t bound;
    uint8_t runtime_verified;
    uint8_t disabled;
    uint8_t request_held;
    uint8_t held_filter_index;
} AzRev1655FilterConsumerStatus;

typedef struct AzRev1655FilterConsumer {
    AzRev1655FilterHostOps host;
    uint32_t worker_thread_id;
    uint32_t probe_count;
    uint32_t scheduled_count;
    uint32_t rejected_count;
    uint32_t deferred_count;
    uint8_t bound;
    uint8_t runtime_verified;
    uint8_t disabled;
    uint8_t request_held;
    uint8_t held_filter_index;
    char replacement_scratch[AZ_REV1655_FILTER_MAX_TEXT + 1u];
} AzRev1655FilterConsumer;

void az_rev1655_filter_consumer_exact_entrypoints(
    AzRev1655FilterEntrypoints *entrypoints);

/* Copies the immutable Rev1655 XEX/PE digests required by bind(). */
void az_rev1655_filter_consumer_exact_provenance(
    AzRev1655FilterProvenance *provenance);

AzRev1655FilterConsumerResult az_rev1655_filter_consumer_bind(
    AzRev1655FilterConsumer *consumer,
    const AzRev1655LoadedImage *image,
    const AzRev1655FilterProvenance *provenance,
    uint32_t worker_thread_id,
    const AzRev1655FilterHostOps *host);

/*
 * Runtime binding requires the same authoritative import resolver as the
 * Rev1655 hook gate. The compatibility entry point above deliberately has no
 * resolver and therefore remains fail-closed for a loader-resolved image.
 */
AzRev1655FilterConsumerResult
az_rev1655_filter_consumer_bind_with_import_resolver(
    AzRev1655FilterConsumer *consumer,
    const AzRev1655LoadedImage *image,
    const AzRev1655ImportResolver *import_resolver,
    const AzRev1655FilterProvenance *provenance,
    uint32_t worker_thread_id,
    const AzRev1655FilterHostOps *host);

/*
 * Stage-1 read-only canary. Call from the dedicated worker before publishing
 * input_detour's filter_consumer_verified gate. It validates all 27 registry
 * entries and copy/destroys the active aggregate without mutation.
 */
AzRev1655FilterConsumerResult az_rev1655_filter_consumer_worker_probe(
    AzRev1655FilterConsumer *consumer);

/*
 * The sole apply entry point. It obtains requests through input_detour's
 * worker API, never from an input/render hook, and calls only the asynchronous
 * scheduler with flag 0x08.
 */
AzRev1655FilterConsumerResult az_rev1655_filter_consumer_worker_step(
    AzRev1655FilterConsumer *consumer);

AzRev1655FilterConsumerResult az_rev1655_filter_consumer_worker_cancel(
    AzRev1655FilterConsumer *consumer);

void az_rev1655_filter_consumer_snapshot_status(
    const AzRev1655FilterConsumer *consumer,
    AzRev1655FilterConsumerStatus *status);

const char *az_rev1655_filter_consumer_result_name(
    AzRev1655FilterConsumerResult result);

#ifdef __cplusplus
}
#endif

#endif
