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

extern sched_stack_top

%macro switch_to_core_stack 0      ; leaves the core id in edi
    swapgs
    mov  ecx, dword [gs:0x18]
    swapgs
    mov  edi, ecx
    lea  rdx, [rel sched_stack_top]
    mov  rsp, [rdx + rcx*8]
%endmacro

global cpu_scheduler_task
extern cpu_scheduler_task_c
cpu_scheduler_task:
    cli
    switch_to_core_stack
    call cpu_scheduler_task_c

.hang:
    hlt
    jmp .hang


global kcontext_block
global kcontext_resume

; void kcontext_block(uint64_t *saved_rsp, mutex *m)
kcontext_block:
    cli
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov  [rdi], rsp             ; where to restart from
    mov  r12, rsi               ; the mutex (r12 was just saved)

    switch_to_core_stack

    mov  byte [r12], 0          ; unlock m (atomic_flag clear); from here another core may resume the thread
    call cpu_scheduler_task_c

; void kcontext_resume(uint64_t saved_rsp)
kcontext_resume:
    cli
    mov  rsp, rdi
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp
    ret                         ; returns into block(), right after kcontext_block

global user_task_hook
user_task_hook:
    pop rdx
    pop rsi
    pop rdi
    iretq