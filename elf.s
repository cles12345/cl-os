section .text
global _start
_start:
    mov eax, 0
    mov ebx, s1
    mov ecx, 17
    int 0x80

.halt:
    jmp .halt

section .data
s1: db "hello from user", 10, 0