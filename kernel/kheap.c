#include <stdint.h>
#include "kheap.h"
#include "pmm.h"
#include "console.h"

#define PAGE_SIZE 4096
#define ALIGN 16
#define HEAP_MAGIC 0xC0DEFACEu

typedef struct block_header {
    uint32_t magic;
    uint32_t size;
    uint32_t free;
    struct block_header* next;
} block_header_t;

static block_header_t* head = 0;
static uint32_t heap_total = 0;
static uint32_t heap_used = 0;

static inline uint32_t align_up(uint32_t x, uint32_t a) {
    return (x + a - 1) & ~(a - 1);
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

static int block_valid(block_header_t* b) {
    return b && b->magic == HEAP_MAGIC;
}

static void block_split(block_header_t* b, uint32_t need) {
    uint32_t hdr = (uint32_t)sizeof(block_header_t);
    if (b->size < need + hdr + ALIGN) return;

    uint8_t* base = (uint8_t*)b;
    block_header_t* nb = (block_header_t*)(base + hdr + need);
    nb->magic = HEAP_MAGIC;
    nb->size = b->size - need - hdr;
    nb->free = 1;
    nb->next = b->next;

    b->size = need;
    b->next = nb;
}

static void coalesce(void) {
    block_header_t* cur = head;
    while (cur && cur->next) {
        block_header_t* nxt = cur->next;

        if (!block_valid(cur) || !block_valid(nxt)) return;

        uint8_t* cur_end = (uint8_t*)cur + sizeof(block_header_t) + cur->size;
        if (cur->free && nxt->free && (uint8_t*)nxt == cur_end) {
            cur->size += sizeof(block_header_t) + nxt->size;
            cur->next = nxt->next;
            continue;
        }

        cur = cur->next;
    }
}

static int heap_extend(uint32_t min_bytes_needed) {
    uint32_t need = align_up(min_bytes_needed, PAGE_SIZE);
    uint32_t pages = need / PAGE_SIZE;

    uint32_t base = pmm_alloc_contiguous(pages);
    if (!base) return 0;

    block_header_t* b = (block_header_t*)base;
    b->magic = HEAP_MAGIC;
    b->size = pages * PAGE_SIZE - sizeof(block_header_t);
    b->free = 1;
    b->next = 0;

    if (!head) {
        head = b;
    } else {
        block_header_t* t = head;
        while (t->next) {
            if (!block_valid(t)) return 0;
            t = t->next;
        }
        if (!block_valid(t)) return 0;
        t->next = b;
    }

    heap_total += pages * PAGE_SIZE;
    coalesce();
    return 1;
}

void kheap_init(void) {
    heap_total = 0;
    heap_used = 0;
    head = 0;
    heap_extend(PAGE_SIZE);
}

void* kmalloc(uint32_t size) {
    if (size == 0) return 0;
    uint32_t need = align_up(size, ALIGN);

    for (int attempt = 0; attempt < 32; attempt++) {
        block_header_t* cur = head;
        while (cur) {
            if (!block_valid(cur)) return 0;

            if (cur->free && cur->size >= need) {
                block_split(cur, need);
                cur->free = 0;
                heap_used += cur->size;
                return (uint8_t*)cur + sizeof(block_header_t);
            }
            cur = cur->next;
        }

        if (!heap_extend(need + sizeof(block_header_t))) {
            return 0;
        }
    }

    return 0;
}

void kfree(void* ptr) {
    if (!ptr) return;

    block_header_t* b = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    if (!block_valid(b)) return;
    if (b->free) return;

    b->free = 1;
    if (heap_used >= b->size) heap_used -= b->size;
    else heap_used = 0;

    coalesce();
}

uint32_t kheap_total_bytes(void) { return heap_total; }
uint32_t kheap_used_bytes(void) { return heap_used; }
uint32_t kheap_free_bytes(void) { return (heap_total >= heap_used) ? (heap_total - heap_used) : 0; }

int kheap_check(void) {
    block_header_t* cur = head;
    while (cur) {
        if (!block_valid(cur)) return 0;
        cur = cur->next;
    }
    return 1;
}

void kheap_dump(void) {
    console_puts("[mem] heap total/used/free=");
    print_u32(kheap_total_bytes());
    console_putc('/');
    print_u32(kheap_used_bytes());
    console_putc('/');
    print_u32(kheap_free_bytes());
    console_puts(" check=");
    console_puts(kheap_check() ? "ok" : "bad");
    console_putc('\n');
}
