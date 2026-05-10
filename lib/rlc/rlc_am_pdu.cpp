#include "rlc_am_pdu.h"

namespace srsran {

bool rlc_am_read_data_pdu_header(srsran::byte_buffer& buf, rlc_am_sn_size sn_size, rlc_am_pdu_header* hdr) {
  if (buf.length() == 0) return false;
  uint8_t b0 = buf[0];
  hdr->dc = (b0 & 0x80) != 0;
  if (!hdr->dc) return false;
  hdr->p = (b0 & 0x40) != 0;
  hdr->si = static_cast<rlc_si_field>((b0 >> 4) & 0x3);
  hdr->sn_size = sn_size;
  
  if (sn_size == rlc_am_sn_size::size12bits) {
    if (buf.length() < 2) return false;
    hdr->sn = ((b0 & 0x0F) << 8) | buf[1];
    size_t header_len = 2;
    if (hdr->si == rlc_si_field::middle_segment || hdr->si == rlc_si_field::last_segment) {
      if (buf.length() < 4) return false;
      hdr->so = (buf[2] << 8) | buf[3];
      header_len = 4;
    } else {
      hdr->so = 0;
    }
    buf.trim_head(header_len);
    return true;
  } else {
    if (buf.length() < 3) return false;
    if ((b0 & 0x0C) != 0) return false; // R bits must be 0
    hdr->sn = ((b0 & 0x03) << 16) | (buf[1] << 8) | buf[2];
    size_t header_len = 3;
    if (hdr->si == rlc_si_field::middle_segment || hdr->si == rlc_si_field::last_segment) {
      if (buf.length() < 5) return false;
      hdr->so = (buf[3] << 8) | buf[4];
      header_len = 5;
    } else {
      hdr->so = 0;
    }
    buf.trim_head(header_len);
    return true;
  }
}

size_t rlc_am_write_data_pdu_header(srsran::span<uint8_t> buf, const rlc_am_pdu_header& hdr) {
  uint8_t* ptr = buf.data();
  ptr[0] = 0x80;
  if (hdr.p) ptr[0] |= 0x40;
  ptr[0] |= (static_cast<uint8_t>(hdr.si) << 4);
  
  if (hdr.sn_size == rlc_am_sn_size::size12bits) {
    ptr[0] |= ((hdr.sn >> 8) & 0x0F);
    ptr[1] = hdr.sn & 0xFF;
    size_t header_len = 2;
    if (hdr.si == rlc_si_field::middle_segment || hdr.si == rlc_si_field::last_segment) {
      ptr[2] = (hdr.so >> 8) & 0xFF;
      ptr[3] = hdr.so & 0xFF;
      header_len = 4;
    }
    return header_len;
  } else {
    ptr[0] |= ((hdr.sn >> 16) & 0x03);
    ptr[1] = (hdr.sn >> 8) & 0xFF;
    ptr[2] = hdr.sn & 0xFF;
    size_t header_len = 3;
    if (hdr.si == rlc_si_field::middle_segment || hdr.si == rlc_si_field::last_segment) {
      ptr[3] = (hdr.so >> 8) & 0xFF;
      ptr[4] = hdr.so & 0xFF;
      header_len = 5;
    }
    return header_len;
  }
}

struct bit_reader {
  srsran::byte_buffer& buf;
  size_t bit_pos = 0;
  
  bool read_bit() {
    bool b = (buf[bit_pos / 8] >> (7 - (bit_pos % 8))) & 1;
    bit_pos++;
    return b;
  }
  uint32_t read_bits(size_t n) {
    uint32_t v = 0;
    for(size_t i=0; i<n; i++) v = (v << 1) | read_bit();
    return v;
  }
  size_t remaining() const { return buf.length() * 8 - bit_pos; }
  void skip_to_byte_boundary() {
    if (bit_pos % 8 != 0) {
      bit_pos += 8 - (bit_pos % 8);
    }
  }
};

struct bit_writer {
  srsran::span<uint8_t> buf;
  size_t bit_pos = 0;
  
  void write_bit(bool b) {
    if (b) {
      buf[bit_pos / 8] |= (1 << (7 - (bit_pos % 8)));
    } else {
      buf[bit_pos / 8] &= ~(1 << (7 - (bit_pos % 8)));
    }
    bit_pos++;
  }
  void write_bits(uint32_t v, size_t n) {
    for(size_t i=0; i<n; i++) {
      write_bit((v >> (n - 1 - i)) & 1);
    }
  }
  void skip_to_byte_boundary() {
    if (bit_pos % 8 != 0) {
      size_t rem = 8 - (bit_pos % 8);
      write_bits(0, rem);
    }
  }
  size_t bytes_written() const {
    return (bit_pos + 7) / 8;
  }
};

bool rlc_am_status_pdu::is_control_pdu(srsran::byte_buffer& buf) {
  if (buf.length() == 0) return false;
  return (buf[0] & 0x80) == 0;
}

bool rlc_am_status_pdu::unpack(srsran::byte_buffer& buf) {
  return unpack(buf, sn_size);
}

bool rlc_am_status_pdu::unpack(srsran::byte_buffer& buf, rlc_am_sn_size sn_size_) {
  sn_size = sn_size_;
  nacks.clear();
  if (buf.length() < 3) return false;
  
  bit_reader r{buf, 0};
  
  bool dc = r.read_bit();
  if (dc) return false;
  
  uint32_t cpt = r.read_bits(3);
  if (cpt != 0) return false;
  
  size_t sn_bits = (sn_size == rlc_am_sn_size::size12bits) ? 12 : 18;
  
  if (r.remaining() < sn_bits + 1) return false;
  
  ack_sn = r.read_bits(sn_bits);
  bool e1 = r.read_bit();
  
  if (sn_size == rlc_am_sn_size::size12bits) {
    r.read_bits(7); // R bits
  } else {
    r.read_bit(); // R bit
  }
  
  while (e1) {
    if (r.remaining() < sn_bits + 3) return false;
    rlc_am_status_nack nack;
    nack.nack_sn = r.read_bits(sn_bits);
    e1 = r.read_bit();
    bool e2 = r.read_bit();
    bool e3 = r.read_bit();
    
    if (sn_size == rlc_am_sn_size::size18bits) {
      r.read_bits(3); // R bits
    } else {
      r.read_bit(); // R bit
    }
    
    if (e2) {
      if (r.remaining() < 32) return false;
      nack.has_so = true;
      nack.so_start = r.read_bits(16);
      nack.so_end = r.read_bits(16);
    }
    
    if (e3) {
      if (r.remaining() < 8) return false;
      nack.has_nack_range = true;
      nack.nack_range = r.read_bits(8);
    }
    
    nacks.push_back(nack);
  }
  
  return true;
}

size_t rlc_am_status_pdu::get_packed_size() const {
  size_t sn_bits = (sn_size == rlc_am_sn_size::size12bits) ? 12 : 18;
  size_t bits = 4 + sn_bits + 1; // D/C, CPT, ACK_SN, E1
  if (sn_size == rlc_am_sn_size::size18bits) {
    bits += 1; // R bit
  } else {
    bits += 7; // R bits
  }
  
  for (const auto& nack : nacks) {
    bits += sn_bits + 3; // NACK_SN, E1, E2, E3
    if (sn_size == rlc_am_sn_size::size18bits) {
      bits += 3; // R bits
    } else {
      bits += 1; // R bit
    }
    if (nack.has_so) {
      bits += 32;
    }
    if (nack.has_nack_range) {
      bits += 8;
    }
  }
  
  return (bits + 7) / 8;
}

size_t rlc_am_status_pdu::pack(srsran::span<uint8_t> buf) const {
  for (size_t i = 0; i < buf.size(); i++) buf[i] = 0;
  bit_writer w{buf, 0};
  
  w.write_bit(false); // D/C
  w.write_bits(0, 3); // CPT
  
  size_t sn_bits = (sn_size == rlc_am_sn_size::size12bits) ? 12 : 18;
  w.write_bits(ack_sn, sn_bits);
  
  w.write_bit(!nacks.empty()); // E1
  
  if (sn_size == rlc_am_sn_size::size18bits) {
    w.write_bit(false); // R bit
  } else {
    w.write_bits(0, 7); // R bits
  }
  
  for (size_t i = 0; i < nacks.size(); i++) {
    const auto& nack = nacks[i];
    w.write_bits(nack.nack_sn, sn_bits);
    bool e1 = (i + 1 < nacks.size());
    w.write_bit(e1);
    w.write_bit(nack.has_so);
    w.write_bit(nack.has_nack_range);
    
    if (sn_size == rlc_am_sn_size::size18bits) {
      w.write_bits(0, 3);
    } else {
      w.write_bit(false);
    }
    
    if (nack.has_so) {
      w.write_bits(nack.so_start, 16);
      w.write_bits(nack.so_end, 16);
    }
    if (nack.has_nack_range) {
      w.write_bits(nack.nack_range, 8);
    }
  }
  
  return w.bytes_written();
}

bool rlc_am_status_pdu::trim(size_t max_size) {
  if (max_size < ((sn_size == rlc_am_sn_size::size12bits) ? 3 : 3)) {
    return false;
  }
  while (get_packed_size() > max_size) {
    if (nacks.empty()) return false;
    uint32_t removed_sn = nacks.back().nack_sn;
    nacks.pop_back();
    ack_sn = removed_sn;
    while (!nacks.empty() && nacks.back().nack_sn == removed_sn) {
      nacks.pop_back();
    }
  }
  return true;
}

} // namespace srsran
