#include <stdint.h>
#include "mb2.h"

static inline uint32_t align8(uint32_t x) { return (x + 7) & ~7u; }

const mb2_mmap_tag_t* mb2_find_mmap(uint32_t mb2_info_addr) {
    const mb2_info_t* info = (const mb2_info_t*)mb2_info_addr;

    uint32_t off = 8; // skip total_size/reserved
    while (off < info->total_size) {
        const mb2_tag_t* tag = (const mb2_tag_t*)(mb2_info_addr + off);
        if (tag->type == MB2_TAG_END) break;
        if (tag->type == MB2_TAG_MMAP) return (const mb2_mmap_tag_t*)tag;
        off += align8(tag->size);
    }
    return 0;
}