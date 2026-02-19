#include <stdint.h>
#include "console.h"
#include "keyboard.h"
#include "shell.h"
#include "pmm.h"
#include "kheap.h"
#include "pit.h"
#include "fs.h"
#include "exec.h"

#define MAX_ARGS 8

static char line[128];
static void* last_ptr = 0;
static uint8_t file_buf[4096];

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

static void print_hex8(uint8_t v) {
    const char* hex = "0123456789ABCDEF";
    console_putc(hex[(v >> 4) & 0xF]);
    console_putc(hex[v & 0xF]);
}

static void print_hex32(uint32_t v) {
    const char* hex = "0123456789ABCDEF";
    console_puts("0x");
    for (int i = 7; i >= 0; i--) {
        console_putc(hex[(v >> (i * 4)) & 0xF]);
    }
}

static int streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static uint32_t parse_u32(const char* s, int* ok) {
    uint32_t v = 0;
    int base = 10;
    int i = 0;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        i = 2;
    }

    if (!s[i]) {
        *ok = 0;
        return 0;
    }

    for (; s[i]; i++) {
        char c = s[i];
        uint32_t d;
        if (c >= '0' && c <= '9') d = (uint32_t)(c - '0');
        else if (base == 16 && c >= 'a' && c <= 'f') d = (uint32_t)(c - 'a' + 10);
        else if (base == 16 && c >= 'A' && c <= 'F') d = (uint32_t)(c - 'A' + 10);
        else {
            *ok = 0;
            return 0;
        }
        if (d >= (uint32_t)base) {
            *ok = 0;
            return 0;
        }
        v = v * (uint32_t)base + d;
    }

    *ok = 1;
    return v;
}

static int split_args(char* s, char** argv, int max_args) {
    int argc = 0;

    while (*s && argc < max_args) {
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) break;

        argv[argc++] = s;
        while (*s && *s != ' ' && *s != '\t') s++;
        if (!*s) break;
        *s++ = 0;
    }

    return argc;
}

static void cmd_help(void) {
    console_puts("Commands: help clear ticks sleep mem heap pmmstat heapstat alloc free hexdump ls cat run\n");
}

static void cmd_mem(void) {
    console_puts("free pages: ");
    print_u32(pmm_free_pages());
    console_putc('/');
    print_u32(pmm_total_pages());
    console_putc('\n');
}

static void cmd_heap(void) {
    console_puts("heap used/total: ");
    print_u32(kheap_used_bytes());
    console_putc('/');
    print_u32(kheap_total_bytes());
    console_putc('\n');
}

static void cmd_alloc(int argc, char** argv) {
    if (argc < 2) {
        console_puts("usage: alloc <bytes>\n");
        return;
    }

    int ok = 0;
    uint32_t n = parse_u32(argv[1], &ok);
    if (!ok || n == 0) {
        console_puts("usage: alloc <bytes>\n");
        return;
    }

    last_ptr = kmalloc(n);
    if (!last_ptr) {
        console_puts("alloc failed\n");
        return;
    }

    console_puts("alloc ok ptr=");
    print_hex32((uint32_t)(uintptr_t)last_ptr);
    console_putc('\n');
}

static void cmd_free(void) {
    if (!last_ptr) {
        console_puts("nothing to free\n");
        return;
    }

    kfree(last_ptr);
    last_ptr = 0;
    console_puts("freed\n");
}

static void cmd_hexdump(int argc, char** argv) {
    if (argc < 3) {
        console_puts("usage: hexdump <addr> <len>\n");
        return;
    }

    int ok1 = 0, ok2 = 0;
    uint32_t addr = parse_u32(argv[1], &ok1);
    uint32_t len = parse_u32(argv[2], &ok2);
    if (!ok1 || !ok2 || len == 0 || len > 256) {
        console_puts("len must be 1..256\n");
        return;
    }

    uint8_t* p = (uint8_t*)(uintptr_t)addr;
    for (uint32_t i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            print_hex32((uint32_t)(uintptr_t)(p + i));
            console_puts(": ");
        }
        print_hex8(p[i]);
        console_putc(' ');
        if ((i % 16) == 15) console_putc('\n');
    }
    if ((len % 16) != 0) console_putc('\n');
}

static void cmd_sleep(int argc, char** argv) {
    if (argc < 2) {
        console_puts("usage: sleep <ms>\n");
        return;
    }

    int ok = 0;
    uint32_t ms = parse_u32(argv[1], &ok);
    if (!ok) {
        console_puts("usage: sleep <ms>\n");
        return;
    }

    pit_sleep(ms);
}

static void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        console_puts("usage: cat <file>\n");
        return;
    }

    uint32_t out = 0;
    if (fs_read_file(argv[1], file_buf, sizeof(file_buf) - 1, &out) < 0) {
        console_puts("cat: file not found or read failed\n");
        return;
    }

    file_buf[out] = 0;
    for (uint32_t i = 0; i < out; i++) {
        console_putc((char)file_buf[i]);
    }
    if (out == 0 || file_buf[out - 1] != '\n') console_putc('\n');
}

static void cmd_run(int argc, char** argv) {
    if (argc < 2) {
        console_puts("usage: run <file>\n");
        return;
    }

    int rc = exec_run(argv[1], argc - 1, &argv[1]);
    if (rc < 0) {
        console_puts("run failed\n");
    }
}

static void execute(char* cmdline) {
    char* argv[MAX_ARGS];
    int argc = split_args(cmdline, argv, MAX_ARGS);
    if (argc == 0) return;

    if (streq(argv[0], "help")) {
        cmd_help();
    } else if (streq(argv[0], "clear")) {
        console_clear();
    } else if (streq(argv[0], "ticks")) {
        print_u32(pit_get_ticks());
        console_putc('\n');
    } else if (streq(argv[0], "sleep")) {
        cmd_sleep(argc, argv);
    } else if (streq(argv[0], "mem")) {
        cmd_mem();
    } else if (streq(argv[0], "heap")) {
        cmd_heap();
    } else if (streq(argv[0], "pmmstat")) {
        pmm_dump_summary();
    } else if (streq(argv[0], "heapstat")) {
        kheap_dump();
    } else if (streq(argv[0], "alloc")) {
        cmd_alloc(argc, argv);
    } else if (streq(argv[0], "free")) {
        cmd_free();
    } else if (streq(argv[0], "hexdump")) {
        cmd_hexdump(argc, argv);
    } else if (streq(argv[0], "ls")) {
        if (fs_list() < 0) console_puts("ls failed\n");
    } else if (streq(argv[0], "cat")) {
        cmd_cat(argc, argv);
    } else if (streq(argv[0], "run")) {
        cmd_run(argc, argv);
    } else {
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
