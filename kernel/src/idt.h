#ifndef IDT_H
#define IDT_H

#include <stdint.h>

void idt_init(void);
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags);
void isr_handler(uint64_t int_num);
void irq_handler(uint64_t int_num);

#endif