#include "s31/game_catalog.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
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
        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { scan_dir(path, catalog, depth + 1); continue; }
        if (!S_ISREG(st.st_mode)) continue;
        s31_game_platform_t platform = s31_game_platform_from_path(path);
        if (platform == S31_GAME_UNKNOWN) continue;
        s31_game_entry_t *item = &catalog->entries[catalog->count++];
        memset(item, 0, sizeof(*item));
        strncpy(item->path, path, sizeof(item->path) - 1);
        strncpy(item->title, entry->d_name, sizeof(item->title) - 1);
        item->platform = platform;
        item->size = (size_t)st.st_size;
        item->supported = platform == S31_GAME_GB || platform == S31_GAME_GBC ||
                          platform == S31_GAME_SMS || platform == S31_GAME_GG ||
                          platform == S31_GAME_SG1000 || platform == S31_GAME_PCE;
        if (platform == S31_GAME_NES) {
            FILE *file = fopen(path, "rb");
            uint8_t header[8];
            size_t read = file ? fread(header, 1, sizeof(header), file) : 0;
            if (file) fclose(file);
            item->valid_header = s31_game_probe_nes(header, read, &item->mapper, &item->supported) == 0;
        }
    }
    closedir(dir);
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
    if (direction < 0) return page ? (page - 1u) * 10u : 0;
    size_t next = (page + 1u) * 10u;
    return next < count ? next : selected;
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
