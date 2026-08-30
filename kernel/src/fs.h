#ifndef FS_H
#define FS_H

#include <stdint.h>

void fs_init(void);

int fs_write_file(const char *path, const void *data, uint64_t size);

int fs_read_file(const char *path, uint8_t *buffer, uint64_t max_size);
int fs_ls_dir(const char *path, char *buffer, uint64_t max_size, uint64_t current_dir_id);

int fs_mk_file(const char *path, uint64_t parent_id);
int fs_mk_dir(const char *path, uint64_t parent_id);

int fs_rm_file(const char *path, uint64_t parent_id);
int fs_rm_dir(const char *path, uint64_t parent_id);

uint64_t path_to_id(char *path, uint64_t current_dir_id);
int id_to_path(uint64_t id, char *buffer);
char *path_resolve(char *path);
char *path_to_abs(char *path, uint64_t current_dir_id);

#endif