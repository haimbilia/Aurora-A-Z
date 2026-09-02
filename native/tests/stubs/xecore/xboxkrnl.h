#ifndef AURORAAZ_TEST_XECORE_XBOXKRNL_H
#define AURORAAZ_TEST_XECORE_XBOXKRNL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t NTSTATUS;

#define SUCCEEDED(status) (((status) & 0xC0000000u) == 0u)
#define FAILED(status) (!SUCCEEDED(status))

#define MEM_COMMIT 0x00001000u
#define MEM_RESERVE 0x00002000u
#define MEM_RELEASE 0x00008000u

#define PAGE_EXECUTE_READWRITE 0x00000040u

typedef enum _REGION {
    REGION_AUTO = 0,
    REGION_TITLE = 1,
    REGION_SYSTEM = 2
} REGION;

bool MmIsAddressValid(void *address);
uint32_t MmQueryAddressProtect(void *base_address);
void MmSetAddressProtect(
    void *base_address,
    uint32_t region_size,
    uint32_t protect_bits);

NTSTATUS NtAllocateVirtualMemory(
    void **base_address_ptr,
    uint32_t *region_size_ptr,
    uint32_t alloc_type,
    uint32_t protect_bits,
    REGION region);

NTSTATUS NtFreeVirtualMemory(
    void **base_address_ptr,
    uint32_t *region_size_ptr,
    uint32_t free_type,
    REGION region);

#ifdef __cplusplus
}
#endif

#endif
