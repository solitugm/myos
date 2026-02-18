#include <stdint.h>
#include "port.h"
#include "pic.h"

static volatile uint16_t* const VGA = (uint16_t*)0xB8000;
static uint16_t row = 0, col = 0;
static uint8_t color = 0x0F;

static void putc(char c) {
    if (c == '\n') { row++; col = 0; return; }
    VGA[row * 80 + col] = (uint16_t)c | ((uint16_t)color << 8);
    col++;
    if (col >= 80) { col = 0; row++; }
}

static void puts(const char* s) { for (; *s; s++) putc(*s); }

void isr_default_handler_c(void) {
    // “뭔가 인터럽트가 왔다” 정도만.
    // 너무 많이 오면 화면 도배되니 주석 처리해도 됨.
    // puts("[INT]\n");
}

void irq1_keyboard_handler_c(void) {
    // 키보드 컨트롤러: 0x60에서 스캔코드 읽기
    uint8_t sc = inb(0x60);

    // 아주 단순 출력: 헥사로 스캔코드 찍기
    const char* hex = "0123456789ABCDEF";
    putc('[');
    putc(hex[(sc >> 4) & 0xF]);
    putc(hex[sc & 0xF]);
    putc(']');
    putc(' ');

    // EOI 보내서 PIC에게 “처리 끝” 알림 (IRQ1)
    pic_send_eoi(1);
}