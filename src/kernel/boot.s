bits 32

MBOOT_PAGE_ALIGN equ 1 << 0
MBOOT_MEM_INFO equ 1 << 1
MBOOT_USE_GFX equ 0

MBOOT_MAGIC equ 0x1BADB002
MBOOT_FLAGS equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO | MBOOT_USE_GFX
MBOOT_CHECKSUM equ -(MBOOT_MAGIC + MBOOT_FLAGS)

section .multiboot
align 4
    dd MBOOT_MAGIC 
    dd MBOOT_FLAGS
    dd MBOOT_CHECKSUM
    dd 0, 0, 0, 0, 0

    dd 0
    dd 800
    dd 629
    dd 32     

section .bss
global stack_top
global stack_bottom
align 16
stack_bottom:
    resb 16384 * 8
stack_top:

section .data
grub_magic: dd 0
grub_info: dd 0

section .boot
global _start
global grub_magic
global grub_info
_start:
    mov [grub_magic - 0xC0000000], ebx
    mov [grub_info - 0xC0000000], eax
    mov ecx, (initial_page_dir - 0xC0000000)
    mov cr3, ecx

    mov ecx, cr4
    or ecx, 0x10
    mov cr4, ecx

    mov ecx, cr0
    or ecx, 0x80000000
    mov cr0, ecx

    jmp higher_half

section .text
extern kmain
higher_half:
    mov esp, stack_top
    xor ebp, ebp

    push dword [grub_magic] 
    push dword [grub_info] 
    call kmain
.halt:
    hlt
    jmp .halt

section .data
align 4096
global initial_page_dir
initial_page_dir:
    dd 0x00000083
    times 767 dd 0

    dd 0x00000083
    times 255 dd 0

    dd (initial_page_dir - 0xC0000000) + 0x003