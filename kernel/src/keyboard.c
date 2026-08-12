#include <stdint.h>
#include "io.h"
#include "keyboard.h"
#define BUFFER_SIZE 256

// Scancode Set 1 - تحويل مبدئي للحروف الأساسية بس (بدون shift, أرقام مطولة, إلخ)
static const char scancode_ascii[128] = {
    0,  0, '1','2','3','4','5','6','7','8','9','0','-','=', '\b', 0,
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',  0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
};

static volatile char buffer[BUFFER_SIZE];
static volatile uint32_t head = 0;
static volatile uint32_t tail = 0;

void click_on_keybord(void){
    uint8_t scancode = inb(0x60);

    if (scancode & 0x80){
        return;
    }
    if (scancode >= 128){
        return;
    }
    char c = scancode_ascii[scancode];
    if (c == 0){
        return;
    }

    uint32_t next_head = (head + 1) % BUFFER_SIZE;

    if (next_head == tail){
        return;
    }

    buffer[head] = c;
    head = next_head;
}

int read_kyboard_from_main(char *char_output_pointer){
    if (head == tail){
        return 0;
    }

    *char_output_pointer = buffer[tail];

    tail = (tail + 1) % BUFFER_SIZE;
    return 1;
}