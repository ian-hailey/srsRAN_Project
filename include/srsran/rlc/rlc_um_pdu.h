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
#include "srsran/adt/span.h"
#include "srsran/rlc/rlc_config.h"
#include <cstdint>

namespace srsran {

/// RLC UM PDU header
struct rlc_um_pdu_header {
  rlc_si_field   si;     ///< Segmentation info
  uint32_t       sn;     ///< Sequence number
  rlc_um_sn_size sn_size; ///< SN size (6 or 12 bits)
  uint16_t       so;     ///< Segment offset (only for non-first segments)
};

/// \brief Reads an RLC UM PDU header from a byte buffer.
///
/// Unpacks the header according to the SN size and SI field.
/// For full SDU (SI=00): 1 byte header (no SN for 6-bit, no SN for 12-bit)
/// For first segment (SI=01): 1 byte (6-bit SN) or 2 bytes (12-bit SN)
/// For middle/last segment (SI=10/11): 3 bytes (6-bit SN + SO) or 4 bytes (12-bit SN + SO)
///
/// \param pdu The PDU buffer to read from (header will be removed)
/// \param sn_size The sequence number size (6 or 12 bits)
/// \param hdr Pointer to the header structure to populate
/// \return true if header was successfully unpacked, false otherwise
bool rlc_um_read_data_pdu_header(byte_buffer& pdu, rlc_um_sn_size sn_size, rlc_um_pdu_header* hdr);

/// \brief Writes an RLC UM PDU header to a buffer.
///
/// Packs the header according to the SN size and SI field.
/// For full SDU (SI=00): 1 byte header
/// For first segment (SI=01): 1 byte (6-bit SN) or 2 bytes (12-bit SN)
/// For middle/last segment (SI=10/11): 3 bytes (6-bit SN + SO) or 4 bytes (12-bit SN + SO)
///
/// \param buf The buffer to write to
/// \param hdr The header to pack
/// \return The number of bytes written (header size)
size_t rlc_um_write_data_pdu_header(span<uint8_t> buf, const rlc_um_pdu_header& hdr);

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
    return format_to(ctx.out(), "si={} sn={} so={}", hdr.si, hdr.sn, hdr.so);
  }
};

} // namespace fmt