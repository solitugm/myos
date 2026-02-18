#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "port.h"
#include "shell.h"
#include "pmm.h"
#include <stdint.h>
#include "kheap.h"

extern uint32_t end;

void kmain(uint32_t mb2_info_addr) {
    console_clear();
    console_puts("Boot OK.\n");

    // 1) GDT 확정 (0x08/0x10 보장)
    gdt_init();

    // 2) PMM 초기화
    kheap_init();

    // 2) PIC 리맵 (IRQ0~15 -> 0x20~0x2F)
    pic_remap(0x20, 0x28);

    // 커널 끝 물리주소
    uint32_t kernel_end = (uint32_t)&end;

    pmm_init(mb2_info_addr, kernel_end);

    // 3) IRQ0(타이머) + IRQ1(키보드)만 허용
    outb(0x21, 0xFC); // 11111100b
    outb(0xA1, 0xFF);

    // 4) PIT 주파수 설정 (100Hz)
    pit_set_frequency(100);

    // 5) IDT 설치 후 인터럽트 ON
    idt_init();
    __asm__ __volatile__("sti");

    console_puts("Timer/Keyboard OK.\n");
    shell_init();

    while (1) {
        shell_tick();                 // 한 줄 입력되면 실행
        __asm__ __volatile__("hlt");  // 인터럽트 올 때까지 쉼
    }
}