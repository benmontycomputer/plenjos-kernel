#include "memory/vmdump.h"

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/apic/ioapic.h"
#include "kernel.h"
#include "lib/stdio.h"
#include "memory/detect.h"
#include "memory/mm.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern char __kernel_start[];
extern char __kernel_end[];

#define E_PRESENT 0x001ULL
#define E_WRITE   0x002ULL
#define E_USER    0x004ULL
#define E_PWT     0x008ULL
#define E_PCD     0x010ULL
#define E_PS      0x080ULL // page size: 1 GiB in a PDPT entry, 2 MiB in a PD entry
#define E_NX      (1ULL << 63)

#define ADDR_4K 0x000FFFFFFFFFF000ULL
#define ADDR_2M 0x000FFFFFFFE00000ULL
#define ADDR_1G 0x000FFFFFC0000000ULL

// Same value detect.c gives TSS_STACK_ADDR before any core has bumped it
#define TSS_REGION_BASE 0xFFFF8E0000000000ULL

// Key bits describing a run of pages that can be merged
#define K_W   0x01u
#define K_U   0x02u
#define K_NX  0x04u
#define K_PWT 0x08u
#define K_PCD 0x10u
#define K_SZ_SHIFT 5 // 0 = 4 KiB, 1 = 2 MiB, 2 = 1 GiB

static const char *const size_names[3] = { "4K", "2M", "1G" };
static const uint64_t size_bytes[3]    = { 0x1000ULL, 0x200000ULL, 0x40000000ULL };

struct vm_region {
    const char *name;
    uint64_t start;
    uint64_t end; // exclusive
};

struct vm_run {
    bool active;
    bool contig; // physical addresses are contiguous across the whole run
    uint32_t key;
    uint64_t start;
    uint64_t end; // exclusive
    uint64_t phys;
};

struct vm_totals {
    uint64_t bytes;
    uint64_t wx_bytes;   // writable AND executable
    uint64_t user_bytes; // reachable from ring 3
    uint64_t pages[3];
};

struct vm_ctx {
    uint32_t halves;
    struct vm_run run;
    struct vm_totals tot[2]; // [0] = user half, [1] = kernel half
    struct vm_region regions[16];
    size_t region_count;
};

// Effective permissions accumulated while descending the tables
struct vm_acc {
    bool write;
    bool user;
    bool nx;
};

static inline struct vm_acc acc_step(struct vm_acc a, uint64_t entry) {
    a.write = a.write && (entry & E_WRITE);
    a.user  = a.user && (entry & E_USER);
    a.nx    = a.nx || (entry & E_NX);
    return a;
}

static void add_region(struct vm_ctx *c, const char *name, uint64_t start, uint64_t end) {
    if (c->region_count >= sizeof(c->regions) / sizeof(c->regions[0])) return;
    c->regions[c->region_count++] = (struct vm_region) { name, start, end };
}

static void run_flush(struct vm_ctx *c) {
    struct vm_run *r = &c->run;
    if (!r->active) return;

    uint32_t k = r->key;
    char perm[4];
    perm[0] = 'r';
    perm[1] = (k & K_W) ? 'w' : '-';
    perm[2] = (k & K_NX) ? '-' : 'x';
    perm[3] = 0;

    printf("%p-%p %s %c %s%s%s %lu KiB ", (void *)r->start, (void *)r->end, perm, (k & K_U) ? 'U' : 'S',
           size_names[k >> K_SZ_SHIFT], (k & K_PCD) ? " UC" : "", (k & K_PWT) ? " WT" : "",
           (uint64_t)((r->end - r->start) >> 10));

    if (r->contig) {
        printf("phys %p", (void *)r->phys);
    } else {
        printf("phys scattered");
    }

    for (size_t i = 0; i < c->region_count; i++) {
        const struct vm_region *g = &c->regions[i];
        if (r->start < g->end && g->start < r->end) printf(" [%s]", g->name);
    }
    printf("\n");

    r->active = false;
}

static void emit_leaf(struct vm_ctx *c, uint64_t virt, uint32_t size_class, uint64_t phys, uint64_t entry,
                      struct vm_acc acc, bool kernel_half) {
    uint64_t size = size_bytes[size_class];
    uint32_t key  = (acc.write ? K_W : 0) | (acc.user ? K_U : 0) | (acc.nx ? K_NX : 0)
                   | ((entry & E_PWT) ? K_PWT : 0) | ((entry & E_PCD) ? K_PCD : 0) | (size_class << K_SZ_SHIFT);

    struct vm_totals *t = &c->tot[kernel_half ? 1 : 0];
    t->bytes += size;
    t->pages[size_class]++;
    if (acc.write && !acc.nx) t->wx_bytes += size;
    if (acc.user) t->user_bytes += size;

    struct vm_run *r = &c->run;
    if (r->active && r->key == key && r->end == virt) {
        if (phys != r->phys + (r->end - r->start)) r->contig = false;
        r->end += size;
        return;
    }

    run_flush(c);
    r->active = true;
    r->contig = true;
    r->key    = key;
    r->start  = virt;
    r->end    = virt + size;
    r->phys   = phys;
}

static inline uint64_t *table_of(uint64_t entry) {
    return (uint64_t *)phys_to_virt(entry & ADDR_4K);
}

static void walk(struct vm_ctx *c, uint64_t *pml4) {
    const struct vm_acc root = { true, true, false };

    for (uint64_t i4 = 0; i4 < 512; i4++) {
        bool kernel_half = i4 >= 256;
        if (!(c->halves & (kernel_half ? VM_DUMP_KERNEL_HALF : VM_DUMP_USER_HALF))) continue;

        uint64_t e4 = pml4[i4];
        if (!(e4 & E_PRESENT)) continue;
        struct vm_acc a4 = acc_step(root, e4);
        uint64_t *pdpt   = table_of(e4);

        for (uint64_t i3 = 0; i3 < 512; i3++) {
            uint64_t e3 = pdpt[i3];
            if (!(e3 & E_PRESENT)) continue;

            uint64_t base3 = (i4 << 39) | (i3 << 30);
            if (kernel_half) base3 |= 0xFFFF000000000000ULL;

            struct vm_acc a3 = acc_step(a4, e3);
            if (e3 & E_PS) {
                emit_leaf(c, base3, 2, e3 & ADDR_1G, e3, a3, kernel_half);
                continue;
            }
            uint64_t *pd = table_of(e3);

            for (uint64_t i2 = 0; i2 < 512; i2++) {
                uint64_t e2 = pd[i2];
                if (!(e2 & E_PRESENT)) continue;

                uint64_t base2 = base3 | (i2 << 21);

                struct vm_acc a2 = acc_step(a3, e2);
                if (e2 & E_PS) {
                    emit_leaf(c, base2, 1, e2 & ADDR_2M, e2, a2, kernel_half);
                    continue;
                }
                uint64_t *pt = table_of(e2);

                for (uint64_t i1 = 0; i1 < 512; i1++) {
                    uint64_t e1 = pt[i1];
                    if (!(e1 & E_PRESENT)) continue;

                    emit_leaf(c, base2 | (i1 << 12), 0, e1 & ADDR_4K, e1, acc_step(a2, e1), kernel_half);
                }
            }
        }
    }

    run_flush(c);
}

static void print_totals(const char *name, const struct vm_totals *t) {
    printf("  %s: %lu KiB mapped (4K x%lu, 2M x%lu, 1G x%lu), W+X %lu KiB, user-reachable %lu KiB\n", name,
           t->bytes >> 10, t->pages[0], t->pages[1], t->pages[2], t->wx_bytes >> 10, t->user_bytes >> 10);
}

void vm_dump(const char *label, pml4_t *pml4, uint32_t halves) {
    if (!pml4) {
        printf("vm_dump(%s): NULL pml4\n", label);
        return;
    }

    // Large (~600 B) but only used at debug time, so keep it off the stack by making it static
    static struct vm_ctx ctx;
    ctx = (struct vm_ctx) { 0 };
    ctx.halves = halves;

    add_region(&ctx, "kernel image", (uint64_t)__kernel_start, (uint64_t)__kernel_end);
    add_region(&ctx, "kernel heap", KERNEL_HEAP_START_ADDR, KERNEL_HEAP_START_ADDR + (1ULL << 40));
    add_region(&ctx, "tss stacks", TSS_REGION_BASE, TSS_STACK_ADDR);
    add_region(&ctx, "hhdm", HHDM_OFFSET, HHDM_OFFSET + (1ULL << 40));
    add_region(&ctx, "lapic", LAPIC_BASE, LAPIC_BASE + PAGE_LEN);
    add_region(&ctx, "ioapic", IOAPIC_ADDR, IOAPIC_ADDR + PAGE_LEN);
    add_region(&ctx, "ld.so", LINKER_OFFS, LINKER_OFFS + (16ULL << 20));
    add_region(&ctx, "user dso area", 0x00007F0000000000ULL, 0x00007FFF00000000ULL);
    add_region(&ctx, "user stacks 0-7", 0x00007FFF8FF00000ULL, 0x0000800000000000ULL);

    printf("\n=== vm_dump: %s (pml4 %p) ===\n", label, (void *)pml4);
    printf("range (start-end exclusive)  perm U/S size  KiB  phys  [known regions]\n");

    walk(&ctx, (uint64_t *)pml4);

    printf("--- totals ---\n");
    if (halves & VM_DUMP_USER_HALF) print_totals("user half  ", &ctx.tot[0]);
    if (halves & VM_DUMP_KERNEL_HALF) print_totals("kernel half", &ctx.tot[1]);
    printf("=== end vm_dump ===\n\n");
}

bool vm_dump_requested(const char *cmdline) {
    static const char word[] = "vmdump";

    if (!cmdline) return false;

    for (const char *p = cmdline; *p; p++) {
        bool starts_word = (p == cmdline) || (p[-1] == ' ');
        if (!starts_word) continue;

        size_t i = 0;
        while (word[i] && p[i] == word[i]) i++;
        if (!word[i] && (p[i] == 0 || p[i] == ' ')) return true;
    }

    return false;
}
