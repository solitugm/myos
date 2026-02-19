#include <stdint.h>
#include "exec.h"
#include "fs.h"
#include "console.h"

typedef int (*user_entry_t)(int argc, char** argv);

static uint8_t exec_file_buf[64 * 1024];
static uint8_t exec_image_buf[64 * 1024];

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

static void mem_zero(uint8_t* dst, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) dst[i] = 0;
}

static void mem_copy(uint8_t* dst, const uint8_t* src, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) dst[i] = src[i];
}

static int add_overflow_u32(uint32_t a, uint32_t b, uint32_t* out) {
    if (a > 0xFFFFFFFFu - b) return 1;
    *out = a + b;
    return 0;
}

static int load_elf_image(const uint8_t* data, uint32_t size, user_entry_t* out_entry) {
    if (!check_elf_executable(data, size)) return -1;

    const elf32_ehdr_t* eh = (const elf32_ehdr_t*)data;
    const elf32_phdr_t* ph = (const elf32_phdr_t*)(data + eh->e_phoff);

    uint32_t min_vaddr = 0xFFFFFFFFu;
    uint32_t max_vaddr = 0;
    int saw_load = 0;

    for (uint32_t i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != 1 || ph[i].p_memsz == 0) continue;

        uint32_t file_end = 0;
        if (add_overflow_u32(ph[i].p_offset, ph[i].p_filesz, &file_end)) return -2;
        if (file_end > size) return -3;
        if (ph[i].p_filesz > ph[i].p_memsz) return -4;

        uint32_t seg_end = 0;
        if (add_overflow_u32(ph[i].p_vaddr, ph[i].p_memsz, &seg_end)) return -5;

        if (ph[i].p_vaddr < min_vaddr) min_vaddr = ph[i].p_vaddr;
        if (seg_end > max_vaddr) max_vaddr = seg_end;
        saw_load = 1;
    }

    if (!saw_load || min_vaddr >= max_vaddr) return -6;

    uint32_t image_size = max_vaddr - min_vaddr;
    if (image_size == 0 || image_size > sizeof(exec_image_buf)) return -7;

    mem_zero(exec_image_buf, image_size);

    for (uint32_t i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != 1 || ph[i].p_memsz == 0) continue;

        uint32_t dst_off = ph[i].p_vaddr - min_vaddr;
        uint32_t dst_end = 0;
        if (add_overflow_u32(dst_off, ph[i].p_memsz, &dst_end)) return -8;
        if (dst_end > image_size) return -9;

        if (ph[i].p_filesz > 0) {
            mem_copy(exec_image_buf + dst_off, data + ph[i].p_offset, ph[i].p_filesz);
        }
    }

    if (eh->e_entry < min_vaddr || eh->e_entry >= max_vaddr) return -10;
    *out_entry = (user_entry_t)(uintptr_t)(exec_image_buf + (eh->e_entry - min_vaddr));
    return 0;
}

int exec_run(const char* name, int argc, char** argv) {
    if (!ext_is(name, ".bin") && !ext_is(name, ".elf")) {
        console_puts("[exec] blocked: only .bin/.elf are allowed\n");
        return -10;
    }

    uint32_t size = 0;
    if (fs_read_file(name, exec_file_buf, sizeof(exec_file_buf), &size) < 0) {
        console_puts("[exec] file read failed\n");
        return -1;
    }

    if (size == 0) {
        console_puts("[exec] empty file\n");
        return -2;
    }

    if (ext_is(name, ".elf")) {
        user_entry_t elf_entry = 0;
        int lr = load_elf_image(exec_file_buf, size, &elf_entry);
        if (lr < 0) {
            console_puts("[exec] blocked: invalid/non-loadable ELF\n");
            return -11;
        }
        int erc = elf_entry(argc, argv);
        console_puts("[exec] exit=");
        print_u32((uint32_t)erc);
        console_putc('\n');
        return 0;
    }

    if (size < sizeof(mbin_hdr_t)) {
        console_puts("[exec] blocked: bad .bin header size\n");
        return -13;
    }

    const mbin_hdr_t* h = (const mbin_hdr_t*)exec_file_buf;
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

    user_entry_t entry = (user_entry_t)(uintptr_t)(exec_file_buf + h->code_off + h->entry_off);
    int rc = entry(argc, argv);

    console_puts("[exec] exit=");
    print_u32((uint32_t)rc);
    console_putc('\n');

    return 0;
}
