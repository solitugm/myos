#include <stdint.h>

static volatile uint16_t* const VGA = (uint16_t*)0xB8000;
static uint16_t row = 0, col = 0;
static uint8_t color = 0x0F; // white on black

static void putc(char c) {
    if (c == '\n') { row++; col = 0; return; }
    VGA[row * 80 + col] = (uint16_t)c | ((uint16_t)color << 8);
    col++;
    if (col >= 80) { col = 0; row++; }
}

static void puts(const char* s) {
    for (; *s; s++) putc(*s);
}

void kmain(void) {
    puts("Boot OK. Hello from your tiny OS.\n");
    puts("Next: GDT/IDT, interrupts, keyboard...\n");
    for(;;) { __asm__ __volatile__("hlt"); }
}