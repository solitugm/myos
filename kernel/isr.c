#include <stdint.h>
#include "port.h"
#include "pic.h"
#include "console.h"
#include "pit.h"
#include "keyboard.h"

volatile uint32_t ticks = 0;

static uint32_t hz = 100; // 우리가 설정할 주파수와 맞춰야 함

void isr_default_handler_c(void) {
    // “뭔가 인터럽트가 왔다” 정도만.
    // 너무 많이 오면 화면 도배되니 주석 처리해도 됨.
    // puts("[INT]\n");
}

void irq1_keyboard_handler_c(void) {
    uint8_t sc = inb(0x60);
    keyboard_handler(sc);
    pic_send_eoi(1);
}

void irq0_timer_handler_c(void) {
    ticks++;

    // 1초마다 점 찍기
    if (ticks % hz == 0) {
        // console_putc('.');
    }

    pic_send_eoi(0);
}