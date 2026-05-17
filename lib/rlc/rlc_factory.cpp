#include "rlc_factory.h"
#include "rlc_tx_tm_entity.h"
#include "rlc_rx_tm_entity.h"
#include "rlc_tx_um_entity.h"
#include "rlc_rx_um_entity.h"
#include "rlc_tx_am_entity.h"
#include "rlc_rx_am_entity.h"
#include "rlc_base_entity.h"

namespace srsran {


std::unique_ptr<rlc_base_entity> create_rlc_entity(const rlc_create_msg& msg) {
    auto entity = std::make_unique<rlc_base_entity>(msg.ue_index, msg.rb_id);
    
    switch (msg.config.mode) {
        case rlc_config::mode::TM: {
            entity->tx_entity = std::make_unique<rlc_tx_tm_entity>(
                msg.ue_index, msg.rb_id, *msg.metrics_collector);
            entity->rx_entity = std::make_unique<rlc_rx_tm_entity>(
                msg.ue_index, msg.rb_id, *msg.metrics_collector);
            break;
        }
        case rlc_config::mode::UM: {
            rlc_um_sn_size sn_size = (msg.config.sn_size == 6) ? 
                                    rlc_um_sn_size::size6bits : rlc_um_sn_size::size12bits;
            entity->tx_entity = std::make_unique<rlc_tx_um_entity>(
                msg.ue_index, msg.rb_id, sn_size, *msg.metrics_collector);
            entity->rx_entity = std::make_unique<rlc_rx_um_entity>(
                msg.ue_index, msg.rb_id, sn_size, *msg.metrics_collector);
            break;
        }
        case rlc_config::mode::AM: {
            entity->tx_entity = std::make_unique<rlc_tx_am_entity>(
                msg.ue_index, msg.rb_id, msg.config.sn_size, *msg.metrics_collector);
            entity->rx_entity = std::make_unique<rlc_rx_am_entity>(
                msg.ue_index, msg.rb_id, msg.config.sn_size, *msg.metrics_collector);
            
            // Interconnect AM TX and RX
            
            // In a full implementation, we would link the status provider/handler here
            // e.g., through a mediator or by passing pointers
            break;
        }
    }
    
    return entity;
}

} // namespace srsran
