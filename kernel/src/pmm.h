#ifndef PMM_H
#define PMM_H

#include <stdint.h>

void pmm_init(void);
uint64_t pmm_alloc_frame(void);
void pmm_free_frame(uint64_t frame_addr);

#endif