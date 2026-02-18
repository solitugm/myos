#include <stdint.h>
#include "console.h"
#include "keyboard.h"
#include "shell.h"
#include "pmm.h"
#include "kheap.h"

// isr.c에 있는 ticks를 쓰고 싶으면 extern으로 끌어다 쓰면 됨
extern volatile uint32_t ticks;

static char line[128];

static void* last_ptr = 0;


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
        console_puts("Commands: help, clear, ticks, mem, heap, alloc N, free\n");
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
    else if (streq(cmd, "heap")) {
        console_puts("heap used/total: ");
        print_u32(kheap_used_bytes());
        console_putc('/');
        print_u32(kheap_total_bytes());
        console_putc('\n');
    }
    else if (cmd[0]=='a' && cmd[1]=='l' && cmd[2]=='l' && cmd[3]=='o' && cmd[4]=='c' && (cmd[5]==' ' || cmd[5]==0)) {
        // "alloc 123"
        const char* p = cmd + 5;
        while (*p == ' ') p++;
        if (*p < '0' || *p > '9') {
            console_puts("usage: alloc <bytes>\n");
        } else {
            uint32_t n = 0;
            while (*p >= '0' && *p <= '9') { n = n*10 + (uint32_t)(*p - '0'); p++; }

            last_ptr = kmalloc(n);
            if (!last_ptr) {
                console_puts("alloc failed\n");
            } else {
                console_puts("alloc ok, ptr=");
                // ptr 주소 16진수 출력(간단)
                uint32_t v = (uint32_t)last_ptr;
                const char* hex = "0123456789ABCDEF";
                console_puts("0x");
                for (int i=7;i>=0;i--) {
                    console_putc(hex[(v >> (i*4)) & 0xF]);
                }
                console_putc('\n');
            }
        }
    }
    else if (streq(cmd, "free")) {
        if (!last_ptr) {
            console_puts("nothing to free\n");
        } else {
            kfree(last_ptr);
            last_ptr = 0;
            console_puts("freed\n");
        }
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