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

#pragma once

#include "srsran/adt/byte_buffer.h"
#include "srsran/adt/span.h"
#include "srsran/rlc/rlc_config.h"
#include <cstdint>
#include <vector>

namespace srsran {

/// RLC AM Data PDU header
struct rlc_am_pdu_header {
  rlc_si_field   si;       ///< Segmentation info
  uint32_t       sn;       ///< Sequence number (12 or 18 bits)
  rlc_am_sn_size sn_size;  ///< SN size
  bool           p;        ///< Polling bit
  uint16_t       so;       ///< Segment offset (only for non-first segments)
};

/// \brief Reads an RLC AM Data PDU header from a byte buffer.
///
/// Unpacks the header according to the SN size and SI field.
/// For 12-bit SN:
///   - Full SDU (SI=00): 2 bytes header
///   - First segment (SI=01): 2 bytes header
///   - Middle/Last segment (SI=10/11): 4 bytes header
/// For 18-bit SN:
///   - Full SDU (SI=00): 3 bytes header
///   - First segment (SI=01): 3 bytes header
///   - Middle/Last segment (SI=10/11): 5 bytes header
///
/// \param pdu The PDU buffer to read from (header will be removed)
/// \param sn_size The sequence number size (12 or 18 bits)
/// \param hdr Pointer to the header structure to populate
/// \return true if header was successfully unpacked, false otherwise
bool rlc_am_read_data_pdu_header(byte_buffer& pdu, rlc_am_sn_size sn_size, rlc_am_pdu_header* hdr);

/// \brief Writes an RLC AM Data PDU header to a buffer.
///
/// Packs the header according to the SN size and SI field.
///
/// \param buf The buffer to write to
/// \param hdr The header to pack
/// \return The number of bytes written (header size)
size_t rlc_am_write_data_pdu_header(span<uint8_t> buf, const rlc_am_pdu_header& hdr);

/// Represents a NACK entry in a Status PDU
struct rlc_am_status_nack {
  static constexpr uint16_t so_end_of_sdu = 0xFFFF; ///< Special value indicating end of SDU

  uint32_t nack_sn;       ///< SN of the RLC SDU being NACKed
  bool     has_so;        ///< Whether SO start/end fields are present
  uint16_t so_start;      ///< Start of missing segment
  uint16_t so_end;        ///< End of missing segment
  bool     has_nack_range; ///< Whether NACK range field is present
  uint8_t  nack_range;    ///< Number of consecutive NACKed SDUs (starting from nack_sn)

  /// Equality operator for NACK entries
  bool operator==(const rlc_am_status_nack& other) const
  {
    return nack_sn == other.nack_sn && has_so == other.has_so && so_start == other.so_start &&
           so_end == other.so_end && has_nack_range == other.has_nack_range && nack_range == other.nack_range;
  }
};

/// \brief RLC AM Status PDU class.
///
/// Handles packing and unpacking of STATUS PDUs as defined in 3GPP TS 38.322.
class rlc_am_status_pdu
{
public:
  /// Constructs a Status PDU with the given SN size.
  explicit rlc_am_status_pdu(rlc_am_sn_size sn_sz) : sn_size(sn_sz) {}

  /// Checks if the given PDU is a control PDU (D/C bit = 0).
  static bool is_control_pdu(const byte_buffer& pdu);

  /// Unpacks the Status PDU from the given buffer.
  bool unpack(const byte_buffer& pdu);

  /// Packs the Status PDU into the given buffer.
  size_t pack(span<uint8_t> buf) const;

  /// Returns the packed size of the Status PDU.
  size_t get_packed_size() const;

  /// Trims the Status PDU to the given size. Removes NACKs from the end as needed.
  /// \param size The target packed size.
  /// \return true if trimming was successful, false if target size is too small.
  bool trim(size_t size);

  /// Pushes a NACK entry to the Status PDU with merge logic for overlapping entries.
  void push_nack(const rlc_am_status_nack& nack);

  /// Resets the Status PDU to empty state (clears all NACKs).
  void reset();

  /// Returns the Acknowledgement SN.
  uint32_t get_ack_sn() const { return ack_sn; }

  /// Returns the list of NACK entries.
  const std::vector<rlc_am_status_nack>& get_nacks() const { return nacks; }

  // Public fields for test access
  uint32_t                  ack_sn = 0;
  std::vector<rlc_am_status_nack> nacks;

private:
  rlc_am_sn_size sn_size;
};

} // namespace srsran

namespace fmt {

template <>
struct formatter<srsran::rlc_am_pdu_header> {
  template <typename ParseContext>
  auto parse(ParseContext& ctx)
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const srsran::rlc_am_pdu_header& hdr, FormatContext& ctx) const
  {
    return format_to(ctx.out(), "si={} sn={} p={} so={}", hdr.si, hdr.sn, hdr.p ? 1 : 0, hdr.so);
  }
};

} // namespace fmt