CC = i386-elf-gcc
CCFLAGS = -m32 -fno-stack-protector -fno-builtin
ASM= nasm
LD = i386-elf-ld

SRC_DIR = src
BUILD_DIR = build
KERNEL_DIR = $(SRC_DIR)/kernel

$(BUILD_DIR)/cl-os.iso: $(BUILD_DIR)/kernel
	@mkdir -p $(BUILD_DIR)/boot
	@mkdir -p $(BUILD_DIR)/boot/grub
	cp grub.cfg $(BUILD_DIR)/boot/grub
	cp $(BUILD_DIR)/kernel $(BUILD_DIR)/boot
	grub-mkrescue -o $(BUILD_DIR)/cl-os.iso $(BUILD_DIR)

$(BUILD_DIR)/kernel:
	@mkdir -p $(BUILD_DIR)
	$(ASM) -f elf32 $(KERNEL_DIR)/boot.s -o $(BUILD_DIR)/boot.o
	$(CC) $(CCFLAGS) -c $(KERNEL_DIR)/main.c -o $(BUILD_DIR)/main.o 
	$(LD) -m elf_i386 -T linker.ld -o $(BUILD_DIR)/kernel $(BUILD_DIR)/boot.o $(BUILD_DIR)/main.o

run: $(BUILD_DIR)/cl-os.iso
	qemu-system-i386 $(BUILD_DIR)/cl-os.iso

clean:
	rm -rf $(BUILD_DIR)