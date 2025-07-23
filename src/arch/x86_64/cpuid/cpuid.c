#include <stdint.h>
#include <stdbool.h>

#include <cpuid.h>

#include "arch/x86_64/cpuid/cpuid.h"

const uint32_t CPUID_FLAG_MSR = 1 << 5;

bool cpuHasMSR()
{
   static uint32_t a, d, unused; // eax, edx
   __get_cpuid(1, &a, &unused, &unused, &d);
   return d & CPUID_FLAG_MSR;
}

void cpuGetMSR(uint32_t msr, uint32_t *lo, uint32_t *hi)
{
   asm volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

void cpuSetMSR(uint32_t msr, uint32_t lo, uint32_t hi)
{
   asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}