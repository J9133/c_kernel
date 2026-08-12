#include <stdint.h>
#include <limine.h>
#include "font8x8_basic.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "io.h"
#include "keyboard.h"
#include "debug.h"
#include "pmm.h"
#include "fs.h"
#define MAX_COLS 256
#define MAX_ROWS 128
#define global_defualt_crus_x 4
#define FRAME_SIZE 4096

uint64_t global_crus_x = global_defualt_crus_x;
uint64_t global_crus_y = 0;
uint64_t global_last_crus_x = 0;
uint64_t global_last_crus_y = 1;
struct limine_framebuffer *fb = 0;
char screen_buffer[MAX_COLS][MAX_ROWS];
char *command;
char *cmd_line_text;

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

extern void asm_main(uint64_t fb_addr, uint64_t pitch, uint64_t width, uint64_t height);

void put_pixel(struct limine_framebuffer *fb, uint64_t x, uint64_t y, uint32_t color){
    uint32_t *start_screen = (uint32_t *)fb->address;
    uint64_t line_pixels = fb->pitch / 4;
    start_screen[y * line_pixels + x] = color;
}

void put_char(struct limine_framebuffer *fb, uint64_t x, uint64_t y, uint32_t color, char tarc, uint32_t color_none){
    uint8_t *target_char = (uint8_t *)font8x8_basic[(int)tarc];
    for (uint64_t row=0; row<8; row++){
        for (uint64_t bit=0; bit<8; bit++){
            if ((target_char[row] >> bit) & 1){
                put_pixel(fb, (x*8)+bit, (y*8)+row, color);
            }else{
                put_pixel(fb, (x*8)+bit, (y*8)+row, color_none);
            }
        }
    }
}

void debug_screen_buffer_small()
{
    for(uint64_t y = 0; y < 20; y++)
    {
        for(uint64_t x = 0; x < 80; x++)
        {
            char c = screen_buffer[y][x];

            if(c == '\0')
                debug_print("'\\0'");
            else if(c == '\n')
                debug_print("'\\n'");
            else if(c == ' ')
                debug_print("'\\s'");
            else
                debug_putc(c);

            debug_putc(' ');
        }

        debug_putc('\n');
    }
}

void debug_command()
{
    for(uint64_t x = 0; x < 20; x++)
    {
        char c = command[x];

        if(c == '\0')
            debug_print("'\\0'");
        else if(c == '\n')
            debug_print("'\\n'");
        else if(c == ' ')
            debug_print("'\\s'");
        else
            debug_putc(c);

        debug_putc(' ');
    }
    debug_putc('\n');
}
void reput_char(struct limine_framebuffer *fb, uint32_t color, uint32_t color_none, uint64_t x, uint64_t y){
    char tc = screen_buffer[y][x];

    if (tc == '\0' || tc == ' ' || tc == '\n'){
        for (uint64_t row = 0; row < 8; row++){
            for (uint64_t bit = 0; bit < 8; bit++){
                put_pixel(fb, (x*8)+bit, (y*8)+row, color_none);
            }
        }
        return;
    }

    put_char(fb, x, y, color, tc, color_none);
}

void put_crus(struct limine_framebuffer *fb, uint32_t color, uint32_t color_none){
    reput_char(fb, color, color_none, global_last_crus_x, global_last_crus_y);
    char tc = screen_buffer[global_crus_y][global_crus_x];
    uint8_t *target_char = font8x8_basic[(int)tc];

    if (tc == '\0' || tc == ' ' || tc == '\n'){
        for (uint64_t row = 0; row < 8; row++){
            for (uint64_t bit = 0; bit < 8; bit++){
                put_pixel(fb, (global_crus_x*8)+bit, (global_crus_y*8)+row, color);
            }
        }
        global_last_crus_x = global_crus_x;
        global_last_crus_y = global_crus_y;
        return;
    }

    for (uint64_t row=0; row<8; row++){
        for (uint64_t bit=0; bit<8; bit++){
            if ((target_char[row] >> bit) & 1){
                put_pixel(fb, (global_crus_x*8)+bit, (global_crus_y*8)+row, color_none);
            }else{
                put_pixel(fb, (global_crus_x*8)+bit, (global_crus_y*8)+row, color);
            }
        }
    }
    global_last_crus_x = global_crus_x;
    global_last_crus_y = global_crus_y;
}

void write(struct limine_framebuffer *fb, uint64_t x, uint64_t y, uint32_t color, uint32_t color_none, char *inputtext){
    uint32_t inputtext_lenght = 0;
    while(inputtext[inputtext_lenght] != '\0'){
        inputtext_lenght++;
    }

    uint64_t crus_x = x;
    uint64_t crus_y = y;

    for (uint32_t i=0; i < inputtext_lenght; i++){
        if (crus_x+7 > (fb->width/8)){
            crus_x = x;
            crus_y++;
        }
        put_char(fb, crus_x, crus_y, color, inputtext[i], color_none);
        crus_x++;
    }
}

void put_char_on_crus(struct limine_framebuffer *fb, uint32_t *color, char *inputtext, int baskspace_state, int mins_line){
    uint32_t inputtext_lenght = 0;
    while(inputtext[inputtext_lenght] != '\0'){
        inputtext_lenght++;
    }

    for (uint32_t i=0; i < inputtext_lenght; i++){
        
        if (baskspace_state == 1){
            if (mins_line == 1){
                put_char(fb, global_crus_x, global_crus_y, color[0], '\0', color[1]);
                screen_buffer[global_crus_y][global_crus_x] = '\0';
                //command[global_crus_x] = '\0';
                global_crus_y--;
                int T = 0;
                for (int64_t i23 = (fb->width/8)-1; i23>=(uint64_t)global_defualt_crus_x; i23--){
                    char screchar = screen_buffer[global_crus_y][i23];
                    if (screchar != '\0'){
                        global_crus_x = i23 +1;
                        screen_buffer[global_crus_y][global_crus_x] = '\0';
                        T = 1;
                        break;
                    }
                }
                if (T == 0){
                    global_crus_x = global_defualt_crus_x;
                }
            }else{
                put_char(fb, global_crus_x, global_crus_y, color[0], '\0', color[1]);
                screen_buffer[global_crus_y][global_crus_x] = '\0';
                //command[global_crus_x] = '\0';
            }
        }else{
            if (global_crus_x+7 > (fb->width/8)){
                global_crus_x = global_defualt_crus_x;
                global_crus_y++;
            }
            put_char(fb, global_crus_x, global_crus_y, color[0], inputtext[i], color[1]);
            screen_buffer[global_crus_y][global_crus_x] = inputtext[i];
            //command[global_crus_x] = inputtext[i];
            global_crus_x++;
        }
    }
}

void draw_rect(struct limine_framebuffer *fb, uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t color) {
    uint32_t *start_screen = (uint32_t *)fb->address;
    uint64_t line_pixels = fb->pitch / 4;
    uint32_t *start_rect = (start_screen + line_pixels * y + x);

    for (uint32_t rect_y = 0; rect_y<height; rect_y++){
        for (uint32_t rect_x = 0; rect_x<width; rect_x++){
            start_rect[rect_y * line_pixels + rect_x] = color;
        }
    }
}

void kmain(void) {
    gdt_init();
    idt_init();
    pic_remap();
    pmm_init();
    fb = fb_request.response->framebuffers[0];

    //char screen_buffer[fb->width/8][fb->height/8];

    uint64_t x = 1;
    uint64_t y = 1;
    uint32_t color = 0xFFFFFFFF;
    uint32_t color_ar[3] = {0xFFFFFFFF, 0x0000000, '\0'};
    

    __asm__ volatile ("sti");

    uint64_t cursor_x = 1;
    uint64_t cursor_y = 1;

    uint64_t f1 = pmm_alloc_frame();
    uint64_t f2 = pmm_alloc_frame();
    uint64_t f3 = pmm_alloc_frame();

    if (f2 - f1 == FRAME_SIZE && f3 - f2 == FRAME_SIZE){
        debug_putc('O'); // "OK" - الفرق بينهم بالضبط 4096، يعني frames متتالية صح
    }else{
        debug_putc('X'); // في مشكلة
    }

    pmm_free_frame(f2);
    uint64_t f4 = pmm_alloc_frame();

    if (f4 == f2){
        debug_putc('R'); // "Reused" - صح، رجع نفس العنوان يلي حررناه
    }else{
        debug_putc('N'); // في مشكلة بمنطق البحث عن أول فاضي
    }

    cmd_line_text = "hi> ";
    write(fb, 0, global_crus_y, color_ar[0], color_ar[1], cmd_line_text);
    for (;;) {
        char c;
        if (read_kyboard_from_main(&c)) {
            char str[2] = {c, '\0'};
            if (str[0] == '\n'){
                global_crus_y++;
                global_crus_x = global_defualt_crus_x;
                //put_char_on_crus(fb, color_ar, str, 0, 0);
                write(fb, 0, global_crus_y, color_ar[0], color_ar[1], cmd_line_text);
            }else if (str[0] == '\b'){
                int mins_line = 0;
                if (!(global_crus_x == (uint64_t)global_defualt_crus_x && global_crus_y == 0)){
                    if (global_crus_x > global_defualt_crus_x){
                        global_crus_x--;
                    }else{
                        mins_line = 0;
                    }
                    put_char_on_crus(fb, color_ar, " ", 1, mins_line);
                }
            }else{
                put_char_on_crus(fb, color_ar, str, 0, 0);
            }
            put_crus(fb, color_ar[0], color_ar[1]);
            //debug_screen_buffer_small();
            command = screen_buffer[global_crus_y];
            //debug_command();
        }
        __asm__("hlt");
    }
}
