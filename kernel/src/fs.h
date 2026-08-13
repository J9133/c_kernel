#ifndef FS_H
#define FS_H

#include <stdint.h>

void fs_init(void);

int fs_write_file(const char *path, const void *data, uint64_t size);

int fs_read_file(const char *path, void *buffer, uint64_t max_size);
int fs_ls_dir(const char *path, char *buffer, uint64_t max_size);

int fs_mk_dir(const char *path);
int fs_mk_file(const char *path, uint64_t parent_id);

#endif