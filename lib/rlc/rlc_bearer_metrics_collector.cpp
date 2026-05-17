#include "rlc_bearer_metrics_collector.h"
#include <algorithm>

namespace srsran {

rlc_bearer_metrics_collector::rlc_bearer_metrics_collector() {
    buffers_.fill(rlc_metrics{});
}

void rlc_bearer_metrics_collector::update_tx_metrics(const rlc_tx_entity::metrics& tx_m) {
    std::lock_guard<std::mutex> lock(update_mutex_);
    uint8_t w_idx = write_idx_.load(std::memory_order_relaxed);
    
    buffers_[w_idx].tx_bytes = tx_m.tx_bytes;
    buffers_[w_idx].tx_pdus = tx_m.tx_pdus;
    
    // Swap write and dirty indices
    uint8_t d_idx = dirty_idx_.exchange(w_idx, std::memory_order_release);
    write_idx_.store(d_idx, std::memory_order_relaxed);
}

void rlc_bearer_metrics_collector::update_rx_metrics(uint64_t rx_bytes, uint64_t rx_pdus) {
    std::lock_guard<std::mutex> lock(update_mutex_);
    uint8_t w_idx = write_idx_.load(std::memory_order_relaxed);
    
    buffers_[w_idx].rx_bytes += rx_bytes;
    buffers_[w_idx].rx_pdus += rx_pdus;
    
    uint8_t d_idx = dirty_idx_.exchange(w_idx, std::memory_order_release);
    write_idx_.store(d_idx, std::memory_order_relaxed);
}

void rlc_bearer_metrics_collector::increment_retx() {
    std::lock_guard<std::mutex> lock(update_mutex_);
    uint8_t w_idx = write_idx_.load(std::memory_order_relaxed);
    
    buffers_[w_idx].retx_count++;
    
    uint8_t d_idx = dirty_idx_.exchange(w_idx, std::memory_order_release);
    write_idx_.store(d_idx, std::memory_order_relaxed);
}

rlc_metrics rlc_bearer_metrics_collector::collect() {
    // Swap read and dirty indices to get latest snapshot
    uint8_t d_idx = dirty_idx_.exchange(read_idx_.load(std::memory_order_relaxed), std::memory_order_acquire);
    read_idx_.store(d_idx, std::memory_order_relaxed);
    
    return buffers_[d_idx];
}

} // namespace srsran
