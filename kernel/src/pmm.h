#ifndef PMM_H
#define PMM_H

#include <stdint.h>

uint64_t pmm_phys_to_virt(uint64_t phys);

void pmm_init(void);
uint64_t pmm_alloc_frame(void);
void pmm_free_frame(uint64_t frame_addr);

#endif