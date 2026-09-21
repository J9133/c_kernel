#include <stdint.h>
#include <limine.h>
#include <stddef.h>
#include "fs.h"
#include "pmm.h"
#include "debug.h"
#include "somthings.h"
#include "ata.h"

#define frame_size_bytes 4096

#define fs_max_files_entrys 1024
#define fs_size_frames (fs_max_files_entrys)

#define fs_table_entry_size_bytes 64

#define fs_table_size_entrys (fs_max_files_entrys)
#define fs_table_size_bytes (fs_table_size_entrys * fs_table_entry_size_bytes)

#define fs_table_size_sectors ((fs_max_files_entrys * 64 + 511) / 512)
#define fs_data_bitmap_max_size_bytes ((fs_max_files_entrys + 7) / 8)

#define data_file_region_size 8

uint64_t sectors = 0;
uint64_t fs_data_bitmap_size_sectors = 0;
uint64_t start_table = 0;
uint64_t start_storage = 0;

uint64_t fs_not_found = 0-1;

uint64_t file_counter = 0;

char path_resolve_path_res[1024];
char path_to_abs_path_res[1024];
char path_to_abs_path_res_tmp[1024];

char id_to_path_path[512][32];
uint64_t id_to_path_point = 0;
int id_to_path_first_one = 0;

struct fs_file_entry {
    char     name[24];
    uint64_t size;
    uint64_t parent_id;
    uint64_t id;
    uint16_t type;
    uint16_t region_size;
    uint32_t flags;
    uint64_t data_frame;
};

static void bitmap_set(uint64_t fream_index){
    uint8_t this_bitmap[512];
    uint64_t sector_num = 1 + (fream_index / 8 / 512);
    uint64_t byte_in_sector = (fream_index / 8) % 512;

    ata_sector_read((uint32_t)sector_num, 1, this_bitmap);
    this_bitmap[byte_in_sector] |= (1 << (fream_index % 8));
    ata_sector_write((uint32_t)sector_num, 1, this_bitmap);
}

static void bitmap_clear(uint64_t fream_index){
    uint8_t this_bitmap[512];
    uint64_t sector_num = 1 + (fream_index / 8 / 512);
    uint64_t byte_in_sector = (fream_index / 8) % 512;

    ata_sector_read((uint32_t)sector_num, 1, this_bitmap);
    this_bitmap[byte_in_sector] &= ~(1 << (fream_index % 8));
    ata_sector_write((uint32_t)sector_num, 1, this_bitmap);
}

static int bitmap_read(uint64_t fream_index){
    uint8_t this_bitmap[512];
    uint64_t sector_num = 1 + (fream_index / 8 / 512);
    uint64_t byte_in_sector = (fream_index / 8) % 512;

    ata_sector_read((uint32_t)sector_num, 1, this_bitmap);
    return this_bitmap[byte_in_sector] & (1 << (fream_index % 8));
}

struct fs_file_entry *get_table_entry(){
    return 0;
}

void write_uint64_to_last_8_bytes(uint64_t frame_index, uint64_t size, uint64_t value){
    if (size == 0){
        return;
    }

    uint64_t data_region_start = start_table + fs_table_size_sectors;
    uint64_t last_sector = data_region_start + frame_index + size - 1;

    uint8_t buf[512];
    ata_sector_read((uint32_t)last_sector, 1, buf);

    for (int i = 0; i < 8; i++){
        buf[504 + i] = (uint8_t)(value >> (i * 8)) & 0xFF;
    }

    ata_sector_write((uint32_t)last_sector, 1, buf);
}

int write_entry_to_table(struct fs_file_entry *this_entry){
    int T = 0;
    uint64_t i = 0;
    uint8_t this_buffer[512];
    while (T == 0){
        if (i >= fs_table_size_sectors){
            T = 1;
        }else{
            int stat = ata_sector_read((uint32_t)start_table+i, 1, this_buffer);
            if (stat == 0){
                for (uint64_t i3 = 0; i3 < 8; i3++){
                    uint8_t this_entry_raw[64];
                    for (uint64_t i2 = 0; i2 < 64; i2++){
                        this_entry_raw[i2] = this_buffer[i3*64 + i2];
                    }
                    struct fs_file_entry *check_entry = (struct fs_file_entry *)this_entry_raw;

                    if (check_entry->id == 0){
                        uint8_t *entry_bytes = (uint8_t *)this_entry;
                        for (uint64_t i2 = 0; i2 < 64; i2++){
                            this_buffer[i3*64 + i2] = entry_bytes[i2];
                        }
                        ata_sector_write((uint32_t)start_table+i, 1, this_buffer);
                        return 0;
                    }
                }
            }
            i++;
        }
    }
    return -1;
}

int id_to_path_internal(uint64_t id, char *buffer){
    int T79 = 0;
    uint64_t i79 = 0;
    uint8_t this_buffer79[512];
    while (T79 == 0){
        if (i79 >= fs_table_size_sectors){
            T79 = 1;
        }else{
            int stat79 = ata_sector_read((uint32_t)start_table+i79, 1, this_buffer79);
            if (stat79 == 0){
                for (uint64_t i379 = 0; i379 < 8; i379++){
                    uint8_t this_entry_raw79[64];
                    for (uint64_t i279 = 0; i279 < 64; i279++){
                        this_entry_raw79[i279] = this_buffer79[i379*64 + i279];
                    }
                    struct fs_file_entry *this_entry = (struct fs_file_entry *)this_entry_raw79;
                    if (this_entry->id == id){
                        int T = 0;
                        uint64_t this_elif_lenght = 0;
                        while (T == 0){
                            if (this_entry->name[this_elif_lenght] == '\0'){
                                T = 1;
                            }else{
                                this_elif_lenght++;
                            }
                        }
                        for (uint64_t i = 0; i < this_elif_lenght; i++){
                            id_to_path_path[id_to_path_point][i] = this_entry->name[i];
                        }
                        id_to_path_point++;
                        if (this_entry->parent_id != 0){
                            id_to_path_internal(this_entry->parent_id, buffer);
                        }
                        return 0;
                    }
                }
            }
            i79++;
        }
    }
    return -1;
}

uint64_t name_and_parent_id_to_id(const char *name, uint64_t parent_id){
    int T = 0;
    uint64_t i = 0;
    uint8_t this_buffer[512];
    while (T == 0){
        if (i >= fs_table_size_sectors){
            T = 1;
        }else{
            int stat = ata_sector_read((uint32_t)start_table+i, 1, this_buffer);
            if (stat == 0){
                for (uint64_t i3 = 0; i3 < 8; i3++){
                    uint8_t this_entry_raw[64];
                    for (uint64_t i2 = 0; i2 < 64; i2++){
                        this_entry_raw[i2] = this_buffer[i3*64 + i2];
                    }
                    struct fs_file_entry *this_entry = (struct fs_file_entry *)this_entry_raw;
                    int stat2 = fs_strcmp(this_entry->name, name, 24);
                    if (stat2 != 0 && this_entry->parent_id == parent_id){
                        return this_entry->id;
                    }
                }
            }
            i++;
        }
    }
    return fs_not_found;
}

int id_to_path(uint64_t id, char *buffer){
    int this_first_one = 0;
    if (id_to_path_first_one == 0){
        this_first_one = 1;
        id_to_path_first_one = 1;
        for (uint64_t i = 0; i < 512; i++){
            for (uint64_t i2 = 0; i2 < 32; i2++){
                id_to_path_path[i][i2] = 0x00;
            }
        }
    }
    int T79 = 0;
    uint64_t i79 = 0;
    uint8_t this_buffer79[512];
    while (T79 == 0){
        if (i79 >= fs_table_size_sectors){
            T79 = 1;
        }else{
            int stat79 = ata_sector_read((uint32_t)start_table+i79, 1, this_buffer79);
            if (stat79 == 0){
                for (uint64_t i379 = 0; i379 < 8; i379++){
                    uint8_t this_entry_raw79[64];
                    for (uint64_t i279 = 0; i279 < 64; i279++){
                        this_entry_raw79[i279] = this_buffer79[i379*64 + i279];
                    }
                    struct fs_file_entry *this_entry = (struct fs_file_entry *)this_entry_raw79;
                    if (this_entry->id == id){
                        if (this_entry->parent_id == 0){
                            int T = 0;
                            uint64_t this_elif_lenght = 0;
                            while (T == 0){
                                if (this_entry->name[this_elif_lenght] == '\0'){
                                    T = 1;
                                }else{
                                    this_elif_lenght++;
                                }
                            }
                            for (uint64_t i = 0; i < this_elif_lenght; i++){
                                id_to_path_path[id_to_path_point][i] = this_entry->name[i];
                            }
                            id_to_path_point++;
                            id_to_path_first_one = 0;
                            T79 = 1;
                            break;
                        }else{
                            int T = 0;
                            uint64_t this_elif_lenght = 0;
                            while (T == 0){
                                if (this_entry->name[this_elif_lenght] == '\0'){
                                    this_elif_lenght++;
                                    T = 1;
                                }else{
                                    this_elif_lenght++;
                                }
                            }
                            for (uint64_t i = 0; i < this_elif_lenght; i++){
                                id_to_path_path[id_to_path_point][i] = this_entry->name[i];
                            }
                            id_to_path_point++;
                            id_to_path(this_entry->parent_id, buffer);
                            T79 = 1;
                            break;
                        }
                    }
                }
            }
        }
        i79++;
    }
    if (this_first_one == 1){
        id_to_path_first_one = 0;
    }
    if (this_first_one == 1){
        uint64_t here_point = 0;
        buffer[here_point] = '/';
        here_point++;
        for (uint64_t i = id_to_path_point; i > 0; i--){
            uint64_t i2_lenght = 0;
            int Ti2 = 0;
            while (Ti2 == 0){
                if (id_to_path_path[i-1][i2_lenght] == '\0'){
                    Ti2 = 1;
                }else{
                    i2_lenght++;
                }
            }
            for (uint64_t i2 = 0; i2 < i2_lenght; i2++){
                buffer[here_point] = id_to_path_path[i-1][i2];
                here_point++;
            }if (i != 1){
                buffer[here_point] = '/';
                here_point++;
            }
        }
        buffer[here_point] = '\0';
        id_to_path_point = 0;
    }
    return 0;
}

char *path_to_name(char *path){
    uint8_t T = 0;
    uint64_t counter = 0;
    while(T == 0){
        if(path[counter] == '\0'){
            T = 1;
        }else{
            counter++;
        }
    }
    int64_t iout = -1;
    for(uint64_t i = 0; i<counter; i++){
        if (path[i] == '/'){
            iout = (int64_t)i;
        }
    }
    static char name[24];
    uint64_t name_len = counter - (iout + 1);
    for(uint64_t i = 0; i<name_len; i++){
        name[i] = path[iout+1+i];
    }
    name[name_len] = '\0';
    return name;
}

int rel_or_abs_path(char *path){
    if(path[0] == '/'){
        return 0;
    }else{
        return 1;
    }
}

uint64_t path_to_parent_id(char *path, uint64_t current_dir_id){
    int T = 0;
    uint64_t i = 0;
    uint8_t this_buffer[512];
    uint64_t this_id = path_to_id(path_to_abs(path, current_dir_id), current_dir_id);
    while (T == 0){
        if (i >= fs_table_size_sectors){
            T = 1;
        }else{
            int stat = ata_sector_read((uint32_t)start_table+i, 1, this_buffer);
            if (stat == 0){
                for (uint64_t i3 = 0; i3 < 8; i3++){
                    uint8_t this_entry_raw[64];
                    for (uint64_t i2 = 0; i2 < 64; i2++){
                        this_entry_raw[i2] = this_buffer[i3*64 + i2];
                    }
                    struct fs_file_entry *this_entry = (struct fs_file_entry *)this_entry_raw;
                    if (this_entry->id == this_id){
                        return this_entry->parent_id;
                    }
                }
            }
            i++;
        }
    }
    return fs_not_found;
}

uint64_t path_to_id(char *path, uint64_t current_dir_id){
    if (path[0] == '.' && path[1] == '\0'){
        return current_dir_id;
    }
    char path_buf[1024];
    uint64_t i0 = 0;
    while (path[i0] != '\0' && i0 < 1023){
        path_buf[i0] = path[i0];
        i0++;
    }
    path_buf[i0] = '\0';
    path = path_buf;

    uint64_t pathTT = 0;
    while (path[pathTT] != '\0'){
        pathTT++;
    }
    if (pathTT > 1 && path[pathTT-1] == '/'){
        path[pathTT-1] = '\0';
    }

    int got_all_f = 0;
    uint64_t while_read_point = 0;
    int while0_finish = 0;
    int64_t path_lenght = 0;

    while(while0_finish == 0){
        if (path[path_lenght] == '\0'){
            path_lenght++;
            while0_finish = 1;
        }else{
            path_lenght++;
        }
    }

    char argv[path_lenght];
    uint64_t argc = 1;

    for(uint64_t i = 0; i < path_lenght; i++){
        if (path[i] == '/'){
            argv[i] = '\0';
            argc++;
        }else{
            argv[i] = path[i];
        }
    }

    char *argvs[argc];
    uint64_t idx = 0;
    int new_word = 1;

    for (uint64_t i = 0; i < path_lenght; i++){
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
        if (rel_or_abs_path(path) == 1){
            return current_dir_id;
        }else{
            return 0;
        }
    }

    uint64_t this_id = (rel_or_abs_path(path) == 1) ? current_dir_id : 0;

    for (uint64_t i = 0; i < idx; i++){
        this_id = name_and_parent_id_to_id(argvs[i], this_id);
    }

    return this_id;
}

char *path_resolve(char *path){
    char path_buf[1024];
    for (uint64_t ipth = 0; ipth < 1024; ipth++){path_buf[ipth] = '\0';}
    uint64_t i0 = 0;
    while (path[i0] != '\0' && i0 < 1023){
        path_buf[i0] = path[i0];
        i0++;
    }
    path_buf[i0] = '\0';
    path = path_buf;

    uint64_t pathTT = 0;
    while (path[pathTT] != '\0'){
        pathTT++;
    }
    if (pathTT > 1 && path[pathTT-1] == '/'){
        path[pathTT-1] = '\0';
    }

    int64_t path_lenght = 0;
    int while0_finish = 0;
    while(while0_finish == 0){
        if (path[path_lenght] == '\0'){
            path_lenght++;
            while0_finish = 1;
        }else{
            path_lenght++;
        }
    }

    char argv[path_lenght];
    uint64_t argc = 1;
    for(uint64_t i = 0; i < path_lenght; i++){
        if (path[i] == '/'){
            argv[i] = '\0';
            argc++;
        }else{
            argv[i] = path[i];
        }
    }

    char *argvs[argc];
    uint64_t idx = 0;
    int new_word = 1;
    for (uint64_t i = 0; i < path_lenght; i++){
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

    char argv_copy[path_lenght];
    for (uint64_t i = 0; i < path_lenght; i++){
        argv_copy[i] = argv[i];
    }

    char *argvs_copy[argc];
    uint64_t idx2 = 0;
    int new_word2 = 1;
    for (uint64_t i = 0; i < path_lenght; i++){
        if (argv_copy[i] == '\0'){
            new_word2 = 1;
        }else{
            if (new_word2){
                argvs_copy[idx2] = &argv_copy[i];
                idx2++;
                new_word2 = 0;
            }
        }
    }

    uint64_t stack_count = 0;
    for (uint64_t i = 0; i < idx; i++){
        if (argvs[i][0] == '.' && argvs[i][1] == '\0'){
            continue;
        }
        if (argvs[i][0] == '.' && argvs[i][1] == '.' && argvs[i][2] == '\0'){
            if (stack_count > 0){
                stack_count--;
            }
            continue;
        }
        argvs_copy[stack_count] = argvs[i];
        stack_count++;
    }

    for (uint64_t ipth = 0; ipth < 1024; ipth++){path_resolve_path_res[ipth] = '\0';}
    uint64_t this_path_buffer = 1;
    path_resolve_path_res[0] = '/';

    for (uint64_t i = 0; i < stack_count; i++){
        int T = 0;
        uint64_t this_idi_lenght = 0;
        while (T == 0){
            if (argvs_copy[i][this_idi_lenght] == '\0'){
                T = 1;
            }else {
                this_idi_lenght++;
            }
        }
        for (uint64_t i2 = 0; i2 < this_idi_lenght; i2++){
            path_resolve_path_res[this_path_buffer] = argvs_copy[i][i2];
            this_path_buffer++;
        }
        if (i < stack_count-1){
            path_resolve_path_res[this_path_buffer] = '/';
            this_path_buffer++;
        }
    }

    path_resolve_path_res[this_path_buffer] = '\0';
    this_path_buffer++;

    return path_resolve_path_res;
}

char *path_to_abs(char *path, uint64_t current_dir_id){
    for (uint64_t ipth = 0; ipth < 1024; ipth++){ path_to_abs_path_res[ipth] = '\0'; }
    if (rel_or_abs_path(path) == 0){
        return path;
    }
    id_to_path(current_dir_id, path_to_abs_path_res);
    uint64_t this_res_point = 0;
    while (path_to_abs_path_res[this_res_point] != '\0'){ this_res_point++; }
    if (this_res_point == 0 || path_to_abs_path_res[this_res_point - 1] != '/'){
        path_to_abs_path_res[this_res_point] = '/';
        this_res_point++;
    }
    uint64_t i = 0;
    while (path[i] != '\0' && this_res_point + i < 1023){
        path_to_abs_path_res[this_res_point + i] = path[i];
        i++;
    }
    path_to_abs_path_res[this_res_point + i] = '\0';

    return path_to_abs_path_res;
}

uint64_t get_data_region(int last_data_frame, uint64_t size){
    int T = 0;
    uint64_t i = 0;
    uint8_t this_buffer[512];
    while (T == 0){
        if (i >= fs_data_bitmap_size_sectors){
            T = 1;
        }else{
            int stat = ata_sector_read((uint32_t)1+i, 1, this_buffer);
            if (stat == 0){
                for (uint64_t i3 = 0; i3 < 512; i3++){
                    uint8_t this_byte = this_buffer[i3];
                    for (uint64_t i4 = 0; i4 < 8; i4++){
                        int this_bit = 0;
                        for (uint64_t i5 = 0; i5 < size; i5++){
                            uint64_t this_shift = i4+i5;
                            uint64_t byte_to_check = this_byte;
                            if ((this_shift) > 7){
                                uint64_t this_byte_shift = this_shift / 8;
                                this_shift = this_shift-this_byte_shift*8;
                                byte_to_check = this_buffer[i3+this_byte_shift];
                            }
                            this_bit += (byte_to_check >> (this_shift)) & 1;
                        }
                        if (this_bit == 0){
                            uint64_t byte_index = i * 512 + i3;
                            uint64_t bit_index = byte_index * 8 + i4;
                            if (last_data_frame != 0){
                                write_uint64_to_last_8_bytes(last_data_frame, size, bit_index);
                            }
                            return bit_index;
                        }
                    }
                }
            }
            i++;
        }
    }
    return fs_not_found;
}

uint64_t fpath_to_path(char *path, uint64_t current_dir_id){
    char buf[1024];
    uint64_t len = 0;
    while (path[len] != '\0' && len < 1023){
        buf[len] = path[len];
        len++;
    }
    buf[len] = '\0';
    while (len > 1 && buf[len - 1] == '/'){
        buf[--len] = '\0';
    }
    int64_t last = -1;
    for (uint64_t i = 0; i < len; i++){
        if (buf[i] == '/'){
            last = (int64_t)i;
        }
    }
    if (last < 0){
        return current_dir_id;
    }
    if (last == 0){
        return 0;
    }
    buf[last] = '\0';
    return path_to_id(buf, current_dir_id);
}

int fs_mk_file(const char *path, uint64_t current_dir_id){
    struct fs_file_entry this_entry_storage;
    struct fs_file_entry *this_entry = &this_entry_storage;
    char *name_result = path_to_name(path);
    char this_name[24];
    for (uint64_t ipth = 0; ipth < 24; ipth++){this_name[ipth] = '\0';}
    for (int j = 0; j < 24 && name_result[j] != '\0'; j++){
        this_name[j] = name_result[j];
    }
    this_name[23] = '\0';
    for (int i = 0; i < 24; i++){
        this_entry->name[i] = this_name[i];
    }
    this_entry->size = 0;
    this_entry->parent_id = fpath_to_path(path_to_abs(path, current_dir_id), current_dir_id);
    if (this_entry->parent_id == fs_not_found){
        return 1;
    }
    this_entry->id = read_last_id_from_disk() + 1;
    if (this_entry->id == fs_not_found){
        return 1;
    }
    this_entry->type = 2;
    this_entry->flags = 0;
    uint64_t data_frame = get_data_region(0, data_file_region_size);
    if (data_frame == fs_not_found){
        return 1;
    }
    for (uint64_t k = 0; k < data_file_region_size; k++){
        bitmap_set(data_frame + k);
    }
    this_entry->data_frame = data_frame;
    this_entry->region_size = data_file_region_size;
    write_entry_to_table(this_entry);
    reload_last_id_to_disk();
    return 0;
}

int fs_mk_file_size(const char *path, uint64_t current_dir_id, uint64_t size){
    struct fs_file_entry this_entry_storage;
    struct fs_file_entry *this_entry = &this_entry_storage;
    char *name_result = path_to_name(path);
    char this_name[24];
    for (uint64_t ipth = 0; ipth < 24; ipth++){this_name[ipth] = '\0';}
    for (int j = 0; j < 24 && name_result[j] != '\0'; j++){
        this_name[j] = name_result[j];
    }
    this_name[23] = '\0';
    for (int i = 0; i < 24; i++){
        this_entry->name[i] = this_name[i];
    }
    this_entry->size = 0;
    this_entry->parent_id = fpath_to_path(path_to_abs(path, current_dir_id), current_dir_id);
    if (this_entry->parent_id == fs_not_found){
        return 1;
    }
    this_entry->id = read_last_id_from_disk() + 1;
    if (this_entry->id == fs_not_found){
        return 1;
    }
    this_entry->type = 2;
    this_entry->flags = 0;
    uint64_t data_frame = get_data_region(0, size);
    if (data_frame == fs_not_found){
        return 1;
    }
    for (uint64_t k = 0; k < size; k++){
        bitmap_set(data_frame + k);
    }
    this_entry->data_frame = data_frame;
    this_entry->region_size = (uint16_t)size;
    write_entry_to_table(this_entry);
    reload_last_id_to_disk();
    return 0;
}

int fs_mk_dir(const char *path, uint64_t current_dir_id){
    struct fs_file_entry this_entry_storage;
    struct fs_file_entry *this_entry = &this_entry_storage;
    char *name_result = path_to_name(path);
    char this_name[24];
    for (uint64_t ipth = 0; ipth < 24; ipth++){this_name[ipth] = '\0';}
    for (int j = 0; j < 24 && name_result[j] != '\0'; j++){
        this_name[j] = name_result[j];
    }
    this_name[23] = '\0';
    for (int i = 0; i < 24; i++){
        this_entry->name[i] = this_name[i];
    }
    this_entry->size = 4;
    this_entry->parent_id = fpath_to_path(path_to_abs(path, current_dir_id), current_dir_id);
    if (this_entry->parent_id == fs_not_found){
        return 1;
    }
    this_entry->id = read_last_id_from_disk() + 1;
    if (this_entry->id == fs_not_found){
        return 1;
    }
    this_entry->type = 3;
    this_entry->flags = 0;
    this_entry->data_frame = 0;
    this_entry->region_size = 0;
    write_entry_to_table(this_entry);
    reload_last_id_to_disk();
    return 0;
}

#define fs_last_sector_payload 504

static uint64_t fs_ptr_from_sector(const uint8_t *s){
    return ((uint64_t)s[511] << 56) | ((uint64_t)s[510] << 48)
         | ((uint64_t)s[509] << 40) | ((uint64_t)s[508] << 32)
         | ((uint64_t)s[507] << 24) | ((uint64_t)s[506] << 16)
         | ((uint64_t)s[505] << 8)  |  (uint64_t)s[504];
}

static void fs_ptr_to_sector(uint8_t *s, uint64_t v){
    for (int k = 0; k < 8; k++){
        s[504 + k] = (uint8_t)(v >> (k * 8));
    }
}

static uint64_t fs_region_capacity(uint64_t region_size){
    return (region_size - 1) * 512 + fs_last_sector_payload;
}

struct fs_file_entry *fs_write_file_inside(const void *data, uint64_t size, struct fs_file_entry *this_entry){
    const uint8_t *src = (const uint8_t *)data;
    uint64_t region_size = (uint64_t)this_entry->region_size;
    if (region_size == 0){
        return this_entry;
    }

    if (size == 0){
        while (src[size] != '\0'){ size++; }
    }

    uint64_t capacity = fs_region_capacity(region_size);
    uint64_t region_start = this_entry->data_frame;
    uint64_t written = 0;
    uint8_t sector_buf[512];

    for (;;){
        uint64_t chunk = size - written;
        if (chunk > capacity){ chunk = capacity; }
        int more = (size - written > capacity);

        for (uint64_t j = 0; j < region_size; j++){
            uint32_t lba = (uint32_t)(start_storage + region_start + j);
            uint64_t payload = (j == region_size - 1) ? fs_last_sector_payload : 512;
            uint64_t off = j * 512;

            ata_sector_read(lba, 1, sector_buf);
            uint64_t old_next = fs_ptr_from_sector(sector_buf);

            for (uint64_t k = 0; k < payload; k++){
                uint64_t idx = off + k;
                sector_buf[k] = (idx < chunk) ? src[written + idx] : 0x00;
            }

            if (j == region_size - 1){
                if (more){

                    uint64_t next = old_next;
                    if (next == 0 || next == fs_not_found){
                        next = get_data_region(0, region_size);
                        if (next == fs_not_found){

                            fs_ptr_to_sector(sector_buf, 0);
                            ata_sector_write(lba, 1, sector_buf);
                            this_entry->size = written + chunk;
                            return this_entry;
                        }
                        for (uint64_t k = 0; k < region_size; k++){
                            bitmap_set(next + k);
                        }
                    }
                    fs_ptr_to_sector(sector_buf, next);
                    ata_sector_write(lba, 1, sector_buf);
                    region_start = next;
                }else{
                    fs_ptr_to_sector(sector_buf, 0);
                    ata_sector_write(lba, 1, sector_buf);
                }
            }else{
                ata_sector_write(lba, 1, sector_buf);
            }
        }

        written += chunk;
        if (!more){
            break;
        }
    }

    this_entry->size = size;
    return this_entry;
}

int fs_write_file(const char *path, const void *data, uint64_t size, uint64_t current_dir_id){
    uint64_t this_id = path_to_id(path_to_abs(path, current_dir_id), current_dir_id);
    if (this_id == fs_not_found || this_id == 0){
        return 1;
    }
    int T = 0;
    uint64_t i = 0;
    uint8_t this_buffer[512];
    while (T == 0){
        if (i >= fs_table_size_sectors){
            T = 1;
        }else{
            int stat = ata_sector_read((uint32_t)start_table+i, 1, this_buffer);
            if (stat == 0){
                for (uint64_t i3 = 0; i3 < 8; i3++){
                    uint8_t this_entry_raw[64];
                    for (uint64_t i2 = 0; i2 < 64; i2++){
                        this_entry_raw[i2] = this_buffer[i3*64 + i2];
                    }
                    struct fs_file_entry *this_entry = (struct fs_file_entry *)this_entry_raw;
                    if (this_entry->id == this_id){
                        if (this_entry->type != 2){
                            return 1;
                        }
                        this_entry = fs_write_file_inside(data, size, this_entry);
                        for (uint64_t i5 = 0; i5 < fs_table_entry_size_bytes; i5++){
                            this_buffer[i3*fs_table_entry_size_bytes+i5] = ((uint8_t *)this_entry)[i5];
                        }
                        ata_sector_write((uint32_t)start_table+i, 1, this_buffer);
                        return 0;
                    }
                }
            }
            i++;
        }
    }
    return 1;
}

#define fs_read_default_limit 1024

int fs_read_file_inside(uint8_t *buffer, uint64_t max_size, struct fs_file_entry *this_entry){
    uint64_t region_size = (uint64_t)this_entry->region_size;
    uint64_t file_size = (uint64_t)this_entry->size;
    uint64_t limit = (max_size == 0) ? fs_read_default_limit : max_size;

    if (region_size == 0 || limit == 0){
        return 0;
    }

    uint64_t to_read = file_size;
    if (to_read > limit - 1){ to_read = limit - 1; }

    uint64_t region_start = this_entry->data_frame;
    uint8_t sector_buf[512];
    uint64_t out = 0;

    while (out < to_read){
        uint64_t next_region = 0;
        for (uint64_t j = 0; j < region_size && out < to_read; j++){
            for (uint64_t k = 0; k < 512; k++){ sector_buf[k] = 0x00; }
            ata_sector_read((uint32_t)(start_storage + region_start + j), 1, sector_buf);
            uint64_t payload = (j == region_size - 1) ? fs_last_sector_payload : 512;
            if (j == region_size - 1){
                next_region = fs_ptr_from_sector(sector_buf);
            }
            for (uint64_t k = 0; k < payload && out < to_read; k++){
                buffer[out++] = sector_buf[k];
            }
        }
        if (out >= to_read){
            break;
        }
        if (next_region == 0 || next_region == fs_not_found){
            break;
        }
        region_start = next_region;
    }

    buffer[out] = '\0';
    return 0;
}

int fs_read_file(const char *path, uint8_t *buffer, uint64_t max_size, uint64_t current_dir_id){
    uint64_t this_id = path_to_id(path_to_abs(path, current_dir_id), current_dir_id);
    if (this_id == fs_not_found || this_id == 0){
        return 1;
    }
    int T = 0;
    uint64_t i = 0;
    uint8_t this_buffer[512];
    while (T == 0){
        if (i >= fs_table_size_sectors){
            T = 1;
        }else{
            int stat = ata_sector_read((uint32_t)start_table+i, 1, this_buffer);
            if (stat == 0){
                for (uint64_t i3 = 0; i3 < 8; i3++){
                    uint8_t this_entry_raw[64];
                    for (uint64_t i2 = 0; i2 < 64; i2++){
                        this_entry_raw[i2] = this_buffer[i3*64 + i2];
                    }
                    struct fs_file_entry *this_entry = (struct fs_file_entry *)this_entry_raw;
                    if (this_entry->id == this_id){
                        if (this_entry->type != 2){
                            return 1;
                        }
                        fs_read_file_inside(buffer, max_size, this_entry);
                        return 0;
                    }
                }
            }
            i++;
        }
    }
    return 1;
}

static int find_entry_by_id(uint64_t id, struct fs_file_entry *out, uint64_t *out_sector, uint64_t *out_slot){
    if (id == 0 || id == fs_not_found){
        return 1;
    }
    uint8_t buf[512];
    for (uint64_t s = 0; s < fs_table_size_sectors; s++){
        if (ata_sector_read((uint32_t)(start_table + s), 1, buf) != 0){
            continue;
        }
        for (uint64_t slot = 0; slot < 8; slot++){
            uint8_t raw[64];
            for (uint64_t k = 0; k < 64; k++){
                raw[k] = buf[slot * 64 + k];
            }
            struct fs_file_entry *e = (struct fs_file_entry *)raw;
            if (e->id == id){
                if (out != 0){
                    uint8_t *ob = (uint8_t *)out;
                    for (uint64_t k = 0; k < 64; k++){
                        ob[k] = raw[k];
                    }
                }
                if (out_sector != 0){ *out_sector = s; }
                if (out_slot != 0){ *out_slot = slot; }
                return 0;
            }
        }
    }
    return 1;
}

static void erase_table_entry(uint64_t sector, uint64_t slot){
    uint8_t buf[512];
    if (ata_sector_read((uint32_t)(start_table + sector), 1, buf) != 0){
        return;
    }
    for (uint64_t k = 0; k < 64; k++){
        buf[slot * 64 + k] = 0x00;
    }
    ata_sector_write((uint32_t)(start_table + sector), 1, buf);
}

static void free_file_regions(struct fs_file_entry *e){
    uint64_t region_size = (uint64_t)e->region_size;
    if (region_size == 0){
        return;
    }

    uint64_t frame_size_sec = 8;
    uint64_t frames_num = (e->size + (frame_size_sec - 1)) / frame_size_sec;
    uint64_t regions_num = (frames_num * frame_size_sec) / region_size;
    if (regions_num == 0){
        regions_num = 1;
    }

    uint64_t region_start = e->data_frame;
    uint8_t sector_buf[512];

    for (uint64_t r = 0; r < regions_num; r++){

        uint64_t next_region = fs_not_found;
        if (r + 1 < regions_num){
            for (uint64_t k = 0; k < 512; k++){ sector_buf[k] = 0x00; }
            ata_sector_read((uint32_t)(start_storage + region_start + region_size - 1), 1, sector_buf);
            next_region = ((uint64_t)sector_buf[511] << 56)
                        | ((uint64_t)sector_buf[510] << 48)
                        | ((uint64_t)sector_buf[509] << 40)
                        | ((uint64_t)sector_buf[508] << 32)
                        | ((uint64_t)sector_buf[507] << 24)
                        | ((uint64_t)sector_buf[506] << 16)
                        | ((uint64_t)sector_buf[505] << 8)
                        |  (uint64_t)sector_buf[504];
        }

        for (uint64_t k = 0; k < region_size; k++){
            bitmap_clear(region_start + k);
        }

        if (next_region == fs_not_found || next_region == 0){
            break;
        }
        region_start = next_region;
    }
}

static int dir_has_children(uint64_t dir_id){
    uint8_t buf[512];
    for (uint64_t s = 0; s < fs_table_size_sectors; s++){
        if (ata_sector_read((uint32_t)(start_table + s), 1, buf) != 0){
            continue;
        }
        for (uint64_t slot = 0; slot < 8; slot++){
            uint8_t raw[64];
            for (uint64_t k = 0; k < 64; k++){
                raw[k] = buf[slot * 64 + k];
            }
            struct fs_file_entry *e = (struct fs_file_entry *)raw;
            if (e->id != 0 && e->parent_id == dir_id){
                return 1;
            }
        }
    }
    return 0;
}

int fs_ls_dir(const char *path, char *buffer, uint64_t max_size, uint64_t current_dir_id){
    if (buffer == 0 || max_size == 0){
        return 1;
    }
    buffer[0] = '\0';

    uint64_t dir_id;
    if (path[0] == '.' && path[1] == '\0'){
        dir_id = current_dir_id;
    }else if (path[0] == '/' && path[1] == '\0'){
        dir_id = 0;
    }else{
        dir_id = path_to_id((char *)path, current_dir_id);
        if (dir_id == fs_not_found){
            return 1;
        }
    }

    if (dir_id != 0){
        struct fs_file_entry dir_entry;
        if (find_entry_by_id(dir_id, &dir_entry, 0, 0) != 0){
            return 1;
        }
        if (dir_entry.type != 3){
            return 1;
        }
    }

    uint64_t out = 0;
    uint8_t buf[512];

    for (uint64_t s = 0; s < fs_table_size_sectors; s++){
        if (ata_sector_read((uint32_t)(start_table + s), 1, buf) != 0){
            continue;
        }
        for (uint64_t slot = 0; slot < 8; slot++){
            uint8_t raw[64];
            for (uint64_t k = 0; k < 64; k++){
                raw[k] = buf[slot * 64 + k];
            }
            struct fs_file_entry *e = (struct fs_file_entry *)raw;
            if (e->id == 0 || e->parent_id != dir_id){
                continue;
            }

            uint64_t name_len = 0;
            while (name_len < 24 && e->name[name_len] != '\0'){
                name_len++;
            }

            uint64_t need = name_len + 1 + ((e->type == 3) ? 1 : 0) + 1;
            if (out + need > max_size){
                buffer[out] = '\0';
                return 0;
            }

            for (uint64_t k = 0; k < name_len; k++){
                buffer[out++] = e->name[k];
            }
            if (e->type == 3){
                buffer[out++] = '/';
            }
            buffer[out++] = '\n';
        }
    }

    if (out > 0 && buffer[out - 1] == '\n'){
        out--;
    }
    buffer[out] = '\0';
    return 0;
}

int fs_rm_file_by_id(const uint64_t entry_id){
    struct fs_file_entry e;
    uint64_t sec = 0, slot = 0;
    if (find_entry_by_id(entry_id, &e, &sec, &slot) != 0){
        return 1;
    }
    if (e.type != 2){
        return 2;
    }
    free_file_regions(&e);
    erase_table_entry(sec, slot);
    reload_last_id_to_disk();
    return 0;
}

int fs_rm_dir_by_id(const uint64_t entry_id){
    struct fs_file_entry e;
    uint64_t sec = 0, slot = 0;
    if (find_entry_by_id(entry_id, &e, &sec, &slot) != 0){
        return 1;
    }
    if (e.type != 3){
        return 2;
    }
    if (dir_has_children(entry_id)){
        return 1;
    }
    erase_table_entry(sec, slot);
    reload_last_id_to_disk();
    return 0;
}

int fs_rm_file(const char *path, uint64_t parent_id){
    uint64_t id = path_to_id((char *)path, parent_id);
    if (id == fs_not_found || id == 0){
        return 1;
    }
    return fs_rm_file_by_id(id);
}

int fs_rm_dir_inside(const char *path, uint64_t parent_id){
    uint64_t id = path_to_id((char *)path, parent_id);
    if (id == fs_not_found || id == 0){
        return 1;
    }
    return fs_rm_dir_by_id(id);
}

int fs_rm_dir(const char *path, uint64_t parent_id){
    return fs_rm_dir_inside(path, parent_id);
}

uint64_t read_from_fs_table_last_id(void){
    uint64_t res = 0;
    int T = 0;
    uint64_t i = 0;
    uint8_t this_buffer[512];
    while (T == 0){
        if (i >= fs_table_size_sectors){
            T = 1;
        }else{
            int stat = ata_sector_read((uint32_t)start_table+i, 1, this_buffer);
            if (stat == 0){
                for (uint64_t i3 = 0; i3 < 8; i3++){
                    uint8_t this_entry_raw[64];
                    for (uint64_t i2 = 0; i2 < 64; i2++){
                        this_entry_raw[i2] = this_buffer[i3*64 + i2];
                    }
                    struct fs_file_entry *this_entry = (struct fs_file_entry *)this_entry_raw;
                    if (this_entry->id > res){
                        res = this_entry->id;
                    }
                }
            }
            i++;
        }
    }
    return res;
}
uint64_t read_last_id_from_disk(void){
    uint8_t last_id[512];

    ata_sector_read((uint32_t)start_table-1, 1, last_id);

    return ((uint64_t)last_id[7] << 56)
                  | ((uint64_t)last_id[6] << 48)
                  | ((uint64_t)last_id[5] << 40)
                  | ((uint64_t)last_id[4] << 32)
                  | ((uint64_t)last_id[3] << 24)
                  | ((uint64_t)last_id[2] << 16)
                  | ((uint64_t)last_id[1] << 8)
                  | ((uint64_t)last_id[0]);

}
void reload_last_id_to_disk(void){
    uint8_t last_id[512];

    uint64_t last_id_num = read_from_fs_table_last_id();

    for (int i = 0; i < 8; i++){
        last_id[i] = (uint8_t)(last_id_num >> (i * 8)) & 0xFF;
    }

    ata_sector_write((uint32_t)start_table-1, 1, last_id);
}

#define fs_sb_version 1
#define fs_sb_payload_bytes 56

int fs_is_new = 0;

static void sb_put64(uint8_t *b, uint64_t off, uint64_t v){
    for (uint64_t i = 0; i < 8; i++){
        b[off + i] = (uint8_t)(v >> (i * 8));
    }
}

static uint64_t sb_get64(const uint8_t *b, uint64_t off){
    uint64_t v = 0;
    for (uint64_t i = 0; i < 8; i++){
        v |= ((uint64_t)b[off + i]) << (i * 8);
    }
    return v;
}

static uint64_t sb_checksum(const uint8_t *b){
    uint64_t h = 14695981039346656037ULL;
    for (uint64_t i = 0; i < fs_sb_payload_bytes; i++){
        h ^= b[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static int sb_magic_ok(const uint8_t *b){
    static const char magic[8] = {'S','I','M','P','L','E','F','S'};
    for (uint64_t i = 0; i < 8; i++){
        if (b[i] != (uint8_t)magic[i]){
            return 0;
        }
    }
    return 1;
}

static void fs_compute_layout(uint64_t total_sectors){
    sectors = total_sectors;
    fs_data_bitmap_size_sectors = (sectors / 8 / 8 + 511) / 512;
    start_table = fs_data_bitmap_size_sectors + 2;
    start_storage = start_table + fs_table_size_sectors;
}

static int fs_write_retry(uint32_t lba, uint8_t *buf){
    int r = 0;
    for (int attempt = 0; attempt < 5; attempt++){
        r = ata_sector_write(lba, 1, buf);
        if (r == 0){
            return 0;
        }
    }
    debug_print("fs: write failed at lba ");
    debug_put64((uint64_t)lba);
    if (r < 0){
        debug_print(" code -");
        debug_put64((uint64_t)(-r));
    }else{
        debug_print(" code ");
        debug_put64((uint64_t)r);
    }
    debug_putc('\n');
    return r;
}

static int fs_write_superblock(void){
    uint8_t sb[512];
    for (uint64_t i = 0; i < 512; i++){ sb[i] = 0x00; }

    static const char magic[8] = {'S','I','M','P','L','E','F','S'};
    for (uint64_t i = 0; i < 8; i++){ sb[i] = (uint8_t)magic[i]; }

    sb_put64(sb, 8,  fs_sb_version);
    sb_put64(sb, 16, sectors);
    sb_put64(sb, 24, fs_data_bitmap_size_sectors);
    sb_put64(sb, 32, start_table);
    sb_put64(sb, 40, start_storage);
    sb_put64(sb, 48, fs_table_size_sectors);
    sb_put64(sb, 56, sb_checksum(sb));

    return fs_write_retry(0, sb);
}

static int fs_load_superblock(uint64_t disk_sectors){
    uint8_t sb[512];
    if (ata_sector_read(0, 1, sb) != 0){
        return 0;
    }
    if (!sb_magic_ok(sb)){
        return 0;
    }
    if (sb_get64(sb, 56) != sb_checksum(sb)){
        return 0;
    }
    if (sb_get64(sb, 8) != fs_sb_version){
        return 0;
    }

    uint64_t sb_total   = sb_get64(sb, 16);
    uint64_t sb_bitmap  = sb_get64(sb, 24);
    uint64_t sb_table   = sb_get64(sb, 32);
    uint64_t sb_storage = sb_get64(sb, 40);
    uint64_t sb_tsecs   = sb_get64(sb, 48);

    if (sb_tsecs != fs_table_size_sectors){
        return 0;
    }

    if (sb_table != sb_bitmap + 2 || sb_storage != sb_table + sb_tsecs){
        return 0;
    }

    if (sb_total > disk_sectors || sb_storage >= sb_total){
        return 0;
    }

    sectors = sb_total;
    fs_data_bitmap_size_sectors = sb_bitmap;
    start_table = sb_table;
    start_storage = sb_storage;
    return 1;
}

static int fs_format(void){
    uint8_t zero_sector[512];
    for (uint64_t i = 0; i < 512; i++){ zero_sector[i] = 0x00; }

    if (fs_write_retry(0, zero_sector) != 0){
        return -1;
    }
    for (uint64_t i = 0; i < fs_data_bitmap_size_sectors; i++){
        if (fs_write_retry((uint32_t)(1 + i), zero_sector) != 0){
            return -1;
        }
    }
    for (uint64_t i = 0; i < fs_table_size_sectors; i++){
        if (fs_write_retry((uint32_t)(start_table + i), zero_sector) != 0){
            return -1;
        }
    }
    if (fs_write_retry((uint32_t)(start_table - 1), zero_sector) != 0){
        return -1;
    }

    return fs_write_superblock();
}

int fs_init(){
    uint16_t ident[256];
    for (uint64_t i = 0; i < 256; i++){ ident[i] = 0; }

    fs_is_new = 0;

    int id_stat = ata_identify(ident);
    uint64_t total = 0;
    if (id_stat == 1){
        uint64_t s48 =
              (uint64_t)ident[100]
            | ((uint64_t)ident[101] << 16)
            | ((uint64_t)ident[102] << 32)
            | ((uint64_t)ident[103] << 48);
        uint64_t s28 =
              (uint64_t)ident[60]
            | ((uint64_t)ident[61] << 16);
        total = (s48 != 0) ? s48 : s28;
    }

    if (total == 0){
        fs_compute_layout(0);
        debug_print("fs: no ATA disk found\n");
        return -1;
    }

    if (fs_load_superblock(total)){

        reload_last_id_to_disk();
        debug_print("fs: existing filesystem loaded\n");
        return 1;
    }

    fs_compute_layout(total);
    if (fs_format() != 0){
        debug_print("fs: format failed\n");
        return -1;
    }
    fs_is_new = 1;
    debug_print("fs: new disk formatted\n");
    return 0;
}