#include "rlc_rx_um_entity.h"

namespace srsran {

rlc_rx_um_entity::rlc_rx_um_entity(uint32_t ue_idx, uint32_t rb_idx, 
                                 rlc_um_sn_size sn_size, 
                                 rlc_bearer_metrics_collector& metrics_collector)
    : rlc_rx_entity(ue_idx, rb_idx), 
      metrics_collector_(metrics_collector), 
      sn_size_(sn_size) {}

uint32_t rlc_rx_um_entity::get_um_modulus() const {
    return (sn_size_ == rlc_um_sn_size::size6bits) ? 64 : 4096;
}

bool rlc_rx_um_entity::is_in_reassembly_window(uint32_t sn) const {
    uint32_t mod = get_um_modulus();
    uint32_t window_size = (sn_size_ == rlc_um_sn_size::size6bits) ? 32 : 2048;
    
    // Modulo arithmetic for window check
    int32_t diff = static_cast<int32_t>(sn) - static_cast<int32_t>(rx_next_highest_);
    if (diff < 0) diff += mod;
    
    return (mod - diff) <= window_size;
}

void rlc_rx_um_entity::handle_pdu(const byte_buffer& buffer) {
    rlc_um_pdu_header header;
    if (!rlc_um_read_data_pdu_header(buffer, sn_size_, &header)) {
        return;
    }

    size_t header_size = header.get_packed_size();
    
    if (header.sn == INVALID_RLC_SN) {
        // Full SDU, deliver immediately
        metrics_collector_.update_rx_metrics(buffer.length() - header_size, 1);
        return;
    }

    if (!is_in_reassembly_window(header.sn)) {
        return; // Discard
    }

    // Place in reassembly window
    auto& sdu_buf = reassembly_window_[header.sn];
    
    byte_buffer payload;
    std::vector<uint8_t> full_data(buffer.length());
    copy_segments(buffer, span<uint8_t>(full_data.data(), full_data.size()));
    (void)payload.append(span<const uint8_t>(full_data.data() + header_size, full_data.size() - header_size));
    
    size_t payload_len = payload.length();
    sdu_buf.segments[header.so] = std::move(payload);
    sdu_buf.total_received_bytes += payload_len;

    if (header.si == rlc_si_field::last_segment) {
        sdu_buf.total_sdu_length = header.so + payload_len;
    }

    // Check if SDU is complete
    if (sdu_buf.total_sdu_length > 0 && sdu_buf.total_received_bytes >= sdu_buf.total_sdu_length) {
        // Simple check: in reality, we'd verify no gaps in offsets
        sdu_buf.is_complete = true;
        
        // Deliver SDU
        metrics_collector_.update_rx_metrics(sdu_buf.total_sdu_length, 1);
        
        // Update RX_Next_Reassembly
        if (header.sn == rx_next_reassembly_) {
            rx_next_reassembly_++; // Simplified increment
        }
        
        reassembly_window_.erase(header.sn);
    }

    // Update RX_Next_Highest
    if (header.sn >= rx_next_highest_) {
        rx_next_highest_ = header.sn + 1;
    }

    // Timer logic would go here (start/stop t-Reassembly)
}

void rlc_rx_um_entity::on_reassembly_timer_expiry() {
    // Discard segments older than current window
    auto it = reassembly_window_.begin();
    while (it != reassembly_window_.end()) {
        if (!is_in_reassembly_window(it->first)) {
            it = reassembly_window_.erase(it);
        } else {
            ++it;
        }
    }
}

void rlc_rx_um_entity::stop() {
    reassembly_window_.clear();
}

} // namespace srsran
