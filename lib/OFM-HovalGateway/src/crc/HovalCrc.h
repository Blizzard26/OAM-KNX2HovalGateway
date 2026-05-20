#pragma once

#include <Arduino.h>
#include <FastCRC.h>
#include <FastCRC_tables.h>
#include <cstddef>
#include <cstdint>

namespace HovalCrc
{
  // Reflected of 0xb006
  static constexpr uint16_t CRC_INITIAL_VALUE = 0x600D;

#define crc_n4(crc, data, table)                                                                                                                               \
  crc ^= data;                                                                                                                                                 \
  crc = pgm_read_word(&table[(crc & 0xff) + 0x300]) ^ pgm_read_word(&table[((crc >> 8) & 0xff) + 0x200]) ^                                                     \
        pgm_read_word(&table[((data >> 16) & 0xff) + 0x100]) ^ pgm_read_word(&table[data >> 24]);

  /**
   * Calculate CRC16 checksum
   *
   * CRC16:
   * poly=0x1021
   * init=0xb006
   * refin=true
   * refout=true
   * xorout=0x0000
   * check=0xff5a
   * residue=0x0000
   *
   * Copied from FastCRC (https://github.com/FrankBoesing/FastCRC/) to allow custom / special init value.
   *
   * @param data Pointer to data buffer
   * @param length Length of data in bytes
   * @return Calculated CRC16 value
   */
  inline static uint16_t calcCrc(const uint8_t* data, size_t length)
  {
    uint16_t crc = CRC_INITIAL_VALUE;

    while (((uintptr_t)data & 3) && length)
    {
      crc = (crc >> 8) ^ pgm_read_word(&crc_table_kermit[(crc & 0xff) ^ *data++]);
      length--;
    }

    while (length >= 16)
    {
      length -= 16;
      crc_n4(crc, ((uint32_t*)data)[0], crc_table_kermit);
      crc_n4(crc, ((uint32_t*)data)[1], crc_table_kermit);
      crc_n4(crc, ((uint32_t*)data)[2], crc_table_kermit);
      crc_n4(crc, ((uint32_t*)data)[3], crc_table_kermit);
      data += 16;
    }

    while (length--)
    {
      crc = (crc >> 8) ^ pgm_read_word(&crc_table_kermit[(crc & 0xff) ^ *data++]);
    }

    return crc;
  }
}
