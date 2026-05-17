#include "rlc_rx_am_entity.h"
#include <algorithm>

namespace srsran {

rlc_rx_am_entity::rlc_rx_am_entity(uint32_t ue_idx, uint32_t rb_idx, 
                                 uint32_t sn_size, 
                                 rlc_bearer_metrics_collector& metrics_collector)
    : rlc_rx_entity(ue_idx, rb_idx), 
      metrics_collector_(metrics_collector), 
      sn_size_(sn_size) {}

uint32_t rlc_rx_am_entity::get_am_modulus() const {
    return (sn_size_ == 12) ? 4096 : 262144;
}

bool rlc_rx_am_entity::is_in_rx_window(uint32_t sn) const {
    uint32_t mod = get_am_modulus();
    uint32_t window_size = (sn_size_ == 12) ? 2048 : 131072;
    
    int32_t diff = static_cast<int32_t>(sn) - static_cast<int32_t>(rx_next_);
    if (diff < 0) diff += mod;
    
    return static_cast<uint32_t>(diff) < window_size;
}

void rlc_rx_am_entity::handle_pdu(const byte_buffer& buffer) {
    rlc_am_pdu_header header;
    std::vector<uint8_t> full_data(buffer.length());
    copy_segments(buffer, span<uint8_t>(full_data.data(), full_data.size()));
    if (!rlc_am_read_data_pdu_header(header, full_data.data(), full_data.size(), sn_size_)) {
        return;
    }

    if (header.dc == 0) {
        // Control PDU (STATUS) - should be handled by TX entity via interconnect
        return;
    }

    if (!is_in_rx_window(header.sn)) {
        return; // Discard
    }

    size_t header_size = (header.sn_size == 12) ? 2 : 3;
    if (header.has_so()) header_size += 2;

    auto& sdu_buf = reassembly_window_[header.sn];
    
    byte_buffer payload;
    (void)payload.append(span<const uint8_t>(full_data.data() + header_size, full_data.size() - header_size));
    
    size_t payload_len = payload.length();
    if (sdu_buf.segments.count(header.so)) {
        return; // Duplicate
    }

    sdu_buf.segments[header.so] = std::move(payload);
    sdu_buf.total_received_bytes += payload_len;

    if (header.si == rlc_si_field::last_segment) {
        sdu_buf.total_sdu_length = header.so + payload_len;
    }

    if (sdu_buf.total_sdu_length > 0 && sdu_buf.total_received_bytes >= sdu_buf.total_sdu_length) {
        // Simplified: deliver SDU
        metrics_collector_.update_rx_metrics(sdu_buf.total_sdu_length, 1);
        
        if (header.sn == rx_next_) {
            rx_next_ = (rx_next_ + 1) % get_am_modulus();
        }
        reassembly_window_.erase(header.sn);
    }

    if (header.sn >= rx_next_highest_) {
        rx_next_highest_ = header.sn + 1;
    }
}

rlc_am_status_pdu rlc_rx_am_entity::get_status_pdu() {
    rlc_am_status_pdu status;
    status.sn_size = sn_size_;
    status.ack_sn = rx_next_;

    // Generate NACKs for missing segments in the window
    for (uint32_t sn = rx_next_; sn < rx_next_highest_; ++sn) {
        if (reassembly_window_.find(sn) == reassembly_window_.end()) {
            rlc_am_status_nack nack;
            nack.nack_sn = sn;
            status.push_nack(nack);
        }
    }

    return status;
}

void rlc_rx_am_entity::on_reassembly_timer_expiry() {
    // Trigger status report logic
}

void rlc_rx_am_entity::on_status_prohibit_timer_expiry() {
    // Reset status prohibit flag
}

void rlc_rx_am_entity::stop() {
    reassembly_window_.clear();
}

} // namespace srsran
