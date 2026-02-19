#pragma once
#include <stdint.h>

int fs_init(void);
int fs_list(void);
int fs_read_file(const char* name, void* buf, uint32_t maxlen, uint32_t* out_len);
int fs_write_file(const char* name, const void* buf, uint32_t len);
int fs_delete_file(const char* name);
int fs_rename_file(const char* old_name, const char* new_name);
int fs_get_file_size(const char* name, uint32_t* out_size);
