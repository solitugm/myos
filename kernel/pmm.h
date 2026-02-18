#pragma once
#include <stdint.h>

void pmm_init(uint32_t mb2_info_addr, uint32_t kernel_end_phys);
uint32_t pmm_alloc_page(void);
void pmm_free_page(uint32_t addr);

uint32_t pmm_total_pages(void);
uint32_t pmm_free_pages(void);

uint32_t pmm_alloc_contiguous(uint32_t pages);
void pmm_free_contiguous(uint32_t addr, uint32_t pages);
