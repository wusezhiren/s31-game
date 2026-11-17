#ifndef S31_GAME_CATALOG_H
#define S31_GAME_CATALOG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum { S31_GAME_UNKNOWN = 0, S31_GAME_NES, S31_GAME_GB, S31_GAME_GBC, S31_GAME_SMS, S31_GAME_GG, S31_GAME_SG1000, S31_GAME_PCE, S31_GAME_GBA, S31_GAME_ARCADE } s31_game_platform_t;
typedef struct { char path[256]; char title[128]; s31_game_platform_t platform; size_t size; uint16_t mapper; bool valid_header; bool supported; } s31_game_entry_t;
#define S31_GAME_CATALOG_MAX 512
typedef struct { s31_game_entry_t entries[S31_GAME_CATALOG_MAX]; size_t count; } s31_game_catalog_t;
typedef struct { uint32_t held_button; uint32_t pressed_at_ms; uint32_t next_repeat_ms; } s31_game_nav_repeat_t;
const char *s31_game_platform_name(s31_game_platform_t platform);
const char *s31_game_platform_icon(s31_game_platform_t platform);
s31_game_platform_t s31_game_platform_from_path(const char *path);
int s31_game_probe_nes(const uint8_t *header, size_t length, uint16_t *mapper, bool *supported);
int s31_game_catalog_scan(const char *root, s31_game_catalog_t *catalog);
void s31_game_catalog_sort(s31_game_catalog_t *catalog);
size_t s31_game_catalog_page_move(size_t selected, size_t count, int direction);
uint32_t s31_game_catalog_repeat(s31_game_nav_repeat_t *state, uint32_t buttons,
                                 uint32_t pressed, uint32_t now_ms);
bool s31_game_nes_mapper_supported(uint16_t mapper);
#endif
