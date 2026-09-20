#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include "uefi_min.h"
#include "core_abi.h"

struct core_image {
    EFI_PHYSICAL_ADDRESS allocation;
    UINTN pages;
    do_permissive_fn do_permissive;
    pcie_gen2_find_first_closed_plm_fn pcie_gen2_find_first_closed_plm;
    pcie_gen2_run_pre_reset_group_fn pcie_gen2_run_pre_reset_group;
    pcie_gen2_check_post_reset_gate_fn pcie_gen2_check_post_reset_gate;
    pcie_gen2_restore_post_reset_group_fn pcie_gen2_restore_post_reset_group;
    ga102_v67_open_plm_fn ga102_v67_open_plm;
    const char *version;
};

EFI_STATUS load_nvpermissive_core(
    EFI_BOOT_SERVICES *bs,
    const void *elf,
    UINTN elf_size,
    struct core_image *out);

#endif
