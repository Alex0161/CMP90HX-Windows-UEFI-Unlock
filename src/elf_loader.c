#include "elf_loader.h"
#include "elf64_min.h"

#define MAX_SECTIONS 128
#define MAX_SYMBOLS 256
#define PAGE_SIZE 4096ULL
#define TRAMPOLINE_BYTES 16ULL
#define CORE_MAX_ADDRESS 0x7fffffffULL

extern uint32_t SYSVABI ioread32(const volatile void *address);
extern void SYSVABI iowrite32(uint32_t value, volatile void *address);
extern void *SYSVABI memcpy(void *destination, const void *source, size_t size);
extern void SYSVABI memcpy_toio(volatile void *destination, const void *source, size_t size);
extern void *SYSVABI memset(void *destination, int value, size_t size);

static UINT64 align_up(UINT64 value, UINT64 alignment)
{
    if (alignment < 1)
        alignment = 1;
    return (value + alignment - 1) & ~(alignment - 1);
}

static int range_ok(UINTN total, UINT64 offset, UINT64 size)
{
    return offset <= total && size <= total - offset;
}

static int string_equal(const char *left, const char *right)
{
    while (*left && *right && *left == *right) {
        left++;
        right++;
    }
    return *left == *right;
}

static UINT64 external_target(const char *name)
{
    if (string_equal(name, "ioread32"))
        return (UINT64)(uintptr_t)&ioread32;
    if (string_equal(name, "iowrite32"))
        return (UINT64)(uintptr_t)&iowrite32;
    if (string_equal(name, "memcpy"))
        return (UINT64)(uintptr_t)&memcpy;
    if (string_equal(name, "memcpy_toio"))
        return (UINT64)(uintptr_t)&memcpy_toio;
    return 0;
}

static UINT64 make_trampoline(
    UINT8 *allocation,
    UINT64 allocation_size,
    UINT64 *cursor,
    UINT64 target)
{
    UINT8 *stub;
    UINT64 location = align_up(*cursor, TRAMPOLINE_BYTES);

    if (!target || location > allocation_size ||
        TRAMPOLINE_BYTES > allocation_size - location)
        return 0;

    stub = allocation + location;
    /* mov rax, imm64; jmp rax; int3 padding */
    stub[0] = 0x48;
    stub[1] = 0xb8;
    *(UINT64 *)(void *)(stub + 2) = target;
    stub[10] = 0xff;
    stub[11] = 0xe0;
    stub[12] = 0xcc;
    stub[13] = 0xcc;
    stub[14] = 0xcc;
    stub[15] = 0xcc;
    *cursor = location + TRAMPOLINE_BYTES;
    return (UINT64)(uintptr_t)stub;
}

static const char *symbol_name(
    const UINT8 *image,
    UINTN image_size,
    const Elf64_Shdr *string_section,
    const Elf64_Sym *symbol)
{
    UINT64 remaining;
    const char *name;

    if (!range_ok(image_size, string_section->sh_offset, string_section->sh_size) ||
        symbol->st_name >= string_section->sh_size)
        return 0;
    name = (const char *)(image + string_section->sh_offset + symbol->st_name);
    remaining = string_section->sh_size - symbol->st_name;
    while (remaining--) {
        if (*name++ == '\0')
            return (const char *)(image + string_section->sh_offset + symbol->st_name);
    }
    return 0;
}

static UINT64 resolve_symbol(
    const UINT8 *image,
    UINTN image_size,
    const Elf64_Ehdr *header,
    const Elf64_Shdr *strings,
    const Elf64_Sym *symbol,
    UINT64 *section_addresses,
    UINT64 *external_stubs,
    UINTN symbol_index,
    UINT8 *allocation,
    UINT64 allocation_size,
    UINT64 *trampoline_cursor)
{
    const char *name;
    UINT64 target;

    if (symbol->st_shndx == SHN_ABS)
        return symbol->st_value;
    if (symbol->st_shndx != SHN_UNDEF) {
        if (symbol->st_shndx >= header->e_shnum ||
            !section_addresses[symbol->st_shndx])
            return 0;
        return section_addresses[symbol->st_shndx] + symbol->st_value;
    }
    if (symbol_index >= MAX_SYMBOLS)
        return 0;
    if (external_stubs[symbol_index])
        return external_stubs[symbol_index];
    name = symbol_name(image, image_size, strings, symbol);
    if (!name)
        return 0;
    target = external_target(name);
    if (!target)
        return 0;
    target = make_trampoline(allocation, allocation_size,
                             trampoline_cursor, target);
    external_stubs[symbol_index] = target;
    return target;
}

EFI_STATUS load_nvpermissive_core(
    EFI_BOOT_SERVICES *bs,
    const void *elf,
    UINTN elf_size,
    struct core_image *out)
{
    const UINT8 *image = (const UINT8 *)elf;
    const Elf64_Ehdr *header;
    const Elf64_Shdr *sections;
    const Elf64_Shdr *symtab = 0;
    const Elf64_Shdr *strings = 0;
    const Elf64_Sym *symbols;
    UINT64 section_addresses[MAX_SECTIONS];
    UINT64 external_stubs[MAX_SYMBOLS];
    UINT64 layout_size = 0;
    UINT64 trampoline_cursor;
    UINT64 allocation_size;
    EFI_PHYSICAL_ADDRESS allocation;
    EFI_STATUS status;
    UINTN pages;
    UINTN sym_count;
    UINTN i;

    if (!bs || !elf || !out || elf_size < sizeof(Elf64_Ehdr))
        return EFI_INVALID_PARAMETER;
    memset(out, 0, sizeof(*out));
    memset(section_addresses, 0, sizeof(section_addresses));
    memset(external_stubs, 0, sizeof(external_stubs));

    header = (const Elf64_Ehdr *)image;
    if (header->e_ident[0] != 0x7f || header->e_ident[1] != 'E' ||
        header->e_ident[2] != 'L' || header->e_ident[3] != 'F' ||
        header->e_ident[4] != 2 || header->e_ident[5] != 1 ||
        header->e_type != ET_REL || header->e_machine != EM_X86_64 ||
        header->e_shentsize != sizeof(Elf64_Shdr) ||
        header->e_shnum == 0 || header->e_shnum > MAX_SECTIONS ||
        !range_ok(elf_size, header->e_shoff,
                  (UINT64)header->e_shnum * sizeof(Elf64_Shdr)))
        return EFI_LOAD_ERROR;

    sections = (const Elf64_Shdr *)(image + header->e_shoff);
    for (i = 0; i < header->e_shnum; i++) {
        const Elf64_Shdr *section = &sections[i];
        if (section->sh_type == SHT_SYMTAB) {
            if (symtab)
                return EFI_UNSUPPORTED;
            symtab = section;
        }
        if (!(section->sh_flags & SHF_ALLOC) || section->sh_size == 0)
            continue;
        if (section->sh_addralign > PAGE_SIZE ||
            (section->sh_addralign &&
             (section->sh_addralign & (section->sh_addralign - 1))))
            return EFI_UNSUPPORTED;
        layout_size = align_up(layout_size, section->sh_addralign);
        section_addresses[i] = layout_size + 1; /* allocation base added later */
        if (section->sh_size > UINT64_MAX - layout_size)
            return EFI_BAD_BUFFER_SIZE;
        layout_size += section->sh_size;
    }
    if (!symtab || !symtab->sh_entsize ||
        symtab->sh_entsize != sizeof(Elf64_Sym) ||
        symtab->sh_link >= header->e_shnum ||
        !range_ok(elf_size, symtab->sh_offset, symtab->sh_size))
        return EFI_LOAD_ERROR;
    strings = &sections[symtab->sh_link];
    if (!range_ok(elf_size, strings->sh_offset, strings->sh_size))
        return EFI_LOAD_ERROR;
    sym_count = symtab->sh_size / sizeof(Elf64_Sym);
    if (!sym_count || sym_count > MAX_SYMBOLS)
        return EFI_UNSUPPORTED;

    trampoline_cursor = align_up(layout_size, TRAMPOLINE_BYTES);
    allocation_size = align_up(trampoline_cursor +
                               sym_count * TRAMPOLINE_BYTES, PAGE_SIZE);
    pages = allocation_size / PAGE_SIZE;
    allocation = CORE_MAX_ADDRESS;
    status = bs->AllocatePages(AllocateMaxAddress, EfiLoaderCode,
                               pages, &allocation);
    if (EFI_ERROR(status))
        return status;
    memset((void *)(uintptr_t)allocation, 0, allocation_size);

    for (i = 0; i < header->e_shnum; i++) {
        const Elf64_Shdr *section = &sections[i];
        UINT64 relative;
        if (!section_addresses[i])
            continue;
        relative = section_addresses[i] - 1;
        section_addresses[i] = allocation + relative;
        if (section->sh_type == SHT_NOBITS)
            continue;
        if (!range_ok(elf_size, section->sh_offset, section->sh_size)) {
            status = EFI_LOAD_ERROR;
            goto fail;
        }
        memcpy((void *)(uintptr_t)section_addresses[i],
               image + section->sh_offset, section->sh_size);
    }

    symbols = (const Elf64_Sym *)(image + symtab->sh_offset);
    for (i = 0; i < header->e_shnum; i++) {
        const Elf64_Shdr *relsec = &sections[i];
        const Elf64_Rela *relocations;
        UINTN count;
        UINTN r;

        if (relsec->sh_type != SHT_RELA)
            continue;
        if (relsec->sh_info >= header->e_shnum ||
            !section_addresses[relsec->sh_info] ||
            relsec->sh_entsize != sizeof(Elf64_Rela) ||
            !range_ok(elf_size, relsec->sh_offset, relsec->sh_size)) {
            status = EFI_LOAD_ERROR;
            goto fail;
        }
        relocations = (const Elf64_Rela *)(image + relsec->sh_offset);
        count = relsec->sh_size / sizeof(Elf64_Rela);
        for (r = 0; r < count; r++) {
            const Elf64_Rela *rela = &relocations[r];
            UINTN symbol_index = ELF64_R_SYM(rela->r_info);
            UINT32 type = ELF64_R_TYPE(rela->r_info);
            UINT64 place;
            UINT64 symbol;
            INT64 value;
            UINT64 target_size;

            target_size = sections[relsec->sh_info].sh_size;
            if (symbol_index >= sym_count || rela->r_offset >= target_size) {
                status = EFI_LOAD_ERROR;
                goto fail;
            }
            place = section_addresses[relsec->sh_info] + rela->r_offset;
            symbol = resolve_symbol(image, elf_size, header,
                                    strings, &symbols[symbol_index],
                                    section_addresses, external_stubs,
                                    symbol_index,
                                    (UINT8 *)(uintptr_t)allocation,
                                    allocation_size, &trampoline_cursor);
            if (!symbol) {
                status = EFI_UNSUPPORTED;
                goto fail;
            }
            if (type == R_X86_64_64) {
                if (rela->r_offset + 8 > target_size) {
                    status = EFI_LOAD_ERROR;
                    goto fail;
                }
                *(UINT64 *)(uintptr_t)place = symbol + rela->r_addend;
            } else if (type == R_X86_64_PC32 || type == R_X86_64_PLT32) {
                if (rela->r_offset + 4 > target_size) {
                    status = EFI_LOAD_ERROR;
                    goto fail;
                }
                value = (INT64)symbol + rela->r_addend - (INT64)place;
                if (value < INT32_MIN || value > INT32_MAX) {
                    status = EFI_UNSUPPORTED;
                    goto fail;
                }
                *(int32_t *)(uintptr_t)place = (int32_t)value;
            } else if (type == R_X86_64_32S) {
                if (rela->r_offset + 4 > target_size) {
                    status = EFI_LOAD_ERROR;
                    goto fail;
                }
                value = (INT64)symbol + rela->r_addend;
                if (value < INT32_MIN || value > INT32_MAX) {
                    status = EFI_UNSUPPORTED;
                    goto fail;
                }
                *(int32_t *)(uintptr_t)place = (int32_t)value;
            } else {
                status = EFI_UNSUPPORTED;
                goto fail;
            }
        }
    }

    for (i = 0; i < sym_count; i++) {
        const char *name = symbol_name(image, elf_size, strings, &symbols[i]);
        UINT64 address;
        if (!name || symbols[i].st_shndx == SHN_UNDEF)
            continue;
        address = resolve_symbol(image, elf_size, header, strings,
                                 &symbols[i], section_addresses,
                                 external_stubs, i,
                                 (UINT8 *)(uintptr_t)allocation,
                                 allocation_size, &trampoline_cursor);
        if (string_equal(name, "do_permissive"))
            out->do_permissive = (do_permissive_fn)(uintptr_t)address;
        else if (string_equal(name, "pcie_gen2_find_first_closed_plm"))
            out->pcie_gen2_find_first_closed_plm =
                (pcie_gen2_find_first_closed_plm_fn)(uintptr_t)address;
        else if (string_equal(name, "pcie_gen2_run_pre_reset_group"))
            out->pcie_gen2_run_pre_reset_group =
                (pcie_gen2_run_pre_reset_group_fn)(uintptr_t)address;
        else if (string_equal(name, "pcie_gen2_check_post_reset_gate"))
            out->pcie_gen2_check_post_reset_gate =
                (pcie_gen2_check_post_reset_gate_fn)(uintptr_t)address;
        else if (string_equal(name, "pcie_gen2_restore_post_reset_group"))
            out->pcie_gen2_restore_post_reset_group =
                (pcie_gen2_restore_post_reset_group_fn)(uintptr_t)address;
        else if (string_equal(name, "ga102_v67_open_plm"))
            out->ga102_v67_open_plm =
                (ga102_v67_open_plm_fn)(uintptr_t)address;
        else if (string_equal(name, "nvperm_core_version"))
            out->version = (const char *)(uintptr_t)address;
    }
    if (!out->do_permissive || !out->pcie_gen2_find_first_closed_plm ||
        !out->pcie_gen2_run_pre_reset_group ||
        !out->pcie_gen2_check_post_reset_gate ||
        !out->pcie_gen2_restore_post_reset_group ||
        !out->ga102_v67_open_plm) {
        status = EFI_NOT_FOUND;
        goto fail;
    }

    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    out->allocation = allocation;
    out->pages = pages;
    return EFI_SUCCESS;

fail:
    bs->FreePages(allocation, pages);
    memset(out, 0, sizeof(*out));
    return status;
}
