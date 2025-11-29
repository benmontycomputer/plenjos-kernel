; global exception_handler_switch_to_kernel

; DON'T USE THIS
exception_handler_switch_to_kernel:
    pop r15 ; return address
    swapgs
    mov rcx, 0xC0000101
    rdmsr
    shl rdx, 32
    or rax, rdx
    mov rbx, [rax+0x10]
    mov cr3, rbx
    mov rbx, [rax+0x00]
    mov rsp, rbx
    push r15 ; return address
    ret