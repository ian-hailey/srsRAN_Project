#pragma once

#include "srsran/adt/byte_buffer.h"
#include "srsran/adt/span.h"
#include <cstdint>
#include <fmt/format.h>
#include "srsran/srslog/srslog.h"

namespace srsran {

enum class rlc_si_field {
  full_sdu = 0,
  first_segment = 1,
  last_segment = 2,
  middle_segment = 3
};

enum class rlc_um_sn_size {
  size6bits = 6,
  size12bits = 12
};

struct rlc_um_pdu_header {
  rlc_si_field si = rlc_si_field::full_sdu;
  uint16_t so = 0;
  uint32_t sn = 0;
  rlc_um_sn_size sn_size = rlc_um_sn_size::size12bits;
};

bool rlc_um_read_data_pdu_header(srsran::byte_buffer& buf, rlc_um_sn_size sn_size, rlc_um_pdu_header* hdr);
size_t rlc_um_write_data_pdu_header(srsran::span<uint8_t> buf, const rlc_um_pdu_header& hdr);

} // namespace srsran
