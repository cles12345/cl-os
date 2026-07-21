global idt_flush
idt_flush:
    mov eax, [esp + 4]
    lidt [eax]
    ret

%macro ISR_NOERRORCODE 1
    global isr%1
    isr%1:
        cli
        push dword 0
        push dword %1
        jmp isr_common_stub
%endmacro

%macro ISR_ERRORCODE 1
    global isr%1
    isr%1:
        cli
        push dword %1
        jmp isr_common_stub
%endmacro

ISR_NOERRORCODE 0
ISR_NOERRORCODE 1
ISR_NOERRORCODE 2
ISR_NOERRORCODE 3
ISR_NOERRORCODE 4
ISR_NOERRORCODE 5
ISR_NOERRORCODE 6
ISR_NOERRORCODE 7
ISR_ERRORCODE    8
ISR_NOERRORCODE 9
ISR_ERRORCODE    10
ISR_ERRORCODE    11
ISR_ERRORCODE    12
ISR_ERRORCODE    13
ISR_ERRORCODE    14
ISR_NOERRORCODE 15
ISR_NOERRORCODE 16
ISR_ERRORCODE    17
ISR_NOERRORCODE 18
ISR_NOERRORCODE 19
ISR_NOERRORCODE 20
ISR_NOERRORCODE 21
ISR_NOERRORCODE 22
ISR_NOERRORCODE 23
ISR_NOERRORCODE 24
ISR_NOERRORCODE 25
ISR_NOERRORCODE 26
ISR_NOERRORCODE 27
ISR_NOERRORCODE 28
ISR_NOERRORCODE 29
ISR_ERRORCODE    30
ISR_NOERRORCODE 31

extern isr_handler
isr_common_stub:
    pusha
    mov eax, ds
    push eax
    mov eax, cr2
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call isr_handler
    add esp, 4

    pop eax
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    iret