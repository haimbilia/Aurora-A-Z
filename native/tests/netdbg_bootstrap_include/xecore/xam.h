#ifndef AURORAAZ_NETDBG_BOOTSTRAP_TEST_XAM_H
#define AURORAAZ_NETDBG_BOOTSTRAP_TEST_XAM_H

#include <stdint.h>

#include <xecore/xboxkrnl.h>

#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 0x00000001u
#define CREATE_ALWAYS 2u
#define FILE_ATTRIBUTE_NORMAL 0x00000080u
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

HANDLE CreateFileA(
    const char *path,
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

int CloseHandle(HANDLE handle);

#endif
