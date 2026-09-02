#ifndef AURORAAZ_REV1655_THREAD_PRIVATE_H
#define AURORAAZ_REV1655_THREAD_PRIVATE_H

#include <stddef.h>
#include <stdint.h>

#include <xecore/xboxkrnl.h>

#define AZ_REV1655_THREAD_WRAPPER_ADDRESS 0x82361AA8u
#define AZ_REV1655_XAPI_THREAD_STARTUP_ADDRESS 0x82804650u
#define AZ_STATUS_REVISION_MISMATCH 0xC0000059u
#define AZ_STATUS_THREAD_NOT_CREATED 0xC0000017u

typedef HANDLE (*AzRev1655ThreadWrapper)(
    void *start_address,
    void *start_context);

typedef enum AzRev1655ThreadCreateResult {
    AZ_REV1655_THREAD_CREATE_OK = 0,
    AZ_REV1655_THREAD_CREATE_REVISION_MISMATCH,
    AZ_REV1655_THREAD_CREATE_FAILED
} AzRev1655ThreadCreateResult;

#if defined(AURORAAZ_REV1655_THREAD_TEST)
extern const uint32_t g_auroraaz_test_xapi_thread_startup[8];
extern const uint32_t g_auroraaz_test_thread_wrapper_probe[25];
HANDLE g_auroraaz_test_thread_wrapper(
    void *start_address,
    void *start_context);
#define AZ_REV1655_XAPI_THREAD_STARTUP_PROBE_POINTER \
    ((const volatile uint32_t *)(uintptr_t) \
        &g_auroraaz_test_xapi_thread_startup[0])
#define AZ_REV1655_THREAD_WRAPPER_PROBE_POINTER \
    ((const volatile uint32_t *)(uintptr_t) \
        &g_auroraaz_test_thread_wrapper_probe[0])
#define AZ_REV1655_THREAD_WRAPPER_CALL_POINTER \
    ((AzRev1655ThreadWrapper)(uintptr_t) \
        &g_auroraaz_test_thread_wrapper)
#else
#define AZ_REV1655_XAPI_THREAD_STARTUP_PROBE_POINTER \
    ((const volatile uint32_t *)(uintptr_t) \
        AZ_REV1655_XAPI_THREAD_STARTUP_ADDRESS)
#define AZ_REV1655_THREAD_WRAPPER_PROBE_POINTER \
    ((const volatile uint32_t *)(uintptr_t) \
        AZ_REV1655_THREAD_WRAPPER_ADDRESS)
#define AZ_REV1655_THREAD_WRAPPER_CALL_POINTER \
    ((AzRev1655ThreadWrapper)(uintptr_t) \
        AZ_REV1655_THREAD_WRAPPER_ADDRESS)
#endif

/* Exact first 32 bytes of Rev1655's XapiThreadStartup at 0x82804650. */
static const uint32_t k_az_rev1655_xapi_thread_startup_probe[8] = {
    0x7D8802A6u,
    0x48163679u,
    0x3BE1FF80u,
    0x9421FF80u,
    0x7C7E1B78u,
    0x7C9D2378u,
    0x39600000u,
    0x917F0050u
};

/* Exact complete Rev1655 wrapper at 0x82361AA8 (25 words / 100 bytes). */
static const uint32_t k_az_rev1655_thread_wrapper_probe[25] = {
    0x7D8802A6u,
    0x9181FFF8u,
    0x9421FFA0u,
    0x3D608280u,
    0x7C882378u,
    0x7C671B78u,
    0x39200002u,
    0x38CB4650u,
    0x38A10054u,
    0x38800000u,
    0x38610050u,
    0x488049B9u,
    0x38800003u,
    0x80610050u,
    0x484A0879u,
    0x3880000Fu,
    0x80610050u,
    0x484A0645u,
    0x80610050u,
    0x484A256Du,
    0x80610050u,
    0x38210060u,
    0x8181FFF8u,
    0x7D8803A6u,
    0x4E800020u
};

static uint8_t az_rev1655_code_matches(
    const volatile uint32_t *actual,
    const uint32_t *expected,
    size_t word_count)
{
    size_t index;
    const size_t byte_count = word_count * sizeof(uint32_t);

    if (actual == NULL || expected == NULL || word_count == 0u ||
        !MmIsAddressValid((void *)(uintptr_t)actual) ||
        !MmIsAddressValid((void *)(uintptr_t)(
            (const uint8_t *)(uintptr_t)actual + byte_count - 1u))) {
        return 0u;
    }

    for (index = 0u; index < word_count; ++index) {
        if (actual[index] != expected[index]) {
            return 0u;
        }
    }

    return 1u;
}

static uint8_t az_rev1655_thread_wrapper_is_valid(void)
{
    return (
        az_rev1655_code_matches(
            AZ_REV1655_XAPI_THREAD_STARTUP_PROBE_POINTER,
            k_az_rev1655_xapi_thread_startup_probe,
            sizeof(k_az_rev1655_xapi_thread_startup_probe) /
                sizeof(k_az_rev1655_xapi_thread_startup_probe[0])) != 0u &&
        az_rev1655_code_matches(
            AZ_REV1655_THREAD_WRAPPER_PROBE_POINTER,
            k_az_rev1655_thread_wrapper_probe,
            sizeof(k_az_rev1655_thread_wrapper_probe) /
                sizeof(k_az_rev1655_thread_wrapper_probe[0])) != 0u) ?
        1u : 0u;
}

static AzRev1655ThreadCreateResult az_rev1655_thread_create(
    void *start_address,
    void *start_context,
    HANDLE *out_thread)
{
    HANDLE thread;

    if (start_address == NULL || out_thread == NULL) {
        return AZ_REV1655_THREAD_CREATE_FAILED;
    }
    *out_thread = NULL;

    if (az_rev1655_thread_wrapper_is_valid() == 0u) {
        return AZ_REV1655_THREAD_CREATE_REVISION_MISMATCH;
    }

    thread = AZ_REV1655_THREAD_WRAPPER_CALL_POINTER(
        start_address,
        start_context);
    if (thread == NULL || thread == INVALID_HANDLE_VALUE) {
        return AZ_REV1655_THREAD_CREATE_FAILED;
    }

    *out_thread = thread;
    return AZ_REV1655_THREAD_CREATE_OK;
}

#endif
