#ifndef SRS_RLC_UM_PDU_H
#define SRS_RLC_UM_PDU_H

#include <cstdint>
#include <vector>
#include <array>
#include <fmt/format.h>
#include "srsran/adt/byte_buffer.h"
#include "srsran/srslog/srslog.h"


namespace srsran {

constexpr uint32_t INVALID_RLC_SN = 0xffffffff;

enum class rlc_um_sn_size {
    size6bits = 6,
    size12bits = 12
};

inline bool operator==(rlc_um_sn_size lhs, uint32_t rhs) {
    return static_cast<uint32_t>(lhs) == rhs;
}
inline bool operator==(uint32_t lhs, rlc_um_sn_size rhs) {
    return lhs == static_cast<uint32_t>(rhs);
}
inline bool operator!=(rlc_um_sn_size lhs, uint32_t rhs) {
    return !(lhs == rhs);
}
inline bool operator!=(uint32_t lhs, rlc_um_sn_size rhs) {
    return !(lhs == rhs);
}

enum class rlc_si_field {
    full_sdu = 0,
    first_segment = 1,
    last_segment = 2,
    middle_segment = 3
};

struct rlc_um_pdu_header {
    rlc_si_field si;
    uint32_t sn;
    uint32_t so;
    uint32_t sn_size; // 6 or 12 bits

    bool has_sn() const { return si != rlc_si_field::full_sdu; }
    bool has_so() const { return has_sn() && si != rlc_si_field::first_segment; }

    size_t get_packed_size() const {
        if (si == rlc_si_field::full_sdu) {
            return 1;
        }
        size_t size = (sn_size == (uint32_t)rlc_um_sn_size::size6bits) ? 1 : 2;
        if (has_so()) {
            size += 2;
        }
        return size;
    }
};

// Updated signatures to match test expectations:
// read: (buffer, sn_size, header_out)
// write: (buffer, header) -> returns packed size

inline size_t rlc_um_write_data_pdu_header(uint8_t* buffer, const rlc_um_pdu_header& header) {
    buffer[0] = (static_cast<uint8_t>(header.si) & 0x03) << 6;
    if (header.has_sn()) {
        if (header.sn_size == (uint32_t)rlc_um_sn_size::size6bits) {
            buffer[0] |= (header.sn & 0x3F);
            if (header.has_so()) {
                buffer[1] = (header.so >> 8) & 0xFF;
                buffer[2] = header.so & 0xFF;
            }
        } else { // 12-bit
            buffer[0] |= (header.sn >> 6) & 0x3F;
            buffer[1] = (header.sn & 0x3F);
            if (header.has_so()) {
                buffer[2] = (header.so >> 8) & 0xFF;
                buffer[3] = header.so & 0xFF;
            }
        }
    }
    return header.get_packed_size();
}

inline size_t rlc_um_write_data_pdu_header(byte_buffer& buffer, const rlc_um_pdu_header& header) {
    std::array<uint8_t, 8> temp;
    size_t size = rlc_um_write_data_pdu_header(temp.data(), header);
    (void)buffer.append(span<const uint8_t>(temp.data(), size));
    return size;
}

inline bool rlc_um_read_data_pdu_header(const uint8_t* buffer, rlc_um_sn_size sn_size, rlc_um_pdu_header* header) {
    if (!buffer || !header) return false;
    
    header->si = static_cast<rlc_si_field>((buffer[0] >> 6) & 0x03);
    header->sn_size = static_cast<uint32_t>(sn_size);
    
    if (header->si == rlc_si_field::full_sdu) {
        header->sn = 0;
        header->so = 0;
        return true;
    }
    
    if (sn_size == rlc_um_sn_size::size6bits) {
        header->sn = buffer[0] & 0x3F;
        if (header->si != rlc_si_field::first_segment) {
            header->so = (buffer[1] << 8) | buffer[2];
        } else {
            header->so = 0;
        }
    } else { // 12-bit
        // Try to read 12 bits: 6 from Oct 1, 6 from Oct 2
        uint32_t high = buffer[0] & 0x3F;
        uint32_t low = buffer[1] & 0x3F;
        header->sn = (high << 6) | low;
        if (header->si != rlc_si_field::first_segment) {
            header->so = (buffer[2] << 8) | buffer[3];
        } else {
            header->so = 0;
        }
    }
    return true;
}

inline bool rlc_um_read_data_pdu_header(const byte_buffer& buffer, rlc_um_sn_size sn_size, rlc_um_pdu_header* header) {
    if (buffer.length() == 0) return false;
    
    // Correct way to peek and check length
    std::array<uint8_t, 8> temp = {0};
    copy_segments(buffer, temp);
    
    rlc_um_pdu_header temp_hdr;
    bool ok = rlc_um_read_data_pdu_header(temp.data(), sn_size, &temp_hdr);
    if (!ok) return false;
    
    // For malformed PDU tests, we might need to be stricter.
    // If the PDU is segmented but the buffer is too short for the SN size.
    if (temp_hdr.si != rlc_si_field::full_sdu) {
        size_t min_len = (sn_size == rlc_um_sn_size::size6bits) ? 1 : 2;
        if (buffer.length() < min_len) return false;
    }

    if (buffer.length() < temp_hdr.get_packed_size()) {
        return false;
    }
    
    return rlc_um_read_data_pdu_header(temp.data(), sn_size, header);
}

template <size_t N>
inline size_t rlc_um_write_data_pdu_header(std::array<uint8_t, N>& buffer, const rlc_um_pdu_header& header) {
    return rlc_um_write_data_pdu_header(buffer.data(), header);
}

} // namespace srsran

template <>
struct fmt::formatter<srsran::rlc_um_pdu_header> {
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const srsran::rlc_um_pdu_header& h, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "UM Header(SI={}, SN={}, SO={})", 
            (int)h.si, h.sn, h.so);
    }
};

#endif // SRS_RLC_UM_PDU_H
