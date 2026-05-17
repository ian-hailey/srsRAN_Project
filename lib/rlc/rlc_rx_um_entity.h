#ifndef SRS_RLC_RX_UM_ENTITY_H
#define SRS_RLC_RX_UM_ENTITY_H

#include "rlc_rx_entity.h"
#include "rlc_um_pdu.h"
#include "rlc_bearer_metrics_collector.h"
#include <map>

namespace srsran {

class rlc_rx_um_entity : public rlc_rx_entity {
public:
    rlc_rx_um_entity(uint32_t ue_index, uint32_t rb_id, 
                    rlc_um_sn_size sn_size, 
                    rlc_bearer_metrics_collector& metrics_collector);
    virtual ~rlc_rx_um_entity() = default;

    // From rlc_rx_lower_layer_interface
    void handle_pdu(const byte_buffer& buffer) override;

    void stop() override;

    // Timer callback for t-Reassembly
    void on_reassembly_timer_expiry();

private:
    rlc_bearer_metrics_collector& metrics_collector_;
    rlc_um_sn_size sn_size_;
    uint32_t rx_next_reassembly_ = 0;
    uint32_t rx_next_highest_ = 0;
    uint32_t rx_timer_trigger_ = 0;

    // SDU reassembly buffer: SN -> map of offset to data
    struct sdu_reassembly_buffer {
        std::map<uint32_t, byte_buffer> segments;
        size_t total_received_bytes = 0;
        size_t total_sdu_length = 0;
        bool is_complete = false;
    };
    std::map<uint32_t, sdu_reassembly_buffer> reassembly_window_;

    uint32_t get_um_modulus() const;
    bool is_in_reassembly_window(uint32_t sn) const;
};

} // namespace srsran

#endif // SRS_RLC_RX_UM_ENTITY_H
