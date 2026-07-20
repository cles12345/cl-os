CC = i386-elf-gcc
CCFLAGS = -m32 -fno-stack-protector -fno-builtin -ffreestanding -nostdlib -nostdinc -Wall -Wextra -Werror -std=gnu11 -O2 -g -I$(KERNEL_DIR) -I$(KERNEL_DIR)/UTILL
ASM = nasm
LD = i386-elf-ld
LDFLAGS = -m elf_i386 -T linker.ld

SRC_DIR = src
BUILD_DIR = build
KERNEL_DIR = $(SRC_DIR)/kernel

C_SRCS = $(shell find $(KERNEL_DIR) -name "*.c")
ASM_SRCS = $(shell find $(KERNEL_DIR) -name "*.asm")
S_SRCS = $(shell find $(KERNEL_DIR) -name "*.s")
C_OBJS = $(patsubst $(KERNEL_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SRCS))
ASM_OBJS = $(patsubst $(KERNEL_DIR)/%.asm, $(BUILD_DIR)/%_asm.o, $(ASM_SRCS))
S_OBJS = $(patsubst $(KERNEL_DIR)/%.s, $(BUILD_DIR)/%_s.o, $(S_SRCS))
OBJS = $(C_OBJS) $(ASM_OBJS) $(S_OBJS)

.PHONY: all run clean

all: $(BUILD_DIR)/cl-os.iso

$(BUILD_DIR)/cl-os.iso: $(BUILD_DIR)/kernel
	rm -f $(BUILD_DIR)/cl-os.iso 2>/dev/null || true
	@mkdir -p $(BUILD_DIR)/boot
	@mkdir -p $(BUILD_DIR)/boot/grub
	cp grub.cfg $(BUILD_DIR)/boot/grub
	cp $(BUILD_DIR)/kernel $(BUILD_DIR)/boot
	grub-mkrescue -o $(BUILD_DIR)/cl-os.iso $(BUILD_DIR)

$(BUILD_DIR)/kernel: $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CCFLAGS) -c $< -o $@

$(BUILD_DIR)/%_asm.o: $(KERNEL_DIR)/%.asm
	@mkdir -p $(dir $@)
	$(ASM) -f elf32 $< -o $@

$(BUILD_DIR)/%_s.o: $(KERNEL_DIR)/%.s
	@mkdir -p $(dir $@)
	$(ASM) -f elf32 $< -o $@

run: $(BUILD_DIR)/cl-os.iso
	qemu-system-i386 -cdrom $<

clean:
	rm -rf $(BUILD_DIR)