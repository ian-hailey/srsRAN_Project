#ifndef SRS_RLC_FACTORY_H
#define SRS_RLC_FACTORY_H

#include <memory>
#include "rlc_base_entity.h"
#include "rlc_bearer_metrics_collector.h"

namespace srsran {

struct rlc_config {
    enum class mode { TM, UM, AM };
    mode mode;
    uint32_t sn_size; // 6 for UM, 12 for UM/AM, 18 for AM
};

struct rlc_create_msg {
    uint32_t ue_index;
    uint32_t rb_id;
    rlc_config config;
    rlc_bearer_metrics_collector* metrics_collector;
};

std::unique_ptr<rlc_base_entity> create_rlc_entity(const rlc_create_msg& msg);

} // namespace srsran

#endif // SRS_RLC_FACTORY_H
