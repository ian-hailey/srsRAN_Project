#include "rlc_rx_tm_entity.h"

namespace srsran {

rlc_rx_tm_entity::rlc_rx_tm_entity(uint32_t ue_idx, uint32_t rb_idx, rlc_bearer_metrics_collector& metrics_collector)
    : rlc_rx_entity(ue_idx, rb_idx), metrics_collector_(metrics_collector) {}

void rlc_rx_tm_entity::handle_pdu(const byte_buffer& buffer) {
    size_t len = buffer.length();
    
    // In TM, the PDU is the SDU. Deliver it directly to upper layer.
    // (The actual delivery mechanism would be defined in the upper layer interface)
    
    // Update metrics
    metrics_collector_.update_rx_metrics(len, 1);
}

void rlc_rx_tm_entity::stop() {
    // No state to clear for TM RX
}

} // namespace srsran
