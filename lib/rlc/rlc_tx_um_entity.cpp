#include "rlc_tx_um_entity.h"

namespace srsran {

rlc_tx_um_entity::rlc_tx_um_entity(uint32_t ue_idx, uint32_t rb_idx, 
                                 rlc_um_sn_size sn_size, 
                                 rlc_bearer_metrics_collector& metrics_collector)
    : rlc_tx_entity(ue_idx, rb_idx), 
      metrics_collector_(metrics_collector), 
      sn_size_(sn_size) {}

void rlc_tx_um_entity::push_sdu(rlc_sdu&& sdu) {
    if (!sdu_queue_.push(std::move(sdu))) {
        // Buffer overflow
    }
}

uint32_t rlc_tx_um_entity::get_um_modulus() const {
    return (sn_size_ == rlc_um_sn_size::size6bits) ? 64 : 4096;
}

size_t rlc_tx_um_entity::pull_pdu(byte_buffer& buffer) {
    // This implementation assumes the buffer passed is the target for one PDU.
    // If an SDU is being segmented, we need to keep track of the current SDU.
    // Since rlc_sdu_queue_lockfree doesn't allow peeking, we must manage a 'current_sdu'.
    
    static thread_local std::optional<rlc_sdu> current_sdu;
    static thread_local uint32_t current_so = 0;

    if (!current_sdu) {
        current_sdu = sdu_queue_.pop();
        if (!current_sdu) return 0;
        current_so = 0;
    }

    const byte_buffer& data = current_sdu->data;
    size_t remaining = data.length() - current_so;
    
    // In a real scenario, we'd know the MAC grant size. 
    // Here we use the provided buffer's capacity or a default max PDU size.
    size_t max_pdu_size = 1500; // Example max size
    size_t header_size = 0;
    
    rlc_um_pdu_header header;
    header.sn_size = static_cast<uint32_t>(sn_size_);
    header.sn = tx_next_;
    header.so = current_so;

    if (current_so == 0) {
        header.si = (remaining <= (max_pdu_size - 4)) ? rlc_si_field::full_sdu : rlc_si_field::first_segment;
    } else if (remaining <= (max_pdu_size - 4)) {
        header.si = rlc_si_field::last_segment;
    } else {
        header.si = rlc_si_field::middle_segment;
    }

    header_size = rlc_um_write_data_pdu_header(nullptr, header); // Simplified size check
    // Actual size calculation would involve buffer offset
    
    size_t payload_len = std::min(remaining, max_pdu_size - header_size);
    
    // Pack header and data into the provided buffer
    std::array<uint8_t, 8> header_buf;
    size_t packed_header_len = rlc_um_write_data_pdu_header(header_buf.data(), header);
    
    (void)buffer.append(span<const uint8_t>(header_buf.data(), packed_header_len));
    
    std::vector<uint8_t> full_data(data.length());
    copy_segments(data, span<uint8_t>(full_data.data(), full_data.size()));
    (void)buffer.append(span<const uint8_t>(full_data.data() + current_so, payload_len));

    current_so += payload_len;
    
    if (current_so >= data.length()) {
        // SDU fully transmitted
        tx_next_ = (tx_next_ + 1) % get_um_modulus();
        current_sdu.reset();
        current_so = 0;
    }

    current_metrics_.tx_bytes += (packed_header_len + payload_len);
    current_metrics_.tx_pdus += 1;
    metrics_collector_.update_tx_metrics(current_metrics_);

    return packed_header_len + payload_len;
}

rlc_tx_entity::metrics rlc_tx_um_entity::get_metrics() const {
    return current_metrics_;
}

void rlc_tx_um_entity::stop() {
    while (sdu_queue_.pop()) {}
}

} // namespace srsran
