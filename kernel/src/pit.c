#include <stdint.h>
#include "pit.h"
#include "io.h"
#include "task.h"
#include "pic.h"
#include "debug.h"

#define PIT_CHANNEL0_PORT 0x40
#define PIT_COMMAND_PORT  0x43
#define PIT_BASE_FREQUENCY 1193182

static volatile uint64_t tick_count = 0;

extern struct task task_a, task_b;
extern struct task *current_task;

void pit_init(uint32_t frequency_hz){
    uint32_t divisor = PIT_BASE_FREQUENCY / frequency_hz;

    outb(PIT_COMMAND_PORT, 0x36);
    outb(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8) & 0xFF));
}

uint64_t pit_get_ticks(void){
    return tick_count;
}

uint64_t schedule(uint64_t current_rsp){
    if (task_count == 0) return current_rsp;
    tick_count++;
    pic_send_eoi(0);

    tasks[current_task_id].rsp = current_rsp;

    do {
        current_task_id = (current_task_id + 1) % task_count;
    } while (!tasks[current_task_id].active);

    return tasks[current_task_id].rsp;
}

void sleep_ticks(uint64_t ticks){
    uint64_t start = pit_get_ticks();
    int64_t target = start + ticks;

    while (pit_get_ticks() < target){
        __asm__ volatile ("hlt");
    }
}

void sleep_ms(uint64_t ms){
    uint64_t ticks = (ms * 100) / 1000;
    sleep_ticks(ticks);
}