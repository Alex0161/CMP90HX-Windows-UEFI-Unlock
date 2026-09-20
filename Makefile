CC       ?= gcc
LD       ?= ld
OBJCOPY  ?= objcopy
PYTHON   ?= python3

CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
	-fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables \
	-fno-pie -fno-ident -fcf-protection=none -mcmodel=large \
	-maccumulate-outgoing-args -mno-red-zone -Iinclude

BUILD := build
CORE  := vendor/nvpermissive-core.o

.PHONY: all clean verify fetch test
all: $(BUILD)/NVPermissiveEFI.efi

fetch:
	./tools/fetch-core.sh

verify: $(CORE)
	$(PYTHON) tools/verify_core.py $(CORE)

test: $(BUILD)/test_elf_loader
	$(BUILD)/test_elf_loader

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/main.o: src/main.c include/uefi_min.h include/core_abi.h include/elf_loader.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/elf_loader.o: src/elf_loader.c include/uefi_min.h include/core_abi.h include/elf64_min.h include/elf_loader.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/core_blob.o: $(CORE) | $(BUILD)
	cd vendor && $(LD) -m elf_x86_64 -r -b binary nvpermissive-core.o -o ../$@
	$(OBJCOPY) --rename-section .data=.core,alloc,load,readonly,data,contents $@

$(BUILD)/NVPermissiveEFI.efi: $(BUILD)/main.o $(BUILD)/elf_loader.o $(BUILD)/core_blob.o
	$(LD) -mi386pep --subsystem 10 --entry efi_main --image-base 0x10000000 \
		--file-alignment 0x200 --section-alignment 0x1000 \
		-o $@ $^
	$(PYTHON) tools/verify_efi.py $@

$(BUILD)/test_elf_loader: tests/test_elf_loader.c src/elf_loader.c $(BUILD)/core_blob.o | $(BUILD)
	$(CC) -std=gnu11 -O2 -Wall -Wextra -Werror -fno-builtin \
		-fno-stack-protector -fcf-protection=none -mno-red-zone -Iinclude \
		tests/test_elf_loader.c src/elf_loader.c $(BUILD)/core_blob.o \
		-Wl,-z,noexecstack -o $@

clean:
	rm -f $(BUILD)/main.o $(BUILD)/elf_loader.o $(BUILD)/core_blob.o \
		$(BUILD)/NVPermissiveEFI.efi $(BUILD)/test_elf_loader
