#ifndef _KERNEL_PLATFORM_H
#define _KERNEL_PLATFORM_H

#ifdef __x86_64__
#    define K_IS_ARCH_X86_64() 1
#else
#    define K_IS_ARCH_X86_64() 0
#endif

#define ARCH(arch) (K_IS_ARCH_##arch())

#endif