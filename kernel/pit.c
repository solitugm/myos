#include <stdint.h>
#include "port.h"
#include "pit.h"

extern volatile uint32_t g_ticks;

static uint32_t pit_hz = 100;

void pit_set_frequency(uint32_t hz) {
    if (hz < 19) hz = 19;
    if (hz > 1000) hz = 1000;

    pit_hz = hz;
    uint32_t divisor = 1193182 / hz;

    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t pit_get_hz(void) {
    return pit_hz;
}

uint32_t pit_get_ticks(void) {
    return g_ticks;
}

void pit_sleep(uint32_t ms) {
    if (ms == 0) return;

    uint32_t start = g_ticks;
    uint32_t wait_ticks = (ms * pit_hz + 999) / 1000;
    if (wait_ticks == 0) wait_ticks = 1;

    while ((g_ticks - start) < wait_ticks) {
        __asm__ __volatile__("hlt");
    }
}

void pit_irq_tick(void) {
    g_ticks++;
}
