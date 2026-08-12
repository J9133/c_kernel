#ifndef DEBUG_H
#define DEBUG_H

#include "io.h"

static inline void debug_putc(char c){
    outb(0xE9, c);
}
static inline void debug_put64(uint64_t n) {
    char buf[20];
    uint64_t i = 0;

    if (n == 0) {
        debug_putc('0');
        return;
    }

    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }

    while (i > 0) {
        debug_putc(buf[--i]);
    }
}


static inline void debug_print(char *str){
    while(*str)
    {
        debug_putc(*str);
        str++;
    }
}

#endif