#include <stdint.h>
#include "console.h"
#include "keyboard.h"
#include "shell.h"
#include "pmm.h"

// isr.c에 있는 ticks를 쓰고 싶으면 extern으로 끌어다 쓰면 됨
extern volatile uint32_t ticks;

static char line[128];

static int streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static void print_u32(uint32_t v) {
    char buf[16];
    int i = 0;

    if (v == 0) {
        console_putc('0');
        return;
    }

    while (v > 0 && i < (int)sizeof(buf)) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }
    while (i--) console_putc(buf[i]);
}

static void execute(const char* cmd) {
    if (!cmd[0]) return;

    if (streq(cmd, "help")) {
        console_puts("Commands: help, clear, ticks, mem\n");
    } 
    else if (streq(cmd, "clear")) {
        console_clear();
    } 
    else if (streq(cmd, "ticks")) {
        print_u32(ticks);
        console_putc('\n');
    } 
    else if (streq(cmd, "mem")) {
        console_puts("free pages: ");
        print_u32(pmm_free_pages());
        console_putc('/');
        print_u32(pmm_total_pages());
        console_putc('\n');
    } 
    else {
        console_puts("Unknown command\n");
    }
}

void shell_init(void) {
    console_puts("shell> ");
}

void shell_tick(void) {
    if (keyboard_getline(line, (int)sizeof(line))) {
        execute(line);
        console_puts("shell> ");
    }
}