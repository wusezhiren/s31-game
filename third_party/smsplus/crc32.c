#include "shared.h"

uint32 smsplus_crc32(uint32 crc, const void *data, size_t size)
{
  const uint8 *bytes = data;
  crc = ~crc;
  while (size--)
  {
    crc ^= *bytes++;
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320u & (uint32)-(int32)(crc & 1));
  }
  return ~crc;
}
