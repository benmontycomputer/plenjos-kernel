; Heavily modified from KeblaOS

; Offsets for the registers_t structure
%define SEG_REG_GS  8*0
%define SEG_REG_FS  8*1
%define SEG_REG_ES  8*2
%define SEG_REG_DS  8*3

%define CR_REG_CR0  8*4
%define CR_REG_CR2  8*5
%define CR_REG_CR3  8*6
%define CR_REG_CR4  8*7

%define GEN_REG_RAX 8*8
%define GEN_REG_RBX 8*9
%define GEN_REG_RCX 8*10
%define GEN_REG_RDX 8*11
%define GEN_REG_RBP 8*12
%define GEN_REG_RDI 8*13
%define GEN_REG_RSI 8*14
%define GEN_REG_R8  8*15
%define GEN_REG_R9  8*16
%define GEN_REG_R10 8*17
%define GEN_REG_R11 8*18
%define GEN_REG_R12 8*19
%define GEN_REG_R13 8*20
%define GEN_REG_R14 8*21
%define GEN_REG_R15 8*22

%define INT_NO      8*23
%define ERR_CODE    8*24

%define REG_IRET_RIP  8*25
%define REG_IRET_CS   8*26
%define REG_IRET_RFLAGS   8*27
%define REG_IRET_RSP      8*28
%define REG_IRET_SS 8*29

%macro load_task_general_registers 0
    mov rax, [r15 + GEN_REG_RAX]
    mov rbx, [r15 + GEN_REG_RBX]
    mov rcx, [r15 + GEN_REG_RCX]
    mov rdx, [r15 + GEN_REG_RDX]
    mov rsi, [r15 + GEN_REG_RSI]
    mov r8, [r15 + GEN_REG_R8]
    mov r9, [r15 + GEN_REG_R9]
    mov r10, [r15 + GEN_REG_R10]
    mov r11, [r15 + GEN_REG_R11]
    mov r12, [r15 + GEN_REG_R12]
    mov r13, [r15 + GEN_REG_R13]
    mov r14, [r15 + GEN_REG_R14]
    mov rdi, [r15 + GEN_REG_RDI]
    mov r15, [r15 + GEN_REG_R15]
%endmacro

global _finalize_task_switch
_finalize_task_switch:
    cli
    ; GS_BASE always holds the user-level GS base (the thread's own gsbase page, written by the caller); the core's gsbase page lives in
    ; IA32_KERNEL_GS_BASE and is only reached through a swapgs pair, so no swapgs is needed before the iretq.
    ;
    ; The iretq frame is built on the per-core kernel stack (the one we are already running on, mapped in every
    ; process), NOT on the thread's user stack: a frame on the user stack could be rewritten by another thread of the
    ; process before the iretq, and would corrupt the red zone. The user rsp is only loaded by the iretq itself.
    mov r15, rdi                  ; the thread's registers_t
    mov rdi, [r15 + CR_REG_CR3]
    mov cr3, rdi                  ; load the new thread's page tables (the kernel and the kernel stacks stay mapped)
    mov rbp, [r15 + GEN_REG_RBP]
    xor rax, rax
    mov ax, 0x23                  ; user data segment; 0x20 | 0x3
    mov ds, ax
    mov es, ax
    push qword 0x23               ; SS
    push qword [r15 + REG_IRET_RSP]
    push qword [r15 + REG_IRET_RFLAGS]
    push qword 0x1B               ; CS: user code segment; 0x18 | 0x3
    push qword [r15 + REG_IRET_RIP]
    load_task_general_registers
    iretq

global cpu_scheduler_task
extern cpu_scheduler_task_c
extern tss_obj
cpu_scheduler_task:
    cli

    ; Load the current core ID and switch to that core's TSS rsp0.
    swapgs
    mov ecx, dword [gs:0x18] ; read processor id from this core's gsbase page (in KERNEL_GS_BASE)
    swapgs
    mov edi, ecx ; first argument of cpu_scheduler_task_c(uint32_t curr_core)
    lea rdx, [rel tss_obj] ; load address of tss_obj into rdx
    imul rcx, rcx, 0x68 ; multiplies processor_id by size of tss_obj
    mov rsp, [rdx + rcx + 0x4]

    call cpu_scheduler_task_c

.hang:
    hlt
    jmp .hang
    