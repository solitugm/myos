#include <stdint.h>
#include "pmm.h"
#include "mb2.h"

#define PAGE_SIZE 4096
#define MAX_MEM   (128 * 1024 * 1024)   // 일단 128MB까지만(너무 크게 잡지 말자)
#define MAX_PAGES (MAX_MEM / PAGE_SIZE)

static uint32_t bitmap[MAX_PAGES / 32];
static uint32_t total_pages = MAX_PAGES;
static uint32_t free_pages = 0;

static inline void set_bit(uint32_t i)   { bitmap[i>>5] |=  (1u<<(i&31)); }
static inline void clear_bit(uint32_t i) { bitmap[i>>5] &= ~(1u<<(i&31)); }
static inline int  test_bit(uint32_t i)  { return (bitmap[i>>5] >> (i&31)) & 1u; }

static void mark_all_used(void) {
    for (uint32_t i=0;i<MAX_PAGES/32;i++) bitmap[i]=0xFFFFFFFFu;
    free_pages = 0;
}

static void mark_range_free(uint32_t start, uint32_t end) {
    // [start, end) physical bytes
    uint32_t p0 = (start + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t p1 = end / PAGE_SIZE;
    if (p1 > MAX_PAGES) p1 = MAX_PAGES;

    for (uint32_t p=p0; p<p1; p++) {
        if (test_bit(p)) { clear_bit(p); free_pages++; }
    }
}

static void mark_range_used(uint32_t start, uint32_t end) {
    uint32_t p0 = start / PAGE_SIZE;
    uint32_t p1 = (end + PAGE_SIZE - 1) / PAGE_SIZE;
    if (p1 > MAX_PAGES) p1 = MAX_PAGES;

    for (uint32_t p=p0; p<p1; p++) {
        if (!test_bit(p)) { set_bit(p); free_pages--; }
    }
}

void pmm_init(uint32_t mb2_info_addr, uint32_t kernel_end_phys) {
    mark_all_used();

    const mb2_mmap_tag_t* mmap = mb2_find_mmap(mb2_info_addr);
    if (!mmap) return;

    // usable(=1) 영역만 free로 풀기
    for (uint32_t off=0; off < mmap->size - sizeof(*mmap); off += mmap->entry_size) {
        const mb2_mmap_entry_t* e = (const mb2_mmap_entry_t*)((uint8_t*)mmap->entries + off);
        if (e->type != 1) continue;

        uint64_t start = e->addr;
        uint64_t end   = e->addr + e->len;

        if (start >= MAX_MEM) continue;
        if (end > MAX_MEM) end = MAX_MEM;

        mark_range_free((uint32_t)start, (uint32_t)end);
    }

    // 커널 영역은 다시 used로 잠금
    mark_range_used(0, kernel_end_phys);

    // 0페이지(Null pointer 방지)도 잠그자
    mark_range_used(0, PAGE_SIZE);
}

uint32_t pmm_alloc_page(void) {
    for (uint32_t p=0; p<MAX_PAGES; p++) {
        if (!test_bit(p)) {
            set_bit(p);
            free_pages--;
            return p * PAGE_SIZE;
        }
    }
    return 0; // out of memory
}

void pmm_free_page(uint32_t addr) {
    uint32_t p = addr / PAGE_SIZE;
    if (p >= MAX_PAGES) return;
    if (test_bit(p)) {
        clear_bit(p);
        free_pages++;
    }
}

uint32_t pmm_total_pages(void) { return total_pages; }
uint32_t pmm_free_pages(void)  { return free_pages; }