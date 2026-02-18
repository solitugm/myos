#include <stdint.h>
#include "pmm.h"
#include "mb2.h"
#include "console.h"

#define PAGE_SIZE 4096
#define MAX_MEM   (128 * 1024 * 1024)   // 일단 128MB까지만(너무 크게 잡지 말자)
#define MAX_PAGES (MAX_MEM / PAGE_SIZE)

static uint32_t bitmap[MAX_PAGES / 32];
static uint32_t total_pages = MAX_PAGES;
static uint32_t free_pages = 0;
static uint32_t first_page = 0;   // 여기부터만 할당을 허용

static inline void set_bit(uint32_t i)   { bitmap[i>>5] |=  (1u<<(i&31)); }
static inline void clear_bit(uint32_t i) { bitmap[i>>5] &= ~(1u<<(i&31)); }
static inline int  test_bit(uint32_t i)  { return (bitmap[i>>5] >> (i&31)) & 1u; }

static void print_hex32(uint32_t v) {
    const char* hex = "0123456789ABCDEF";
    console_puts("0x");
    for (int i = 7; i >= 0; i--) {
        console_putc(hex[(v >> (i * 4)) & 0xF]);
    }
}

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
    for (uint32_t off = 0; off < mmap->size - sizeof(*mmap); off += mmap->entry_size) {
        const mb2_mmap_entry_t* e =
            (const mb2_mmap_entry_t*)((uint8_t*)mmap->entries + off);
        if (e->type != 1) continue;

        uint64_t start = e->addr;
        uint64_t end   = e->addr + e->len;

        if (start >= MAX_MEM) continue;
        if (end > MAX_MEM) end = MAX_MEM;

        mark_range_free((uint32_t)start, (uint32_t)end);
    }

    // ---- 핵심 수정 포인트 ----
    // 커널 끝을 페이지 경계로 올림(align up)
    uint32_t kend = (kernel_end_phys + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // 0~kend 구간(커널/부트/저주소 포함)은 무조건 사용중 처리
    mark_range_used(0, kend);

    // bitmap 자체가 커널 .bss에 있으니, bitmap 영역도 확실히 사용중으로 잠금
    uint32_t b0 = (uint32_t)bitmap;
    uint32_t b1 = b0 + sizeof(bitmap);

    // mark_range_used는 [start,end) 바이트 범위니까, end를 페이지 경계로 올림
    b1 = (b1 + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    b0 = b0 & ~(PAGE_SIZE - 1);

    mark_range_used(b0, b1);

    console_puts("[pmm] bitmap=");
    print_hex32((uint32_t)bitmap);
    console_putc('\n');

    // 앞으로 할당 탐색은 커널 끝 다음 페이지부터만
    uint32_t fp1 = kend / PAGE_SIZE;
    uint32_t fp2 = b1 / PAGE_SIZE;
    first_page = (fp1 > fp2) ? fp1 : fp2;

    // (원래 있던 0페이지 잠금은 위 mark_range_used(0, kend)에 포함되므로 필요 없음)
}

uint32_t pmm_alloc_page(void) {
    for (uint32_t p = first_page; p < MAX_PAGES; p++) {
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

uint32_t pmm_alloc_contiguous(uint32_t pages) {
    if (pages == 0) return 0;

    uint32_t run = 0;
    uint32_t start = 0;

    uint32_t start_page = first_page;   // ✅ 중요
    if (start_page == 0) start_page = 1; // 안전장치

    for (uint32_t p = start_page; p < MAX_PAGES; p++) {
        if (!test_bit(p)) {
            if (run == 0) start = p;
            run++;
            if (run == pages) {

                // ✅ 여기서부터 "진짜 set 되는지" 검사
                for (uint32_t i = 0; i < pages; i++) {
                    if (test_bit(start + i)) {
                        // 이미 set이면 중복할당 시도
                        // (출력은 선택) 일단 바로 실패로
                        return 0;
                    }
                    set_bit(start + i);

                    // ✅ 안전검사: set 했는데도 0이면 비트맵 오염
                    if (!test_bit(start + i)) return 0;
                }

                free_pages -= pages;
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
        if (test_bit(p)) {
            clear_bit(p);
            free_pages++;
        }
    }
}
