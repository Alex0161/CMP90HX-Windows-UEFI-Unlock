#ifndef CORE_ABI_H
#define CORE_ABI_H

#include <stddef.h>
#include <stdint.h>
#include "uefi_min.h"

struct gpu_dev;
struct pcie_gen2_plm_target;
struct pcie_gen2_internal_state;

typedef void *(SYSVABI *gpu_host_alloc_fn)(struct gpu_dev *, size_t);
typedef void (SYSVABI *gpu_host_free_fn)(struct gpu_dev *, void *);
typedef void *(SYSVABI *gpu_dma_alloc_fn)(struct gpu_dev *, size_t, uint64_t *);
typedef void (SYSVABI *gpu_dma_free_fn)(struct gpu_dev *, void *, size_t, uint64_t);
typedef int (SYSVABI *gpu_pci_read_fn)(struct gpu_dev *, unsigned, uint32_t *);
typedef void (SYSVABI *gpu_delay_fn)(unsigned);
typedef int (SYSVABI *gpu_sbr_fn)(struct gpu_dev *);
typedef void (SYSVABI *gpu_log_fn)(struct gpu_dev *, const char *, ...);

struct gpu_dev {
    volatile void *mmio;
    uint16_t pci_device_id;
    void *fb_mmio;
    uint64_t fb_base;
    uint64_t fb_map_size;
    gpu_host_alloc_fn host_alloc;
    gpu_host_free_fn host_free;
    gpu_dma_alloc_fn dma_alloc;
    gpu_dma_free_fn dma_free;
    gpu_dma_alloc_fn dma_alloc_low;
    gpu_pci_read_fn pci_cfg_read32;
    gpu_delay_fn delay_ms;
    gpu_sbr_fn secondary_bus_reset;
    gpu_log_fn log;
    void *platform_data;
};

_Static_assert(offsetof(struct gpu_dev, fb_mmio) == 16,
               "gpu_dev ABI mismatch at fb_mmio");
_Static_assert(offsetof(struct gpu_dev, host_alloc) == 40,
               "gpu_dev ABI mismatch at host_alloc");
_Static_assert(offsetof(struct gpu_dev, platform_data) == 112,
               "gpu_dev ABI mismatch at platform_data");
_Static_assert(sizeof(struct gpu_dev) == 120,
               "gpu_dev ABI size mismatch");

#define NVPERM_MODE_COMPUTE 1
#define NVPERM_MODE_GRAPHICS 2
#define NVPERM_MODE_GR_RESET 3
#define NVPERM_MODE_GR_ACR 4
#define NVPERM_MODE_GR_INSPECT 5

typedef int (SYSVABI *do_permissive_fn)(struct gpu_dev *, int);

struct pcie_gen2_plm_target {
    uint32_t addr;
    const char *name;
};

struct pcie_gen2_internal_state {
    uint32_t xp_lctrl2;
    uint32_t remote_rate;
    uint32_t vsec_device;
};

typedef int (SYSVABI *pcie_gen2_find_first_closed_plm_fn)(
    struct gpu_dev *, const struct pcie_gen2_plm_target **);
typedef int (SYSVABI *pcie_gen2_run_pre_reset_group_fn)(struct gpu_dev *);
typedef int (SYSVABI *pcie_gen2_check_post_reset_gate_fn)(
    struct gpu_dev *, struct pcie_gen2_internal_state *);
typedef int (SYSVABI *pcie_gen2_restore_post_reset_group_fn)(
    struct gpu_dev *, struct pcie_gen2_internal_state *);
typedef int (SYSVABI *ga102_v67_open_plm_fn)(
    struct gpu_dev *, uint32_t, const char *, uint32_t *);

#endif
