#include <stdint.h>
#include "exec.h"
#include "fs.h"
#include "console.h"

typedef int (*user_entry_t)(int argc, char** argv);

static uint8_t exec_buf[64 * 1024];

static void print_u32(uint32_t v) {
    char buf[16];
    int i = 0;
    if (v == 0) {
        console_putc('0');
        return;
    }
    while (v && i < (int)sizeof(buf)) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }
    while (i--) console_putc(buf[i]);
}

int exec_run(const char* name, int argc, char** argv) {
    uint32_t size = 0;
    if (fs_read_file(name, exec_buf, sizeof(exec_buf), &size) < 0) {
        return -1;
    }

    if (size == 0) {
        return -2;
    }

    user_entry_t entry = (user_entry_t)(uintptr_t)exec_buf;
    int rc = entry(argc, argv);

    console_puts("[exec] exit=");
    print_u32((uint32_t)rc);
    console_putc('\n');

    return 0;
}
