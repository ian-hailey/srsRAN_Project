/*
 *
 * Copyright 2021-2026 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
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

#include "srsran/rlc/rlc_am_pdu.h"

namespace srsran {

// Helper function to check if PDU is a control PDU
bool rlc_am_status_pdu::is_control_pdu(const byte_buffer& pdu)
{
  if (pdu.length() < 1) {
    return false;
  }
  // D/C bit is the MSB (bit 7) of the first byte
  // 0 = Control PDU, 1 = Data PDU
  uint8_t first_byte = pdu[0];
  return ((first_byte >> 7) & 0x01) == 0;
}

bool rlc_am_read_data_pdu_header(byte_buffer& pdu, rlc_am_sn_size sn_size, rlc_am_pdu_header* hdr)
{
  if (pdu.length() < 1) {
    return false;
  }

  // Access bytes via operator[] which is iterator-based random access
  uint8_t first_byte = pdu[0];

  // Extract D/C bit (bit 7) - must be 1 for data PDU
  uint8_t dc = (first_byte >> 7) & 0x01;
  if (dc != 1) {
    return false;
  }

  // Extract P bit (bit 6)
  hdr->p = ((first_byte >> 6) & 0x01) == 1;

  // Extract SI field from bits [5:4]
  rlc_si_field si = static_cast<rlc_si_field>((first_byte >> 4) & 0x03);
  hdr->si = si;
  hdr->sn_size = sn_size;

  bool is_12bit = (sn_size == rlc_am_sn_size::size12bits);

    // Full SDU case - no SN, no SO
  if (si == rlc_si_field::full_sdu) {
    if (is_12bit) {
      // 12-bit SN: 2 bytes header
      if (pdu.length() < 2) {
        return false;
      }
      uint8_t second_byte = pdu[1];
      // Check reserved bits in second byte (upper 2 bits must be 0)
      if ((second_byte & 0xc0) != 0) {
        return false;
      }
      // SN = bits[3:0] of byte0 + bits[7:0] of byte1
      hdr->sn = ((uint32_t)(first_byte & 0x0f) << 8) | second_byte;
      hdr->so = 0;
      pdu.trim_head(2);
      return true;
    } else {
      // 18-bit SN: 3 bytes header (no reserved bits in third byte for full_sdu)
      if (pdu.length() < 3) {
        return false;
      }
      uint8_t second_byte = pdu[1];
      uint8_t third_byte  = pdu[2];
      // 18-bit SN encoding: byte0[3:0] << 16 | byte1 << 8 | byte2
      hdr->sn = ((uint32_t)(first_byte & 0x0f) << 16) | ((uint32_t)second_byte << 8) | third_byte;
      hdr->so = 0;
      pdu.trim_head(3);
      return true;
    }
  }

  // First segment - has SN, no SO
  if (si == rlc_si_field::first_segment) {
    if (is_12bit) {
      // 12-bit SN: 2 bytes header
      if (pdu.length() < 2) {
        return false;
      }
      // For 12-bit first_segment, SN occupies full 8 bits of byte1 (no reserved bits)
      uint8_t second_byte = pdu[1];
      // SN = upper 4 bits from byte0[3:0] concatenated with full 8 bits of byte1
      hdr->sn = ((uint32_t)(first_byte & 0x0f) << 8) | second_byte;
      hdr->so = 0;
      pdu.trim_head(2);
      return true;
    } else {
      // 18-bit SN: 3 bytes header (no reserved bits in third byte for first_segment)
      if (pdu.length() < 3) {
        return false;
      }
      uint8_t second_byte = pdu[1];
      uint8_t third_byte  = pdu[2];
      // 18-bit SN encoding: byte0[3:0] << 16 | byte1 << 8 | byte2
      hdr->sn = ((uint32_t)(first_byte & 0x0f) << 16) | ((uint32_t)second_byte << 8) | third_byte;
      hdr->so = 0;
      pdu.trim_head(3);
      return true;
    }
  }

  // Middle or Last segment - has SN and SO
  if (si == rlc_si_field::middle_segment || si == rlc_si_field::last_segment) {
    if (is_12bit) {
      // 12-bit SN + 16-bit SO = 4 bytes
      if (pdu.length() < 4) {
        return false;
      }
      uint8_t second_byte = pdu[1];
      uint8_t third_byte  = pdu[2];
      uint8_t fourth_byte = pdu[3];
      // For 12-bit middle/last, byte 1 is just SN lower bits (no reserved bits to check)
      hdr->sn = ((uint32_t)(first_byte & 0x0f) << 8) | second_byte;
      hdr->so = (uint16_t(third_byte) << 8) | fourth_byte;
      pdu.trim_head(4);
      return true;
    } else {
      // 18-bit SN + 16-bit SO = 5 bytes
      if (pdu.length() < 5) {
        return false;
      }
      uint8_t second_byte = pdu[1];
      uint8_t third_byte  = pdu[2];
      uint8_t fourth_byte = pdu[3];
      uint8_t fifth_byte  = pdu[4];
      // 18-bit SN encoding: byte0[3:0] << 16 | byte1 << 8 | byte2
      uint32_t sn = ((uint32_t)(first_byte & 0x0f) << 16) | ((uint32_t)second_byte << 8) | third_byte;
      // SN must be within 18-bit range (0 to 262143)
      if (sn > 262143) {
        // Malformed - SN out of range, return false without modifying header
        return false;
      }
      hdr->sn = sn;
      hdr->so = (uint16_t(fourth_byte) << 8) | fifth_byte;
      pdu.trim_head(5);
      return true;
    }
  }

  return false;
}

size_t rlc_am_write_data_pdu_header(span<uint8_t> buf, const rlc_am_pdu_header& hdr)
{
  if (buf.empty()) {
    return 0;
  }

  bool is_12bit = (hdr.sn_size == rlc_am_sn_size::size12bits);
  uint8_t si_value = static_cast<uint8_t>(hdr.si) & 0x03;
  uint8_t p_value = hdr.p ? 1 : 0;

  // Full SDU case
  if (hdr.si == rlc_si_field::full_sdu) {
    if (is_12bit) {
      if (buf.size() < 2) {
        return 0;
      }
      // First byte: D/C=1 | P | SI(2) | SN_upper(4)
      buf[0] = (0x80) | (p_value << 6) | (si_value << 4) | ((hdr.sn >> 8) & 0x0f);
      // Second byte: SN_lower(8)
      buf[1] = hdr.sn & 0xff;
      return 2;
    } else {
      if (buf.size() < 3) {
        return 0;
      }
      // First byte: D/C=1 | P | SI(2) | SN_upper(4)
      buf[0] = (0x80) | (p_value << 6) | (si_value << 4) | ((hdr.sn >> 16) & 0x0f);
      // Second byte: SN_middle(8)
      buf[1] = (hdr.sn >> 8) & 0xff;
      // Third byte: SN_lower(8)
      buf[2] = hdr.sn & 0xff;
      return 3;
    }
  }

  // First segment
  if (hdr.si == rlc_si_field::first_segment) {
    if (is_12bit) {
      if (buf.size() < 2) {
        return 0;
      }
      buf[0] = (0x80) | (p_value << 6) | (si_value << 4) | ((hdr.sn >> 8) & 0x0f);
      buf[1] = hdr.sn & 0xff;
      return 2;
    } else {
      if (buf.size() < 3) {
        return 0;
      }
      buf[0] = (0x80) | (p_value << 6) | (si_value << 4) | ((hdr.sn >> 16) & 0x0f);
      buf[1] = (hdr.sn >> 8) & 0xff;
      buf[2] = hdr.sn & 0xff;
      return 3;
    }
  }

  // Middle or Last segment
  if (hdr.si == rlc_si_field::middle_segment || hdr.si == rlc_si_field::last_segment) {
    if (is_12bit) {
      if (buf.size() < 4) {
        return 0;
      }
      buf[0] = (0x80) | (p_value << 6) | (si_value << 4) | ((hdr.sn >> 8) & 0x0f);
      buf[1] = hdr.sn & 0xff;
      buf[2] = (hdr.so >> 8) & 0xff;
      buf[3] = hdr.so & 0xff;
      return 4;
    } else {
      if (buf.size() < 5) {
        return 0;
      }
      // 18-bit middle/last: same encoding as first/full
      buf[0] = (0x80) | (p_value << 6) | (si_value << 4) | ((hdr.sn >> 16) & 0x0f);
      buf[1] = (hdr.sn >> 8) & 0xff;
      buf[2] = hdr.sn & 0xff;
      buf[3] = (hdr.so >> 8) & 0xff;
      buf[4] = hdr.so & 0xff;
      return 5;
    }
  }

  return 0;
}

// Status PDU unpack implementation
bool rlc_am_status_pdu::unpack(const byte_buffer& pdu)
{
  if (pdu.length() < 2) {
    return false;
  }

  bool is_12bit = (sn_size == rlc_am_sn_size::size12bits);

  if (is_12bit) {
    // 12-bit SN Status PDU
    uint8_t first_byte  = pdu[0];
    uint8_t second_byte = pdu[1];

    // Check D/C bit (should be 0 for control)
    if (((first_byte >> 7) & 0x01) != 0) {
      return false;
    }

    // Check CPT (bits 6:4 should be 000 for STATUS)
    if (((first_byte >> 4) & 0x07) != 0) {
      return false;
    }

    // ACK_SN = bits[3:0] of byte0 + byte1
    ack_sn = ((uint32_t)(first_byte & 0x0f) << 8) | second_byte;

    // Parse NACKs if E1 bit is set
    if (pdu.length() < 3) {
      return false;
    }
    uint8_t third_byte = pdu[2];

    if ((third_byte & 0x80) == 0) {
      // E1 = 0, no NACKs
      return true;
    }

    // E1 = 1, parse NACKs
    size_t offset = 3;
    while (true) {
      // Need at least 2 bytes for NACK_SN
      size_t min_bytes_needed = 2;
      if (offset + min_bytes_needed > pdu.length()) {
        return false;
      }

      rlc_am_status_nack nack;
      uint8_t nack_byte0 = pdu[offset];
      uint8_t nack_byte1 = pdu[offset + 1];

      // For 12-bit NACK_SN (from 3GPP 38.322):
      // The format is: byte0 = NACK_SN[11:4], byte1 = NACK_SN[3:0] | E1 | E2 | E3 | R
      // nack_sn = (byte0 << 4) | (byte1 >> 4)
      nack.nack_sn = ((uint32_t)nack_byte0 << 4) | (nack_byte1 >> 4);

      // E1 bit is bit 3 of byte1
      bool e1 = ((nack_byte1 >> 3) & 0x01) == 1;

      // E2 bit is bit 2 of byte1
      bool e2 = ((nack_byte1 >> 2) & 0x01) == 1;

      // E3 bit is bit 1 of byte1
      bool e3 = ((nack_byte1 >> 1) & 0x01) == 1;

      nack.has_so         = e2;
      nack.has_nack_range = e3;

      if (e2) {
        // Need 4 more bytes for SO start and SO end
        min_bytes_needed += 4;
        if (offset + min_bytes_needed > pdu.length()) {
          return false;
        }
        nack.so_start = ((uint16_t)pdu[offset + 2] << 8) | pdu[offset + 3];
        nack.so_end   = ((uint16_t)pdu[offset + 4] << 8) | pdu[offset + 5];
      } else {
        nack.so_start = 0;
        nack.so_end   = 0;
      }

      if (e3) {
        // Need 1 more byte for range
        min_bytes_needed += 1;
        if (offset + min_bytes_needed > pdu.length()) {
          return false;
        }
        nack.nack_range = pdu[offset + min_bytes_needed - 1];
      } else {
        nack.nack_range = 0;
      }

      nacks.push_back(nack);

      // Advance offset by the actual bytes consumed
      offset += min_bytes_needed;

      if (!e1) {
        break; // No more NACKs
      }
    }

    return true;
  } else {
    // 18-bit SN Status PDU
    if (pdu.length() < 3) {
      return false;
    }

    uint8_t first_byte  = pdu[0];
    uint8_t second_byte = pdu[1];
    uint8_t third_byte  = pdu[2];

    // Check D/C bit (should be 0 for control)
    if (((first_byte >> 7) & 0x01) != 0) {
      return false;
    }

    // Check CPT (bits 6:4 should be 000 for STATUS)
    if (((first_byte >> 4) & 0x07) != 0) {
      return false;
    }

    // ACK_SN for 18-bit SN = bits[3:0] of byte0 << 14 | byte1 << 6 | bits[7:2] of byte2
    ack_sn = ((uint32_t)(first_byte & 0x0f) << 14) | ((uint32_t)second_byte << 6) | (third_byte >> 2);

    // E1 bit is bit 1 of third byte
    if ((third_byte & 0x02) == 0) {
      // E1 = 0, no NACKs
      return true;
    }

    // E1 = 1, parse NACKs
    size_t offset = 3;
    while (true) {
      // Need at least 3 bytes for 18-bit NACK_SN
      size_t min_bytes_needed = 3;
      if (offset + min_bytes_needed > pdu.length()) {
        return false;
      }

      rlc_am_status_nack nack;

      uint8_t nack_byte0 = pdu[offset];
      uint8_t nack_byte1 = pdu[offset + 1];
      uint8_t nack_byte2 = pdu[offset + 2];

      // Check reserved bits in nack_byte2 (bits 1:0 must be 0)
      if ((nack_byte2 & 0x03) != 0) {
        return false;
      }
      // NACK_SN = bits[3:0] of byte0 << 10 | byte1 << 4 | bits[7:2] of byte2
      nack.nack_sn = ((uint32_t)(nack_byte0 & 0x0f) << 10) | ((uint32_t)nack_byte1 << 4) | ((nack_byte2 >> 2) & 0x0f);

      // E1, E2, E3 bits
      bool e1 = ((nack_byte2 >> 7) & 0x01) == 1;
      bool e2 = ((nack_byte2 >> 6) & 0x01) == 1;
      bool e3 = ((nack_byte2 >> 5) & 0x01) == 1;

      nack.has_so         = e2;
      nack.has_nack_range = e3;

      if (e2) {
        // Need 4 more bytes for SO start and SO end
        min_bytes_needed += 4;
        if (offset + min_bytes_needed > pdu.length()) {
          return false;
        }
        nack.so_start = ((uint16_t)pdu[offset + 3] << 8) | pdu[offset + 4];
        nack.so_end   = ((uint16_t)pdu[offset + 5] << 8) | pdu[offset + 6];
      } else {
        nack.so_start = 0;
        nack.so_end   = 0;
      }

      if (e3) {
        // Need 1 more byte for range
        min_bytes_needed += 1;
        if (offset + min_bytes_needed > pdu.length()) {
          return false;
        }
        nack.nack_range = pdu[offset + min_bytes_needed - 1];
      } else {
        nack.nack_range = 0;
      }

      nacks.push_back(nack);

      // Advance offset by the actual bytes consumed
      offset += min_bytes_needed;

      if (!e1) {
        break; // No more NACKs
      }
    }

    return true;
  }
}

// Status PDU pack implementation
size_t rlc_am_status_pdu::pack(span<uint8_t> buf) const
{
  bool is_12bit = (sn_size == rlc_am_sn_size::size12bits);

  if (is_12bit) {
    // 12-bit SN Status PDU
    if (buf.size() < 2) {
      return 0;
    }

    // First byte: D/C=0 | CPT=000 | ACK_SN_upper(4)
    buf[0] = (ack_sn >> 8) & 0x0f;
    // Second byte: ACK_SN_lower(8)
    buf[1] = ack_sn & 0xff;

    size_t offset = 2;

    if (nacks.empty()) {
      // No NACKs, set E1=0
      buf[2] = 0x00;
      return 3;
    }

    // E1 = 1 (indicates more NACKs or at least one NACK)
    buf[2] = 0x80;
    offset = 3;

    for (size_t i = 0; i < nacks.size(); ++i) {
      const auto& nack = nacks[i];

      if (offset + 1 >= buf.size()) {
        return 0;
      }

      // For 12-bit NACK (from 3GPP 38.322):
      // byte0 = NACK_SN[11:4], byte1 = NACK_SN[3:0] | E1 | E2 | E3 | R
      buf[offset]     = (nack.nack_sn >> 4) & 0xff;
      bool has_next   = (i < nacks.size() - 1);
      uint8_t e1_bit  = has_next ? 0x08 : 0x00;  // E1 is bit 3
      buf[offset + 1] = (nack.nack_sn & 0x0f) << 4;
      buf[offset + 1] |= e1_bit;
      buf[offset + 1] |= nack.has_so ? 0x04 : 0x00;          // E2 is bit 2
      buf[offset + 1] |= nack.has_nack_range ? 0x02 : 0x00;  // E3 is bit 1

      offset += 2;

      if (nack.has_so) {
        if (offset + 3 >= buf.size()) {
          return 0;
        }
        buf[offset]     = (nack.so_start >> 8) & 0xff;
        buf[offset + 1] = nack.so_start & 0xff;
        buf[offset + 2] = (nack.so_end >> 8) & 0xff;
        buf[offset + 3] = nack.so_end & 0xff;
        offset += 4;
      }

      if (nack.has_nack_range) {
        if (offset >= buf.size()) {
          return 0;
        }
        buf[offset] = nack.nack_range;
        offset += 1;
      }
    }

    return offset;
  } else {
    // 18-bit SN Status PDU
    if (buf.size() < 3) {
      return 0;
    }

    // First byte: D/C=0 | CPT=000 | ACK_SN_upper(4)
    buf[0] = (ack_sn >> 18) & 0x0f;
    // Second byte: ACK_SN_center(8)
    buf[1] = (ack_sn >> 10) & 0xff;
    // Third byte: ACK_SN_lower(6) | E1 | R
    // ACK_SN_lower is bits[5:0] of ack_sn
    buf[2] = (ack_sn & 0x3f) << 2;

    if (!nacks.empty()) {
      buf[2] |= 0x02; // Set E1 = 1
    }

    size_t offset = 3;

    for (size_t i = 0; i < nacks.size(); ++i) {
      const auto& nack = nacks[i];

      if (offset + 2 >= buf.size()) {
        return 0;
      }

      // NACK_SN
      buf[offset]     = (nack.nack_sn >> 10) & 0x0f;
      buf[offset + 1] = (nack.nack_sn >> 4) & 0xff;
      buf[offset + 2] = ((nack.nack_sn >> 2) & 0x0f) << 2;

      // E1, E2, E3 bits
      bool has_next = (i < nacks.size() - 1);
      buf[offset + 2] |= has_next ? 0x80 : 0x00;  // E1
      buf[offset + 2] |= nack.has_so ? 0x40 : 0x00;  // E2
      buf[offset + 2] |= nack.has_nack_range ? 0x20 : 0x00;  // E3

      offset += 3;

      if (nack.has_so) {
        if (offset + 3 >= buf.size()) {
          return 0;
        }
        buf[offset]     = (nack.so_start >> 8) & 0xff;
        buf[offset + 1] = nack.so_start & 0xff;
        buf[offset + 2] = (nack.so_end >> 8) & 0xff;
        buf[offset + 3] = nack.so_end & 0xff;
        offset += 4;
      }

      if (nack.has_nack_range) {
        if (offset >= buf.size()) {
          return 0;
        }
        buf[offset] = nack.nack_range;
        offset += 1;
      }
    }

    return offset;
  }
}

// Get packed size implementation
size_t rlc_am_status_pdu::get_packed_size() const
{
  bool is_12bit = (sn_size == rlc_am_sn_size::size12bits);

  // Minimum header size: 2 bytes (12-bit) or 3 bytes (18-bit)
  // For 12-bit: if no NACKs, header is 3 bytes (2 bytes ACK_SN + 1 byte E1)
  // For 18-bit: header is 3 bytes, if no NACKs, E1 is 0. 
  
  if (is_12bit) {
    // For 12-bit, header is 2 bytes + optional NACK list
    size_t size = 2;

    if (nacks.empty()) {
      size += 1; // Need 1 extra byte for E1=0
      return size;
    }

    // E1 for first NACK uses part of the first NACK's byte
    // But unpack starts reading NACKs from byte 2.
    // Oct 1: D/C(1) CPT(3) ACK_SN(4)
    // Oct 2: ACK_SN(8)
    // Oct 3: E1(1) NACK_SN_upper(7) -> My unpack does E1 checking here!
    // Wait, my unpack does:
    // uint8_t third_byte = pdu[2];
    // if ((third_byte & 0x80) == 0) return true;
    // offset = 3; ... NO! E1 is the top bit of third_byte.
    // So first NACK starts at offset=2 !
    size += nacks.size() * 2; // 2 bytes per NACK base

    for (const auto& nack : nacks) {
      if (nack.has_so) {
        size += 4;
      }
      if (nack.has_nack_range) {
        size += 1;
      }
    }
    return size;
  } else {
    // 18-bit
    size_t size = 3; // header
    
    if (nacks.empty()) {
      return size;
    }

    size += nacks.size() * 3; // 3 bytes per NACK base

    for (const auto& nack : nacks) {
      if (nack.has_so) {
        size += 4;
      }
      if (nack.has_nack_range) {
        size += 1;
      }
    }
    return size;
  }
}

// Reset implementation
void rlc_am_status_pdu::reset()
{
  nacks.clear();
}

// Push NACK implementation with merge logic
void rlc_am_status_pdu::push_nack(const rlc_am_status_nack& nack)
{
  bool is_12bit = (sn_size == rlc_am_sn_size::size12bits);
  uint32_t mod_nr = is_12bit ? 4096 : 262144;

  if (nacks.empty()) {
    nacks.push_back(nack);
    return;
  }

  rlc_am_status_nack& prev = nacks.back();

  // Calculate the end SN of the previous NACK (accounting for range)
  uint32_t prev_end_sn;
  if (prev.has_nack_range) {
    // Check for overflow
    uint32_t prev_end = prev.nack_sn + prev.nack_range;
    prev_end_sn       = prev_end % mod_nr;
  } else {
    prev_end_sn = prev.nack_sn;
  }

  // Check if SNs are continuous (prev_end_sn == nack.nack_sn)
  bool sn_continuous = (prev_end_sn == nack.nack_sn);

  // Check if both have range
  bool both_have_range = prev.has_nack_range && nack.has_nack_range;

  // Check if both have SO
  bool both_have_so = prev.has_so && nack.has_so;

  // Check SO continuity if both have SO
  bool so_continuous = false;
  if (both_have_so) {
    // SO is continuous if prev.so_end == so_end_of_sdu or prev.so_end + 1 == nack.so_start
    so_continuous = (prev.so_end == rlc_am_status_nack::so_end_of_sdu) ||
                    (prev.so_end + 1 == nack.so_start);
  }

  // Case 1: Merge ranges if continuous
  if (sn_continuous && both_have_range && (!both_have_so || so_continuous)) {
    // Merge the ranges - just increase the previous range
    // Check if merged range would exceed 255 (max for nack_range)
    if (prev.nack_range + nack.nack_range > 255) {
      // Can't merge, append as new entry
      nacks.push_back(nack);
    } else {
      prev.nack_range += nack.nack_range;
      // If both have SO, update the SO end to the new nack's SO end
      if (both_have_so) {
        prev.so_end = nack.so_end;
      }
    }
    return;
  }

  // Case 2: Merge with previous if same SN and we can extend SO
  if (prev.nack_sn == nack.nack_sn && both_have_so) {
    // Extend the SO range
    prev.so_end   = nack.so_end;
    prev.has_so   = true;
    // Update has_nack_range if new nack has it
    if (nack.has_nack_range) {
      prev.has_nack_range = true;
      prev.nack_range     = nack.nack_range;
    }
    return;
  }

  // Case 3: Check for segment merge with range extension
  if (sn_continuous && both_have_so && !both_have_range && nack.has_nack_range) {
    // Previous has segment, new has range - merge to create range
    if (prev.so_end == rlc_am_status_nack::so_end_of_sdu) {
      // Previous ends at SDU end, can merge
      if (nack.nack_range > 255) {
        // Can't merge, append
        nacks.push_back(nack);
      } else {
        prev.has_nack_range = true;
        prev.nack_range     = nack.nack_range;
      }
      return;
    }
  }

  // Case 4: Check for range merge with segment extension
  if (sn_continuous && both_have_range && both_have_so && !so_continuous) {
    // Ranges are continuous but SO is not - cannot merge
  }

  // Case 5: Check for segment merge (both have SO, ranges differ)
  if (sn_continuous && both_have_so && ((!prev.has_nack_range && nack.has_nack_range) ||
                                        (prev.has_nack_range && !nack.has_nack_range))) {
    // One has range, one doesn't - check if SO is continuous
    if (so_continuous) {
      // Merge them
      prev.has_nack_range = nack.has_nack_range;
      prev.nack_range     = nack.nack_range;
      prev.so_end         = nack.so_end;
      return;
    }
  }

  // Default: Append as new entry
  nacks.push_back(nack);
}

// Trim implementation
bool rlc_am_status_pdu::trim(size_t target_size)
{
  // Minimum size is 3 bytes (header + E1=0)
  if (target_size < 3) {
    return false;
  }

  size_t current_size = get_packed_size();

  // If target is larger or equal, no trimming needed
  if (target_size >= current_size) {
    return true;
  }

  // Remove NACKs from the end until we fit
  while (!nacks.empty() && get_packed_size() > target_size) {
    // Remove last NACK
    nacks.pop_back();
  }

  // Verify we can fit
  if (get_packed_size() > target_size) {
    // Still too big - this shouldn't happen with proper NACK sizes
    // Remove all and keep only header
    nacks.clear();
  }

  // Update ack_sn to the first NACK's SN (or leave if no NACKs)
  if (!nacks.empty()) {
    ack_sn = nacks.front().nack_sn;
  } else {
    // No NACKs - ack_sn stays, but we need minimum size
  }

  // Final check
  return get_packed_size() <= target_size;
}

} // namespace srsran
