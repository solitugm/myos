#include "console.h"

static volatile uint16_t* const VGA = (uint16_t*)0xB8000;
static uint16_t row = 0, col = 0;
static uint8_t color = 0x0F;

static void scroll_if_needed(void) {
    if (row < 25) return;

    // 한 줄 위로 당기기
    for (int y = 1; y < 25; y++) {
        for (int x = 0; x < 80; x++) {
            VGA[(y - 1) * 80 + x] = VGA[y * 80 + x];
        }
    }

    // 마지막 줄 비우기
    uint16_t blank = (uint16_t)' ' | ((uint16_t)color << 8);
    for (int x = 0; x < 80; x++) {
        VGA[24 * 80 + x] = blank;
    }

    row = 24;  // 마지막 줄에 고정
}

void console_clear(void) {
    for (uint16_t y=0; y<25; y++)
        for (uint16_t x=0; x<80; x++)
            VGA[y*80+x] = (uint16_t)' ' | ((uint16_t)color << 8);
    row = 0; col = 0;
}

void console_putc(char c) {
    if (c == '\n') {
        row++;
        col = 0;
        scroll_if_needed();   // ✅ 여기 추가
        return;
    }

    VGA[row*80 + col] = (uint16_t)c | ((uint16_t)color << 8);
    col++;

    if (col >= 80) {
        col = 0;
        row++;
        scroll_if_needed();   // ✅ 여기 추가
    }
}

void console_puts(const char* s) {
    for (; *s; s++) console_putc(*s);
}

void console_backspace(void) {
    if (col == 0) return;     // 아주 단순 처리(라인 넘김은 나중에)
    col--;
    VGA[row*80 + col] = (uint16_t)' ' | ((uint16_t)color << 8);
}