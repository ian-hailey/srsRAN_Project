#include "rlc_um_pdu.h"

namespace srsran {

bool rlc_um_read_data_pdu_header(srsran::byte_buffer& buf, rlc_um_sn_size sn_size, rlc_um_pdu_header* hdr) {
  if (buf.length() == 0) return false;
  
  hdr->si = static_cast<rlc_si_field>((buf[0] >> 6) & 0x3);
  hdr->sn_size = sn_size;
  
  if (hdr->si == rlc_si_field::full_sdu) {
    if ((buf[0] & 0x3F) != 0) return false;
    buf.trim_head(1);
    return true;
  }
  
  if (sn_size == rlc_um_sn_size::size6bits) {
    hdr->sn = buf[0] & 0x3F;
    size_t header_len = 1;
    if (hdr->si != rlc_si_field::first_segment) {
      if (buf.length() < 3) return false;
      hdr->so = (buf[1] << 8) | buf[2];
      header_len = 3;
    } else {
      hdr->so = 0;
    }
    buf.trim_head(header_len);
    return true;
  } else {
    if (buf.length() < 2) return false;
    if ((buf[0] & 0x30) != 0) return false;
    hdr->sn = ((buf[0] & 0x0F) << 8) | buf[1];
    size_t header_len = 2;
    if (hdr->si != rlc_si_field::first_segment) {
      if (buf.length() < 4) return false;
      hdr->so = (buf[2] << 8) | buf[3];
      header_len = 4;
    } else {
      hdr->so = 0;
    }
    buf.trim_head(header_len);
    return true;
  }
}

size_t rlc_um_write_data_pdu_header(srsran::span<uint8_t> buf, const rlc_um_pdu_header& hdr) {
  uint8_t* ptr = buf.data();
  ptr[0] = (static_cast<uint8_t>(hdr.si) << 6);
  if (hdr.si == rlc_si_field::full_sdu) {
    return 1;
  }
  if (hdr.sn_size == rlc_um_sn_size::size6bits) {
    ptr[0] |= (hdr.sn & 0x3F);
    if (hdr.si == rlc_si_field::first_segment) {
      return 1;
    }
    ptr[1] = (hdr.so >> 8) & 0xFF;
    ptr[2] = hdr.so & 0xFF;
    return 3;
  } else {
    ptr[0] |= ((hdr.sn >> 8) & 0x0F);
    ptr[1] = hdr.sn & 0xFF;
    if (hdr.si == rlc_si_field::first_segment) {
      return 2;
    }
    ptr[2] = (hdr.so >> 8) & 0xFF;
    ptr[3] = hdr.so & 0xFF;
    return 4;
  }
}

} // namespace srsran
