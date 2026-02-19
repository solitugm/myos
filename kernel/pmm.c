#include <stdint.h>
#include "pmm.h"
#include "mb2.h"
#include "console.h"

#define PAGE_SIZE 4096
#define MAX_MEM   (128 * 1024 * 1024)
#define MAX_PAGES (MAX_MEM / PAGE_SIZE)

static uint32_t bitmap[MAX_PAGES / 32];
static uint32_t total_pages = MAX_PAGES;
static uint32_t free_pages = 0;
static uint32_t first_page = 0;
static uint32_t double_free_cnt = 0;

static inline void set_bit(uint32_t i) { bitmap[i >> 5] |= (1u << (i & 31)); }
static inline void clear_bit(uint32_t i) { bitmap[i >> 5] &= ~(1u << (i & 31)); }
static inline int test_bit(uint32_t i) { return (bitmap[i >> 5] >> (i & 31)) & 1u; }

static void print_hex32(uint32_t v) {
    const char* hex = "0123456789ABCDEF";
    console_puts("0x");
    for (int i = 7; i >= 0; i--) console_putc(hex[(v >> (i * 4)) & 0xF]);
}

static void print_u32(uint32_t v) {
    char buf[16];
    int i = 0;

    if (v == 0) {
        console_putc('0');
        return;
    }

    while (v && i < (int)sizeof(buf)) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }

    while (i--) console_putc(buf[i]);
}

static void mark_all_used(void) {
    for (uint32_t i = 0; i < MAX_PAGES / 32; i++) bitmap[i] = 0xFFFFFFFFu;
    free_pages = 0;
}

static void mark_range_free(uint32_t start, uint32_t end) {
    uint32_t p0 = (start + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t p1 = end / PAGE_SIZE;
    if (p1 > MAX_PAGES) p1 = MAX_PAGES;

    for (uint32_t p = p0; p < p1; p++) {
        if (test_bit(p)) {
            clear_bit(p);
            free_pages++;
        }
    }
}

static void mark_range_used(uint32_t start, uint32_t end) {
    uint32_t p0 = start / PAGE_SIZE;
    uint32_t p1 = (end + PAGE_SIZE - 1) / PAGE_SIZE;
    if (p1 > MAX_PAGES) p1 = MAX_PAGES;

    for (uint32_t p = p0; p < p1; p++) {
        if (!test_bit(p)) {
            set_bit(p);
            if (free_pages > 0) free_pages--;
        }
    }
}

void pmm_init(uint32_t mb2_info_addr, uint32_t kernel_end_phys) {
    mark_all_used();
    double_free_cnt = 0;

    const mb2_mmap_tag_t* mmap = mb2_find_mmap(mb2_info_addr);
    if (!mmap) return;

    for (uint32_t off = 0; off < mmap->size - sizeof(*mmap); off += mmap->entry_size) {
        const mb2_mmap_entry_t* e = (const mb2_mmap_entry_t*)((uint8_t*)mmap->entries + off);
        if (e->type != 1) continue;

        uint64_t start = e->addr;
        uint64_t end = e->addr + e->len;

        if (start >= MAX_MEM) continue;
        if (end > MAX_MEM) end = MAX_MEM;

        mark_range_free((uint32_t)start, (uint32_t)end);
    }

    uint32_t kend = (kernel_end_phys + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    mark_range_used(0, kend);

    uint32_t b0 = (uint32_t)bitmap & ~(PAGE_SIZE - 1);
    uint32_t b1 = ((uint32_t)bitmap + sizeof(bitmap) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    mark_range_used(b0, b1);

    uint32_t fp1 = kend / PAGE_SIZE;
    uint32_t fp2 = b1 / PAGE_SIZE;
    first_page = (fp1 > fp2) ? fp1 : fp2;

    console_puts("[mem] pmm bitmap=");
    print_hex32((uint32_t)bitmap);
    console_puts(" first_page=");
    print_u32(first_page);
    console_putc('\n');
}

uint32_t pmm_alloc_page(void) {
    for (uint32_t p = first_page; p < MAX_PAGES; p++) {
        if (!test_bit(p)) {
            set_bit(p);
            if (free_pages > 0) free_pages--;
            return p * PAGE_SIZE;
        }
    }
    return 0;
}

void pmm_free_page(uint32_t addr) {
    uint32_t p = addr / PAGE_SIZE;
    if (p >= MAX_PAGES || p < first_page) return;

    if (!test_bit(p)) {
        double_free_cnt++;
        return;
    }

    clear_bit(p);
    free_pages++;
}

uint32_t pmm_alloc_contiguous(uint32_t pages) {
    if (pages == 0) return 0;

    uint32_t run = 0;
    uint32_t start = 0;

    for (uint32_t p = first_page; p < MAX_PAGES; p++) {
        if (!test_bit(p)) {
            if (run == 0) start = p;
            run++;
            if (run == pages) {
                for (uint32_t i = 0; i < pages; i++) set_bit(start + i);
                if (free_pages >= pages) free_pages -= pages;
                else free_pages = 0;
                return start * PAGE_SIZE;
            }
        } else {
            run = 0;
        }
    }

    return 0;
}

void pmm_free_contiguous(uint32_t addr, uint32_t pages) {
    if (!addr || pages == 0) return;

    uint32_t start = addr / PAGE_SIZE;
    if (start + pages > MAX_PAGES) return;

    for (uint32_t i = 0; i < pages; i++) {
        uint32_t p = start + i;
        if (p < first_page) continue;

        if (!test_bit(p)) {
            double_free_cnt++;
            continue;
        }

        clear_bit(p);
        free_pages++;
    }
}

uint32_t pmm_total_pages(void) { return total_pages; }
uint32_t pmm_free_pages(void) { return free_pages; }
uint32_t pmm_first_alloc_page(void) { return first_page; }
uint32_t pmm_double_free_count(void) { return double_free_cnt; }

int pmm_is_page_used(uint32_t addr) {
    uint32_t p = addr / PAGE_SIZE;
    if (p >= MAX_PAGES) return 1;
    return test_bit(p);
}

void pmm_dump_summary(void) {
    console_puts("[mem] pmm free/total=");
    print_u32(free_pages);
    console_putc('/');
    print_u32(total_pages);
    console_puts(" double_free=");
    print_u32(double_free_cnt);
    console_putc('\n');
}
