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
#include <vector>

namespace srsran {

/// \brief Invalid RLC sequence number.
constexpr uint32_t INVALID_RLC_SN = 0xffffffff;

/// \brief Header of an AMD PDU.
///
/// Ref: 3GPP TS 38.322 Sec. 6.2.2.4
struct rlc_am_pdu_header {
  rlc_dc_field   dc      = rlc_dc_field::data; ///< Data/Control (1 bit)
  uint32_t       p       = 0;                  ///< Polling bit (1 bit)
  rlc_si_field   si      = rlc_si_field::full_sdu; ///< Segmentation Info (2 bits)
  rlc_am_sn_size sn_size = rlc_am_sn_size::size12bits; ///< SN size (12 or 18 bits)
  uint32_t       sn      = 0;                  ///< Sequence Number
  uint32_t       so      = 0;                  ///< Segment Offset (16 bits)

  /// \brief Get the packed size of the header based on SI and SN size.
  /// \return Size of the header in bytes.
  size_t get_packed_size() const;
};

/// \brief Structure representing a NACK in a STATUS PDU.
struct rlc_am_status_nack {
  static constexpr uint16_t so_end_of_sdu = 0xffff; ///< Special SOend value indicating "to the end of the SDU"

  uint32_t  nack_sn        = 0;      ///< SN of the NACK'ed SDU
  bool      has_so         = false;  ///< Flag indicating if SO_start/SO_end are present
  uint32_t  so_start       = 0;      ///< Start of missing byte segment (16 bits)
  uint32_t  so_end         = 0;      ///< End of missing byte segment (16 bits)
  bool      has_nack_range = false;  ///< Flag indicating if NACK range is present
  uint32_t  nack_range     = 0;      ///< Number of consecutively lost SDUs (8 bits)
};

/// \brief Class representing an RLC AM STATUS PDU.
///
/// Handles packing and unpacking of STATUS PDUs for both 12-bit and 18-bit SN formats.
/// Ref: 3GPP TS 38.322 Sec. 6.2.2.5
class rlc_am_status_pdu
{
public:
  /// \brief Constructor.
  /// \param sn_size_ SN size (12bit or 18bit).
  explicit rlc_am_status_pdu(rlc_am_sn_size sn_size_);

  /// \brief Pack the STATUS PDU into a buffer.
  /// \param buf Buffer to write the packed PDU.
  /// \return Number of bytes written.
  size_t pack(span<uint8_t> buf) const;

  /// \brief Unpack a STATUS PDU from a byte buffer.
  /// \param pdu Buffer containing the PDU.
  /// \return true if the unpacking was successful, false otherwise.
  bool unpack(const byte_buffer& pdu);

  /// \brief Check if a byte buffer contains a control PDU.
  /// \param pdu Buffer to check.
  /// \return true if the buffer contains a control PDU.
  static bool is_control_pdu(const byte_buffer& pdu);

  /// \brief Add a NACK to the STATUS PDU.
  /// \param nack NACK to add.
  void push_nack(const rlc_am_status_nack& nack);

  /// \brief Get the list of NACKs.
  /// \return Const reference to the NACK vector.
  const std::vector<rlc_am_status_nack>& get_nacks() const { return nacks; }

  /// \brief Get the packed size of the STATUS PDU in bytes.
  /// \return Size of the packed PDU in bytes.
  size_t get_packed_size() const;

  /// \brief Reset the STATUS PDU (clear all NACKs and reset ACK_SN).
  void reset();

  /// \brief Trim NACKs until the packed size fits within the given maximum size.
  /// \param max_size Maximum allowed packed size in bytes.
  /// \return true if any NACKs were removed, false otherwise.
  bool trim(uint32_t max_size);

  rlc_am_sn_size sn_size = rlc_am_sn_size::size12bits; ///< SN size
  uint32_t       ack_sn = 0;                            ///< Acknowledgement SN

private:
  std::vector<rlc_am_status_nack> nacks; ///< List of NACKs
  srslog::basic_logger&           logger; ///< Logger
};

/// \brief Read the header of an AMD PDU from a byte buffer.
/// \param buf Buffer containing the PDU.
/// \param sn_size SN size (12bit or 18bit).
/// \param hdr Output header structure.
/// \return true if the header was successfully read, false otherwise.
bool rlc_am_read_data_pdu_header(const byte_buffer& buf, rlc_am_sn_size sn_size, rlc_am_pdu_header* hdr);

/// \brief Write the header of an AMD PDU to a buffer.
/// \param buf Buffer to write the header to.
/// \param hdr Header structure.
/// \return Number of bytes written.
size_t rlc_am_write_data_pdu_header(span<uint8_t> buf, const rlc_am_pdu_header& hdr);

inline bool operator==(const rlc_am_status_nack& lhs, const rlc_am_status_nack& rhs)
{
  return lhs.nack_sn == rhs.nack_sn && lhs.has_so == rhs.has_so && lhs.so_start == rhs.so_start &&
         lhs.so_end == rhs.so_end && lhs.has_nack_range == rhs.has_nack_range && lhs.nack_range == rhs.nack_range;
}

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
    return format_to(ctx.out(),
                     "rlc_am_pdu_header dc={} p={} si={} sn_size={} sn={} so={}",
                     hdr.dc,
                     hdr.p,
                     hdr.si,
                     to_number(hdr.sn_size),
                     hdr.sn,
                     hdr.so);
  }
};

template <>
struct formatter<srsran::rlc_am_status_nack> {
  template <typename ParseContext>
  auto parse(ParseContext& ctx)
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const srsran::rlc_am_status_nack& nack, FormatContext& ctx) const
  {
    return format_to(ctx.out(),
                     "rlc_am_status_nack nack_sn={} has_so={} so_start={} so_end={} has_nack_range={} nack_range={}",
                     nack.nack_sn,
                     nack.has_so,
                     nack.so_start,
                     nack.so_end,
                     nack.has_nack_range,
                     nack.nack_range);
  }
};

template <>
struct formatter<srsran::rlc_am_status_pdu> {
  template <typename ParseContext>
  auto parse(ParseContext& ctx)
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const srsran::rlc_am_status_pdu& status, FormatContext& ctx) const
  {
    return format_to(ctx.out(),
                     "rlc_am_status_pdu sn_size={} ack_sn={} nacks=[{}]",
                     to_number(status.sn_size),
                     status.ack_sn,
                     status.get_nacks().size());
  }
};

} // namespace fmt