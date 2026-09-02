#ifndef AURORAAZ_REV1655_RUNTIME_TEST_XBOXKRNL_H
#define AURORAAZ_REV1655_RUNTIME_TEST_XBOXKRNL_H

#include <stdbool.h>
#include <stdint.h>

typedef void *HANDLE;
typedef int32_t NTSTATUS;

#define FAILED(status) ((NTSTATUS)(status) < (NTSTATUS)0)
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

bool MmIsAddressValid(void *address);
int DbgPrint(const char *format, ...);

NTSTATUS KeDelayExecutionThread(
    uint32_t wait_mode,
    uint32_t alertable,
    int64_t *interval);

NTSTATUS NtClose(HANDLE handle);

NTSTATUS NtWaitForSingleObjectEx(
    uint32_t handle,
    uint32_t wait_mode,
    uint32_t alertable,
    int64_t *timeout);

#endif
