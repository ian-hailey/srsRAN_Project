#ifndef SRS_RLC_AM_INTERCONNECT_H
#define SRS_RLC_AM_INTERCONNECT_H

#include <vector>
#include <cstdint>
#include "rlc_am_pdu.h"

namespace srsran {

class rlc_rx_am_status_provider {
public:
    virtual ~rlc_rx_am_status_provider() = default;
    virtual rlc_am_status_pdu get_status_pdu() = 0;
};

class rlc_tx_am_status_handler {
public:
    virtual ~rlc_tx_am_status_handler() = default;
    virtual void handle_status_pdu(const rlc_am_status_pdu& status_pdu) = 0;
};

class rlc_tx_am_status_notifier {
public:
    virtual ~rlc_tx_am_status_notifier() = default;
    virtual void notify_status_update() = 0;
};

} // namespace srsran

#endif // SRS_RLC_AM_INTERCONNECT_H
