#pragma once
#include <stdint.h>

void kheap_init(void);
void* kmalloc(uint32_t size);
void kfree(void* ptr);

uint32_t kheap_total_bytes(void);
uint32_t kheap_used_bytes(void);
uint32_t kheap_free_bytes(void);

int kheap_check(void);
void kheap_dump(void);
