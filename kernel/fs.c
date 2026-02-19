#include <stdint.h>
#include "fs.h"
#include "ata.h"
#include "console.h"

#pragma pack(push, 1)
typedef struct {
    uint8_t jmp[3];
    char oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_entries;
    uint16_t total_sectors16;
    uint8_t media;
    uint16_t fat_size16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors32;
} fat_bpb_t;

typedef struct {
    char name[8];
    char ext[3];
    uint8_t attr;
    uint8_t ntres;
    uint8_t crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t last_access_date;
    uint16_t first_cluster_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t first_cluster_lo;
    uint32_t file_size;
} dirent83_t;
#pragma pack(pop)

static fat_bpb_t g_bpb;
static uint8_t g_sector[512];
static uint8_t g_fat_sector[512];
static int g_ready = 0;

static uint32_t g_root_lba = 0;
static uint32_t g_root_sectors = 0;
static uint32_t g_data_lba = 0;
static uint32_t g_fat_lba = 0;
static uint16_t g_fat_size = 0;

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
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

static void to_83(const char* in, char out[11]) {
    for (int i = 0; i < 11; i++) out[i] = ' ';

    int oi = 0;
    int ext = 0;
    for (int i = 0; in[i]; i++) {
        if (in[i] == '.') {
            ext = 1;
            oi = 8;
            continue;
        }
        if (!ext) {
            if (oi >= 8) continue;
            out[oi++] = upc(in[i]);
        } else {
            if (oi >= 11) continue;
            out[oi++] = upc(in[i]);
        }
    }
}

static void format_name(const dirent83_t* e, char out[13]) {
    int p = 0;

    for (int i = 0; i < 8 && e->name[i] != ' '; i++) {
        out[p++] = e->name[i];
    }

    if (e->ext[0] != ' ') {
        out[p++] = '.';
        for (int i = 0; i < 3 && e->ext[i] != ' '; i++) {
            out[p++] = e->ext[i];
        }
    }

    out[p] = 0;
}

static uint16_t fat12_next_cluster(uint16_t cluster) {
    uint32_t fat_offset = cluster + (cluster / 2);
    uint32_t fat_sector = g_fat_lba + (fat_offset / 512);
    uint32_t ent_off = fat_offset % 512;

    if (fat_sector >= g_fat_lba + g_fat_size) return 0xFFF;
    if (ata_read28(fat_sector, 1, g_fat_sector) < 0) return 0xFFF;

    uint16_t val = *(uint16_t*)&g_fat_sector[ent_off];
    if (cluster & 1) val >>= 4;
    else val &= 0x0FFF;

    return val;
}

static int find_root_entry(const char* name, dirent83_t* out_ent) {
    char want[11];
    to_83(name, want);

    for (uint32_t s = 0; s < g_root_sectors; s++) {
        if (ata_read28(g_root_lba + s, 1, g_sector) < 0) return -1;

        dirent83_t* ents = (dirent83_t*)g_sector;
        for (int i = 0; i < 16; i++) {
            dirent83_t* e = &ents[i];
            if ((uint8_t)e->name[0] == 0x00) return -1;
            if ((uint8_t)e->name[0] == 0xE5) continue;
            if (e->attr == 0x0F) continue;
            if (e->attr & 0x08) continue;

            int ok = 1;
            for (int j = 0; j < 8; j++) if (e->name[j] != want[j]) ok = 0;
            for (int j = 0; j < 3; j++) if (e->ext[j] != want[8 + j]) ok = 0;
            if (!ok) continue;

            *out_ent = *e;
            return 0;
        }
    }

    return -1;
}

int fs_init(void) {
    g_ready = 0;

    int rc = ata_init();
    if (rc < 0) {
        console_puts("[fs] ata init failed\n");
        return rc;
    }

    if (ata_read28(0, 1, g_sector) < 0) {
        console_puts("[fs] boot sector read failed\n");
        return -1;
    }

    g_bpb = *(fat_bpb_t*)g_sector;
    if (g_bpb.bytes_per_sector != 512 || g_bpb.num_fats == 0 || g_bpb.fat_size16 == 0) {
        console_puts("[fs] unsupported fat\n");
        return -1;
    }

    g_fat_lba = g_bpb.reserved_sectors;
    g_fat_size = g_bpb.fat_size16;

    g_root_sectors = (g_bpb.root_entries * 32 + (g_bpb.bytes_per_sector - 1)) / g_bpb.bytes_per_sector;
    g_root_lba = g_fat_lba + (g_bpb.num_fats * g_bpb.fat_size16);
    g_data_lba = g_root_lba + g_root_sectors;

    g_ready = 1;
    console_puts("[fs] FAT12 ready\n");
    return 0;
}

int fs_list(void) {
    if (!g_ready) return -1;

    for (uint32_t s = 0; s < g_root_sectors; s++) {
        if (ata_read28(g_root_lba + s, 1, g_sector) < 0) return -1;

        dirent83_t* ents = (dirent83_t*)g_sector;
        for (int i = 0; i < 16; i++) {
            dirent83_t* e = &ents[i];
            if ((uint8_t)e->name[0] == 0x00) return 0;
            if ((uint8_t)e->name[0] == 0xE5) continue;
            if (e->attr == 0x0F) continue;
            if (e->attr & 0x08) continue;

            char name[13];
            format_name(e, name);
            console_puts(name);
            console_puts(" ");
            print_u32(e->file_size);
            console_putc('\n');
        }
    }

    return 0;
}

int fs_read_file(const char* name, void* buf, uint32_t maxlen, uint32_t* out_len) {
    if (!g_ready || !name || !buf || !out_len) return -1;

    dirent83_t ent;
    if (find_root_entry(name, &ent) < 0) return -1;

    uint32_t to_copy = ent.file_size;
    if (to_copy > maxlen) to_copy = maxlen;

    uint8_t* out = (uint8_t*)buf;
    uint32_t copied = 0;
    uint16_t cluster = ent.first_cluster_lo;

    while (cluster >= 2 && cluster < 0xFF8 && copied < to_copy) {
        uint32_t first_sector = g_data_lba + (cluster - 2) * g_bpb.sectors_per_cluster;

        for (uint8_t s = 0; s < g_bpb.sectors_per_cluster && copied < to_copy; s++) {
            if (ata_read28(first_sector + s, 1, g_sector) < 0) return -1;

            uint32_t chunk = to_copy - copied;
            if (chunk > 512) chunk = 512;
            for (uint32_t i = 0; i < chunk; i++) {
                out[copied + i] = g_sector[i];
            }
            copied += chunk;
        }

        cluster = fat12_next_cluster(cluster);
    }

    *out_len = copied;
    return 0;
}
