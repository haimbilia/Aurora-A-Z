#ifndef AURORAAZ_REV1655_RUNTIME_TEST_XBOXKRNL_H
#define AURORAAZ_REV1655_RUNTIME_TEST_XBOXKRNL_H

#include <stdbool.h>
#include <stdint.h>

typedef void *HANDLE;
typedef int32_t NTSTATUS;

#define FAILED(status) ((NTSTATUS)(status) < (NTSTATUS)0)
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 0x00000001u
#define CREATE_ALWAYS 2u
#define OPEN_EXISTING 3u
#define FILE_ATTRIBUTE_NORMAL 0x00000080u

bool MmIsAddressValid(void *address);
int DbgPrint(const char *format, ...);

HANDLE CreateFileA(
    char *path,
    uint32_t desired_access,
    uint32_t share_mode,
    void *security_attributes,
    uint32_t creation_disposition,
    uint32_t flags_and_attributes,
    HANDLE template_file);

int WriteFile(
    HANDLE file,
    void *buffer,
    uint32_t bytes_to_write,
    uint32_t *bytes_written,
    void *overlapped);

int ReadFile(
    HANDLE file,
    void *buffer,
    uint32_t bytes_to_read,
    uint32_t *bytes_read,
    void *overlapped);

int CloseHandle(HANDLE handle);

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
