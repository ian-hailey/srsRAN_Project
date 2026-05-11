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

#include "rlc_am_pdu.h"

using namespace srsran;

namespace srsran {

// Implementation of rlc_am_pdu_header methods

size_t rlc_am_pdu_header::get_packed_size() const
{
  if (si == rlc_si_field::full_sdu || si == rlc_si_field::first_segment) {
    if (sn_size == rlc_am_sn_size::size12bits) {
      return 2;
    }
    return 3;
  }
  if (sn_size == rlc_am_sn_size::size12bits) {
    return 4;
  }
  return 5;
}

bool rlc_am_read_data_pdu_header(const byte_buffer& buf, rlc_am_sn_size sn_size, rlc_am_pdu_header* hdr)
{
  if (buf.empty()) {
    return false;
  }
  hdr->sn_size = sn_size;

  auto it = buf.begin();

  uint8_t byte0 = *it;

  hdr->dc = static_cast<rlc_dc_field>((byte0 >> 7) & 0x01);
  hdr->p  = (byte0 >> 6) & 0x01;
  hdr->si = static_cast<rlc_si_field>((byte0 >> 4) & 0x03);

  if (sn_size == rlc_am_sn_size::size12bits) {
    if (buf.length() < 2) {
      return false;
    }
    ++it;
    uint8_t byte1 = *it;
    hdr->sn       = ((static_cast<uint32_t>(byte0 & 0x0F)) << 8) | byte1;

    if (hdr->si == rlc_si_field::full_sdu || hdr->si == rlc_si_field::first_segment) {
      hdr->so = 0;
      return true;
    }

    if (buf.length() < 4) {
      return false;
    }
    ++it;
    hdr->so = (static_cast<uint32_t>(*it) << 8);
    ++it;
    hdr->so |= static_cast<uint32_t>(*it);
    return true;
  }

  // 18-bit SN: byte0[3:0], byte1, byte2 all encode SN[17:0]
  if (buf.length() < 3) {
    return false;
  }
  ++it;
  uint8_t byte1 = *it;
  ++it;
  uint8_t byte2 = *it;

  // For middle/last segment: byte0[3] (bit 3 of byte0) is reserved and must be 0
  bool has_so = (hdr->si == rlc_si_field::middle_segment || hdr->si == rlc_si_field::last_segment);
  if (has_so && (byte0 & 0x08)) {
    hdr->sn = 0;
    hdr->so = 0;
    return false;
  }

  // SN is constructed from byte0[3:0], byte1, byte2 as a 24-bit value
  // Top 6 bits (bits 23:18) must be 0 to be a valid 18-bit SN
  uint32_t sn = ((static_cast<uint32_t>(byte0 & 0x0F)) << 16) | (static_cast<uint32_t>(byte1) << 8) |
                static_cast<uint32_t>(byte2);
  if (sn > 262143) {
    hdr->sn = 0;
    return false;
  }
  hdr->sn = sn;

  if (!has_so) {
    hdr->so = 0;
    return true;
  }

  if (buf.length() < 5) {
    return false;
  }
  ++it;
  hdr->so = (static_cast<uint32_t>(*it) << 8);
  ++it;
  hdr->so |= static_cast<uint32_t>(*it);
  return true;
}

size_t rlc_am_write_data_pdu_header(span<uint8_t> buf, const rlc_am_pdu_header& hdr)
{
  uint8_t dc_val = static_cast<uint8_t>(hdr.dc);
  uint8_t p_val  = hdr.p ? 1 : 0;
  uint8_t si_val = static_cast<uint8_t>(hdr.si);

  if (hdr.sn_size == rlc_am_sn_size::size12bits) {
    buf[0] = (dc_val << 7) | (p_val << 6) | (si_val << 4) | ((hdr.sn >> 8) & 0x0F);
    buf[1] = hdr.sn & 0xFF;
    if (hdr.si == rlc_si_field::middle_segment || hdr.si == rlc_si_field::last_segment) {
      buf[2] = (hdr.so >> 8) & 0xFF;
      buf[3] = hdr.so & 0xFF;
      return 4;
    }
    return 2;
  }

  // 18-bit SN: byte0[3:0], byte1, byte2 encode SN[17:0]
  buf[0] = (dc_val << 7) | (p_val << 6) | (si_val << 4) | ((hdr.sn >> 16) & 0x0F);
  buf[1] = (hdr.sn >> 8) & 0xFF;
  buf[2] = hdr.sn & 0xFF;

  if (hdr.si == rlc_si_field::full_sdu || hdr.si == rlc_si_field::first_segment) {
    return 3;
  }

  buf[3] = (hdr.so >> 8) & 0xFF;
  buf[4] = hdr.so & 0xFF;
  return 5;
}

// rlc_am_status_pdu implementation

rlc_am_status_pdu::rlc_am_status_pdu(rlc_am_sn_size sn_size_) : sn_size(sn_size_), ack_sn(0), logger(srslog::fetch_basic_logger("RLC"))
{
}

bool rlc_am_status_pdu::is_control_pdu(const byte_buffer& pdu)
{
  if (pdu.empty()) {
    return false;
  }
  return ((*pdu.begin() >> 7) & 0x01) == 0;
}

bool rlc_am_status_pdu::unpack(const byte_buffer& pdu)
{
  nacks.clear();
  ack_sn = 0;

  if (pdu.empty()) {
    return false;
  }

  // Copy to vector for safe indexed access
  size_t len = pdu.length();
  std::vector<uint8_t> buf(len);
  size_t idx = 0;
  for (auto it = pdu.begin(); it != pdu.end(); ++it) {
    buf[idx++] = *it;
  }

  uint8_t byte0 = buf[0];

  // D/C must be 0 (control)
  if ((byte0 >> 7) != 0) {
    return false;
  }

  // CPT must be 000 (STATUS)
  if (((byte0 >> 4) & 0x07) != 0) {
    return false;
  }

  if (sn_size == rlc_am_sn_size::size12bits) {
    // Need at least 3 bytes for ACK_SN header
    if (len < 3) {
      return false;
    }
    uint8_t byte1 = buf[1];
    ack_sn        = ((static_cast<uint32_t>(byte0 & 0x0F)) << 8) | byte1;

    uint8_t byte2 = buf[2];
    if (!(byte2 >> 7 & 0x01)) {
      return true;
    }

    size_t offset = 2;

    while (true) {
      // Need at least 2 bytes for NACK_SN (indices offset+1 and offset+2)
      if (offset + 2 >= len) {
        return false;
      }
      uint8_t nack_upper      = buf[offset + 1];
      uint8_t nack_lower_byte = buf[offset + 2];
      uint32_t nack_sn_val    = (static_cast<uint32_t>(nack_upper) << 4) | ((nack_lower_byte >> 4) & 0x0F);
      offset += 2;

      rlc_am_status_nack nack;
      nack.nack_sn        = nack_sn_val;
      nack.has_so         = false;
      nack.so_start       = 0;
      nack.so_end         = 0;
      nack.has_nack_range = false;
      nack.nack_range     = 0;

      uint8_t e1_val = (nack_lower_byte >> 3) & 0x01;
      uint8_t e2_val = (nack_lower_byte >> 2) & 0x01;
      uint8_t e3_val = (nack_lower_byte >> 1) & 0x01;

      if (e2_val) {
        if (offset + 4 >= len) {
          return false;
        }
        nack.so_start = (static_cast<uint32_t>(buf[offset + 1]) << 8) | static_cast<uint32_t>(buf[offset + 2]);
        nack.so_end   = (static_cast<uint32_t>(buf[offset + 3]) << 8) | static_cast<uint32_t>(buf[offset + 4]);
        offset += 4;
        nack.has_so = true;
      }

      if (e3_val) {
        if (offset + 1 >= len) {
          return false;
        }
        nack.nack_range     = buf[offset + 1];
        nack.has_nack_range = true;
        offset += 1;
      }

      nacks.push_back(nack);

      if (e1_val == 0) {
        break;
      }
    }
    return true;
  }

  // 18-bit SN
  if (len < 3) {
    return false;
  }
  uint8_t byte1 = buf[1];
  uint8_t byte2 = buf[2];

  // 18-bit ACK_SN is packed as:
  // byte0[3:0] = ACK_SN[17:14], byte1 = ACK_SN[13:6], byte2[7:2] = ACK_SN[5:0]
  // byte2[1] = E1, byte2[0] = R
  ack_sn = ((static_cast<uint32_t>(byte0 & 0x0F)) << 14) | (static_cast<uint32_t>(byte1) << 6) |
           (static_cast<uint32_t>(byte2) >> 2);

  if (ack_sn > 262143) {
    return false;
  }

  if (!((byte2 >> 1) & 0x01)) {
    return true;
  }

  size_t offset = 2;

  while (true) {
    if (offset + 3 >= len) {
      return false;
    }
    uint8_t nack_upper      = buf[offset + 1];
    uint8_t nack_center     = buf[offset + 2];
    uint8_t nack_lower_byte = buf[offset + 3];
    offset += 3;

    // 18-bit NACK_SN: byte0 = NACK_SN[17:10], byte1 = NACK_SN[9:2], byte2[7:6] = NACK_SN[1:0]
    // byte2[5] = E1, byte2[4] = E2, byte2[3] = E3
    uint32_t nack_sn_val = (static_cast<uint32_t>(nack_upper) << 10) |
                           (static_cast<uint32_t>(nack_center) << 2) |
                           ((static_cast<uint32_t>(nack_lower_byte) >> 6) & 0x03);
    if (nack_sn_val > 262143) {
      return false;
    }

    rlc_am_status_nack nack;
    nack.nack_sn        = nack_sn_val;
    nack.has_so         = false;
    nack.so_start       = 0;
    nack.so_end         = 0;
    nack.has_nack_range = false;
    nack.nack_range     = 0;

    uint8_t e1_val = (nack_lower_byte >> 5) & 0x01;
    uint8_t e2_val = (nack_lower_byte >> 4) & 0x01;
    uint8_t e3_val = (nack_lower_byte >> 3) & 0x01;

    if (e2_val) {
      if (offset + 4 >= len) {
        return false;
      }
      nack.so_start = (static_cast<uint32_t>(buf[offset + 1]) << 8) | static_cast<uint32_t>(buf[offset + 2]);
      nack.so_end   = (static_cast<uint32_t>(buf[offset + 3]) << 8) | static_cast<uint32_t>(buf[offset + 4]);
      offset += 4;
      nack.has_so = true;
    }

    if (e3_val) {
      if (offset + 1 >= len) {
        return false;
      }
      nack.nack_range     = buf[offset + 1];
      nack.has_nack_range = true;
      offset += 1;
    }

    nacks.push_back(nack);

    if (e1_val == 0) {
      break;
    }
  }
  return true;
}

size_t rlc_am_status_pdu::pack(span<uint8_t> buf) const
{
  if (sn_size == rlc_am_sn_size::size12bits) {
    buf[0] = (0 << 7) | (0 << 4) | ((ack_sn >> 8) & 0x0F);
    buf[1] = ack_sn & 0xFF;
    buf[2] = (nacks.empty() ? 0x00 : 0x80) | 0x00;

    size_t offset = 3;

    for (unsigned i = 0; i < nacks.size(); i++) {
      const auto& nack = nacks[i];
      bool has_next    = (i + 1 < nacks.size());

      buf[offset]     = (nack.nack_sn >> 4) & 0xFF;
      buf[offset + 1] = ((nack.nack_sn & 0x0F) << 4) | (has_next ? 0x08 : 0x00) |
                        (nack.has_so ? 0x04 : 0x00) | (nack.has_nack_range ? 0x02 : 0x00) | 0x00;
      offset += 2;

      if (nack.has_so) {
        buf[offset]     = (nack.so_start >> 8) & 0xFF;
        buf[offset + 1] = nack.so_start & 0xFF;
        buf[offset + 2] = (nack.so_end >> 8) & 0xFF;
        buf[offset + 3] = nack.so_end & 0xFF;
        offset += 4;
      }

      if (nack.has_nack_range) {
        buf[offset] = nack.nack_range;
        offset += 1;
      }
    }

    return offset;
  }

  // 18-bit SN STATUS PDU
  // byte0[3:0] = ACK_SN[17:14], byte1 = ACK_SN[13:6], byte2[7:2] = ACK_SN[5:0], byte2[1] = E1, byte2[0] = R
  buf[0] = (0 << 7) | (0 << 4) | ((ack_sn >> 14) & 0x0F);
  buf[1] = (ack_sn >> 6) & 0xFF;
  buf[2] = ((ack_sn & 0x3F) << 2) | (nacks.empty() ? 0x00 : 0x02) | 0x00;

  size_t offset = 3;

  for (unsigned i = 0; i < nacks.size(); i++) {
    const auto& nack = nacks[i];
    bool has_next    = (i + 1 < nacks.size());

    // byte0 = NACK_SN[17:10], byte1 = NACK_SN[9:2], byte2[7:6] = NACK_SN[1:0]
    // byte2[5] = E1, byte2[4] = E2, byte2[3] = E3, byte2[2:0] = RRR
    buf[offset]     = (nack.nack_sn >> 10) & 0xFF;
    buf[offset + 1] = (nack.nack_sn >> 2) & 0xFF;
    buf[offset + 2] = ((nack.nack_sn & 0x03) << 6) | (has_next ? 0x20 : 0x00) |
                      (nack.has_so ? 0x10 : 0x00) | (nack.has_nack_range ? 0x08 : 0x00) | 0x00;
    offset += 3;

    if (nack.has_so) {
      buf[offset]     = (nack.so_start >> 8) & 0xFF;
      buf[offset + 1] = nack.so_start & 0xFF;
      buf[offset + 2] = (nack.so_end >> 8) & 0xFF;
      buf[offset + 3] = nack.so_end & 0xFF;
      offset += 4;
    }

    if (nack.has_nack_range) {
      buf[offset] = nack.nack_range;
      offset += 1;
    }
  }

  return offset;
}

size_t rlc_am_status_pdu::get_packed_size() const
{
  size_t size = 0;
  if (sn_size == rlc_am_sn_size::size12bits) {
    size = 3;
    for (auto& nack : nacks) {
      size += 2;
      if (nack.has_so) {
        size += 4;
      }
      if (nack.has_nack_range) {
        size += 1;
      }
    }
  } else {
    size = 3;
    for (auto& nack : nacks) {
      size += 3;
      if (nack.has_so) {
        size += 4;
      }
      if (nack.has_nack_range) {
        size += 1;
      }
    }
  }
  return size;
}

void rlc_am_status_pdu::reset()
{
  nacks.clear();
  ack_sn = 0;
}

void rlc_am_status_pdu::push_nack(const rlc_am_status_nack& nack)
{
  if (nacks.empty()) {
    nacks.push_back(nack);
    return;
  }

  rlc_am_status_nack& prev = nacks.back();
  uint32_t mod = cardinality(to_number(sn_size));

  // Determine if the SNs are consecutive (prev.sn + 1 == nack.sn) using modular arithmetic
  bool sn_consecutive = ((prev.nack_sn + 1) % mod == nack.nack_sn);

  // Determine if the SNs are continuous considering a range (prev.sn + prev.range == nack.sn)
  bool sn_range_continuous = false;
  if (prev.has_nack_range) {
    sn_range_continuous = ((prev.nack_sn + prev.nack_range) % mod == nack.nack_sn);
  }

  // Case: prev=SDU + curr=SDU -> merge into range
  if (sn_consecutive && !prev.has_so && !prev.has_nack_range && !nack.has_so && !nack.has_nack_range) {
    prev.has_nack_range = true;
    prev.nack_range = 2;
    return;
  }

  // Case: prev=SDU + curr=segment (so_start==0) -> add SO to prev + range (not range+segm)
  if (sn_consecutive && !prev.has_so && !prev.has_nack_range && nack.has_so && nack.so_start == 0 && !nack.has_nack_range) {
    prev.has_so = true;
    prev.so_start = 0;
    prev.so_end = nack.so_end;
    prev.has_nack_range = true;
    prev.nack_range = 2;
    return;
  }

  // Case: prev=segment (so_end==0xffff) + curr=SDU -> add range
  if (sn_consecutive && prev.has_so && !prev.has_nack_range && !nack.has_so && !nack.has_nack_range && prev.so_end == rlc_am_status_nack::so_end_of_sdu) {
    prev.has_nack_range = true;
    prev.nack_range = 2;
    return;
  }

  // Case: prev=segment (so_end==0xffff) + curr=segment (so_start==0) -> merge SO, add range (not range+segm)
  if (sn_consecutive && prev.has_so && !prev.has_nack_range && nack.has_so && nack.so_start == 0 && !nack.has_nack_range && prev.so_end == rlc_am_status_nack::so_end_of_sdu) {
    prev.so_end = nack.so_end;
    prev.has_nack_range = true;
    prev.nack_range = 2;
    return;
  }

  // Case: prev=range + curr=SDU (continuous when curr.sn == prev.sn + prev.range)
  if (sn_range_continuous && prev.has_nack_range && !prev.has_so && !nack.has_so && !nack.has_nack_range) {
    if (prev.nack_range < 255) {
      prev.nack_range++;
      return;
    }
    nacks.push_back(nack);
    return;
  }

  // Case: prev=range + curr=segment (so_start==0) -> add SO to prev, extend range
  if (sn_range_continuous && prev.has_nack_range && !prev.has_so && nack.has_so && nack.so_start == 0 && !nack.has_nack_range) {
    prev.has_so = true;
    prev.so_start = 0;
    prev.so_end = nack.so_end;
    if (prev.nack_range < 255) {
      prev.nack_range++;
      return;
    }
    nacks.push_back(nack);
    return;
  }

  // Case: prev=range with SO + curr=SDU (continuous when curr.sn == prev.sn + prev.range)
  if (sn_range_continuous && prev.has_nack_range && prev.has_so && !nack.has_so && !nack.has_nack_range) {
    if (prev.so_end == rlc_am_status_nack::so_end_of_sdu && prev.nack_range < 255) {
      prev.nack_range++;
      return;
    }
    nacks.push_back(nack);
    return;
  }

  // Case: prev=range with SO + curr=segment (so_start==0) -> update SO end, extend range
  if (sn_range_continuous && prev.has_nack_range && prev.has_so && nack.has_so && nack.so_start == 0 && !nack.has_nack_range) {
    if (prev.so_end == rlc_am_status_nack::so_end_of_sdu && prev.nack_range < 255) {
      prev.so_end = nack.so_end;
      prev.nack_range++;
      return;
    }
    nacks.push_back(nack);
    return;
  }

  // Case: prev=SDU + curr=range (no SO)
  if (sn_consecutive && !prev.has_so && !prev.has_nack_range && nack.has_nack_range && !nack.has_so) {
    if (nack.nack_range < 255) {
      prev.has_nack_range = true;
      prev.nack_range = nack.nack_range + 1;
      return;
    }
    nacks.push_back(nack);
    return;
  }

  // Case: prev=SDU + curr=range+segm (so_start==0)
  if (sn_consecutive && !prev.has_so && !prev.has_nack_range && nack.has_nack_range && nack.has_so && nack.so_start == 0) {
    if (nack.nack_range < 255) {
      prev.has_so = true;
      prev.so_start = 0;
      prev.so_end = nack.so_end;
      prev.has_nack_range = true;
      prev.nack_range = nack.nack_range + 1;
      return;
    }
    nacks.push_back(nack);
    return;
  }

  // Case: prev=segment + curr=range (no SO)
  if (sn_consecutive && prev.has_so && !prev.has_nack_range && nack.has_nack_range && !nack.has_so) {
    if (prev.so_end == rlc_am_status_nack::so_end_of_sdu) {
      prev.has_nack_range = true;
      prev.nack_range = nack.nack_range + 1;
      return;
    }
    nacks.push_back(nack);
    return;
  }

  // Case: prev=segment + curr=range+segm (so_start==0)
  if (sn_consecutive && prev.has_so && !prev.has_nack_range && nack.has_nack_range && nack.has_so && nack.so_start == 0) {
    if (prev.so_end == rlc_am_status_nack::so_end_of_sdu) {
      prev.so_end = nack.so_end;
      prev.has_nack_range = true;
      prev.nack_range = nack.nack_range + 1;
      return;
    }
    nacks.push_back(nack);
    return;
  }

  // Case: prev=range (no SO) + curr=range (no SO) (merge if continuous)
  if (sn_range_continuous && prev.has_nack_range && !prev.has_so && nack.has_nack_range && !nack.has_so) {
    if (prev.nack_range + nack.nack_range <= 255) {
      prev.nack_range += nack.nack_range;
      return;
    }
    nacks.push_back(nack);
    return;
  }

  // Case: prev=range (no SO) + curr=range+segm (so_start==0) -> add SO, merge ranges
  if (sn_range_continuous && prev.has_nack_range && !prev.has_so && nack.has_nack_range && nack.has_so && nack.so_start == 0) {
    if (prev.nack_range + nack.nack_range <= 255) {
      prev.has_so = true;
      prev.so_start = 0;
      prev.so_end = nack.so_end;
      prev.nack_range += nack.nack_range;
      return;
    }
    nacks.push_back(nack);
    return;
  }

  // Case: prev=range with SO + curr=range (no SO)
  if (sn_range_continuous && prev.has_nack_range && prev.has_so && nack.has_nack_range && !nack.has_so) {
    if (prev.so_end == rlc_am_status_nack::so_end_of_sdu && prev.nack_range + nack.nack_range <= 255) {
      prev.nack_range += nack.nack_range;
      return;
    }
    nacks.push_back(nack);
    return;
  }

  // Case: prev=range with SO + curr=range+segm (so_start==0) -> update SO end, merge ranges
  if (sn_range_continuous && prev.has_nack_range && prev.has_so && nack.has_nack_range && nack.has_so && nack.so_start == 0) {
    if (prev.so_end == rlc_am_status_nack::so_end_of_sdu && prev.nack_range + nack.nack_range <= 255) {
      prev.so_end = nack.so_end;
      prev.nack_range += nack.nack_range;
      return;
    }
    nacks.push_back(nack);
    return;
  }

  // Fallback: append
  nacks.push_back(nack);
}

bool rlc_am_status_pdu::trim(uint32_t max_size)
{
  if (get_packed_size() <= max_size) {
    return true;
  }

  // Cannot trim if there are no NACKs to remove
  if (nacks.empty()) {
    return false;
  }

  // Work on a copy to avoid modifying the PDU if trimming fails
  std::vector<rlc_am_status_nack> nacks_copy = nacks;

  while (!nacks_copy.empty()) {
    uint32_t removed_sn = nacks_copy.back().nack_sn;
    // Remove all NACKs with the same SN (they all refer to the same SDU)
    while (!nacks_copy.empty() && nacks_copy.back().nack_sn == removed_sn) {
      nacks_copy.pop_back();
    }

    // Calculate size after removal
    size_t new_size = 3; // minimum size
    for (auto& n : nacks_copy) {
      new_size += (sn_size == rlc_am_sn_size::size12bits ? 2 : 3);
      if (n.has_so) {
        new_size += 4;
      }
      if (n.has_nack_range) {
        new_size += 1;
      }
    }

    if (new_size <= max_size) {
      // Apply the changes
      nacks = nacks_copy;
      ack_sn = removed_sn;
      return true;
    }
  }

  // Cannot trim to the requested size
  return false;
}

} // namespace srsran