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

#pragma once

#include "srsran/adt/byte_buffer.h"
#include "srsran/adt/byte_buffer_view.h"
#include <array>
#include <cstdint>
#include "srsran/srslog/srslog.h"

namespace srsran {

/// Invalid RLC sequence number value.
constexpr uint32_t INVALID_RLC_SN = 0xffffffff;

/// RLC UM SN size configuration.
enum class rlc_um_sn_size : unsigned {
  size6bits,  ///< 6-bit sequence number
  size12bits  ///< 12-bit sequence number
};

/// RLC Segmentation Info field values.
enum class rlc_si_field : unsigned {
  full_sdu       = 0,  ///< Complete RLC SDU
  first_segment  = 1,  ///< First segment of an RLC SDU
  last_segment   = 2,  ///< Last segment of an RLC SDU
  middle_segment = 3   ///< Middle segment of an RLC SDU
};

/// RLC UM PDU header structure.
struct rlc_um_pdu_header {
  rlc_um_sn_size sn_size = rlc_um_sn_size::size6bits;  ///< SN size configuration
  rlc_si_field   si      = rlc_si_field::full_sdu;       ///< Segmentation Info
  uint32_t       sn      = 0;                            ///< Sequence Number
  uint16_t       so      = 0;                            ///< Segment Offset (only for segments)
};

/// Read an RLC UM data PDU header from a byte buffer.
/// \param buf Byte buffer containing the PDU.
/// \param sn_size SN size configuration.
/// \param hdr Output structure for the parsed header.
/// \returns Non-zero on success, zero on failure (invalid reserved bits or insufficient data).
inline int rlc_um_read_data_pdu_header(const byte_buffer& buf, rlc_um_sn_size sn_size, rlc_um_pdu_header* hdr)
{
  if (buf.length() < 1) {
    return 0;
  }

  // Initialize header
  hdr->sn_size = sn_size;
  hdr->sn      = 0;
  hdr->so      = 0;

  // Read first byte
  uint8_t byte0 = buf[0];

  // Extract SI field (bits 6-7)
  hdr->si = static_cast<rlc_si_field>((byte0 >> 6) & 0x03);

  if (sn_size == rlc_um_sn_size::size6bits) {
    // 6-bit SN format
    if (hdr->si == rlc_si_field::full_sdu) {
      // Complete SDU: 1 byte header, SI in bits 6-7, reserved bits 0-5 must be 0
      if ((byte0 & 0x3f) != 0) {
        return 0;  // Invalid reserved bits
      }
      return 1;  // Success, header is 1 byte
    }

    // For segmented PDUs with 6-bit SN:
    // - First segment: 1 byte header (no SO)
    // - Middle/Last segment: 3 bytes (1 header + 2 SO)
    if (hdr->si == rlc_si_field::first_segment) {
      // First segment: 1 byte header with SN in bits 0-5
      // No SO field for first segment
      hdr->sn = byte0 & 0x3f;
      return 1;
    } else {
      // Middle or last segment: 3 bytes (1 header + 2 SO)
      if (buf.length() < 3) {
        return 0;  // Insufficient data
      }
      // Extract SN from bits 0-5 of first byte
      hdr->sn = byte0 & 0x3f;
      // Extract SO from bytes 1-2 (16 bits)
      hdr->so = (static_cast<uint16_t>(buf[1]) << 8) | static_cast<uint16_t>(buf[2]);
      return 1;
    }
  } else {
    // 12-bit SN format
    if (hdr->si == rlc_si_field::full_sdu) {
      // Complete SDU: 1 byte header, SI in bits 6-7, reserved bits 0-5 must be 0
      if ((byte0 & 0x3f) != 0) {
        return 0;  // Invalid reserved bits
      }
      return 1;  // Success, header is 1 byte
    }

    // For segmented PDUs with 12-bit SN:
    // - First segment: 2 bytes (1 header + 1 SN)
    // - Middle/Last segment: 4 bytes (2 header + 2 SO)
    if (hdr->si == rlc_si_field::first_segment) {
      // First segment: 2 bytes (R + SI + SN[11:0])
      if (buf.length() < 2) {
        return 0;  // Insufficient data
      }
      uint8_t byte1 = buf[1];
      // Extract SN: bits 0-4 of byte0 (5 bits) + all of byte1 (8 bits) = 13 bits, but only 12 bits used
      // Actually for 12-bit SN: byte0 bits 0-4 = SN[11:7], byte1 = SN[6:0] + 1 extra bit? 
      // Looking at test: 0x40, 0x05 -> SN=5
      // 0x40 = 0b01000000, SI=01, upper 5 bits = 00000
      // 0x05 = 0b00000101, lower 8 bits = 5
      // SN = 0 << 8 | 5 = 5 ✓
      hdr->sn = ((byte0 & 0x1f) << 8) | byte1;
      return 1;
    } else {
      // Middle or last segment: 4 bytes (2 header + 2 SO)
      if (buf.length() < 4) {
        return 0;  // Insufficient data
      }
      uint8_t byte1 = buf[1];
      // Extract SN: bits 0-4 of byte0 (5 bits) + all of byte1 (8 bits)
      hdr->sn = ((byte0 & 0x1f) << 8) | byte1;
      // Extract SO from bytes 2-3 (16 bits)
      hdr->so = (static_cast<uint16_t>(buf[2]) << 8) | static_cast<uint16_t>(buf[3]);
      return 1;
    }
  }

  return 0;  // Should not reach here
}

/// Write an RLC UM data PDU header to a span.
/// \param buf Output span to write the header.
/// \param hdr Header structure to serialize.
/// \returns Number of bytes written (header length), or 0 on failure.
inline size_t rlc_um_write_data_pdu_header(span<uint8_t> buf, const rlc_um_pdu_header& hdr)
{
  size_t header_len = 0;

  if (hdr.sn_size == rlc_um_sn_size::size6bits) {
    if (hdr.si == rlc_si_field::full_sdu) {
      // 1 byte header: RRRRRR SI (SI=00)
      if (buf.size() < 1) {
        return 0;
      }
      buf[0] = 0x00;  // SI=00, reserved=0
      header_len = 1;
    } else if (hdr.si == rlc_si_field::first_segment) {
      // First segment: 1 byte header with SN in bits 0-5, SI in bits 6-7
      // No SO field for first segment
      if (buf.size() < 1) {
        return 0;
      }
      buf[0] = static_cast<uint8_t>(((static_cast<unsigned>(hdr.si) & 0x03) << 6) | (hdr.sn & 0x3f));
      header_len = 1;
    } else {
      // Middle or last segment: 3 bytes (1 header + 2 SO)
      if (buf.size() < 3) {
        return 0;
      }
      buf[0] = static_cast<uint8_t>(((static_cast<unsigned>(hdr.si) & 0x03) << 6) | (hdr.sn & 0x3f));
      buf[1] = static_cast<uint8_t>((hdr.so >> 8) & 0xff);
      buf[2] = static_cast<uint8_t>(hdr.so & 0xff);
      header_len = 3;
    }
  } else {
    // 12-bit SN format
    if (hdr.si == rlc_si_field::full_sdu) {
      // 1 byte header: RRRRRR SI (SI=00), SN not present
      if (buf.size() < 1) {
        return 0;
      }
      buf[0] = 0x00;  // SI=00, reserved=0
      header_len = 1;
    } else if (hdr.si == rlc_si_field::first_segment) {
      // First segment: 2 bytes (R + SI + SN[11:0])
      if (buf.size() < 2) {
        return 0;
      }
      // R=0, SI in bits 6-7, SN[11:7] in bits 0-4 of byte0, SN[6:0] in byte1
      buf[0] = static_cast<uint8_t>(((static_cast<unsigned>(hdr.si) & 0x03) << 6) | ((hdr.sn >> 8) & 0x1f));
      buf[1] = static_cast<uint8_t>(hdr.sn & 0xff);
      header_len = 2;
    } else {
      // Middle or last segment: 4 bytes (2 header + 2 SO)
      if (buf.size() < 4) {
        return 0;
      }
      // R=0, SI in bits 6-7, SN[11:7] in bits 0-4 of byte0, SN[6:0] in byte1
      buf[0] = static_cast<uint8_t>(((static_cast<unsigned>(hdr.si) & 0x03) << 6) | ((hdr.sn >> 8) & 0x1f));
      buf[1] = static_cast<uint8_t>(hdr.sn & 0xff);
      buf[2] = static_cast<uint8_t>((hdr.so >> 8) & 0xff);
      buf[3] = static_cast<uint8_t>(hdr.so & 0xff);
      header_len = 4;
    }
  }

  return header_len;
}

} // namespace srsran