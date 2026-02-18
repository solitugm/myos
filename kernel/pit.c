#include <stdint.h>
#include "port.h"
#include "pit.h"

void pit_set_frequency(uint32_t hz) {
    if (hz < 19) hz = 19;           // 너무 낮으면 의미 없음
    if (hz > 1000) hz = 1000;       // 너무 높이면 출력 난장판

    uint32_t divisor = 1193182 / hz;

    outb(0x43, 0x36);               // channel 0, lobyte/hibyte, mode 3
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}