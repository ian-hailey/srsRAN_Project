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
#include "srsran/srslog/srslog.h"
#include <array>
#include <cstdint>
#include <vector>
#include <fmt/format.h>

namespace srsran {

/// Invalid RLC sequence number value.
constexpr uint32_t INVALID_RLC_SN = 0xffffffff;

/// RLC AM SN size configuration.
enum class rlc_am_sn_size : unsigned {
  size12bits,  ///< 12-bit sequence number
  size18bits   ///< 18-bit sequence number
};

/// Convert rlc_am_sn_size to a numeric value.
constexpr unsigned to_number(rlc_am_sn_size sn_size)
{
  return static_cast<unsigned>(sn_size);
}

/// RLC Segmentation Info field values.
enum class rlc_si_field : unsigned {
  full_sdu       = 0,  ///< Complete RLC SDU
  first_segment  = 1,  ///< First segment of an RLC SDU
  last_segment   = 2,  ///< Last segment of an RLC SDU
  middle_segment = 3   ///< Middle segment of an RLC SDU
};

/// RLC AM PDU header structure.
struct rlc_am_pdu_header {
  rlc_am_sn_size sn_size = rlc_am_sn_size::size12bits;  ///< SN size configuration
  uint8_t        dc      = 1;                            ///< Data/Control flag (1=Data, 0=Control)
  uint8_t        p       = 0;                            ///< Polling bit
  rlc_si_field   si      = rlc_si_field::full_sdu;       ///< Segmentation Info
  uint32_t       sn      = 0;                            ///< Sequence Number
  uint16_t       so      = 0;                            ///< Segment Offset (only for segments)
};

/// RLC AM Status PDU NACK structure.
struct rlc_am_status_nack {
  static constexpr uint16_t so_end_of_sdu = 0xFFFF;  ///< Special value indicating end of SDU

  uint32_t nack_sn    = 0;       ///< NACK sequence number
  bool     has_so     = false;   ///< Whether SO fields are present
  uint16_t so_start   = 0;       ///< Start offset
  uint16_t so_end     = 0;       ///< End offset
  bool     has_nack_range = false; ///< Whether NACK range is present
  uint8_t  nack_range = 0;       ///< Number of consecutive missing SDUs

  bool operator==(const rlc_am_status_nack& other) const {
    return nack_sn == other.nack_sn &&
           has_so == other.has_so &&
           so_start == other.so_start &&
           so_end == other.so_end &&
           has_nack_range == other.has_nack_range &&
           nack_range == other.nack_range;
  }
};

/// Calculate cardinality based on SN size index.
constexpr uint32_t cardinality(unsigned sn_size_idx)
{
  // sn_size_idx: 0 = 12-bit, 1 = 18-bit
  return (sn_size_idx == 0) ? 4096 : 262144;
}

/// RLC AM Status PDU class.
class rlc_am_status_pdu {
public:
  /// Construct a status PDU with the given SN size.
  explicit rlc_am_status_pdu(rlc_am_sn_size sn_size_);
  rlc_am_status_pdu(const rlc_am_status_pdu& other);
  rlc_am_status_pdu& operator=(const rlc_am_status_pdu& other);

  /// ACK_SN field - sequence number of the next expected PDU.
  uint32_t ack_sn = 0;

  /// Get reference to the NACK list.
  std::vector<rlc_am_status_nack>& get_nacks() { return nacks; }
  const std::vector<rlc_am_status_nack>& get_nacks() const { return nacks; }

  /// Add a NACK to the status PDU.
  /// \param nack NACK structure to add.
  void push_nack(const rlc_am_status_nack& nack);

  /// Get the estimated packed size of the status PDU.
  /// \returns Estimated size in bytes.
  size_t get_packed_size() const;

  /// Trim the status PDU to fit within the specified size.
  /// \param max_size Maximum size in bytes.
  /// \returns true if trimming was successful.
  bool trim(size_t max_size);

  /// Reset the status PDU.
  void reset();

  /// Unpack a status PDU from a byte buffer.
  /// \param buf Byte buffer containing the PDU.
  /// \returns true on success, false on failure.
  bool unpack(const byte_buffer& buf);

  /// Pack the status PDU to a span.
  /// \param buf Output span to write the PDU.
  /// \returns Number of bytes written.
  size_t pack(span<uint8_t> buf) const;

  /// Check if a byte buffer contains a control PDU.
  /// \param buf Byte buffer to check.
  /// \returns true if it's a control PDU.
  static bool is_control_pdu(const byte_buffer& buf);

private:
  rlc_am_sn_size sn_size;
  std::vector<rlc_am_status_nack> nacks;
};

/// Read an RLC AM data PDU header from a byte buffer.
/// \param buf Byte buffer containing the PDU.
/// \param sn_size SN size configuration.
/// \param hdr Output structure for the parsed header.
/// \returns true on success, false on failure.
bool rlc_am_read_data_pdu_header(const byte_buffer& buf, rlc_am_sn_size sn_size, rlc_am_pdu_header* hdr);

/// Write an RLC AM data PDU header to a span.
/// \param buf Output span to write the header.
/// \param hdr Header structure to serialize.
/// \returns Number of bytes written (header length).
size_t rlc_am_write_data_pdu_header(span<uint8_t> buf, const rlc_am_pdu_header& hdr);

} // namespace srsran

// Formatter for rlc_am_sn_size (must be outside srsran namespace)
template <>
struct fmt::formatter<srsran::rlc_am_sn_size> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const srsran::rlc_am_sn_size& sn_size, FormatContext& ctx) const {
    return fmt::format_to(ctx.out(), "{}bit", srsran::to_number(sn_size));
  }
};
