#include <stdint.h>
#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "port.h"
#include "shell.h"
#include "pmm.h"
#include "kheap.h"
#include "fs.h"

extern uint32_t end;

void kmain(uint32_t mb2_info_addr) {
    console_clear();
    console_puts("[init] Boot OK\n");

    gdt_init();
    console_puts("[init] GDT ready\n");

    pic_remap(0x20, 0x28);
    console_puts("[irq] PIC remapped\n");

    uint32_t kernel_end = (uint32_t)&end;
    pmm_init(mb2_info_addr, kernel_end);
    kheap_init();

    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);

    pit_set_frequency(100);
    console_puts("[irq] PIT 100Hz\n");

    idt_init();
    __asm__ __volatile__("sti");
    console_puts("[irq] IDT loaded, interrupts enabled\n");

    console_enable_cursor(14, 15);

    if (fs_init() < 0) {
        console_puts("[fs] init skipped (no ATA/FAT media)\n");
    }

    shell_init();

    while (1) {
        shell_tick();
        __asm__ __volatile__("hlt");
    }
}
