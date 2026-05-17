#ifndef SRS_RLC_SDU_QUEUE_LOCKFREE_H
#define SRS_RLC_SDU_QUEUE_LOCKFREE_H

#include <atomic>
#include <vector>
#include <memory>
#include <optional>
#include "srsran/adt/byte_buffer.h"

namespace srsran {

struct rlc_sdu {
    uint32_t pdcp_sn;
    byte_buffer data;
};

class rlc_sdu_queue_lockfree {
public:
    explicit rlc_sdu_queue_lockfree(size_t capacity = 1024)
        : capacity_(capacity), head_(0), tail_(0), total_bytes_(0), total_sdus_(0) {
        queue_.resize(capacity);
    }

    ~rlc_sdu_queue_lockfree() = default;

    bool push(rlc_sdu&& sdu) {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) % capacity_;

        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue full
        }

        total_bytes_.fetch_add(sdu.data.length(), std::memory_order_relaxed);
        total_sdus_.fetch_add(1, std::memory_order_relaxed);
        queue_[current_tail] = std::move(sdu);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    std::optional<rlc_sdu> pop() {
        size_t current_head = head_.load(std::memory_order_relaxed);
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt; // Queue empty
        }

        rlc_sdu sdu = std::move(queue_[current_head]);
        head_.store((current_head + 1) % capacity_, std::memory_order_release);
        
        total_bytes_.fetch_sub(sdu.data.length(), std::memory_order_relaxed);
        total_sdus_.fetch_sub(1, std::memory_order_relaxed);
        
        return sdu;
    }

    void discard_up_to_sn(uint32_t pdcp_sn) {
        while (true) {
            size_t current_head = head_.load(std::memory_order_relaxed);
            if (current_head == tail_.load(std::memory_order_acquire)) break;

            if (queue_[current_head].pdcp_sn <= pdcp_sn) {
                total_bytes_.fetch_sub(queue_[current_head].data.length(), std::memory_order_relaxed);
                total_sdus_.fetch_sub(1, std::memory_order_relaxed);
                head_.store((current_head + 1) % capacity_, std::memory_order_release);
            } else {
                break;
            }
        }
    }

    size_t size() const {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t = tail_.load(std::memory_order_acquire);
        return (t >= h) ? (t - h) : (capacity_ - h + t);
    }

    uint64_t total_bytes() const { return total_bytes_.load(std::memory_order_relaxed); }
    uint64_t total_sdus() const { return total_sdus_.load(std::memory_order_relaxed); }

private:
    const size_t capacity_;
    std::vector<rlc_sdu> queue_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
    std::atomic<uint64_t> total_bytes_;
    std::atomic<uint64_t> total_sdus_;
};

} // namespace srsran

#endif // SRS_RLC_SDU_QUEUE_LOCKFREE_H
