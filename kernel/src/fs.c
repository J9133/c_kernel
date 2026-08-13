#include <stdint.h>
#include <limine.h>
#include "fs.h"
#include "pmm.h"
#include "debug.h"

#define FRAME_SIZE 4096

#define FS_TABLE_FRAMES 16
#define FS_ENTRY_SIZE 72
#define FS_MAX_FILES 1024
#define FS_STORAGE_FRAMES 4096

uint64_t fs_file_count;
static uint64_t file_table_frames[FS_TABLE_FRAMES];

struct fs_file_entry {
    char     name[32];       // 32 bytes
    uint64_t size;           // 8
    uint64_t parent_id;      // 8
    uint64_t id;             // 8
    uint32_t type;           // 4
    uint32_t flags;          // 4
    uint64_t data_frame;     // 8
};


static uint8_t *bitmap;
static uint64_t bitmap_total_bytes;
static uint64_t bitmap_size;

static void bitmap_set(uint64_t fream_index){
    bitmap[fream_index / 8] |= (1 << (fream_index % 8));
}

static void bitmap_clear(uint64_t fream_index){
    bitmap[fream_index / 8] &= ~(1 << (fream_index % 8));
}

static int bitmap_read(uint64_t fream_index){
    return bitmap[fream_index / 8] & (1 << (fream_index % 8));
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
    uint64_t iout = 0;
    for(uint64_t i = 0; i<counter; i++){
        if (path[i] == '/'){
            iout = i;
        }
    }
    static char name[32];
    for(uint64_t i = 0; i<(counter-iout); i++){
        char for_char = path[iout+i+1];
        name[i] = for_char;
    }
    return name;
}

void Mstrcpy(char *dest, const char *src){
    uint64_t i = 0;
    while (src[i] != '\0'){
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void fs_bitmap_init(void){
    bitmap_total_bytes = FS_MAX_FILES / 8;
    uint64_t phys = pmm_alloc_frame();
    bitmap = (uint8_t *)pmm_phys_to_virt(phys);
    for (uint64_t i = 0; i < bitmap_total_bytes; i++){
        bitmap[i] = 0x00;
    }
}

uint64_t get_last(){
    for (uint64_t i = 0; i < bitmap_total_bytes*8; i++){
        for(uint8_t i2 = 0; i < 8; i2++){
            uint64_t hi = (i*8)+(i2);
            int hif = bitmap_read(hi);
            if (hif == 0){
                return hi;
            }
        }
    }
    return 0;
}

struct fs_file_entry *add_item(uint64_t ag1){
    uint64_t offset = (ag1 % (FRAME_SIZE / FS_ENTRY_SIZE)) * FS_ENTRY_SIZE;
    uint64_t frame_index = ag1 / (FRAME_SIZE / FS_ENTRY_SIZE);
    offset = (ag1 % (FRAME_SIZE / FS_ENTRY_SIZE)) * FS_ENTRY_SIZE;
    uint8_t *entry = (uint8_t *)pmm_phys_to_virt(file_table_frames[frame_index]) + offset;
    for (uint64_t i = 0; i<FS_ENTRY_SIZE; i++){
        entry[i] = 0x00;
    }
    return (struct fs_file_entry *)entry;
}

void fs_init(){
    for (uint64_t i = 0; i < FS_TABLE_FRAMES; i++){
        file_table_frames[i] = pmm_alloc_frame();
    }
    
    fs_bitmap_init();
}

uint64_t fs_alloc_frame(void){
    for (uint64_t i = 0; i<bitmap_total_bytes; i++){
        if (bitmap_read(i) == 0){
            bitmap_set(i);
            return i * FRAME_SIZE;
        }
    }
    return 0;
}

int fs_mk_file(const char *path, uint64_t parent_id){
    uint64_t bit_place = get_last();
    bitmap_set(bit_place);
    struct fs_file_entry *entry = add_item(bit_place);
    char *name = path_to_name((char *)path);
    Mstrcpy(entry->name, name);
    entry->size = 1;
    entry->parent_id = parent_id;
    entry->id = bit_place;
    entry->type = 1;
    entry->data_frame = fs_alloc_frame();
    //uint8_t *rr = (uint8_t *)entry;
    //for (int i = 0; i<72; i++){
    //    debug_putc((char)rr[i]);
    //}
    uint8_t *rr = (uint8_t *)entry+32;
    for (int i = 0; i<8; i++){
        debug_putc((char)rr[i]);
    }
    return 0;
}
