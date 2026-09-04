#include <stddef.h>
#include <stdint.h>

#include <auroraaz/module_settings_detour.h>

typedef struct AzModuleSettingsBridge {
    volatile uint32_t pending;
    volatile uint32_t disabled;
} AzModuleSettingsBridge;

static AzModuleSettingsBridge g_settings_bridge;

void az_module_settings_detour_reset(void)
{
    __atomic_store_n(&g_settings_bridge.pending, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&g_settings_bridge.disabled, 0u, __ATOMIC_RELEASE);
}

void az_module_settings_detour_begin_shutdown(void)
{
    __atomic_store_n(&g_settings_bridge.disabled, 1u, __ATOMIC_RELEASE);
    __atomic_store_n(&g_settings_bridge.pending, 0u, __ATOMIC_RELEASE);
}

uint8_t az_module_settings_detour_take_request(void)
{
    if (__atomic_load_n(
            &g_settings_bridge.disabled,
            __ATOMIC_ACQUIRE) != 0u) {
        return 0u;
    }
    return __atomic_exchange_n(
        &g_settings_bridge.pending,
        0u,
        __ATOMIC_ACQ_REL) != 0u ? 1u : 0u;
}

void az_rev1655_module_settings_detour_c(void)
{
    if (__atomic_load_n(
            &g_settings_bridge.disabled,
            __ATOMIC_ACQUIRE) == 0u) {
        __atomic_store_n(&g_settings_bridge.pending, 1u, __ATOMIC_RELEASE);
    }
}
