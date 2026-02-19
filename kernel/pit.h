#pragma once
#include <stdint.h>

void pit_set_frequency(uint32_t hz);
uint32_t pit_get_hz(void);
uint32_t pit_get_ticks(void);
void pit_sleep(uint32_t ms);
void pit_irq_tick(void);
