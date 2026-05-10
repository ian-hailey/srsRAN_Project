#pragma once

#include "srsran/adt/byte_buffer.h"
#include "srsran/adt/span.h"
#include <cstdint>
#include <vector>
#include <fmt/format.h>
#include "srsran/srslog/srslog.h"
#include "rlc_um_pdu.h"

namespace srsran {

enum class rlc_am_sn_size {
  size12bits = 12,
  size18bits = 18
};

constexpr uint32_t INVALID_RLC_SN = 0xffffffff;

struct rlc_am_pdu_header {
  bool dc = true; // true=data, false=control
  bool p = false;
  rlc_si_field si = rlc_si_field::full_sdu;
  uint16_t so = 0;
  uint32_t sn = 0;
  rlc_am_sn_size sn_size = rlc_am_sn_size::size12bits;

  size_t get_packed_size() const {
    size_t sz = (sn_size == rlc_am_sn_size::size12bits) ? 2 : 3;
    if (si == rlc_si_field::middle_segment || si == rlc_si_field::last_segment) {
      sz += 2;
    }
    return sz;
  }
};

bool rlc_am_read_data_pdu_header(srsran::byte_buffer& buf, rlc_am_sn_size sn_size, rlc_am_pdu_header* hdr);
size_t rlc_am_write_data_pdu_header(srsran::span<uint8_t> buf, const rlc_am_pdu_header& hdr);

struct rlc_am_status_nack {
  static constexpr uint16_t so_end_of_sdu = 0xffff;

  uint32_t nack_sn = 0;
  bool has_so = false;
  uint16_t so_start = 0;
  uint16_t so_end = 0;
  bool has_nack_range = false;
  uint8_t nack_range = 0;
  
  bool operator==(const rlc_am_status_nack& other) const {
    return nack_sn == other.nack_sn && has_so == other.has_so && so_start == other.so_start && so_end == other.so_end && has_nack_range == other.has_nack_range && nack_range == other.nack_range;
  }
};

class rlc_am_status_pdu {
public:
  rlc_am_status_pdu() = default;
  explicit rlc_am_status_pdu(rlc_am_sn_size sn_size_) : sn_size(sn_size_) {}

  uint32_t ack_sn = 0;
  rlc_am_sn_size sn_size = rlc_am_sn_size::size12bits;

  size_t get_packed_size() const;
  size_t pack(srsran::span<uint8_t> buf) const;
  bool unpack(srsran::byte_buffer& buf);
  bool unpack(srsran::byte_buffer& buf, rlc_am_sn_size sn_size);
  bool trim(size_t max_size);
  void push_nack(const rlc_am_status_nack& nack) {
    if (!nacks.empty()) {
      auto& prev = nacks.back();
      uint32_t mod = (sn_size == rlc_am_sn_size::size12bits) ? 4096 : 262144;
      uint32_t prev_range = prev.has_nack_range ? prev.nack_range : 1;
      uint32_t next_range = nack.has_nack_range ? nack.nack_range : 1;
      
      uint32_t expected_next = (prev.nack_sn + prev_range) % mod;
      
      if (expected_next == nack.nack_sn) {
        bool prev_covers_end = (!prev.has_so || prev.so_end == 0xffff);
        bool next_covers_start = (!nack.has_so || nack.so_start == 0);
        
        if (prev_covers_end && next_covers_start && (prev_range + next_range <= 255)) {
          bool new_has_so = prev.has_so || nack.has_so;
          uint16_t new_so_start = prev.has_so ? prev.so_start : 0;
          uint16_t new_so_end = nack.has_so ? nack.so_end : 0xffff;
          
          prev.has_nack_range = true;
          prev.nack_range = prev_range + next_range;
          prev.has_so = new_has_so;
          prev.so_start = new_has_so ? new_so_start : 0;
          prev.so_end = new_has_so ? new_so_end : 0;
          return;
        }
      }
    }
    nacks.push_back(nack);
  }
  const std::vector<rlc_am_status_nack>& get_nacks() const { return nacks; }
  void reset() { nacks.clear(); ack_sn = 0; }
  
  static bool is_control_pdu(srsran::byte_buffer& buf);

private:
  std::vector<rlc_am_status_nack> nacks;
};

inline constexpr uint32_t to_number(rlc_am_sn_size sz) {
  return static_cast<uint32_t>(sz);
}

inline constexpr uint32_t cardinality(uint32_t bits) {
  return 1u << bits;
}

} // namespace srsran

namespace fmt {
template <>
struct formatter<srsran::rlc_am_sn_size> : formatter<int> {
  template <typename FormatContext>
  auto format(srsran::rlc_am_sn_size c, FormatContext& ctx) const {
    return formatter<int>::format(static_cast<int>(c), ctx);
  }
};
} // namespace fmt
