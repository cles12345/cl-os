section .text
global _start
_start:
    mov eax, 4
    mov ebx, 1
    mov ecx, s1
    mov edx, 17
    int 0x80

    jmp _start

section .data
s1: db "hello from user2", 10