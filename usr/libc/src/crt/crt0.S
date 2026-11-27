; https://wiki.osdev.org/Creating_a_C_Library

.section .text

.global _start
_start:
	# ld.so (and the kernel) enter here with rsp pointing at argc, SysV-style:
	#   (rsp) = argc, 8(rsp) = argv[0] ... NULL, then envp[0] ... NULL
	# No argument registers are set up, so read everything off the stack.
	xorq %rbp, %rbp          # end of the frame pointer chain
	movq (%rsp), %rdi        # argc
	leaq 8(%rsp), %rsi       # argv
	leaq 16(%rsp,%rdi,8), %rdx # envp = argv + argc + 1
	andq $-16, %rsp          # keep the stack 16-byte aligned for calls

	# Keep them in callee-saved registers across the init calls.
	movq %rdi, %r12
	movq %rsi, %r13
	movq %rdx, %r14

	# initialize_standard_library(argc, argv, envc, envp)
	xorl %edx, %edx          # envc (unused)
	movq %r14, %rcx
	call initialize_standard_library

	# Run the global constructors.
	call _init

	# main(argc, argv, envp)
	movq %r12, %rdi
	movq %r13, %rsi
	movq %r14, %rdx
	call main

	# Terminate the process with the exit code.
	movl %eax, %edi
	call exit
.size _start, . - _start