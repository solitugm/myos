#include <stdint.h>
#include "exec.h"
#include "fs.h"
#include "console.h"

typedef int (*user_entry_t)(int argc, char** argv);

static uint8_t exec_buf[64 * 1024];

typedef struct {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf32_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elf32_phdr_t;

typedef struct {
    uint8_t magic[4];
    uint32_t version;
    uint32_t flags;
    uint32_t entry_off;
    uint32_t code_off;
    uint32_t code_size;
} __attribute__((packed)) mbin_hdr_t;

#define MBIN_EXECUTABLE 0x1u

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

static char upc(char c) {
    if (c >= 'a' && c <= 'z') return (char)(c - 'a' + 'A');
    return c;
}

static const char* find_ext(const char* name) {
    const char* dot = 0;
    for (; *name; name++) {
        if (*name == '.') dot = name;
    }
    return dot;
}

static int ext_is(const char* name, const char* want) {
    const char* ext = find_ext(name);
    if (!ext) return 0;

    while (*ext && *want) {
        if (upc(*ext) != upc(*want)) return 0;
        ext++;
        want++;
    }
    return *ext == 0 && *want == 0;
}

static int check_elf_executable(const uint8_t* data, uint32_t size) {
    if (size < sizeof(elf32_ehdr_t)) return 0;

    const elf32_ehdr_t* eh = (const elf32_ehdr_t*)data;
    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' || eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F') return 0;
    if (eh->e_ident[4] != 1 || eh->e_ident[5] != 1 || eh->e_ident[6] != 1) return 0;
    if (eh->e_machine != 3) return 0;
    if (!(eh->e_type == 2 || eh->e_type == 3)) return 0;

    if (eh->e_phoff >= size) return 0;
    if (eh->e_phentsize != sizeof(elf32_phdr_t)) return 0;
    if (eh->e_phnum == 0) return 0;

    uint32_t ph_total = (uint32_t)eh->e_phentsize * (uint32_t)eh->e_phnum;
    if (eh->e_phoff + ph_total > size) return 0;

    const elf32_phdr_t* ph = (const elf32_phdr_t*)(data + eh->e_phoff);
    for (uint32_t i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != 1) continue; // PT_LOAD
        if (ph[i].p_filesz == 0) continue;
        if (ph[i].p_flags & 0x1) return 1; // PF_X
    }

    return 0;
}

int exec_run(const char* name, int argc, char** argv) {
    if (!ext_is(name, ".bin") && !ext_is(name, ".elf")) {
        console_puts("[exec] blocked: only .bin/.elf are allowed\n");
        return -10;
    }

    uint32_t size = 0;
    if (fs_read_file(name, exec_buf, sizeof(exec_buf), &size) < 0) {
        console_puts("[exec] file read failed\n");
        return -1;
    }

    if (size == 0) {
        console_puts("[exec] empty file\n");
        return -2;
    }

    if (ext_is(name, ".elf")) {
        if (!check_elf_executable(exec_buf, size)) {
            console_puts("[exec] blocked: invalid/non-executable ELF\n");
            return -11;
        }
        console_puts("[exec] ELF verified but loader is not implemented yet\n");
        return -12;
    }

    if (size < sizeof(mbin_hdr_t)) {
        console_puts("[exec] blocked: bad .bin header size\n");
        return -13;
    }

    const mbin_hdr_t* h = (const mbin_hdr_t*)exec_buf;
    if (!(h->magic[0] == 'M' && h->magic[1] == 'B' && h->magic[2] == 'I' && h->magic[3] == 'N')) {
        console_puts("[exec] blocked: bad .bin magic (need MBIN)\n");
        return -14;
    }
    if (h->version != 1) {
        console_puts("[exec] blocked: unsupported .bin version\n");
        return -15;
    }
    if (!(h->flags & MBIN_EXECUTABLE)) {
        console_puts("[exec] blocked: .bin is not executable\n");
        return -16;
    }
    if (h->code_off >= size || h->code_size > size || h->code_off + h->code_size > size) {
        console_puts("[exec] blocked: invalid .bin code range\n");
        return -17;
    }
    if (h->entry_off >= h->code_size) {
        console_puts("[exec] blocked: invalid .bin entry\n");
        return -18;
    }

    user_entry_t entry = (user_entry_t)(uintptr_t)(exec_buf + h->code_off + h->entry_off);
    int rc = entry(argc, argv);

    console_puts("[exec] exit=");
    print_u32((uint32_t)rc);
    console_putc('\n');

    return 0;
}
