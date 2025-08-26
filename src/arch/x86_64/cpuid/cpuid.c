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

uint64_t read_msr(uint32_t msr) {
   uint32_t lo, hi;

   asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));

   return ((uint64_t)hi << 32) | lo;
}

void write_msr(uint32_t msr, uint64_t val) {
   uint32_t lo, hi;

   lo = (uint64_t)(val & 0xFFFFFFFF);
   hi = val >> 32;

   asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}