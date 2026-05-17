#ifndef SRS_RLC_RX_ENTITY_H
#define SRS_RLC_RX_ENTITY_H

#include <memory>
#include <vector>
#include "rlc_am_pdu.h"
#include "rlc_um_pdu.h"
#include "srsran/srslog/srslog.h"
#include "srsran/adt/byte_buffer.h"

namespace srsran {

class rlc_rx_lower_layer_interface {
public:
    virtual ~rlc_rx_lower_layer_interface() = default;
    virtual void handle_pdu(const byte_buffer& buffer) = 0;
};

class rlc_rx_entity : public rlc_rx_lower_layer_interface {
public:
    rlc_rx_entity(uint32_t ue_idx, uint32_t rb_idx) 
        : ue_index(ue_idx), rb_id(rb_idx) {}
    
    virtual ~rlc_rx_entity() = default;

    virtual void stop() = 0;

protected:
    uint32_t ue_index;
    uint32_t rb_id;
};

} // namespace srsran

#endif // SRS_RLC_RX_ENTITY_H
