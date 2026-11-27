#include "s31/game_catalog.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#ifdef ESP_PLATFORM
#include "esp_log.h"
#endif
const char *s31_game_platform_name(s31_game_platform_t p) { static const char *v[]={"Unknown","NES","GB","GBC","SMS","Game Gear","SG-1000","PCE","GBA","Arcade"}; return p<=S31_GAME_ARCADE?v[p]:v[0]; }
const char *s31_game_platform_icon(s31_game_platform_t p) { static const char *v[]={"?","FC","GB","GBC","SMS","GG","SG","PCE","GBA","ARCADE"}; return p<=S31_GAME_ARCADE?v[p]:v[0]; }
bool s31_game_nes_mapper_supported(uint16_t mapper)
{
    static const uint16_t supported[] = {
        0,1,2,3,4,7,8,9,10,11,13,15,16,17,18,19,21,22,23,24,25,26,30,32,33,34,
        40,41,42,43,44,45,46,47,48,49,50,51,57,58,60,61,62,64,65,66,67,68,69,70,
        71,72,73,74,75,76,77,78,79,80,82,83,86,87,88,89,90,91,92,93,94,95,96,97,
        100,101,105,107,108,109,110,112,113,114,115,116,117,118,119,122,133,134,135,
        140,151,160,180,181,182,183,185,187,188,189,191,193,194,200,201,202,206,222,
        225,226,227,228,229,230,231,232,233,234,235,236,240,241,242,243,244,245,246,
        248,249,250,251,252,253,254,255
    };
    for (size_t i = 0; i < sizeof(supported) / sizeof(supported[0]); ++i)
        if (supported[i] == mapper) return true;
    return false;
}
s31_game_platform_t s31_game_platform_from_path(const char *path) { const char *d=strrchr(path?path:"",'.'); if(!d)return S31_GAME_UNKNOWN; if(!strcasecmp(d,".nes"))return S31_GAME_NES; if(!strcasecmp(d,".gb"))return S31_GAME_GB; if(!strcasecmp(d,".gbc"))return S31_GAME_GBC; if(!strcasecmp(d,".sms"))return S31_GAME_SMS; if(!strcasecmp(d,".gg"))return S31_GAME_GG; if(!strcasecmp(d,".sg")||!strcasecmp(d,".sg1000"))return S31_GAME_SG1000; if(!strcasecmp(d,".pce"))return S31_GAME_PCE; if(!strcasecmp(d,".gba"))return S31_GAME_GBA; if(!strcasecmp(d,".zip"))return S31_GAME_ARCADE; return S31_GAME_UNKNOWN; }
int s31_game_probe_nes(const uint8_t *h,size_t n,uint16_t *m,bool *ok) { if(!h||n<8||!m||!ok||memcmp(h,"NES\x1a",4))return -1; *m=(uint16_t)((h[6]>>4)|(h[7]&0xf0)); *ok=s31_game_nes_mapper_supported(*m); return 0; }

static void scan_dir(const char *root, s31_game_catalog_t *catalog, unsigned depth)
{
    if (depth > 6 || catalog->count >= S31_GAME_CATALOG_MAX) return;
    DIR *dir = opendir(root);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) && catalog->count < S31_GAME_CATALOG_MAX) {
        if (entry->d_name[0] == '.') continue;
        char path[256];
        int n = snprintf(path, sizeof(path), "%s/%s", root, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(path)) continue;
#ifdef DT_DIR
        /* 优先利用 d_type 判断目录/文件类型，避免对无关文件和子目录执行大量 stat() */
        if (entry->d_type == DT_DIR) {
            scan_dir(path, catalog, depth + 1);
            continue;
        }
        if (entry->d_type == DT_REG) {
            s31_game_platform_t quick_platform = s31_game_platform_from_path(entry->d_name);
            if (quick_platform == S31_GAME_UNKNOWN) continue;
        }
#endif
        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { scan_dir(path, catalog, depth + 1); continue; }
        if (!S_ISREG(st.st_mode)) continue;
        s31_game_platform_t platform = s31_game_platform_from_path(path);
        if (platform == S31_GAME_UNKNOWN) continue;
        s31_game_entry_t *item = &catalog->entries[catalog->count++];
        memset(item, 0, sizeof(*item));
        memcpy(item->path, path, (size_t)n + 1);
#ifdef ESP_PLATFORM
        static bool logged_utf8_name;
        if (!logged_utf8_name) {
            for (const unsigned char *p = (const unsigned char *)entry->d_name; *p; ++p) {
                if (*p >= 0x80) {
                    ESP_LOGI("s31_catalog", "UTF-8 filename from FAT: %s", entry->d_name);
                    logged_utf8_name = true;
                    break;
                }
            }
        }
#endif
        /*
         * Directory entries are UTF-8 on the FAT VFS.  A plain strncpy can
         * cut a three-byte Chinese character in half when a long filename is
         * copied into title[128].  LVGL then sees an invalid UTF-8 sequence
         * and the game name is rendered as missing text.  Keep the copy
         * bounded, but only terminate at a complete UTF-8 code point.
         */
        size_t title_limit = sizeof(item->title) - 1;
        size_t title_len = 0;
        while (entry->d_name[title_len] && title_len < title_limit) {
            unsigned char first = (unsigned char)entry->d_name[title_len];
            size_t codepoint_len = first < 0x80 ? 1u :
                                   (first < 0xe0 ? 2u :
                                    (first < 0xf0 ? 3u : 4u));
            if (title_len + codepoint_len > title_limit) break;
            title_len += codepoint_len;
        }
        memcpy(item->title, entry->d_name, title_len);
        item->title[title_len] = '\0';
        item->platform = platform;
        item->size = (size_t)st.st_size;
        item->supported = platform == S31_GAME_GB || platform == S31_GAME_GBC ||
                          platform == S31_GAME_SMS || platform == S31_GAME_GG ||
                          platform == S31_GAME_SG1000 || platform == S31_GAME_PCE;
        if (platform == S31_GAME_NES) {
            FILE *file = fopen(path, "rb");
            uint8_t header[8];
            size_t read = 0;
            if (file) {
                /* 仅需读取 8 字节 iNES 头，关闭 stdio 预读缓冲以减少 SD 扇区读取 */
                setvbuf(file, NULL, _IONBF, 0);
                read = fread(header, 1, sizeof(header), file);
                fclose(file);
            }
            item->valid_header = s31_game_probe_nes(header, read, &item->mapper, &item->supported) == 0;
        }
    }
    closedir(dir);
}

#define S31_CATALOG_CACHE_MAGIC 0x31433353u
#define S31_CATALOG_CACHE_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t entry_size;
    uint32_t count;
} s31_catalog_cache_header_t;

int s31_game_catalog_load_cache(const char *cache_path, s31_game_catalog_t *catalog)
{
    if (!cache_path || !catalog) return -1;
    FILE *file = fopen(cache_path, "rb");
    if (!file) return -1;
    s31_catalog_cache_header_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, file) != 1 ||
        hdr.magic != S31_CATALOG_CACHE_MAGIC ||
        hdr.version != S31_CATALOG_CACHE_VERSION ||
        hdr.entry_size != sizeof(s31_game_entry_t) ||
        hdr.count > S31_GAME_CATALOG_MAX) {
        fclose(file);
        return -1;
    }
    memset(catalog, 0, sizeof(*catalog));
    if (hdr.count > 0 && fread(catalog->entries, sizeof(s31_game_entry_t), hdr.count, file) != hdr.count) {
        fclose(file);
        memset(catalog, 0, sizeof(*catalog));
        return -1;
    }
    fclose(file);
    catalog->count = hdr.count;
    return (int)catalog->count;
}

int s31_game_catalog_save_cache(const char *cache_path, const s31_game_catalog_t *catalog)
{
    if (!cache_path || !catalog || catalog->count > S31_GAME_CATALOG_MAX) return -1;
    char tmp_path[280];
    int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", cache_path);
    if (n < 0 || (size_t)n >= sizeof(tmp_path)) return -1;
    FILE *file = fopen(tmp_path, "wb");
    if (!file) return -1;
    s31_catalog_cache_header_t hdr = {
        .magic = S31_CATALOG_CACHE_MAGIC,
        .version = S31_CATALOG_CACHE_VERSION,
        .entry_size = (uint32_t)sizeof(s31_game_entry_t),
        .count = (uint32_t)catalog->count,
    };
    bool ok = fwrite(&hdr, sizeof(hdr), 1, file) == 1;
    if (ok && catalog->count > 0) {
        ok = fwrite(catalog->entries, sizeof(s31_game_entry_t), catalog->count, file) == catalog->count;
    }
    if (fclose(file) != 0) ok = false;
    if (!ok) {
        remove(tmp_path);
        return -1;
    }
    remove(cache_path);
    if (rename(tmp_path, cache_path) != 0) {
        remove(tmp_path);
        return -1;
    }
    return 0;
}

static int entry_compare(const void *left, const void *right)
{
    const s31_game_entry_t *a = left, *b = right;
    if (a->platform != b->platform) return (int)a->platform - (int)b->platform;
    return strcasecmp(a->title, b->title);
}

int s31_game_catalog_scan(const char *root, s31_game_catalog_t *catalog)
{
    if (!root || !catalog) return -1;
    memset(catalog, 0, sizeof(*catalog));
    scan_dir(root, catalog, 0);
    s31_game_catalog_sort(catalog);
    return (int)catalog->count;
}

void s31_game_catalog_sort(s31_game_catalog_t *catalog)
{
    if (catalog && catalog->count > 1) qsort(catalog->entries, catalog->count,
                                               sizeof(catalog->entries[0]), entry_compare);
}

size_t s31_game_catalog_page_move(size_t selected, size_t count, int direction)
{
    if (!count) return 0;
    size_t page = selected / 10u;
    size_t page_count = (count + 9u) / 10u;
    if (direction < 0) {
        return page ? (page - 1u) * 10u : (page_count - 1u) * 10u;
    }
    size_t next = (page + 1u) * 10u;
    return next < count ? next : 0;
}

uint32_t s31_game_catalog_repeat(s31_game_nav_repeat_t *state, uint32_t buttons,
                                 uint32_t pressed, uint32_t now_ms)
{
    const uint32_t nav_mask = 0x0fu;
    uint32_t held = buttons & nav_mask;
    uint32_t initial = pressed & nav_mask;
    if (!held) { memset(state, 0, sizeof(*state)); return 0; }
    uint32_t button = held & (uint32_t)(-(int32_t)held);
    if (initial & button || state->held_button != button) {
        state->held_button = button;
        state->pressed_at_ms = now_ms;
        state->next_repeat_ms = now_ms + 400u;
        return button;
    }
    if ((int32_t)(now_ms - state->next_repeat_ms) >= 0) {
        state->next_repeat_ms += 100u;
        if ((int32_t)(now_ms - state->next_repeat_ms) >= 0)
            state->next_repeat_ms = now_ms + 100u;
        return button;
    }
    return 0;
}
