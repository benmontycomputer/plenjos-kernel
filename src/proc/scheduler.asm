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
    ; First, save the current location of the kernel stack pointer to the kernel gsbase:
    swapgs
    mov [gs:0], rsp
    swapgs
    ; Next, load the registers_t struct into r15:
    mov r15, rdi
    mov rdi, [r15 + CR_REG_CR3]   ; Prepare for page table loading
    mov rsp, [r15 + REG_IRET_RSP] ; Load new thread's stack

    ; After loading the new thread's page table, we are still in ring 0, so we can access
    ; kernel data structures that are mapped into the process's address space.
    mov cr3, rdi ; Load new thread's page tables
    mov rbp, [r15 + GEN_REG_RBP]
    mov r14, [r15 + REG_IRET_RIP]
    xor rax, rax ; zeroes out the entire rax register; otherwise the topmost bits might be wrong when we push it to stack
    mov ax, 0x23        ; user data segment; 0x20 | 0x3
    mov ds, ax
    mov es, ax
    push rax            ; push user data segment
    lea rax, [rsp + 0x08] ; the offset is needed because we have already pushed user data segment to the stack
    push rax            ; push user stack
    push qword [r15 + REG_IRET_RFLAGS]
    push 0x1B           ; user code segment; 0x18 | 0x3
    push r14            ; push user instruction pointer
    load_task_general_registers
    .end:
    iretq
