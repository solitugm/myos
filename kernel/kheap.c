#include <stdint.h>
#include "kheap.h"
#include "pmm.h"
#include "console.h"

#define PAGE_SIZE 4096
#define ALIGN     16

typedef struct block_header {
    uint32_t size;                // payload size
    uint32_t free;                // 1 free, 0 used
    struct block_header* next;
} block_header_t;

static block_header_t* head = 0;
static uint32_t heap_total = 0;
static uint32_t heap_used  = 0;

static inline uint32_t align_up(uint32_t x, uint32_t a) {
    return (x + a - 1) & ~(a - 1);
}

static void print_hex32(uint32_t v) {
    const char* hex = "0123456789ABCDEF";
    console_puts("0x");
    for (int i = 7; i >= 0; i--) {
        console_putc(hex[(v >> (i * 4)) & 0xF]);
    }
}

static void block_split(block_header_t* b, uint32_t need) {
    // b는 free 블록, need는 payload 크기(정렬된 값)
    uint32_t hdr = (uint32_t)sizeof(block_header_t);
    if (b->size < need + hdr + ALIGN) return; // 쪼갤 만큼 여유 없음

    uint8_t* base = (uint8_t*)b;
    block_header_t* nb = (block_header_t*)(base + hdr + need);
    nb->size = b->size - need - hdr;
    nb->free = 1;
    nb->next = b->next;

    b->size = need;
    b->next = nb;
}

static void coalesce(void) {
    // 인접 free 블록 합치기 (연속 메모리 전제)
    block_header_t* cur = head;
    while (cur && cur->next) {
        block_header_t* nxt = cur->next;

        uint8_t* cur_end = (uint8_t*)cur + sizeof(block_header_t) + cur->size;
        if (cur->free && nxt->free && (uint8_t*)nxt == cur_end) {
            cur->size += sizeof(block_header_t) + nxt->size;
            cur->next = nxt->next;
            continue; // 같은 cur로 다시 시도
        }
        cur = cur->next;
    }
}

static int heap_extend(uint32_t min_bytes_needed) {
    console_puts("[kheap] extend\n");
    uint32_t need = align_up(min_bytes_needed, PAGE_SIZE);
    uint32_t pages = need / PAGE_SIZE;

    uint32_t base = pmm_alloc_contiguous(pages);

    uint32_t probe = pmm_alloc_contiguous(1);
    console_puts("[kheap] probe=");
    print_hex32(probe);
    console_putc('\n');

    if (probe) pmm_free_contiguous(probe, 1);

    console_puts("[kheap] base=");
    print_hex32(base);
    console_putc('\n');

    if (!base) return 0;

    block_header_t* b = (block_header_t*)base;
    b->size = pages * PAGE_SIZE - sizeof(block_header_t);
    b->free = 1;
    b->next = 0;

    if (!head) {
        head = b;
    } else {
        block_header_t* t = head;
        int guard = 0;
        while (t->next) {
            t = t->next;
            if (++guard > 100000) { console_puts("[kheap] loop!\n"); return 0; }
        }
        t->next = b;
    }

    heap_total += pages * PAGE_SIZE;

    // 연속 확장을 했으니 coalesce가 잘 합쳐줌
    coalesce();
    console_puts("[kheap] extend done\n");
    return 1;
}

void kheap_init(void) {
    // 처음에는 1페이지로 시작
    heap_total = 0;
    heap_used = 0;
    head = 0;

    // 최소 1페이지 확보
    heap_extend(PAGE_SIZE);
}

void* kmalloc(uint32_t size) {
    if (size == 0) return 0;
    uint32_t need = align_up(size, ALIGN);

    for (int attempt = 0; attempt < 32; attempt++) {
        block_header_t* cur = head;
        while (cur) {
            if (cur->free && cur->size >= need) {
                block_split(cur, need);
                cur->free = 0;
                heap_used += cur->size;
                return (uint8_t*)cur + sizeof(block_header_t);
            }
            cur = cur->next;
        }

        // 없으면 확장
        if (!heap_extend(need + sizeof(block_header_t))) {
            return 0;
        }
    }

    // 32번 확장했는데도 못 찾으면 구조가 뭔가 잘못된 것
    return 0;
}

void kfree(void* ptr) {
    if (!ptr) return;
    block_header_t* b = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    if (b->free) return; // 이중 free 방지(대충)
    b->free = 1;
    heap_used -= b->size;
    coalesce();
}

uint32_t kheap_total_bytes(void) { return heap_total; }
uint32_t kheap_used_bytes(void)  { return heap_used; }