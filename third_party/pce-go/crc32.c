#include "pce-go.h"

uint32_t pce_crc32(uint32_t crc, const void *data, size_t size)
{
    const uint8_t *bytes = data;
    crc = ~crc;
    while (size--) {
        crc ^= *bytes++;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (uint32_t)-(int32_t)(crc & 1));
    }
    return ~crc;
}
