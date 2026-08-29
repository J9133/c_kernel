#include <stdint.h>
#include <limine.h>
#include "somthings.h"
#include "pmm.h"
#include "debug.h"

void fs_strncpy(char *dest, const char *src, uint64_t max){
    uint64_t i = 0;
    if (max != 0){
        while (i < max && src[i] != '\0'){
            dest[i] = src[i];
            i++;
        }
        while (i < max){
            dest[i] = '\0';
            i++;
        }
    }else{
        while (src[i] != '\0'){
            dest[i] = src[i];
            i++;
        }
        dest[i] = '\0';
    }
}

int fs_strcmp(char *rdi, char *rsi, uint64_t max){
    for(uint64_t i = 0; i < max; i++){
        uint8_t rdi_char = rdi[i];
        uint8_t rsi_char = rsi[i];
        if (rdi_char == '\0' && rsi_char == '\0'){
            return 2;
        }
        if (rdi_char != rsi_char){
            return 0;
        }
    }
    return 1;
}