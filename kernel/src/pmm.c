#include <stdint.h>
#include <limine.h>
#include "pmm.h"
#include "debug.h"

#define FRAME_SIZE 4096

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

static uint8_t *bitmap;
static uint64_t bitmap_phys;
static uint64_t total_frame;
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

void pmm_init(void){
    uint64_t hi = 0;
    for (uint64_t i = 0;
        i < memmap_request.response->entry_count;
        i++) {

        struct limine_memmap_entry *entry =
            memmap_request.response->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE)
            continue;

        uint64_t end = entry->base + entry->length;

        if (end > hi)
            hi = end;
    }

    total_frame = (hi + FRAME_SIZE - 1) / FRAME_SIZE;
    bitmap_size = (total_frame + 7) / 8;

    for(uint64_t i = 0; i<memmap_request.response->entry_count; i++){
        struct limine_memmap_entry *this_entry = memmap_request.response->entries[i];
        if (this_entry->type == LIMINE_MEMMAP_USABLE){
            if (this_entry->length >= bitmap_size){
                bitmap_phys = this_entry->base;
                bitmap = (uint8_t *)(bitmap_phys + hhdm_request.response->offset);
                break;
            }
        }
    }

    for(uint64_t i = 0; i < bitmap_size; i++){
        bitmap[i] = 0xFF;
    }

    for (uint64_t i = 0; i<memmap_request.response->entry_count; i++){
        struct limine_memmap_entry *this_entry = memmap_request.response->entries[i];
        if (this_entry->type != LIMINE_MEMMAP_USABLE){
            continue;
        }
        uint64_t frame_count_in_region = this_entry->length / FRAME_SIZE;
        for (uint64_t i2 = 0; i2<frame_count_in_region; i2++){
            bitmap_clear(i2+(this_entry->base/FRAME_SIZE));
        }
    }
    for (uint64_t i3 = 0; i3<((bitmap_size + FRAME_SIZE -1) / FRAME_SIZE); i3++){
        bitmap_set(((uint64_t)bitmap_phys/FRAME_SIZE)+i3);
    }
    return;
}

uint64_t pmm_alloc_frame(void){
    for (uint64_t i = 0; i<total_frame; i++){
        if (bitmap_read(i) == 0){
            bitmap_set(i);
            return i * FRAME_SIZE;
        }
    }
    return 0;
}

void pmm_free_frame(uint64_t FA){
    bitmap_clear(FA/FRAME_SIZE);
}