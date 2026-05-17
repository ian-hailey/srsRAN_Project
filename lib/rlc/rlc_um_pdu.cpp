/*
 *
 * Copyright 2021-2026 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "srsran/rlc/rlc_um_pdu.h"

namespace srsran {

bool rlc_um_read_data_pdu_header(byte_buffer& pdu, rlc_um_sn_size sn_size, rlc_um_pdu_header* hdr)
{
  if (pdu.length() < 1) {
    return false;
  }

  // Access bytes via operator[] which is iterator-based random access
  uint8_t first_byte = pdu[0];

  // Extract SI field from upper 2 bits
  rlc_si_field si = static_cast<rlc_si_field>((first_byte >> 6) & 0x03);
  hdr->si = si;
  hdr->sn_size = sn_size;

  bool is_6bit = (sn_size == rlc_um_sn_size::size6bits);

  // Full SDU case - no SN, no SO
  if (si == rlc_si_field::full_sdu) {
    // Check reserved bits are zero
    if ((first_byte & 0x3f) != 0) {
      return false;
    }
    hdr->sn = 0;
    hdr->so = 0;
    pdu.trim_head(1);
    return true;
  }

  // First segment - has SN, no SO
  if (si == rlc_si_field::first_segment) {
    if (is_6bit) {
      // 6-bit SN in lower 6 bits of first byte (no reserved bits - lower bits are SN)
      if (pdu.length() < 1) {
        return false;
      }
      hdr->sn = first_byte & 0x3f;
      hdr->so = 0;
      pdu.trim_head(1);
      return true;
    } else {
      // 12-bit SN across two bytes
      if (pdu.length() < 2) {
        return false;
      }
      uint8_t second_byte = pdu[1];
      // Check reserved bits in second byte (upper 2 bits)
      if ((second_byte & 0xc0) != 0) {
        return false;
      }
      // SN = bits[5:0] of byte0 + bits[7:0] of byte1 (lower 6 bits used)
      hdr->sn = ((uint32_t)(first_byte & 0x3f) << 6) | (second_byte & 0x3f);
      hdr->so = 0;
      pdu.trim_head(2);
      return true;
    }
  }

  // Middle or Last segment - has SN and SO
  if (si == rlc_si_field::middle_segment || si == rlc_si_field::last_segment) {
    if (is_6bit) {
      // 6-bit SN + 16-bit SO = 3 bytes
      if (pdu.length() < 3) {
        return false;
      }
      uint8_t second_byte = pdu[1];
      uint8_t third_byte = pdu[2];
      // SN occupies lower 6 bits of first byte
      hdr->sn = first_byte & 0x3f;
      hdr->so = (uint16_t(second_byte) << 8) | third_byte;
      pdu.trim_head(3);
      return true;
    } else {
      // 12-bit SN + 16-bit SO = 4 bytes
      if (pdu.length() < 4) {
        return false;
      }
      uint8_t second_byte = pdu[1];
      uint8_t third_byte = pdu[2];
      uint8_t fourth_byte = pdu[3];
      // Check reserved bits in second byte (upper 2 bits for 12-bit SN)
      if ((second_byte & 0xc0) != 0) {
        return false;
      }
      // For middle/last segments with 12-bit SN, SN uses 8-bit shift
      hdr->sn = ((uint32_t)(first_byte & 0x3f) << 8) | second_byte;
      hdr->so = (uint16_t(third_byte) << 8) | fourth_byte;
      pdu.trim_head(4);
      return true;
    }
  }

  return false;
}

size_t rlc_um_write_data_pdu_header(span<uint8_t> buf, const rlc_um_pdu_header& hdr)
{
  if (buf.empty()) {
    return 0;
  }

  bool is_6bit = (hdr.sn_size == rlc_um_sn_size::size6bits);
  uint8_t si_value = static_cast<uint8_t>(hdr.si) & 0x03;

  // Full SDU case - no SN, no SO
  if (hdr.si == rlc_si_field::full_sdu) {
    buf[0] = (si_value << 6);
    return 1;
  }

  // First segment - has SN, no SO
  if (hdr.si == rlc_si_field::first_segment) {
    if (is_6bit) {
      if (buf.size() < 1) {
        return 0;
      }
      buf[0] = (si_value << 6) | (hdr.sn & 0x3f);
      return 1;
    } else {
      if (buf.size() < 2) {
        return 0;
      }
      // First byte: SI(2) + SN_upper(6)
      buf[0] = (si_value << 6) | ((hdr.sn >> 6) & 0x3f);
      // Second byte: SN_lower(6) + reserved(2=0)
      buf[1] = (hdr.sn & 0x3f);
      return 2;
    }
  }

  // Middle or Last segment - has SN and SO
  if (hdr.si == rlc_si_field::middle_segment || hdr.si == rlc_si_field::last_segment) {
    if (is_6bit) {
      if (buf.size() < 3) {
        return 0;
      }
      buf[0] = (si_value << 6) | (hdr.sn & 0x3f);
      buf[1] = (hdr.so >> 8) & 0xff;
      buf[2] = hdr.so & 0xff;
      return 3;
    } else {
      if (buf.size() < 4) {
        return 0;
      }
      // First byte: SI(2) + SN_upper(6)
      buf[0] = (si_value << 6) | ((hdr.sn >> 8) & 0x3f);
      // Second byte: SN_lower(8)
      buf[1] = hdr.sn & 0xff;
      buf[2] = (hdr.so >> 8) & 0xff;
      buf[3] = hdr.so & 0xff;
      return 4;
    }
  }

  return 0;
}

} // namespace srsran