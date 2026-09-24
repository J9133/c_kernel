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
#include "pit.h"
#include "task.h"
#include "ata.h"
#define MAX_COLS 256
#define MAX_ROWS 128
#define FRAME_SIZE 4096

#define max_command_lenght_bytes 4096

uint64_t screen_cols;
uint64_t screen_rows;

uint64_t screen_width;
uint64_t screen_height;

uint64_t global_defualt_crus_x = 0;
uint64_t global_crus_x = 0;
uint64_t global_crus_y = 0;
uint64_t global_last_crus_x = 0;
uint64_t global_last_crus_y = 0;
struct limine_framebuffer *fb = 0;
char screen_buffer[MAX_COLS][MAX_ROWS];
char command_storage[max_command_lenght_bytes];
char *command = command_storage;
char cmd_line_text[1024];

uint64_t command_point_c_main = 0;

uint8_t *read_out;

int last_if_stat = 0;

extern void context_switch(uint64_t *old_rsp_ptr, uint64_t new_rsp);

uint64_t main_not_found = 0-1;
uint64_t rng_state = 88172645463325252ULL;

// shell {

uint32_t color_ar[3] = {0xFFFFFFFF, 0xFF000000, '\0'};
uint32_t color_ar_error[3] = {0xFFFF0000, 0xFF000000, '\0'};

int max_lenght_command_f1 = 32;

uint64_t last_global_command_lenght;

uint64_t current_dir_id;

uint64_t task_b_counter = 0;

char get_input_c_enter_char;

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

extern void asm_main(uint64_t fb_addr, uint64_t pitch, uint64_t width, uint64_t height);

void plus_gc_y(struct limine_framebuffer *fb, uint32_t color, uint32_t color_none);
void scroll_screen(struct limine_framebuffer *fb, uint32_t color, uint32_t color_none);
int shell_command(const char *test_cmd);
void recal_clt(void);
void delet_task(char *name);
void run_task(void);
void creat_task_path(char *run_path, char *name);
void creat_task_func(void (*entry)(void), char *name);
void make_the_tree(void);

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
    for(uint64_t y = 0; y < 80; y++)
    {
        for(uint64_t x = 0; x < 20; x++)
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

uint64_t write(struct limine_framebuffer *fb, uint64_t x, uint64_t y, uint32_t color, uint32_t color_none, char *inputtext){
    uint32_t inputtext_lenght = 0;
    while(inputtext[inputtext_lenght] != '\0'){
        inputtext_lenght++;
    }

    uint64_t crus_x = x;
    uint64_t crus_y = y;

    if (crus_y > screen_rows - 1){
        crus_y = screen_rows - 1;
    }

    for (uint32_t i=0; i < inputtext_lenght; i++){
        if (crus_x+7 > (screen_width/8)){
            crus_x = x;
            if (crus_y < screen_rows - 1){
                crus_y++;
            }else{
                scroll_screen(fb, color, color_none);
            }
        }
        put_char(fb, crus_x, crus_y, color, inputtext[i], color_none);
        screen_buffer[crus_y][crus_x] = inputtext[i];
        crus_x++;
    }

    return crus_y;
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
            }
        }else{
            if (global_crus_x+7 > (screen_width/8)){
                global_crus_x = global_defualt_crus_x;
                plus_gc_y(fb, color[0], color[1]);
            }
            put_char(fb, global_crus_x, global_crus_y, color[0], inputtext[i], color[1]);
            screen_buffer[global_crus_y][global_crus_x] = inputtext[i];
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

uint64_t str_to_u64(const char *str) {
    uint64_t result = 0;
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result;
}


int is_valid_number(const char *str){
    if (str[0] == '\0'){
        return 0;
    }
    for (uint64_t i = 0; str[i] != '\0'; i++){
        if (str[i] < '0' || str[i] > '9'){
            return 0;
        }
    }
    return 1;
}

char *u64_to_str(uint64_t num) {
    static char str[21];
    char temp[21];
    int i = 0;
    int j = 0;

    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return str;
    }

    while (num > 0) {
        temp[i] = (num % 10) + '0';
        num /= 10;
        i++;
    }

    while (i > 0) {
        str[j] = temp[i - 1];
        i--;
        j++;
    }

    str[j] = '\0';
    return str;
}

char *str_concat(const char *s1, const char *s2){
    static char dst[4][1024];
    static int slot = 0;

    char *out = dst[slot];
    slot = (slot + 1) % 4;

    uint64_t i = 0;
    uint64_t j = 0;

    while (s1[i] != '\0'){
        out[j++] = s1[i++];
    }

    i = 0;
    while (s2[i] != '\0'){
        out[j++] = s2[i++];
    }

    out[j] = '\0';
    return out;
}

uint64_t simple_rand(void){
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

// shell {

void re_drw_screen(void){

    global_defualt_crus_x = 0;
    global_crus_x = 0;
    global_crus_y = 0;
    global_last_crus_x = 0;
    global_last_crus_y = 0;

    for (uint64_t yF = 0; yF < screen_rows; yF++){
        for (uint64_t xF = 0; xF < screen_cols; xF++){
            write(fb, xF, yF, color_ar[0], color_ar[1], " ");
            screen_buffer[yF][xF] = 0x00;
        }
    }

    recal_clt();
    global_crus_x = global_defualt_crus_x;
    global_crus_y;
    write(fb, 0, global_crus_y-1, color_ar[0], color_ar[1], cmd_line_text);
}

char get_input_c(void){
    int gic_T = 0;
    char c;

    while (gic_T == 0) {
        if (read_kyboard_from_main(&c)) {
            gic_T = 1;
        }
        __asm__("hlt");
    }
    return c;
}

int enter(const char *cmd){
    int got_all_f = 0;
    uint64_t while_read_point = 0;
    int while0_finish = 0;
    int64_t command_lenght = 0;

    while(while0_finish == 0){
        if (cmd[command_lenght] == '\0'){
            command_lenght++;
            while0_finish = 1;
        }else{
            command_lenght++;
        }
    }

    char argv[command_lenght];
    uint64_t argc = 1;

    for(uint64_t i = 0; i < command_lenght; i++){
        if (cmd[i] == ' '){
            argv[i] = '\0';
            argc++;
        }else{
            argv[i] = cmd[i];
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
            this_stat = fs_ls_dir(path_resolve(path_to_abs(argvs[1], current_dir_id)), read_out, FRAME_SIZE, current_dir_id);
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
        this_stat = fs_mk_file(path_resolve(path_to_abs(argvs[1], current_dir_id)), current_dir_id);
        if (this_stat == 1){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, fs_not_found_message, sizeof(fs_not_found_message));
            return 3;
        }
        return 0;
    }
    else if (fs_strcmp(argvs[0], "mkdir", command_lenght)){
        int this_stat = 0;
        this_stat = fs_mk_dir(path_resolve(path_to_abs(argvs[1], current_dir_id)), current_dir_id);
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
        this_stat = fs_read_file(path_resolve(path_to_abs(argvs[1], current_dir_id)), read_out, 0, current_dir_id);
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
        if (idx == 4){
            if (fs_strcmp(argvs[3], "f", 2)){
                char cp_src[1024];
                char cp_dst[1024];
                fs_strncpy(cp_src, path_resolve(path_to_abs(argvs[1], current_dir_id)), 1024);
                fs_strncpy(cp_dst, path_resolve(path_to_abs(argvs[2], current_dir_id)), 1024);
                this_stat = fs_copy_file(cp_src, cp_dst, current_dir_id);
                if (this_stat == 1){
                    for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
                    fs_strncpy((char *)read_out, fs_not_found_message, sizeof(fs_not_found_message));
                    return 3;
                }
                return 0;
            }
        }
        char *content_start = argvs[2];
        uint64_t content_len = command_lenght - (content_start - argv);

        for (uint64_t i = 0; i < content_len - 1; i++){
            if (content_start[i] == '\0'){
                content_start[i] = ' ';
            }
        }
        this_stat = fs_write_file(path_resolve(path_to_abs(argvs[1], current_dir_id)), content_start, 0, current_dir_id);
        if (this_stat == 1){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, fs_not_found_message, sizeof(fs_not_found_message));
            return 3;
        }
        return 0;
    }else if (fs_strcmp(argvs[0], "wadd", command_lenght)){
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
        
        char this_file_read[FRAME_SIZE];
        for (uint64_t ipth = 0; ipth < 1024; ipth++){this_file_read[ipth] = '\0';}
        fs_read_file(path_resolve(path_to_abs(argvs[1], current_dir_id)), this_file_read, FRAME_SIZE, current_dir_id);
        int T = 0;
        uint64_t this_file_read_poitn = 0;
        while (T == 0){
            if(this_file_read[this_file_read_poitn] == '\0'){
                T = 1;
            }else{
                this_file_read_poitn++;
            }
        }

        this_stat = fs_write_file(path_resolve(path_to_abs(argvs[1], current_dir_id)), str_concat(str_concat(this_file_read, content_start), "\n"), 0, current_dir_id);
        if (this_stat == 1){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, fs_not_found_message, sizeof(fs_not_found_message));
            return 3;
        }
        return 0;
    }
    else if (fs_strcmp(argvs[0], "cd", command_lenght)){
        uint64_t last_cdd = current_dir_id;
        debug_print(path_resolve(path_to_abs(argvs[1], current_dir_id)));
        debug_put64(path_to_id(path_resolve(path_to_abs(argvs[1], current_dir_id)), current_dir_id));
        current_dir_id = path_to_id(path_resolve(path_to_abs(argvs[1], current_dir_id)), current_dir_id);
        if (current_dir_id == main_not_found){
            debug_print("fijufj");
            current_dir_id = last_cdd;
            return 3;
        }
        return 0;
    }
    else if (fs_strcmp(argvs[0], "rm", command_lenght)){
        int this_stat = 0;
        this_stat = fs_rm_file(path_resolve(path_to_abs(argvs[1], current_dir_id)), current_dir_id);
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
        this_stat = fs_rm_dir(path_resolve(path_to_abs(argvs[1], current_dir_id)), current_dir_id);
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
    else if (fs_strcmp(argvs[0], "pwd", command_lenght)){
        int this_stat = 0;
        this_stat = id_to_path(current_dir_id, read_out);
        if (this_stat == 1){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, fs_not_found_message, sizeof(fs_not_found_message));
            return 3;
        }else if (this_stat == 2){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, main_not_folder_message, sizeof(main_not_folder_message));
            return 3;
        }
        return 2;
    }
    else if (fs_strcmp(argvs[0], "sw", command_lenght)){
        int this_stat = 0;
        int this_f = 0;
        uint32_t this_color_a = color_ar[0];
        uint32_t this_color_b = color_ar[1];
        if (idx > 3){
            if (fs_strcmp(argvs[3], "f", 1)){
                this_f = 1;
            }
        }
        if (idx > 5){
            if (fs_strcmp(argvs[4], "c", 1)){
                if (fs_strcmp(argvs[5], "0", 1)){
                    this_color_b = 0xFF000000;
                }else if (fs_strcmp(argvs[5], "1", 1)){
                    this_color_b = 0xFFFFFFFF;
                }else if (fs_strcmp(argvs[5], "2", 1)){
                    this_color_b = 0xFF0000FF;
                }else if (fs_strcmp(argvs[5], "3", 1)){
                    this_color_b = 0xFF00FF00;
                }else if (fs_strcmp(argvs[5], "4", 1)){
                    this_color_b = 0xFFFF0000;
                }else if (fs_strcmp(argvs[5], "5", 1)){
                    this_color_b = 0xFFFFFF00;
                }else if (fs_strcmp(argvs[5], "6", 1)){
                    this_color_b = 0xFFFF00FF;
                }
            }
        }
        if (this_f == 0){
            write(fb, str_to_u64(argvs[1]), str_to_u64(argvs[2]), this_color_a, this_color_b, " ");
        }else{
            int this_stat1 = 0;
            int this_stat2 = 0;
            uint64_t this_if_enter_cheak_size = 4;
            uint8_t this_if_enter_buffer_1[this_if_enter_cheak_size];
            uint8_t this_if_enter_buffer_2[this_if_enter_cheak_size];
            for (uint64_t i = 0; i < this_if_enter_cheak_size; i++) this_if_enter_buffer_1[i] = 0x00;
            for (uint64_t i = 0; i < this_if_enter_cheak_size; i++) this_if_enter_buffer_2[i] = 0x00;
            this_stat1 = fs_read_file(path_resolve(path_to_abs(argvs[1], current_dir_id)), this_if_enter_buffer_1, this_if_enter_cheak_size, current_dir_id);
            this_stat2 = fs_read_file(path_resolve(path_to_abs(argvs[2], current_dir_id)), this_if_enter_buffer_2, this_if_enter_cheak_size, current_dir_id);
            if (this_stat1 == 1 || this_stat2 == 1){
                return 1;
            }
            write(fb, str_to_u64(this_if_enter_buffer_1), str_to_u64(this_if_enter_buffer_2), this_color_a, this_color_b, " ");
        }
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
    else if (fs_strcmp(argvs[0], "sh", command_lenght)){
        int this_stat = 0;
        char this_read_out[1024];
        for (uint64_t ipth = 0; ipth < 1024; ipth++){this_read_out[ipth] = '\0';}
        fs_read_file(path_resolve(path_to_abs(argvs[1], current_dir_id)), this_read_out, 0, current_dir_id);
        char sh_command[1024];
        for (uint64_t ipth = 0; ipth < 1024; ipth++){sh_command[ipth] = '\0';}
        uint64_t i2 = 0;
        for (uint64_t i = 0; i < 1024; i++){
            if (this_read_out[i] == '\0') break;
            
            if (this_read_out[i] != '\n'){
                sh_command[i2++] = this_read_out[i];
            }else{
                sh_command[i2] = '\0';
                shell_command(sh_command);
                for (uint64_t ipth = 0; ipth < 1024; ipth++){sh_command[ipth] = '\0';}
                i2 = 0;
            }
        }
        if (i2 > 0){
            sh_command[i2] = '\0';
            shell_command(sh_command);
        }
        return 0;
        if (this_stat == 1){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, fs_not_found_message, sizeof(fs_not_found_message));
            return 3;
        }else if (this_stat == 2){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, main_not_folder_message, sizeof(main_not_folder_message));
            return 3;
        }
        return 2;
    }
    else if (fs_strcmp(argvs[0], "clear", command_lenght)){
        re_drw_screen();
        return 0;
    }
    else if (fs_strcmp(argvs[0], "if", command_lenght)){
        int this_stat1 = 0;
        int this_stat2 = 0;
        uint64_t this_if_enter_cheak_size = 1024;
        uint8_t this_if_enter_buffer_1[this_if_enter_cheak_size];
        uint8_t this_if_enter_buffer_2[this_if_enter_cheak_size];
        for (uint64_t i = 0; i < this_if_enter_cheak_size; i++) this_if_enter_buffer_1[i] = 0x00;
        for (uint64_t i = 0; i < this_if_enter_cheak_size; i++) this_if_enter_buffer_2[i] = 0x00;
        this_stat1 = fs_read_file(path_resolve(path_to_abs(argvs[1], current_dir_id)), this_if_enter_buffer_1, this_if_enter_cheak_size, current_dir_id);
        this_stat2 = fs_read_file(path_resolve(path_to_abs(argvs[2], current_dir_id)), this_if_enter_buffer_2, this_if_enter_cheak_size, current_dir_id);
        if (this_stat1 == 1 || this_stat2 == 1){
            for (uint64_t i = 0; i < FRAME_SIZE; i++) read_out[i] = 0x00;
            fs_strncpy((char *)read_out, fs_not_found_message, sizeof(fs_not_found_message));
            return 3;
        }
        last_if_stat = 1;
        for (uint64_t i = 0; i < this_if_enter_cheak_size; i++){
            if (this_if_enter_buffer_1[i] != this_if_enter_buffer_2[i]){
                last_if_stat = 0;
            }
        }
        return 0;
    }
    else if (fs_strcmp(argvs[0], "fi", command_lenght)){
        if (last_if_stat == 0){
            return 4;
        }
        char this_fi_command[1024];
        uint64_t this_fi_command_point = 0;
        for (uint64_t i = 1; i < idx; i++){
            if (i > 1){
                this_fi_command[this_fi_command_point] = ' ';
                this_fi_command_point++;
            }
            int T = 0;
            uint64_t this_fi_for_idi_lenhgt = 0;
            while (T == 0){
                if (argvs[i][this_fi_for_idi_lenhgt] == '\0'){
                    T = 1;
                }else{
                    this_fi_for_idi_lenhgt++;
                }
            }
            for (uint64_t i2 = 0; i2 < this_fi_for_idi_lenhgt; i2++){
                this_fi_command[this_fi_command_point] = argvs[i][i2];
                this_fi_command_point++;
            }
        }
        shell_command(this_fi_command);
        return 0;
    }
    else if (fs_strcmp(argvs[0], "else", command_lenght)){
        if (last_if_stat == 1){
            return 4;
        }
        char this_else_command[1024];
        uint64_t this_else_command_point = 0;
        for (uint64_t i = 1; i < idx; i++){
            if (i > 1){
                this_else_command[this_else_command_point] = ' ';
                this_else_command_point++;
            }
            int T = 0;
            uint64_t this_else_for_idi_lenhgt = 0;
            while (T == 0){
                if (argvs[i][this_else_for_idi_lenhgt] == '\0'){
                    T = 1;
                }else{
                    this_else_for_idi_lenhgt++;
                }
            }
            for (uint64_t i2 = 0; i2 < this_else_for_idi_lenhgt; i2++){
                this_else_command[this_else_command_point] = argvs[i][i2];
                this_else_command_point++;
            }
        }
        shell_command(this_else_command);
        return 0;
    }
    else if (fs_strcmp(argvs[0], "echo", command_lenght)){
        for (uint64_t ipth = 0; ipth < 1024; ipth++){read_out[ipth] = '\0';}
        uint64_t read_out_point = 0;
        for (uint64_t i = 1; i < idx; i++){
            if (i > 1){
                read_out[read_out_point] = ' ';
                read_out_point++;
            }
            int T = 0;
            uint64_t this_echo_for_idi_lenhgt = 0;
            while (T == 0){
                if (argvs[i][this_echo_for_idi_lenhgt] == '\0'){
                    T = 1;
                }else{
                    this_echo_for_idi_lenhgt++;
                }
            }
            for (uint64_t i2 = 0; i2 < this_echo_for_idi_lenhgt; i2++){
                read_out[read_out_point] = argvs[i][i2];
                read_out_point++;
            }
        }
        return 2;
    }
    else if (fs_strcmp(argvs[0], "input", command_lenght)){
        char this_c = get_input_c();
        char this_arr[2] = {this_c, '\0'};
        fs_write_file("/input/input_c", this_arr, 0, current_dir_id);
        return 0;
    }
    else if (fs_strcmp(argvs[0], "sleep", command_lenght)){
        uint64_t delay_ms_argvs1 = str_to_u64(argvs[1]);
        sleep_ms(delay_ms_argvs1);
        return 0;
    }
    else if (fs_strcmp(argvs[0], "for", command_lenght)){
        uint64_t for_range = str_to_u64(argvs[1]);
        enter(str_concat("mk /proc/for/", argvs[3]));
        for (uint64_t i = 0; i < for_range; i++){
            char this_this[1024];
            this_this[0] = 's';
            this_this[1] = 'h';
            this_this[2] = ' ';
            
            uint64_t i2 = 3;
            uint64_t src = 0;
            while (argvs[2][src] != '\0' && i2 < 1023){
                this_this[i2] = argvs[2][src];
                i2++;
                src++;
            }
            this_this[i2] = '\0'; 
            shell_command(str_concat(str_concat(str_concat("write /proc/for/", argvs[3]), " "), u64_to_str(i)));
            shell_command(this_this);
        }
        return 0;
    }
    else if (fs_strcmp(argvs[0], "rand", command_lenght)){
        uint64_t rand_num = simple_rand();
        shell_command(str_concat("write /var/rand ", u64_to_str(rand_num)));
        if (fs_strcmp(argvs[1], "o", 1)){
            fs_strncpy(read_out, u64_to_str(rand_num), 10);
            return 2;
        }else{
            if (is_valid_number(argvs[1]) == 1){
                shell_command(str_concat("write /var/rand_2 ", u64_to_str(rand_num % str_to_u64(argvs[1]))));
            }
            return 0;
        }
    }
    else if (fs_strcmp(argvs[0], "task", command_lenght)){
        for (uint64_t ipth = 0; ipth < 1024; ipth++){read_out[ipth] = '\0';}
        if (fs_strcmp(argvs[1], "list", command_lenght)){
            uint64_t read_out_point = 0;
            for (uint64_t i = 0; i < task_count; i++){
                if (tasks[i].active != 0){
                    int T = 0;
                    uint64_t tast_list_ro_pointer = 0;
                    while (T == 0){
                        if (tasks[i].name[tast_list_ro_pointer] != '\0'){
                            read_out[read_out_point] = tasks[i].name[tast_list_ro_pointer];
                            tast_list_ro_pointer++;
                            read_out_point++;
                        }else{
                            read_out[read_out_point] = '\n';
                            read_out_point++;
                            T = 1;
                        }
                    }
                }
            }
            return 2;
        }else if (fs_strcmp(argvs[1], "del", command_lenght)){
            char *content_start = argvs[2];
            uint64_t content_len = command_lenght - (content_start - argv);

            for (uint64_t i = 0; i < content_len - 1; i++){
                if (content_start[i] == '\0'){
                    content_start[i] = ' ';
                }
            }
            delet_task(content_start);
        }else if (fs_strcmp(argvs[1], "create", command_lenght)){
            creat_task_path(argvs[2], argvs[3]);
        }
        return 1;
    }
    else if (fs_strcmp(argvs[0], "se", command_lenght)){
        int this_stat = 0;
        int this_f = 0;
        int this_ff = 0;
        if (idx > 4){
            if (fs_strcmp(argvs[4], "fi", 2)){
                this_f = 1;
                this_ff = 1;
            }else if (fs_strcmp(argvs[4], "f", 1)){
                this_f = 1;
            }else if (fs_strcmp(argvs[4], "i", 2)){
                this_ff = 1;
            }
        }
        char argvs3_path[1024];

        int T = 0;
        uint64_t argvs3_lenght = 0;
        while (T == 0){
            if (argvs[3][argvs3_lenght] == '\0'){
                T = 1;
                argvs3_lenght++;
            }else{
                argvs3_lenght++;
            }
        }

        if (this_ff == 1){
            fs_read_file(argvs[3], argvs3_path, 0, current_dir_id);
        }else{
            for (uint64_t i = 0; i < argvs3_lenght; i++){
                argvs3_path[i] = argvs[3][i];
            }
        }

        if (this_f == 0){
            write(fb, str_to_u64(argvs[1]), str_to_u64(argvs[2]), color_ar[0], color_ar[1], argvs3_path);
        }else{
            int this_stat1 = 0;
            int this_stat2 = 0;
            uint64_t this_if_enter_cheak_size = 4;
            uint8_t this_if_enter_buffer_1[this_if_enter_cheak_size];
            uint8_t this_if_enter_buffer_2[this_if_enter_cheak_size];
            for (uint64_t i = 0; i < this_if_enter_cheak_size; i++) this_if_enter_buffer_1[i] = 0x00;
            for (uint64_t i = 0; i < this_if_enter_cheak_size; i++) this_if_enter_buffer_2[i] = 0x00;
            this_stat1 = fs_read_file(path_resolve(path_to_abs(argvs[1], current_dir_id)), this_if_enter_buffer_1, this_if_enter_cheak_size, current_dir_id);
            this_stat2 = fs_read_file(path_resolve(path_to_abs(argvs[2], current_dir_id)), this_if_enter_buffer_2, this_if_enter_cheak_size, current_dir_id);
            if (this_stat1 == 1 || this_stat2 == 1){
                return 1;
            }
            write(fb, str_to_u64(this_if_enter_buffer_1), str_to_u64(this_if_enter_buffer_2), color_ar[0], color_ar[1], argvs3_path);
        }
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
    else if (fs_strcmp(argvs[0], "math", command_lenght)){
        int this_stat = 0;
        uint64_t this_res = 0;
        uint64_t par1 = 0;
        char *par2 = argvs[3];
        uint64_t par3 = 0;
        uint8_t par1_str[8];
        uint8_t par3_str[8];
        if (idx > 5){
            if ( fs_strcmp(argvs[5], "f", 2)){
                uint8_t this_buffer[512];
                for (uint64_t ipth = 0; ipth < 512; ipth++){this_buffer[ipth] = '\0';}
                fs_read_file(path_to_abs(argvs[2], current_dir_id), this_buffer, 0, current_dir_id);
                for (uint64_t i = 0; i < 8; i++){
                    par1_str[i] = this_buffer[i];
                }
                for (uint64_t ipth = 0; ipth < 512; ipth++){this_buffer[ipth] = '\0';}
                fs_read_file(path_to_abs(argvs[4], current_dir_id), this_buffer, 0, current_dir_id);
                for (uint64_t i = 0; i < 8; i++){
                    par3_str[i] = this_buffer[i];
                }
                par1 = str_to_u64(par1_str);
                par3 = str_to_u64(par3_str);
            }
        }else{
            par1 = str_to_u64(argvs[2]);
            par3 = str_to_u64(argvs[4]);
        }
        if      (par2[0] == '+' && par2[1] == '\0'){this_res = par1 + par3;}
        else if (par2[0] == '-' && par2[1] == '\0'){this_res = par1 - par3;}
        else if (par2[0] == '/' && par2[1] == '\0'){this_res = par1 / par3;}
        else if (par2[0] == '*' && par2[1] == '\0'){this_res = par1 * par3;}
        else if (par2[0] == 'c' && par2[1] == '\0'){if (par1 > par3){this_res = 2;}else if (par1 == par3){this_res = 1;}else if (par1 < par3){this_res = 0;}}
        else{return 4;}
        this_stat = fs_write_file(path_resolve(path_to_abs(argvs[1], current_dir_id)), u64_to_str(this_res), 0, current_dir_id);
        return 0;
    }
    else if (fs_strcmp(argvs[0], "test", command_lenght)){
        char test_target[1024];
        char test_output[1024];
        fs_strncpy(test_target, path_resolve(path_to_abs(argvs[2], current_dir_id)), 1024);
        fs_strncpy(test_output, path_resolve(path_to_abs(argvs[1], current_dir_id)), 1024);
        fs_test_file(test_target, test_output, current_dir_id);
        return 0;
    }
    return 1;
}
int shell_command(const char *test_cmd){
    int enter_output = enter(test_cmd);

    if (enter_output == 2 || enter_output == 3){
        if (enter_output == 2){
            global_crus_y = write(fb, 0, global_crus_y, color_ar[0], color_ar[1], (char *)read_out);
        }else{
            global_crus_y = write(fb, 0, global_crus_y, color_ar_error[0], color_ar_error[1], (char *)read_out);
        }
        plus_gc_y(fb, color_ar[0], color_ar[1]);
        global_crus_x = global_defualt_crus_x;
    }
    
    return enter_output;
}

void recal_clt(void){
    id_to_path(current_dir_id, cmd_line_text);
    int T = 0;
    uint64_t clt_lenght = 0;
    while (T == 0){
        if (cmd_line_text[clt_lenght] == '\0'){
            cmd_line_text[clt_lenght] = '>';
            cmd_line_text[clt_lenght +1] = ' ';
            cmd_line_text[clt_lenght +2] = '\0';
            T = 1;
        }else{
            clt_lenght++;
        }
    }
    global_defualt_crus_x = clt_lenght+2;
}

void scroll_screen(struct limine_framebuffer *fb, uint32_t color, uint32_t color_none){
    for (uint64_t y = 0; y < screen_rows - 1; y++){
        for (uint64_t x = 0; x < screen_cols; x++){
            screen_buffer[y][x] = screen_buffer[y+1][x];
        }
    }

    for (uint64_t x = 0; x < screen_cols; x++){
        screen_buffer[screen_rows-1][x] = '\0';
    }

    for (uint64_t y = 0; y < screen_rows; y++){
        for (uint64_t x = 0; x < screen_cols; x++){
            put_char(fb, x, y, color, screen_buffer[y][x], color_none);
        }
    }
}

void plus_gc_y(struct limine_framebuffer *fb, uint32_t color, uint32_t color_none){
    if (global_crus_y < screen_rows - 1){
        global_crus_y++;
    }else{
        scroll_screen(fb, color, color_none);
    }
}

void shell(void){
    uint64_t x = 1;
    uint64_t y = 1;
    uint32_t color = 0xFFFFFFFF;

    int shell_T = 0;

    while (shell_T == 0) {
        put_crus(fb, color_ar[0], color_ar[1]);
        char c;
        if (read_kyboard_from_main(&c)) {
            char str[2] = {c, '\0'};
            if (str[0] == '\n'){
                plus_gc_y(fb, color_ar[0], color_ar[1]);
                global_crus_x = global_defualt_crus_x;
                int enter_output = enter(command);
                for(uint64_t i8 = 0; i8 < last_global_command_lenght; i8++){
                    command[i8] = 0x00;
                }
                command_point_c_main = 0;
                recal_clt();
                plus_gc_y(fb, color_ar[0], color_ar[1]);
                if(enter_output == 2 || enter_output == 3 ){
                    uint64_t this_read_out_lenght = 0;
                    for(uint64_t iiro = 0; iiro<FRAME_SIZE; iiro++){
                        this_read_out_lenght++;
                        if (read_out[iiro] == '\0'){break;}
                    }
                    if(enter_output == 2){
                        global_crus_y = write(fb, 0, global_crus_y, color_ar[0], color_ar[1], read_out);       
                    }else if (enter_output == 3){
                        global_crus_y = write(fb, 0, global_crus_y, color_ar_error[0], color_ar_error[1], read_out);  
                    }
                    plus_gc_y(fb, color_ar[0], color_ar[1]);
                    global_crus_x = global_defualt_crus_x;
                    write(fb, 0, global_crus_y, color_ar[0], color_ar[1], cmd_line_text);
                }else{
                    global_crus_x = global_defualt_crus_x;
                    write(fb, 0, global_crus_y, color_ar[0], color_ar[1], cmd_line_text);
                }
                put_crus(fb, color_ar[0], color_ar[1]);

                shell_T = 1;
                continue;
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
    return;
}

// shell }

void task_create(struct task *t, uint8_t *stack, uint64_t stack_size, void (*entry)(void)){
    uint64_t *stack_top = (uint64_t *)(stack + stack_size);
    stack_top = (uint64_t *)((uint64_t)stack_top & ~0xFULL);

    uint64_t *sp = stack_top;

    *(--sp) = 0x00;
    *(--sp) = (uint64_t)stack_top;
    *(--sp) = 0x202;                 // RFLAGS (IF=1)
    *(--sp) = 0x08;                  // CS
    *(--sp) = (uint64_t)entry;       // RIP

    *(--sp) = 0;  // error code
    *(--sp) = 32; // int_num

    *(--sp) = 0; // rax
    *(--sp) = 0; // rbx
    *(--sp) = 0; // rcx
    *(--sp) = 0; // rdx
    *(--sp) = 0; // rsi
    *(--sp) = 0; // rdi
    *(--sp) = 0; // rbp
    *(--sp) = 0; // r8
    *(--sp) = 0; // r9
    *(--sp) = 0; // r10
    *(--sp) = 0; // r11
    *(--sp) = 0; // r12
    *(--sp) = 0; // r13
    *(--sp) = 0; // r14
    *(--sp) = 0; // r15

    t->rsp = (uint64_t)sp;
    t->active = 1;
}


struct task tasks[max_tasks];
uint8_t task_stacks[max_tasks][16777216];

uint64_t task_count = 0;
uint64_t current_task_id = 0;

void task_exit(void){
    tasks[current_task_id].active = 0;

    __asm__ volatile("int $32");

    for(;;){
        __asm__ volatile("hlt");
    }
}

static void copy_task_name(uint64_t idx, char *name){
    int ou = 0;
    for (uint64_t i = 0; i < 32; i++){
        if (ou == 0){
            tasks[idx].name[i] = name[i];
        }
        if (name[i] == '\0'){ou = 1;}
    }
}

void creat_task_path(char *run_path, char *name){
    if (task_count >= max_tasks){
        return;
    }

    copy_task_name(task_count, name);

    int i = 0;
    while (run_path[i] != '\0' && i < 1023){
        tasks[task_count].run_path[i] = run_path[i];
        i++;
    }
    tasks[task_count].run_path[i] = '\0';

    task_create(
        &tasks[task_count],
        task_stacks[task_count],
        sizeof(task_stacks[task_count]),
        run_task
    );

    task_count++;
}

void creat_task_func(void (*entry)(void), char *name){
    if (task_count >= max_tasks){
        return;
    }

    copy_task_name(task_count, name);
    tasks[task_count].run_path[0] = '\0';
    
    task_create(
        &tasks[task_count],
        task_stacks[task_count],
        sizeof(task_stacks[task_count]),
        entry
    );

    task_count++;
}

void delet_task(char *name){
    if (task_count >= max_tasks){
        return;
    }
    for (uint64_t i = 0; i < task_count; i++){
        int T = 0;
        uint64_t TN_lenght = 0;
        while (T == 0){
            if (tasks[i].name[TN_lenght] == '\0'){
                T = 1;
            }else{
                TN_lenght++;
            }
        }
        int T2 = 0;
        uint64_t lenght = 0;
        while (T2 == 0){
            if (name[lenght] == '\0'){
                T2 = 1;
            }else{
                lenght++;
            }
        }
        if (TN_lenght != lenght){continue;}
        int ou = 0;
        for (uint64_t i2 = 0; i2 < TN_lenght; i2++){
            if (tasks[i].name[i2] != name[i2] && ou == 0){
                ou = 1;
                continue;
            }
        }
        if (ou == 0){
            tasks[i].active = 0;
            return;
        }
    }
    return;
}


void make_the_tree(void){
    // make the root folders
    shell_command("mkdir /home");
    shell_command("mkdir /var");
    shell_command("mkdir /proc");
    shell_command("mkdir /tmp");
    shell_command("mkdir /input");
    shell_command("mkdir /proc/for");

    // some files/folders
    shell_command("mkdir /home/jad");

    // var

    shell_command("mk /tmp/value1");
    shell_command("write /tmp/value1 1");
    shell_command("mk /proc/shell");
    shell_command("mk /input/input_c");
    shell_command("mk /var/rand");
    shell_command("mk /var/rand_2");

    shell_command("write /proc/shell 1");
    
    shell_command("rand i");

    return;
}


void boot_commands(void){

    shell_command("test /home/jad/games/mygame1/firsttime /home/jad/games/mygame1/firsttime");
    shell_command("if /home/jad/games/mygame1/firsttime /tmp/value1");
    shell_command("else /home/jad/games/mygame1/creatfile");
    shell_command("mk /home/jad/games/mygame1/firsttime");

    shell_command("cd /home/jad/games/mygame1/");

    shell_command("mk ./creatfile");
    shell_command("wadd ./creatfile mk ./px");
    shell_command("wadd ./creatfile mk ./py");
    shell_command("wadd ./creatfile write ./px 0");
    shell_command("wadd ./creatfile write ./py 0");
    shell_command("wadd ./creatfile mk ./score");
    shell_command("wadd ./creatfile write ./score 0");
    shell_command("sh ./creatfile");
    
    shell_command("mk ./mainloop");
    shell_command("wadd ./mainloop mainloop");
}

void test_task_normal_shell(void){
    for(;;){
        uint8_t this_buffer[2];
        fs_read_file("/proc/shell", this_buffer, 2, current_dir_id);
        if (this_buffer[0] == '1'){
            shell();
        }
        __asm__ volatile ("hlt");
    }
}

void test_task_counter(void){
    for(;;){
        write(fb, 40, 0, color_ar[0], color_ar[1], u64_to_str(task_b_counter));
        sleep_ms(500);
        task_b_counter++;
        __asm__ volatile ("hlt");
    }
}

void run_task(void){
    for(;;){
        shell_command(str_concat("sh ", tasks[current_task_id].run_path));
        __asm__ volatile ("hlt");
    }
}

char path_174[1024];

void echo_table(void){
    for(;;){
        debug_put64(current_dir_id);
        debug_putc('\n');
        //sleep_ms(1000);
        //for (uint64_t i = 0; i < 11; i++){
        //    id_to_path(i, path_174);
        //    debug_put64(i);
        //    debug_putc(' ');
        //    debug_print(path_174);
        //    debug_putc('\n');
        //}
        __asm__ volatile ("hlt");
    }
}

void kmain(void) {
    gdt_init();
    idt_init();
    pic_remap(); 
    pmm_init();
    int fs_is_new = fs_init();    

    fb = fb_request.response->framebuffers[0];

    recal_clt();
    uint64_t x = 1;
    uint64_t y = 1;
    uint32_t color = 0xFFFFFFFF;

    screen_cols = (fb->width / 8);
    screen_rows = (fb->height /8);

    screen_height = (fb->height);
    screen_width = (fb->width);

    read_out = (uint8_t *)pmm_phys_to_virt(pmm_alloc_frame());

    for (uint64_t i = 0; i < FRAME_SIZE; i++){
        read_out[i] = 0x00;
    }

    uint64_t cursor_x = 1;
    uint64_t cursor_y = 1;

    for (uint64_t yF = 0; yF < screen_rows; yF++){
        for (uint64_t xF = 0; xF < screen_cols; xF++){
            write(fb, xF, yF, color_ar[0], color_ar[1], " ");
        }
    }
    
    recal_clt();
    global_crus_x = global_defualt_crus_x;
    write(fb, 0, global_crus_y, color_ar[0], color_ar[1], cmd_line_text);
    
    if (fs_is_new == 0){
        make_the_tree();
    }

    for (uint64_t i = 0; i < 11; i++){
        id_to_path(i, path_174);
        debug_put64(i);
        debug_putc(' ');
        debug_print(path_174);
        debug_putc('\n');
    }

    boot_commands();

    enter(command);

    creat_task_func(test_task_normal_shell, "shell");
    creat_task_func(test_task_counter, "counter");
    current_task_id = 0;

    pit_init(100);

    extern void jump_to_task(uint64_t rsp);
    jump_to_task(tasks[0].rsp);
    
    for(;;) { __asm__("hlt"); }

}