#pragma once
#include <stdint.h>

int fs_init(void);
int fs_list(void);
int fs_read_file(const char* name, void* buf, uint32_t maxlen, uint32_t* out_len);
