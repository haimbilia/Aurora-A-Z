#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <auroraaz/module_settings_detour.h>

typedef struct AzModuleSettingsBridge {
    volatile uint32_t pending_mode;
    volatile uint32_t disabled;
    volatile uint32_t hook_calls;
    volatile uint32_t requests_taken;
} AzModuleSettingsBridge;

static AzModuleSettingsBridge g_settings_bridge;

uint8_t az_module_settings_write_label(
    uint8_t *wstring_object,
    uint16_t *external_storage,
    uint32_t external_code_units)
{
    static const uint16_t label[AZ_MODULE_SETTINGS_LABEL_LENGTH + 1u] = {
        (uint16_t)'A', (uint16_t)'u', (uint16_t)'r', (uint16_t)'o',
        (uint16_t)'r', (uint16_t)'a', (uint16_t)' ', (uint16_t)'A',
        (uint16_t)'-', (uint16_t)'Z', 0u
    };
    uint32_t capacity;

    if (wstring_object == NULL || external_storage == NULL) {
        return 0u;
    }
    memcpy(&capacity, wstring_object + 0x14u, sizeof(capacity));
    if (capacity < AZ_MODULE_SETTINGS_LABEL_LENGTH ||
        external_code_units < AZ_MODULE_SETTINGS_LABEL_LENGTH + 1u) {
        return 0u;
    }
    memcpy(external_storage, label, sizeof(label));
    capacity = AZ_MODULE_SETTINGS_LABEL_LENGTH;
    memcpy(wstring_object + 0x10u, &capacity, sizeof(capacity));
    return 1u;
}

void az_module_settings_detour_reset(void)
{
    __atomic_store_n(&g_settings_bridge.pending_mode, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&g_settings_bridge.disabled, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&g_settings_bridge.hook_calls, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&g_settings_bridge.requests_taken, 0u, __ATOMIC_RELEASE);
}

void az_module_settings_detour_begin_shutdown(void)
{
    __atomic_store_n(&g_settings_bridge.disabled, 1u, __ATOMIC_RELEASE);
    __atomic_store_n(&g_settings_bridge.pending_mode, 0u, __ATOMIC_RELEASE);
}

const uint16_t *az_module_settings_scene_path(void)
{
    static const uint16_t path[] = {
        (uint16_t)'g', (uint16_t)'a', (uint16_t)'m', (uint16_t)'e',
        (uint16_t)':', (uint16_t)'\\', (uint16_t)'D', (uint16_t)'a',
        (uint16_t)'t', (uint16_t)'a', (uint16_t)'\\',
        (uint16_t)'A', (uint16_t)'u', (uint16_t)'r', (uint16_t)'o',
        (uint16_t)'r', (uint16_t)'a', (uint16_t)'A', (uint16_t)'Z',
        (uint16_t)'_', (uint16_t)'S', (uint16_t)'e', (uint16_t)'t',
        (uint16_t)'t', (uint16_t)'i', (uint16_t)'n', (uint16_t)'g',
        (uint16_t)'s', (uint16_t)'.', (uint16_t)'x', (uint16_t)'u',
        (uint16_t)'r', 0u
    };

    (void)__atomic_add_fetch(
        &g_settings_bridge.hook_calls, 1u, __ATOMIC_ACQ_REL);
    return path;
}

uint8_t az_module_settings_request_mode(uint32_t mode)
{
    if (mode > AZ_MODULE_SETTINGS_MODE_FILTER ||
        __atomic_load_n(
            &g_settings_bridge.disabled,
            __ATOMIC_ACQUIRE) != 0u) {
        return 0u;
    }
    __atomic_store_n(
        &g_settings_bridge.pending_mode,
        mode + 1u,
        __ATOMIC_RELEASE);
    return 1u;
}

uint8_t az_module_settings_take_mode_request(uint32_t *mode)
{
    uint32_t pending;

    if (mode == NULL || __atomic_load_n(
            &g_settings_bridge.disabled,
            __ATOMIC_ACQUIRE) != 0u) {
        return 0u;
    }
    pending = __atomic_exchange_n(
        &g_settings_bridge.pending_mode,
        0u,
        __ATOMIC_ACQ_REL);
    if (pending == 0u || pending > AZ_MODULE_SETTINGS_MODE_FILTER + 1u) {
        return 0u;
    }
    *mode = pending - 1u;
    (void)__atomic_add_fetch(
        &g_settings_bridge.requests_taken, 1u, __ATOMIC_ACQ_REL);
    return 1u;
}

void az_module_settings_detour_snapshot_status(
    AzModuleSettingsDetourStatus *status)
{
    if (status == NULL) {
        return;
    }
    status->hook_calls = __atomic_load_n(
        &g_settings_bridge.hook_calls, __ATOMIC_ACQUIRE);
    status->requests_taken = __atomic_load_n(
        &g_settings_bridge.requests_taken, __ATOMIC_ACQUIRE);
    status->pending = __atomic_load_n(
        &g_settings_bridge.pending_mode, __ATOMIC_ACQUIRE) != 0u ? 1u : 0u;
    status->disabled = __atomic_load_n(
        &g_settings_bridge.disabled, __ATOMIC_ACQUIRE) != 0u ? 1u : 0u;
}
