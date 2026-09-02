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

#endif
