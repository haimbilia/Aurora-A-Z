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
#define AZ_MONITOR_INTERVAL_100NS (-1000000LL)

typedef struct AzCanaryResult {
    AzImageResult image;
    AzCompatibilityResult compatibility;
} AzCanaryResult;

static uint32_t g_running = 0u;
static HANDLE g_monitor_thread = NULL;

static AzCanaryResult validate_running_aurora(void)
{
    const uint8_t *image =
        (const uint8_t *)(uintptr_t)AZ_REV1655_IMAGE_BASE;
    const uint8_t *text = NULL;
    size_t text_size = 0u;
    AzCanaryResult result = {
        AZ_IMAGE_NULL,
        AZ_COMPAT_BAD_TEXT_BASE
    };

    if (!MmIsAddressValid((void *)image) ||
        !MmIsAddressValid((void *)(image + 0x3FFu))) {
        return result;
    }

    result.image = az_locate_rev1655_text(
        image,
        (size_t)AZ_REV1655_NT_IMAGE_SIZE,
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
    (void)context;

    if (__atomic_load_n(&g_running, __ATOMIC_ACQUIRE) != 0u) {
        const AzCanaryResult result = validate_running_aurora();
        DbgPrint(
            "AuroraAZ: canary image=%s, compatibility=%s\n",
            az_image_result_name(result.image),
            az_compatibility_result_name(result.compatibility));
    }

    /*
     * Stay observable to NOVA without touching the module loader.  Detach can
     * therefore stop and join this thread while the loader lock is held: the
     * worker needs only this atomic flag and a bounded kernel delay to exit.
     */
    while (__atomic_load_n(&g_running, __ATOMIC_ACQUIRE) != 0u) {
        {
            int64_t interval = AZ_MONITOR_INTERVAL_100NS;
            (void)KeDelayExecutionThread(0u, 0u, &interval);
        }
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
            NTSTATUS wait_status;

            do {
                wait_status = NtWaitForSingleObjectEx(
                    (uint32_t)(uintptr_t)g_monitor_thread,
                    0u,
                    0u,
                    NULL);
            } while (FAILED(wait_status));

            (void)NtClose(g_monitor_thread);
            g_monitor_thread = NULL;
        }

        DbgPrint("AuroraAZ: canary detach\n");
    }

    return 1;
}
