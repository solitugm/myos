#include "console.h"

static volatile uint16_t* const VGA = (uint16_t*)0xB8000;
static uint16_t row = 0;
static uint16_t col = 0;
static uint8_t color = 0x0F;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void scroll_if_needed(void) {
    if (row < 25) return;

    for (int y = 1; y < 25; y++) {
        for (int x = 0; x < 80; x++) {
            VGA[(y - 1) * 80 + x] = VGA[y * 80 + x];
        }
    }

    uint16_t blank = (uint16_t)' ' | ((uint16_t)color << 8);
    for (int x = 0; x < 80; x++) {
        VGA[24 * 80 + x] = blank;
    }

    row = 24;
}

void console_set_cursor(uint16_t r, uint16_t c) {
    if (r > 24) r = 24;
    if (c > 79) c = 79;

    row = r;
    col = c;

    uint16_t pos = (uint16_t)(row * 80 + col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void console_enable_cursor(uint8_t start, uint8_t end) {
    outb(0x3D4, 0x0A);
    uint8_t cur_start = inb(0x3D5);
    cur_start = (cur_start & 0xC0) | (start & 0x1F);
    outb(0x3D5, cur_start);

    outb(0x3D4, 0x0B);
    uint8_t cur_end = inb(0x3D5);
    cur_end = (cur_end & 0xE0) | (end & 0x1F);
    outb(0x3D5, cur_end);
}

void console_clear(void) {
    for (uint16_t y = 0; y < 25; y++) {
        for (uint16_t x = 0; x < 80; x++) {
            VGA[y * 80 + x] = (uint16_t)' ' | ((uint16_t)color << 8);
        }
    }
    console_set_cursor(0, 0);
}

void console_putc(char c) {
    if (c == '\n') {
        row++;
        col = 0;
        scroll_if_needed();
        console_set_cursor(row, col);
        return;
    }

    VGA[row * 80 + col] = (uint16_t)c | ((uint16_t)color << 8);
    col++;

    if (col >= 80) {
        col = 0;
        row++;
        scroll_if_needed();
    }

    console_set_cursor(row, col);
}

void console_puts(const char* s) {
    for (; *s; s++) console_putc(*s);
}

void console_backspace(void) {
    if (col == 0) return;
    col--;
    VGA[row * 80 + col] = (uint16_t)' ' | ((uint16_t)color << 8);
    console_set_cursor(row, col);
}
