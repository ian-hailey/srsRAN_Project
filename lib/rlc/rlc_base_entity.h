#ifndef SRS_RLC_BASE_ENTITY_H
#define SRS_RLC_BASE_ENTITY_H

#include <memory>
#include "rlc_tx_entity.h"
#include "rlc_rx_entity.h"

namespace srsran {

class rlc_entity {
public:
    virtual ~rlc_entity() = default;
};

class rlc_base_entity : public rlc_entity {
public:
    rlc_base_entity(uint32_t ue_idx, uint32_t rb_idx)
        : ue_index(ue_idx), rb_id(rb_idx) {}

    virtual ~rlc_base_entity() = default;

    virtual void stop() {}

    rlc_tx_upper_layer_data_interface* get_tx_upper_layer_data_interface() {
        return tx_entity.get();
    }

    rlc_tx_lower_layer_interface* get_tx_lower_layer_interface() {
        return tx_entity.get();
    }

    rlc_rx_lower_layer_interface* get_rx_lower_layer_interface() {
        return rx_entity.get();
    }

public:
    uint32_t ue_index;
    uint32_t rb_id;
    std::unique_ptr<rlc_tx_entity> tx_entity;
    std::unique_ptr<rlc_rx_entity> rx_entity;
};

} // namespace srsran

#endif // SRS_RLC_BASE_ENTITY_H
