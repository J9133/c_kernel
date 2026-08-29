#include <stdint.h>
#include <limine.h>
#include <stddef.h>
#include "fs.h"
#include "pmm.h"
#include "debug.h"
#include "somthings.h"

#define frame_size_bytes 4096

#define fs_max_files_entrys 1024
#define fs_size_frames (fs_max_files_entrys)

#define fs_table_entry_size_bytes 72

#define fs_table_size_entrys (fs_max_files_entrys)
#define fs_table_size_bytes (fs_table_size_entrys * fs_table_entry_size_bytes)

uint64_t fs_not_found = 0-1;

uint64_t file_counter = 0;

uint8_t *fs[fs_size_frames];
uint8_t fs_table[fs_table_size_bytes];

// fs_bitmap

#define fs_bitmap_size_bytes (fs_max_files_entrys / 8)
#define fs_bitmap_size_entrys (fs_max_files_entrys)

uint8_t fs_bitmap[fs_bitmap_size_bytes];

struct fs_file_entry {
    char     name[32];       // 32 bytes
    uint64_t size;           // 8
    uint64_t parent_id;      // 8
    uint64_t id;             // 8
    uint32_t type;           // 4
    uint32_t flags;          // 4
    uint64_t data_frame;     // 8
    };


static void bitmap_set(uint8_t *this_bitmap, uint64_t fream_index){
    this_bitmap[fream_index / 8] |= (1 << (fream_index % 8));
}

static void bitmap_clear(uint8_t *this_bitmap, uint64_t fream_index){
    this_bitmap[fream_index / 8] &= ~(1 << (fream_index % 8));
}

static int bitmap_read(uint8_t *this_bitmap, uint64_t fream_index){
    return this_bitmap[fream_index / 8] & (1 << (fream_index % 8));
}

struct fs_file_entry *get_table_entry(){
    uint64_t entry_index = 0;
    for (uint64_t i = 0; i < fs_bitmap_size_entrys; i++){
        int this_stat = bitmap_read(fs_bitmap, i);
        if (!this_stat){
            bitmap_set(fs_bitmap, i);
            entry_index = i;
            return (struct fs_file_entry *)(fs_table + entry_index * fs_table_entry_size_bytes);
            break;
        }
    }
    return NULL;
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
    static char name[32];
    uint64_t name_len = counter - (iout + 1);
    for(uint64_t i = 0; i<name_len; i++){
        name[i] = path[iout+1+i];
    }
    name[name_len] = '\0';
    return name;
}

int rel_or_abs_path(char *path){
    //int T = 0;
    //uint64_t path_lenght_1 = 0;
    //while (T == 0){
    //    if (path[path_lenght_1] == '\0'){
    //        T = 1;
    //    }else{
    //        path_lenght_1++;
    //    }
    //}
    if(path[0] == '/'){
        return 1;
    }else{
        return 0;
    }
}

uint64_t path_to_parent_id(char *path, uint64_t current_dir_id){
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
    int T = 0;
    uint64_t path_lenght = 0;
    uint64_t names_count_in_path = 0;
    char Vpath_buf[1024];
    char *Vpath = Vpath_buf;
    for (uint64_t i = 0; i < 1024; i++){
        Vpath_buf[i] = 0x00;
    }
    while(T == 0){
        if (path[path_lenght] == '\0'){
            Vpath[path_lenght] = '\0';
            T = 1;
            continue;
        }else{
            if (path[path_lenght] == '/'){
                names_count_in_path++;
                Vpath[path_lenght] = '\0';
            }else{
                Vpath[path_lenght] = path[path_lenght];
            }
        }
        path_lenght++;
    }
    char *names_in_path[names_count_in_path + 1];
    
    if (names_count_in_path == 0){
        if (rel_or_abs_path(path) == 0){
            return current_dir_id;
        }else{
            return 0;
        }
        return fs_not_found;
    }

    uint64_t idx = 0;
    int new_word = 1;

    for (uint64_t i = 0; i < path_lenght; i++){
        if (Vpath[i] == '\0'){
            new_word = 1;
        }else{
            if (new_word){
                names_in_path[idx] = &Vpath[i];
                idx++;
                new_word = 0;
            }
        }
    }

    uint64_t current_parent_id = 0;
    if (rel_or_abs_path(path) == 0){
        current_parent_id = current_dir_id;
    }
    int found = 0;

    for (uint64_t i = 0; i < idx-1; i++){
        found = 0;
        for (uint64_t i2 = 0; i2 < fs_table_size_bytes; i2+=fs_table_entry_size_bytes){
            struct fs_file_entry *this_entry = (struct fs_file_entry *)&fs_table[i2];
            int stat = fs_strcmp(this_entry->name, names_in_path[i], 32);
            if (stat != 0 && this_entry->type == 2 && this_entry->parent_id == current_parent_id){
                current_parent_id = this_entry->id;
                found = 1;
                break; 
            }
        }
        if(!found){
            return fs_not_found;
        }
    }

    return current_parent_id;
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
        if (rel_or_abs_path(path) == 0){
            return current_dir_id;
        }else{
            return 0;
        }
        //return fs_not_found;
    }

    //uint64_t parent_id;
    //if (rel_or_abs_path(path)){
    uint64_t parent_id = path_to_parent_id(path, current_dir_id);
    //}else{
    //    parent_id = current_dir_id;
    //}
    char *last_name = argvs[idx -1];

    for (uint64_t i2 = 0; i2 < fs_table_size_bytes; i2+=fs_table_entry_size_bytes){
        struct fs_file_entry *this_entry = (struct fs_file_entry *)&fs_table[i2];
        int stat = fs_strcmp(this_entry->name, last_name, 32);
        if (stat != 0 && this_entry->parent_id == parent_id){
            return this_entry->id;
        }
    }
    return fs_not_found;
}

int fs_mk_file(const char *path, uint64_t parent_id){
    if (file_counter>=fs_max_files_entrys){
        return 2;
    }
    struct fs_file_entry *entry = get_table_entry();
    if (entry == NULL){
        return 3;
    }
    uint64_t entry_index = ((uint8_t *)entry - fs_table) / fs_table_entry_size_bytes;
    fs_strncpy(entry->name, path_to_name(path), sizeof(entry->name));
    uint64_t target_id = path_to_parent_id(path, parent_id);
    if (target_id == fs_not_found){
        return 1;
    }
    if (rel_or_abs_path(path) == 0){
        entry->parent_id = parent_id;
    }else{    
        entry->parent_id = target_id;
    }
    entry->size = 1;
    entry->id = entry_index;
    entry->type = 1;
    entry->flags = 0;
    entry->data_frame = entry_index;
    file_counter++;
    return 0;
}

int fs_mk_dir(const char *path, uint64_t parent_id){
    if (file_counter>=fs_max_files_entrys){
        return 3;
    }
    struct fs_file_entry *entry = get_table_entry();
    if (entry == NULL){
        return 2;
    }
    uint64_t entry_index = ((uint8_t *)entry - fs_table) / fs_table_entry_size_bytes;
    fs_strncpy(entry->name, path_to_name(path), sizeof(entry->name));
    uint64_t target_id = path_to_parent_id(path, parent_id);
    if (target_id == fs_not_found){
        return 1;
    }
    if (rel_or_abs_path(path) == 0){
        entry->parent_id = parent_id;
    }else{    
        entry->parent_id = target_id;
    }
    entry->size = 0;
    entry->id = entry_index;
    entry->type = 2;
    entry->flags = 0;
    entry->data_frame = entry_index;
    file_counter++;
    return 0;}

int fs_write_file(const char *path, const void *data, uint64_t size){
    for (uint64_t i = 0; i < fs_table_size_bytes; i+=fs_table_entry_size_bytes){
        struct fs_file_entry *this_entry = (struct fs_file_entry *)&fs_table[i];
        int stat = fs_strcmp(this_entry->name, path_to_name(path), 32);
        if (stat != 0 && this_entry->type == 1){
            fs_strncpy(fs[this_entry->data_frame], data, size);
            return 0;
        }
    }
    return 1;}
int fs_read_file(const char *path, uint8_t *buffer, uint64_t max_size){
    for (uint64_t i = 0; i < fs_table_size_bytes; i+=fs_table_entry_size_bytes){
        struct fs_file_entry *this_entry = (struct fs_file_entry *)&fs_table[i];
        int stat = fs_strcmp(this_entry->name, path_to_name(path), 32);
        if (stat != 0){
            if(max_size == 0){
                max_size = (this_entry->size)*frame_size_bytes;
            }
            for(uint64_t i2 = 0; i2 < max_size; i2++){
                if (fs[this_entry->data_frame][i2] != '\0'){
                    buffer[i2] = fs[this_entry->data_frame][i2];
                }
            }
            return 0;
        }
    }
    return 1;}

int fs_ls_dir(const char *path, char *buffer, uint64_t max_size, uint64_t current_dir_id){

    for (uint64_t i = 0; i < max_size; i++){
        buffer[i] = 0x00;
    }
    uint64_t output_poiter_buffer = 0;

    uint64_t target_id = path_to_id((char *)path, current_dir_id);

    if (target_id == fs_not_found){
        return 1;
    }

    for (uint64_t i = 0; i < fs_table_size_bytes; i += fs_table_entry_size_bytes){
        struct fs_file_entry *this_entry = (struct fs_file_entry *)&fs_table[i];
        if (bitmap_read(fs_bitmap, i/fs_table_entry_size_bytes)){
            if (this_entry->parent_id == target_id){
                int this_path_lenght = 0;
                for (int i2 = 0; i2 < 32; i2++){
                    if (this_entry->name[i2] == '\0'){
                        break;
                    }
                    this_path_lenght++;
                }
                for (int i2 = 0; i2 < this_path_lenght; i2++){
                    buffer[output_poiter_buffer+i2] = this_entry->name[i2];
                }
                buffer[output_poiter_buffer+this_path_lenght] = '\n';
                output_poiter_buffer += this_path_lenght + 1;
            }
        }
    }

    //for(uint64_t i = 0; i < max_size; i++){  
    //    if (output[i] == '\0'){break;}
    //    buffer[i] = output[i];
    //}
    
    return 0;
}

int fs_rm_file(const char *path, uint64_t parent_id){
    uint64_t entry_id = path_to_id((char *)path, parent_id);
    for (uint64_t i = 0; i < fs_table_size_bytes; i+=fs_table_entry_size_bytes){
        struct fs_file_entry *this_entry = (struct fs_file_entry *)&fs_table[i];
        if(this_entry->id == entry_id){
            if (this_entry->type != 1){
                return 2;
            }
            for (uint64_t i2 = 0; i2 < frame_size_bytes; i2++){
                fs[this_entry->data_frame][i2] = 0x00;
            }
            for (uint64_t i2 = 0; i2 < fs_table_entry_size_bytes; i2++){
                fs_table[i+i2] = 0x00;
            }
            bitmap_clear(fs_bitmap, entry_id);
            return 0;
        }
    }
    return 1;
}

int fs_rm_file_by_id(const uint64_t entry_id){
    for (uint64_t i = 0; i < fs_table_size_bytes; i+=fs_table_entry_size_bytes){
        struct fs_file_entry *this_entry = (struct fs_file_entry *)&fs_table[i];
        if(this_entry->id == entry_id){
            if (this_entry->type != 1){
                return 2;
            }
            for (uint64_t i2 = 0; i2 < frame_size_bytes; i2++){
                fs[this_entry->data_frame][i2] = 0x00;
            }
            for (uint64_t i2 = 0; i2 < fs_table_entry_size_bytes; i2++){
                fs_table[i+i2] = 0x00;
            }
            bitmap_clear(fs_bitmap, entry_id);
            return 0;
        }
    }
    return 1;
}

int fs_rm_dir_by_id(const uint64_t entry_id){
    for (uint64_t i = 0; i < fs_table_size_bytes; i+=fs_table_entry_size_bytes){
        struct fs_file_entry *this_entry = (struct fs_file_entry *)&fs_table[i];
        if(this_entry->id == entry_id){
            if (this_entry->type != 2){
                return 2;
            }
            for (uint64_t i2 = 0; i2 < fs_table_entry_size_bytes; i2++){
                fs_table[i+i2] = 0x00;
            }
            bitmap_clear(fs_bitmap, entry_id);
            return 0;
        }
    }
    return 1;
}

int fs_rm_dir_inside(const char *path, uint64_t parent_id){
    uint64_t this_dir_entry_id = path_to_id((char *)path, parent_id);
    if (this_dir_entry_id == fs_not_found){
        return 1;
    }   
    for (uint64_t i = 0; i < fs_table_size_bytes; i+=fs_table_entry_size_bytes){
            if (!bitmap_read(fs_bitmap, i / fs_table_entry_size_bytes)){
                continue;
            }
        struct fs_file_entry *this_entry = (struct fs_file_entry *)&fs_table[i];
        if (this_entry->parent_id == this_dir_entry_id){
            if (this_entry->type == 2){
                fs_rm_dir_inside(this_entry->name, this_dir_entry_id);
                fs_rm_dir_by_id(this_entry->id);
                return 0;
            }else{
                fs_rm_file_by_id(this_entry->id);
                return 0;
            }
        }
    }
    return 1;
}

int fs_rm_dir(const char *path, uint64_t parent_id){
    uint64_t this_dir_entry_id = path_to_id((char *)path, parent_id);
    if (this_dir_entry_id == fs_not_found){
        return 1;
    }
    int this_stat = fs_rm_dir_inside(path, parent_id);
    fs_rm_dir_by_id(this_dir_entry_id);
    return this_stat;
}

void fs_init(void){
    for (uint64_t i = 0; i < fs_size_frames; i++){
        fs[i] = (uint8_t *)pmm_phys_to_virt(pmm_alloc_frame());
    }
    for (uint64_t i = 0; i < fs_table_size_bytes; i++){
        fs_table[i] = 0x00;
    }
    for (uint64_t i = 0; i < fs_bitmap_size_bytes; i++){
        fs_bitmap[i] = 0x00;
    }
    bitmap_set(fs_bitmap, 0);
}