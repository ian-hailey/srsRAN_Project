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
#include "srsran/support/integer_types.h"
#include <array>
#include <cstdint>

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
  rlc_si_field   si      = rlc_si_field::full_sdu;      ///< Segmentation Info
  uint32_t       sn      = 0;                           ///< Sequence Number
  uint16_t       so      = 0;                           ///< Segment Offset (only for segments)
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

  // Check reserved bits - for 6-bit SN, bits 0-5 are SN, for 12-bit SN with full SDU, bits 0-5 should be 0
  // For complete SDU (SI=00), there's no SN field, so bits 0-5 should be reserved (must be 0)
  if (hdr->si == rlc_si_field::full_sdu) {
    // For full SDU, bits 0-5 are reserved and must be 0
    if ((byte0 & 0x3f) != 0) {
      return 0;  // Invalid reserved bits
    }
    return 1;  // Success, header is 1 byte
  }

  // For segmented PDUs, we need to check if SO is present
  bool has_so = (hdr->si == rlc_si_field::first_segment || hdr->si == rlc_si_field::middle_segment ||
                 hdr->si == rlc_si_field::last_segment);

  if (sn_size == rlc_um_sn_size::size6bits) {
    // 6-bit SN format
    if (has_so) {
      // Need 3 bytes: 1 header + 2 SO
      if (buf.length() < 3) {
        return 0;
      }
      // Extract SN from bits 0-5 of first byte
      hdr->sn = byte0 & 0x3f;
      // Extract SO from bytes 1-2 (16 bits)
      hdr->so = (static_cast<uint16_t>(buf[1]) << 8) | static_cast<uint16_t>(buf[2]);
    } else {
      // Need 1 byte for header with SN
      if (buf.length() < 1) {
        return 0;
      }
      // Extract SN from bits 0-5 of first byte
      hdr->sn = byte0 & 0x3f;
    }
  } else {
    // 12-bit SN format
    if (buf.length() < 2) {
      return 0;
    }
    uint8_t byte1 = buf[1];
    // Extract SN: bits 0-4 of byte0 and all 8 bits of byte1
    hdr->sn = ((byte0 & 0x1f) << 8) | byte1;

    if (has_so) {
      // Need 4 bytes: 2 header + 2 SO
      if (buf.length() < 4) {
        return 0;
      }
      // Extract SO from bytes 2-3 (16 bits)
      hdr->so = (static_cast<uint16_t>(buf[2]) << 8) | static_cast<uint16_t>(buf[3]);
    }
  }

  return 1;  // Success
}

/// Write an RLC UM data PDU header to a span.
/// \param buf Output span to write the header.
/// \param hdr Header structure to serialize.
/// \returns Number of bytes written (header length).
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
    } else {
      bool has_so = (hdr.si != rlc_si_field::full_sdu);
      if (has_so) {
        // 3 bytes: SN(6 bits) SI(2 bits) | SO(16 bits)
        if (buf.size() < 3) {
          return 0;
        }
        buf[0] = static_cast<uint8_t>(((hdr.si & 0x03) << 6) | (hdr.sn & 0x3f));
        buf[1] = static_cast<uint8_t>((hdr.so >> 8) & 0xff);
        buf[2] = static_cast<uint8_t>(hdr.so & 0xff);
        header_len = 3;
      } else {
        // This case shouldn't happen for UM - segments should have SO
        // But handle it anyway
        if (buf.size() < 1) {
          return 0;
        }
        buf[0] = static_cast<uint8_t>(((hdr.si & 0x03) << 6) | (hdr.sn & 0x3f));
        header_len = 1;
      }
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
    } else {
      // 2 byte header + optional SO
      bool has_so = (hdr.si != rlc_si_field::full_sdu);
      if (has_so) {
        // 4 bytes: R(1 bit) SI(2 bits) SN(12 bits) | SO(16 bits)
        if (buf.size() < 4) {
          return 0;
        }
        buf[0] = static_cast<uint8_t>((hdr.sn >> 8) & 0x1f);  // Upper 5 bits of SN (bit 7=0 reserved)
        buf[1] = static_cast<uint8_t>(hdr.sn & 0xff);          // Lower 8 bits of SN
        buf[2] = static_cast<uint8_t>((hdr.so >> 8) & 0xff);   // Upper 8 bits of SO
        buf[3] = static_cast<uint8_t>(hdr.so & 0xff);          // Lower 8 bits of SO
        header_len = 4;
      } else {
        // 2 bytes: R(1 bit) SI(2 bits) SN(12 bits)
        if (buf.size() < 2) {
          return 0;
        }
        buf[0] = static_cast<uint8_t>((hdr.sn >> 8) & 0x1f);  // Upper 5 bits of SN
        buf[1] = static_cast<uint8_t>(hdr.sn & 0xff);          // Lower 8 bits of SN
        header_len = 2;
      }
    }
  }

  return header_len;
}

} // namespace srsran