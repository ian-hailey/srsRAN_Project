#ifndef SRS_RLC_AM_PDU_H
#define SRS_RLC_AM_PDU_H

#include <cstdint>
#include <vector>
#include <fmt/format.h>
#include "srsran/adt/byte_buffer.h"
#include "rlc_um_pdu.h"

namespace srsran {

enum class rlc_am_sn_size {
    size12bits = 12,
    size18bits = 18
};

inline bool operator==(rlc_am_sn_size lhs, uint32_t rhs) {
    return static_cast<uint32_t>(lhs) == rhs;
}
inline bool operator==(uint32_t lhs, rlc_am_sn_size rhs) {
    return lhs == static_cast<uint32_t>(rhs);
}
inline bool operator!=(rlc_am_sn_size lhs, uint32_t rhs) {
    return !(lhs == rhs);
}
inline bool operator!=(uint32_t lhs, rlc_am_sn_size rhs) {
    return !(lhs == rhs);
}

inline uint32_t to_number(rlc_am_sn_size sn_size) {
    return static_cast<uint32_t>(sn_size);
}

inline uint32_t cardinality(uint32_t sn_size) {
    return (sn_size == 12) ? 4096 : 262144;
}


struct rlc_am_pdu_header {
    uint8_t dc;      // Data/Control: 1 for Data, 0 for Control
    uint8_t p;       // Polling bit
    rlc_si_field si;    // Segmentation Info
    uint32_t sn;     // Sequence Number
    uint32_t so;     // Segment Offset
    uint32_t sn_size; // 12 or 18 bits

    bool is_data() const { return dc == 1; }
    bool is_control() const { return dc == 0; }
    bool has_sn() const { return si != rlc_si_field::full_sdu; }
    bool has_so() const { return has_sn() && si != rlc_si_field::first_segment; }

    size_t get_packed_size() const {
        size_t size = (sn_size == 12) ? 2 : 3;
        if (has_so()) {
            size += 2;
        }
        return size;
    }
};

struct rlc_am_status_nack {
    uint32_t nack_sn;
    bool has_so = false;
    uint32_t so_start = 0;
    uint32_t so_end = 0;
    bool has_nack_range = false;
    uint32_t nack_range = 0;

    static constexpr uint16_t so_end_of_sdu = 0xFFFF;

    bool operator==(const rlc_am_status_nack& other) const {
        return nack_sn == other.nack_sn &&
               has_so == other.has_so &&
               so_start == other.so_start &&
               so_end == other.so_end &&
               has_nack_range == other.has_nack_range &&
               nack_range == other.nack_range;
    }
    bool operator!=(const rlc_am_status_nack& other) const {
        return !(*this == other);
    }
};

class rlc_am_status_pdu {
public:
    rlc_am_status_pdu() : ack_sn(0), sn_size(12) {}
    rlc_am_status_pdu(uint32_t _ack_sn, uint32_t _sn_size) : ack_sn(_ack_sn), sn_size(_sn_size) {}
    rlc_am_status_pdu(rlc_am_sn_size _sn_size) : ack_sn(0), sn_size(static_cast<uint32_t>(_sn_size)) {}

    void push_nack(const rlc_am_status_nack& nack) { nacks.push_back(nack); }
    const std::vector<rlc_am_status_nack>& get_nacks() const { return nacks; }
    
    void reset() {
        ack_sn = 0;
        nacks.clear();
    }
    
    std::vector<uint8_t> pack() const;
    size_t pack(uint8_t* buffer) const;
    template <size_t N>
    size_t pack(std::array<uint8_t, N>& buffer) const {
        return pack(buffer.data());
    }
    bool unpack(const uint8_t* buffer, size_t size);
    bool unpack(const byte_buffer& buffer);

    bool is_control_pdu() const { return true; }
    static bool is_control_pdu(const byte_buffer& buffer);
    
    bool trim(size_t max_size);
    size_t get_packed_size() const;

    uint32_t ack_sn;
    uint32_t sn_size;
    std::vector<rlc_am_status_nack> nacks;
};

size_t rlc_am_write_data_pdu_header(const rlc_am_pdu_header& header, uint8_t* buffer);
template <size_t N>
size_t rlc_am_write_data_pdu_header(std::array<uint8_t, N>& buffer, const rlc_am_pdu_header& header) {
    return rlc_am_write_data_pdu_header(header, buffer.data());
}

bool rlc_am_read_data_pdu_header(rlc_am_pdu_header& header, const uint8_t* buffer, size_t size, uint32_t sn_size);
bool rlc_am_read_data_pdu_header(const byte_buffer& buffer, rlc_am_sn_size sn_size, rlc_am_pdu_header* header);

} // namespace srsran

template <>
struct fmt::formatter<srsran::rlc_am_sn_size> {
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const srsran::rlc_am_sn_size& s, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", static_cast<uint32_t>(s));
    }
};

template <>
struct fmt::formatter<srsran::rlc_am_pdu_header> {
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const srsran::rlc_am_pdu_header& h, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "AM Header(DC={}, P={}, SI={}, SN={}, SO={})", 
            h.dc, h.p, (int)h.si, h.sn, h.so);
    }
};

template <>
struct fmt::formatter<srsran::rlc_am_status_nack> {
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const srsran::rlc_am_status_nack& n, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "NACK(SN={}, SO={}:{}, Range={})", 
            n.nack_sn, n.has_so ? n.so_start : 0, n.has_so ? n.so_end : 0, n.has_nack_range ? n.nack_range : 0);
    }
};

template <>
struct fmt::formatter<srsran::rlc_am_status_pdu> {
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const srsran::rlc_am_status_pdu& p, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "AM Status(ACK_SN={}, NACKs={})", p.ack_sn, p.nacks.size());
    }
};

#endif // SRS_RLC_AM_PDU_H
