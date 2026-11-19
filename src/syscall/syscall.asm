global syscall_entry
extern syscall_dispatch

syscall_entry:
    swapgs                      ; switch to kernel gsbase
    mov [gs:SCRATCH_RAX], rax   ; save rax
    mov [gs:SCRATCH_RSP], rsp   ; save rsp

    mov rax, cr3
    mov [gs:SCRATCH_CR3], rax   ; save cr3
    mov rax, [gs:0x20]
    mov cr3, rax                ; load kernel page table

    mov rsp, [gs:0x10]          ; load the user thread's kernel syscall stack

    ; Build the stack frame for iretq
    push qword 0x23             ; SS
    push qword [gs:SCRATCH_RSP] ; user RSP
    push r11                    ; user RFLAGS, saved by syscall
    push qword 0x1B             ; CS
    push rcx                    ; user RIP, saved by syscall

    mov rax, [gs:SCRATCH_RAX]
    swapgs                      ; done with kernel gsbase

    push 0 ; dummy error code
    push 0x80                   ; dummy int_no

    isr_save_ctx

    ; Note: regs->cr3 is NOT correct at this point and should not be used by syscall handler. Syscall handler MUST correct it before returning.

    mov rdi, rsp                ; pass registers_t *
    cld
    call syscall_dispatch

    isr_restore_ctx

    add rsp, 16

    iretq                       ; sysret can fail on non-canonical RIP