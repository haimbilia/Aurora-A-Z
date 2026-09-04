#if !defined(AURORAAZ_XBOX360)
#error "rev1655_runtime.c must only be built for the Xbox 360 target"
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <xecore/xam.h>
#include <xecore/xboxkrnl.h>

#include <auroraaz/compatibility.h>
#include <auroraaz/browse_consumer_rev1655.h>
#include <auroraaz/content_launch_detour.h>
#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
#include <auroraaz/filter_consumer_xbox360.h>
#endif
#include <auroraaz/hook_runtime.h>
#include <auroraaz/image.h>
#include <auroraaz/input_detour.h>
#include <auroraaz/m2a_input_telemetry.h>
#include <auroraaz/module_settings_detour.h>
#include <auroraaz/netdbg_bootstrap.h>
#include <auroraaz/netdbg_lifetime_rev1655.h>
#include <auroraaz/overlay_renderer_xbox360.h>
#include <auroraaz/operation_mode.h>
#include <auroraaz/render_detours.h>
#include <auroraaz/rev1655_hook_gate.h>
#include <auroraaz/rev1655_runtime.h>
#include <auroraaz/scene_gate_rev1655.h>

#include "rev1655_hook_gate_private.h"
#include "rev1655_thread_private.h"

#define AZ_IMAGE_VALIDATION_STRIDE 0x1000u
#define AZ_REV1655_PE_HEADER_SIZE 0x400u
#define AZ_M2A_INPUT_TARGET_ADDRESS 0x82801D90u
#define AZ_M2B_RENDER_MENU_TARGET_ADDRESS 0x82358A08u
#define AZ_M2B_FONT_END_TARGET_ADDRESS 0x8247E390u
#define AZ_CONTENT_LAUNCH_TARGET_ADDRESS 0x82294DD0u
#define AZ_MODULE_SETTINGS_TARGET_ADDRESS 0x822C8B88u
#define AZ_XUI_ELEMENT_GET_FIRST_CHILD_ORDINAL 811u
#define AZ_XUI_ELEMENT_GET_NEXT_ORDINAL 816u
#define AZ_XUI_TEXT_ELEMENT_SET_TEXT_ORDINAL 867u
#define AZ_XUI_TEXT_ELEMENT_GET_TEXT_ORDINAL 870u
#define AZ_XUI_IMAGE_ELEMENT_SET_IMAGE_PATH_ORDINAL 915u
#define AZ_XUI_ELEMENT_HAS_FOCUS_ORDINAL 921u
#define AZ_XUI_ELEMENT_GET_DESCENDANT_BY_ID_ORDINAL 2111u
#define AZ_SCENE_CACHE_HEAD_ADDRESS 0x82BC006Cu
#define AZ_ICON_UI_TICK_INTERVAL 30u
#define AZ_BROWSE_LIST_MANAGER_SINGLETON_ADDRESS 0x8223FF30u
#define AZ_BROWSE_LIST_LOOKUP_ADDRESS 0x8223FFB0u
#define AZ_BROWSE_GCM_SINGLETON_ADDRESS 0x82223060u
#define AZ_BROWSE_POPULATE_ADDRESS 0x8234C698u
#define AZ_BROWSE_ACTIVE_MAP_OFFSET 0x74u
#define AZ_BROWSE_HELPER_OFFSET 0x550u
#define AZ_BROWSE_CURRENT_INDEX_OFFSET 0x584u
#define AZ_BROWSE_LAYOUT_OFFSET 0x21B8u
#define AZ_BROWSE_ENABLED_OFFSET 0x225Cu
#define AZ_BROWSE_BLOCKED_OFFSET 0x225Eu
#define AZ_BROWSE_READY_OFFSET 0x225Fu
#define AZ_BROWSE_HELPER_PREVIOUS_INDEX_OFFSET 0x30u
#define AZ_BROWSE_HELPER_CURRENT_INDEX_OFFSET 0x34u
#define AZ_BROWSE_HELPER_OWNER_CALLBACK_OFFSET 0x10u
#define AZ_INPUT_SIGNATURE_SIZE 20u
#define AZ_RENDER_SIGNATURE_SIZE 16u
#define AZ_OBSERVATION_DRAIN_BUDGET 8u
#define AZ_TELEMETRY_FLUSH_TICKS 5u
#define AZ_RENDER_TELEMETRY_FLUSH_TICKS 100u
#define AZ_WORKER_INTERVAL_100NS (-500000LL)
#define AZ_CONTROL_WAIT_100NS (-10000LL)

#define AZ_LIFETIME_UNPINNED 0u
#define AZ_LIFETIME_PINNING 1u
#define AZ_LIFETIME_PINNED 2u
#define AZ_LIFETIME_FAILED 3u
#define AZ_REV1655_IMPORT_LIBRARY_COUNT 2u
#define AZ_FILTER_THREAD_WAIT_TICKS 2000u
#define AZ_FILTER_APPLY_UNARMED 0u
#define AZ_FILTER_APPLY_ARMED 1u
#define AZ_FILTER_APPLY_COOLDOWN 2u
#define AZ_FILTER_APPLY_FAILED 3u
#define AZ_FILTER_FALLBACK_COOLDOWN_TICKS 160u
#define AZ_FILTER_IDLE_STABLE_TICKS 4u
#define AZ_M3C_TELEMETRY_FLUSH_TICKS 20u

#define AZ_BROWSE_APPLY_NONE 0u
#define AZ_BROWSE_APPLY_BAD_PUBLICATION 1u
#define AZ_BROWSE_APPLY_BAD_GCM 2u
#define AZ_BROWSE_APPLY_STOCK_GATE 3u
#define AZ_BROWSE_APPLY_BAD_LAYOUT 4u
#define AZ_BROWSE_APPLY_BAD_HELPER 5u
#define AZ_BROWSE_APPLY_ALREADY_SELECTED 6u
#define AZ_BROWSE_APPLY_MOVE_REJECTED 7u
#define AZ_BROWSE_APPLY_MOVED 8u

#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
typedef int32_t (*AzXuiGetChildFn)(uint32_t, uint32_t *);
typedef int32_t (*AzXuiGetDescendantFn)(
    uint32_t, const uint16_t *, uint32_t *);
typedef int32_t (*AzXuiGetTextFn)(uint32_t, const uint16_t **);
typedef int32_t (*AzXuiSetTextFn)(uint32_t, const uint16_t *);
typedef int32_t (*AzXuiSetImagePathFn)(uint32_t, const uint16_t *);
typedef int32_t (*AzXuiHasFocusFn)(uint32_t);
#endif

#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
#define AZ_FILTER_GCM_SINGLETON_ADDRESS 0x82223060u
#define AZ_FILTER_COPY_ACTIVE_ADDRESS 0x8222B3E8u
#define AZ_FILTER_DESTROY_ACTIVE_ADDRESS 0x8222B6C8u
#define AZ_FILTER_STRING_CONSTRUCT_ADDRESS 0x82212CE8u
#define AZ_FILTER_STRING_ASSIGN_ADDRESS 0x82212DB0u
#define AZ_FILTER_STRING_LIFECYCLE_ADDRESS 0x82213580u
#define AZ_FILTER_VECTOR_PUSH_ADDRESS 0x822A6228u
#define AZ_FILTER_REGISTRY_SINGLETON_ADDRESS 0x82271000u
#define AZ_FILTER_REGISTRY_LOOKUP_ADDRESS 0x82324C60u
#define AZ_FILTER_SCHEDULER_ADDRESS 0x82343628u
#define AZ_FILTER_QUEUE_BEGIN_OFFSET 0x28u
#define AZ_FILTER_QUEUE_END_OFFSET 0x2Cu
#define AZ_FILTER_QUEUE_CAPACITY_OFFSET 0x30u
#define AZ_FILTER_WORKER_BUSY_OFFSET 0x2D8u
#endif

typedef char AzM2aInputContinuationMustMatch[
    AZ_M2A_INPUT_TARGET_ADDRESS + 4u ==
        AZ_REV1655_INPUT_WRAPPER_CONTINUE_ADDRESS ? 1 : -1];

typedef struct AzRev1655Runtime {
    AzHookArena arena;
    AzLiveHook input_hook;
    AzLiveHook render_menu_hook;
    AzLiveHook font_end_hook;
    AzLiveHook content_launch_hook;
    AzLiveHook module_settings_hook;
    AzOverlayRenderer renderer;
    HANDLE worker_thread;
    volatile uint32_t state;
    volatile uint32_t stage;
    volatile uint32_t observations_logged;
    volatile uint32_t lifetime_state;
    volatile uint32_t pinned_ordinal4_export;
    volatile uint32_t netdbg_wrapper_address;
    volatile uint32_t shutdown_requests;
#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
    volatile uint32_t operation_mode;
    volatile uint32_t browse_bind_result;
    volatile uint32_t browse_step_result;
    volatile uint32_t browse_apply_reason;
    volatile uint32_t browse_apply_target;
    volatile uint32_t browse_apply_current;
    volatile uint32_t browse_apply_count;
    AzRev1655BrowseConsumer browse_consumer;
    uint8_t settings_dialog_active;
    uint8_t settings_a_owned;
    volatile uint32_t settings_resolve_result;
    volatile uint32_t settings_call_result;
    volatile uint32_t settings_completions;
    volatile uint32_t module_label_result;
    volatile uint32_t icon_cache_result;
    volatile uint32_t settings_xur_cache_result;
    volatile uint32_t icon_apply_result;
    volatile uint32_t icon_ui_ticks;
    AzXuiGetChildFn xui_get_first_child;
    AzXuiGetChildFn xui_get_next;
    AzXuiGetDescendantFn xui_get_descendant;
    AzXuiGetTextFn xui_get_text;
    AzXuiSetTextFn xui_set_text;
    AzXuiSetImagePathFn xui_set_image_path;
    AzXuiHasFocusFn xui_has_focus;
    volatile uint32_t worker_thread_id;
    volatile uint32_t filter_bind_result;
    volatile uint32_t filter_probe_result;
    volatile uint32_t filter_step_result;
    volatile uint32_t filter_apply_state;
    uint32_t filter_cooldown_ticks;
    uint32_t filter_idle_ticks;
    uint32_t filter_activity_seen;
    AzRev1655FilterConsumer filter_consumer;
#endif
    AzM2aInputTelemetry telemetry;
    uint32_t telemetry_flush_ticks;
    uint32_t render_telemetry_flush_ticks;
    uint32_t m3c_telemetry_flush_ticks;
} AzRev1655Runtime;

typedef struct AzM2bRenderMarker {
    uint8_t magic[4];
    uint32_t version;
    uint32_t record_size;
    uint32_t stage;
    uint32_t state;
    uint32_t render_menu_target;
    uint32_t font_end_target;
    uint32_t render_menu_calls;
    uint32_t font_end_calls;
    uint32_t last_render_result;
    uint32_t last_note_result;
    uint32_t last_draw_result;
    uint32_t scene_probes;
    uint32_t scene_allowed;
    uint32_t scene_denied;
    uint32_t last_scene_reason;
    uint32_t scene_configured;
    uint32_t renderer_validated;
} AzM2bRenderMarker;

typedef char AzM2bRenderMarkerMustBe72Bytes[
    (sizeof(AzM2bRenderMarker) == 72u) ? 1 : -1];

#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
typedef struct AzM3bFilterMarker {
    uint8_t magic[4];
    uint32_t version;
    uint32_t record_size;
    uint32_t worker_thread_id;
    uint32_t bind_result;
    uint32_t probe_result;
    uint32_t probe_count;
    uint32_t runtime_verified;
    uint32_t disabled;
    uint32_t worker_step_result;
    uint32_t scheduled_count;
    uint32_t rejected_count;
    uint32_t deferred_count;
    uint32_t apply_state;
    uint32_t filter_gate_published;
    uint32_t cooldown_ticks;
    uint32_t idle_ticks;
    uint32_t activity_seen;
    uint32_t shutdown_requests;
    uint32_t runtime_state;
} AzM3bFilterMarker;

typedef char AzM3bFilterMarkerMustBe80Bytes[
    (sizeof(AzM3bFilterMarker) == 80u) ? 1 : -1];

typedef struct AzM3cBrowseSettingsMarker {
    uint8_t magic[4];
    uint32_t version;
    uint32_t record_size;
    uint32_t operation_mode;
    uint32_t browse_bind_result;
    uint32_t browse_step_result;
    uint32_t consumer_queued;
    uint32_t consumer_no_match;
    uint32_t consumer_rejected;
    uint32_t input_queued;
    uint32_t input_applied;
    uint32_t input_rejected;
    uint32_t input_pending;
    uint32_t input_in_flight;
    uint32_t apply_reason;
    uint32_t apply_target;
    uint32_t apply_current;
    uint32_t apply_count;
    uint32_t label_result;
    uint32_t settings_hook_calls;
    uint32_t settings_requests_taken;
    uint32_t settings_pending;
    uint32_t settings_resolve_result;
    uint32_t settings_call_result;
    uint32_t settings_completions;
    uint32_t settings_dialog_active;
    uint32_t runtime_state;
    uint32_t shutdown_requests;
    uint32_t icon_cache_result;
    uint32_t icon_apply_result;
} AzM3cBrowseSettingsMarker;

typedef char AzM3cBrowseSettingsMarkerMustBe120Bytes[
    (sizeof(AzM3cBrowseSettingsMarker) == 120u) ? 1 : -1];
#endif

static AzRev1655Runtime g_runtime;
static char g_input_telemetry_slot_a_path[] =
    AZ_M2A_INPUT_TELEMETRY_SLOT_A_PATH;
static char g_input_telemetry_slot_b_path[] =
    AZ_M2A_INPUT_TELEMETRY_SLOT_B_PATH;
#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
static char g_render_marker_path[] =
    "game:\\Data\\Logs\\AuroraAZ-M2b.bin";
static char g_filter_marker_path[] =
    "game:\\Data\\Logs\\AuroraAZ-M3b-filter.bin";
static char g_m3c_marker_path[] =
    "game:\\Data\\Logs\\AuroraAZ-M3c-browse-settings.bin";
static char g_operation_mode_path[] =
    "game:\\Data\\AuroraAZ.ini";
static char g_icon_cache_path[] =
    "game:\\Data\\AuroraAZ-icon.png";
static char g_settings_xur_cache_path[] =
    "game:\\Data\\AuroraAZ_Settings.xur";

extern const uint8_t g_auroraaz_embedded_icon_png[];
extern const uint32_t g_auroraaz_embedded_icon_png_size;
extern const uint8_t g_auroraaz_embedded_settings_xur[];
extern const uint32_t g_auroraaz_embedded_settings_xur_size;

/* xecorelib exports ordinal 748 but does not yet declare the prototype. */
extern int32_t XamIsUIActive(void);
/* xecorelib exports ordinal 1040 but does not yet declare the prototype. */
extern uint32_t GetCurrentThreadId(void);
#endif

static uint32_t load_u32(const volatile uint32_t *value)
{
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static void store_u32(volatile uint32_t *value, uint32_t replacement)
{
    __atomic_store_n(value, replacement, __ATOMIC_RELEASE);
}

static void wait_for_control(void)
{
    int64_t interval = AZ_CONTROL_WAIT_100NS;

    (void)KeDelayExecutionThread(0u, 0u, &interval);
}

static void wait_for_input(void)
{
    int64_t interval = AZ_WORKER_INTERVAL_100NS;

    (void)KeDelayExecutionThread(0u, 0u, &interval);
}

static uint8_t write_complete_file(
    char *path,
    const uint8_t *bytes,
    uint32_t size)
{
    HANDLE file;
    uint32_t bytes_written = 0u;
    int write_succeeded;
    int close_succeeded;

    if (path == NULL || bytes == NULL || size == 0u) {
        return 0u;
    }

    file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == NULL || file == INVALID_HANDLE_VALUE) {
        return 0u;
    }

    write_succeeded = WriteFile(
        file,
        (void *)bytes,
        size,
        &bytes_written,
        NULL);
    close_succeeded = CloseHandle(file);
    return write_succeeded != 0 &&
        bytes_written == size &&
        close_succeeded != 0 ? 1u : 0u;
}

#if defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
void az_rev1655_runtime_test_reset_lifetime(void)
{
    store_u32(&g_runtime.pinned_ordinal4_export, 0u);
    store_u32(&g_runtime.netdbg_wrapper_address, 0u);
    store_u32(&g_runtime.lifetime_state, AZ_LIFETIME_UNPINNED);
}

uint8_t az_rev1655_runtime_test_write_complete_file(
    char *path,
    const uint8_t *bytes,
    uint32_t size)
{
    return write_complete_file(path, bytes, size);
}
#endif

static uint8_t read_complete_file(
    char *path,
    uint8_t *bytes,
    uint32_t size)
{
    HANDLE file;
    uint32_t bytes_read = 0u;
    int read_succeeded;
    int close_succeeded;

    if (path == NULL || bytes == NULL || size == 0u) {
        return 0u;
    }

    file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == NULL || file == INVALID_HANDLE_VALUE) {
        return 0u;
    }

    read_succeeded = ReadFile(
        file,
        bytes,
        size,
        &bytes_read,
        NULL);
    close_succeeded = CloseHandle(file);
    return read_succeeded != 0 &&
        bytes_read == size &&
        close_succeeded != 0 ? 1u : 0u;
}

#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
static uint8_t read_operation_mode(AzOperationMode *mode)
{
    uint8_t bytes[AZ_OPERATION_MODE_CONFIG_MAX_SIZE + 1u];
    HANDLE file;
    uint32_t bytes_read = 0u;
    int read_succeeded;
    int close_succeeded;

    if (mode == NULL) {
        return 0u;
    }
    *mode = AZ_OPERATION_MODE_BROWSE;
    file = CreateFileA(
        g_operation_mode_path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == NULL || file == INVALID_HANDLE_VALUE) {
        return 0u;
    }
    read_succeeded = ReadFile(
        file,
        bytes,
        (uint32_t)sizeof(bytes),
        &bytes_read,
        NULL);
    close_succeeded = CloseHandle(file);
    if (read_succeeded == 0 || close_succeeded == 0 ||
        bytes_read > AZ_OPERATION_MODE_CONFIG_MAX_SIZE) {
        return 0u;
    }
    *mode = az_operation_mode_parse(bytes, (size_t)bytes_read);
    return 1u;
}

static uint8_t write_operation_mode(AzOperationMode mode)
{
    char bytes[AZ_OPERATION_MODE_CONFIG_MAX_SIZE];
    size_t size = az_operation_mode_serialize(
        mode, bytes, sizeof(bytes));

    if (size == 0u || size > (size_t)UINT32_MAX) {
        return 0u;
    }
    return write_complete_file(
        g_operation_mode_path,
        (const uint8_t *)bytes,
        (uint32_t)size);
}
#endif

/* Continue the serial space left by an earlier title session. Without this,
 * a valid stale slot with a larger generation could outrank the new run. */
static void seed_input_telemetry_generation(void)
{
    uint8_t slot_a[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint8_t slot_b[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint8_t selected_slot = AZ_M2A_INPUT_TELEMETRY_SLOT_A;
    uint32_t generation = 0u;
    const uint8_t *valid_a = NULL;
    const uint8_t *valid_b = NULL;

    if (read_complete_file(
            g_input_telemetry_slot_a_path,
            slot_a,
            (uint32_t)sizeof(slot_a)) != 0u) {
        valid_a = slot_a;
    }
    if (read_complete_file(
            g_input_telemetry_slot_b_path,
            slot_b,
            (uint32_t)sizeof(slot_b)) != 0u) {
        valid_b = slot_b;
    }

    if (az_m2a_input_telemetry_select_newest_be(
            valid_a,
            valid_a != NULL ? sizeof(slot_a) : 0u,
            valid_b,
            valid_b != NULL ? sizeof(slot_b) : 0u,
            &selected_slot,
            &generation) == AZ_M2A_INPUT_TELEMETRY_OK) {
        (void)selected_slot;
        (void)az_m2a_input_telemetry_seed_generation(
            &g_runtime.telemetry,
            generation);
    }
}

/* Worker-only durable evidence. A failed or torn slot never acknowledges the
 * in-memory revision; the same CRC-protected generation is retried later. */
static void flush_input_telemetry(uint8_t force)
{
    AzInputDetourStatus status;
    uint8_t record[AZ_M2A_INPUT_TELEMETRY_RECORD_SIZE];
    uint32_t revision_token = 0u;
    uint32_t generation = 0u;
    char *path;

    az_rev1655_input_detour_snapshot_status(&status);
    (void)az_m2a_input_telemetry_update_runtime(
        &g_runtime.telemetry,
        1u,
        (AzRev1655RuntimeState)load_u32(&g_runtime.state),
        status.observation_drops);

    if (az_m2a_input_telemetry_is_dirty(&g_runtime.telemetry) == 0u) {
        g_runtime.telemetry_flush_ticks = 0u;
        return;
    }

    if (force == 0u) {
        if (g_runtime.telemetry_flush_ticks < AZ_TELEMETRY_FLUSH_TICKS) {
            ++g_runtime.telemetry_flush_ticks;
        }
        if (g_runtime.telemetry_flush_ticks < AZ_TELEMETRY_FLUSH_TICKS) {
            return;
        }
    }
    g_runtime.telemetry_flush_ticks = 0u;

    if (az_m2a_input_telemetry_snapshot_be(
            &g_runtime.telemetry,
            record,
            sizeof(record),
            &revision_token) != AZ_M2A_INPUT_TELEMETRY_OK ||
        az_m2a_input_telemetry_validate_record_be(
            record,
            sizeof(record),
            &generation) != AZ_M2A_INPUT_TELEMETRY_OK) {
        return;
    }

    path = (generation & 1u) != 0u ?
        g_input_telemetry_slot_a_path :
        g_input_telemetry_slot_b_path;
    if (write_complete_file(
            path,
            record,
            (uint32_t)sizeof(record)) != 0u) {
        (void)az_m2a_input_telemetry_acknowledge(
            &g_runtime.telemetry,
            revision_token);
    }
}

#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
static int overlay_address_is_valid(void *address)
{
    return MmIsAddressValid(address) ? 1 : 0;
}

static int32_t overlay_system_ui_is_active(void)
{
    return XamIsUIActive();
}

/* The scene decision is sampled in the same Font::End callback that draws.
 * It is intentionally local: observe-only input remains unable to consume. */
static void snapshot_input_with_current_scene(AzInputDetourStatus *status)
{
    AzSceneGateDecision decision;
    uint8_t allowed;

    if (status == NULL) {
        return;
    }
    allowed = az_rev1655_scene_gate_probe(&decision) != 0u ? 1u : 0u;
    /* Publish the same final-coverflow decision used for drawing so the next
     * correlated input frame can own selector controls. */
    az_rev1655_input_detour_set_scene_allows_capture(allowed);
    az_rev1655_input_detour_snapshot_status(status);
}

static void flush_render_telemetry(uint8_t force)
{
    AzRenderDetourStatus render_status;
    AzSceneGateStatus scene_status;
    AzM2bRenderMarker marker;

    if (load_u32(&g_runtime.stage) !=
        (uint32_t)AZ_REV1655_RUNTIME_STAGE_OVERLAY_CANARY) {
        return;
    }
    if (force == 0u) {
        if (g_runtime.render_telemetry_flush_ticks <
            AZ_RENDER_TELEMETRY_FLUSH_TICKS) {
            ++g_runtime.render_telemetry_flush_ticks;
        }
        if (g_runtime.render_telemetry_flush_ticks <
            AZ_RENDER_TELEMETRY_FLUSH_TICKS) {
            return;
        }
    }
    g_runtime.render_telemetry_flush_ticks = 0u;

    az_rev1655_render_detours_snapshot_status(&render_status);
    az_rev1655_scene_gate_snapshot_status(&scene_status);
    memset(&marker, 0, sizeof(marker));
    marker.magic[0] = 'A';
    marker.magic[1] = 'Z';
    marker.magic[2] = 'R';
    marker.magic[3] = '2';
    marker.version = 1u;
    marker.record_size = (uint32_t)sizeof(marker);
    marker.stage = load_u32(&g_runtime.stage);
    marker.state = load_u32(&g_runtime.state);
    marker.render_menu_target = AZ_M2B_RENDER_MENU_TARGET_ADDRESS;
    marker.font_end_target = AZ_M2B_FONT_END_TARGET_ADDRESS;
    marker.render_menu_calls = render_status.render_menu_calls;
    marker.font_end_calls = render_status.font_end_calls;
    marker.last_render_result =
        (uint32_t)render_status.last_render_menu_result;
    marker.last_note_result = (uint32_t)render_status.last_note_result;
    marker.last_draw_result = (uint32_t)render_status.last_draw_result;
    marker.scene_probes = scene_status.probes;
    marker.scene_allowed = scene_status.allowed;
    marker.scene_denied = scene_status.denied;
    marker.last_scene_reason = (uint32_t)scene_status.last_reason;
    marker.scene_configured = scene_status.configured;
    marker.renderer_validated = g_runtime.renderer.rev1655_validated;
    (void)write_complete_file(
        g_render_marker_path,
        (const uint8_t *)&marker,
        (uint32_t)sizeof(marker));
}

static void flush_filter_probe_telemetry(void)
{
    AzRev1655FilterConsumerStatus status;
    AzM3bFilterMarker marker;

    memset(&status, 0, sizeof(status));
    memset(&marker, 0, sizeof(marker));
    az_rev1655_filter_consumer_snapshot_status(
        &g_runtime.filter_consumer, &status);
    marker.magic[0] = 'A';
    marker.magic[1] = 'Z';
    marker.magic[2] = 'F';
    marker.magic[3] = '3';
    marker.version = 5u;
    marker.record_size = (uint32_t)sizeof(marker);
    marker.worker_thread_id = load_u32(&g_runtime.worker_thread_id);
    marker.bind_result = load_u32(&g_runtime.filter_bind_result);
    marker.probe_result = load_u32(&g_runtime.filter_probe_result);
    marker.probe_count = status.probe_count;
    marker.runtime_verified = status.runtime_verified;
    marker.disabled = status.disabled;
    marker.worker_step_result = load_u32(&g_runtime.filter_step_result);
    marker.scheduled_count = status.scheduled_count;
    marker.rejected_count = status.rejected_count;
    marker.deferred_count = status.deferred_count;
    marker.apply_state = load_u32(&g_runtime.filter_apply_state);
    marker.filter_gate_published =
        marker.apply_state == AZ_FILTER_APPLY_ARMED ? 1u : 0u;
    marker.cooldown_ticks = g_runtime.filter_cooldown_ticks;
    marker.idle_ticks = g_runtime.filter_idle_ticks;
    marker.activity_seen = g_runtime.filter_activity_seen;
    marker.shutdown_requests = load_u32(&g_runtime.shutdown_requests);
    marker.runtime_state = load_u32(&g_runtime.state);
    (void)write_complete_file(
        g_filter_marker_path,
        (const uint8_t *)&marker,
        (uint32_t)sizeof(marker));
}

static void flush_m3c_telemetry(uint8_t force)
{
    AzM3cBrowseSettingsMarker marker;
    AzInputDetourStatus input;
    AzModuleSettingsDetourStatus settings;

    if (force == 0u) {
        if (g_runtime.m3c_telemetry_flush_ticks <
            AZ_M3C_TELEMETRY_FLUSH_TICKS) {
            ++g_runtime.m3c_telemetry_flush_ticks;
        }
        if (g_runtime.m3c_telemetry_flush_ticks <
            AZ_M3C_TELEMETRY_FLUSH_TICKS) {
            return;
        }
    }
    g_runtime.m3c_telemetry_flush_ticks = 0u;
    memset(&marker, 0, sizeof(marker));
    memset(&input, 0, sizeof(input));
    memset(&settings, 0, sizeof(settings));
    az_rev1655_input_detour_snapshot_status(&input);
    az_module_settings_detour_snapshot_status(&settings);
    marker.magic[0] = 'A';
    marker.magic[1] = 'Z';
    marker.magic[2] = 'C';
    marker.magic[3] = '3';
    marker.version = 1u;
    marker.record_size = (uint32_t)sizeof(marker);
    marker.operation_mode = load_u32(&g_runtime.operation_mode);
    marker.browse_bind_result = load_u32(&g_runtime.browse_bind_result);
    marker.browse_step_result = load_u32(&g_runtime.browse_step_result);
    marker.consumer_queued = g_runtime.browse_consumer.queued_count;
    marker.consumer_no_match = g_runtime.browse_consumer.no_match_count;
    marker.consumer_rejected = g_runtime.browse_consumer.rejected_count;
    marker.input_queued = input.browse_jump_queued;
    marker.input_applied = input.browse_jump_applied;
    marker.input_rejected = input.browse_jump_rejected;
    marker.input_pending = input.browse_jump_pending;
    marker.input_in_flight = input.browse_jump_in_flight;
    marker.apply_reason = load_u32(&g_runtime.browse_apply_reason);
    marker.apply_target = load_u32(&g_runtime.browse_apply_target);
    marker.apply_current = load_u32(&g_runtime.browse_apply_current);
    marker.apply_count = load_u32(&g_runtime.browse_apply_count);
    marker.label_result = load_u32(&g_runtime.module_label_result);
    marker.settings_hook_calls = settings.hook_calls;
    marker.settings_requests_taken = settings.requests_taken;
    marker.settings_pending = settings.pending;
    marker.settings_resolve_result = load_u32(
        &g_runtime.settings_resolve_result);
    marker.settings_call_result = load_u32(
        &g_runtime.settings_call_result);
    marker.settings_completions = load_u32(
        &g_runtime.settings_completions);
    marker.settings_dialog_active = g_runtime.settings_dialog_active;
    marker.runtime_state = load_u32(&g_runtime.state);
    marker.shutdown_requests = load_u32(&g_runtime.shutdown_requests);
    marker.icon_cache_result = load_u32(&g_runtime.icon_cache_result);
    marker.icon_apply_result = load_u32(&g_runtime.icon_apply_result);
    (void)write_complete_file(
        g_m3c_marker_path,
        (const uint8_t *)&marker,
        (uint32_t)sizeof(marker));
}
#endif

#if defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
void az_rev1655_runtime_test_telemetry_init(uint32_t generation)
{
    az_m2a_input_telemetry_init(&g_runtime.telemetry);
    if (generation != 0u) {
        (void)az_m2a_input_telemetry_seed_generation(
            &g_runtime.telemetry,
            generation);
    }
    g_runtime.telemetry_flush_ticks = 0u;
    store_u32(
        &g_runtime.state,
        (uint32_t)AZ_REV1655_RUNTIME_RUNNING);
}

AzM2aInputTelemetryResult az_rev1655_runtime_test_telemetry_record(
    const AzInputDetourObservation *observation)
{
    return az_m2a_input_telemetry_record(
        &g_runtime.telemetry,
        observation);
}

void az_rev1655_runtime_test_telemetry_flush(uint8_t force)
{
    flush_input_telemetry(force);
}

uint8_t az_rev1655_runtime_test_telemetry_is_dirty(void)
{
    return az_m2a_input_telemetry_is_dirty(&g_runtime.telemetry);
}

void az_rev1655_runtime_test_telemetry_finish(void)
{
    store_u32(
        &g_runtime.state,
        (uint32_t)AZ_REV1655_RUNTIME_STOPPED);
}
#endif

/* Validate every page that the exact-image gate will actually read. The
 * loaded PE has intentionally unmapped gaps outside its header and .text;
 * probing the entire SizeOfImage would reject the genuine Rev1655 image. */
static uint8_t image_range_is_mapped(const uint8_t *image, size_t size)
{
    size_t offset;
    const uintptr_t start = (uintptr_t)image;

    if (image == NULL || size == 0u ||
        start > UINTPTR_MAX - (size - 1u)) {
        return 0u;
    }

    for (offset = 0u; offset < size;
         offset += (size_t)AZ_IMAGE_VALIDATION_STRIDE) {
        if (!MmIsAddressValid((void *)(image + offset))) {
            return 0u;
        }
    }

    return MmIsAddressValid((void *)(image + size - 1u)) ? 1u : 0u;
}

typedef struct AzRev1655RuntimeImportResolverContext {
    HMODULE modules[AZ_REV1655_IMPORT_LIBRARY_COUNT];
    uint8_t module_queries[AZ_REV1655_IMPORT_LIBRARY_COUNT];
} AzRev1655RuntimeImportResolverContext;

static const char *import_module_identity(
    AzRev1655ImportLibrary library,
    size_t *module_index)
{
    if (module_index == NULL) {
        return NULL;
    }

    switch (library) {
    case AZ_REV1655_IMPORT_LIBRARY_XAM:
        *module_index = 0u;
        return "xam.xex";
    case AZ_REV1655_IMPORT_LIBRARY_XBOXKRNL:
        *module_index = 1u;
        return "xboxkrnl.exe";
    default:
        return NULL;
    }
}

/* Resolve each frozen import identity from the loader, never from mutable
 * thunk/IAT bytes. Module handles are looked up once per complete validation;
 * each of the 350 callbacks still performs its own ordinal resolution. */
static int resolve_runtime_import_target(
    void *context,
    AzRev1655ImportLibrary library,
    uint16_t ordinal,
    size_t thunk_index,
    uint32_t *out_target)
{
    AzRev1655RuntimeImportResolverContext *resolver_context =
        (AzRev1655RuntimeImportResolverContext *)context;
    const char *identity;
    HMODULE module;
    void *procedure = NULL;
    uintptr_t target;
    size_t module_index = 0u;
    NTSTATUS status;

    (void)thunk_index;
    if (resolver_context == NULL || out_target == NULL) {
        return 0;
    }
    *out_target = 0u;

    identity = import_module_identity(library, &module_index);
    if (identity == NULL ||
        module_index >= (size_t)AZ_REV1655_IMPORT_LIBRARY_COUNT) {
        return 0;
    }

    if (resolver_context->module_queries[module_index] == 0u) {
        resolver_context->module_queries[module_index] = 1u;
        status = XexGetModuleHandle(
            identity,
            &resolver_context->modules[module_index]);
        if (FAILED(status) ||
            resolver_context->modules[module_index] == NULL) {
            resolver_context->modules[module_index] = NULL;
            return 0;
        }
    }

    module = resolver_context->modules[module_index];
    if (module == NULL) {
        return 0;
    }

    status = XexGetProcedureAddress(module, (uint32_t)ordinal, &procedure);
    if (FAILED(status) || procedure == NULL) {
        return 0;
    }

    target = (uintptr_t)procedure;
    if (target > (uintptr_t)(UINT32_MAX - 3u) ||
        (target & (uintptr_t)3u) != (uintptr_t)0u ||
        !MmIsAddressValid((void *)target) ||
        !MmIsAddressValid((void *)(target + (uintptr_t)3u))) {
        return 0;
    }

    *out_target = (uint32_t)target;
    return 1;
}

#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
typedef void *(*AzFilterSingletonFn)(void);
typedef void *(*AzFilterCopyFn)(
    AzRev1655ActiveAggregateD0 *, const void *, uint32_t);
typedef void (*AzFilterDestroyFn)(AzRev1655ActiveAggregateD0 *);
typedef void *(*AzFilterStringConstructFn)(
    AzRev1655AuroraString *, const char *);
typedef void *(*AzFilterStringAssignFn)(
    AzRev1655AuroraString *, const char *, uint32_t);
typedef void (*AzFilterStringLifecycleFn)(
    AzRev1655AuroraString *, uint32_t, uint32_t);
typedef void (*AzFilterVectorPushFn)(
    AzRev1655AuroraStringVector *, const AzRev1655AuroraString *);
typedef int32_t (*AzFilterRegistryLookupFn)(
    void *, uint32_t, const char *);
typedef int32_t (*AzFilterScheduleFn)(
    void *, const AzRev1655FilterContext38 *,
    const AzRev1655FilterWork74 *, uint32_t);
typedef void *(*AzBrowseListLookupFn)(void *, const uint32_t *);
typedef uint32_t (*AzBrowsePopulateFn)(void *, void *, uint32_t);
typedef void (*AzBrowseSelectionChangedFn)(void *, uint32_t, uint32_t);

static uint8_t filter_address_range_is_valid(
    void *context,
    const void *address,
    size_t size)
{
    const uint8_t *bytes = (const uint8_t *)address;
    const uintptr_t start = (uintptr_t)address;
    size_t offset;

    (void)context;
    if (bytes == NULL || size == 0u ||
        start > UINTPTR_MAX - (size - 1u)) {
        return 0u;
    }
    for (offset = 0u; offset < size; offset += AZ_IMAGE_VALIDATION_STRIDE) {
        if (!MmIsAddressValid((void *)(bytes + offset))) {
            return 0u;
        }
    }
    return MmIsAddressValid((void *)(bytes + size - 1u)) ? 1u : 0u;
}

static void *filter_gcm_singleton(void *context)
{
    (void)context;
    return ((AzFilterSingletonFn)(uintptr_t)
        AZ_FILTER_GCM_SINGLETON_ADDRESS)();
}

static void *filter_registry_singleton(void *context)
{
    (void)context;
    return ((AzFilterSingletonFn)(uintptr_t)
        AZ_FILTER_REGISTRY_SINGLETON_ADDRESS)();
}

static uint32_t filter_current_thread_id(void *context)
{
    (void)context;
    return GetCurrentThreadId();
}

static uint8_t filter_worker_affinity_verified(
    void *context,
    uint32_t worker_thread_id)
{
    (void)context;
    return worker_thread_id != 0u &&
        GetCurrentThreadId() == worker_thread_id ? 1u : 0u;
}

static uint8_t filter_coverflow_is_interactive(void *context)
{
    AzInputDetourStatus status;

    (void)context;
    az_rev1655_input_detour_snapshot_status(&status);
    return status.scene_allows_capture != 0u && XamIsUIActive() == 0 ? 1u : 0u;
}

static uint8_t filter_queue_is_demonstrably_idle(void *context)
{
    const uint8_t *gcm = (const uint8_t *)filter_gcm_singleton(context);
    uint32_t begin;
    uint32_t end;
    uint32_t capacity;

    if (filter_address_range_is_valid(
            context, gcm, AZ_FILTER_WORKER_BUSY_OFFSET + 1u) == 0u) {
        return 0u;
    }
    memcpy(&begin, gcm + AZ_FILTER_QUEUE_BEGIN_OFFSET, sizeof(begin));
    memcpy(&end, gcm + AZ_FILTER_QUEUE_END_OFFSET, sizeof(end));
    memcpy(&capacity, gcm + AZ_FILTER_QUEUE_CAPACITY_OFFSET, sizeof(capacity));
    if (begin > end || end > capacity ||
        ((begin | end | capacity) & 3u) != 0u ||
        gcm[AZ_FILTER_WORKER_BUSY_OFFSET] != 0u) {
        return 0u;
    }
    return begin == end ? 1u : 0u;
}

static AzInputDetourResult filter_take_request(
    void *context,
    uint8_t *filter_index)
{
    (void)context;
    return az_rev1655_input_detour_take_filter_request(filter_index);
}

static void filter_finish_request(void *context)
{
    (void)context;
    az_rev1655_input_detour_finish_filter_request();
}

static uint8_t filter_registry_lookup(
    void *context,
    void *registry,
    uint32_t registry_type,
    const char *identifier)
{
    (void)context;
    if (registry == NULL || identifier == NULL) {
        return 0u;
    }
    return ((AzFilterRegistryLookupFn)(uintptr_t)
        AZ_FILTER_REGISTRY_LOOKUP_ADDRESS)(
            registry, registry_type, identifier) != 0 ? 1u : 0u;
}

static uint8_t filter_copy_active(
    void *context,
    AzRev1655ActiveAggregateD0 *destination,
    const void *gcm_plus_60,
    uint32_t staging_selector)
{
    void *result;

    (void)context;
    if (destination == NULL || gcm_plus_60 == NULL) {
        return 0u;
    }
    result = ((AzFilterCopyFn)(uintptr_t)AZ_FILTER_COPY_ACTIVE_ADDRESS)(
        destination, gcm_plus_60, staging_selector);
    return result == destination ? 1u : 0u;
}

static void filter_destroy_active(
    void *context,
    AzRev1655ActiveAggregateD0 *aggregate)
{
    (void)context;
    ((AzFilterDestroyFn)(uintptr_t)
        AZ_FILTER_DESTROY_ACTIVE_ADDRESS)(aggregate);
}

static uint8_t filter_string_view(
    void *context,
    const AzRev1655AuroraString *value,
    const char **characters,
    uint32_t *length,
    uint32_t *capacity)
{
    uint32_t pointer_address;

    if (value == NULL || characters == NULL || length == NULL ||
        capacity == NULL || filter_address_range_is_valid(
            context, value, sizeof(*value)) == 0u) {
        return 0u;
    }
    memcpy(length, value->storage + 0x10u, sizeof(*length));
    memcpy(capacity, value->storage + 0x14u, sizeof(*capacity));
    if (*capacity < 0x10u) {
        *characters = (const char *)value->storage;
    }
    else {
        memcpy(&pointer_address, value->storage, sizeof(pointer_address));
        *characters = (const char *)(uintptr_t)pointer_address;
    }
    return 1u;
}

static uint8_t filter_string_construct(
    void *context,
    AzRev1655AuroraString *destination,
    const char *source)
{
    (void)context;
    return ((AzFilterStringConstructFn)(uintptr_t)
        AZ_FILTER_STRING_CONSTRUCT_ADDRESS)(destination, source) ==
            destination ? 1u : 0u;
}

static uint8_t filter_string_assign(
    void *context,
    AzRev1655AuroraString *destination,
    const char *source,
    uint32_t length)
{
    (void)context;
    return ((AzFilterStringAssignFn)(uintptr_t)
        AZ_FILTER_STRING_ASSIGN_ADDRESS)(destination, source, length) ==
            destination ? 1u : 0u;
}

static void filter_string_destroy(
    void *context,
    AzRev1655AuroraString *value)
{
    (void)context;
    ((AzFilterStringLifecycleFn)(uintptr_t)
        AZ_FILTER_STRING_LIFECYCLE_ADDRESS)(value, 1u, 0u);
}

static uint8_t filter_vector_count(
    void *context,
    const AzRev1655AuroraStringVector *vector,
    uint32_t *count)
{
    uint32_t span;

    (void)context;
    if (vector == NULL || count == NULL ||
        vector->begin_address > vector->end_address ||
        vector->end_address > vector->capacity_address ||
        ((vector->begin_address | vector->end_address |
          vector->capacity_address) & 3u) != 0u) {
        return 0u;
    }
    span = vector->end_address - vector->begin_address;
    if (span % AZ_REV1655_AURORA_STRING_SIZE != 0u ||
        span / AZ_REV1655_AURORA_STRING_SIZE >
            AZ_REV1655_FILTER_MAX_VECTOR_ITEMS) {
        return 0u;
    }
    *count = span / AZ_REV1655_AURORA_STRING_SIZE;
    return 1u;
}

static AzRev1655AuroraString *filter_vector_at(
    void *context,
    AzRev1655AuroraStringVector *vector,
    uint32_t index)
{
    uint32_t count;
    uintptr_t address;

    if (filter_vector_count(context, vector, &count) == 0u || index >= count) {
        return NULL;
    }
    address = (uintptr_t)vector->begin_address +
        (uintptr_t)index * AZ_REV1655_AURORA_STRING_SIZE;
    return filter_address_range_is_valid(
        context, (void *)address, AZ_REV1655_AURORA_STRING_SIZE) != 0u ?
            (AzRev1655AuroraString *)address : NULL;
}

static uint8_t filter_vector_push(
    void *context,
    AzRev1655AuroraStringVector *vector,
    const AzRev1655AuroraString *value)
{
    uint32_t before;
    uint32_t after;

    if (filter_vector_count(context, vector, &before) == 0u ||
        value == NULL || before >= AZ_REV1655_FILTER_MAX_VECTOR_ITEMS) {
        return 0u;
    }
    ((AzFilterVectorPushFn)(uintptr_t)AZ_FILTER_VECTOR_PUSH_ADDRESS)(
        vector, value);
    return filter_vector_count(context, vector, &after) != 0u &&
        after == before + 1u ? 1u : 0u;
}

static uint8_t filter_vector_erase(
    void *context,
    AzRev1655AuroraStringVector *vector,
    uint32_t index)
{
    uint32_t count;
    uint32_t current_index;
    uint32_t after;
    AzRev1655AuroraString *last;

    if (filter_vector_count(context, vector, &count) == 0u ||
        index >= count) {
        return 0u;
    }
    for (current_index = index; current_index + 1u < count;
         ++current_index) {
        AzRev1655AuroraString *current = filter_vector_at(
            context, vector, current_index);
        AzRev1655AuroraString *next = filter_vector_at(
            context, vector, current_index + 1u);
        const char *characters = NULL;
        uint32_t length = 0u;
        uint32_t capacity = 0u;

        if (current == NULL || next == NULL ||
            filter_string_view(
                context, next, &characters, &length, &capacity) == 0u ||
            filter_string_assign(
                context, current, characters, length) == 0u) {
            return 0u;
        }
    }

    last = filter_vector_at(context, vector, count - 1u);
    if (last == NULL ||
        vector->end_address < AZ_REV1655_AURORA_STRING_SIZE) {
        return 0u;
    }
    filter_string_destroy(context, last);
    vector->end_address -= AZ_REV1655_AURORA_STRING_SIZE;
    return filter_vector_count(context, vector, &after) != 0u &&
        after + 1u == count ? 1u : 0u;
}

static int32_t filter_schedule(
    void *context,
    void *gcm_plus_8,
    const AzRev1655FilterContext38 *filter_context,
    const AzRev1655FilterWork74 *work,
    uint32_t flags)
{
    (void)context;
    return ((AzFilterScheduleFn)(uintptr_t)AZ_FILTER_SCHEDULER_ADDRESS)(
        gcm_plus_8, filter_context, work, flags);
}

static void initialize_filter_host(AzRev1655FilterHostOps *host)
{
    memset(host, 0, sizeof(*host));
    az_rev1655_filter_consumer_exact_entrypoints(&host->entrypoints);
    host->current_thread_id = &filter_current_thread_id;
    host->worker_affinity_verified = &filter_worker_affinity_verified;
    host->coverflow_is_interactive = &filter_coverflow_is_interactive;
    host->filter_queue_is_demonstrably_idle =
        &filter_queue_is_demonstrably_idle;
    host->address_range_is_valid = &filter_address_range_is_valid;
    host->take_filter_request = &filter_take_request;
    host->finish_filter_request = &filter_finish_request;
    host->gcm_singleton = &filter_gcm_singleton;
    host->registry_singleton = &filter_registry_singleton;
    host->registry_lookup = &filter_registry_lookup;
    host->copy_active_aggregate = &filter_copy_active;
    host->destroy_active_aggregate = &filter_destroy_active;
    host->string_view = &filter_string_view;
    host->string_construct_cstring = &filter_string_construct;
    host->string_assign_bytes = &filter_string_assign;
    host->string_destroy = &filter_string_destroy;
    host->vector_count = &filter_vector_count;
    host->vector_at = &filter_vector_at;
    host->vector_push_back = &filter_vector_push;
    host->vector_erase = &filter_vector_erase;
    host->schedule_filter = &filter_schedule;
}

static AzRev1655FilterConsumerResult bind_filter_consumer_pristine(
    const AzRev1655LoadedImage *image,
    const AzRev1655HookPermit *permit,
    uint32_t worker_thread_id)
{
    AzRev1655FilterProvenance provenance;
    AzRev1655FilterHostOps host;

    az_rev1655_filter_consumer_exact_provenance(&provenance);
    initialize_filter_host(&host);
    return az_rev1655_filter_consumer_bind_with_validated_permit(
        &g_runtime.filter_consumer,
        image,
        permit,
        &provenance,
        worker_thread_id,
        &host);
}

static AzInputDetourResult browse_take_request(
    void *context,
    uint8_t *filter_index)
{
    return filter_take_request(context, filter_index);
}

static void browse_finish_request(void *context)
{
    filter_finish_request(context);
}

static void *browse_gcm_singleton(void *context)
{
    (void)context;
    return ((AzFilterSingletonFn)(uintptr_t)
        AZ_BROWSE_GCM_SINGLETON_ADDRESS)();
}

static uint8_t browse_active_list(
    void *context,
    AzRev1655BrowseList *list)
{
    uint8_t *manager;
    void *vector;
    uint32_t key = 2u;
    uint32_t begin_address;
    uint32_t end_address;

    if (list == NULL) {
        return 0u;
    }
    list->begin = NULL;
    list->end = NULL;
    manager = (uint8_t *)((AzFilterSingletonFn)(uintptr_t)
        AZ_BROWSE_LIST_MANAGER_SINGLETON_ADDRESS)();
    if (filter_address_range_is_valid(
            context,
            manager,
            AZ_BROWSE_ACTIVE_MAP_OFFSET + sizeof(uint32_t)) == 0u) {
        return 0u;
    }
    vector = ((AzBrowseListLookupFn)(uintptr_t)
        AZ_BROWSE_LIST_LOOKUP_ADDRESS)(
            manager + AZ_BROWSE_ACTIVE_MAP_OFFSET, &key);
    if (filter_address_range_is_valid(
            context, vector, sizeof(uint32_t) * 2u) == 0u) {
        return 0u;
    }
    memcpy(&begin_address, vector, sizeof(begin_address));
    memcpy(
        &end_address,
        (const uint8_t *)vector + sizeof(uint32_t),
        sizeof(end_address));
    if (end_address < begin_address ||
        ((begin_address | end_address) & 3u) != 0u) {
        return 0u;
    }
    list->begin = (const AzRev1655BrowseItem *)(uintptr_t)begin_address;
    list->end = (const AzRev1655BrowseItem *)(uintptr_t)end_address;
    return 1u;
}

static uint8_t browse_publish_jump(
    void *context,
    void *gcm,
    uint32_t target_index,
    uint32_t item_count)
{
    (void)context;
    return az_rev1655_input_detour_publish_browse_jump(
        (uintptr_t)gcm, target_index, item_count);
}

static uint8_t browse_apply_jump(
    void *context,
    uintptr_t published_gcm,
    uint32_t target_index,
    uint32_t item_count)
{
    uint8_t *gcm;
    uint8_t *helper;
    void *layout;
    void *owner;
    void *owner_vtable;
    AzBrowseSelectionChangedFn selection_changed;
    uint32_t current_index;
    AzBrowsePopulateFn populate;

    (void)context;
    store_u32(&g_runtime.browse_apply_target, target_index);
    store_u32(&g_runtime.browse_apply_count, item_count);
    store_u32(&g_runtime.browse_apply_current, UINT32_MAX);
    store_u32(&g_runtime.browse_apply_reason, AZ_BROWSE_APPLY_NONE);
    gcm = (uint8_t *)browse_gcm_singleton(NULL);
    if (item_count == 0u || target_index >= item_count) {
        store_u32(
            &g_runtime.browse_apply_reason,
            AZ_BROWSE_APPLY_BAD_PUBLICATION);
        return 0u;
    }
    if ((uintptr_t)gcm != published_gcm ||
        filter_address_range_is_valid(
            NULL, gcm, AZ_BROWSE_READY_OFFSET + 1u) == 0u) {
        store_u32(&g_runtime.browse_apply_reason, AZ_BROWSE_APPLY_BAD_GCM);
        return 0u;
    }
    if (
        gcm[AZ_BROWSE_BLOCKED_OFFSET] == 1u ||
        gcm[AZ_BROWSE_READY_OFFSET] == 0u ||
        gcm[AZ_BROWSE_ENABLED_OFFSET] == 0u) {
        store_u32(
            &g_runtime.browse_apply_reason,
            AZ_BROWSE_APPLY_STOCK_GATE);
        return 0u;
    }

    memcpy(&layout, gcm + AZ_BROWSE_LAYOUT_OFFSET, sizeof(layout));
    helper = gcm + AZ_BROWSE_HELPER_OFFSET;
    if (layout == NULL || filter_address_range_is_valid(
            NULL,
            helper,
            AZ_BROWSE_HELPER_CURRENT_INDEX_OFFSET + sizeof(uint32_t)) == 0u) {
        store_u32(
            &g_runtime.browse_apply_reason,
            AZ_BROWSE_APPLY_BAD_LAYOUT);
        return 0u;
    }
    memcpy(
        &current_index,
        gcm + AZ_BROWSE_CURRENT_INDEX_OFFSET,
        sizeof(current_index));
    store_u32(&g_runtime.browse_apply_current, current_index);
    if (current_index >= item_count) {
        store_u32(
            &g_runtime.browse_apply_reason,
            AZ_BROWSE_APPLY_BAD_HELPER);
        return 0u;
    }
    if (target_index == current_index) {
        store_u32(
            &g_runtime.browse_apply_reason,
            AZ_BROWSE_APPLY_ALREADY_SELECTED);
        return 1u;
    }

    /* Aurora itself uses this exact sequence when it restores a selected
     * title after rebuilding the active list: repopulate the coverflow window
     * around the requested index, publish old/new selection indices, then
     * notify the coverflow owner. The ordinary left/right helpers animate
     * every intervening cover, which is not a browse-to-letter operation. */
    populate = (AzBrowsePopulateFn)(uintptr_t)AZ_BROWSE_POPULATE_ADDRESS;
    if (populate(helper, layout, target_index) == 0u) {
        store_u32(
            &g_runtime.browse_apply_reason,
            AZ_BROWSE_APPLY_MOVE_REJECTED);
        return 0u;
    }
    memcpy(
        helper + AZ_BROWSE_HELPER_PREVIOUS_INDEX_OFFSET,
        &current_index,
        sizeof(current_index));
    memcpy(
        helper + AZ_BROWSE_HELPER_CURRENT_INDEX_OFFSET,
        &target_index,
        sizeof(target_index));

    memcpy(&owner, helper, sizeof(owner));
    if (owner != NULL && filter_address_range_is_valid(
            NULL, owner, sizeof(owner_vtable)) != 0u) {
        memcpy(&owner_vtable, owner, sizeof(owner_vtable));
        if (owner_vtable != NULL && filter_address_range_is_valid(
                NULL,
                (const uint8_t *)owner_vtable +
                    AZ_BROWSE_HELPER_OWNER_CALLBACK_OFFSET,
                sizeof(selection_changed)) != 0u) {
            memcpy(
                &selection_changed,
                (const uint8_t *)owner_vtable +
                    AZ_BROWSE_HELPER_OWNER_CALLBACK_OFFSET,
                sizeof(selection_changed));
            if (selection_changed != NULL &&
                filter_address_range_is_valid(
                    NULL,
                    (const void *)(uintptr_t)selection_changed,
                    sizeof(uint32_t)) != 0u) {
                selection_changed(owner, current_index, target_index);
            }
        }
    }
    store_u32(&g_runtime.browse_apply_reason, AZ_BROWSE_APPLY_MOVED);
    return 1u;
}

static AzRev1655BrowseResult bind_browse_consumer(void)
{
    AzRev1655BrowseHostOps host;

    memset(&host, 0, sizeof(host));
    host.context = NULL;
    host.coverflow_is_interactive = &filter_coverflow_is_interactive;
    host.address_range_is_valid = &filter_address_range_is_valid;
    host.take_request = &browse_take_request;
    host.finish_request = &browse_finish_request;
    host.gcm_singleton = &browse_gcm_singleton;
    host.active_list = &browse_active_list;
    host.string_view = &filter_string_view;
    host.publish_jump = &browse_publish_jump;
    return az_rev1655_browse_consumer_bind(
        &g_runtime.browse_consumer, &host);
}

static uint8_t rename_netdbg_module_row(void)
{
    uint32_t wrapper_address = load_u32(
        &g_runtime.netdbg_wrapper_address);
    uint8_t *label;
    uint32_t storage_address;
    uint32_t capacity;
    uint16_t *storage;

    if (wrapper_address == 0u || filter_address_range_is_valid(
            NULL,
            (void *)(uintptr_t)wrapper_address,
            AZ_REV1655_NETDBG_LABEL_OFFSET +
                AZ_REV1655_AURORA_STRING_SIZE) == 0u) {
        return 0u;
    }
    label = (uint8_t *)(uintptr_t)(
        wrapper_address + AZ_REV1655_NETDBG_LABEL_OFFSET);
    memcpy(&capacity, label + 0x14u, sizeof(capacity));
    if (capacity < 8u || capacity > 1024u) {
        return 0u;
    }
    memcpy(&storage_address, label, sizeof(storage_address));
    storage = (uint16_t *)(uintptr_t)storage_address;
    if (filter_address_range_is_valid(
            NULL,
            storage,
            ((size_t)capacity + 1u) * sizeof(uint16_t)) == 0u) {
        return 0u;
    }
    return az_module_settings_write_label(
        label, storage, capacity + 1u);
}

static uint8_t target_utf16_equals(
    const uint16_t *actual,
    const uint16_t *expected,
    uint32_t expected_length)
{
    uint32_t index;

    if (actual == NULL || expected == NULL ||
        filter_address_range_is_valid(
            NULL, actual,
            ((size_t)expected_length + 1u) * sizeof(uint16_t)) == 0u) {
        return 0u;
    }
    for (index = 0u; index < expected_length; ++index) {
        uint16_t value;

        memcpy(&value, actual + index, sizeof(value));
        if (value != expected[index]) {
            return 0u;
        }
    }
    {
        uint16_t terminator;
        memcpy(&terminator, actual + expected_length, sizeof(terminator));
        return terminator == 0u ? 1u : 0u;
    }
}

static uint8_t target_utf16_has_basename(
    const uint16_t *path,
    const uint16_t *basename,
    uint32_t basename_length)
{
    uint16_t units[256];
    uint32_t length;
    uint32_t index;

    if (path == NULL || basename == NULL) {
        return 0u;
    }
    for (length = 0u; length < 256u; ++length) {
        if (filter_address_range_is_valid(
                NULL, path + length, sizeof(uint16_t)) == 0u) {
            return 0u;
        }
        memcpy(&units[length], path + length, sizeof(uint16_t));
        if (units[length] == 0u) {
            break;
        }
    }
    if (length == 256u || length < basename_length) {
        return 0u;
    }
    for (index = 0u; index < basename_length; ++index) {
        if (units[length - basename_length + index] != basename[index]) {
            return 0u;
        }
    }
    return 1u;
}

static uint32_t find_scene_handle(
    const uint16_t *basename,
    uint32_t basename_length)
{
    uint32_t node;
    uint32_t count;

    if (basename == NULL || basename_length == 0u ||
        filter_address_range_is_valid(
            NULL,
            (const void *)(uintptr_t)AZ_SCENE_CACHE_HEAD_ADDRESS,
            sizeof(node)) == 0u) {
        return 0u;
    }
    memcpy(
        &node,
        (const void *)(uintptr_t)AZ_SCENE_CACHE_HEAD_ADDRESS,
        sizeof(node));
    for (count = 0u; node != 0u && count < 256u; ++count) {
        uint32_t path_address;
        uint32_t handle;
        uint32_t acquired;
        uint32_t next;

        if (filter_address_range_is_valid(
                NULL, (const void *)(uintptr_t)node, 16u) == 0u) {
            return 0u;
        }
        memcpy(&path_address, (const void *)(uintptr_t)node, 4u);
        memcpy(&handle, (const void *)(uintptr_t)(node + 4u), 4u);
        memcpy(&acquired, (const void *)(uintptr_t)(node + 8u), 4u);
        memcpy(&next, (const void *)(uintptr_t)(node + 12u), 4u);
        if (acquired == 1u && handle != 0u &&
            target_utf16_has_basename(
                (const uint16_t *)(uintptr_t)path_address,
                basename,
                basename_length) != 0u) {
            return handle;
        }
        if (next == node) {
            return 0u;
        }
        node = next;
    }
    return 0u;
}

static uint32_t find_settings_scene_handle(void)
{
    static const uint16_t basename[] = {
        'A','u','r','o','r','a','_','S','e','t','t','i','n','g','s',
        '.','x','u','r',0
    };
    return find_scene_handle(
        basename,
        (uint32_t)(sizeof(basename) / sizeof(basename[0]) - 1u));
}

static uint32_t find_auroraaz_settings_scene_handle(void)
{
    static const uint16_t basename[] = {
        'A','u','r','o','r','a','A','Z','_','S','e','t','t','i','n','g','s',
        '.','x','u','r',0
    };
    return find_scene_handle(
        basename,
        (uint32_t)(sizeof(basename) / sizeof(basename[0]) - 1u));
}

static uint8_t resolve_icon_xui(void)
{
    HMODULE xam = NULL;
    void *first = NULL;
    void *next = NULL;
    void *descendant = NULL;
    void *text = NULL;
    void *set_text = NULL;
    void *set_image = NULL;
    void *has_focus = NULL;

    if (g_runtime.xui_set_text != NULL &&
        g_runtime.xui_set_image_path != NULL &&
        g_runtime.xui_has_focus != NULL) {
        return 1u;
    }
    if (FAILED(XexGetModuleHandle("xam.xex", &xam)) || xam == NULL ||
        FAILED(XexGetProcedureAddress(
            xam, AZ_XUI_ELEMENT_GET_FIRST_CHILD_ORDINAL, &first)) ||
        FAILED(XexGetProcedureAddress(
            xam, AZ_XUI_ELEMENT_GET_NEXT_ORDINAL, &next)) ||
        FAILED(XexGetProcedureAddress(
            xam, AZ_XUI_ELEMENT_GET_DESCENDANT_BY_ID_ORDINAL, &descendant)) ||
        FAILED(XexGetProcedureAddress(
            xam, AZ_XUI_TEXT_ELEMENT_GET_TEXT_ORDINAL, &text)) ||
        FAILED(XexGetProcedureAddress(
            xam, AZ_XUI_TEXT_ELEMENT_SET_TEXT_ORDINAL, &set_text)) ||
        FAILED(XexGetProcedureAddress(
            xam, AZ_XUI_IMAGE_ELEMENT_SET_IMAGE_PATH_ORDINAL, &set_image)) ||
        FAILED(XexGetProcedureAddress(
            xam, AZ_XUI_ELEMENT_HAS_FOCUS_ORDINAL, &has_focus)) ||
        first == NULL || next == NULL || descendant == NULL || text == NULL ||
        set_text == NULL || set_image == NULL || has_focus == NULL) {
        return 0u;
    }
    g_runtime.xui_get_first_child = (AzXuiGetChildFn)(uintptr_t)first;
    g_runtime.xui_get_next = (AzXuiGetChildFn)(uintptr_t)next;
    g_runtime.xui_get_descendant =
        (AzXuiGetDescendantFn)(uintptr_t)descendant;
    g_runtime.xui_get_text = (AzXuiGetTextFn)(uintptr_t)text;
    g_runtime.xui_set_text = (AzXuiSetTextFn)(uintptr_t)set_text;
    g_runtime.xui_set_image_path =
        (AzXuiSetImagePathFn)(uintptr_t)set_image;
    g_runtime.xui_has_focus = (AzXuiHasFocusFn)(uintptr_t)has_focus;
    return 1u;
}

static void module_ui_tick(void *context)
{
    static const uint16_t module_list_id[] = {
        'M','o','d','u','l','e','L','i','s','t',0
    };
    static const uint16_t title_id[] = {
        'T','i','t','l','e','P','r','e','s','e','n','t','e','r',0
    };
    static const uint16_t icon_id[] = {
        'I','c','o','n','P','r','e','s','e','n','t','e','r',0
    };
    static const uint16_t expected_title[] = {
        'A','u','r','o','r','a',' ','A','-','Z',0
    };
    static const uint16_t icon_path[] = {
        'g','a','m','e',':','\\','D','a','t','a','\\','A','u','r','o','r','a',
        'A','Z','-','i','c','o','n','.','p','n','g',0
    };
    static const uint16_t mode_status_id[] = {
        'M','o','d','e','S','t','a','t','u','s',0
    };
    static const uint16_t browse_status[] = {
        'C','u','r','r','e','n','t',' ','m','o','d','e',':',' ','B','r','o','w','s','e',0
    };
    static const uint16_t filter_status[] = {
        'C','u','r','r','e','n','t',' ','m','o','d','e',':',' ','F','i','l','t','e','r',0
    };
    uint32_t ticks;
    uint32_t scene;
    uint32_t list = 0u;
    uint32_t item = 0u;
    uint32_t count;
    uint32_t module_scene;

    (void)context;
    if (resolve_icon_xui() == 0u) {
        g_runtime.settings_dialog_active = 0u;
        store_u32(&g_runtime.settings_resolve_result, 2u);
    }
    else {
        module_scene = find_auroraaz_settings_scene_handle();
        g_runtime.settings_dialog_active =
            module_scene != 0u &&
            g_runtime.xui_has_focus(module_scene) == 1 ? 1u : 0u;
        if (g_runtime.settings_dialog_active != 0u) {
            uint32_t status_label = 0u;
            if (g_runtime.xui_get_descendant(
                    module_scene, mode_status_id, &status_label) >= 0 &&
                status_label != 0u) {
                const uint16_t *status_text =
                    load_u32(&g_runtime.operation_mode) ==
                        (uint32_t)AZ_OPERATION_MODE_FILTER ?
                            filter_status : browse_status;
                (void)g_runtime.xui_set_text(status_label, status_text);
            }
        }
        store_u32(&g_runtime.settings_resolve_result, 1u);
        if (g_runtime.settings_dialog_active == 0u) {
            g_runtime.settings_a_owned = 0u;
        }
    }
    ticks = __atomic_add_fetch(
        &g_runtime.icon_ui_ticks, 1u, __ATOMIC_ACQ_REL);
    if (ticks < AZ_ICON_UI_TICK_INTERVAL) {
        return;
    }
    store_u32(&g_runtime.icon_ui_ticks, 0u);
    if (load_u32(&g_runtime.icon_cache_result) != 1u ||
        resolve_icon_xui() == 0u) {
        store_u32(&g_runtime.icon_apply_result, 2u);
        return;
    }
    scene = find_settings_scene_handle();
    if (scene == 0u) {
        store_u32(&g_runtime.icon_apply_result, 3u);
        return;
    }
    if (g_runtime.xui_get_descendant(
            scene, module_list_id, &list) < 0 || list == 0u) {
        store_u32(&g_runtime.icon_apply_result, 4u);
        return;
    }
    if (g_runtime.xui_get_first_child(list, &item) < 0 || item == 0u) {
        store_u32(&g_runtime.icon_apply_result, 5u);
        return;
    }
    for (count = 0u; item != 0u && count < 32u; ++count) {
        uint32_t title = 0u;
        const uint16_t *title_text = NULL;

        if (g_runtime.xui_get_descendant(
                item, title_id, &title) >= 0 && title != 0u &&
            g_runtime.xui_get_text(title, &title_text) >= 0 &&
            target_utf16_equals(
                title_text,
                expected_title,
                AZ_MODULE_SETTINGS_LABEL_LENGTH) != 0u) {
            uint32_t icon = 0u;

            if (g_runtime.xui_get_descendant(
                    item, icon_id, &icon) < 0 || icon == 0u) {
                store_u32(&g_runtime.icon_apply_result, 6u);
                return;
            }
            if (g_runtime.xui_set_image_path(icon, icon_path) < 0) {
                store_u32(&g_runtime.icon_apply_result, 7u);
                return;
            }
            store_u32(&g_runtime.icon_apply_result, 8u);
            return;
        }
        {
            uint32_t next = 0u;
            if (g_runtime.xui_get_next(item, &next) < 0 || next == item) {
                break;
            }
            item = next;
        }
    }
    store_u32(&g_runtime.icon_apply_result, 5u);
}

static uint8_t module_settings_ui_input(
    void *context,
    const AzInputKeystroke *keystroke)
{
    static const uint16_t browse_id[] = {
        'B','r','o','w','s','e','M','o','d','e',0
    };
    static const uint16_t filter_id[] = {
        'F','i','l','t','e','r','M','o','d','e',0
    };
    uint32_t scene;
    uint32_t browse = 0u;
    uint32_t filter = 0u;
    uint32_t mode;

    (void)context;
    if (keystroke == NULL ||
        keystroke->virtual_key != AZ_VK_PAD_A ||
        resolve_icon_xui() == 0u) {
        return 0u;
    }
    if ((keystroke->flags & AZ_KEYSTROKE_KEYUP) != 0u) {
        if (g_runtime.settings_a_owned == 0u) {
            return 0u;
        }
        g_runtime.settings_a_owned = 0u;
        return 1u;
    }
    if ((keystroke->flags & AZ_KEYSTROKE_REPEAT) != 0u) {
        return g_runtime.settings_a_owned;
    }
    if ((keystroke->flags & AZ_KEYSTROKE_KEYDOWN) == 0u) {
        return 0u;
    }

    scene = find_auroraaz_settings_scene_handle();
    if (scene == 0u || g_runtime.xui_has_focus(scene) != 1) {
        return 0u;
    }
    if (g_runtime.xui_get_descendant(scene, browse_id, &browse) < 0 ||
        browse == 0u ||
        g_runtime.xui_get_descendant(scene, filter_id, &filter) < 0 ||
        filter == 0u) {
        return 0u;
    }
    if (g_runtime.xui_has_focus(browse) == 1) {
        mode = AZ_MODULE_SETTINGS_MODE_BROWSE;
    }
    else if (g_runtime.xui_has_focus(filter) == 1) {
        mode = AZ_MODULE_SETTINGS_MODE_FILTER;
    }
    else {
        return 0u;
    }
    if (az_module_settings_request_mode(mode) == 0u) {
        return 0u;
    }
    g_runtime.settings_a_owned = 1u;
    store_u32(&g_runtime.settings_call_result, 0u);
    return 1u;
}

static void publish_consumer_gate_for_mode(AzOperationMode mode)
{
    uint8_t consumer_ready;

    if (mode == AZ_OPERATION_MODE_BROWSE) {
        consumer_ready = load_u32(&g_runtime.browse_bind_result) ==
            (uint32_t)AZ_REV1655_BROWSE_IDLE ? 1u : 0u;
    }
    else {
        consumer_ready = load_u32(&g_runtime.filter_apply_state) ==
            AZ_FILTER_APPLY_ARMED ? 1u : 0u;
    }
    az_rev1655_input_detour_publish_verification(
        1u, 1u, 1u, consumer_ready);
    az_rev1655_input_detour_confirm_controls(AZ_INPUT_VERIFIED_REQUIRED);
    (void)az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_CONSUME);
}

static void apply_operation_mode(AzOperationMode mode)
{
    mode = az_operation_mode_sanitize((uint32_t)mode);
    if (write_operation_mode(mode) == 0u) {
        DbgPrint("AuroraAZ: could not persist operation mode\n");
    }
    store_u32(&g_runtime.operation_mode, (uint32_t)mode);
    publish_consumer_gate_for_mode(mode);
    DbgPrint(
        "AuroraAZ: operation mode=%s\n",
        az_operation_mode_name(mode));
}

static void settings_worker_step(void)
{
    uint32_t mode;

    if (az_module_settings_take_mode_request(&mode) != 0u) {
        apply_operation_mode(mode == AZ_MODULE_SETTINGS_MODE_FILTER ?
            AZ_OPERATION_MODE_FILTER : AZ_OPERATION_MODE_BROWSE);
        (void)__atomic_add_fetch(
            &g_runtime.settings_completions, 1u, __ATOMIC_ACQ_REL);
    }
}
#endif

static AzRev1655RuntimeResult validate_input_site(
    AzRev1655LoadedImage *image,
    AzRev1655ResolvedHookSite *resolved,
    uint8_t log_failures,
    const AzRev1655HookPermit **out_permit)
{
    const AzRev1655HookPermit *permit = NULL;
    const AzRev1655HookSiteDescriptor *descriptor;
    AzRev1655RuntimeImportResolverContext resolver_context = {
        {NULL, NULL},
        {0u, 0u}
    };
    const AzRev1655ImportResolver import_resolver = {
        resolve_runtime_import_target,
        &resolver_context
    };
    AzRev1655HookGateResult gate_result;

    if (out_permit != NULL) {
        *out_permit = NULL;
    }

    image->bytes = (const uint8_t *)(uintptr_t)AZ_REV1655_IMAGE_BASE;
    image->size = (size_t)AZ_REV1655_NT_IMAGE_SIZE;
    image->virtual_address = AZ_REV1655_IMAGE_BASE;

    if (image_range_is_mapped(
            image->bytes,
            (size_t)AZ_REV1655_PE_HEADER_SIZE) == 0u ||
        image_range_is_mapped(
            (const uint8_t *)(uintptr_t)AZ_REV1655_TEXT_BASE,
            (size_t)AZ_REV1655_TEXT_SIZE) == 0u) {
        if (log_failures != 0u) {
            DbgPrint("AuroraAZ: M2a image mapping rejected\n");
        }
        return AZ_REV1655_RUNTIME_IMAGE_UNMAPPED;
    }

    /* This verifies the header, immutable .text prefix, all loader-resolved
     * import thunks, canonical complete-.text hash, and every reviewed site.
     * No arena, bridge state, thread, or executable byte is mutated before
     * the complete gate succeeds. */
    gate_result = az_rev1655_hook_gate_validate_with_import_resolver(
        image,
        &import_resolver,
        &permit);
    if (gate_result != AZ_REV1655_HOOK_GATE_OK) {
        if (log_failures != 0u) {
            DbgPrint(
                "AuroraAZ: M2a image gate rejected: %s\n",
                az_rev1655_hook_gate_result_name(gate_result));
        }
        return AZ_REV1655_RUNTIME_IMAGE_REJECTED;
    }

    descriptor = az_rev1655_hook_gate_site(
        permit,
        AZ_REV1655_HOOK_SITE_INPUT_WRAPPER);
    if (descriptor == NULL) {
        if (log_failures != 0u) {
            DbgPrint("AuroraAZ: M2a input descriptor rejected\n");
        }
        return AZ_REV1655_RUNTIME_SITE_REJECTED;
    }

    gate_result = az_rev1655_hook_gate_resolve_site(
        permit,
        descriptor,
        image,
        resolved);
    if (gate_result != AZ_REV1655_HOOK_GATE_OK) {
        if (log_failures != 0u) {
            DbgPrint(
                "AuroraAZ: M2a live input gate rejected: %s\n",
                az_rev1655_hook_gate_result_name(gate_result));
        }
        return AZ_REV1655_RUNTIME_SITE_REJECTED;
    }

    if (resolved->target_address != AZ_M2A_INPUT_TARGET_ADDRESS ||
        resolved->expected_instruction !=
            AZ_REV1655_INPUT_WRAPPER_FIRST_INSTRUCTION ||
        resolved->complete_signature_size !=
            (size_t)AZ_INPUT_SIGNATURE_SIZE) {
        if (log_failures != 0u) {
            DbgPrint(
                "AuroraAZ: M2a wrong input site target=%08X insn=%08X size=%u\n",
                (unsigned int)resolved->target_address,
                (unsigned int)resolved->expected_instruction,
                (unsigned int)resolved->complete_signature_size);
        }
        return AZ_REV1655_RUNTIME_WRONG_INPUT_SITE;
    }

    if (out_permit != NULL) {
        *out_permit = permit;
    }

    return AZ_REV1655_RUNTIME_OK;
}

#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
static AzRev1655RuntimeResult validate_overlay_sites(
    const AzRev1655LoadedImage *image,
    AzRev1655ResolvedHookSite *render_menu,
    AzRev1655ResolvedHookSite *font_end,
    AzRev1655ResolvedHookSite *content_launch,
    AzRev1655ResolvedHookSite *module_settings)
{
    const AzRev1655HookPermit *permit = NULL;
    const AzRev1655HookSiteDescriptor *descriptor;
    AzRev1655RuntimeImportResolverContext resolver_context = {
        {NULL, NULL},
        {0u, 0u}
    };
    const AzRev1655ImportResolver import_resolver = {
        resolve_runtime_import_target,
        &resolver_context
    };
    AzRev1655HookGateResult gate_result;

    if (image == NULL || render_menu == NULL || font_end == NULL ||
        content_launch == NULL || module_settings == NULL) {
        return AZ_REV1655_RUNTIME_RENDER_SITE_REJECTED;
    }

    gate_result = az_rev1655_hook_gate_validate_with_import_resolver(
        image,
        &import_resolver,
        &permit);
    if (gate_result != AZ_REV1655_HOOK_GATE_OK || permit == NULL) {
        return AZ_REV1655_RUNTIME_IMAGE_REJECTED;
    }

    descriptor = az_rev1655_hook_gate_site(
        permit,
        AZ_REV1655_HOOK_SITE_RENDER_MENU);
    if (descriptor == NULL ||
        az_rev1655_hook_gate_resolve_site(
            permit,
            descriptor,
            image,
            render_menu) != AZ_REV1655_HOOK_GATE_OK ||
        render_menu->target_address !=
            AZ_M2B_RENDER_MENU_TARGET_ADDRESS ||
        render_menu->expected_instruction !=
            AZ_REV1655_RENDER_MENU_FIRST_INSTRUCTION ||
        render_menu->complete_signature_size !=
            (size_t)AZ_RENDER_SIGNATURE_SIZE) {
        return AZ_REV1655_RUNTIME_RENDER_SITE_REJECTED;
    }

    descriptor = az_rev1655_hook_gate_site(
        permit,
        AZ_REV1655_HOOK_SITE_FONT_END);
    if (descriptor == NULL ||
        az_rev1655_hook_gate_resolve_site(
            permit,
            descriptor,
            image,
            font_end) != AZ_REV1655_HOOK_GATE_OK ||
        font_end->target_address != AZ_M2B_FONT_END_TARGET_ADDRESS ||
        font_end->expected_instruction !=
            AZ_REV1655_FONT_END_FIRST_INSTRUCTION ||
        font_end->complete_signature_size !=
            (size_t)AZ_RENDER_SIGNATURE_SIZE) {
        return AZ_REV1655_RUNTIME_RENDER_SITE_REJECTED;
    }

    descriptor = az_rev1655_hook_gate_site(
        permit,
        AZ_REV1655_HOOK_SITE_CONTENT_LAUNCH);
    if (descriptor == NULL ||
        az_rev1655_hook_gate_resolve_site(
            permit,
            descriptor,
            image,
            content_launch) != AZ_REV1655_HOOK_GATE_OK ||
        content_launch->target_address !=
            AZ_CONTENT_LAUNCH_TARGET_ADDRESS ||
        content_launch->expected_instruction !=
            AZ_REV1655_CONTENT_LAUNCH_FIRST_INSTRUCTION ||
        content_launch->complete_signature_size !=
            (size_t)AZ_RENDER_SIGNATURE_SIZE) {
        return AZ_REV1655_RUNTIME_LAUNCH_SITE_REJECTED;
    }

    descriptor = az_rev1655_hook_gate_site(
        permit,
        AZ_REV1655_HOOK_SITE_MODULE_SETTINGS);
    if (descriptor == NULL ||
        az_rev1655_hook_gate_resolve_site(
            permit,
            descriptor,
            image,
            module_settings) != AZ_REV1655_HOOK_GATE_OK ||
        module_settings->target_address !=
            AZ_MODULE_SETTINGS_TARGET_ADDRESS ||
        module_settings->expected_instruction !=
            AZ_REV1655_MODULE_SETTINGS_FIRST_INSTRUCTION ||
        module_settings->complete_signature_size !=
            (size_t)AZ_RENDER_SIGNATURE_SIZE) {
        return AZ_REV1655_RUNTIME_SETTINGS_SITE_REJECTED;
    }

    return AZ_REV1655_RUNTIME_OK;
}
#endif

AzRev1655RuntimeResult az_rev1655_runtime_pin_module(
    uint32_t expected_ordinal4_export)
{
    AzRev1655LoadedImage image;
    AzRev1655ResolvedHookSite resolved;
    AzNetDbgLifetimeRev1655Status lifetime_status;
    AzNetDbgLifetimeRev1655Result lifetime_result;
    AzRev1655RuntimeResult validation;
    uint32_t expected_state;
    uint32_t state;

    if (expected_ordinal4_export == 0u) {
        return AZ_REV1655_RUNTIME_LIFETIME_REJECTED;
    }

    state = load_u32(&g_runtime.lifetime_state);
    if (state == AZ_LIFETIME_PINNED) {
        return load_u32(&g_runtime.pinned_ordinal4_export) ==
            expected_ordinal4_export ?
                AZ_REV1655_RUNTIME_OK :
                AZ_REV1655_RUNTIME_LIFETIME_REJECTED;
    }
    if (state != AZ_LIFETIME_UNPINNED) {
        return state == AZ_LIFETIME_PINNING ?
            AZ_REV1655_RUNTIME_BUSY :
            AZ_REV1655_RUNTIME_LIFETIME_REJECTED;
    }

    expected_state = AZ_LIFETIME_UNPINNED;
    if (!__atomic_compare_exchange_n(
            &g_runtime.lifetime_state,
            &expected_state,
            AZ_LIFETIME_PINNING,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        return expected_state == AZ_LIFETIME_PINNED &&
            load_u32(&g_runtime.pinned_ordinal4_export) ==
                expected_ordinal4_export ?
                    AZ_REV1655_RUNTIME_OK :
                    AZ_REV1655_RUNTIME_BUSY;
    }

    /* The ordinal-4 callback performs this complete read-only revision gate
     * synchronously. Aurora cannot regain control and unload key 7 before the
     * subsequent policy CAS has made this image title-resident. */
    validation = validate_input_site(&image, &resolved, 0u, NULL);
    if (validation != AZ_REV1655_RUNTIME_OK) {
        store_u32(&g_runtime.lifetime_state, AZ_LIFETIME_FAILED);
        return validation;
    }
    if (az_rev1655_thread_wrapper_is_valid() == 0u) {
        store_u32(&g_runtime.lifetime_state, AZ_LIFETIME_FAILED);
        return AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED;
    }

    lifetime_result = az_rev1655_netdbg_lifetime_pin_default(
        1u,
        expected_ordinal4_export,
        &lifetime_status);
    if (lifetime_result != AZ_NETDBG_LIFETIME_OK ||
        lifetime_status.pinned_for_title_lifetime == 0u ||
        lifetime_status.compare_exchange_succeeded == 0u ||
        lifetime_status.policy_after !=
            AZ_REV1655_NETDBG_POLICY_RESIDENT) {
        store_u32(&g_runtime.lifetime_state, AZ_LIFETIME_FAILED);
        return AZ_REV1655_RUNTIME_LIFETIME_REJECTED;
    }

    store_u32(
        &g_runtime.pinned_ordinal4_export,
        expected_ordinal4_export);
    store_u32(
        &g_runtime.netdbg_wrapper_address,
        lifetime_status.wrapper_address);
    store_u32(&g_runtime.lifetime_state, AZ_LIFETIME_PINNED);
    return AZ_REV1655_RUNTIME_OK;
}

static void log_observation(const AzInputDetourObservation *observation)
{
    DbgPrint(
        "AuroraAZ: M2a input n=%u frame=%u lr=%08X vk=%04X flags=%04X "
        "user=%u hid=%u control=%u event=%u command=%u coverflow=%u "
        "would=%u consumed=%u filter=%u stage=%u/%u\n",
        (unsigned int)observation->serial,
        (unsigned int)observation->input_frame,
        (unsigned int)observation->caller_return_address,
        (unsigned int)observation->keystroke.virtual_key,
        (unsigned int)observation->keystroke.flags,
        (unsigned int)observation->keystroke.user_index,
        (unsigned int)observation->keystroke.hid_code,
        (unsigned int)observation->translation.control,
        (unsigned int)observation->translation.event,
        (unsigned int)observation->translation.command,
        (unsigned int)observation->coverflow_active,
        (unsigned int)observation->would_handle,
        (unsigned int)observation->consumed,
        (unsigned int)observation->filter_queued,
        (unsigned int)observation->requested_stage,
        (unsigned int)observation->effective_stage);
}

/* One worker pass has a fixed logging budget, even if input remains busy. */
static uint32_t drain_observation_pass(void)
{
    uint32_t drained = 0u;

    while (drained < AZ_OBSERVATION_DRAIN_BUDGET) {
        AzInputDetourObservation observation;
        const AzInputDetourResult result =
            az_rev1655_input_detour_take_observation(&observation);

        if (result == AZ_INPUT_DETOUR_NO_OBSERVATION) {
            break;
        }
        if (result != AZ_INPUT_DETOUR_OK) {
            DbgPrint(
                "AuroraAZ: M2a observation drain rejected: %s\n",
                az_input_detour_result_name(result));
            break;
        }

        log_observation(&observation);
        (void)az_m2a_input_telemetry_record(
            &g_runtime.telemetry,
            &observation);
        (void)__atomic_add_fetch(
            &g_runtime.observations_logged,
            1u,
            __ATOMIC_ACQ_REL);
        ++drained;
    }

    return drained;
}

static void drain_all_observations(void)
{
    while (drain_observation_pass() == AZ_OBSERVATION_DRAIN_BUDGET) {
        /* Keep each pass bounded and yield between full batches. */
        wait_for_control();
    }
}

static void remove_input_hook_safely(void)
{
    uint8_t reported_failure = 0u;

    for (;;) {
        const AzHookRuntimeResult result =
            az_live_hook_remove(&g_runtime.input_hook);

        if (result == AZ_HOOK_RUNTIME_OK) {
            break;
        }
        if (result != AZ_HOOK_RUNTIME_QUIESCING &&
            reported_failure == 0u) {
            DbgPrint(
                "AuroraAZ: M2a hook removal waiting: %s\n",
                az_hook_runtime_result_name(result));
            reported_failure = 1u;
        }

        (void)drain_observation_pass();
        wait_for_control();
    }

    while (az_live_hook_can_unload(&g_runtime.input_hook) == 0u ||
           az_rev1655_input_detour_shutdown_ready() == 0u) {
        (void)drain_observation_pass();
        wait_for_control();
    }
}

#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
static void remove_overlay_hook_for_title_exit(
    AzLiveHook *hook,
    const char *label)
{
    AzHookRuntimeResult result;

    if (hook == NULL || hook->installed == 0u) {
        return;
    }
    result = az_live_hook_remove(hook);
    if (result != AZ_HOOK_RUNTIME_OK &&
        result != AZ_HOOK_RUNTIME_NOT_INSTALLED) {
        /* The module is pinned and the title process is exiting. Never keep
         * the worker alive indefinitely if Aurora has already changed or
         * unmapped a hook target; closed bridges remain pass-through. */
        DbgPrint(
            "AuroraAZ: title-exit %s hook restore: %s\n",
            label,
            az_hook_runtime_result_name(result));
    }
}
#endif

static void rollback_installed_input_hook(void)
{
    AzInputDetourStatus status;

    (void)az_rev1655_input_detour_request_stage(AZ_INPUT_DETOUR_OFF);
    az_rev1655_input_detour_publish_verification(0u, 0u, 0u, 0u);

    for (;;) {
        const AzHookRuntimeResult result =
            az_live_hook_remove(&g_runtime.input_hook);

        if (result == AZ_HOOK_RUNTIME_OK) {
            break;
        }
        if (result != AZ_HOOK_RUNTIME_QUIESCING) {
            DbgPrint(
                "AuroraAZ: M2a startup rollback waiting: %s\n",
                az_hook_runtime_result_name(result));
        }
        wait_for_control();
    }

    do {
        az_rev1655_input_detour_snapshot_status(&status);
        if (status.in_flight != 0u) {
            wait_for_control();
        }
    } while (status.in_flight != 0u);

    drain_all_observations();
}

static uint32_t input_observe_worker(void *context)
{
    uint32_t state;

    (void)context;
#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
    store_u32(&g_runtime.worker_thread_id, GetCurrentThreadId());
#endif

    do {
        state = load_u32(&g_runtime.state);
        if (state == (uint32_t)AZ_REV1655_RUNTIME_STARTING) {
            wait_for_control();
        }
    } while (state == (uint32_t)AZ_REV1655_RUNTIME_STARTING);

    if (state == (uint32_t)AZ_REV1655_RUNTIME_RUNNING ||
        state == (uint32_t)AZ_REV1655_RUNTIME_STOPPING) {
        seed_input_telemetry_generation();
    }

    if (state == (uint32_t)AZ_REV1655_RUNTIME_RUNNING) {
        DbgPrint(
            "AuroraAZ: runtime active stage=%u input=%08X "
            "selector-consume=enabled filter=disabled\n",
            (unsigned int)load_u32(&g_runtime.stage),
            (unsigned int)AZ_M2A_INPUT_TARGET_ADDRESS);
        flush_input_telemetry(1u);
#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
        flush_render_telemetry(1u);
        flush_filter_probe_telemetry();
        flush_m3c_telemetry(1u);
#endif
    }

    while (load_u32(&g_runtime.state) ==
           (uint32_t)AZ_REV1655_RUNTIME_RUNNING) {
        (void)drain_observation_pass();
        flush_input_telemetry(0u);
#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
        settings_worker_step();
        flush_m3c_telemetry(0u);
        if (load_u32(&g_runtime.filter_bind_result) ==
                (uint32_t)AZ_REV1655_FILTER_CONSUMER_IDLE &&
            (load_u32(&g_runtime.filter_probe_result) ==
                 (uint32_t)AZ_REV1655_FILTER_CONSUMER_NOT_BOUND ||
             load_u32(&g_runtime.filter_probe_result) ==
                 (uint32_t)AZ_REV1655_FILTER_CONSUMER_DEFERRED)) {
            const AzRev1655FilterConsumerResult probe_result =
                az_rev1655_filter_consumer_worker_probe(
                    &g_runtime.filter_consumer);
            store_u32(
                &g_runtime.filter_probe_result,
                (uint32_t)probe_result);
            if (probe_result == AZ_REV1655_FILTER_CONSUMER_IDLE) {
                /* Hardware probe passed. Arm asynchronous applies.  Each
                 * successful schedule revokes the gate until Aurora's work
                 * queue has remained idle beyond the conservative cooldown. */
                store_u32(
                    &g_runtime.filter_apply_state,
                    AZ_FILTER_APPLY_ARMED);
                g_runtime.filter_cooldown_ticks = 0u;
                g_runtime.filter_idle_ticks = 0u;
                g_runtime.filter_activity_seen = 0u;
                publish_consumer_gate_for_mode(
                    (AzOperationMode)load_u32(
                        &g_runtime.operation_mode));
            }
            flush_filter_probe_telemetry();
        }
        if (load_u32(&g_runtime.operation_mode) ==
                (uint32_t)AZ_OPERATION_MODE_BROWSE &&
            load_u32(&g_runtime.browse_bind_result) ==
                (uint32_t)AZ_REV1655_BROWSE_IDLE) {
            const AzRev1655BrowseResult browse_result =
                az_rev1655_browse_consumer_worker_step(
                    &g_runtime.browse_consumer);
            store_u32(
                &g_runtime.browse_step_result,
                (uint32_t)browse_result);
        }
        if (load_u32(&g_runtime.operation_mode) ==
                (uint32_t)AZ_OPERATION_MODE_FILTER &&
            load_u32(&g_runtime.filter_apply_state) ==
            AZ_FILTER_APPLY_ARMED) {
            const AzRev1655FilterConsumerResult step_result =
                az_rev1655_filter_consumer_worker_step(
                    &g_runtime.filter_consumer);

            store_u32(
                &g_runtime.filter_step_result,
                (uint32_t)step_result);
            if (step_result != AZ_REV1655_FILTER_CONSUMER_IDLE) {
                flush_filter_probe_telemetry();
            }
            if (step_result == AZ_REV1655_FILTER_CONSUMER_SCHEDULED) {
                az_rev1655_input_detour_publish_verification(
                    1u, 1u, 1u, 0u);
                az_rev1655_input_detour_confirm_controls(
                    AZ_INPUT_VERIFIED_REQUIRED);
                (void)az_rev1655_input_detour_request_stage(
                    AZ_INPUT_DETOUR_CONSUME);
                store_u32(
                    &g_runtime.filter_apply_state,
                    AZ_FILTER_APPLY_COOLDOWN);
                g_runtime.filter_cooldown_ticks = 0u;
                g_runtime.filter_idle_ticks = 0u;
                g_runtime.filter_activity_seen = 0u;
                flush_filter_probe_telemetry();
            }
            else if (step_result != AZ_REV1655_FILTER_CONSUMER_IDLE &&
                     step_result != AZ_REV1655_FILTER_CONSUMER_DEFERRED &&
                     step_result != AZ_REV1655_FILTER_CONSUMER_INPUT_BUSY) {
                az_rev1655_input_detour_publish_verification(
                    1u, 1u, 1u, 0u);
                az_rev1655_input_detour_confirm_controls(
                    AZ_INPUT_VERIFIED_REQUIRED);
                (void)az_rev1655_input_detour_request_stage(
                    AZ_INPUT_DETOUR_CONSUME);
                store_u32(
                    &g_runtime.filter_apply_state,
                    AZ_FILTER_APPLY_FAILED);
                flush_filter_probe_telemetry();
            }
        }
        else if (load_u32(&g_runtime.operation_mode) ==
                     (uint32_t)AZ_OPERATION_MODE_FILTER &&
                 load_u32(&g_runtime.filter_apply_state) ==
                 AZ_FILTER_APPLY_COOLDOWN) {
            uint8_t queue_idle;

            if (g_runtime.filter_cooldown_ticks <
                AZ_FILTER_FALLBACK_COOLDOWN_TICKS) {
                ++g_runtime.filter_cooldown_ticks;
            }
            queue_idle = filter_queue_is_demonstrably_idle(NULL);
            if (queue_idle == 0u) {
                if (g_runtime.filter_activity_seen == 0u) {
                    g_runtime.filter_activity_seen = 1u;
                    flush_filter_probe_telemetry();
                }
                g_runtime.filter_idle_ticks = 0u;
            }
            else if (g_runtime.filter_activity_seen != 0u ||
                     g_runtime.filter_cooldown_ticks >=
                        AZ_FILTER_FALLBACK_COOLDOWN_TICKS) {
                if (g_runtime.filter_idle_ticks <
                    AZ_FILTER_IDLE_STABLE_TICKS) {
                    ++g_runtime.filter_idle_ticks;
                }
            }
            else {
                g_runtime.filter_idle_ticks = 0u;
            }
            if ((g_runtime.filter_cooldown_ticks >=
                     AZ_FILTER_FALLBACK_COOLDOWN_TICKS ||
                 g_runtime.filter_activity_seen != 0u) &&
                g_runtime.filter_idle_ticks >=
                    AZ_FILTER_IDLE_STABLE_TICKS) {
                az_rev1655_input_detour_publish_verification(
                    1u, 1u, 1u, 1u);
                az_rev1655_input_detour_confirm_controls(
                    AZ_INPUT_VERIFIED_REQUIRED);
                if (az_rev1655_input_detour_request_stage(
                        AZ_INPUT_DETOUR_CONSUME) == AZ_INPUT_DETOUR_OK) {
                    store_u32(
                        &g_runtime.filter_step_result,
                        (uint32_t)AZ_REV1655_FILTER_CONSUMER_IDLE);
                    store_u32(
                        &g_runtime.filter_apply_state,
                        AZ_FILTER_APPLY_ARMED);
                    g_runtime.filter_cooldown_ticks = 0u;
                    g_runtime.filter_idle_ticks = 0u;
                    g_runtime.filter_activity_seen = 0u;
                }
                flush_filter_probe_telemetry();
            }
        }
        flush_render_telemetry(0u);
#endif
        wait_for_input();
    }

    if (load_u32(&g_runtime.state) ==
        (uint32_t)AZ_REV1655_RUNTIME_STOPPING) {
        AzInputDetourStatus status;

        /* Revoke capture and rendering before restoring hook targets. The
         * module remains title-pinned, so a direct callback already in flight
         * can still return safely after its target is restored. */
#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
        if (load_u32(&g_runtime.stage) ==
            (uint32_t)AZ_REV1655_RUNTIME_STAGE_OVERLAY_CANARY) {
            az_overlay_renderer_begin_unload(&g_runtime.renderer);
            (void)az_rev1655_browse_consumer_cancel(
                &g_runtime.browse_consumer);
            (void)az_rev1655_filter_consumer_worker_cancel(
                &g_runtime.filter_consumer);
        }
#endif
        az_module_settings_detour_begin_shutdown();
        az_rev1655_input_detour_begin_shutdown();
#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
        remove_overlay_hook_for_title_exit(
            &g_runtime.module_settings_hook, "ModuleSettings");
        remove_overlay_hook_for_title_exit(
            &g_runtime.content_launch_hook, "ContentLauncher");
        remove_overlay_hook_for_title_exit(
            &g_runtime.font_end_hook, "Font::End");
        remove_overlay_hook_for_title_exit(
            &g_runtime.render_menu_hook, "RenderMenu");
#endif
        remove_input_hook_safely();
        drain_all_observations();
        az_rev1655_input_detour_snapshot_status(&status);
        DbgPrint(
            "AuroraAZ: M2a input observe stopped calls=%u keys=%u "
            "drops=%u logged=%u consumed=%u\n",
            (unsigned int)status.main_calls,
            (unsigned int)status.successful_keys,
            (unsigned int)status.observation_drops,
            (unsigned int)load_u32(&g_runtime.observations_logged),
            (unsigned int)status.consumed_controls);
        store_u32(
            &g_runtime.stage,
            (uint32_t)AZ_REV1655_RUNTIME_STAGE_DISABLED);
        store_u32(
            &g_runtime.state,
            (uint32_t)AZ_REV1655_RUNTIME_CLOSED);
        flush_input_telemetry(1u);
#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
        flush_filter_probe_telemetry();
#endif
    }

    return 0u;
}

static void wait_for_worker_exit(void)
{
    if (g_runtime.worker_thread != NULL) {
        NTSTATUS wait_status;

        do {
            wait_status = NtWaitForSingleObjectEx(
                (uint32_t)(uintptr_t)g_runtime.worker_thread,
                0u,
                0u,
                NULL);
        } while (FAILED(wait_status));

        (void)NtClose(g_runtime.worker_thread);
        g_runtime.worker_thread = NULL;
    }
}

static void stop_starting_worker(void)
{
    /* The resumed worker is waiting on STARTING. Release it and join before
     * any caller may unload module code. */
    store_u32(
        &g_runtime.state,
        (uint32_t)AZ_REV1655_RUNTIME_STOPPED);
    wait_for_worker_exit();
}

static AzRev1655RuntimeResult start_input_observe(void)
{
    AzRev1655LoadedImage image;
    AzRev1655ResolvedHookSite resolved;
    AzRev1655RuntimeResult validation;
    AzHookRuntimeResult hook_result;
    AzInputDetourResult detour_result;
    AzRev1655ThreadCreateResult create_result;
    HANDLE worker_thread = NULL;

    validation = validate_input_site(&image, &resolved, 1u, NULL);
    if (validation != AZ_REV1655_RUNTIME_OK) {
        return validation;
    }

    if (az_rev1655_thread_wrapper_is_valid() == 0u) {
        DbgPrint("AuroraAZ: M2a thread-wrapper probe rejected\n");
        return AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED;
    }

    az_rev1655_input_detour_reset();
    az_rev1655_input_detour_publish_verification(1u, 0u, 0u, 0u);

    create_result = az_rev1655_thread_create(
        (void *)(uintptr_t)&input_observe_worker,
        NULL,
        &worker_thread);
    if (create_result != AZ_REV1655_THREAD_CREATE_OK) {
        az_rev1655_input_detour_publish_verification(0u, 0u, 0u, 0u);
        if (create_result ==
            AZ_REV1655_THREAD_CREATE_REVISION_MISMATCH) {
            DbgPrint("AuroraAZ: M2a thread wrapper changed before call\n");
            return AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED;
        }
        DbgPrint("AuroraAZ: M2a worker creation failed\n");
        return AZ_REV1655_RUNTIME_THREAD_CREATE_FAILED;
    }
    g_runtime.worker_thread = worker_thread;

    hook_result = az_live_hook_install_direct(
        resolved.target_address,
        resolved.expected_instruction,
        (const void *)(uintptr_t)&az_rev1655_input_direct_detour_entry,
        &g_runtime.input_hook);
    if (hook_result != AZ_HOOK_RUNTIME_OK) {
        az_rev1655_input_detour_publish_verification(0u, 0u, 0u, 0u);
        stop_starting_worker();
        DbgPrint(
            "AuroraAZ: M2a input hook rejected: %s\n",
            az_hook_runtime_result_name(hook_result));
        return AZ_REV1655_RUNTIME_HOOK_INSTALL_FAILED;
    }

    /* Render, filter, control-confirmation, and scene gates remain false. */
    az_rev1655_input_detour_publish_verification(1u, 1u, 0u, 0u);
    detour_result = az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_OBSERVE);
    if (detour_result != AZ_INPUT_DETOUR_OK) {
        rollback_installed_input_hook();
        stop_starting_worker();
        DbgPrint(
            "AuroraAZ: M2a observe stage rejected: %s\n",
            az_input_detour_result_name(detour_result));
        return AZ_REV1655_RUNTIME_DETOUR_STAGE_FAILED;
    }

    store_u32(
        &g_runtime.stage,
        (uint32_t)AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE);
    /* Aurora's wrapper has resumed the worker, which waits in STARTING while
     * the hook and observe gate are prepared. This release-store is the only
     * publication that lets it touch the runtime. */
    store_u32(
        &g_runtime.state,
        (uint32_t)AZ_REV1655_RUNTIME_RUNNING);
    return AZ_REV1655_RUNTIME_OK;
}

#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
static void retain_published_hooks_fail_closed(uint8_t render_verified)
{
    /* Direct hooks have no admission counter. Once any one-word branch has
     * been published, retain all backing code/state until cold title exit;
     * never race a returning callback with hot rollback or reinitialization. */
    az_rev1655_input_detour_publish_verification(
        1u,
        g_runtime.input_hook.installed != 0u ? 1u : 0u,
        render_verified,
        0u);
    if (g_runtime.input_hook.installed != 0u) {
        (void)az_rev1655_input_detour_request_stage(
            AZ_INPUT_DETOUR_OBSERVE);
    }
    store_u32(
        &g_runtime.stage,
        (uint32_t)AZ_REV1655_RUNTIME_STAGE_OVERLAY_CANARY);
    store_u32(
        &g_runtime.state,
        (uint32_t)AZ_REV1655_RUNTIME_RUNNING);
}

static AzRev1655RuntimeResult start_overlay_canary(void)
{
    AzRev1655LoadedImage image;
    AzRev1655ResolvedHookSite input_site;
    AzRev1655ResolvedHookSite render_menu_site;
    AzRev1655ResolvedHookSite font_end_site;
    AzRev1655ResolvedHookSite content_launch_site;
    AzRev1655ResolvedHookSite module_settings_site;
    AzRev1655RenderDetourBindings bindings;
    AzRev1655RuntimeResult validation;
    AzOverlayRendererResult renderer_result;
    AzSceneGateConfigureResult scene_result;
    AzRenderDetourResult render_detour_result;
    AzHookRuntimeResult hook_result;
    AzInputDetourResult input_result;
    AzRev1655ThreadCreateResult create_result;
    AzRev1655FilterConsumerResult filter_bind_result;
    AzRev1655BrowseResult browse_bind_result;
    const AzRev1655HookPermit *revision_permit = NULL;
    uint32_t wait_tick;
    HANDLE worker_thread = NULL;

    validation = validate_input_site(
        &image, &input_site, 1u, &revision_permit);
    if (validation != AZ_REV1655_RUNTIME_OK) {
        return validation;
    }
    store_u32(
        &g_runtime.module_label_result,
        rename_netdbg_module_row() != 0u ? 1u : 2u);
    if (load_u32(&g_runtime.module_label_result) != 1u) {
        DbgPrint("AuroraAZ: Configure Modules row rename failed\n");
    }
    store_u32(
        &g_runtime.icon_cache_result,
        write_complete_file(
            g_icon_cache_path,
            g_auroraaz_embedded_icon_png,
            g_auroraaz_embedded_icon_png_size) != 0u ? 1u : 2u);
    store_u32(
        &g_runtime.settings_xur_cache_result,
        write_complete_file(
            g_settings_xur_cache_path,
            g_auroraaz_embedded_settings_xur,
            g_auroraaz_embedded_settings_xur_size) != 0u ? 1u : 2u);
    if (load_u32(&g_runtime.settings_xur_cache_result) != 1u) {
        DbgPrint("AuroraAZ: embedded settings resource cache failed\n");
        return AZ_REV1655_RUNTIME_HOOK_INSTALL_FAILED;
    }
    validation = validate_overlay_sites(
        &image,
        &render_menu_site,
        &font_end_site,
        &content_launch_site,
        &module_settings_site);
    if (validation != AZ_REV1655_RUNTIME_OK) {
        DbgPrint("AuroraAZ: overlay integration sites rejected\n");
        return validation;
    }

    renderer_result = az_overlay_renderer_init_rev1655(
        &g_runtime.renderer,
        (const uint8_t *)(uintptr_t)AZ_REV1655_TEXT_BASE,
        (size_t)AZ_REV1655_TEXT_SIZE,
        AZ_REV1655_TEXT_BASE,
        1u,
        &overlay_address_is_valid,
        &overlay_system_ui_is_active);
    if (renderer_result != AZ_OVERLAY_RENDERER_OK) {
        DbgPrint(
            "AuroraAZ: M2b renderer rejected: %s\n",
            az_overlay_renderer_result_name(renderer_result));
        return AZ_REV1655_RUNTIME_RENDER_INIT_FAILED;
    }

    az_rev1655_scene_gate_reset();
    scene_result = az_rev1655_scene_gate_configure_default(1u);
    if (scene_result != AZ_SCENE_GATE_CONFIGURE_OK) {
        DbgPrint(
            "AuroraAZ: M2b scene gate rejected: %s\n",
            az_scene_gate_configure_result_name(scene_result));
        return AZ_REV1655_RUNTIME_SCENE_GATE_FAILED;
    }

    memset(&bindings, 0, sizeof(bindings));
    bindings.renderer = &g_runtime.renderer;
    bindings.note_overlay = &az_overlay_renderer_note_render_menu;
    bindings.note_input = &az_rev1655_input_detour_note_render;
    bindings.try_draw = &az_overlay_renderer_try_draw;
    bindings.release_texture = &az_overlay_renderer_release_texture;
    bindings.snapshot_selector =
        &az_rev1655_input_detour_snapshot_selector;
    bindings.snapshot_input_status = &snapshot_input_with_current_scene;
    bindings.viewport_width = 1280.0f;
    bindings.viewport_height = 720.0f;
    az_rev1655_render_detours_reset();
    render_detour_result = az_rev1655_render_detours_configure(&bindings);
    if (render_detour_result != AZ_RENDER_DETOUR_OK) {
        DbgPrint(
            "AuroraAZ: M2b detour bridge rejected: %s\n",
            az_render_detour_result_name(render_detour_result));
        return AZ_REV1655_RUNTIME_RENDER_DETOUR_FAILED;
    }

    if (az_rev1655_thread_wrapper_is_valid() == 0u) {
        return AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED;
    }
    az_rev1655_input_detour_reset();
    az_rev1655_input_detour_configure_browse_jump(
        &browse_apply_jump, NULL);
    az_rev1655_input_detour_configure_ui_tick(
        &module_ui_tick, NULL);
    az_rev1655_input_detour_configure_ui_input(
        &module_settings_ui_input, NULL);
    az_module_settings_detour_reset();
    az_rev1655_input_detour_publish_verification(1u, 0u, 0u, 0u);
    create_result = az_rev1655_thread_create(
        (void *)(uintptr_t)&input_observe_worker,
        NULL,
        &worker_thread);
    if (create_result != AZ_REV1655_THREAD_CREATE_OK) {
        az_rev1655_input_detour_publish_verification(0u, 0u, 0u, 0u);
        return create_result ==
            AZ_REV1655_THREAD_CREATE_REVISION_MISMATCH ?
                AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED :
                AZ_REV1655_RUNTIME_THREAD_CREATE_FAILED;
    }
    g_runtime.worker_thread = worker_thread;

    /* The initial whole-image gate issued revision_permit before setup began.
     * Bind reuses that exact-image permit and rechecks all filter helper
     * windows, avoiding a racy redundant whole-text hash while Aurora's own
     * startup workers are active. This still precedes our first direct hook. */
    for (wait_tick = 0u; wait_tick < AZ_FILTER_THREAD_WAIT_TICKS; ++wait_tick) {
        if (load_u32(&g_runtime.worker_thread_id) != 0u) {
            break;
        }
        wait_for_control();
    }
    if (load_u32(&g_runtime.worker_thread_id) == 0u) {
        filter_bind_result = AZ_REV1655_FILTER_CONSUMER_NOT_WORKER;
    }
    else {
        filter_bind_result = bind_filter_consumer_pristine(
            &image,
            revision_permit,
            load_u32(&g_runtime.worker_thread_id));
    }
    store_u32(
        &g_runtime.filter_bind_result,
        (uint32_t)filter_bind_result);
    browse_bind_result = bind_browse_consumer();
    store_u32(
        &g_runtime.browse_bind_result,
        (uint32_t)browse_bind_result);

    hook_result = az_live_hook_install_direct(
        input_site.target_address,
        input_site.expected_instruction,
        (const void *)(uintptr_t)&az_rev1655_input_direct_detour_entry,
        &g_runtime.input_hook);
    if (hook_result != AZ_HOOK_RUNTIME_OK) {
        az_rev1655_input_detour_publish_verification(0u, 0u, 0u, 0u);
        stop_starting_worker();
        return AZ_REV1655_RUNTIME_HOOK_INSTALL_FAILED;
    }

    hook_result = az_live_hook_install_direct(
        render_menu_site.target_address,
        render_menu_site.expected_instruction,
        (const void *)(uintptr_t)
            &az_rev1655_render_menu_direct_detour_entry,
        &g_runtime.render_menu_hook);
    if (hook_result != AZ_HOOK_RUNTIME_OK) {
        retain_published_hooks_fail_closed(0u);
        return AZ_REV1655_RUNTIME_HOOK_INSTALL_FAILED;
    }

    hook_result = az_live_hook_install_direct(
        font_end_site.target_address,
        font_end_site.expected_instruction,
        (const void *)(uintptr_t)&az_rev1655_font_end_direct_detour_entry,
        &g_runtime.font_end_hook);
    if (hook_result != AZ_HOOK_RUNTIME_OK) {
        retain_published_hooks_fail_closed(0u);
        return AZ_REV1655_RUNTIME_HOOK_INSTALL_FAILED;
    }

    hook_result = az_live_hook_install_direct(
        content_launch_site.target_address,
        content_launch_site.expected_instruction,
        (const void *)(uintptr_t)
            &az_rev1655_content_launch_direct_detour_entry,
        &g_runtime.content_launch_hook);
    if (hook_result != AZ_HOOK_RUNTIME_OK) {
        retain_published_hooks_fail_closed(1u);
        return AZ_REV1655_RUNTIME_HOOK_INSTALL_FAILED;
    }

    hook_result = az_live_hook_install_direct(
        module_settings_site.target_address,
        module_settings_site.expected_instruction,
        (const void *)(uintptr_t)
            &az_rev1655_module_settings_direct_detour_entry,
        &g_runtime.module_settings_hook);
    if (hook_result != AZ_HOOK_RUNTIME_OK) {
        retain_published_hooks_fail_closed(1u);
        return AZ_REV1655_RUNTIME_HOOK_INSTALL_FAILED;
    }

    /* M2a hardware telemetry already proved every requested virtual key. The
     * selector may own only held R3 and horizontal navigation. A remains
     * entirely Aurora-owned; releasing R3 commits only after the independently
     * gated filter worker is verified. */
    az_rev1655_input_detour_publish_verification(
        1u,
        1u,
        1u,
        load_u32(&g_runtime.operation_mode) ==
                (uint32_t)AZ_OPERATION_MODE_BROWSE &&
            browse_bind_result == AZ_REV1655_BROWSE_IDLE ? 1u : 0u);
    az_rev1655_input_detour_confirm_controls(AZ_INPUT_VERIFIED_REQUIRED);
    input_result = az_rev1655_input_detour_request_stage(
        AZ_INPUT_DETOUR_CONSUME);
    if (input_result != AZ_INPUT_DETOUR_OK) {
        retain_published_hooks_fail_closed(1u);
        return AZ_REV1655_RUNTIME_DETOUR_STAGE_FAILED;
    }

    store_u32(
        &g_runtime.stage,
        (uint32_t)AZ_REV1655_RUNTIME_STAGE_OVERLAY_CANARY);
    store_u32(
        &g_runtime.state,
        (uint32_t)AZ_REV1655_RUNTIME_RUNNING);
    return AZ_REV1655_RUNTIME_OK;
}
#endif

AzRev1655RuntimeResult az_rev1655_runtime_start(
    AzRev1655RuntimeStage stage)
{
    uint32_t expected;
    AzRev1655RuntimeResult result;

    if (stage != AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE &&
        stage != AZ_REV1655_RUNTIME_STAGE_OVERLAY_CANARY) {
        return AZ_REV1655_RUNTIME_BAD_STAGE;
    }
    if (load_u32(&g_runtime.lifetime_state) != AZ_LIFETIME_PINNED ||
        load_u32(&g_runtime.pinned_ordinal4_export) !=
            (uint32_t)(uintptr_t)&AuroraAZNetDbgWrite) {
        return AZ_REV1655_RUNTIME_LIFETIME_REJECTED;
    }

    expected = load_u32(&g_runtime.state);
    if (expected == (uint32_t)AZ_REV1655_RUNTIME_RUNNING &&
        load_u32(&g_runtime.stage) == (uint32_t)stage) {
        return AZ_REV1655_RUNTIME_OK;
    }
    if (expected == (uint32_t)AZ_REV1655_RUNTIME_CLOSED) {
        return AZ_REV1655_RUNTIME_CLOSED_RESULT;
    }
    if (expected != (uint32_t)AZ_REV1655_RUNTIME_STOPPED ||
        !__atomic_compare_exchange_n(
            &g_runtime.state,
            &expected,
            (uint32_t)AZ_REV1655_RUNTIME_STARTING,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        return AZ_REV1655_RUNTIME_BUSY;
    }

    store_u32(&g_runtime.observations_logged, 0u);
#if !defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
    {
        AzOperationMode mode = AZ_OPERATION_MODE_BROWSE;
        (void)read_operation_mode(&mode);
        store_u32(&g_runtime.operation_mode, (uint32_t)mode);
    }
    store_u32(
        &g_runtime.browse_bind_result,
        (uint32_t)AZ_REV1655_BROWSE_BAD_BINDINGS);
    store_u32(
        &g_runtime.browse_step_result,
        (uint32_t)AZ_REV1655_BROWSE_IDLE);
    store_u32(&g_runtime.browse_apply_reason, AZ_BROWSE_APPLY_NONE);
    store_u32(&g_runtime.browse_apply_target, UINT32_MAX);
    store_u32(&g_runtime.browse_apply_current, UINT32_MAX);
    store_u32(&g_runtime.browse_apply_count, 0u);
    memset(&g_runtime.browse_consumer, 0,
        sizeof(g_runtime.browse_consumer));
    g_runtime.settings_dialog_active = 0u;
    g_runtime.settings_a_owned = 0u;
    store_u32(&g_runtime.settings_resolve_result, 0u);
    store_u32(&g_runtime.settings_call_result, UINT32_MAX);
    store_u32(&g_runtime.settings_completions, 0u);
    store_u32(&g_runtime.module_label_result, 0u);
    store_u32(&g_runtime.icon_cache_result, 0u);
    store_u32(&g_runtime.settings_xur_cache_result, 0u);
    store_u32(&g_runtime.icon_apply_result, 0u);
    store_u32(&g_runtime.icon_ui_ticks, 0u);
    g_runtime.xui_get_first_child = NULL;
    g_runtime.xui_get_next = NULL;
    g_runtime.xui_get_descendant = NULL;
    g_runtime.xui_get_text = NULL;
    g_runtime.xui_set_text = NULL;
    g_runtime.xui_set_image_path = NULL;
    g_runtime.xui_has_focus = NULL;
    store_u32(&g_runtime.worker_thread_id, 0u);
    store_u32(
        &g_runtime.filter_bind_result,
        (uint32_t)AZ_REV1655_FILTER_CONSUMER_NOT_BOUND);
    store_u32(
        &g_runtime.filter_probe_result,
        (uint32_t)AZ_REV1655_FILTER_CONSUMER_NOT_BOUND);
    store_u32(
        &g_runtime.filter_step_result,
        (uint32_t)AZ_REV1655_FILTER_CONSUMER_IDLE);
    store_u32(
        &g_runtime.filter_apply_state,
        AZ_FILTER_APPLY_UNARMED);
    g_runtime.filter_cooldown_ticks = 0u;
    g_runtime.filter_idle_ticks = 0u;
    g_runtime.filter_activity_seen = 0u;
    memset(&g_runtime.filter_consumer, 0, sizeof(g_runtime.filter_consumer));
#endif
    az_m2a_input_telemetry_init(&g_runtime.telemetry);
    g_runtime.telemetry_flush_ticks = 0u;
    g_runtime.render_telemetry_flush_ticks = 0u;
    g_runtime.m3c_telemetry_flush_ticks = 0u;
    store_u32(&g_runtime.shutdown_requests, 0u);
#if defined(AURORAAZ_REV1655_RUNTIME_TEST_IO)
    if (stage == AZ_REV1655_RUNTIME_STAGE_OVERLAY_CANARY) {
        result = AZ_REV1655_RUNTIME_BAD_STAGE;
    }
    else {
        result = start_input_observe();
    }
#else
    if (stage == AZ_REV1655_RUNTIME_STAGE_OVERLAY_CANARY) {
        result = start_overlay_canary();
    }
    else {
        result = start_input_observe();
    }
#endif
    if (result != AZ_REV1655_RUNTIME_OK &&
        load_u32(&g_runtime.state) ==
            (uint32_t)AZ_REV1655_RUNTIME_STARTING) {
        store_u32(
            &g_runtime.stage,
            (uint32_t)AZ_REV1655_RUNTIME_STAGE_DISABLED);
        store_u32(
            &g_runtime.state,
            (uint32_t)AZ_REV1655_RUNTIME_STOPPED);
    }
    return result;
}

void az_rev1655_runtime_shutdown(void)
{
    for (;;) {
        uint32_t state = load_u32(&g_runtime.state);

        if (state == (uint32_t)AZ_REV1655_RUNTIME_STOPPED ||
            state == (uint32_t)AZ_REV1655_RUNTIME_CLOSED) {
            return;
        }
        if (state == (uint32_t)AZ_REV1655_RUNTIME_STARTING) {
            wait_for_control();
            continue;
        }
        if (state == (uint32_t)AZ_REV1655_RUNTIME_STOPPING) {
            break;
        }
        if (state == (uint32_t)AZ_REV1655_RUNTIME_RUNNING) {
            uint32_t expected =
                (uint32_t)AZ_REV1655_RUNTIME_RUNNING;

            if (__atomic_compare_exchange_n(
                    &g_runtime.state,
                    &expected,
                    (uint32_t)AZ_REV1655_RUNTIME_STOPPING,
                    0,
                    __ATOMIC_ACQ_REL,
                    __ATOMIC_ACQUIRE)) {
                break;
            }
            continue;
        }

        return;
    }

    wait_for_worker_exit();
}

void az_rev1655_runtime_request_shutdown(void)
{
    uint32_t expected = (uint32_t)AZ_REV1655_RUNTIME_RUNNING;

    (void)__atomic_add_fetch(
        &g_runtime.shutdown_requests,
        1u,
        __ATOMIC_ACQ_REL);
    (void)__atomic_compare_exchange_n(
        &g_runtime.state,
        &expected,
        (uint32_t)AZ_REV1655_RUNTIME_STOPPING,
        0,
        __ATOMIC_ACQ_REL,
        __ATOMIC_ACQUIRE);
}

AzRev1655RuntimeState az_rev1655_runtime_state(void)
{
    return (AzRev1655RuntimeState)load_u32(&g_runtime.state);
}

AzRev1655RuntimeStage az_rev1655_runtime_stage(void)
{
    return (AzRev1655RuntimeStage)load_u32(&g_runtime.stage);
}

uint32_t az_rev1655_runtime_observations_logged(void)
{
    return load_u32(&g_runtime.observations_logged);
}

const char *az_rev1655_runtime_result_name(AzRev1655RuntimeResult result)
{
    switch (result) {
    case AZ_REV1655_RUNTIME_OK:
        return "ok";
    case AZ_REV1655_RUNTIME_BAD_STAGE:
        return "bad-stage";
    case AZ_REV1655_RUNTIME_BUSY:
        return "busy";
    case AZ_REV1655_RUNTIME_CLOSED_RESULT:
        return "closed";
    case AZ_REV1655_RUNTIME_IMAGE_UNMAPPED:
        return "image-unmapped";
    case AZ_REV1655_RUNTIME_IMAGE_REJECTED:
        return "image-rejected";
    case AZ_REV1655_RUNTIME_SITE_REJECTED:
        return "site-rejected";
    case AZ_REV1655_RUNTIME_WRONG_INPUT_SITE:
        return "wrong-input-site";
    case AZ_REV1655_RUNTIME_LIFETIME_REJECTED:
        return "lifetime-rejected";
    case AZ_REV1655_RUNTIME_ARENA_FAILED:
        return "arena-failed";
    case AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED:
        return "thread-startup-rejected";
    case AZ_REV1655_RUNTIME_THREAD_CREATE_FAILED:
        return "thread-create-failed";
    case AZ_REV1655_RUNTIME_HOOK_INSTALL_FAILED:
        return "hook-install-failed";
    case AZ_REV1655_RUNTIME_DETOUR_STAGE_FAILED:
        return "detour-stage-failed";
    case AZ_REV1655_RUNTIME_RENDER_SITE_REJECTED:
        return "render-site-rejected";
    case AZ_REV1655_RUNTIME_RENDER_INIT_FAILED:
        return "render-init-failed";
    case AZ_REV1655_RUNTIME_SCENE_GATE_FAILED:
        return "scene-gate-failed";
    case AZ_REV1655_RUNTIME_RENDER_DETOUR_FAILED:
        return "render-detour-failed";
    case AZ_REV1655_RUNTIME_LAUNCH_SITE_REJECTED:
        return "launch-site-rejected";
    case AZ_REV1655_RUNTIME_SETTINGS_SITE_REJECTED:
        return "settings-site-rejected";
    default:
        return "unknown";
    }
}
