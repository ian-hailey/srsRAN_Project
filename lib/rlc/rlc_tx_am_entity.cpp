#include "rlc_tx_am_entity.h"
#include <algorithm>
#include <array>

namespace srsran {

rlc_tx_am_entity::rlc_tx_am_entity(uint32_t ue_idx, uint32_t rb_idx, 
                                 uint32_t sn_size, 
                                 rlc_bearer_metrics_collector& metrics_collector)
    : rlc_tx_entity(ue_idx, rb_idx), 
      metrics_collector_(metrics_collector), 
      sn_size_(sn_size) {}

void rlc_tx_am_entity::push_sdu(rlc_sdu&& sdu) {
    if (!sdu_queue_.push(std::move(sdu))) {
        // Buffer overflow
    }
}

uint32_t rlc_tx_am_entity::get_am_modulus() const {
    return (sn_size_ == 12) ? 4096 : 262144;
}

bool rlc_tx_am_entity::is_in_tx_window(uint32_t sn) const {
    uint32_t mod = get_am_modulus();
    uint32_t window_size = (sn_size_ == 12) ? 2048 : 131072;
    
    int32_t diff = static_cast<int32_t>(sn) - static_cast<int32_t>(tx_next_ack_);
    if (diff < 0) diff += mod;
    
    return static_cast<uint32_t>(diff) < window_size;
}

void rlc_tx_am_entity::handle_status_pdu(const rlc_am_status_pdu& status_pdu) {
    // Update TX_Next_Ack
    uint32_t ack_sn = status_pdu.ack_sn;
    tx_next_ack_ = ack_sn; // Simplified: should be the smallest non-acked SN

    // Process NACKs
    for (const auto& nack : status_pdu.nacks) {
        if (is_in_tx_window(nack.nack_sn)) {
            rlc_retx_element retx_el;
            retx_el.sn = nack.nack_sn;
            retx_el.so = nack.so_start;
            // In a real implementation, we'd retrieve the actual data from a buffer
            retx_queue_.push(std::move(retx_el));
        }
    }
}

size_t rlc_tx_am_entity::pull_pdu(byte_buffer& buffer) {
    // 1. Try retransmissions first
    auto retx_sdu = retx_queue_.pop();
    if (retx_sdu) {
        rlc_am_pdu_header header;
        header.dc = 1;
        header.p = 0; // Polling logic would be applied here
        header.si = rlc_si_field::middle_segment; // Simplified
        header.sn_size = sn_size_;
        header.sn = retx_sdu->sn;
        header.so = retx_sdu->so;
        
        std::array<uint8_t, 8> header_buf;
        rlc_am_write_data_pdu_header(header, header_buf.data());
        
        // Here we would append actual data from the retx_element
        (void)buffer.append(span<const uint8_t>(header_buf.data(), 3)); 
        return 3; 
    }

    // 2. Process new SDUs
    auto sdu = sdu_queue_.pop();
    if (!sdu) return 0;

    // Simplified segmentation and packing
    rlc_am_pdu_header header;
    header.dc = 1;
    header.p = 0;
    header.si = rlc_si_field::full_sdu;
    header.sn_size = sn_size_;
    header.sn = tx_next_;
    header.so = 0;

    std::array<uint8_t, 8> header_buf;
    rlc_am_write_data_pdu_header(header, header_buf.data());
    
    (void)buffer.append(span<const uint8_t>(header_buf.data(), 3));
    (void)buffer.append(sdu->data);

    tx_next_ = (tx_next_ + 1) % get_am_modulus();
    
    current_metrics_.tx_bytes += (3 + sdu->data.length());
    current_metrics_.tx_pdus += 1;
    metrics_collector_.update_tx_metrics(current_metrics_);

    return 3 + sdu->data.length();
}

rlc_tx_entity::metrics rlc_tx_am_entity::get_metrics() const {
    return current_metrics_;
}

void rlc_tx_am_entity::stop() {
    while (sdu_queue_.pop()) {}
    retx_queue_.clear();
}

} // namespace srsran
