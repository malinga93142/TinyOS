CC := gcc
LD := ld

CONFIG_NMI  ?= 0
MCMODEL     ?= large
SMP         ?= 1
MEM         ?= 1G
QEMU_DEVICE ?= ich9-ahci
GRUB_TIMEOUT ?= 0
GDB_SCRIPT  ?= debug.gdb
# override: make run-tcg DBG_ITEMS=int,mmu
DBG_ITEMS   ?= int,cpu_reset,guest_errors,unimp
# qemu64 catches accidental host-feature use
DBG_CPU     ?= max
# SMP=1 removes AP/SIPI noise from the logs
DBG_SMP     ?= $(SMP)

SRC_DIR   := src
USER_DIR  := user
BUILD_DIR := build
TARGET    := boot.elf
USER_PROGS := hello

KERNEL_CFLAGS := -g -m64 -ffreestanding -nostdlib -fno-pie -mcmodel=$(MCMODEL) \
                 -mno-red-zone -mno-avx2 -mno-sse -mno-avx -fno-stack-protector \
                 -Wall -Wextra -DCONFIG_LAPIC_NMI=$(CONFIG_NMI)  \
                 -Iinclude -Isubsystems
USER_CFLAGS   := -m64 -O2 -ffreestanding -nostdlib -static -fno-pie -no-pie \
                 -fno-stack-protector -mgeneral-regs-only -Wall -Wextra
ASFLAGS       := -g -m64
LDFLAGS       := -T linker.ld
DEPFLAGS      := -MMD -MP

C_SRCS := $(wildcard $(SRC_DIR)/*.c)
S_SRCS := $(wildcard $(SRC_DIR)/*.S)
OBJS := $(C_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) \
        $(S_SRCS:$(SRC_DIR)/%.S=$(BUILD_DIR)/%.o)
USER_BINS := $(USER_PROGS:%=$(BUILD_DIR)/user/%.bin)
USER_OBJS := $(BUILD_DIR)/user/start.o $(USER_PROGS:%=$(BUILD_DIR)/user/%.o)
DEPS := $(OBJS:.o=.d) $(USER_OBJS:.o=.d)

QEMU := qemu-system-x86_64
QEMU_COMMON := -machine pc -m $(MEM) \
               -drive file=kernel.iso,format=raw,media=cdrom,if=ide,index=2 \
               -drive file=disk.img,format=raw,if=ide,index=0 \
               -drive file=ahci_test.img,format=raw,if=none,id=hd0 \
               -device $(QEMU_DEVICE),id=ahci \
               -device ide-hd,drive=hd0,bus=ahci.0 \
               -serial mon:stdio -no-reboot -no-shutdown -d int \
               -smp $(SMP) -enable-kvm
QEMU_FLAGS     := $(QEMU_COMMON) -cpu host
QEMU_EXT_FLAGS := $(QEMU_COMMON) -cpu host,+xsave,+avx,+invtsc
QEMU_GDB_FLAGS := $(QEMU_COMMON) -cpu host -s -S
QEMU_TCG_FLAGS := -m 1G -cdrom kernel.iso -serial mon:stdio -no-reboot \
                  -no-shutdown -display none -smp $(DBG_SMP) -accel tcg \
                  -cpu $(DBG_CPU) -d $(DBG_ITEMS) -D qemu.log \
                  -debugcon file:debugcon.log \
                  -device isa-debug-exit,iobase=0xf4,iosize=0x04

.PHONY: all iso run run-ext run-gdb run-tcg run-tcg-gdb gdb clean
.PRECIOUS: $(BUILD_DIR)/user/%.elf

all: $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	@echo "[CC] $@"
	@$(CC) $(KERNEL_CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S
	@mkdir -p $(@D)
	@echo "[AS] $@"
	@$(CC) $(ASFLAGS) $(DEPFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	@echo "[LD] $@"
	@$(LD) $(LDFLAGS) $(OBJS) -o $@

$(BUILD_DIR)/user/%.o: $(USER_DIR)/%.c
	@mkdir -p $(@D)
	@echo "[CC] $@"
	@$(CC) $(USER_CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/user/%.o: $(USER_DIR)/%.S
	@mkdir -p $(@D)
	@echo "[AS] $@"
	@$(CC) -m64 $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/user/%.elf: $(BUILD_DIR)/user/start.o $(BUILD_DIR)/user/%.o
	@echo "[LD] $@"
	@$(LD) -T $(USER_DIR)/user.ld $^ -o $@

$(BUILD_DIR)/user/%.bin: $(BUILD_DIR)/user/%.elf
	@objcopy -O binary $< $@

$(BUILD_DIR)/blob.o: $(USER_BINS)

kernel.iso: $(TARGET)
	@echo "[ISO] $@"
	@mkdir -p isodir/boot/grub
	@cp $(TARGET) isodir/boot/
	@printf 'set timeout=%s\nset default=0\n\nmenuentry "my kernel (Standard)" {\n  multiboot2 /boot/boot.elf\n  boot\n}\n\nmenuentry "my kernel (Debug Mode / Verbose)" {\n  multiboot2 /boot/boot.elf debug_mode=1 log_level=verbose\n  boot\n}\n' '$(GRUB_TIMEOUT)' > isodir/boot/grub/grub.cfg
	@grub-mkrescue -o $@ isodir 2>/dev/null

disk.img:
	@echo "[DISK] Creating 64MB virtual disk image..."
	@dd if=/dev/zero of=disk.img bs=1M count=64 status=none

iso: kernel.iso

run: kernel.iso
	$(QEMU) $(QEMU_FLAGS)

run-ext: kernel.iso
	$(QEMU) $(QEMU_EXT_FLAGS)

run-gdb: kernel.iso
	$(QEMU) $(QEMU_GDB_FLAGS)

run-tcg: kernel.iso
	$(QEMU) $(QEMU_TCG_FLAGS)

run-tcg-gdb: kernel.iso
	$(QEMU) $(QEMU_TCG_FLAGS) -s -S

gdb: $(TARGET) $(GDB_SCRIPT)
	gdb -q $(TARGET) -x $(GDB_SCRIPT)

clean:
	rm -rf $(BUILD_DIR) *.elf isodir kernel.iso qemu.log debugcon.log

-include $(DEPS)
