#include "rlc_am_pdu.h"
#include "rlc_um_pdu.h"
#include "srsran/adt/byte_buffer.h"
#include <algorithm>
#include <cstring>
#include <array>

namespace srsran {

// --- AM PDU Helpers ---

size_t rlc_am_write_data_pdu_header(const rlc_am_pdu_header& header, uint8_t* buffer) {
    if (!buffer) return 0;
    
    buffer[0] = (header.dc << 7) | (header.p << 6) | ((static_cast<uint8_t>(header.si) & 0x03) << 4);
    
    if (header.sn_size == 12) {
        buffer[0] |= (header.sn >> 8) & 0x0F;
        buffer[1] = header.sn & 0xFF;
        if (header.has_so()) {
            buffer[2] = (header.so >> 8) & 0xFF;
            buffer[3] = header.so & 0xFF;
        }
    } else { // 18-bit
        buffer[0] |= (header.sn >> 14) & 0x0F;
        buffer[1] = (header.sn >> 6) & 0xFF;
        buffer[2] = (header.sn & 0x3F) << 2;
        if (header.has_so()) {
            buffer[3] = (header.so >> 8) & 0xFF;
            buffer[4] = header.so & 0xFF;
        }
    }
    return header.get_packed_size();
}

bool rlc_am_read_data_pdu_header(rlc_am_pdu_header& header, const uint8_t* buffer, size_t size, uint32_t sn_size) {
    if (!buffer || size < 1) return false;
    
    header.dc = (buffer[0] >> 7) & 0x01;
    header.p = (buffer[0] >> 6) & 0x01;
    header.si = static_cast<rlc_si_field>((buffer[0] >> 4) & 0x03);
    header.sn_size = sn_size;
    
    if (sn_size == 12) {
        if (size < 2) return false;
        header.sn = ((buffer[0] & 0x0F) << 8) | buffer[1];
        if (header.has_so()) {
            if (size < 4) return false;
            header.so = (buffer[2] << 8) | buffer[3];
        } else {
            header.so = 0;
        }
    } else { // 18-bit
        if (size < 3) return false;
        header.sn = ((buffer[0] & 0x0F) << 14) | ((buffer[1] & 0xFF) << 6) | (buffer[2] >> 2);
        if (header.has_so()) {
            if (size < 5) return false;
            header.so = (buffer[3] << 8) | buffer[4];
        } else {
            header.so = 0;
        }
    }
    return true;
}


// --- AM Status PDU ---

size_t rlc_am_status_pdu::get_packed_size() const {
    size_t size = (sn_size == 12) ? 2 : 3;
    if (nacks.empty()) {
        size += (sn_size == 12) ? 1 : 0; // 12-bit needs trailing E1=0, 18-bit E1 is in ACK_SN
    } else {
        size += (sn_size == 12) ? 1 : 0; // 12-bit needs initial E1=1, 18-bit E1 is in ACK_SN
        for (const auto& nack : nacks) {
            size += (sn_size == 12) ? 2 : 3;
            if (nack.has_so) size += 4; 
            if (nack.has_nack_range) size += 1;
        }
        if (sn_size == 12) size += 1; // Trailing E1=0
    }
    return size;
}

std::vector<uint8_t> rlc_am_status_pdu::pack() const {
    std::vector<uint8_t> res(get_packed_size());
    pack(res.data());
    return res;
}

size_t rlc_am_status_pdu::pack(uint8_t* buffer) const {
    if (!buffer) return 0;
    uint8_t* curr = buffer;
    
    uint8_t first_byte = 0x00; 
    if (sn_size == 12) {
        *curr++ = first_byte | ((ack_sn >> 8) & 0x0F);
        *curr++ = ack_sn & 0xFF;
    } else { // 18-bit
        *curr++ = first_byte | ((ack_sn >> 14) & 0x0F);
        *curr++ = (ack_sn >> 6) & 0xFF;
        uint8_t last_byte = (ack_sn & 0x3F) << 2;
        if (!nacks.empty()) last_byte |= 0x02; // E1 = 1
        *curr++ = last_byte;
    }

    if (sn_size == 12) {
        if (nacks.empty()) {
            *curr++ = 0x00; // E1 = 0
        } else {
            for (const auto& nack : nacks) {
                *curr++ = 0x80; // E1 = 1
                *curr++ = (nack.nack_sn >> 4) & 0xFF;
                uint8_t low = (nack.nack_sn & 0x0F) << 4;
                if (nack.has_so) low |= 0x02;
                if (nack.has_nack_range) low |= 0x01;
                *curr++ = low;
                if (nack.has_so) {
                    *curr++ = (nack.so_start >> 8) & 0xFF;
                    *curr++ = nack.so_start & 0xFF;
                    *curr++ = (nack.so_end >> 8) & 0xFF;
                    *curr++ = nack.so_end & 0xFF;
                }
                if (nack.has_nack_range) {
                    *curr++ = nack.nack_range & 0xFF;
                }
            }
            *curr++ = 0x00; // Trailing E1 = 0
        }
    } else { // 18-bit
        for (size_t i = 0; i < nacks.size(); ++i) {
            const auto& nack = nacks[i];
            *curr++ = (nack.nack_sn >> 14) & 0x0F;
            *curr++ = (nack.nack_sn >> 6) & 0xFF;
            uint8_t low = (nack.nack_sn & 0x3F) << 2;
            if (nack.has_so) low |= 0x02;
            if (nack.has_nack_range) low |= 0x01;
            if (i + 1 < nacks.size()) low |= 0x02; // E1 = 1 for next
            *curr++ = low;
            if (nack.has_so) {
                *curr++ = (nack.so_start >> 8) & 0xFF;
                *curr++ = nack.so_start & 0xFF;
                *curr++ = (nack.so_end >> 8) & 0xFF;
                *curr++ = nack.so_end & 0xFF;
            }
            if (nack.has_nack_range) {
                *curr++ = nack.nack_range & 0xFF;
            }
        }
    }
    return curr - buffer;
}

bool rlc_am_status_pdu::unpack(const uint8_t* buffer, size_t size) {
    if (!buffer || size < 2) return false;
    
    const uint8_t* curr = buffer;
    if (sn_size == 12) {
        ack_sn = ((curr[0] & 0x0F) << 8) | curr[1];
        curr += 2;
    } else {
        if (size < 3) return false;
        ack_sn = ((curr[0] & 0x0F) << 14) | ((curr[1] & 0xFF) << 6) | (curr[2] >> 2);
        curr += 3;
    }

    while (curr < buffer + size) {
        if (sn_size == 12) {
            if (curr >= buffer + size) break;
            bool e1 = (*curr & 0x80) != 0;
            curr++;
            if (!e1) break;

            if (curr + 1 >= buffer + size) return false;
            rlc_am_status_nack nack;
            nack.nack_sn = (curr[0] << 4) | (curr[1] >> 4);
            uint8_t low = curr[1] & 0x0F;
            nack.has_so = (low & 0x02) != 0;
            nack.has_nack_range = (low & 0x01) != 0;
            curr += 2;

            if (nack.has_so) {
                if (curr + 3 >= buffer + size) return false;
                nack.so_start = (curr[0] << 8) | curr[1];
                nack.so_end = (curr[2] << 8) | curr[3];
                curr += 4;
            }
            if (nack.has_nack_range) {
                if (curr >= buffer + size) return false;
                nack.nack_range = curr[0];
                curr += 1;
            }
            nacks.push_back(nack);
        } else {
            if (curr == buffer + 3) {
                if (!(buffer[2] & 0x02)) break;
            }

            if (curr + 2 >= buffer + size) return false;
            rlc_am_status_nack nack;
            nack.nack_sn = ((curr[0] & 0x0F) << 14) | ((curr[1] & 0xFF) << 6) | (curr[2] >> 2);
            uint8_t low = curr[2] & 0x03;
            nack.has_so = (low & 0x02) != 0;
            nack.has_nack_range = (low & 0x01) != 0;
            
            bool next_e1 = (curr[2] & 0x02) != 0;
            curr += 3;

            if (nack.has_so) {
                if (curr + 3 >= buffer + size) return false;
                nack.so_start = (curr[0] << 8) | curr[1];
                nack.so_end = (curr[2] << 8) | curr[3];
                curr += 4;
            }
            if (nack.has_nack_range) {
                if (curr >= buffer + size) return false;
                nack.nack_range = curr[0];
                curr += 1;
            }
            nacks.push_back(nack);
            if (!next_e1) break;
        }
    }
    return true;
}

bool rlc_am_status_pdu::unpack(const byte_buffer& buffer) {
    std::array<uint8_t, 64> temp;
    size_t copied = copy_segments(buffer, temp);
    return unpack(temp.data(), copied);
}

bool rlc_am_status_pdu::is_control_pdu(const byte_buffer& buffer) {
    if (buffer.empty()) return false;
    return (buffer[0] & 0x80) == 0;
}

bool rlc_am_status_pdu::trim(size_t max_size) {
    bool trimmed = false;
    while (!nacks.empty() && get_packed_size() > max_size) {
        nacks.pop_back();
        trimmed = true;
    }
    return trimmed;
}

bool rlc_am_read_data_pdu_header(const byte_buffer& buffer, rlc_am_sn_size sn_size, rlc_am_pdu_header* header) {
    if (!header) return false;
    std::array<uint8_t, 16> temp;
    size_t copied = copy_segments(buffer, temp);
    return rlc_am_read_data_pdu_header(*header, temp.data(), copied, static_cast<uint32_t>(sn_size));
}

} // namespace srsran