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
static uint8_t g_sector2[512];
static int g_ready = 0;

static uint32_t g_root_lba = 0;
static uint32_t g_root_sectors = 0;
static uint32_t g_data_lba = 0;
static uint32_t g_fat_lba = 0;
static uint16_t g_fat_size = 0;
static uint32_t g_cluster_limit = 0; // exclusive upper bound

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

static int name83_eq(const dirent83_t* e, const char n83[11]) {
    for (int i = 0; i < 8; i++) if (e->name[i] != n83[i]) return 0;
    for (int i = 0; i < 3; i++) if (e->ext[i] != n83[8 + i]) return 0;
    return 1;
}

static void format_name(const dirent83_t* e, char out[13]) {
    int p = 0;
    for (int i = 0; i < 8 && e->name[i] != ' '; i++) out[p++] = e->name[i];
    if (e->ext[0] != ' ') {
        out[p++] = '.';
        for (int i = 0; i < 3 && e->ext[i] != ' '; i++) out[p++] = e->ext[i];
    }
    out[p] = 0;
}

static uint32_t cluster_first_lba(uint16_t cluster) {
    return g_data_lba + (uint32_t)(cluster - 2) * g_bpb.sectors_per_cluster;
}

static int is_usable_file(const dirent83_t* e) {
    if ((uint8_t)e->name[0] == 0x00) return 0;
    if ((uint8_t)e->name[0] == 0xE5) return 0;
    if (e->attr == 0x0F) return 0;
    if (e->attr & 0x08) return 0;
    if (e->attr & 0x10) return 0;
    return 1;
}

static int root_find_entry(const char* name, dirent83_t* out_ent, uint32_t* out_lba, int* out_idx) {
    char n83[11];
    to_83(name, n83);

    for (uint32_t s = 0; s < g_root_sectors; s++) {
        uint32_t lba = g_root_lba + s;
        if (ata_read28(lba, 1, g_sector) < 0) return -1;

        dirent83_t* ents = (dirent83_t*)g_sector;
        for (int i = 0; i < 16; i++) {
            dirent83_t* e = &ents[i];
            if ((uint8_t)e->name[0] == 0x00) return -1;
            if (!is_usable_file(e)) continue;
            if (!name83_eq(e, n83)) continue;

            if (out_ent) *out_ent = *e;
            if (out_lba) *out_lba = lba;
            if (out_idx) *out_idx = i;
            return 0;
        }
    }

    return -1;
}

static int root_find_free(uint32_t* out_lba, int* out_idx) {
    for (uint32_t s = 0; s < g_root_sectors; s++) {
        uint32_t lba = g_root_lba + s;
        if (ata_read28(lba, 1, g_sector) < 0) return -1;

        dirent83_t* ents = (dirent83_t*)g_sector;
        for (int i = 0; i < 16; i++) {
            uint8_t first = (uint8_t)ents[i].name[0];
            if (first == 0xE5 || first == 0x00) {
                if (out_lba) *out_lba = lba;
                if (out_idx) *out_idx = i;
                return 0;
            }
        }
    }

    return -1;
}

static int root_write_entry(uint32_t lba, int idx, const dirent83_t* ent) {
    if (idx < 0 || idx >= 16) return -1;
    if (ata_read28(lba, 1, g_sector) < 0) return -1;

    dirent83_t* ents = (dirent83_t*)g_sector;
    ents[idx] = *ent;

    if (ata_write28(lba, 1, g_sector) < 0) return -1;
    return 0;
}

static int fat12_get(uint16_t cluster, uint16_t* out_val) {
    uint32_t off = (uint32_t)cluster + ((uint32_t)cluster / 2);
    uint32_t sec = off / 512;
    uint32_t in = off % 512;

    if (sec >= g_fat_size) return -1;
    if (ata_read28(g_fat_lba + sec, 1, g_sector) < 0) return -1;

    uint8_t b0 = g_sector[in];
    uint8_t b1;
    if (in == 511) {
        if (sec + 1 >= g_fat_size) return -1;
        if (ata_read28(g_fat_lba + sec + 1, 1, g_sector2) < 0) return -1;
        b1 = g_sector2[0];
    } else {
        b1 = g_sector[in + 1];
    }

    uint16_t pair = (uint16_t)(b0 | ((uint16_t)b1 << 8));
    if (cluster & 1) pair >>= 4;
    else pair &= 0x0FFF;

    *out_val = pair;
    return 0;
}

static int fat12_set_one_copy(uint32_t fat_base_lba, uint16_t cluster, uint16_t value12) {
    uint32_t off = (uint32_t)cluster + ((uint32_t)cluster / 2);
    uint32_t sec = off / 512;
    uint32_t in = off % 512;

    if (sec >= g_fat_size) return -1;
    if (ata_read28(fat_base_lba + sec, 1, g_sector) < 0) return -1;

    if (in == 511) {
        if (sec + 1 >= g_fat_size) return -1;
        if (ata_read28(fat_base_lba + sec + 1, 1, g_sector2) < 0) return -1;

        uint16_t pair = (uint16_t)(g_sector[511] | ((uint16_t)g_sector2[0] << 8));
        if (cluster & 1) pair = (uint16_t)((pair & 0x000F) | ((value12 & 0x0FFF) << 4));
        else pair = (uint16_t)((pair & 0xF000) | (value12 & 0x0FFF));

        g_sector[511] = (uint8_t)(pair & 0xFF);
        g_sector2[0] = (uint8_t)((pair >> 8) & 0xFF);

        if (ata_write28(fat_base_lba + sec, 1, g_sector) < 0) return -1;
        if (ata_write28(fat_base_lba + sec + 1, 1, g_sector2) < 0) return -1;
        return 0;
    }

    uint16_t pair = (uint16_t)(g_sector[in] | ((uint16_t)g_sector[in + 1] << 8));
    if (cluster & 1) pair = (uint16_t)((pair & 0x000F) | ((value12 & 0x0FFF) << 4));
    else pair = (uint16_t)((pair & 0xF000) | (value12 & 0x0FFF));

    g_sector[in] = (uint8_t)(pair & 0xFF);
    g_sector[in + 1] = (uint8_t)((pair >> 8) & 0xFF);

    if (ata_write28(fat_base_lba + sec, 1, g_sector) < 0) return -1;
    return 0;
}

static int fat12_set(uint16_t cluster, uint16_t value12) {
    for (uint8_t i = 0; i < g_bpb.num_fats; i++) {
        uint32_t base = g_fat_lba + (uint32_t)i * g_fat_size;
        if (fat12_set_one_copy(base, cluster, value12) < 0) return -1;
    }
    return 0;
}

static void fat12_free_chain(uint16_t start) {
    uint16_t cur = start;
    for (uint32_t guard = 0; guard < g_cluster_limit; guard++) {
        if (cur < 2 || cur >= g_cluster_limit) return;

        uint16_t next = 0xFFF;
        if (fat12_get(cur, &next) < 0) return;
        if (fat12_set(cur, 0x000) < 0) return;

        if (next >= 0xFF8 || next < 2) return;
        cur = next;
    }
}

static int fat12_alloc_chain(uint32_t need_clusters, uint16_t* out_start) {
    if (need_clusters == 0) {
        *out_start = 0;
        return 0;
    }

    static uint16_t chain[4096];
    if (need_clusters > (uint32_t)(sizeof(chain) / sizeof(chain[0]))) return -1;

    uint32_t found = 0;
    for (uint16_t c = 2; c < g_cluster_limit && found < need_clusters; c++) {
        uint16_t v = 0xFFF;
        if (fat12_get(c, &v) < 0) return -1;
        if (v == 0x000) {
            chain[found++] = c;
        }
    }

    if (found != need_clusters) return -1;

    for (uint32_t i = 0; i < need_clusters; i++) {
        uint16_t next = (i + 1 < need_clusters) ? chain[i + 1] : 0xFFF;
        if (fat12_set(chain[i], next) < 0) {
            for (uint32_t j = 0; j <= i; j++) {
                fat12_set(chain[j], 0x000);
            }
            return -1;
        }
    }

    *out_start = chain[0];
    return 0;
}

static int fs_write_file_data(uint16_t start_cluster, const uint8_t* data, uint32_t len) {
    if (len == 0) return 0;
    if (start_cluster < 2) return -1;

    uint16_t cluster = start_cluster;
    uint32_t pos = 0;

    for (uint32_t guard = 0; guard < g_cluster_limit && pos < len; guard++) {
        uint32_t lba = cluster_first_lba(cluster);

        for (uint8_t s = 0; s < g_bpb.sectors_per_cluster && pos < len; s++) {
            for (int i = 0; i < 512; i++) g_sector[i] = 0;

            uint32_t chunk = len - pos;
            if (chunk > 512) chunk = 512;
            for (uint32_t i = 0; i < chunk; i++) g_sector[i] = data[pos + i];

            if (ata_write28(lba + s, 1, g_sector) < 0) return -1;
            pos += chunk;
        }

        if (pos >= len) break;

        uint16_t next = 0xFFF;
        if (fat12_get(cluster, &next) < 0) return -1;
        if (next < 2 || next >= 0xFF8) return -1;
        cluster = next;
    }

    return (pos == len) ? 0 : -1;
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

    uint32_t total_sectors = g_bpb.total_sectors16 ? g_bpb.total_sectors16 : g_bpb.total_sectors32;
    if (total_sectors == 0) return -1;

    g_fat_lba = g_bpb.reserved_sectors;
    g_fat_size = g_bpb.fat_size16;
    g_root_sectors = (g_bpb.root_entries * 32 + (g_bpb.bytes_per_sector - 1)) / g_bpb.bytes_per_sector;
    g_root_lba = g_fat_lba + (g_bpb.num_fats * g_bpb.fat_size16);
    g_data_lba = g_root_lba + g_root_sectors;

    uint32_t data_sectors = total_sectors - g_data_lba;
    uint32_t clusters = data_sectors / g_bpb.sectors_per_cluster;
    g_cluster_limit = (uint16_t)(clusters + 2);

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
            if (!is_usable_file(e)) continue;

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

int fs_get_file_size(const char* name, uint32_t* out_size) {
    if (!g_ready || !name || !out_size) return -1;

    dirent83_t ent;
    if (root_find_entry(name, &ent, 0, 0) < 0) return -1;

    *out_size = ent.file_size;
    return 0;
}

int fs_read_file(const char* name, void* buf, uint32_t maxlen, uint32_t* out_len) {
    if (!g_ready || !name || !buf || !out_len) return -1;

    dirent83_t ent;
    if (root_find_entry(name, &ent, 0, 0) < 0) return -1;

    uint32_t to_copy = ent.file_size;
    if (to_copy > maxlen) to_copy = maxlen;

    uint8_t* out = (uint8_t*)buf;
    uint32_t copied = 0;
    uint16_t cluster = ent.first_cluster_lo;

    while (cluster >= 2 && cluster < 0xFF8 && copied < to_copy) {
        uint32_t first_sector = cluster_first_lba(cluster);

        for (uint8_t s = 0; s < g_bpb.sectors_per_cluster && copied < to_copy; s++) {
            if (ata_read28(first_sector + s, 1, g_sector) < 0) return -1;

            uint32_t chunk = to_copy - copied;
            if (chunk > 512) chunk = 512;
            for (uint32_t i = 0; i < chunk; i++) out[copied + i] = g_sector[i];
            copied += chunk;
        }

        if (copied >= to_copy) break;
        if (fat12_get(cluster, &cluster) < 0) return -1;
    }

    *out_len = copied;
    return 0;
}

int fs_write_file(const char* name, const void* buf, uint32_t len) {
    if (!g_ready || !name) return -1;
    if (len > 0 && !buf) return -1;

    dirent83_t old_ent;
    uint32_t old_lba = 0;
    int old_idx = -1;
    int exists = (root_find_entry(name, &old_ent, &old_lba, &old_idx) == 0);

    uint32_t cluster_bytes = (uint32_t)g_bpb.sectors_per_cluster * 512;
    uint32_t need_clusters = (len == 0) ? 0 : ((len + cluster_bytes - 1) / cluster_bytes);

    uint16_t new_start = 0;
    if (fat12_alloc_chain(need_clusters, &new_start) < 0) return -1;

    if (len > 0) {
        if (fs_write_file_data(new_start, (const uint8_t*)buf, len) < 0) {
            fat12_free_chain(new_start);
            return -1;
        }
    }

    dirent83_t ent;
    uint32_t dst_lba;
    int dst_idx;

    if (exists) {
        ent = old_ent;
        dst_lba = old_lba;
        dst_idx = old_idx;
    } else {
        if (root_find_free(&dst_lba, &dst_idx) < 0) {
            if (new_start >= 2) fat12_free_chain(new_start);
            return -1;
        }

        for (int i = 0; i < (int)sizeof(ent); i++) ((uint8_t*)&ent)[i] = 0;
        char n83[11];
        to_83(name, n83);
        for (int i = 0; i < 8; i++) ent.name[i] = n83[i];
        for (int i = 0; i < 3; i++) ent.ext[i] = n83[8 + i];
        ent.attr = 0x20;
    }

    ent.first_cluster_lo = new_start;
    ent.first_cluster_hi = 0;
    ent.file_size = len;

    if (root_write_entry(dst_lba, dst_idx, &ent) < 0) {
        if (new_start >= 2) fat12_free_chain(new_start);
        return -1;
    }

    if (exists && old_ent.first_cluster_lo >= 2) {
        fat12_free_chain(old_ent.first_cluster_lo);
    }

    return 0;
}

int fs_delete_file(const char* name) {
    if (!g_ready || !name) return -1;

    dirent83_t ent;
    uint32_t lba = 0;
    int idx = -1;
    if (root_find_entry(name, &ent, &lba, &idx) < 0) return -1;

    if (ent.first_cluster_lo >= 2) fat12_free_chain(ent.first_cluster_lo);

    if (ata_read28(lba, 1, g_sector) < 0) return -1;
    dirent83_t* ents = (dirent83_t*)g_sector;
    ents[idx].name[0] = (char)0xE5;
    if (ata_write28(lba, 1, g_sector) < 0) return -1;

    return 0;
}

int fs_rename_file(const char* old_name, const char* new_name) {
    if (!g_ready || !old_name || !new_name) return -1;

    dirent83_t ent;
    uint32_t lba = 0;
    int idx = -1;
    if (root_find_entry(old_name, &ent, &lba, &idx) < 0) return -1;

    if (root_find_entry(new_name, 0, 0, 0) == 0) return -1;

    char n83[11];
    to_83(new_name, n83);
    for (int i = 0; i < 8; i++) ent.name[i] = n83[i];
    for (int i = 0; i < 3; i++) ent.ext[i] = n83[8 + i];

    return root_write_entry(lba, idx, &ent);
}
