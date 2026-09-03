CC = i386-elf-gcc
CCFLAGS = -m32 -fno-stack-protector -fno-builtin -ffreestanding -nostdlib -nostdinc -Wall -Wextra -std=gnu11 -g -I$(KERNEL_DIR) -I$(KERNEL_DIR)/UTILL
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

all: $(BUILD_DIR)/cl-os.img
 
$(BUILD_DIR)/cl-os.img: $(BUILD_DIR)/kernel $(BUILD_DIR)/boot/elf
	rm -f $(BUILD_DIR)/cl-os.img 2>/dev/null || true
	@mkdir -p $(BUILD_DIR)/boot
	@mkdir -p $(BUILD_DIR)/boot/grub
	cp grub.cfg $(BUILD_DIR)/boot/grub
	cp $(BUILD_DIR)/kernel $(BUILD_DIR)/boot
	dd if=/dev/zero of=$@ bs=1M count=100
	mkfs.ext3 -F -O ^extents,^flex_bg,^64bit,^metadata_csum,^mmp,^dir_nlink,^extra_isize,^huge_file,^uninit_bg -I 128 -b 1024 $@
	sudo losetup -f $@
	LOOP=$$(sudo losetup -l | grep $@ | tail -1 | awk '{print $$1}'); \
	sudo mount $$LOOP /mnt; \
	sudo mkdir -p /mnt/boot; \
	sudo mkdir -p /mnt/boot/grub; \
	sudo cp $(BUILD_DIR)/boot/grub/grub.cfg /mnt/boot/grub/; \
	sudo cp $(BUILD_DIR)/kernel /mnt/boot/; \
	sudo cp $(BUILD_DIR)/boot/elf /mnt/; \
	sudo grub-install --target=i386-pc --boot-directory=/mnt/boot --force $$LOOP; \
	sudo umount /mnt; \
	sudo losetup -d $$LOOP

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

$(BUILD_DIR)/boot/elf: elf.s
	@mkdir -p $(BUILD_DIR)/boot
	$(ASM) -f elf32 $< -o elf.o
	$(LD) -m elf_i386 -o $@ elf.o
	rm -rf elf.o

run: $(BUILD_DIR)/cl-os.img
	qemu-system-i386 -hda $<

clean:
	rm -rf $(BUILD_DIR)