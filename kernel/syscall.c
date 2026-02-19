#include <stdint.h>
#include "syscall.h"
#include "console.h"
#include "pit.h"
#include "keyboard.h"

enum {
    SYS_WRITE = 1,
    SYS_EXIT = 2,
    SYS_GET_TICKS = 3,
    SYS_READ_KEY = 4,
};

uint32_t syscall_dispatch(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a3;

    switch (num) {
        case SYS_WRITE: {
            const char* s = (const char*)a1;
            uint32_t len = a2;
            if (!s) return (uint32_t)-1;
            for (uint32_t i = 0; i < len; i++) {
                char c = s[i];
                if (!c) break;
                console_putc(c);
            }
            return len;
        }
        case SYS_EXIT:
            return a1;
        case SYS_GET_TICKS:
            return pit_get_ticks();
        case SYS_READ_KEY: {
            int c = keyboard_read_char();
            if (c < 0) return 0xFFFFFFFFu;
            return (uint32_t)c;
        }
        default:
            return 0xFFFFFFFFu;
    }
}
