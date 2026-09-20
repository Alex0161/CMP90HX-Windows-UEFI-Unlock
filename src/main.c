#include "uefi_min.h"
#include "core_abi.h"
#include "elf_loader.h"

#define CMP_VENDOR_ID 0x10deU
#define CMP90HX_DEVICE_ID 0x220dU
#define MAX_CMP_GPUS 4U
#define NV_PMC_BOOT_0 0x00000000U
#define BOOT0_ARCH_MASK (0x1fU << 24)
#define BOOT0_ARCH_GA10X (0x17U << 24)
#define BOOT0_IMPL_MASK (0x0fU << 20)
#define BOOT0_IMPL_GA102 (0x02U << 20)
#define PCI_COMMAND_OFFSET 0x04U
#define PCI_COMMAND_MEMORY 0x0002U
#define PCI_COMMAND_MASTER 0x0004U
#define PCI_BAR0_OFFSET 0x10U
#define PCI_CAPABILITY_LIST_OFFSET 0x34U
#define PCI_CAP_ID_EXP 0x10U
#define PCI_BRIDGE_BUS_OFFSET 0x18U
#define PCI_BRIDGE_CONTROL_OFFSET 0x3eU
#define PCI_BRIDGE_CTL_BUS_RESET 0x0040U
#define PCI_CLASS_BRIDGE_PCI 0x0604U
#define FB_WINDOW_SIZE 0x10000000ULL
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define PAGE_SIZE 4096ULL
#define PCIE_LNKCAP_OFFSET 0x0cU
#define PCIE_LNKCTL_OFFSET 0x10U
#define PCIE_LNKSTA_OFFSET 0x12U
#define PCIE_LNKCAP2_OFFSET 0x2cU
#define PCIE_LNKCTL2_OFFSET 0x30U
#define PCIE_LINK_SPEED_MASK 0x000fU
#define PCIE_LINK_SPEED_GEN2 0x0002U
#define PCIE_LINK_WIDTH_MASK 0x03f0U
#define PCIE_LINK_WIDTH_SHIFT 4U
#define PCIE_LINK_RETRAIN 0x0020U
#define PCIE_LNKCAP2_GEN2 (1U << 1)
#define PCIE_GEN2_RETRAIN_ATTEMPTS 20U
#define PCIE_GEN2_RETRAIN_DELAY_MS 100U
#define PCIE_GEN2_PLM_TARGET_COUNT 9U
#define PCIE_GEN2_PLM_OPEN_VALUE 0xffffffffU
#define NV_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT 0x0082381cU
#define NV_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1 0x00823820U
#define NV_FUSE_FEATURE_OVERRIDE_GFX_SPEED_SELECT 0x00823830U
#define NVPERM_COMPUTE_SS0 0x88888888U
#define NVPERM_COMPUTE_SS1 0x00000008U
#define NVPERM_GRAPHICS_SPEED 0x00000004U

extern const UINT8 _binary_nvpermissive_core_o_start[];
extern const UINT8 _binary_nvpermissive_core_o_end[];

static EFI_SYSTEM_TABLE *g_system_table;
static EFI_BOOT_SERVICES *g_boot_services;

static const CHAR16 WINDOWS_BOOT_PATH[] = {
    '\\', 'E', 'F', 'I', '\\', 'M', 'i', 'c', 'r', 'o', 's', 'o', 'f', 't',
    '\\', 'B', 'o', 'o', 't', '\\', 'b', 'o', 'o', 't', 'm', 'g', 'f', 'w',
    '.', 'e', 'f', 'i', 0
};

struct pci_location {
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *rb; /* legacy fallback */
    EFI_PCI_IO_PROTOCOL *pio;             /* enumerated PCI function */
    UINT16 segment;
    UINT8 bus;
    UINT8 device;
    UINT8 function;
};

struct platform_gpu {
    UINTN index;
    struct pci_location gpu;
    struct pci_location bridge;
    UINT64 bar0;
    UINT64 bar1;
    UINT32 saved_bars[6];
    UINT16 saved_command;
};

enum requested_mode {
    REQUEST_INSPECT,
    REQUEST_WINDOWS,
    REQUEST_WINDOWS_NO_GEN2,
    REQUEST_UNLOCK,
    REQUEST_UNLOCK_GEN2,
    REQUEST_ACR_FULL,
    REQUEST_COMPUTE,
    REQUEST_GRAPHICS,
    REQUEST_GR_RESET,
    REQUEST_GR_ACR
};

static void console_write(const char *text)
{
    CHAR16 wide[256];
    UINTN used = 0;

    if (!g_system_table || !g_system_table->ConOut)
        return;
    while (*text) {
        if (*text == '\n') {
            if (used + 2 >= ARRAY_SIZE(wide)) {
                wide[used] = 0;
                g_system_table->ConOut->OutputString(g_system_table->ConOut, wide);
                used = 0;
            }
            wide[used++] = '\r';
        }
        wide[used++] = (UINT8)*text++;
        if (used + 1 >= ARRAY_SIZE(wide)) {
            wide[used] = 0;
            g_system_table->ConOut->OutputString(g_system_table->ConOut, wide);
            used = 0;
        }
    }
    if (used) {
        wide[used] = 0;
        g_system_table->ConOut->OutputString(g_system_table->ConOut, wide);
    }
}

static UINTN append_char(char *out, UINTN cap, UINTN pos, char value)
{
    if (pos + 1 < cap)
        out[pos] = value;
    return pos + 1;
}

static UINTN append_string(char *out, UINTN cap, UINTN pos, const char *text)
{
    if (!text)
        text = "(null)";
    while (*text)
        pos = append_char(out, cap, pos, *text++);
    return pos;
}

static UINTN append_number(char *out, UINTN cap, UINTN pos, UINT64 value,
                           UINT32 base, int upper, UINTN width, char pad,
                           int negative)
{
    char digits[32];
    const char *alphabet = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    UINTN count = 0;
    UINTN total;

    do {
        digits[count++] = alphabet[value % base];
        value /= base;
    } while (value && count < ARRAY_SIZE(digits));
    total = count + (negative ? 1U : 0U);
    if (negative && pad == '0')
        pos = append_char(out, cap, pos, '-');
    while (total < width) {
        pos = append_char(out, cap, pos, pad);
        total++;
    }
    if (negative && pad != '0')
        pos = append_char(out, cap, pos, '-');
    while (count)
        pos = append_char(out, cap, pos, digits[--count]);
    return pos;
}

static UINTN format_message(char *out, UINTN cap, const char *format,
                            __builtin_va_list args)
{
    UINTN pos = 0;

    while (*format) {
        char pad = ' ';
        UINTN width = 0;
        int length = 0;
        char spec;

        if (*format != '%') {
            pos = append_char(out, cap, pos, *format++);
            continue;
        }
        format++;
        if (*format == '%') {
            pos = append_char(out, cap, pos, *format++);
            continue;
        }
        while (*format == '#' || *format == '-' || *format == '+' || *format == ' ')
            format++;
        if (*format == '0') {
            pad = '0';
            format++;
        }
        while (*format >= '0' && *format <= '9')
            width = width * 10 + (UINTN)(*format++ - '0');
        if (*format == '.') {
            format++;
            while (*format >= '0' && *format <= '9')
                format++;
        }
        if (*format == 'l') {
            length = 1;
            format++;
            if (*format == 'l') {
                length = 2;
                format++;
            }
        } else if (*format == 'z') {
            length = 3;
            format++;
        }
        spec = *format ? *format++ : 0;
        if (spec == 's') {
            pos = append_string(out, cap, pos, __builtin_va_arg(args, const char *));
        } else if (spec == 'c') {
            pos = append_char(out, cap, pos,
                              (char)__builtin_va_arg(args, int));
        } else if (spec == 'p') {
            UINT64 value = (UINT64)(uintptr_t)__builtin_va_arg(args, void *);
            pos = append_string(out, cap, pos, "0x");
            pos = append_number(out, cap, pos, value, 16, 0,
                                width ? width : 1, '0', 0);
        } else if (spec == 'x' || spec == 'X' || spec == 'u') {
            UINT64 value;
            if (length == 2)
                value = __builtin_va_arg(args, unsigned long long);
            else if (length == 1)
                value = __builtin_va_arg(args, unsigned long);
            else if (length == 3)
                value = __builtin_va_arg(args, size_t);
            else
                value = __builtin_va_arg(args, unsigned int);
            pos = append_number(out, cap, pos, value,
                                spec == 'u' ? 10U : 16U,
                                spec == 'X', width, pad, 0);
        } else if (spec == 'd' || spec == 'i') {
            INT64 signed_value;
            UINT64 value;
            int negative;
            if (length == 2)
                signed_value = __builtin_va_arg(args, long long);
            else if (length == 1)
                signed_value = __builtin_va_arg(args, long);
            else if (length == 3)
                signed_value = (INT64)__builtin_va_arg(args, ptrdiff_t);
            else
                signed_value = __builtin_va_arg(args, int);
            negative = signed_value < 0;
            value = negative ? (UINT64)(-(signed_value + 1)) + 1U
                             : (UINT64)signed_value;
            pos = append_number(out, cap, pos, value, 10, 0, width, pad,
                                negative);
        } else {
            pos = append_char(out, cap, pos, '%');
            if (spec)
                pos = append_char(out, cap, pos, spec);
        }
    }
    if (cap)
        out[pos < cap ? pos : cap - 1] = 0;
    return pos;
}

static void console_printf(const char *format, ...)
{
    char buffer[1024];
    __builtin_va_list args;
    __builtin_va_start(args, format);
    format_message(buffer, sizeof(buffer), format, args);
    __builtin_va_end(args);
    console_write(buffer);
}

void SYSVABI gpu_log(struct gpu_dev *g, const char *format, ...)
{
    char buffer[1024];
    __builtin_va_list args;
    __builtin_va_start(args, format);
    format_message(buffer, sizeof(buffer), format, args);
    __builtin_va_end(args);
    if (g && g->platform_data) {
        struct platform_gpu *platform = (struct platform_gpu *)g->platform_data;
        console_printf("[GPU%u core] ", (unsigned)(platform->index + 1U));
    } else {
        console_write("[core] ");
    }
    console_write(buffer);
}

void *SYSVABI memcpy(void *destination, const void *source, size_t size)
{
    UINT8 *dst = (UINT8 *)destination;
    const UINT8 *src = (const UINT8 *)source;
    while (size--)
        *dst++ = *src++;
    return destination;
}

void *SYSVABI memset(void *destination, int value, size_t size)
{
    UINT8 *dst = (UINT8 *)destination;
    while (size--)
        *dst++ = (UINT8)value;
    return destination;
}

void SYSVABI memcpy_toio(volatile void *destination, const void *source,
                         size_t size)
{
    volatile UINT8 *dst = (volatile UINT8 *)destination;
    const UINT8 *src = (const UINT8 *)source;
    while (size--)
        *dst++ = *src++;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

uint32_t SYSVABI ioread32(const volatile void *address)
{
    UINT32 value = *(const volatile UINT32 *)address;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    return value;
}

void SYSVABI iowrite32(uint32_t value, volatile void *address)
{
    *(volatile UINT32 *)address = value;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

static EFI_STATUS pci_read(const struct pci_location *location, UINT32 offset,
                           EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH width,
                           void *value)
{
    if (location->pio && location->pio->Pci.Read)
        return location->pio->Pci.Read(location->pio, width, offset, 1, value);
    if (!location->rb)
        return EFI_NOT_FOUND;
    return location->rb->Pci.Read(location->rb, width,
        EFI_PCI_ADDRESS(location->bus, location->device,
                        location->function, offset), 1, value);
}

static EFI_STATUS pci_write(const struct pci_location *location, UINT32 offset,
                            EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH width,
                            void *value)
{
    if (location->pio && location->pio->Pci.Write)
        return location->pio->Pci.Write(location->pio, width, offset, 1, value);
    if (!location->rb)
        return EFI_NOT_FOUND;
    return location->rb->Pci.Write(location->rb, width,
        EFI_PCI_ADDRESS(location->bus, location->device,
                        location->function, offset), 1, value);
}

static UINTN char16_length(const CHAR16 *text)
{
    UINTN length = 0;
    while (text && text[length])
        length++;
    return length;
}

static UINTN device_path_node_length(const EFI_DEVICE_PATH_PROTOCOL *node)
{
    return (UINTN)node->Length[0] | ((UINTN)node->Length[1] << 8);
}

static void set_device_path_node_length(EFI_DEVICE_PATH_PROTOCOL *node,
                                        UINTN length)
{
    node->Length[0] = (UINT8)(length & 0xffU);
    node->Length[1] = (UINT8)((length >> 8) & 0xffU);
}

static EFI_DEVICE_PATH_PROTOCOL *append_file_device_path(
    EFI_HANDLE filesystem_handle, const CHAR16 *path)
{
    EFI_DEVICE_PATH_PROTOCOL *base = 0;
    EFI_DEVICE_PATH_PROTOCOL *node;
    EFI_DEVICE_PATH_PROTOCOL *result = 0;
    UINTN base_bytes = 0;
    UINTN path_bytes;
    UINTN file_node_bytes;
    UINTN total_bytes;
    UINTN guard = 0;

    if (EFI_ERROR(g_boot_services->HandleProtocol(
            filesystem_handle, (EFI_GUID *)&EFI_DEVICE_PATH_PROTOCOL_GUID,
            (void **)&base)) || !base)
        return 0;

    node = base;
    for (;;) {
        UINTN node_bytes = device_path_node_length(node);
        if (node_bytes < sizeof(*node) || node_bytes > 0xffffU ||
            base_bytes > 0x10000U - node_bytes)
            return 0;
        if (node->Type == END_DEVICE_PATH_TYPE &&
            node->SubType == END_ENTIRE_DEVICE_PATH_SUBTYPE)
            break;
        base_bytes += node_bytes;
        node = (EFI_DEVICE_PATH_PROTOCOL *)((UINT8 *)node + node_bytes);
        if (++guard > 256U)
            return 0;
    }

    path_bytes = (char16_length(path) + 1U) * sizeof(CHAR16);
    file_node_bytes = sizeof(EFI_DEVICE_PATH_PROTOCOL) + path_bytes;
    total_bytes = base_bytes + file_node_bytes + sizeof(EFI_DEVICE_PATH_PROTOCOL);
    if (file_node_bytes > 0xffffU ||
        EFI_ERROR(g_boot_services->AllocatePool(
            EfiLoaderData, total_bytes, (void **)&result)))
        return 0;

    memcpy(result, base, base_bytes);
    node = (EFI_DEVICE_PATH_PROTOCOL *)((UINT8 *)result + base_bytes);
    node->Type = MEDIA_DEVICE_PATH;
    node->SubType = MEDIA_FILEPATH_DP;
    set_device_path_node_length(node, file_node_bytes);
    memcpy((UINT8 *)node + sizeof(*node), path, path_bytes);
    node = (EFI_DEVICE_PATH_PROTOCOL *)((UINT8 *)node + file_node_bytes);
    node->Type = END_DEVICE_PATH_TYPE;
    node->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
    set_device_path_node_length(node, sizeof(*node));
    return result;
}

static int filesystem_has_windows_boot(EFI_HANDLE handle)
{
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *filesystem = 0;
    EFI_FILE_PROTOCOL *root = 0;
    EFI_FILE_PROTOCOL *file = 0;
    EFI_STATUS status;

    status = g_boot_services->HandleProtocol(
        handle, (EFI_GUID *)&EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID,
        (void **)&filesystem);
    if (EFI_ERROR(status) || !filesystem || !filesystem->OpenVolume)
        return 0;
    status = filesystem->OpenVolume(filesystem, &root);
    if (EFI_ERROR(status) || !root || !root->Open || !root->Close)
        return 0;
    status = root->Open(root, &file, (CHAR16 *)(uintptr_t)WINDOWS_BOOT_PATH,
                        EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(status) && file && file->Close)
        file->Close(file);
    root->Close(root);
    return !EFI_ERROR(status);
}

static EFI_STATUS chainload_windows(EFI_HANDLE parent_image)
{
    EFI_HANDLE *handles = 0;
    EFI_HANDLE windows_filesystem = 0;
    EFI_DEVICE_PATH_PROTOCOL *boot_path;
    EFI_HANDLE child_image = 0;
    CHAR16 *exit_data = 0;
    UINTN exit_data_size = 0;
    UINTN handle_count = 0;
    UINTN matches = 0;
    UINTN i;
    EFI_STATUS status;

    status = g_boot_services->LocateHandleBuffer(
        ByProtocol, (EFI_GUID *)&EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID,
        0, &handle_count, &handles);
    if (EFI_ERROR(status))
        return status;
    for (i = 0; i < handle_count; i++) {
        if (!filesystem_has_windows_boot(handles[i]))
            continue;
        windows_filesystem = handles[i];
        matches++;
    }
    g_boot_services->FreePool(handles);
    if (matches != 1U) {
        console_printf("Automatic chainload refused: found %u Windows boot partitions; expected one.\n",
                       (unsigned)matches);
        console_write("Boot Windows manually without resetting the machine.\n");
        return matches ? EFI_ABORTED : EFI_NOT_FOUND;
    }

    boot_path = append_file_device_path(windows_filesystem, WINDOWS_BOOT_PATH);
    if (!boot_path)
        return EFI_OUT_OF_RESOURCES;
    console_write("Chainloading \\EFI\\Microsoft\\Boot\\bootmgfw.efi...\n");
    status = g_boot_services->LoadImage(
        0, parent_image, boot_path, 0, 0, &child_image);
    g_boot_services->FreePool(boot_path);
    if (EFI_ERROR(status)) {
        console_printf("Windows LoadImage failed: 0x%llx\n",
                       (unsigned long long)status);
        return status;
    }
    if (g_boot_services->SetWatchdogTimer)
        g_boot_services->SetWatchdogTimer(0, 0, 0, 0);
    status = g_boot_services->StartImage(
        child_image, &exit_data_size, &exit_data);
    if (exit_data)
        g_boot_services->FreePool(exit_data);
    if (child_image && g_boot_services->UnloadImage)
        g_boot_services->UnloadImage(child_image);
    console_printf("Windows boot manager returned: 0x%llx\n",
                   (unsigned long long)status);
    return status;
}

static int same_pci_location(const struct pci_location *a,
                             const struct pci_location *b)
{
    return a->segment == b->segment &&
           a->bus == b->bus &&
           a->device == b->device &&
           a->function == b->function;
}

static int find_cmp90hx(struct platform_gpu *platforms, UINTN capacity)
{
    EFI_HANDLE *handles = 0;
    UINTN handle_count = 0;
    EFI_STATUS status;
    UINTN h;
    int matches = 0;

    console_write("AUTO4-v2: enumerating firmware PCI handles (no brute-force BDF scan)...\n");
    status = g_boot_services->LocateHandleBuffer(
        ByProtocol, (EFI_GUID *)&EFI_PCI_IO_PROTOCOL_GUID,
        0, &handle_count, &handles);
    if (EFI_ERROR(status)) {
        console_printf("PCI_IO LocateHandleBuffer failed: 0x%llx\n",
                       (unsigned long long)status);
        return -1;
    }
    console_printf("AUTO4-v2: firmware exposed %u PCI function handle(s).\n",
                   (unsigned)handle_count);

    for (h = 0; h < handle_count; h++) {
        EFI_PCI_IO_PROTOCOL *pio = 0;
        UINTN segment = 0, bus = 0, dev = 0, func = 0;
        UINT32 id = 0xffffffffU;
        struct pci_location location;
        UINTN i;
        int duplicate = 0;

        status = g_boot_services->HandleProtocol(
            handles[h], (EFI_GUID *)&EFI_PCI_IO_PROTOCOL_GUID,
            (void **)&pio);
        if (EFI_ERROR(status) || !pio || !pio->GetLocation || !pio->Pci.Read)
            continue;
        status = pio->GetLocation(pio, &segment, &bus, &dev, &func);
        if (EFI_ERROR(status) || segment > 0xffffU || bus > 0xffU ||
            dev > 0x1fU || func > 7U)
            continue;
        status = pio->Pci.Read(pio, EfiPciWidthUint32, 0, 1, &id);
        if (EFI_ERROR(status))
            continue;
        if ((id & 0xffffU) != CMP_VENDOR_ID || (id >> 16) != CMP90HX_DEVICE_ID)
            continue;

        memset(&location, 0, sizeof(location));
        location.pio = pio;
        location.segment = (UINT16)segment;
        location.bus = (UINT8)bus;
        location.device = (UINT8)dev;
        location.function = (UINT8)func;

        for (i = 0; i < (UINTN)matches && i < capacity; i++) {
            if (same_pci_location(&platforms[i].gpu, &location)) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate)
            continue;
        console_printf("AUTO4-v2: CMP candidate %04x:%02x:%02x.%u = 10de:220d\n",
                       (unsigned)segment, (unsigned)bus, (unsigned)dev,
                       (unsigned)func);
        if ((UINTN)matches < capacity) {
            platforms[matches].index = (UINTN)matches;
            platforms[matches].gpu = location;
        }
        matches++;
    }
    g_boot_services->FreePool(handles);
    return matches;
}

static int find_upstream_bridge(struct platform_gpu *platform)
{
    EFI_HANDLE *handles = 0;
    UINTN handle_count = 0;
    EFI_STATUS status;
    UINTN h;
    UINT32 best_span = 0xffffffffU;
    UINT32 best_secondary = 0;
    int found = 0;

    status = g_boot_services->LocateHandleBuffer(
        ByProtocol, (EFI_GUID *)&EFI_PCI_IO_PROTOCOL_GUID,
        0, &handle_count, &handles);
    if (EFI_ERROR(status))
        return -1;

    for (h = 0; h < handle_count; h++) {
        EFI_PCI_IO_PROTOCOL *pio = 0;
        UINTN segment = 0, bus = 0, dev = 0, func = 0;
        struct pci_location location;
        UINT32 id = 0xffffffffU;
        UINT32 class_revision = 0;
        UINT32 buses = 0;
        UINT32 secondary, subordinate, span;

        status = g_boot_services->HandleProtocol(
            handles[h], (EFI_GUID *)&EFI_PCI_IO_PROTOCOL_GUID,
            (void **)&pio);
        if (EFI_ERROR(status) || !pio || !pio->GetLocation || !pio->Pci.Read)
            continue;
        if (EFI_ERROR(pio->GetLocation(pio, &segment, &bus, &dev, &func)) ||
            segment != platform->gpu.segment || bus > 0xffU || dev > 0x1fU ||
            func > 7U)
            continue;
        if (EFI_ERROR(pio->Pci.Read(pio, EfiPciWidthUint32, 0, 1, &id)) ||
            (id & 0xffffU) == 0xffffU)
            continue;
        if (EFI_ERROR(pio->Pci.Read(pio, EfiPciWidthUint32, 0x08, 1,
                                    &class_revision)) ||
            ((class_revision >> 16) & 0xffffU) != PCI_CLASS_BRIDGE_PCI)
            continue;
        if (EFI_ERROR(pio->Pci.Read(pio, EfiPciWidthUint32,
                                    PCI_BRIDGE_BUS_OFFSET, 1, &buses)))
            continue;
        secondary = (buses >> 8) & 0xffU;
        subordinate = (buses >> 16) & 0xffU;
        if (secondary > platform->gpu.bus || subordinate < platform->gpu.bus)
            continue;
        span = subordinate - secondary;
        if (!found || span < best_span ||
            (span == best_span && secondary > best_secondary)) {
            memset(&location, 0, sizeof(location));
            location.pio = pio;
            location.segment = (UINT16)segment;
            location.bus = (UINT8)bus;
            location.device = (UINT8)dev;
            location.function = (UINT8)func;
            platform->bridge = location;
            best_span = span;
            best_secondary = secondary;
            found = 1;
        }
    }
    g_boot_services->FreePool(handles);
    return found ? 0 : -1;
}

static int decode_memory_bar(struct platform_gpu *platform, UINT32 offset,
                             UINT64 *address, UINT32 *consumed)
{
    UINT32 low;
    UINT32 high = 0;
    if (EFI_ERROR(pci_read(&platform->gpu, offset,
                           EfiPciWidthUint32, &low)) || (low & 1U))
        return -1;
    *consumed = 1;
    if (((low >> 1) & 3U) == 2U) {
        if (EFI_ERROR(pci_read(&platform->gpu, offset + 4,
                               EfiPciWidthUint32, &high)))
            return -1;
        *consumed = 2;
    }
    *address = ((UINT64)high << 32) | (low & ~0x0fU);
    return *address ? 0 : -1;
}

static int prepare_pci_resources(struct platform_gpu *platform)
{
    UINT32 bar0_slots;
    UINT32 bar1_slots;
    UINT32 bar1_offset;
    UINTN i;

    if (EFI_ERROR(pci_read(&platform->gpu, PCI_COMMAND_OFFSET,
                           EfiPciWidthUint16, &platform->saved_command)))
        return -1;
    for (i = 0; i < ARRAY_SIZE(platform->saved_bars); i++) {
        if (EFI_ERROR(pci_read(&platform->gpu,
                               PCI_BAR0_OFFSET + (UINT32)i * 4,
                               EfiPciWidthUint32,
                               &platform->saved_bars[i])))
            return -1;
    }
    if (decode_memory_bar(platform, PCI_BAR0_OFFSET,
                          &platform->bar0, &bar0_slots))
        return -1;
    bar1_offset = PCI_BAR0_OFFSET + bar0_slots * 4;
    if (decode_memory_bar(platform, bar1_offset,
                          &platform->bar1, &bar1_slots))
        return -1;
    (void)bar1_slots;
    platform->saved_command |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
    if (EFI_ERROR(pci_write(&platform->gpu, PCI_COMMAND_OFFSET,
                            EfiPciWidthUint16,
                            &platform->saved_command)))
        return -1;
    return 0;
}

static int restore_gpu_config(struct platform_gpu *platform)
{
    UINTN i;
    for (i = 0; i < ARRAY_SIZE(platform->saved_bars); i++) {
        UINT32 value = platform->saved_bars[i];
        if (EFI_ERROR(pci_write(&platform->gpu,
                                PCI_BAR0_OFFSET + (UINT32)i * 4,
                                EfiPciWidthUint32, &value)))
            return -1;
    }
    if (EFI_ERROR(pci_write(&platform->gpu, PCI_COMMAND_OFFSET,
                            EfiPciWidthUint16,
                            &platform->saved_command)))
        return -1;
    return 0;
}

static void *SYSVABI host_alloc(struct gpu_dev *g, size_t size)
{
    void *memory = 0;
    struct platform_gpu *platform = (struct platform_gpu *)g->platform_data;
    (void)platform;
    if (EFI_ERROR(g_boot_services->AllocatePool(EfiLoaderData, size, &memory)))
        return 0;
    memset(memory, 0, size);
    return memory;
}

static void SYSVABI host_free(struct gpu_dev *g, void *memory)
{
    (void)g;
    if (memory)
        g_boot_services->FreePool(memory);
}

static void *SYSVABI dma_alloc_below_4g(struct gpu_dev *g, size_t size,
                                        uint64_t *physical)
{
    EFI_PHYSICAL_ADDRESS address = 0xffffffffULL;
    UINTN pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    (void)g;
    if (!physical || !pages ||
        EFI_ERROR(g_boot_services->AllocatePages(
            AllocateMaxAddress, EfiLoaderData, pages, &address)))
        return 0;
    memset((void *)(uintptr_t)address, 0, pages * PAGE_SIZE);
    *physical = address;
    return (void *)(uintptr_t)address;
}

static void SYSVABI dma_free(struct gpu_dev *g, void *memory, size_t size,
                             uint64_t physical)
{
    UINTN pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    (void)g;
    (void)memory;
    if (physical && pages)
        g_boot_services->FreePages(physical, pages);
}

static int SYSVABI pci_cfg_read32(struct gpu_dev *g, unsigned offset,
                                  uint32_t *value)
{
    struct platform_gpu *platform = (struct platform_gpu *)g->platform_data;
    return EFI_ERROR(pci_read(&platform->gpu, offset,
                              EfiPciWidthUint32, value)) ? -1 : 0;
}

static void SYSVABI delay_ms(unsigned milliseconds)
{
    g_boot_services->Stall((UINTN)milliseconds * 1000U);
}

static int sbr_once(struct gpu_dev *g, UINT32 settle_ms)
{
    struct platform_gpu *platform = (struct platform_gpu *)g->platform_data;
    UINT16 bridge_control;
    UINT16 asserted;
    UINT16 restored;
    UINT32 id = 0xffffffffU;

    if (EFI_ERROR(pci_read(&platform->bridge, PCI_BRIDGE_CONTROL_OFFSET,
                           EfiPciWidthUint16, &bridge_control)))
        return -1;
    asserted = bridge_control | PCI_BRIDGE_CTL_BUS_RESET;
    restored = bridge_control & ~PCI_BRIDGE_CTL_BUS_RESET;
    if (EFI_ERROR(pci_write(&platform->bridge, PCI_BRIDGE_CONTROL_OFFSET,
                            EfiPciWidthUint16, &asserted)))
        return -1;
    g_boot_services->Stall(100000U);
    if (EFI_ERROR(pci_write(&platform->bridge, PCI_BRIDGE_CONTROL_OFFSET,
                            EfiPciWidthUint16, &restored)))
        return -1;
    g_boot_services->Stall(1000000U);
    if (restore_gpu_config(platform))
        return -1;
    if (settle_ms)
        g_boot_services->Stall((UINTN)settle_ms * 1000U);
    if (EFI_ERROR(pci_read(&platform->gpu, 0, EfiPciWidthUint32, &id)) ||
        id != ((CMP90HX_DEVICE_ID << 16) | CMP_VENDOR_ID))
        return -1;
    return 0;
}

static int SYSVABI secondary_bus_reset(struct gpu_dev *g)
{
    gpu_log(g, "SBR via %04x:%02x:%02x.%u; settling 8 seconds\n",
            ((struct platform_gpu *)g->platform_data)->bridge.segment,
            ((struct platform_gpu *)g->platform_data)->bridge.bus,
            ((struct platform_gpu *)g->platform_data)->bridge.device,
            ((struct platform_gpu *)g->platform_data)->bridge.function);
    return sbr_once(g, 8000U);
}

static int dual_sbr(struct gpu_dev *g)
{
    if (g->secondary_bus_reset(g))
        return -1;
    if (g->secondary_bus_reset(g))
        return -1;
    gpu_log(g, "dual SBR complete\n");
    return 0;
}

static int find_pcie_capability(const struct pci_location *location,
                                UINT16 *capability)
{
    UINT8 pointer = 0;
    UINTN visited = 0;

    if (!capability ||
        EFI_ERROR(pci_read(location, PCI_CAPABILITY_LIST_OFFSET,
                           EfiPciWidthUint8, &pointer)))
        return -1;
    while (pointer && visited++ < 48U) {
        UINT16 header = 0;

        pointer &= 0xfcU;
        if (pointer < 0x40U || pointer > 0xfcU ||
            EFI_ERROR(pci_read(location, pointer, EfiPciWidthUint16,
                               &header)))
            return -1;
        if ((header & 0xffU) == PCI_CAP_ID_EXP) {
            *capability = pointer;
            return 0;
        }
        pointer = (UINT8)(header >> 8);
    }
    return -1;
}

static int read_link_capabilities(struct gpu_dev *gpu,
                                  UINT16 *gpu_capability,
                                  UINT16 *bridge_capability)
{
    struct platform_gpu *platform =
        (struct platform_gpu *)gpu->platform_data;
    UINT32 gpu_lnkcap = 0;
    UINT32 gpu_lnkcap2 = 0;
    UINT32 bridge_lnkcap = 0;

    if (find_pcie_capability(&platform->gpu, gpu_capability) ||
        find_pcie_capability(&platform->bridge, bridge_capability) ||
        EFI_ERROR(pci_read(&platform->gpu,
                           *gpu_capability + PCIE_LNKCAP_OFFSET,
                           EfiPciWidthUint32, &gpu_lnkcap)) ||
        EFI_ERROR(pci_read(&platform->gpu,
                           *gpu_capability + PCIE_LNKCAP2_OFFSET,
                           EfiPciWidthUint32, &gpu_lnkcap2)) ||
        EFI_ERROR(pci_read(&platform->bridge,
                           *bridge_capability + PCIE_LNKCAP_OFFSET,
                           EfiPciWidthUint32, &bridge_lnkcap)))
        return -1;
    if ((gpu_lnkcap & PCIE_LINK_SPEED_MASK) < PCIE_LINK_SPEED_GEN2 ||
        !(gpu_lnkcap2 & PCIE_LNKCAP2_GEN2) ||
        (bridge_lnkcap & PCIE_LINK_SPEED_MASK) < PCIE_LINK_SPEED_GEN2)
        return -1;

    gpu_log(gpu, "pcie-gen2: capability gate GPU LnkCap=0x%08x "
            "LnkCap2=0x%08x RP LnkCap=0x%08x\n",
            gpu_lnkcap, gpu_lnkcap2, bridge_lnkcap);
    return 0;
}

static int prearm_bridge_gen2_target(struct gpu_dev *gpu,
                                     UINT16 bridge_capability)
{
    struct platform_gpu *platform =
        (struct platform_gpu *)gpu->platform_data;
    UINT16 before = 0;
    UINT16 target;
    UINT16 readback = 0;

    if (EFI_ERROR(pci_read(&platform->bridge,
                           bridge_capability + PCIE_LNKCTL2_OFFSET,
                           EfiPciWidthUint16, &before)))
        return -1;
    target = (before & ~PCIE_LINK_SPEED_MASK) | PCIE_LINK_SPEED_GEN2;
    if (EFI_ERROR(pci_write(&platform->bridge,
                            bridge_capability + PCIE_LNKCTL2_OFFSET,
                            EfiPciWidthUint16, &target)) ||
        EFI_ERROR(pci_read(&platform->bridge,
                           bridge_capability + PCIE_LNKCTL2_OFFSET,
                           EfiPciWidthUint16, &readback)) ||
        readback != target)
        return -1;
    gpu_log(gpu, "pcie-gen2: pre-armed RP LnkCtl2 target Gen2 "
            "before=0x%04x post=0x%04x; final SBR will resample partner rate\n",
            before, readback);
    return 0;
}

static int set_gen2_target_and_retrain(struct gpu_dev *gpu,
                                       UINT16 gpu_capability,
                                       UINT16 bridge_capability)
{
    struct platform_gpu *platform =
        (struct platform_gpu *)gpu->platform_data;
    UINT16 gpu_ctl2 = 0;
    UINT16 bridge_ctl2 = 0;
    UINT16 bridge_ctl = 0;
    UINT16 gpu_before = 0;
    UINT16 bridge_before = 0;
    UINT16 gpu_status = 0;
    UINT16 bridge_status = 0;
    UINT16 gpu_width_before;
    UINT16 bridge_width_before;
    UINTN attempt;

    if (EFI_ERROR(pci_read(&platform->gpu,
                           gpu_capability + PCIE_LNKSTA_OFFSET,
                           EfiPciWidthUint16, &gpu_before)) ||
        EFI_ERROR(pci_read(&platform->bridge,
                           bridge_capability + PCIE_LNKSTA_OFFSET,
                           EfiPciWidthUint16, &bridge_before)))
        return -1;
    gpu_width_before = (gpu_before & PCIE_LINK_WIDTH_MASK) >>
                       PCIE_LINK_WIDTH_SHIFT;
    bridge_width_before = (bridge_before & PCIE_LINK_WIDTH_MASK) >>
                          PCIE_LINK_WIDTH_SHIFT;
    gpu_log(gpu, "pcie-gen2: before GPU=0x%04x Gen%u x%u "
            "RP=0x%04x Gen%u x%u\n",
            gpu_before, gpu_before & PCIE_LINK_SPEED_MASK, gpu_width_before,
            bridge_before, bridge_before & PCIE_LINK_SPEED_MASK,
            bridge_width_before);

    if ((gpu_before & PCIE_LINK_SPEED_MASK) == PCIE_LINK_SPEED_GEN2 &&
        (bridge_before & PCIE_LINK_SPEED_MASK) == PCIE_LINK_SPEED_GEN2)
        return 0;

    if (EFI_ERROR(pci_read(&platform->gpu,
                           gpu_capability + PCIE_LNKCTL2_OFFSET,
                           EfiPciWidthUint16, &gpu_ctl2)))
        return -1;
    gpu_ctl2 = (gpu_ctl2 & ~PCIE_LINK_SPEED_MASK) | PCIE_LINK_SPEED_GEN2;
    if (EFI_ERROR(pci_write(&platform->gpu,
                            gpu_capability + PCIE_LNKCTL2_OFFSET,
                            EfiPciWidthUint16, &gpu_ctl2)) ||
        EFI_ERROR(pci_read(&platform->gpu,
                           gpu_capability + PCIE_LNKCTL2_OFFSET,
                           EfiPciWidthUint16, &gpu_status)) ||
        gpu_status != gpu_ctl2)
        return -1;

    if (EFI_ERROR(pci_read(&platform->bridge,
                           bridge_capability + PCIE_LNKCTL2_OFFSET,
                           EfiPciWidthUint16, &bridge_ctl2)))
        return -1;
    bridge_ctl2 = (bridge_ctl2 & ~PCIE_LINK_SPEED_MASK) |
                  PCIE_LINK_SPEED_GEN2;
    if (EFI_ERROR(pci_write(&platform->bridge,
                            bridge_capability + PCIE_LNKCTL2_OFFSET,
                            EfiPciWidthUint16, &bridge_ctl2)) ||
        EFI_ERROR(pci_read(&platform->bridge,
                           bridge_capability + PCIE_LNKCTL2_OFFSET,
                           EfiPciWidthUint16, &bridge_status)) ||
        bridge_status != bridge_ctl2)
        return -1;

    gpu_log(gpu, "pcie-gen2: Target=Gen2 GPU LnkCtl2=0x%04x "
            "RP LnkCtl2=0x%04x\n", gpu_ctl2, bridge_ctl2);
    if (EFI_ERROR(pci_read(&platform->bridge,
                           bridge_capability + PCIE_LNKCTL_OFFSET,
                           EfiPciWidthUint16, &bridge_ctl)))
        return -1;
    bridge_ctl |= PCIE_LINK_RETRAIN;
    if (EFI_ERROR(pci_write(&platform->bridge,
                            bridge_capability + PCIE_LNKCTL_OFFSET,
                            EfiPciWidthUint16, &bridge_ctl)))
        return -1;

    for (attempt = 0; attempt < PCIE_GEN2_RETRAIN_ATTEMPTS; attempt++) {
        g_boot_services->Stall(PCIE_GEN2_RETRAIN_DELAY_MS * 1000U);
        if (EFI_ERROR(pci_read(&platform->gpu,
                               gpu_capability + PCIE_LNKSTA_OFFSET,
                               EfiPciWidthUint16, &gpu_status)) ||
            EFI_ERROR(pci_read(&platform->bridge,                               bridge_capability + PCIE_LNKSTA_OFFSET,
                               EfiPciWidthUint16, &bridge_status)))
            continue;
        if ((gpu_status & PCIE_LINK_SPEED_MASK) == PCIE_LINK_SPEED_GEN2 &&
            (bridge_status & PCIE_LINK_SPEED_MASK) == PCIE_LINK_SPEED_GEN2 &&
            ((gpu_status & PCIE_LINK_WIDTH_MASK) >>
                PCIE_LINK_WIDTH_SHIFT) >= gpu_width_before &&
            ((bridge_status & PCIE_LINK_WIDTH_MASK) >>
                PCIE_LINK_WIDTH_SHIFT) >= bridge_width_before) {
            gpu_log(gpu, "pcie-gen2: SUCCESS GPU=0x%04x Gen2 x%u "
                    "RP=0x%04x Gen2 x%u attempt=%u\n",
                    gpu_status,
                    (gpu_status & PCIE_LINK_WIDTH_MASK) >>
                        PCIE_LINK_WIDTH_SHIFT,
                    bridge_status,
                    (bridge_status & PCIE_LINK_WIDTH_MASK) >>
                        PCIE_LINK_WIDTH_SHIFT,
                    (unsigned)attempt);
            return 0;
        }
    }

    gpu_log(gpu, "pcie-gen2: retrain failed GPU=0x%04x RP=0x%04x\n",
            gpu_status, bridge_status);
    return -1;
}

static int run_pcie_gen2(struct core_image *core, struct gpu_dev *gpu)
{
    struct pcie_gen2_internal_state internal_state;
    const struct pcie_gen2_plm_target *target = 0;
    UINT16 gpu_capability = 0;
    UINT16 bridge_capability = 0;
    UINT32 post = 0;
    UINTN opened = 0;
    int result;
    int scan;

    memset(&internal_state, 0, sizeof(internal_state));
    console_write("\n== PCIe Gen2 ==\n");
    for (;;) {
        scan = core->pcie_gen2_find_first_closed_plm(gpu, &target);
        if (scan < 0)
            return -1;
        if (scan == 0)
            break;
        if (opened++ >= PCIE_GEN2_PLM_TARGET_COUNT || !target)
            return -1;
        gpu_log(gpu, "pcie-gen2: opening %s at 0x%08x\n",
                target->name, target->addr);
        result = core->ga102_v67_open_plm(
            gpu, target->addr, target->name, &post);
        if (result || post != PCIE_GEN2_PLM_OPEN_VALUE)
            return -1;
        if (dual_sbr(gpu))
            return -1;
    }

    result = core->pcie_gen2_run_pre_reset_group(gpu);
    if (result)
        return result;
    if (read_link_capabilities(gpu, &gpu_capability, &bridge_capability))
        return -1;
    if (prearm_bridge_gen2_target(gpu, bridge_capability))
        return -1;
    if (dual_sbr(gpu))
        return -1;
    if (read_link_capabilities(gpu, &gpu_capability, &bridge_capability))
        return -1;
    result = core->pcie_gen2_check_post_reset_gate(gpu, &internal_state);
    if (result)
        return result;
    result = core->pcie_gen2_restore_post_reset_group(gpu, &internal_state);
    if (result)
        return result;
    return set_gen2_target_and_retrain(
        gpu, gpu_capability, bridge_capability);
}

static int verify_windows_unlock_state(struct gpu_dev *gpu)
{
    UINT32 compute_ss0 = ioread32((const volatile void *)(uintptr_t)
        ((UINT64)(uintptr_t)gpu->mmio +
         NV_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT));
    UINT32 compute_ss1 = ioread32((const volatile void *)(uintptr_t)
        ((UINT64)(uintptr_t)gpu->mmio +
         NV_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1));
    UINT32 graphics = ioread32((const volatile void *)(uintptr_t)
        ((UINT64)(uintptr_t)gpu->mmio +
         NV_FUSE_FEATURE_OVERRIDE_GFX_SPEED_SELECT));

    gpu_log(gpu, "windows boundary: SS0=0x%08x SS1=0x%08x "
            "GFX=0x%08x\n", compute_ss0, compute_ss1, graphics);
    if (compute_ss0 != NVPERM_COMPUTE_SS0 ||
        compute_ss1 != NVPERM_COMPUTE_SS1 ||
        graphics != NVPERM_GRAPHICS_SPEED) {
        gpu_log(gpu, "windows boundary verification FAILED\n");
        return -1;
    }
    gpu_log(gpu, "windows boundary verification PASS\n");
    return 0;
}

static int has_word(const CHAR16 *options, UINTN bytes, const char *word)
{
    UINTN chars = bytes / sizeof(CHAR16);
    UINTN i;
    for (i = 0; i < chars; i++) {
        UINTN j = 0;
        if (i && options[i - 1] > ' ')
            continue;
        while (word[j] && i + j < chars &&
               options[i + j] == (CHAR16)(UINT8)word[j])
            j++;
        if (!word[j] && (i + j == chars || options[i + j] <= ' '))
            return 1;
    }
    return 0;
}

static __attribute__((unused)) enum requested_mode parse_mode(EFI_HANDLE image_handle)
{
    EFI_LOADED_IMAGE_PROTOCOL *loaded = 0;
    /*
     * AUTO4 build: when launched directly as removable-media BOOTX64.EFI,
     * UEFI supplies no command line. Default to the Windows-safe no-Gen2
     * path. An explicit "inspect" load option keeps a read-only escape
     * hatch when this image is launched from a UEFI shell.
     */
    if (EFI_ERROR(g_boot_services->HandleProtocol(
            image_handle, (EFI_GUID *)&EFI_LOADED_IMAGE_PROTOCOL_GUID,
            (void **)&loaded)) || !loaded || !loaded->LoadOptions)
        return REQUEST_WINDOWS_NO_GEN2;
    if (has_word((const CHAR16 *)loaded->LoadOptions,
                 loaded->LoadOptionsSize, "inspect"))
        return REQUEST_INSPECT;
    if (has_word((const CHAR16 *)loaded->LoadOptions,
                 loaded->LoadOptionsSize, "windows-nogen2"))
        return REQUEST_WINDOWS_NO_GEN2;
    if (has_word((const CHAR16 *)loaded->LoadOptions,
                 loaded->LoadOptionsSize, "unlock-gen2"))
        return REQUEST_UNLOCK_GEN2;
    if (has_word((const CHAR16 *)loaded->LoadOptions,
                 loaded->LoadOptionsSize, "acr-full"))
        return REQUEST_ACR_FULL;
    if (has_word((const CHAR16 *)loaded->LoadOptions,
                 loaded->LoadOptionsSize, "windows") ||
        has_word((const CHAR16 *)loaded->LoadOptions,
                 loaded->LoadOptionsSize, "full"))
        return REQUEST_WINDOWS;
    if (has_word((const CHAR16 *)loaded->LoadOptions,
                 loaded->LoadOptionsSize, "unlock"))
        return REQUEST_UNLOCK;
    if (has_word((const CHAR16 *)loaded->LoadOptions,
                 loaded->LoadOptionsSize, "compute"))
        return REQUEST_COMPUTE;
    if (has_word((const CHAR16 *)loaded->LoadOptions,
                 loaded->LoadOptionsSize, "graphics"))
        return REQUEST_GRAPHICS;
    if (has_word((const CHAR16 *)loaded->LoadOptions,
                 loaded->LoadOptionsSize, "gr-reset"))
        return REQUEST_GR_RESET;
    if (has_word((const CHAR16 *)loaded->LoadOptions,
                 loaded->LoadOptionsSize, "gr-acr"))
        return REQUEST_GR_ACR;
    return REQUEST_INSPECT;
}

static int run_one(struct core_image *core, struct gpu_dev *gpu, int mode,
                   const char *name, int reset_after)
{
    int result;

    console_printf("\n== %s ==\n", name);
    result = core->do_permissive(gpu, mode);
    if (result) {
        console_printf("%s failed: %d\n", name, result);
        return result;
    }

    if (reset_after && dual_sbr(gpu)) {
        console_printf("%s completed, but required dual SBR failed\n", name);
        return -1;
    }

    console_printf("%s complete\n", name);
    return 0;
}

static int run_requested(enum requested_mode request, struct core_image *core,
                         struct gpu_dev *gpu)
{
    if (request == REQUEST_INSPECT)
        return run_one(core, gpu, NVPERM_MODE_GR_INSPECT, "read-only inspect", 0);
    if (request == REQUEST_COMPUTE)
        return run_one(core, gpu, NVPERM_MODE_COMPUTE, "compute", 1);
    if (request == REQUEST_GRAPHICS)
        return run_one(core, gpu, NVPERM_MODE_GRAPHICS, "graphics", 1);
    if (request == REQUEST_GR_RESET)
        return run_one(core, gpu, NVPERM_MODE_GR_RESET, "gr-reset", 1);
    if (request == REQUEST_GR_ACR)
        return run_one(core, gpu, NVPERM_MODE_GR_ACR, "gr-acr", 0);

    if (request == REQUEST_ACR_FULL) {
        console_write("\nRESEARCH MODE: FECS/GPCCS ACR initialization may cause Windows Code 43.\n");
        console_write("Establishing a clean initial SEC2 session...\n");
        if (dual_sbr(gpu))
            return -1;
        if (run_one(core, gpu, NVPERM_MODE_COMPUTE, "compute", 1))
            return -1;
        if (run_one(core, gpu, NVPERM_MODE_GRAPHICS, "graphics", 1))
            return -1;
        if (run_one(core, gpu, NVPERM_MODE_GR_RESET, "gr-reset", 1))
            return -1;
        return run_one(core, gpu, NVPERM_MODE_GR_ACR, "gr-acr", 0);
    }

    console_write("\nEstablishing a clean initial SEC2 session...\n");
    if (dual_sbr(gpu))
        return -1;
    if (run_one(core, gpu, NVPERM_MODE_COMPUTE, "compute", 1))
        return -1;
    if (run_one(core, gpu, NVPERM_MODE_GRAPHICS, "graphics", 1))
        return -1;
    if ((request == REQUEST_WINDOWS || request == REQUEST_UNLOCK_GEN2) &&
        run_pcie_gen2(core, gpu))
        return -1;
    return verify_windows_unlock_state(gpu);
}

static __attribute__((unused)) void windows_handoff_stop(const char *reason, EFI_STATUS status)
{
    console_write("\n============================================================\n");
    console_write("WINDOWS HANDOFF STOP - v2W\n");
    if (reason)
        console_printf("%s\n", reason);
    if (status)
        console_printf("EFI status: 0x%llx\n", (unsigned long long)status);
    console_write("EFI will NOT return to firmware Boot Manager.\n");
    console_write("Power the machine fully off before retrying.\n");
    console_write("============================================================\n");
    if (g_boot_services && g_boot_services->SetWatchdogTimer)
        g_boot_services->SetWatchdogTimer(0, 0, 0, 0);
    for (;;) {
        if (g_boot_services && g_boot_services->Stall)
            g_boot_services->Stall(1000000U);
    }
}


typedef struct {
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY_MANUAL;

struct manual_text_input_protocol;
typedef EFI_STATUS (EFIAPI *MANUAL_READ_KEY)(
    struct manual_text_input_protocol *, EFI_INPUT_KEY_MANUAL *);

typedef struct manual_text_input_protocol {
    void *Reset;
    MANUAL_READ_KEY ReadKeyStroke;
    void *WaitForKey;
} MANUAL_TEXT_INPUT_PROTOCOL;

static CHAR16 manual_wait_key(void)
{
    MANUAL_TEXT_INPUT_PROTOCOL *input;
    EFI_INPUT_KEY_MANUAL key;
    EFI_STATUS status;

    if (!g_system_table || !g_system_table->ConIn)
        return 0;
    input = (MANUAL_TEXT_INPUT_PROTOCOL *)g_system_table->ConIn;
    if (!input->ReadKeyStroke)
        return 0;

    for (;;) {
        key.ScanCode = 0;
        key.UnicodeChar = 0;
        status = input->ReadKeyStroke(input, &key);
        if (!EFI_ERROR(status) && key.UnicodeChar)
            return key.UnicodeChar;
        if (g_boot_services && g_boot_services->Stall)
            g_boot_services->Stall(10000U);
    }
}

static void manual_print_state(const int state[4])
{
    UINTN i;
    console_write("\nCurrent manual state: ");
    for (i = 0; i < 4U; i++) {
        const char *name = state[i] > 0 ? "PASS" :
                           state[i] < 0 ? "FAIL" : "----";
        console_printf("GPU%u=%s%s", (unsigned)(i + 1U), name,
                       i == 3U ? "\n" : "  ");
    }
}

static int manual_check_all(struct gpu_dev gpus[4], int state[4])
{
    UINTN i;
    int all_ok = 1;

    console_write("\n========== MANUAL CURRENT-STATE CHECK ==========\n");
    for (i = 0; i < 4U; i++) {
        int rc = verify_windows_unlock_state(&gpus[i]);
        state[i] = rc ? -1 : 1;
        console_printf("CHECK GPU%u: %s\n", (unsigned)(i + 1U),
                       rc ? "FAIL" : "PASS");
        if (rc)
            all_ok = 0;
    }
    console_printf("MANUAL CHECK RESULT: %s\n",
                   all_ok ? "4/4 PASS" : "NOT ALL PASS");
    return all_ok ? 0 : -1;
}

static void manual_halt(void)
{
    console_write("\nMANUAL HALT. Power off when ready.\n");
    if (g_boot_services && g_boot_services->SetWatchdogTimer)
        g_boot_services->SetWatchdogTimer(0, 0, 0, 0);
    for (;;) {
        if (g_boot_services && g_boot_services->Stall)
            g_boot_services->Stall(1000000U);
    }
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
    struct platform_gpu platforms[MAX_CMP_GPUS];
    struct gpu_dev gpus[MAX_CMP_GPUS];
    struct core_image core;
    EFI_STATUS core_status;
    int matches;
    int state[4] = {0, 0, 0, 0};
    UINTN i;

    g_system_table = system_table;
    g_boot_services = system_table ? system_table->BootServices : 0;
    if (!g_boot_services)
        return EFI_INVALID_PARAMETER;

    memset(platforms, 0, sizeof(platforms));
    memset(gpus, 0, sizeof(gpus));
    memset(&core, 0, sizeof(core));

    console_write("CMP90HX MANUAL-4 ALL-V2 / UEFI\n");
    console_write("Manual one-GPU-at-a-time mode. Windows NEVER starts automatically.\n");
    console_write("GPU1-GPU4: identical original v2 core and identical dual-SBR path.\n\n");

    matches = find_cmp90hx(platforms, ARRAY_SIZE(platforms));
    if (matches != 4) {
        console_printf("MANUAL-4 requires exactly 4 CMP 90HX devices; found %d.\n", matches);
        manual_halt();
    }

    console_write("Found all 4 CMP 90HX devices. Preflight only; no unlock writes yet.\n");

    for (i = 0; i < 4U; i++) {
        UINT32 boot0;
        UINTN j;

        if (find_upstream_bridge(&platforms[i])) {
            console_printf("GPU%u: upstream bridge not found.\n", (unsigned)(i + 1U));
            manual_halt();
        }
        for (j = 0; j < i; j++) {
            if (same_pci_location(&platforms[i].bridge, &platforms[j].bridge)) {
                console_printf("GPU%u/GPU%u share one bridge; unsafe for manual SBR.\n",
                               (unsigned)(j + 1U), (unsigned)(i + 1U));
                manual_halt();
            }
        }
        if (prepare_pci_resources(&platforms[i])) {
            console_printf("GPU%u: PCI/BAR preparation failed.\n", (unsigned)(i + 1U));
            manual_halt();
        }

        boot0 = ioread32((const volatile void *)(uintptr_t)
                         (platforms[i].bar0 + NV_PMC_BOOT_0));
        console_printf("GPU%u = %04x:%02x:%02x.%u  bridge=%04x:%02x:%02x.%u  BOOT0=0x%08x\n",
                       (unsigned)(i + 1U),
                       platforms[i].gpu.segment, platforms[i].gpu.bus,
                       platforms[i].gpu.device, platforms[i].gpu.function,
                       platforms[i].bridge.segment, platforms[i].bridge.bus,
                       platforms[i].bridge.device, platforms[i].bridge.function,
                       boot0);
        if ((boot0 & BOOT0_ARCH_MASK) != BOOT0_ARCH_GA10X ||
            (boot0 & BOOT0_IMPL_MASK) != BOOT0_IMPL_GA102) {
            console_printf("GPU%u is not GA102; stopping.\n", (unsigned)(i + 1U));
            manual_halt();
        }
    }

    core_status = load_nvpermissive_core(
        g_boot_services,
        _binary_nvpermissive_core_o_start,
        (UINTN)(_binary_nvpermissive_core_o_end -
                _binary_nvpermissive_core_o_start),
        &core);
    if (EFI_ERROR(core_status)) {
        console_printf("Original v2 core load failed: 0x%llx\n",
                       (unsigned long long)core_status);
        manual_halt();
    }

    for (i = 0; i < 4U; i++) {
        struct gpu_dev *gpu = &gpus[i];
        struct platform_gpu *platform = &platforms[i];

        gpu->mmio = (volatile void *)(uintptr_t)platform->bar0;
        gpu->pci_device_id = CMP90HX_DEVICE_ID;
        gpu->fb_mmio = (void *)(uintptr_t)platform->bar1;
        gpu->fb_base = 0;
        gpu->fb_map_size = FB_WINDOW_SIZE;
        gpu->host_alloc = host_alloc;
        gpu->host_free = host_free;
        gpu->dma_alloc = dma_alloc_below_4g;
        gpu->dma_free = dma_free;
        gpu->dma_alloc_low = dma_alloc_below_4g;
        gpu->pci_cfg_read32 = pci_cfg_read32;
        gpu->delay_ms = delay_ms;
        gpu->secondary_bus_reset = secondary_bus_reset;
        gpu->log = gpu_log;
        gpu->platform_data = platform;
    }

    if (g_boot_services->SetWatchdogTimer)
        g_boot_services->SetWatchdogTimer(0, 0, 0, 0);

    for (;;) {
        CHAR16 key;
        UINTN target;
        int result;

        manual_print_state(state);
        console_write("\nMANUAL MENU (no automatic Windows boot):\n");
        console_write("  1  Unlock GPU1 only (original v2)\n");
        console_write("  2  Unlock GPU2 only (original v2)\n");
        console_write("  3  Unlock GPU3 only (original v2)\n");
        console_write("  4  Unlock GPU4 only (original v2, same as GPU1-3)\n");
        console_write("  C  Check actual SS0/SS1/GFX state of all four GPUs\n");
        console_write("  W  Manually start Windows (allowed only after actual 4/4 PASS)\n");
        console_write("  H  Halt here\n");
        console_write("Press key: ");
        key = manual_wait_key();
        if (key >= 'a' && key <= 'z')
            key = (CHAR16)(key - ('a' - 'A'));
        console_printf("%c\n", (char)key);

        if (key >= '1' && key <= '4') {
            target = (UINTN)(key - '1');
            console_printf("\n========== MANUAL GPU%u: %04x:%02x:%02x.%u ==========\n",
                           (unsigned)(target + 1U),
                           platforms[target].gpu.segment,
                           platforms[target].gpu.bus,
                           platforms[target].gpu.device,
                           platforms[target].gpu.function);
            console_write("GPU manual profile: original v2 core; identical dual-SBR path for all GPUs.\n");

            result = run_requested(REQUEST_WINDOWS_NO_GEN2,
                                   &core, &gpus[target]);
            state[target] = result ? -1 : 1;
            console_printf("\nMANUAL GPU%u RESULT: %s (%d)\n",
                           (unsigned)(target + 1U),
                           result ? "FAIL" : "PASS", result);
            console_write("Returning to manual menu WITHOUT reboot.\n");
            continue;
        }

        if (key == 'C') {
            manual_check_all(gpus, state);
            continue;
        }

        if (key == 'W') {
            EFI_STATUS boot_status;
            if (manual_check_all(gpus, state)) {
                console_write("\nWINDOWS NOT STARTED: actual state is not 4/4 PASS.\n");
                console_write("Run the failed GPU number again, then press C.\n");
                continue;
            }
            console_write("\nMANUAL 4/4 PASS confirmed.\n");
            console_write("Starting Windows Boot Manager by explicit W command. No reset/POST.\n");
            boot_status = chainload_windows(image_handle);
            console_printf("Windows Boot Manager returned: 0x%llx\n",
                           (unsigned long long)boot_status);
            console_write("Returned to manual menu; no automatic fallback will occur.\n");
            continue;
        }

        if (key == 'H')
            manual_halt();

        console_write("Unknown key. Use 1,2,3,4,C,W,H.\n");
    }
}
