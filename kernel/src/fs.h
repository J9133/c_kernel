#ifndef FS_H
#define FS_H

#include <stdint.h>

int fs_init(void);

int fs_write_file(const char *path, const void *data, uint64_t size, uint64_t current_dir_id);

int fs_read_file(const char *path, uint8_t *buffer, uint64_t max_size, uint64_t current_dir_id);
int fs_ls_dir(const char *path, char *buffer, uint64_t max_size, uint64_t current_dir_id);

int fs_mk_file(const char *path, uint64_t parent_id);
int fs_mk_dir(const char *path, uint64_t parent_id);

int fs_rm_file(const char *path, uint64_t parent_id);
int fs_rm_dir(const char *path, uint64_t parent_id);

uint64_t path_to_id(char *path, uint64_t current_dir_id);
int id_to_path(uint64_t id, char *buffer);
char *path_resolve(char *path);
char *path_to_abs(char *path, uint64_t current_dir_id);

uint64_t path_to_parent_id(char *path, uint64_t current_dir_id);
uint64_t name_and_parent_id_to_id(const char *name, uint64_t parent_id);
uint64_t read_from_fs_table_last_id(void);
uint64_t read_last_id_from_disk(void);
void reload_last_id_to_disk(void);

#endif
