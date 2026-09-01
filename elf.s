section .text
global _start
_start:
    mov eax, 4
    mov ebx, 0
    mov ecx, s1
    mov edx, 16
    int 0x80

.halt:
    jmp .halt

section .data
s1: db "hello from user", 10