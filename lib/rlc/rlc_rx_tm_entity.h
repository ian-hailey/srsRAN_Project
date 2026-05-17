#ifndef SRS_RLC_RX_TM_ENTITY_H
#define SRS_RLC_RX_TM_ENTITY_H

#include "rlc_rx_entity.h"
#include "rlc_bearer_metrics_collector.h"

namespace srsran {

class rlc_rx_tm_entity : public rlc_rx_entity {
public:
    rlc_rx_tm_entity(uint32_t ue_index, uint32_t rb_id, rlc_bearer_metrics_collector& metrics_collector);
    virtual ~rlc_rx_tm_entity() = default;

    // From rlc_rx_lower_layer_interface
    void handle_pdu(const byte_buffer& buffer) override;

    void stop() override;

private:
    rlc_bearer_metrics_collector& metrics_collector_;
};

} // namespace srsran

#endif // SRS_RLC_RX_TM_ENTITY_H
