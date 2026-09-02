#ifndef TASK_H
#define TASK_H
#include <stdint.h>

#define max_tasks 16

struct task {
    uint64_t rsp;
    uint64_t active;
};

extern struct task tasks[max_tasks];
extern uint64_t task_count;
extern uint64_t current_task_id;

extern void context_switch(uint64_t *old_rsp_ptr, uint64_t new_rsp);
void task_create(struct task *t, uint8_t *stack, uint64_t stack_size, void (*entry)(void));

#endif