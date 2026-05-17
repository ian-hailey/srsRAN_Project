#ifndef SRS_RLC_TX_AM_ENTITY_H
#define SRS_RLC_TX_AM_ENTITY_H

#include "rlc_tx_entity.h"
#include "rlc_sdu_queue_lockfree.h"
#include "rlc_retx_queue.h"
#include "rlc_bearer_metrics_collector.h"
#include "rlc_am_pdu.h"
#include "rlc_am_interconnect.h"

namespace srsran {

class rlc_tx_am_entity : public rlc_tx_entity, public rlc_tx_am_status_handler {
public:
    rlc_tx_am_entity(uint32_t ue_index, uint32_t rb_id, 
                   uint32_t sn_size, 
                   rlc_bearer_metrics_collector& metrics_collector);
    virtual ~rlc_tx_am_entity() = default;

    // From rlc_tx_upper_layer_data_interface
    void push_sdu(rlc_sdu&& sdu);

    // From rlc_tx_lower_layer_interface
    size_t pull_pdu(byte_buffer& buffer) override;

    // From rlc_tx_metrics
    metrics get_metrics() const override;

    // From rlc_tx_am_status_handler
    void handle_status_pdu(const rlc_am_status_pdu& status_pdu) override;

    void stop() override;

private:
    rlc_bearer_metrics_collector& metrics_collector_;
    rlc_sdu_queue_lockfree sdu_queue_;
    rlc_retx_queue retx_queue_;
    
    uint32_t sn_size_;
    uint32_t tx_next_ = 0;
    uint32_t tx_next_ack_ = 0;
    uint32_t poll_sn_ = 0;
    uint32_t pdu_without_poll_ = 0;
    uint32_t byte_without_poll_ = 0;
    uint32_t next_so_ = 0;
    
    metrics current_metrics_{};

    uint32_t get_am_modulus() const;
    bool is_in_tx_window(uint32_t sn) const;
    void process_retransmissions(byte_buffer& buffer, size_t& size);
};

} // namespace srsran

#endif // SRS_RLC_TX_AM_ENTITY_H
