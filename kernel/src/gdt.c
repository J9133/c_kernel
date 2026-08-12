#include <stdint.h>
#include "gdt.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct gdt_entry gdt[3] = {
    {0, 0, 0, 0, 0, 0},                     // Null descriptor
    {0xFFFF, 0, 0, 0x9A, 0xAF, 0},           // Kernel code segment
    {0xFFFF, 0, 0, 0x92, 0xAF, 0},           // Kernel data segment
};

static struct gdt_ptr gdtp = {
    .limit = sizeof(gdt) - 1,
    .base = (uint64_t)&gdt
};

static inline void load_gdt(struct gdt_ptr *ptr) {
    __asm__ volatile (
        "lgdt (%0)\n"
        "push $0x08\n"
        "lea 1f(%%rip), %%rax\n"
        "push %%rax\n"
        "lretq\n"
        "1:\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        : : "r"(ptr) : "rax", "memory"
    );
}

void gdt_init(void) {
    load_gdt(&gdtp);
}