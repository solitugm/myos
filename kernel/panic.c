#include "panic.h"
#include "console.h"

void panic(const char* msg) {
    console_puts("\n[panic] ");
    if (msg) {
        console_puts(msg);
    } else {
        console_puts("unknown");
    }
    console_putc('\n');

    __asm__ __volatile__("cli");
    while (1) {
        __asm__ __volatile__("hlt");
    }
}
