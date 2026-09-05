#ifndef AURORAAZ_NETDBG_BOOTSTRAP_TEST_XBOXKRNL_H
#define AURORAAZ_NETDBG_BOOTSTRAP_TEST_XBOXKRNL_H

#include <stdint.h>
#include <stdbool.h>

typedef void *HANDLE;
typedef int32_t NTSTATUS;

#define FAILED(status) ((NTSTATUS)(status) < (NTSTATUS)0)
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

bool MmIsAddressValid(void *address);

NTSTATUS NtClose(HANDLE handle);
NTSTATUS ExCreateThread(HANDLE *, uint32_t, uint32_t *, void *, void *, void *, uint32_t);
NTSTATUS NtResumeThread(HANDLE, uint32_t *);
NTSTATUS ExTerminateThread(uint32_t);
NTSTATUS KeDelayExecutionThread(uint32_t, uint32_t, int64_t *);

#endif
