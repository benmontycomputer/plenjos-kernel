%macro isr_err_stub 1
isr_stub_%+%1:
    cli
                        ; Do not need to push dummy error code 
    push %1             ; Interrupt number

    push r15            ; Save general-purpose registers in reverse order (to match RESTORE_REGISTERS)
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rsi
    push rdi
    push rbp
    push rdx
    push rcx
    push rbx
    push rax

    mov rax, cr4         ; save control registers
    push rax
    mov rax, cr3
    push rax
    mov rax, cr2
    push rax
    mov rax, cr0
    push rax
    
    mov ax, ds           ; Save segment registers
    push rax
    mov ax, es
    push rax
    push fs
    push gs

    mov rdi, rsp         ; Pass pointer to the `registers_t` structure
    cld                  ; Clear the direction flag
    call exception_handler     ; Call the interrupt handler

    pop gs               ; Restore segment registers
    pop fs
    pop rax
    mov es, ax
    pop rax
    mov ds, ax

    pop rax             ; restore control registers
    mov cr0, rax
    pop rax
    mov cr2, rax
    pop rax
    mov cr3, rax
    pop rax
    mov cr4, rax

    pop rax             ; Restore general-purpose registers
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rdi
    pop rsi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    add rsp, 8           ; Remove the pushed interrupt number only

    iretq                ; Return from the interrupt using IRETQ
%endmacro
; if writing for 64-bit, use iretq instead
%macro isr_no_err_stub 1
isr_stub_%+%1:
    cli;

    push 0          ; Dummy error code
    push %1         ; Interrupt number
    
    push r15        ; Save general-purpose registers in reverse order (to match RESTORE_REGISTERS)
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rsi
    push rdi
    push rbp
    push rdx
    push rcx
    push rbx
    push rax

    mov rax, cr4         ; save control registers
    push rax
    mov rax, cr3
    push rax
    mov rax, cr2
    push rax
    mov rax, cr0
    push rax

    mov ax, ds      ; Save segment registers
    push rax
    mov ax, es
    push rax
    push fs
    push gs
    
    mov rdi, rsp         ; Pass pointer to the `registers_t` structure
    cld                  ; Clear the direction flag
    call exception_handler     ; Call the interrupt handler

    pop gs               ; Restore segment registers
    pop fs
    pop rax
    mov es, ax
    pop rax
    mov ds, ax

    pop rax             ; restore control registers
    mov cr0, rax
    pop rax
    mov cr2, rax
    pop rax
    mov cr3, rax
    pop rax
    mov cr4, rax

    pop rax             ; Restore general-purpose registers
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rdi
    pop rsi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    
    add rsp, 16         ; Clean up interrupt no and dummy error code

    iretq               ; Return from the interrupt using IRETQ (iret values remain intact)
%endmacro

; Setup Interrupt Request(IRQ)
%macro IRQ 2
isr_stub_%+%2:
    cli
    ; Stack already has 5*8=40 bytes data
    push 0               ; Dummy error code
    push %2              ; Interrupt number
    
    ; Save general-purpose registers in reverse order (to match RESTORE_REGISTERS)
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rsi
    push rdi
    push rbp
    push rdx
    push rcx
    push rbx
    push rax

    mov rax, cr4         ; save control registers
    push rax
    mov rax, cr3
    push rax
    mov rax, cr2
    push rax
    mov rax, cr0
    push rax

    ; Save segment registers
    mov ax, ds
    push rax
    mov ax, es
    push rax
    push fs
    push gs
    
    mov rdi, rsp                    ; Pass the current stack pointer to `pic_irq_handler`
    cld
    call irq_handler
    
    ; Restore segment registers
    pop gs
    pop fs
    pop rax
    mov es, ax
    pop rax
    mov ds, ax

    pop rax             ; restore control registers
    mov cr0, rax
    pop rax
    mov cr2, rax
    pop rax
    mov cr3, rax
    pop rax
    mov cr4, rax

    ; Restore general-purpose registers
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rdi
    pop rsi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    add rsp, 16 ; Clean up interrupt no and dummy error code
    
    iretq                    ; Return from Interrupt
%endmacro

extern irq_handler

; The first number will define irq function number like irq0, irq2 etc second is interrupt number 
; This will push error code 0
IRQ   0,    32
IRQ   1,    33      ; Keyboard Interrupt
IRQ   2,    34
IRQ   3,    35
IRQ   4,    36
IRQ   5,    37
IRQ   6,    38
IRQ   7,    39
IRQ   8,    40
IRQ   9,    41
IRQ  10,    42
IRQ  11,    43
IRQ  12,    44      ; Mouse Interrupt
IRQ  13,    45
IRQ  14,    46
IRQ  15,    47

IRQ  16,    48      ; APIC Timer Interrupt
IRQ  17,    49      ; HPET Timer Interrupt
IRQ  18,    50      ; IPI
IRQ  19,    51

; Custom System Call
IRQ  96,    128    ; System Call

extern exception_handler
isr_no_err_stub 0
isr_no_err_stub 1
isr_no_err_stub 2
isr_no_err_stub 3
isr_no_err_stub 4
isr_no_err_stub 5
isr_no_err_stub 6
isr_no_err_stub 7
isr_err_stub    8
isr_no_err_stub 9
isr_err_stub    10
isr_err_stub    11
isr_err_stub    12
isr_err_stub    13
isr_err_stub    14
isr_no_err_stub 15
isr_no_err_stub 16
isr_err_stub    17
isr_no_err_stub 18
isr_no_err_stub 19
isr_no_err_stub 20
isr_no_err_stub 21
isr_no_err_stub 22
isr_no_err_stub 23
isr_no_err_stub 24
isr_no_err_stub 25
isr_no_err_stub 26
isr_no_err_stub 27
isr_no_err_stub 28
isr_no_err_stub 29
isr_err_stub    30
isr_no_err_stub 31

global isr_stub_table
isr_stub_table:
%assign i 0 
%rep    52
    dq isr_stub_%+i ; use DQ instead if targeting 64-bit
%assign i i+1 
%endrep
dq isr_stub_%+128