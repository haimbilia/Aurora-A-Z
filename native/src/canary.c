#if !defined(AURORAAZ_XBOX360)
#error "canary.c must only be built for the Xbox 360 target"
#endif

#include <stdint.h>
#include <stddef.h>

#include <xecore/xboxkrnl.h>

#include <auroraaz/compatibility.h>
#include <auroraaz/image.h>

#define AZ_DLL_PROCESS_DETACH 0u
#define AZ_DLL_PROCESS_ATTACH 1u
#define AZ_SYSTEM_THREAD_FLAG 2u
#define AZ_POLL_INTERVAL_100NS (-5000000LL)
#define AZ_MAX_POLLS 120u

typedef struct AzLoaderEntry {
    uint8_t reserved_00[0x18];
    uint32_t dll_base;
    uint32_t image_base;
    uint32_t image_size;
    uint8_t reserved_24[0x14];
    uint32_t full_image_size;
    uint32_t entry_point;
} AzLoaderEntry;

typedef char AzLoaderEntryImageBaseOffset[
    offsetof(AzLoaderEntry, image_base) == 0x1Cu ? 1 : -1];
typedef char AzLoaderEntryFullImageSizeOffset[
    offsetof(AzLoaderEntry, full_image_size) == 0x38u ? 1 : -1];
typedef char AzLoaderEntryEntryPointOffset[
    offsetof(AzLoaderEntry, entry_point) == 0x3Cu ? 1 : -1];

typedef struct AzCanaryResult {
    AzImageResult image;
    AzCompatibilityResult compatibility;
} AzCanaryResult;

static uint32_t g_running = 0u;
static HANDLE g_monitor_thread = NULL;

static HMODULE find_running_aurora(void)
{
    HMODULE aurora_module = NULL;
    NTSTATUS status;

    status = XexGetModuleHandle("Aurora.exe", &aurora_module);
    if (FAILED(status) || aurora_module == NULL) {
        status = XexGetModuleHandle("Aurora.xex", &aurora_module);
    }

    if (FAILED(status) || aurora_module == NULL) {
        return NULL;
    }

    return aurora_module;
}

static AzCanaryResult validate_running_aurora(HMODULE aurora_module)
{
    const AzLoaderEntry *loader;
    const uint8_t *image;
    const uint8_t *text = NULL;
    size_t text_size = 0u;
    AzCanaryResult result = {
        AZ_IMAGE_NULL,
        AZ_COMPAT_BAD_TEXT_BASE
    };

    if (aurora_module == NULL ||
        !MmIsAddressValid(aurora_module) ||
        !MmIsAddressValid(
            (uint8_t *)aurora_module + sizeof(AzLoaderEntry) - 1u)) {
        return result;
    }

    loader = (const AzLoaderEntry *)aurora_module;
    if (loader->image_base != AZ_REV1655_IMAGE_BASE ||
        loader->entry_point != AZ_REV1655_ENTRY_POINT) {
        result.image = AZ_IMAGE_BAD_IDENTITY;
        return result;
    }
    if (loader->image_size != AZ_REV1655_NT_IMAGE_SIZE ||
        loader->full_image_size != AZ_REV1655_FULL_IMAGE_SIZE) {
        result.image = AZ_IMAGE_BAD_SIZE;
        return result;
    }

    image = (const uint8_t *)(uintptr_t)loader->image_base;
    if (!MmIsAddressValid((void *)image) ||
        !MmIsAddressValid((void *)(image + 0x3FFu))) {
        return result;
    }

    result.image = az_locate_rev1655_text(
        image,
        (size_t)loader->image_size,
        &text,
        &text_size);
    if (result.image != AZ_IMAGE_REV1655 || text == NULL ||
        text_size == 0u ||
        !MmIsAddressValid((void *)text) ||
        !MmIsAddressValid((void *)(text + text_size - 1u))) {
        return result;
    }

    result.compatibility = az_validate_rev1655_text(
        text,
        text_size,
        (uint32_t)(uintptr_t)text);
    return result;
}

static uint32_t monitor_aurora(void *context)
{
    uint32_t poll;

    (void)context;

    for (poll = 0u;
         poll < AZ_MAX_POLLS &&
            __atomic_load_n(&g_running, __ATOMIC_ACQUIRE) != 0u;
         ++poll) {
        HMODULE current_module = find_running_aurora();

        if (current_module != NULL) {
            const AzCanaryResult result =
                validate_running_aurora(current_module);
            DbgPrint(
                "AuroraAZ: canary found Aurora, image=%s, compatibility=%s\n",
                az_image_result_name(result.image),
                az_compatibility_result_name(result.compatibility));
            break;
        }

        {
            int64_t interval = AZ_POLL_INTERVAL_100NS;
            (void)KeDelayExecutionThread(0u, 0u, &interval);
        }
    }

    if (poll == AZ_MAX_POLLS) {
        DbgPrint("AuroraAZ: canary timed out waiting for Aurora\n");
    }

    __atomic_store_n(&g_running, 0u, __ATOMIC_RELEASE);
    return 0u;
}

int DllMain(void *module, uint32_t reason, void *reserved)
{
    (void)module;
    (void)reserved;

    if (reason == AZ_DLL_PROCESS_ATTACH) {
        NTSTATUS status;

        __atomic_store_n(&g_running, 1u, __ATOMIC_RELEASE);
        status = ExCreateThread(
            &g_monitor_thread,
            0u,
            NULL,
            NULL,
            (void *)(uintptr_t)&monitor_aurora,
            NULL,
            AZ_SYSTEM_THREAD_FLAG);
        if (FAILED(status)) {
            __atomic_store_n(&g_running, 0u, __ATOMIC_RELEASE);
            g_monitor_thread = NULL;
            DbgPrint("AuroraAZ: canary monitor start failed, status=%08X\n", status);
        }
        else {
            DbgPrint("AuroraAZ: canary monitor started\n");
        }
    }
    else if (reason == AZ_DLL_PROCESS_DETACH) {
        __atomic_store_n(&g_running, 0u, __ATOMIC_RELEASE);

        if (g_monitor_thread != NULL) {
            (void)NtWaitForSingleObjectEx(
                (uint32_t)(uintptr_t)g_monitor_thread,
                0u,
                0u,
                NULL);
            (void)NtClose(g_monitor_thread);
            g_monitor_thread = NULL;
        }

        DbgPrint("AuroraAZ: canary detach\n");
    }

    return 1;
}
