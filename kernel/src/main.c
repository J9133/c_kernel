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
#include "somthings.h"
#include "messages.h"
#define MAX_COLS 256
#define MAX_ROWS 128
#define global_defualt_crus_x 4
#define FRAME_SIZE 4096

#define max_command_lenght_bytes 4096

uint64_t screen_cols;
uint64_t screen_rows;

uint64_t screen_width;
uint64_t screen_height;

uint64_t global_crus_x = global_defualt_crus_x;
uint64_t global_crus_y = 0;
uint64_t global_last_crus_x = 0;
uint64_t global_last_crus_y = 0;
struct limine_framebuffer *fb = 0;
char screen_buffer[MAX_COLS][MAX_ROWS];
char command_storage[max_command_lenght_bytes];
char *command = command_storage;
char *cmd_line_text;

uint64_t command_point_c_main = 0;

uint8_t *read_out;

struct task {
    uint64_t rsp;
};

extern void context_switch(uint64_t *old_rsp_ptr, uint64_t new_rsp);

uint8_t stack_a[4096];
uint8_t stack_b[4096];
struct task task_a, task_b;
struct task *current_task;

uint64_t main_not_found = 0-1;

// shell {

int max_lenght_command_f1 = 32;

uint64_t last_global_command_lenght;

uint64_t current_dir_id;

// shell }

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

__attribute__((aligned(16))) uint8_t stack_a[4096];
__attribute__((aligned(16))) uint8_t stack_b[4096];

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
    for(uint64_t x = 0; x < 50; x++)
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
        if (crus_x+7 > (screen_width/8)){
            crus_x = x;
            crus_y++;
        }
        put_char(fb, crus_x, crus_y, color, inputtext[i], color_none);
        screen_buffer[crus_y][crus_x] = inputtext[i];
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
                for (int64_t i23 = (screen_width/8)-1; i23>=(uint64_t)global_defualt_crus_x; i23--){
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
            if (global_crus_x+7 > (screen_width/8)){
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

// shell {

int enter(){
    int got_all_f = 0;
    uint64_t while_read_point = 0;
    int while0_finish = 0;
    int64_t command_lenght = 0;

    while(while0_finish == 0){
        if (command[command_lenght] == '\0'){
            command_lenght++;
            while0_finish = 1;
        }else{
            command_lenght++;
        }
    }

    char argv[command_lenght];
    uint64_t argc = 1;

    for(uint64_t i = 0; i < command_lenght; i++){
        if (command[i] == ' '){
            argv[i] = '\0';
            argc++;
        }else{
            argv[i] = command[i];
        }
    }

    last_global_command_lenght = command_lenght;

    char *argvs[argc];
    uint64_t idx = 0;
    int new_word = 1;

    for (uint64_t i = 0; i < command_lenght; i++){
        if (argv[i] == '\0'){
            new_word = 1;
        }else{
            if (new_word){
                argvs[idx] = &argv[i];
                idx++;
                new_word = 0;
            }
        }
    }
    
    if (idx == 0){
        return 0;
    }

    if (fs_strcmp(argvs[0], "ls", command_lenght)){
        for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
        int this_stat = 0;
        if (idx == 1){
            this_stat = fs_ls_dir(".", read_out, FRAME_SIZE, current_dir_id);
        }else{
            this_stat = fs_ls_dir(argvs[1], read_out, FRAME_SIZE, current_dir_id);
        }
        if (this_stat == 1){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, fs_not_found_message, sizeof(fs_not_found_message));
            return 3;
        }
        return 2;
    }
    else if (fs_strcmp(argvs[0], "mk", command_lenght)){
        int this_stat = 0;
        this_stat = fs_mk_file(argvs[1], current_dir_id);
        if (this_stat == 1){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, fs_not_found_message, sizeof(fs_not_found_message));
            return 3;
        }
        return 0;
    }
    else if (fs_strcmp(argvs[0], "mkdir", command_lenght)){
        int this_stat = 0;
        this_stat = fs_mk_dir(argvs[1], current_dir_id);
        if (this_stat == 1){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, fs_not_found_message, sizeof(fs_not_found_message));
            return 3;
        }
        return 0;
    }
    else if (fs_strcmp(argvs[0], "cat", command_lenght)){
        int this_stat = 0;
        for (uint64_t i = 0; i < FRAME_SIZE; i++) {read_out[i] = 0x00;}
        this_stat = fs_read_file(argvs[1], read_out, 0);
        if (this_stat == 1){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, fs_not_found_message, sizeof(fs_not_found_message));
            return 3;
        }
        return 2;
    }else if (fs_strcmp(argvs[0], "write", command_lenght)){
        int this_stat = 0;
        if (argc < 3){
            return 1;
        }
        char *content_start = argvs[2];
        uint64_t content_len = command_lenght - (content_start - argv);

        for (uint64_t i = 0; i < content_len - 1; i++){
            if (content_start[i] == '\0'){
                content_start[i] = ' ';
            }
        }
        this_stat = fs_write_file(argvs[1], content_start, 0);
        if (this_stat == 1){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, fs_not_found_message, sizeof(fs_not_found_message));
            return 3;
        }
        return 0;
    }
    else if (fs_strcmp(argvs[0], "cd", command_lenght)){
        uint64_t last_cdd = current_dir_id;
        current_dir_id = path_to_id(argvs[1], current_dir_id);
        if (current_dir_id == main_not_found){
            current_dir_id = last_cdd;
            return 3;
        }
        return 0;
    }
    else if (fs_strcmp(argvs[0], "rm", command_lenght)){
        int this_stat = 0;
        this_stat = fs_rm_file(argvs[1], current_dir_id);
        if (this_stat == 1){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, fs_not_found_message, sizeof(fs_not_found_message));
            return 3;
        }else if (this_stat == 2){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, main_not_file_message, sizeof(main_not_file_message));
            return 3;
        }
        return 0;
    }
    else if (fs_strcmp(argvs[0], "rmdir", command_lenght)){
        int this_stat = 0;
        this_stat = fs_rm_dir(argvs[1], current_dir_id);
        if (this_stat == 1){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, fs_not_found_message, sizeof(fs_not_found_message));
            return 3;
        }else if (this_stat == 2){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, main_not_folder_message, sizeof(main_not_folder_message));
            return 3;
        }
        return 0;
    }

    
    for(uint64_t i = 0; i < max_command_lenght_bytes; i++){
        command[i] = 0x00;
    }

    return 1;
}

int shell_command(const char *test_cmd){
    for(uint64_t i = 0; i < max_command_lenght_bytes; i++){
        command[i] = '\0';
    }
    uint64_t test_cmd_len = 0;

    while (test_cmd[test_cmd_len] != '\0'){
        test_cmd_len++;
    }

    //for (uint64_t i = 0; i < max_command_lenght_bytes; i++){
    //    if (test_cmd[i] != '\0'){
    //        test_cmd_len++;
    //    }
    //}
    for (uint64_t i = 0; i < test_cmd_len; i++){
        command[i] = test_cmd[i];
    }
    
    enter();

    for(uint64_t i = 0; i < max_command_lenght_bytes; i++){
        command[i] = 0x00;
    }
}

void shell(void){
    uint64_t x = 1;
    uint64_t y = 1;
    uint32_t color = 0xFFFFFFFF;
    uint32_t color_ar[3] = {0xFFFFFFFF, 0x0000000, '\0'};
    uint32_t color_ar_error[3] = {0xFFFF0000, 0x0000000, '\0'};
    
    for (;;) {
        char c;
        if (read_kyboard_from_main(&c)) {
            char str[2] = {c, '\0'};
            if (str[0] == '\n'){
                global_crus_y++;
                global_crus_x = global_defualt_crus_x;
                //put_char_on_crus(fb, color_ar, str, 0, 0);
                int enter_output = enter();
                for(uint64_t i8 = 0; i8 < last_global_command_lenght; i8++){
                    command[i8] = 0x00;
                }
                command_point_c_main = 0;
                if(enter_output == 2 || enter_output == 3 ){
                    uint64_t this_read_out_lenght = 0;
                    for(uint64_t iiro = 0; iiro<FRAME_SIZE; iiro++){
                        this_read_out_lenght++;
                        if (read_out[iiro] == '\0'){break;}
                    }
                    if(enter_output == 2){
                        write(fb, 0, global_crus_y, color_ar[0], color_ar[1], read_out);       
                    }else if (enter_output == 3){
                        write(fb, 0, global_crus_y, color_ar_error[0], color_ar_error[1], read_out);  
                    }
                    global_crus_y++;
                    global_crus_y+=(this_read_out_lenght + (screen_cols-1))/screen_cols;
                    global_crus_x = global_defualt_crus_x;
                    write(fb, 0, global_crus_y, color_ar[0], color_ar[1], cmd_line_text);
                }else{
                    write(fb, 0, global_crus_y, color_ar[0], color_ar[1], cmd_line_text);
                }
                put_crus(fb, color_ar[0], color_ar[1]);
                return;
            }else if (str[0] == '\b'){
                int mins_line = 0;
                if (!(global_crus_x == (uint64_t)global_defualt_crus_x && global_crus_y == 0)){
                    if (global_crus_x > global_defualt_crus_x){
                        global_crus_x--;
                    }else{
                        mins_line = 0;
                    }
                    put_char_on_crus(fb, color_ar, " ", 1, mins_line);
                    if (command_point_c_main > 0){
                        command_point_c_main--;
                        command[command_point_c_main] = 0x00;
                    }
                }
            }else{
                put_char_on_crus(fb, color_ar, str, 0, 0);
                if (command_point_c_main < max_command_lenght_bytes - 1){
                    command[command_point_c_main] = str[0];
                    command_point_c_main++;
                }
            }
            put_crus(fb, color_ar[0], color_ar[1]);
            debug_command();
        }
        __asm__("hlt");
    }
}

// shell }

void task_create(struct task *t, uint8_t *stack, uint64_t stack_size, void (*entry)(void)){
    uint64_t *sp = (uint64_t *)(stack + stack_size);

    sp = (uint64_t *)((uint64_t)sp & ~0xFULL);
    sp = (uint64_t *)((uint64_t)sp - 8);
    *(--sp) = (uint64_t)entry;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    t->rsp = (uint64_t)sp;
}
void task_a_func(void){
    for(int i = 0; i < 5; i++){
        debug_putc('A');
    }
    for(;;){
        debug_putc('A1');
        shell();
        current_task = &task_b;
        debug_putc('A2');
        context_switch(&task_a.rsp, task_b.rsp);
        __asm__ volatile ("hlt");
    }
}

void task_b_func(void){

    uint32_t color_ar[3] = {0xFFFFFFFF, 0x0000000, '\0'};
    for(int i = 0; i < 5; i++){
        debug_putc('B');
    }
    for(;;){
        uint64_t this_crus_x_b = global_crus_x;
        global_crus_x = global_crus_x*2;

        write(fb, global_crus_x, global_crus_y, color_ar[0], color_ar[1], cmd_line_text);
        debug_putc('B1');
        shell();
        global_crus_x = this_crus_x_b;
        current_task = &task_a;
        debug_putc('B2');
        context_switch(&task_b.rsp, task_a.rsp);
        __asm__ volatile ("hlt");
    }
}

void kmain(void) {
    gdt_init();
    idt_init();
    pic_remap();
    pmm_init();
    fs_init();
    fb = fb_request.response->framebuffers[0];

    uint64_t x = 1;
    uint64_t y = 1;
    uint32_t color = 0xFFFFFFFF;
    uint32_t color_ar[3] = {0xFFFFFFFF, 0x0000000, '\0'};
    
    screen_cols = (fb->width / 8);
    screen_rows = (fb->height /8);

    screen_height = (fb->height);
    screen_width = (fb->width);

    read_out = (uint8_t *)pmm_phys_to_virt(pmm_alloc_frame());

    for (uint64_t i = 0; i < FRAME_SIZE; i++){
        read_out[i] = 0x00;
    }

    __asm__ volatile ("sti");

    uint64_t cursor_x = 1;
    uint64_t cursor_y = 1;

    cmd_line_text = "hi> ";
    uint8_t test_buffer[10];
    write(fb, 0, global_crus_y, color_ar[0], color_ar[1], cmd_line_text);

    shell_command("mk /file31");
    //shell_command("write file31 hello in this world this sentens is the some sentens and it is for test my os or my kernel la2an ana 3am barmaj kernel min al 0     o hala2 baed hek bade jareb he  o kaman   o ba3ed hek heo hal she lazem eshtegel btw");
    shell_command("write file31 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95 96 97 98 99 100");

    shell_command("mkdir /home");
    shell_command("mkdir /home/jad");
    shell_command("mk /config.cfg");
    shell_command("mkdir /home/jad/doc");
    shell_command("mkdir /home/jad/downloads");
    shell_command("mk /home/jad/doc/file1.txt");
    shell_command("mkdir /home/jad/downloads/chthis_crus_x_byu");

    task_create(&task_a, stack_a, sizeof(stack_a), task_a_func);
    task_create(&task_b, stack_b, sizeof(stack_b), task_b_func);

    //uint64_t dummy_rsp;
    //context_switch(&dummy_rsp, task_a.rsp);

    for (;;){
        shell();
    }

    //for (;;) {
    //    char c;
    //    if (read_kyboard_from_main(&c)) {
    //        char str[2] = {c, '\0'};
    //        if (str[0] == '\n'){
    //            global_crus_y++;
    //            global_crus_x = global_defualt_crus_x;
    //            //put_char_on_crus(fb, color_ar, str, 0, 0);
    //            int enter_output = enter();
    //            for(uint64_t i8 = 0; i8 < last_global_command_lenght; i8++){
    //                command[i8] = 0x00;
    //            }
    //            command_point_c_main = 0;
    //            if(enter_output == 2){
    //                uint64_t this_read_out_lenght = 0;
    //                for(uint64_t iiro = 0; iiro<FRAME_SIZE; iiro++){
    //                    this_read_out_lenght++;
    //                    if (read_out[iiro] == '\0'){break;}
    //                }
    //                debug_print(read_out);
    //                write(fb, 0, global_crus_y, color_ar[0], color_ar[1], read_out);       
    //                //for(uint64_t iro = 0; iro<this_read_out_lenght; iro++){
    //                //    char temp_read_out_iro[2] = {read_out[iro], '\0'};
    //                //    write(fb, iro, global_crus_y, color_ar[0], color_ar[1], temp_read_out_iro);
    //                //}
    //                global_crus_y+=(this_read_out_lenght + (screen_cols-1))/screen_cols;
    //                global_crus_x = global_defualt_crus_x;
    //                write(fb, 0, global_crus_y, color_ar[0], color_ar[1], cmd_line_text);
    //            }else{
    //                write(fb, 0, global_crus_y, color_ar[0], color_ar[1], cmd_line_text);
    //            }
    //        }else if (str[0] == '\b'){
    //            int mins_line = 0;
    //            if (!(global_crus_x == (uint64_t)global_defualt_crus_x && global_crus_y == 0)){
    //                if (global_crus_x > global_defualt_crus_x){
    //                    global_crus_x--;
    //                }else{
    //                    mins_line = 0;
    //                }
    //                put_char_on_crus(fb, color_ar, " ", 1, mins_line);
    //                if (command_point_c_main > 0){
    //                    command_point_c_main--;
    //                    command[command_point_c_main] = 0x00;
    //                }
    //            }
    //        }else{
    //            put_char_on_crus(fb, color_ar, str, 0, 0);
    //            if (command_point_c_main < max_command_lenght_bytes - 1){
    //                command[command_point_c_main] = str[0];
    //                command_point_c_main++;
    //            }
    //        }
    //        put_crus(fb, color_ar[0], color_ar[1]);
    //        debug_command();
    //    }
    //    __asm__("hlt");
    //}
}
