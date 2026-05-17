#ifndef SRS_RLC_TX_ENTITY_H
#define SRS_RLC_TX_ENTITY_H

#include <memory>
#include <vector>
#include "rlc_am_pdu.h"
#include "rlc_um_pdu.h"
#include "srsran/srslog/srslog.h"
#include "srsran/adt/byte_buffer.h"

namespace srsran {

// Forward declarations of interfaces
class rlc_tx_upper_layer_data_interface {
public:
    virtual ~rlc_tx_upper_layer_data_interface() = default;
};

class rlc_tx_lower_layer_interface {
public:
    virtual ~rlc_tx_lower_layer_interface() = default;
    virtual size_t pull_pdu(byte_buffer& buffer) = 0;
};

class rlc_tx_metrics {
public:
    virtual ~rlc_tx_metrics() = default;
    struct metrics {
        uint64_t tx_bytes = 0;
        uint64_t tx_pdus = 0;
    };
    virtual metrics get_metrics() const = 0;
};

class rlc_tx_entity : public rlc_tx_upper_layer_data_interface, 
                     public rlc_tx_lower_layer_interface, 
                     public rlc_tx_metrics {
public:
    rlc_tx_entity(uint32_t ue_idx, uint32_t rb_idx) 
        : ue_index(ue_idx), rb_id(rb_idx) {}
    
    virtual ~rlc_tx_entity() = default;

    virtual void stop() = 0;

protected:
    uint32_t ue_index;
    uint32_t rb_id;
};

} // namespace srsran

#endif // SRS_RLC_TX_ENTITY_H
