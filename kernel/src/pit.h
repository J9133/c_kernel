#ifndef PIT_H
#define PIT_H

#include <stdint.h>

void pit_init(uint32_t frequency_hz);
uint64_t pit_get_ticks(void);
uint64_t schedule(uint64_t current_rsp);
void sleep_ticks(uint64_t ticks);
void sleep_ms(uint64_t ms);

#endif