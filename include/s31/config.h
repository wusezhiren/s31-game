#ifndef S31_CONFIG_H
#define S31_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define S31_CPU_HZ 320000000u
#define S31_PSRAM_BYTES (16u * 1024u * 1024u)
#define S31_SCREEN_WIDTH 800u
#define S31_SCREEN_HEIGHT 480u
#define S31_FRAMEBUFFER_BYTES (S31_SCREEN_WIDTH * S31_SCREEN_HEIGHT * 2u)
#define S31_AUDIO_RATE 48000u
#define S31_AUDIO_CHANNELS 2u

/* Keep the ROM and framebuffer budgets separate: the display buffer is not
 * available to an emulator core. The loader uses the PSRAM budget below. */
#define S31_ROM_BUDGET_BYTES (S31_PSRAM_BYTES - S31_FRAMEBUFFER_BYTES)

typedef enum {
    S31_TIER_EASY = 0,
    S31_TIER_OPTIMIZED = 1,
    S31_TIER_UNSUPPORTED = 2
} s31_tier_t;

typedef enum {
    S31_PLATFORM_FC,
    S31_PLATFORM_GB,
    S31_PLATFORM_GBC,
    S31_PLATFORM_SMS,
    S31_PLATFORM_GG,
    S31_PLATFORM_SG1000,
    S31_PLATFORM_PCE,
    S31_PLATFORM_ATARI2600,
    S31_PLATFORM_ATARI7800,
    S31_PLATFORM_NGP,
    S31_PLATFORM_NGPC,
    S31_PLATFORM_WONDERSWAN,
    S31_PLATFORM_COLECOVISION,
    S31_PLATFORM_VECTREX,
    S31_PLATFORM_GAMEWATCH,
    S31_PLATFORM_GBA,
    S31_PLATFORM_SNES,
    S31_PLATFORM_MD,
    S31_PLATFORM_ARCADE_EARLY,
    S31_PLATFORM_CPS1,
    S31_PLATFORM_ATARI_LYNX,
    S31_PLATFORM_VIRTUAL_BOY,
    S31_PLATFORM_CPS2,
    S31_PLATFORM_CPS3,
    S31_PLATFORM_NEOGEO,
    S31_PLATFORM_PS1,
    S31_PLATFORM_N64,
    S31_PLATFORM_DC,
    S31_PLATFORM_SATURN,
    S31_PLATFORM_THREEDO,
    S31_PLATFORM_SIXTYFOUR_X,
    S31_PLATFORM_NDS,
    S31_PLATFORM_THREEDS,
    S31_PLATFORM_CD_FULL,
    S31_PLATFORM_COUNT
} s31_platform_id_t;

typedef struct {
    s31_platform_id_t id;
    const char *name;
    s31_tier_t tier;
    size_t max_rom_bytes;
    uint16_t target_fps;
    uint8_t needs_scanline_timing;
    uint8_t needs_streaming_cd;
} s31_platform_info_t;

const s31_platform_info_t *s31_platform_info(s31_platform_id_t id);
const char *s31_tier_name(s31_tier_t tier);

#endif
