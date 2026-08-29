#ifndef SOMETHINGS_H
#define SOMETHINGS_H

#include <stdint.h>

void fs_strncpy(char *dest, const char *src, uint64_t max);

int fs_strcmp(char *rdi, char *rsi, uint64_t max);

#endif