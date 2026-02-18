#include <stdint.h>
#include "idt.h"
#include "pic.h"
#include "port.h"
#include "gdt.h"

static volatile uint16_t* const VGA = (uint16_t*)0xB8000;
static uint16_t row = 0, col = 0;
static uint8_t color = 0x0F;

static void clear_screen() {
    for (uint16_t y = 0; y < 25; y++)
        for (uint16_t x = 0; x < 80; x++)
            VGA[y * 80 + x] = (uint16_t)' ' | ((uint16_t)color << 8);
    row = 0; col = 0;
}
static void putc(char c) {
    if (c == '\n') { row++; col = 0; return; }
    VGA[row * 80 + col] = (uint16_t)c | ((uint16_t)color << 8);
    col++;
    if (col >= 80) { col = 0; row++; }
}
static void puts(const char* s) { for (; *s; s++) putc(*s); }

void kmain(void) {
    clear_screen();
    puts("Boot OK.\n");

    gdt_init();

    // 1) PIC 리맵: IRQ0~15를 0x20~0x2F로 옮김
    pic_remap(0x20, 0x28);
    outb(0x21, 0xFD); // 11111101b : IRQ1(키보드)만 허용
    outb(0xA1, 0xFF); // 슬레이브 PIC 전부 막음

    // 2) IDT 설치
    idt_init();

    // 3) 인터럽트 ON
    __asm__ __volatile__("sti");

    puts("IDT/PIC OK. Press keys...\n");

    for(;;) { __asm__ __volatile__("hlt"); }
}