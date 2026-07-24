global jump_to_user_mode
jump_to_user_mode:
    cli 

    mov eax, [esp + 4]
    mov ebx, [esp + 8]

    mov cx, (4 * 8) | 3
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx

	push (4 * 8) | 3 
    push ebx
    pushf
    or dword [esp], 0x200
    push (3 * 8) | 3
    push eax
    iret