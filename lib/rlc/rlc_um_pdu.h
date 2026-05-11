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

#pragma once

#include "srsran/adt/byte_buffer.h"
#include "srsran/rlc/rlc_config.h"
#include "srsran/srslog/srslog.h"
#include "fmt/format.h"
#include <cstdint>

namespace srsran {

/// \brief Header of an UMD PDU.
///
/// The header contains SI field, SN, and optionally SO for segmented PDUs.
/// Ref: 3GPP TS 38.322 Sec. 6.2.2.3.
struct rlc_um_pdu_header {
  rlc_si_field   si      = rlc_si_field::full_sdu; ///< Segmentation Info (2 bits)
  rlc_um_sn_size sn_size = rlc_um_sn_size::size6bits; ///< Sequence Number size (6 or 12 bits)
  uint32_t       sn      = 0;                          ///< Sequence Number
  uint32_t       so      = 0;                          ///< Segment Offset (16 bits)
};

/// \brief Read the header of a UMD PDU from a byte buffer.
///
/// \param buf The byte buffer containing the PDU.
/// \param sn_size SN size (6bit or 12bit).
/// \param hdr Output header structure.
/// \return true if the header was successfully read, false otherwise.
inline bool rlc_um_read_data_pdu_header(const byte_buffer& buf, rlc_um_sn_size sn_size, rlc_um_pdu_header* hdr)
{
  if (buf.empty()) {
    return false;
  }
  hdr->sn_size = sn_size;

  // Read first byte
  uint8_t byte0 = *buf.begin();

  // Extract SI (bits 7-6)
  uint8_t si_val  = (byte0 >> 6) & 0x03;
  hdr->si         = static_cast<rlc_si_field>(si_val);

  if (sn_size == rlc_um_sn_size::size6bits) {
    if (si_val == 0b00) {
      // Full SDU: header is 1 byte [SI(2) | R(6)]
      // Reserved bits (bits 5-0) must be 0
      if ((byte0 & 0x3F) != 0) {
        return false;
      }
      hdr->sn = 0;
      hdr->so = 0;
      return true;
    }
    // First segment (SI=01): header is 1 byte [SI(2) | SN(6)]
    if (si_val == 0b01) {
      hdr->sn = byte0 & 0x3F;
      hdr->so = 0;
      return true;
    }
    // Middle (SI=11) or Last segment (SI=10): header is 1 byte [SI(2) | SN(6)] + 2 bytes SO
    if (buf.length() < 3) {
      return false;
    }
    hdr->sn = byte0 & 0x3F;
    // Read SO (2 bytes)
    auto it   = buf.begin();
    ++it; // skip byte0
    hdr->so = (static_cast<uint32_t>(*it) << 8);
    ++it;
    hdr->so |= static_cast<uint32_t>(*it);
    return true;
  }

  // sn_size == 12 bits
  if (buf.length() < 1) {
    return false;
  }

  if (si_val == 0b00) {
    // Full SDU: header is 1 byte [SI(2) | R(6)]
    if ((byte0 & 0x3F) != 0) {
      return false;
    }
    hdr->sn = 0;
    hdr->so = 0;
    return true;
  }

  if (buf.length() < 2) {
    return false;
  }

  // For 12-bit SN, byte0: [SI(2) | R(2) | SN_high(4)]
  // byte1: [SN_low(8)]
  uint8_t sn_high = byte0 & 0x0F;
  auto    it      = buf.begin();
  ++it;
  uint8_t sn_low     = *it;
  hdr->sn            = (static_cast<uint32_t>(sn_high) << 8) | sn_low;

  if (si_val == 0b01) {
    // First segment: no SO
    hdr->so = 0;
    return true;
  }

  // Middle (SI=11) or Last segment (SI=10): need SO (2 bytes)
  if (buf.length() < 4) {
    return false;
  }
  ++it;
  hdr->so = (static_cast<uint32_t>(*it) << 8);
  ++it;
  hdr->so |= static_cast<uint32_t>(*it);
  return true;
}

/// \brief Write the header of a UMD PDU to a buffer.
///
/// \param buf The buffer to write the header to.
/// \param hdr The header structure.
/// \return The number of bytes written.
inline size_t rlc_um_write_data_pdu_header(span<uint8_t> buf, const rlc_um_pdu_header& hdr)
{
  uint8_t si_val = static_cast<uint8_t>(hdr.si);

  if (hdr.sn_size == rlc_um_sn_size::size6bits) {
    if (hdr.si == rlc_si_field::full_sdu) {
      // Full SDU: 1 byte [SI(2) | R(6)]
      buf[0] = (si_val << 6) | 0x00;
      return 1;
    }
    if (hdr.si == rlc_si_field::first_segment) {
      // First segment: 1 byte [SI(2) | SN(6)]
      buf[0] = (si_val << 6) | (hdr.sn & 0x3F);
      return 1;
    }
    // Middle or Last segment: 1 byte [SI(2) | SN(6)] + 2 bytes SO
    buf[0] = (si_val << 6) | (hdr.sn & 0x3F);
    buf[1] = (hdr.so >> 8) & 0xFF;
    buf[2] = hdr.so & 0xFF;
    return 3;
  }

  // sn_size == 12 bits
  if (hdr.si == rlc_si_field::full_sdu) {
    // Full SDU: 1 byte [SI(2) | R(6)]
    buf[0] = (si_val << 6) | 0x00;
    return 1;
  }
  if (hdr.si == rlc_si_field::first_segment) {
    // First segment: 2 bytes [SI(2) | R(2) | SN_high(4)] + [SN_low(8)]
    buf[0] = (si_val << 6) | ((hdr.sn >> 8) & 0x0F);
    buf[1] = hdr.sn & 0xFF;
    return 2;
  }
  // Middle or Last segment: header + 2 bytes SO
  buf[0] = (si_val << 6) | ((hdr.sn >> 8) & 0x0F);
  buf[1] = hdr.sn & 0xFF;
  buf[2] = (hdr.so >> 8) & 0xFF;
  buf[3] = hdr.so & 0xFF;
  return 4;
}

} // namespace srsran

namespace fmt {

template <>
struct formatter<srsran::rlc_um_pdu_header> {
  template <typename ParseContext>
  auto parse(ParseContext& ctx)
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const srsran::rlc_um_pdu_header& hdr, FormatContext& ctx) const
  {
    return format_to(ctx.out(),
                     "rlc_um_pdu_header si={} sn_size={} sn={} so={}",
                     hdr.si,
                     to_number(hdr.sn_size),
                     hdr.sn,
                     hdr.so);
  }
};

} // namespace fmt