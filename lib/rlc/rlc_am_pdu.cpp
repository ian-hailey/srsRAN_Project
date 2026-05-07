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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "rlc_am_pdu.h"
#include <cstring>

namespace srsran {

// RLC AM Status PDU implementation
rlc_am_status_pdu::rlc_am_status_pdu(rlc_am_sn_size sn_size_) : sn_size(sn_size_) {}

rlc_am_status_pdu::rlc_am_status_pdu(const rlc_am_status_pdu& other) :
    ack_sn(other.ack_sn), sn_size(other.sn_size), nacks(other.nacks)
{
}

rlc_am_status_pdu& rlc_am_status_pdu::operator=(const rlc_am_status_pdu& other)
{
  if (this != &other) {
    sn_size = other.sn_size;
    ack_sn  = other.ack_sn;
    nacks   = other.nacks;
  }
  return *this;
}

void rlc_am_status_pdu::push_nack(const rlc_am_status_nack& nack)
{
  nacks.push_back(nack);
}

size_t rlc_am_status_pdu::get_packed_size() const
{
  size_t size = (sn_size == rlc_am_sn_size::size12bits) ? 3 : 4;  // Header with ACK_SN

  for (const auto& nack : nacks) {
    size += (sn_size == rlc_am_sn_size::size12bits) ? 3 : 3;  // NACK_SN (3 bytes for 12-bit, 3 bytes for 18-bit)

    if (nack.has_so) {
      size += 4;  // SOstart + SOend
    }

    if (nack.has_nack_range) {
      size += 1;  // NACK range
    }
  }

  return size;
}

bool rlc_am_status_pdu::trim(size_t max_size)
{
  size_t current_size = get_packed_size();

  if (current_size <= max_size) {
    return true;
  }

  // Remove NACKs from the end until we fit
  size_t nack_size = (sn_size == rlc_am_sn_size::size12bits) ? 3 : 3;

  while (!nacks.empty() && current_size > max_size) {
    const auto& nack = nacks.back();
    current_size -= nack_size;

    if (nack.has_so) {
      current_size -= 4;
    }

    if (nack.has_nack_range) {
      current_size -= 1;
    }

    nacks.pop_back();
  }

  return current_size <= max_size;
}

void rlc_am_status_pdu::reset()
{
  ack_sn = 0;
  nacks.clear();
}

bool rlc_am_read_data_pdu_header(const byte_buffer& buf, rlc_am_sn_size sn_size, rlc_am_pdu_header* hdr)
{
  if (buf.length() < 1) {
    return false;
  }

  hdr->sn_size = sn_size;
  hdr->sn      = 0;
  hdr->so      = 0;

  uint8_t byte0 = buf[0];

  // Extract DC field (bit 7)
  hdr->dc = (byte0 >> 7) & 0x01;

  // Extract P field (bit 6)
  hdr->p = (byte0 >> 6) & 0x01;

  // Extract SI field (bits 4-5)
  hdr->si = static_cast<rlc_si_field>((byte0 >> 4) & 0x03);

  if (sn_size == rlc_am_sn_size::size12bits) {
    // 12-bit SN format
    if (hdr->si == rlc_si_field::full_sdu) {
      // 2 bytes: DC(1) P(1) SI(2) SN(12)
      if (buf.length() < 2) {
        return false;
      }
      // Check reserved bits (bits 0-3 of byte0 should be 0)
      if ((byte0 & 0x0f) != 0) {
        return false;
      }
      hdr->sn = byte0 & 0x0f;  // Upper 4 bits of SN
      hdr->sn |= (static_cast<uint32_t>(buf[1]) << 4);  // Lower 8 bits
    } else if (hdr->si == rlc_si_field::first_segment) {
      // First segment: 2 bytes (no SO)
      if (buf.length() < 2) {
        return false;
      }
      hdr->sn = (byte0 & 0x0f) << 8;  // Upper 4 bits of SN
      hdr->sn |= static_cast<uint32_t>(buf[1]);  // Lower 8 bits
      hdr->so = 0;  // No SO field for first segment
    } else {
      // Middle or last segment: 4 bytes with SO
      if (buf.length() < 4) {
        return false;
      }
      hdr->sn = (byte0 & 0x0f) << 8;  // Upper 4 bits of SN
      hdr->sn |= static_cast<uint32_t>(buf[1]);  // Lower 8 bits
      hdr->so = (static_cast<uint16_t>(buf[2]) << 8) | static_cast<uint16_t>(buf[3]);
    }
  } else {
    // 18-bit SN format
    if (hdr->si == rlc_si_field::full_sdu || hdr->si == rlc_si_field::first_segment) {
      // 3 bytes: DC(1) P(1) SI(2) SN(18) - no SO for full_sdu or first_segment
      if (buf.length() < 3) {
        return false;
      }
      // SN[17:14] in upper 4 bits of byte0 (bits 3-0 after D/C, P, SI)
      uint32_t sn_upper = (byte0 & 0x0f);  // Upper 4 bits of SN
      // Check SN range: 18-bit SN max is 262143, so SN[17:14] must be 0-3
      if (sn_upper > 3) {
        return false;
      }
      hdr->sn = (sn_upper << 16);
      hdr->sn |= (static_cast<uint32_t>(buf[1]) << 8);  // Middle 8 bits
      hdr->sn |= static_cast<uint32_t>(buf[2]);  // Lower 8 bits
      hdr->so = 0;  // No SO field
    } else {
      // With SO: 5 bytes (middle_segment or last_segment)
      if (buf.length() < 5) {
        return false;
      }
      uint32_t sn_upper = (byte0 & 0x0f);  // Upper 4 bits of SN
      // Check SN range: 18-bit SN max is 262143, so SN[17:14] must be 0-3
      if (sn_upper > 3) {
        return false;
      }
      hdr->sn = (sn_upper << 16);
      hdr->sn |= (static_cast<uint32_t>(buf[1]) << 8);
      hdr->sn |= static_cast<uint32_t>(buf[2]);  // All 8 bits of byte2
      hdr->so = (static_cast<uint16_t>(buf[3]) << 8) | static_cast<uint16_t>(buf[4]);
    }
  }

  return true;
}

size_t rlc_am_write_data_pdu_header(span<uint8_t> buf, const rlc_am_pdu_header& hdr)
{
  if (hdr.sn_size == rlc_am_sn_size::size12bits) {
    if (hdr.si == rlc_si_field::full_sdu) {
      // 2 bytes: DC(1) P(1) SI(2) SN(12)
      if (buf.size() < 2) {
        return 0;
      }
      buf[0] = static_cast<uint8_t>((hdr.dc << 7) | (hdr.p << 6) | ((static_cast<uint8_t>(hdr.si) & 0x03) << 4) | ((hdr.sn >> 8) & 0x0f));
      buf[1] = static_cast<uint8_t>(hdr.sn & 0xff);
      return 2;
    } else if (hdr.si == rlc_si_field::first_segment) {
      // First segment: 2 bytes (no SO)
      if (buf.size() < 2) {
        return 0;
      }
      buf[0] = static_cast<uint8_t>((hdr.dc << 7) | (hdr.p << 6) | ((static_cast<uint8_t>(hdr.si) & 0x03) << 4) | ((hdr.sn >> 8) & 0x0f));
      buf[1] = static_cast<uint8_t>(hdr.sn & 0xff);
      return 2;
    } else {
      // Middle or last segment: 4 bytes with SO
      if (buf.size() < 4) {
        return 0;
      }
      buf[0] = static_cast<uint8_t>((hdr.dc << 7) | (hdr.p << 6) | ((static_cast<uint8_t>(hdr.si) & 0x03) << 4) | ((hdr.sn >> 8) & 0x0f));
      buf[1] = static_cast<uint8_t>(hdr.sn & 0xff);
      buf[2] = static_cast<uint8_t>((hdr.so >> 8) & 0xff);
      buf[3] = static_cast<uint8_t>(hdr.so & 0xff);
      return 4;
    }
  } else {
    // 18-bit SN format
    if (hdr.si == rlc_si_field::full_sdu || hdr.si == rlc_si_field::first_segment) {
      // 3 bytes: DC(1) P(1) SI(2) SN(18) - no SO for full_sdu or first_segment
      if (buf.size() < 3) {
        return 0;
      }
      buf[0] = static_cast<uint8_t>((hdr.dc << 7) | (hdr.p << 6) | ((static_cast<uint8_t>(hdr.si) & 0x03) << 4) | ((hdr.sn >> 16) & 0x0f));
      buf[1] = static_cast<uint8_t>((hdr.sn >> 8) & 0xff);
      buf[2] = static_cast<uint8_t>(hdr.sn & 0xff);
      return 3;
    } else {
      // With SO: 5 bytes (middle_segment or last_segment)
      if (buf.size() < 5) {
        return 0;
      }
      buf[0] = static_cast<uint8_t>((hdr.dc << 7) | (hdr.p << 6) | ((static_cast<uint8_t>(hdr.si) & 0x03) << 4) | ((hdr.sn >> 16) & 0x0f));
      buf[1] = static_cast<uint8_t>((hdr.sn >> 8) & 0xff);
      buf[2] = static_cast<uint8_t>(hdr.sn & 0xff);
      buf[3] = static_cast<uint8_t>((hdr.so >> 8) & 0xff);
      buf[4] = static_cast<uint8_t>(hdr.so & 0xff);
      return 5;
    }
  }
}

bool rlc_am_status_pdu::unpack(const byte_buffer& buf)
{
  if (buf.length() < 3) {
    return false;
  }

  nacks.clear();

  uint8_t byte0 = buf[0];
  uint8_t byte1 = buf[1];

  // Check D/C bit (must be 0 for control PDU)
  if ((byte0 >> 7) != 0) {
    return false;
  }

  // Check CPT field (bits 4-6, should be 000 for STATUS PDU)
  if (((byte0 >> 4) & 0x07) != 0) {
    return false;
  }

  if (sn_size == rlc_am_sn_size::size12bits) {
    // 12-bit SN format
    // ACK_SN: bits 0-3 of byte0 and all of byte1
    ack_sn = ((byte0 & 0x0f) << 8) | byte1;

    if (buf.length() < 3) {
      return false;
    }
    uint8_t byte2 = buf[2];

    // E1 bit (bit 7 of byte2)
    bool e1 = (byte2 >> 7) & 0x01;

    if (!e1) {
      // No NACKs, just ACK_SN
      return true;
    }

    // Parse NACKs
    // Format: NACK_SN[11:4](8) | NACK_SN[3:0](4) + E1(1) + E2(1) + E3(1) + R(1)
    // Each NACK entry is 2 bytes, with E1 indicating if another NACK follows
    size_t offset = 3;
    while (offset + 2 <= buf.length()) {  // Need at least 2 bytes for NACK entry
      rlc_am_status_nack nack;

      // First byte: NACK_SN[11:4] (8 bits)
      uint8_t first_byte = buf[offset];
      nack.nack_sn = (first_byte << 4);  // Upper 8 bits

      // Second byte: NACK_SN[3:0](4) + E1(1) + E2(1) + E3(1) + R(1)
      uint8_t second_byte = buf[offset + 1];
      nack.nack_sn |= ((second_byte >> 4) & 0x0f);  // Lower 4 bits

      uint8_t flags = second_byte & 0x0f;
      e1 = (flags & 0x08) != 0;  // E1 bit
      nack.has_so = (flags & 0x04) != 0;  // E2 bit
      nack.has_nack_range = (flags & 0x02) != 0;  // E3 bit

      offset += 2;

      if (nack.has_so) {
        if (offset + 2 > buf.length()) {
          return false;
        }
        nack.so_start = (static_cast<uint16_t>(buf[offset]) << 8) | buf[offset + 1];
        offset += 2;

        if (offset + 2 > buf.length()) {
          return false;
        }
        nack.so_end = (static_cast<uint16_t>(buf[offset]) << 8) | buf[offset + 1];
        offset += 2;
      }

      if (nack.has_nack_range) {
        if (offset + 1 > buf.length()) {
          return false;
        }
        nack.nack_range = buf[offset];
        offset += 1;
      }

      nacks.push_back(nack);

      if (!e1) {
        break;
      }
      
      // Check if there are enough bytes for the next NACK entry
      if (offset + 2 > buf.length()) {
        return false;
      }
    }
  } else {
    // 18-bit SN format
    // ACK_SN: bits 4-7 of byte0 + all of byte1 + upper 6 bits of byte2
    if (buf.length() < 3) {
      return false;
    }
    ack_sn = ((static_cast<uint32_t>(buf[0]) & 0x0f) << 12) | (static_cast<uint32_t>(buf[1]) << 4) | (static_cast<uint32_t>(buf[2]) >> 2);

    // E1 bit (bit 5 of byte3)
    uint8_t byte3 = buf[3];
    bool e1 = (byte3 & 0x20) != 0;

    if (!e1) {
      return true;
    }

    // Parse NACKs
    size_t offset = 4;
    while (offset < buf.length()) {
      if (offset + 3 > buf.length()) {
        return false;
      }

      rlc_am_status_nack nack;
      nack.nack_sn = (static_cast<uint32_t>(buf[offset]) << 10) | (static_cast<uint32_t>(buf[offset + 1]) << 2) | ((buf[offset + 2] >> 6) & 0x03);

      uint8_t nibble = (buf[offset + 2] & 0x3f);
      nack.has_so = (nibble & 0x20) != 0;  // E2 bit
      nack.has_nack_range = (nibble & 0x10) != 0;  // E3 bit

      offset += 3;

      if (nack.has_so) {
        if (offset + 2 > buf.length()) {
          return false;
        }
        nack.so_start = (static_cast<uint16_t>(buf[offset]) << 8) | buf[offset + 1];
        offset += 2;

        if (offset + 2 > buf.length()) {
          return false;
        }
        nack.so_end = (static_cast<uint16_t>(buf[offset]) << 8) | buf[offset + 1];
        offset += 2;
      }

      if (nack.has_nack_range) {
        if (offset + 1 > buf.length()) {
          return false;
        }
        nack.nack_range = buf[offset];
        offset += 1;
      }

      nacks.push_back(nack);

      // Check E1 bit for next NACK
      if (offset >= buf.length()) {
        break;
      }

      uint8_t next_byte = buf[offset];
      e1 = (next_byte & 0x80) != 0;

      if (!e1) {
        break;
      }
    }
  }

  return true;
}

size_t rlc_am_status_pdu::pack(span<uint8_t> buf) const
{
  size_t offset = 0;

  if (sn_size == rlc_am_sn_size::size12bits) {
    // Header: D/C(1) CPT(3) ACK_SN(12)
    if (buf.size() < 3) {
      return 0;
    }
    buf[0] = 0x08 | ((ack_sn >> 8) & 0x0f);  // D/C=0, CPT=000, upper 4 bits of ACK_SN
    buf[1] = static_cast<uint8_t>(ack_sn & 0xff);

    if (nacks.empty()) {
      // Just ACK_SN with E1=0
      buf[2] = 0x00;
      return 3;
    }

    // Write E1=1 at byte 2 to indicate NACKs follow
    buf[2] = 0x80;
    offset = 3;  // Start after the E1 byte

    for (size_t i = 0; i < nacks.size(); ++i) {
      const rlc_am_status_nack& nack = nacks[i];

      // Format: NACK_SN[11:4](8) | NACK_SN[3:0](4) + E1(1) + E2(1) + E3(1) + R(1)
      uint8_t first_byte = static_cast<uint8_t>((nack.nack_sn >> 4) & 0xff);  // NACK_SN[11:4]
      
      uint8_t second_flags = 0x00;
      if (i < nacks.size() - 1) {
        second_flags |= 0x08;  // E1=1 for more NACKs
      }
      if (nack.has_so) {
        second_flags |= 0x04;  // E2=1
      }
      if (nack.has_nack_range) {
        second_flags |= 0x02;  // E3=1
      }
      
      uint8_t second_byte = static_cast<uint8_t>((nack.nack_sn & 0x0f) << 4) | second_flags;

      if (offset + 2 > buf.size()) {
        return 0;
      }
      buf[offset] = first_byte;
      buf[offset + 1] = second_byte;
      offset += 2;

      if (nack.has_so) {
        if (offset + 4 > buf.size()) {
          return 0;
        }
        buf[offset] = static_cast<uint8_t>((nack.so_start >> 8) & 0xff);
        buf[offset + 1] = static_cast<uint8_t>(nack.so_start & 0xff);
        offset += 2;

        buf[offset] = static_cast<uint8_t>((nack.so_end >> 8) & 0xff);
        buf[offset + 1] = static_cast<uint8_t>(nack.so_end & 0xff);
        offset += 2;
      }

      if (nack.has_nack_range) {
        if (offset + 1 > buf.size()) {
          return 0;
        }
        buf[offset] = nack.nack_range;
        offset += 1;
      }
    }
  } else {
    // 18-bit SN format
    // Header: D/C(1) CPT(3) ACK_SN(18)
    if (nacks.empty()) {
      // STATUS PDU with no NACKs: 3 bytes
      // D/C(1) + CPT(3) + ACK_SN[17:14](4) in byte 0
      // ACK_SN[13:6](8) in byte 1
      // ACK_SN[5:0](6) + E1(1) + R(1) in byte 2
      if (buf.size() < 3) {
        return 0;
      }
      buf[0] = 0x0e;  // D/C=0, CPT=000
      buf[1] = static_cast<uint8_t>((ack_sn >> 12) & 0xff);
      buf[2] = static_cast<uint8_t>(((ack_sn & 0xfff) >> 6) & 0xff);
      // E1=0, R=0
      return 3;
    }

    if (buf.size() < 4) {
      return 0;
    }
    buf[0] = 0x0e;  // D/C=0, CPT=000
    buf[1] = static_cast<uint8_t>((ack_sn >> 12) & 0xff);
    buf[2] = static_cast<uint8_t>(((ack_sn & 0xfff) >> 6) & 0xff);
    buf[3] = 0x20;  // E1=1, R=0

    offset = 4;

    for (size_t i = 0; i < nacks.size(); ++i) {
      const rlc_am_status_nack& nack = nacks[i];

      if (offset + 3 > buf.size()) {
        return 0;
      }

      uint8_t nibble = 0x20;  // E1=1
      if (nack.has_so) {
        nibble |= 0x20;  // E2=1
      }
      if (nack.has_nack_range) {
        nibble |= 0x10;  // E3=1
      }

      buf[offset] = static_cast<uint8_t>((nack.nack_sn >> 10) & 0xff);
      buf[offset + 1] = static_cast<uint8_t>((nack.nack_sn >> 2) & 0xff);
      buf[offset + 2] = static_cast<uint8_t>(((nack.nack_sn & 0x03) << 6) | nibble);
      offset += 3;

      if (nack.has_so) {
        if (offset + 2 > buf.size()) {
          return 0;
        }
        buf[offset] = static_cast<uint8_t>((nack.so_start >> 8) & 0xff);
        buf[offset + 1] = static_cast<uint8_t>(nack.so_start & 0xff);
        offset += 2;

        if (offset + 2 > buf.size()) {
          return 0;
        }
        buf[offset] = static_cast<uint8_t>((nack.so_end >> 8) & 0xff);
        buf[offset + 1] = static_cast<uint8_t>(nack.so_end & 0xff);
        offset += 2;
      }

      if (nack.has_nack_range) {
        if (offset + 1 > buf.size()) {
          return 0;
        }
        buf[offset] = nack.nack_range;
        offset += 1;
      }
    }

    // Final byte with E1=0
    if (offset < buf.size()) {
      buf[offset] = 0x00;
      offset += 1;
    }
  }

  return offset;
}

bool rlc_am_status_pdu::is_control_pdu(const byte_buffer& buf)
{
  if (buf.length() < 1) {
    return false;
  }

  uint8_t byte0 = buf[0];

  // D/C bit must be 0 for control PDU
  if ((byte0 >> 7) != 0) {
    return false;
  }

  // CPT field (bits 4-6)
  uint8_t cpt = (byte0 >> 4) & 0x07;

  // CPT=000 is STATUS PDU
  return cpt == 0;
}

} // namespace srsran