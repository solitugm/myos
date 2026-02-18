#pragma once
#include <stdint.h>

#define MB2_TAG_END      0
#define MB2_TAG_MMAP     6

typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} mb2_info_t;

typedef struct {
    uint32_t type;
    uint32_t size;
} mb2_tag_t;

typedef struct {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} __attribute__((packed)) mb2_mmap_entry_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    mb2_mmap_entry_t entries[];
} mb2_mmap_tag_t;

const mb2_mmap_tag_t* mb2_find_mmap(uint32_t mb2_info_addr);