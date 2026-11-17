#ifndef S31_ROM_LOADER_H
#define S31_ROM_LOADER_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    S31_ROM_OK = 0,
    S31_ROM_INVALID_ARGUMENT,
    S31_ROM_TOO_LARGE,
    S31_ROM_OUT_OF_MEMORY
} s31_rom_result_t;

typedef struct {
    uint8_t *data;
    size_t size;
} s31_rom_t;

s31_rom_result_t s31_rom_load(s31_rom_t *rom, const uint8_t *data,
                              size_t size, size_t platform_limit);
void s31_rom_unload(s31_rom_t *rom);

#endif
