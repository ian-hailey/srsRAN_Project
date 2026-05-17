#ifndef SRS_RLC_BEARER_METRICS_COLLECTOR_H
#define SRS_RLC_BEARER_METRICS_COLLECTOR_H

#include <atomic>
#include <mutex>
#include <array>
#include "rlc_tx_entity.h"
#include "rlc_rx_entity.h"

namespace srsran {

struct rlc_metrics {
    uint64_t tx_bytes = 0;
    uint64_t tx_pdus = 0;
    uint64_t rx_bytes = 0;
    uint64_t rx_pdus = 0;
    uint64_t retx_count = 0;
};

class rlc_bearer_metrics_collector {
public:
    rlc_bearer_metrics_collector();
    ~rlc_bearer_metrics_collector() = default;

    void update_tx_metrics(const rlc_tx_entity::metrics& tx_m);
    void update_rx_metrics(uint64_t rx_bytes, uint64_t rx_pdus);
    void increment_retx();

    rlc_metrics collect();

private:
    // Triple buffering for lock-free read of aggregated metrics
    std::array<rlc_metrics, 3> buffers_;
    std::atomic<uint8_t> write_idx_{0};
    std::atomic<uint8_t> read_idx_{1};
    std::atomic<uint8_t> dirty_idx_{2};

    std::mutex update_mutex_;
};

} // namespace srsran

#endif // SRS_RLC_BEARER_METRICS_COLLECTOR_H
