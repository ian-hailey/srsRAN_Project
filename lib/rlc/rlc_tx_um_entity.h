#ifndef SRS_RLC_TX_UM_ENTITY_H
#define SRS_RLC_TX_UM_ENTITY_H

#include "rlc_tx_entity.h"
#include "rlc_sdu_queue_lockfree.h"
#include "rlc_bearer_metrics_collector.h"
#include "rlc_um_pdu.h"

namespace srsran {

class rlc_tx_um_entity : public rlc_tx_entity {
public:
    rlc_tx_um_entity(uint32_t ue_index, uint32_t rb_id, 
                   rlc_um_sn_size sn_size, 
                   rlc_bearer_metrics_collector& metrics_collector);
    virtual ~rlc_tx_um_entity() = default;

    // From rlc_tx_upper_layer_data_interface
    void push_sdu(rlc_sdu&& sdu);

    // From rlc_tx_lower_layer_interface
    size_t pull_pdu(byte_buffer& buffer) override;

    // From rlc_tx_metrics
    metrics get_metrics() const override;

    void stop() override;

private:
    rlc_bearer_metrics_collector& metrics_collector_;
    rlc_sdu_queue_lockfree sdu_queue_;
    rlc_um_sn_size sn_size_;
    uint32_t tx_next_ = 0;
    uint32_t next_so_ = 0;
    metrics current_metrics_{};

    uint32_t get_um_modulus() const;
};

} // namespace srsran

#endif // SRS_RLC_TX_UM_ENTITY_H
