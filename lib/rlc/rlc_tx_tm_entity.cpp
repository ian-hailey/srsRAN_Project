#include "rlc_tx_tm_entity.h"

namespace srsran {

rlc_tx_tm_entity::rlc_tx_tm_entity(uint32_t ue_idx, uint32_t rb_idx, rlc_bearer_metrics_collector& metrics_collector)
    : rlc_tx_entity(ue_idx, rb_idx), metrics_collector_(metrics_collector) {}

void rlc_tx_tm_entity::push_sdu(rlc_sdu&& sdu) {
    if (!sdu_queue_.push(std::move(sdu))) {
        // In a real implementation, we might signal buffer overflow to upper layers
    }
}

size_t rlc_tx_tm_entity::pull_pdu(byte_buffer& buffer) {
    auto sdu = sdu_queue_.pop();
    if (!sdu) {
        return 0;
    }

    size_t len = sdu->data.length();
    (void)buffer.append(sdu->data);

    current_metrics_.tx_bytes += len;
    current_metrics_.tx_pdus += 1;

    // Sync with global collector
    metrics_collector_.update_tx_metrics(current_metrics_);

    return len;
}

rlc_tx_entity::metrics rlc_tx_tm_entity::get_metrics() const {
    return current_metrics_;
}

void rlc_tx_tm_entity::stop() {
    while (sdu_queue_.pop()) {
        // drain queue
    }
}

} // namespace srsran
