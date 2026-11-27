section .text
bits 64

global jump_to_entry:function hidden

; jump_to_entry(entry, stack)
jump_to_entry:
    ; rdi: entry, rsi: stack (must be 16-byte aligned)
    mov rsp, rsi
    xor ebp, ebp        ; outermost frame
    xor edx, edx        ; rdx technically passes an atexit/fini function for _start
    jmp rdi