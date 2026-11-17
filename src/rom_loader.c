#include "s31/rom_loader.h"
#include <stdlib.h>
#include <string.h>

s31_rom_result_t s31_rom_load(s31_rom_t *rom, const uint8_t *data,
                              size_t size, size_t platform_limit) {
    if (!rom || (!data && size)) return S31_ROM_INVALID_ARGUMENT;
    if (size > platform_limit) return S31_ROM_TOO_LARGE;
    uint8_t *copy = size ? malloc(size) : NULL;
    if (size && !copy) return S31_ROM_OUT_OF_MEMORY;
    if (size) memcpy(copy, data, size);
    s31_rom_unload(rom);
    rom->data = copy;
    rom->size = size;
    return S31_ROM_OK;
}

void s31_rom_unload(s31_rom_t *rom) {
    if (!rom) return;
    free(rom->data);
    rom->data = NULL;
    rom->size = 0;
}
