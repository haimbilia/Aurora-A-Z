#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <auroraaz/filter_consumer_xbox360.h>
#include <auroraaz/filters.h>
#include <auroraaz/image.h>

typedef char AzRev1655AuroraStringSizeCheck[
    sizeof(AzRev1655AuroraString) == AZ_REV1655_AURORA_STRING_SIZE ? 1 : -1];
typedef char AzRev1655WorkSizeCheck[
    sizeof(AzRev1655FilterWork74) == AZ_REV1655_WORK74_SIZE ? 1 : -1];
typedef char AzRev1655ContextSizeCheck[
    sizeof(AzRev1655FilterContext38) == AZ_REV1655_CONTEXT38_SIZE ? 1 : -1];
typedef char AzRev1655AggregateSizeCheck[
    sizeof(AzRev1655ActiveAggregateD0) ==
        AZ_REV1655_ACTIVE_AGGREGATE_SIZE ? 1 : -1];
typedef char AzRev1655CompiledOffsetCheck[
    offsetof(AzRev1655FilterWork74, compiled_quickview_filter) == 0x30u ?
        1 : -1];
typedef char AzRev1655SearchOffsetCheck[
    offsetof(AzRev1655FilterWork74, search_text) == 0x4Cu ? 1 : -1];
typedef char AzRev1655VectorOffsetCheck[
    offsetof(AzRev1655FilterWork74, additional_filter_ids) == 0x68u ?
        1 : -1];
typedef char AzRev1655ContextOffsetCheck[
    offsetof(AzRev1655ActiveAggregateD0, context) == 0x78u ? 1 : -1];

#define AZ_REV1655_GCM_SINGLETON_ADDRESS 0x82223060u
#define AZ_REV1655_COPY_ACTIVE_ADDRESS 0x8222B3E8u
#define AZ_REV1655_DESTROY_ACTIVE_ADDRESS 0x8222B6C8u
#define AZ_REV1655_STRING_CONSTRUCT_ADDRESS 0x82212CE8u
#define AZ_REV1655_STRING_ASSIGN_ADDRESS 0x82212DB0u
#define AZ_REV1655_STRING_LIFECYCLE_ADDRESS 0x82213580u
#define AZ_REV1655_VECTOR_PUSH_ADDRESS 0x822A6228u
#define AZ_REV1655_REGISTRY_SINGLETON_ADDRESS 0x82271000u
#define AZ_REV1655_REGISTRY_LOOKUP_ADDRESS 0x82324C60u
#define AZ_REV1655_SCHEDULER_ADDRESS 0x82343628u
#define AZ_REV1655_FILTER_WORKER_ADDRESS 0x82356588u
#define AZ_REV1655_STOCK_FILTER_HANDLER_ADDRESS 0x822E5838u
#define AZ_REV1655_GCM_REQUIRED_SPAN 0x138u

#define AZ_REV1655_GCM_COPY_SOURCE_OFFSET 0x60u
#define AZ_REV1655_GCM_QUEUE_OFFSET 0x08u

static const uint8_t k_rev1655_xex_sha256[32] = {
    0x58, 0x3B, 0xCD, 0x44, 0x2D, 0x80, 0x17, 0xD6,
    0xFC, 0xB2, 0x64, 0x5B, 0x93, 0xCD, 0xA9, 0x87,
    0xF4, 0xC0, 0xA4, 0x3A, 0x68, 0x8B, 0x65, 0x2D,
    0x73, 0x64, 0xCC, 0xAE, 0xDA, 0xEE, 0xFA, 0x9F
};

static const uint8_t k_rev1655_pe_sha256[32] = {
    0x5B, 0xB5, 0xBA, 0xF8, 0xDF, 0x4C, 0xCB, 0x19,
    0x72, 0x41, 0xB3, 0x49, 0x35, 0xEB, 0x40, 0x0F,
    0x36, 0xC8, 0xC2, 0x06, 0x48, 0xCC, 0x07, 0x4E,
    0x2C, 0x30, 0xFA, 0x80, 0xAD, 0xD3, 0x7E, 0x3C
};

static const uint8_t k_gcm_singleton_signature[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x91, 0x81, 0xFF, 0xF8,
    0xFB, 0xE1, 0xFF, 0xF0, 0x3B, 0xE1, 0xFF, 0xA0,
    0x94, 0x21, 0xFF, 0xA0, 0x3D, 0x40, 0x82, 0xBC,
    0x81, 0x6A, 0x05, 0x7C
};

static const uint8_t k_copy_active_signature[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x91, 0x81, 0xFF, 0xF8,
    0xFB, 0xE1, 0xFF, 0xF0, 0x94, 0x21, 0xFF, 0xA0,
    0x7C, 0x7F, 0x1B, 0x78, 0x2F, 0x05, 0x00, 0x01,
    0x40, 0x9A, 0x00, 0x0C, 0x38, 0x84, 0x00, 0xD8,
    0x48, 0x00, 0x00, 0x08, 0x38, 0x84, 0x00, 0x08
};

static const uint8_t k_destroy_active_signature[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x48, 0x73, 0xC6, 0x01,
    0x3B, 0xE1, 0xFF, 0x80, 0x94, 0x21, 0xFF, 0x80
};

static const uint8_t k_string_construct_signature[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x91, 0x81, 0xFF, 0xF8,
    0xFB, 0xC1, 0xFF, 0xE8, 0xFB, 0xE1, 0xFF, 0xF0,
    0x94, 0x21, 0xFF, 0x90, 0x7C, 0x9E, 0x23, 0x78,
    0x38, 0xA0, 0x00, 0x00, 0x38, 0x80, 0x00, 0x00,
    0x7C, 0x7F, 0x1B, 0x78, 0x48, 0x00, 0x08, 0x75
};

static const uint8_t k_string_assign_signature[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x48, 0x75, 0x4F, 0x19,
    0x94, 0x21, 0xFF, 0x90, 0x7C, 0x7F, 0x1B, 0x78
};

static const uint8_t k_string_lifecycle_signature[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x48, 0x75, 0x47, 0x49,
    0x94, 0x21, 0xFF, 0x90, 0x7C, 0x7F, 0x1B, 0x78
};

static const uint8_t k_vector_push_signature[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x48, 0x6C, 0x1A, 0xA1,
    0x3B, 0xE1, 0xFF, 0x80, 0x94, 0x21, 0xFF, 0x80
};

static const uint8_t k_registry_singleton_signature[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x91, 0x81, 0xFF, 0xF8,
    0xFB, 0xE1, 0xFF, 0xF0, 0x3B, 0xE1, 0xFF, 0xA0,
    0x94, 0x21, 0xFF, 0xA0, 0x3D, 0x40, 0x82, 0xBD,
    0x81, 0x6A, 0x9E, 0x44
};

static const uint8_t k_registry_lookup_signature[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x48, 0x64, 0x30, 0x69,
    0x3B, 0xE1, 0xFF, 0x60, 0x94, 0x21, 0xFF, 0x60
};

static const uint8_t k_scheduler_signature[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x48, 0x62, 0x46, 0x8D,
    0x94, 0x21, 0xFF, 0x60, 0x7C, 0x7B, 0x1B, 0x78
};

static const uint8_t k_filter_worker_signature[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x48, 0x61, 0x17, 0x29,
    0x3B, 0xE1, 0xFF, 0x30, 0x94, 0x21, 0xFF, 0x30
};

static const uint8_t k_stock_filter_handler_signature[] = {
    0x7D, 0x88, 0x02, 0xA6, 0x91, 0x81, 0xFF, 0xF8,
    0xFB, 0xC1, 0xFF, 0xE8, 0xFB, 0xE1, 0xFF, 0xF0,
    0x3B, 0xE1, 0xFE, 0x40, 0x94, 0x21, 0xFE, 0x40,
    0x7C, 0x7E, 0x1B, 0x78, 0x38, 0x7F, 0x00, 0x50
};

typedef struct AzSignatureWindow {
    uint32_t address;
    const uint8_t *bytes;
    size_t size;
} AzSignatureWindow;

static const AzSignatureWindow k_required_signatures[] = {
    { AZ_REV1655_GCM_SINGLETON_ADDRESS,
      k_gcm_singleton_signature, sizeof(k_gcm_singleton_signature) },
    { AZ_REV1655_COPY_ACTIVE_ADDRESS,
      k_copy_active_signature, sizeof(k_copy_active_signature) },
    { AZ_REV1655_DESTROY_ACTIVE_ADDRESS,
      k_destroy_active_signature, sizeof(k_destroy_active_signature) },
    { AZ_REV1655_STRING_CONSTRUCT_ADDRESS,
      k_string_construct_signature, sizeof(k_string_construct_signature) },
    { AZ_REV1655_STRING_ASSIGN_ADDRESS,
      k_string_assign_signature, sizeof(k_string_assign_signature) },
    { AZ_REV1655_STRING_LIFECYCLE_ADDRESS,
      k_string_lifecycle_signature, sizeof(k_string_lifecycle_signature) },
    { AZ_REV1655_VECTOR_PUSH_ADDRESS,
      k_vector_push_signature, sizeof(k_vector_push_signature) },
    { AZ_REV1655_REGISTRY_SINGLETON_ADDRESS,
      k_registry_singleton_signature,
      sizeof(k_registry_singleton_signature) },
    { AZ_REV1655_REGISTRY_LOOKUP_ADDRESS,
      k_registry_lookup_signature, sizeof(k_registry_lookup_signature) },
    { AZ_REV1655_SCHEDULER_ADDRESS,
      k_scheduler_signature, sizeof(k_scheduler_signature) },
    { AZ_REV1655_FILTER_WORKER_ADDRESS,
      k_filter_worker_signature, sizeof(k_filter_worker_signature) },
    { AZ_REV1655_STOCK_FILTER_HANDLER_ADDRESS,
      k_stock_filter_handler_signature,
      sizeof(k_stock_filter_handler_signature) }
};

typedef struct AzStringView {
    const char *characters;
    uint32_t length;
    uint32_t capacity;
} AzStringView;

typedef struct AzNameFilterClassification {
    AzRev1655AuroraString *raw_element;
    uint32_t raw_count;
    uint8_t raw_index;
    uint32_t raw_vector_index;
    uint32_t compiled_count;
    uint8_t compiled_index;
    uint32_t compiled_offset;
    uint32_t compiled_length;
    uint8_t ambiguous;
} AzNameFilterClassification;

static size_t bounded_cstring_length(const char *value, size_t maximum)
{
    size_t length = 0u;

    if (value == NULL) {
        return maximum + 1u;
    }
    while (length <= maximum && value[length] != '\0') {
        ++length;
    }
    return length;
}

static uint8_t bytes_equal(
    const uint8_t *left,
    const uint8_t *right,
    size_t size)
{
    size_t index;
    uint8_t difference = 0u;

    if (left == NULL || right == NULL) {
        return 0u;
    }
    for (index = 0u; index < size; ++index) {
        difference = (uint8_t)(difference | (uint8_t)(left[index] ^ right[index]));
    }
    return difference == 0u ? 1u : 0u;
}

static uint8_t text_equals(
    const char *characters,
    uint32_t length,
    const char *candidate)
{
    const size_t candidate_length = bounded_cstring_length(
        candidate, AZ_REV1655_FILTER_MAX_TEXT);

    if (characters == NULL ||
        candidate_length > AZ_REV1655_FILTER_MAX_TEXT ||
        candidate_length != (size_t)length) {
        return 0u;
    }
    return memcmp(characters, candidate, candidate_length) == 0 ? 1u : 0u;
}

static uint8_t range_contains_text(
    const char *characters,
    uint32_t length,
    const char *needle)
{
    const size_t needle_length = bounded_cstring_length(
        needle, AZ_REV1655_FILTER_MAX_TEXT);
    size_t offset;

    if (characters == NULL || needle_length == 0u ||
        needle_length > (size_t)length) {
        return 0u;
    }
    for (offset = 0u; offset + needle_length <= (size_t)length; ++offset) {
        if (memcmp(characters + offset, needle, needle_length) == 0) {
            return 1u;
        }
    }
    return 0u;
}

static uint8_t exact_entrypoints_match(
    const AzRev1655FilterEntrypoints *entrypoints)
{
    if (entrypoints == NULL) {
        return 0u;
    }
    return
        entrypoints->gcm_singleton == AZ_REV1655_GCM_SINGLETON_ADDRESS &&
        entrypoints->copy_active_aggregate == AZ_REV1655_COPY_ACTIVE_ADDRESS &&
        entrypoints->destroy_active_aggregate ==
            AZ_REV1655_DESTROY_ACTIVE_ADDRESS &&
        entrypoints->string_construct_cstring ==
            AZ_REV1655_STRING_CONSTRUCT_ADDRESS &&
        entrypoints->string_assign_bytes == AZ_REV1655_STRING_ASSIGN_ADDRESS &&
        entrypoints->string_lifecycle == AZ_REV1655_STRING_LIFECYCLE_ADDRESS &&
        entrypoints->vector_push_back == AZ_REV1655_VECTOR_PUSH_ADDRESS &&
        entrypoints->registry_singleton ==
            AZ_REV1655_REGISTRY_SINGLETON_ADDRESS &&
        entrypoints->registry_lookup == AZ_REV1655_REGISTRY_LOOKUP_ADDRESS &&
        entrypoints->scheduler == AZ_REV1655_SCHEDULER_ADDRESS ? 1u : 0u;
}

static uint8_t host_callbacks_are_complete(
    const AzRev1655FilterHostOps *host)
{
    if (host == NULL || exact_entrypoints_match(&host->entrypoints) == 0u) {
        return 0u;
    }
    return
        host->current_thread_id != NULL &&
        host->worker_affinity_verified != NULL &&
        host->coverflow_is_interactive != NULL &&
        host->filter_queue_is_demonstrably_idle != NULL &&
        host->address_range_is_valid != NULL &&
        host->take_filter_request != NULL &&
        host->finish_filter_request != NULL &&
        host->gcm_singleton != NULL &&
        host->registry_singleton != NULL &&
        host->registry_lookup != NULL &&
        host->copy_active_aggregate != NULL &&
        host->destroy_active_aggregate != NULL &&
        host->string_view != NULL &&
        host->string_construct_cstring != NULL &&
        host->string_assign_bytes != NULL &&
        host->string_destroy != NULL &&
        host->vector_count != NULL &&
        host->vector_at != NULL &&
        host->vector_push_back != NULL &&
        host->vector_erase != NULL &&
        host->schedule_filter != NULL ? 1u : 0u;
}

static uint8_t signatures_match(const AzRev1655LoadedImage *image)
{
    size_t index;

    if (image == NULL || image->bytes == NULL ||
        image->virtual_address != AZ_REV1655_IMAGE_BASE ||
        image->size != (size_t)AZ_REV1655_NT_IMAGE_SIZE) {
        return 0u;
    }

    for (index = 0u;
         index < sizeof(k_required_signatures) / sizeof(k_required_signatures[0]);
         ++index) {
        const AzSignatureWindow *window = &k_required_signatures[index];
        size_t offset;

        if (window->address < image->virtual_address) {
            return 0u;
        }
        offset = (size_t)(window->address - image->virtual_address);
        if (offset > image->size || window->size > image->size - offset ||
            memcmp(image->bytes + offset, window->bytes, window->size) != 0) {
            return 0u;
        }
    }
    return 1u;
}

static uint8_t range_is_valid(
    const AzRev1655FilterConsumer *consumer,
    const void *address,
    size_t size)
{
    uintptr_t start;

    if (consumer == NULL || address == NULL || size == 0u) {
        return 0u;
    }
    start = (uintptr_t)address;
    if (start > UINTPTR_MAX - (size - 1u)) {
        return 0u;
    }
    return consumer->host.address_range_is_valid(
        consumer->host.context, address, size) != 0u ? 1u : 0u;
}

static uint8_t string_is_valid(
    const AzRev1655FilterConsumer *consumer,
    const AzRev1655AuroraString *value,
    AzStringView *view)
{
    const char *characters = NULL;
    uint32_t length = 0u;
    uint32_t capacity = 0u;
    uint32_t index;

    if (consumer == NULL || value == NULL || view == NULL ||
        range_is_valid(consumer, value, sizeof(*value)) == 0u ||
        consumer->host.string_view(
            consumer->host.context,
            value,
            &characters,
            &length,
            &capacity) == 0u ||
        characters == NULL ||
        length > AZ_REV1655_FILTER_MAX_TEXT ||
        capacity > AZ_REV1655_FILTER_MAX_TEXT ||
        length > capacity ||
        (capacity < 0x10u && length >= 0x10u) ||
        range_is_valid(consumer, characters, (size_t)length + 1u) == 0u ||
        characters[length] != '\0') {
        return 0u;
    }

    for (index = 0u; index < length; ++index) {
        if (characters[index] == '\0') {
            return 0u;
        }
    }

    view->characters = characters;
    view->length = length;
    view->capacity = capacity;
    return 1u;
}

static uint8_t work_is_valid(
    const AzRev1655FilterConsumer *consumer,
    AzRev1655FilterWork74 *work)
{
    AzStringView ignored;
    uint32_t count = 0u;
    uint32_t index;

    if (consumer == NULL || work == NULL ||
        range_is_valid(consumer, work, sizeof(*work)) == 0u ||
        work->quickview_id == 0u ||
        string_is_valid(consumer, &work->sort_method, &ignored) == 0u ||
        string_is_valid(
            consumer, &work->compiled_quickview_filter, &ignored) == 0u ||
        string_is_valid(consumer, &work->search_text, &ignored) == 0u ||
        consumer->host.vector_count(
            consumer->host.context,
            &work->additional_filter_ids,
            &count) == 0u ||
        count > AZ_REV1655_FILTER_MAX_VECTOR_ITEMS) {
        return 0u;
    }

    for (index = 0u; index < count; ++index) {
        AzRev1655AuroraString *element = consumer->host.vector_at(
            consumer->host.context,
            &work->additional_filter_ids,
            index);
        if (element == NULL || string_is_valid(consumer, element, &ignored) == 0u) {
            return 0u;
        }
    }
    return 1u;
}

static uint8_t aggregate_is_valid(
    const AzRev1655FilterConsumer *consumer,
    AzRev1655ActiveAggregateD0 *aggregate)
{
    AzStringView ignored;

    if (consumer == NULL || aggregate == NULL ||
        range_is_valid(consumer, aggregate, sizeof(*aggregate)) == 0u ||
        work_is_valid(consumer, &aggregate->work) == 0u ||
        string_is_valid(consumer, &aggregate->context.values[0], &ignored) == 0u ||
        string_is_valid(consumer, &aggregate->context.values[1], &ignored) == 0u) {
        return 0u;
    }
    return 1u;
}

static uint8_t append_character(
    char *destination,
    size_t capacity,
    size_t *length,
    char value)
{
    if (destination == NULL || length == NULL || *length >= capacity) {
        return 0u;
    }
    destination[*length] = value;
    ++*length;
    return 1u;
}

static uint8_t append_text(
    char *destination,
    size_t capacity,
    size_t *length,
    const char *source,
    size_t source_length)
{
    if (destination == NULL || length == NULL || source == NULL ||
        *length > capacity || source_length > capacity - *length) {
        return 0u;
    }
    memcpy(destination + *length, source, source_length);
    *length += source_length;
    return 1u;
}

static uint8_t build_compiled_leaf(
    const char *method,
    char *destination,
    size_t capacity,
    uint32_t *output_length)
{
    static const char prefix[] = "GameListFilterCategories[\"";
    static const char separator[] = "\"][\"";
    static const char suffix[] = "\"](Content)";
    const size_t method_length = bounded_cstring_length(
        method, AZ_REV1655_FILTER_MAX_TEXT);
    size_t component_start = 0u;
    size_t index;
    size_t length = 0u;

    if (method == NULL || destination == NULL || output_length == NULL ||
        capacity == 0u || method_length == 0u ||
        method_length > AZ_REV1655_FILTER_MAX_TEXT ||
        append_text(destination, capacity - 1u, &length,
            prefix, sizeof(prefix) - 1u) == 0u) {
        return 0u;
    }

    for (index = 0u; index <= method_length; ++index) {
        if (index == method_length || method[index] == '.') {
            if (index == component_start ||
                append_text(destination, capacity - 1u, &length,
                    method + component_start, index - component_start) == 0u) {
                return 0u;
            }
            if (index != method_length &&
                append_text(destination, capacity - 1u, &length,
                    separator, sizeof(separator) - 1u) == 0u) {
                return 0u;
            }
            component_start = index + 1u;
        }
    }

    if (append_text(destination, capacity - 1u, &length,
            suffix, sizeof(suffix) - 1u) == 0u ||
        append_character(destination, capacity, &length, '\0') == 0u ||
        length - 1u > UINT32_MAX) {
        return 0u;
    }
    *output_length = (uint32_t)(length - 1u);
    return 1u;
}

static uint8_t compiled_leaf_has_boundaries(
    const char *text,
    uint32_t text_length,
    uint32_t offset,
    uint32_t leaf_length)
{
    char before = '\0';
    char after = '\0';

    if (text == NULL || offset > text_length ||
        leaf_length > text_length - offset) {
        return 0u;
    }
    if (offset != 0u) {
        before = text[offset - 1u];
        if (!(before == ' ' || before == '\t' || before == '\r' ||
              before == '\n' || before == '(')) {
            return 0u;
        }
    }
    if (offset + leaf_length != text_length) {
        after = text[offset + leaf_length];
        if (!(after == ' ' || after == '\t' || after == '\r' ||
              after == '\n' || after == ')')) {
            return 0u;
        }
    }
    return 1u;
}

static uint8_t canonical_index_for_text(
    const char *characters,
    uint32_t length,
    uint8_t *filter_index)
{
    uint8_t index;

    if (filter_index == NULL) {
        return 0u;
    }
    for (index = AZ_FILTER_OTHER_INDEX;
         index < AZ_GLYPH_COUNT;
         ++index) {
        const char *method = az_filter_method_for_index(index);
        if (text_equals(characters, length, method) != 0u) {
            *filter_index = index;
            return 1u;
        }
    }
    return 0u;
}

static void classify_name_filters(
    const AzRev1655FilterConsumer *consumer,
    AzRev1655FilterWork74 *work,
    AzNameFilterClassification *classification)
{
    static const char name_filter[] = "NameFilter";
    AzStringView compiled;
    uint32_t vector_count = 0u;
    uint32_t vector_index;
    uint8_t canonical_index;
    uint8_t filter_index;

    memset(classification, 0, sizeof(*classification));
    classification->raw_index = AZ_NO_GLYPH;
    classification->compiled_index = AZ_NO_GLYPH;

    if (consumer->host.vector_count(
            consumer->host.context,
            &work->additional_filter_ids,
            &vector_count) == 0u ||
        vector_count > AZ_REV1655_FILTER_MAX_VECTOR_ITEMS ||
        string_is_valid(
            consumer,
            &work->compiled_quickview_filter,
            &compiled) == 0u) {
        classification->ambiguous = 1u;
        return;
    }

    for (vector_index = 0u; vector_index < vector_count; ++vector_index) {
        AzRev1655AuroraString *element = consumer->host.vector_at(
            consumer->host.context,
            &work->additional_filter_ids,
            vector_index);
        AzStringView raw;

        if (element == NULL || string_is_valid(consumer, element, &raw) == 0u) {
            classification->ambiguous = 1u;
            return;
        }
        if (canonical_index_for_text(
                raw.characters, raw.length, &canonical_index) != 0u) {
            ++classification->raw_count;
            classification->raw_element = element;
            classification->raw_index = canonical_index;
            classification->raw_vector_index = vector_index;
        }
        else if (range_contains_text(
                raw.characters, raw.length, name_filter) != 0u) {
            classification->ambiguous = 1u;
        }
    }

    for (filter_index = AZ_FILTER_OTHER_INDEX;
         filter_index < AZ_GLYPH_COUNT;
         ++filter_index) {
        char leaf[128];
        uint32_t leaf_length = 0u;
        uint32_t offset;

        if (build_compiled_leaf(
                az_filter_method_for_index(filter_index),
                leaf,
                sizeof(leaf),
                &leaf_length) == 0u) {
            classification->ambiguous = 1u;
            return;
        }
        for (offset = 0u;
             offset <= compiled.length && leaf_length <= compiled.length - offset;
             ++offset) {
            if (memcmp(compiled.characters + offset, leaf, leaf_length) == 0 &&
                compiled_leaf_has_boundaries(
                    compiled.characters,
                    compiled.length,
                    offset,
                    leaf_length) != 0u) {
                ++classification->compiled_count;
                classification->compiled_index = filter_index;
                classification->compiled_offset = offset;
                classification->compiled_length = leaf_length;
            }
        }
    }

    if (range_contains_text(
            compiled.characters, compiled.length, name_filter) != 0u) {
        uint32_t name_offset;
        uint32_t name_length = (uint32_t)(sizeof(name_filter) - 1u);

        if (classification->compiled_count != 1u) {
            classification->ambiguous = 1u;
        }
        for (name_offset = 0u;
             name_offset <= compiled.length &&
                 name_length <= compiled.length - name_offset;
             ++name_offset) {
            if (memcmp(
                    compiled.characters + name_offset,
                    name_filter,
                    name_length) == 0) {
                const uint32_t leaf_start = classification->compiled_offset;
                const uint32_t leaf_end = leaf_start +
                    classification->compiled_length;
                if (classification->compiled_count != 1u ||
                    name_offset < leaf_start || name_offset >= leaf_end) {
                    classification->ambiguous = 1u;
                }
            }
        }
    }

    if (classification->raw_count > 1u ||
        classification->compiled_count > 1u ||
        (classification->raw_count != 0u &&
         classification->compiled_count != 0u)) {
        classification->ambiguous = 1u;
    }
}

static uint8_t classification_is_selected(
    const AzNameFilterClassification *classification,
    uint8_t selected_index)
{
    if (classification == NULL || classification->ambiguous != 0u) {
        return 0u;
    }
    if (classification->raw_count == 1u &&
        classification->compiled_count == 0u &&
        classification->raw_index == selected_index) {
        return 1u;
    }
    if (classification->compiled_count == 1u &&
        classification->raw_count == 0u &&
        classification->compiled_index == selected_index) {
        return 1u;
    }
    return 0u;
}

static uint8_t replace_compiled_leaf(
    AzRev1655FilterConsumer *consumer,
    AzRev1655FilterWork74 *work,
    const AzNameFilterClassification *classification,
    const char *selected_method)
{
    AzStringView current;
    char selected_leaf[128];
    uint32_t selected_leaf_length = 0u;
    size_t new_length;
    size_t suffix_offset;
    size_t suffix_length;

    if (consumer == NULL || work == NULL || classification == NULL ||
        string_is_valid(
            consumer,
            &work->compiled_quickview_filter,
            &current) == 0u ||
        build_compiled_leaf(
            selected_method,
            selected_leaf,
            sizeof(selected_leaf),
            &selected_leaf_length) == 0u ||
        classification->compiled_offset > current.length ||
        classification->compiled_length >
            current.length - classification->compiled_offset) {
        return 0u;
    }

    new_length = (size_t)current.length -
        (size_t)classification->compiled_length +
        (size_t)selected_leaf_length;
    if (new_length > AZ_REV1655_FILTER_MAX_TEXT || new_length > UINT32_MAX) {
        return 0u;
    }

    suffix_offset = (size_t)classification->compiled_offset +
        (size_t)classification->compiled_length;
    suffix_length = (size_t)current.length - suffix_offset;
    memcpy(
        consumer->replacement_scratch,
        current.characters,
        (size_t)classification->compiled_offset);
    memcpy(
        consumer->replacement_scratch + classification->compiled_offset,
        selected_leaf,
        (size_t)selected_leaf_length);
    memcpy(
        consumer->replacement_scratch + classification->compiled_offset +
            selected_leaf_length,
        current.characters + suffix_offset,
        suffix_length);
    consumer->replacement_scratch[new_length] = '\0';

    return consumer->host.string_assign_bytes(
        consumer->host.context,
        &work->compiled_quickview_filter,
        consumer->replacement_scratch,
        (uint32_t)new_length) != 0u ? 1u : 0u;
}

static uint8_t remove_compiled_leaf(
    AzRev1655FilterConsumer *consumer,
    AzRev1655FilterWork74 *work,
    const AzNameFilterClassification *classification)
{
    static const char conjunction[] = " and ";
    static const char match_all[] = "return true";
    AzStringView current;
    size_t remove_start;
    size_t remove_end;
    size_t new_length;

    if (consumer == NULL || work == NULL || classification == NULL ||
        classification->compiled_count != 1u ||
        string_is_valid(
            consumer,
            &work->compiled_quickview_filter,
            &current) == 0u ||
        classification->compiled_offset > current.length ||
        classification->compiled_length >
            current.length - classification->compiled_offset) {
        return 0u;
    }

    remove_start = (size_t)classification->compiled_offset;
    remove_end = remove_start + (size_t)classification->compiled_length;
    if (remove_start == 0u && remove_end == (size_t)current.length) {
        return consumer->host.string_assign_bytes(
            consumer->host.context,
            &work->compiled_quickview_filter,
            match_all,
            (uint32_t)(sizeof(match_all) - 1u));
    }

    if (remove_start >= sizeof(conjunction) - 1u &&
        memcmp(
            current.characters + remove_start - (sizeof(conjunction) - 1u),
            conjunction,
            sizeof(conjunction) - 1u) == 0) {
        remove_start -= sizeof(conjunction) - 1u;
    }
    else if (remove_end + (sizeof(conjunction) - 1u) <= current.length &&
        memcmp(
            current.characters + remove_end,
            conjunction,
            sizeof(conjunction) - 1u) == 0) {
        remove_end += sizeof(conjunction) - 1u;
    }
    else {
        return 0u;
    }

    new_length = (size_t)current.length - (remove_end - remove_start);
    if (new_length > AZ_REV1655_FILTER_MAX_TEXT || new_length > UINT32_MAX) {
        return 0u;
    }
    memcpy(consumer->replacement_scratch, current.characters, remove_start);
    memcpy(
        consumer->replacement_scratch + remove_start,
        current.characters + remove_end,
        (size_t)current.length - remove_end);
    consumer->replacement_scratch[new_length] = '\0';
    return consumer->host.string_assign_bytes(
        consumer->host.context,
        &work->compiled_quickview_filter,
        consumer->replacement_scratch,
        (uint32_t)new_length);
}

static uint8_t caller_is_worker(const AzRev1655FilterConsumer *consumer)
{
    uint32_t current_thread;

    if (consumer == NULL || consumer->bound == 0u ||
        consumer->worker_thread_id == 0u) {
        return 0u;
    }
    current_thread = consumer->host.current_thread_id(
        consumer->host.context);
    return current_thread == consumer->worker_thread_id ? 1u : 0u;
}

static uint8_t runtime_gates_allow_work(
    const AzRev1655FilterConsumer *consumer)
{
    if (consumer == NULL ||
        consumer->host.worker_affinity_verified(
            consumer->host.context,
            consumer->worker_thread_id) == 0u ||
        consumer->host.coverflow_is_interactive(
            consumer->host.context) == 0u ||
        consumer->host.filter_queue_is_demonstrably_idle(
            consumer->host.context) == 0u) {
        return 0u;
    }
    return 1u;
}

static void *validated_gcm(AzRev1655FilterConsumer *consumer)
{
    void *gcm;

    if (consumer == NULL) {
        return NULL;
    }
    gcm = consumer->host.gcm_singleton(consumer->host.context);
    if (range_is_valid(consumer, gcm, AZ_REV1655_GCM_REQUIRED_SPAN) == 0u) {
        return NULL;
    }
    return gcm;
}

static void *validated_registry(AzRev1655FilterConsumer *consumer)
{
    void *registry;

    if (consumer == NULL) {
        return NULL;
    }
    registry = consumer->host.registry_singleton(consumer->host.context);
    if (range_is_valid(consumer, registry, sizeof(uint32_t)) == 0u) {
        return NULL;
    }
    return registry;
}

static AzRev1655FilterConsumerResult apply_held_filter(
    AzRev1655FilterConsumer *consumer)
{
    AzRev1655ActiveAggregateD0 snapshot;
    AzRev1655AuroraString selected_host;
    AzNameFilterClassification classification;
    AzNameFilterClassification final_classification;
    const char *selected_method;
    void *gcm;
    void *registry;
    size_t selected_length;
    int32_t schedule_result = 1;
    uint8_t snapshot_live = 0u;
    uint8_t selected_live = 0u;
    uint8_t select_all;
    AzRev1655FilterConsumerResult result =
        AZ_REV1655_FILTER_CONSUMER_MUTATION_FAILED;

    if (runtime_gates_allow_work(consumer) == 0u) {
        ++consumer->deferred_count;
        return AZ_REV1655_FILTER_CONSUMER_DEFERRED;
    }
    if (consumer->held_filter_index >= AZ_GLYPH_COUNT) {
        consumer->disabled = 1u;
        return AZ_REV1655_FILTER_CONSUMER_BAD_FILTER_INDEX;
    }

    select_all = consumer->held_filter_index == AZ_FILTER_ALL_INDEX ? 1u : 0u;
    selected_method = select_all != 0u ? NULL :
        az_filter_method_for_index(consumer->held_filter_index);
    selected_length = select_all != 0u ? 0u : bounded_cstring_length(
        selected_method, AZ_REV1655_FILTER_MAX_TEXT);
    if (select_all == 0u &&
        (selected_method == NULL || selected_length == 0u ||
         selected_length > AZ_REV1655_FILTER_MAX_TEXT ||
         selected_length > UINT32_MAX)) {
        consumer->disabled = 1u;
        return AZ_REV1655_FILTER_CONSUMER_BAD_FILTER_INDEX;
    }

    registry = validated_registry(consumer);
    if (registry == NULL || (select_all == 0u &&
        consumer->host.registry_lookup(
            consumer->host.context,
            registry,
            0u,
            selected_method) == 0u)) {
        consumer->disabled = 1u;
        return AZ_REV1655_FILTER_CONSUMER_REGISTRY_MISSING;
    }

    gcm = validated_gcm(consumer);
    if (gcm == NULL) {
        consumer->disabled = 1u;
        return AZ_REV1655_FILTER_CONSUMER_SNAPSHOT_INVALID;
    }

    if (consumer->host.copy_active_aggregate(
            consumer->host.context,
            &snapshot,
            (const uint8_t *)gcm + AZ_REV1655_GCM_COPY_SOURCE_OFFSET,
            0u) == 0u) {
        consumer->disabled = 1u;
        return AZ_REV1655_FILTER_CONSUMER_SNAPSHOT_INVALID;
    }
    snapshot_live = 1u;

    if (aggregate_is_valid(consumer, &snapshot) == 0u) {
        consumer->disabled = 1u;
        result = AZ_REV1655_FILTER_CONSUMER_SNAPSHOT_INVALID;
        goto cleanup;
    }

    classify_name_filters(consumer, &snapshot.work, &classification);
    if (classification.ambiguous != 0u) {
        result = AZ_REV1655_FILTER_CONSUMER_AMBIGUOUS_NAME_FILTER;
        goto cleanup;
    }

    if (select_all != 0u) {
        if (classification.raw_count == 1u &&
            classification.compiled_count == 0u) {
            if (consumer->host.vector_erase(
                    consumer->host.context,
                    &snapshot.work.additional_filter_ids,
                    classification.raw_vector_index) == 0u) {
                result = AZ_REV1655_FILTER_CONSUMER_MUTATION_FAILED;
                goto cleanup;
            }
        }
        else if (classification.compiled_count == 1u &&
                 classification.raw_count == 0u) {
            if (remove_compiled_leaf(
                    consumer,
                    &snapshot.work,
                    &classification) == 0u) {
                result = AZ_REV1655_FILTER_CONSUMER_MUTATION_FAILED;
                goto cleanup;
            }
        }
        else if (classification.raw_count != 0u ||
                 classification.compiled_count != 0u) {
            result = AZ_REV1655_FILTER_CONSUMER_AMBIGUOUS_NAME_FILTER;
            goto cleanup;
        }
    }
    else if (classification.raw_count == 0u &&
        classification.compiled_count == 0u) {
        if (consumer->host.string_construct_cstring(
                consumer->host.context,
                &selected_host,
                selected_method) == 0u) {
            result = AZ_REV1655_FILTER_CONSUMER_MUTATION_FAILED;
            goto cleanup;
        }
        selected_live = 1u;
        if (consumer->host.vector_push_back(
                consumer->host.context,
                &snapshot.work.additional_filter_ids,
                &selected_host) == 0u) {
            result = AZ_REV1655_FILTER_CONSUMER_MUTATION_FAILED;
            goto cleanup;
        }
    }
    else if (classification.raw_count == 1u &&
             classification.compiled_count == 0u &&
             classification.raw_element != NULL) {
        if (consumer->host.string_assign_bytes(
                consumer->host.context,
                classification.raw_element,
                selected_method,
                (uint32_t)selected_length) == 0u) {
            result = AZ_REV1655_FILTER_CONSUMER_MUTATION_FAILED;
            goto cleanup;
        }
    }
    else if (classification.compiled_count == 1u &&
             classification.raw_count == 0u) {
        if (replace_compiled_leaf(
                consumer,
                &snapshot.work,
                &classification,
                selected_method) == 0u) {
            result = AZ_REV1655_FILTER_CONSUMER_MUTATION_FAILED;
            goto cleanup;
        }
    }
    else {
        result = AZ_REV1655_FILTER_CONSUMER_AMBIGUOUS_NAME_FILTER;
        goto cleanup;
    }

    if (aggregate_is_valid(consumer, &snapshot) == 0u) {
        consumer->disabled = 1u;
        result = AZ_REV1655_FILTER_CONSUMER_SNAPSHOT_INVALID;
        goto cleanup;
    }
    classify_name_filters(consumer, &snapshot.work, &final_classification);
    if ((select_all != 0u &&
         (final_classification.ambiguous != 0u ||
          final_classification.raw_count != 0u ||
          final_classification.compiled_count != 0u)) ||
        (select_all == 0u && classification_is_selected(
            &final_classification,
            consumer->held_filter_index) == 0u)) {
        result = AZ_REV1655_FILTER_CONSUMER_MUTATION_FAILED;
        goto cleanup;
    }

    schedule_result = consumer->host.schedule_filter(
        consumer->host.context,
        (uint8_t *)gcm + AZ_REV1655_GCM_QUEUE_OFFSET,
        &snapshot.context,
        &snapshot.work,
        AZ_REV1655_FILTER_FLAGS_ADDITIONAL);
    result = schedule_result == 0 ?
        AZ_REV1655_FILTER_CONSUMER_SCHEDULED :
        AZ_REV1655_FILTER_CONSUMER_SCHEDULE_FAILED;

cleanup:
    if (selected_live != 0u) {
        consumer->host.string_destroy(
            consumer->host.context, &selected_host);
    }
    if (snapshot_live != 0u) {
        consumer->host.destroy_active_aggregate(
            consumer->host.context, &snapshot);
    }
    return result;
}

static void note_completed_result(
    AzRev1655FilterConsumer *consumer,
    AzRev1655FilterConsumerResult result)
{
    if (consumer == NULL) {
        return;
    }
    if (result == AZ_REV1655_FILTER_CONSUMER_SCHEDULED) {
        ++consumer->scheduled_count;
    }
    else if (result != AZ_REV1655_FILTER_CONSUMER_IDLE &&
             result != AZ_REV1655_FILTER_CONSUMER_DEFERRED) {
        ++consumer->rejected_count;
    }
}

void az_rev1655_filter_consumer_exact_entrypoints(
    AzRev1655FilterEntrypoints *entrypoints)
{
    if (entrypoints == NULL) {
        return;
    }
    entrypoints->gcm_singleton = AZ_REV1655_GCM_SINGLETON_ADDRESS;
    entrypoints->copy_active_aggregate = AZ_REV1655_COPY_ACTIVE_ADDRESS;
    entrypoints->destroy_active_aggregate = AZ_REV1655_DESTROY_ACTIVE_ADDRESS;
    entrypoints->string_construct_cstring =
        AZ_REV1655_STRING_CONSTRUCT_ADDRESS;
    entrypoints->string_assign_bytes = AZ_REV1655_STRING_ASSIGN_ADDRESS;
    entrypoints->string_lifecycle = AZ_REV1655_STRING_LIFECYCLE_ADDRESS;
    entrypoints->vector_push_back = AZ_REV1655_VECTOR_PUSH_ADDRESS;
    entrypoints->registry_singleton = AZ_REV1655_REGISTRY_SINGLETON_ADDRESS;
    entrypoints->registry_lookup = AZ_REV1655_REGISTRY_LOOKUP_ADDRESS;
    entrypoints->scheduler = AZ_REV1655_SCHEDULER_ADDRESS;
}

void az_rev1655_filter_consumer_exact_provenance(
    AzRev1655FilterProvenance *provenance)
{
    if (provenance == NULL) {
        return;
    }
    memcpy(
        provenance->aurora_xex_sha256,
        k_rev1655_xex_sha256,
        sizeof(k_rev1655_xex_sha256));
    memcpy(
        provenance->extracted_pe_sha256,
        k_rev1655_pe_sha256,
        sizeof(k_rev1655_pe_sha256));
}

static AzRev1655FilterConsumerResult bind_with_import_resolver(
    AzRev1655FilterConsumer *consumer,
    const AzRev1655LoadedImage *image,
    const AzRev1655ImportResolver *import_resolver,
    const AzRev1655FilterProvenance *provenance,
    uint32_t worker_thread_id,
    const AzRev1655FilterHostOps *host)
{
    const AzRev1655HookPermit *permit = NULL;

    if (consumer == NULL) {
        return AZ_REV1655_FILTER_CONSUMER_NULL_ARGUMENT;
    }
    memset(consumer, 0, sizeof(*consumer));
    consumer->held_filter_index = AZ_NO_GLYPH;

    if (image == NULL || provenance == NULL || host == NULL ||
        worker_thread_id == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_NULL_ARGUMENT;
    }
    if (bytes_equal(
            provenance->aurora_xex_sha256,
            k_rev1655_xex_sha256,
            sizeof(k_rev1655_xex_sha256)) == 0u ||
        bytes_equal(
            provenance->extracted_pe_sha256,
            k_rev1655_pe_sha256,
            sizeof(k_rev1655_pe_sha256)) == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_BAD_PROVENANCE;
    }
    if (az_rev1655_hook_gate_validate_with_import_resolver(
            image, import_resolver, &permit) !=
            AZ_REV1655_HOOK_GATE_OK || permit == NULL) {
        return AZ_REV1655_FILTER_CONSUMER_BAD_IMAGE;
    }
    if (signatures_match(image) == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_BAD_HELPER_SIGNATURE;
    }
    if (host_callbacks_are_complete(host) == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_BAD_BINDINGS;
    }

    consumer->host = *host;
    consumer->worker_thread_id = worker_thread_id;
    consumer->bound = 1u;
    return AZ_REV1655_FILTER_CONSUMER_IDLE;
}

AzRev1655FilterConsumerResult az_rev1655_filter_consumer_bind(
    AzRev1655FilterConsumer *consumer,
    const AzRev1655LoadedImage *image,
    const AzRev1655FilterProvenance *provenance,
    uint32_t worker_thread_id,
    const AzRev1655FilterHostOps *host)
{
    return bind_with_import_resolver(
        consumer,
        image,
        NULL,
        provenance,
        worker_thread_id,
        host);
}

AzRev1655FilterConsumerResult
az_rev1655_filter_consumer_bind_with_import_resolver(
    AzRev1655FilterConsumer *consumer,
    const AzRev1655LoadedImage *image,
    const AzRev1655ImportResolver *import_resolver,
    const AzRev1655FilterProvenance *provenance,
    uint32_t worker_thread_id,
    const AzRev1655FilterHostOps *host)
{
    return bind_with_import_resolver(
        consumer,
        image,
        import_resolver,
        provenance,
        worker_thread_id,
        host);
}

AzRev1655FilterConsumerResult
az_rev1655_filter_consumer_bind_with_validated_permit(
    AzRev1655FilterConsumer *consumer,
    const AzRev1655LoadedImage *image,
    const AzRev1655HookPermit *permit,
    const AzRev1655FilterProvenance *provenance,
    uint32_t worker_thread_id,
    const AzRev1655FilterHostOps *host)
{
    if (consumer == NULL) {
        return AZ_REV1655_FILTER_CONSUMER_NULL_ARGUMENT;
    }
    memset(consumer, 0, sizeof(*consumer));
    consumer->held_filter_index = AZ_NO_GLYPH;

    if (image == NULL || permit == NULL || provenance == NULL ||
        host == NULL || worker_thread_id == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_NULL_ARGUMENT;
    }
    if (bytes_equal(
            provenance->aurora_xex_sha256,
            k_rev1655_xex_sha256,
            sizeof(k_rev1655_xex_sha256)) == 0u ||
        bytes_equal(
            provenance->extracted_pe_sha256,
            k_rev1655_pe_sha256,
            sizeof(k_rev1655_pe_sha256)) == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_BAD_PROVENANCE;
    }
    if (az_rev1655_hook_gate_permit_matches_image(permit, image) == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_BAD_IMAGE;
    }
    if (signatures_match(image) == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_BAD_HELPER_SIGNATURE;
    }
    if (host_callbacks_are_complete(host) == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_BAD_BINDINGS;
    }

    consumer->host = *host;
    consumer->worker_thread_id = worker_thread_id;
    consumer->bound = 1u;
    return AZ_REV1655_FILTER_CONSUMER_IDLE;
}

AzRev1655FilterConsumerResult az_rev1655_filter_consumer_worker_probe(
    AzRev1655FilterConsumer *consumer)
{
    AzRev1655ActiveAggregateD0 snapshot;
    void *gcm;
    void *registry;
    uint8_t index;

    if (consumer == NULL) {
        return AZ_REV1655_FILTER_CONSUMER_NULL_ARGUMENT;
    }
    if (consumer->bound == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_NOT_BOUND;
    }
    if (caller_is_worker(consumer) == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_NOT_WORKER;
    }
    if (consumer->disabled != 0u) {
        return AZ_REV1655_FILTER_CONSUMER_DISABLED;
    }
    if (runtime_gates_allow_work(consumer) == 0u) {
        ++consumer->deferred_count;
        return AZ_REV1655_FILTER_CONSUMER_DEFERRED;
    }

    registry = validated_registry(consumer);
    if (registry == NULL) {
        consumer->disabled = 1u;
        return AZ_REV1655_FILTER_CONSUMER_REGISTRY_MISSING;
    }
    for (index = AZ_FILTER_OTHER_INDEX;
         index < AZ_GLYPH_COUNT;
         ++index) {
        const char *identifier = az_filter_method_for_index(index);
        if (identifier == NULL || consumer->host.registry_lookup(
                consumer->host.context,
                registry,
                0u,
                identifier) == 0u) {
            consumer->disabled = 1u;
            return AZ_REV1655_FILTER_CONSUMER_REGISTRY_MISSING;
        }
    }

    gcm = validated_gcm(consumer);
    if (gcm == NULL || consumer->host.copy_active_aggregate(
            consumer->host.context,
            &snapshot,
            (const uint8_t *)gcm + AZ_REV1655_GCM_COPY_SOURCE_OFFSET,
            0u) == 0u) {
        consumer->disabled = 1u;
        return AZ_REV1655_FILTER_CONSUMER_SNAPSHOT_INVALID;
    }

    if (aggregate_is_valid(consumer, &snapshot) == 0u) {
        consumer->host.destroy_active_aggregate(
            consumer->host.context, &snapshot);
        consumer->disabled = 1u;
        return AZ_REV1655_FILTER_CONSUMER_SNAPSHOT_INVALID;
    }
    consumer->host.destroy_active_aggregate(
        consumer->host.context, &snapshot);
    consumer->runtime_verified = 1u;
    ++consumer->probe_count;
    return AZ_REV1655_FILTER_CONSUMER_IDLE;
}

AzRev1655FilterConsumerResult az_rev1655_filter_consumer_worker_step(
    AzRev1655FilterConsumer *consumer)
{
    AzRev1655FilterConsumerResult result;
    AzInputDetourResult input_result;
    uint8_t filter_index = AZ_NO_GLYPH;

    if (consumer == NULL) {
        return AZ_REV1655_FILTER_CONSUMER_NULL_ARGUMENT;
    }
    if (consumer->bound == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_NOT_BOUND;
    }
    if (caller_is_worker(consumer) == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_NOT_WORKER;
    }
    if (consumer->disabled != 0u) {
        return AZ_REV1655_FILTER_CONSUMER_DISABLED;
    }
    if (consumer->runtime_verified == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_NOT_RUNTIME_VERIFIED;
    }

    if (consumer->request_held == 0u) {
        input_result = consumer->host.take_filter_request(
            consumer->host.context, &filter_index);
        if (input_result == AZ_INPUT_DETOUR_NO_FILTER) {
            return AZ_REV1655_FILTER_CONSUMER_IDLE;
        }
        if (input_result == AZ_INPUT_DETOUR_FILTER_BUSY) {
            return AZ_REV1655_FILTER_CONSUMER_INPUT_BUSY;
        }
        if (input_result != AZ_INPUT_DETOUR_OK) {
            return AZ_REV1655_FILTER_CONSUMER_INPUT_ERROR;
        }
        consumer->request_held = 1u;
        consumer->held_filter_index = filter_index;
    }

    result = apply_held_filter(consumer);
    if (result == AZ_REV1655_FILTER_CONSUMER_DEFERRED) {
        return result;
    }

    consumer->host.finish_filter_request(consumer->host.context);
    consumer->request_held = 0u;
    consumer->held_filter_index = AZ_NO_GLYPH;
    note_completed_result(consumer, result);
    return result;
}

AzRev1655FilterConsumerResult az_rev1655_filter_consumer_worker_cancel(
    AzRev1655FilterConsumer *consumer)
{
    if (consumer == NULL) {
        return AZ_REV1655_FILTER_CONSUMER_NULL_ARGUMENT;
    }
    if (consumer->bound == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_NOT_BOUND;
    }
    if (caller_is_worker(consumer) == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_NOT_WORKER;
    }
    if (consumer->request_held == 0u) {
        return AZ_REV1655_FILTER_CONSUMER_IDLE;
    }

    consumer->host.finish_filter_request(consumer->host.context);
    consumer->request_held = 0u;
    consumer->held_filter_index = AZ_NO_GLYPH;
    ++consumer->rejected_count;
    return AZ_REV1655_FILTER_CONSUMER_CANCELLED;
}

void az_rev1655_filter_consumer_snapshot_status(
    const AzRev1655FilterConsumer *consumer,
    AzRev1655FilterConsumerStatus *status)
{
    if (consumer == NULL || status == NULL) {
        return;
    }
    status->worker_thread_id = consumer->worker_thread_id;
    status->probe_count = consumer->probe_count;
    status->scheduled_count = consumer->scheduled_count;
    status->rejected_count = consumer->rejected_count;
    status->deferred_count = consumer->deferred_count;
    status->bound = consumer->bound;
    status->runtime_verified = consumer->runtime_verified;
    status->disabled = consumer->disabled;
    status->request_held = consumer->request_held;
    status->held_filter_index = consumer->held_filter_index;
}

const char *az_rev1655_filter_consumer_result_name(
    AzRev1655FilterConsumerResult result)
{
    switch (result) {
    case AZ_REV1655_FILTER_CONSUMER_SCHEDULED:
        return "scheduled";
    case AZ_REV1655_FILTER_CONSUMER_IDLE:
        return "idle";
    case AZ_REV1655_FILTER_CONSUMER_DEFERRED:
        return "deferred";
    case AZ_REV1655_FILTER_CONSUMER_NOT_WORKER:
        return "not-worker";
    case AZ_REV1655_FILTER_CONSUMER_NOT_BOUND:
        return "not-bound";
    case AZ_REV1655_FILTER_CONSUMER_NOT_RUNTIME_VERIFIED:
        return "not-runtime-verified";
    case AZ_REV1655_FILTER_CONSUMER_DISABLED:
        return "disabled";
    case AZ_REV1655_FILTER_CONSUMER_NULL_ARGUMENT:
        return "null-argument";
    case AZ_REV1655_FILTER_CONSUMER_BAD_PROVENANCE:
        return "bad-provenance";
    case AZ_REV1655_FILTER_CONSUMER_BAD_IMAGE:
        return "bad-image";
    case AZ_REV1655_FILTER_CONSUMER_BAD_HELPER_SIGNATURE:
        return "bad-helper-signature";
    case AZ_REV1655_FILTER_CONSUMER_BAD_BINDINGS:
        return "bad-bindings";
    case AZ_REV1655_FILTER_CONSUMER_INPUT_BUSY:
        return "input-busy";
    case AZ_REV1655_FILTER_CONSUMER_INPUT_ERROR:
        return "input-error";
    case AZ_REV1655_FILTER_CONSUMER_BAD_FILTER_INDEX:
        return "bad-filter-index";
    case AZ_REV1655_FILTER_CONSUMER_REGISTRY_MISSING:
        return "registry-missing";
    case AZ_REV1655_FILTER_CONSUMER_SNAPSHOT_INVALID:
        return "snapshot-invalid";
    case AZ_REV1655_FILTER_CONSUMER_AMBIGUOUS_NAME_FILTER:
        return "ambiguous-name-filter";
    case AZ_REV1655_FILTER_CONSUMER_MUTATION_FAILED:
        return "mutation-failed";
    case AZ_REV1655_FILTER_CONSUMER_SCHEDULE_FAILED:
        return "schedule-failed";
    case AZ_REV1655_FILTER_CONSUMER_CANCELLED:
        return "cancelled";
    default:
        return "unknown-filter-consumer-result";
    }
}
