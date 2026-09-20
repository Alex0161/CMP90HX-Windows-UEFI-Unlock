#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

#include "elf_loader.h"

#define PAGE_SIZE 4096ULL

extern const UINT8 _binary_nvpermissive_core_o_start[];
extern const UINT8 _binary_nvpermissive_core_o_end[];

static EFI_STATUS EFIAPI test_allocate_pages(
    EFI_ALLOCATE_TYPE type,
    EFI_MEMORY_TYPE memory_type,
    UINTN pages,
    EFI_PHYSICAL_ADDRESS *address)
{
    void *memory;
    (void)type;
    (void)memory_type;
    memory = mmap(0, pages * PAGE_SIZE,
                  PROT_READ | PROT_WRITE | PROT_EXEC,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (memory == MAP_FAILED)
        return EFI_OUT_OF_RESOURCES;
    *address = (EFI_PHYSICAL_ADDRESS)(uintptr_t)memory;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI test_free_pages(EFI_PHYSICAL_ADDRESS address,
                                          UINTN pages)
{
    return munmap((void *)(uintptr_t)address, pages * PAGE_SIZE) == 0
        ? EFI_SUCCESS : EFI_DEVICE_ERROR;
}

uint32_t SYSVABI ioread32(const volatile void *address)
{
    return *(const volatile uint32_t *)address;
}

void SYSVABI iowrite32(uint32_t value, volatile void *address)
{
    *(volatile uint32_t *)address = value;
}

void *SYSVABI memcpy(void *destination, const void *source, size_t size)
{
    uint8_t *dst = (uint8_t *)destination;
    const uint8_t *src = (const uint8_t *)source;
    while (size--)
        *dst++ = *src++;
    return destination;
}

void *SYSVABI memset(void *destination, int value, size_t size)
{
    uint8_t *dst = (uint8_t *)destination;
    while (size--)
        *dst++ = (uint8_t)value;
    return destination;
}

void SYSVABI memcpy_toio(volatile void *destination, const void *source,
                         size_t size)
{
    volatile uint8_t *dst = (volatile uint8_t *)destination;
    const uint8_t *src = (const uint8_t *)source;
    while (size--)
        *dst++ = *src++;
}

int main(void)
{
    EFI_BOOT_SERVICES services;
    struct core_image core;
    EFI_STATUS status;

    memset(&services, 0, sizeof(services));
    memset(&core, 0, sizeof(core));
    services.AllocatePages = test_allocate_pages;
    services.FreePages = test_free_pages;
    status = load_nvpermissive_core(
        &services,
        _binary_nvpermissive_core_o_start,
        (UINTN)(_binary_nvpermissive_core_o_end -
                _binary_nvpermissive_core_o_start),
        &core);
    if (EFI_ERROR(status)) {
        fprintf(stderr, "loader returned 0x%llx\n",
                (unsigned long long)status);
        return 1;
    }
    if (!core.do_permissive || !core.pcie_gen2_find_first_closed_plm ||
        !core.pcie_gen2_run_pre_reset_group ||
        !core.pcie_gen2_check_post_reset_gate ||
        !core.pcie_gen2_restore_post_reset_group ||
        !core.ga102_v67_open_plm || !core.version ||
        core.allocation >= 0x80000000ULL) {
        fprintf(stderr, "loaded core failed invariants\n");
        return 1;
    }
    printf("loader OK: allocation=0x%llx pages=%llu version=%s\n",
           (unsigned long long)core.allocation,
           (unsigned long long)core.pages,
           core.version);
    services.FreePages(core.allocation, core.pages);
    return 0;
}
