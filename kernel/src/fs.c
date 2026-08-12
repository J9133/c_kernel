#include <stdint.h>
#include <limine.h>
#include "fs.h"
#include "pmm.h"
#include "debug.h"

#define FRAME_SIZE 4096

#define FS_TABLE_FRAMES 16
#define FS_ENTRY_SIZE 64
#define FS_MAX_FILES 1024

uint64_t fs_file_count;
static uint64_t file_table_frames[FS_TABLE_FRAMES];

struct fs_file_entry {
    char     name[32];       // 32 bytes
    uint64_t size;           // 8
    uint32_t parent_id;      // 4
    uint32_t id;             // 4
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

void fs_bitmap_init(void){
    bitmap_total_bytes = FS_MAX_FILES * FS_ENTRY_SIZE;
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

void add_item(uint64_t ag1){
    uint64_t offset = (ag1 % (FRAME_SIZE / FS_ENTRY_SIZE)) * FS_ENTRY_SIZE;
    uint64_t frame_index = ag1 / (FRAME_SIZE / FS_ENTRY_SIZE);
    offset = (ag1 % (FRAME_SIZE / FS_ENTRY_SIZE)) * FS_ENTRY_SIZE;
    uint8_t *entry = (uint8_t *)file_table_frames[frame_index] + offset;
    for (uint64_t i = 0; i<FS_ENTRY_SIZE; i++){
        entry[i] = 0x00;
    }

}
void fs_init(){
    for (uint64_t i = 0; i < FS_TABLE_FRAMES; i++){
        file_table_frames[i] = pmm_alloc_frame();
    }
    fs_bitmap_init();
}

int fs_mk_file(const char *path){
    uint64_t bit_place = get_last();
    bitmap_set(bit_place);
    add_item(bit_place);
}