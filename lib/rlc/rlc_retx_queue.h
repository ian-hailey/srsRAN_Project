#ifndef SRS_RLC_RETX_QUEUE_H
#define SRS_RLC_RETX_QUEUE_H

#include <vector>
#include <memory>
#include <optional>
#include <cstdint>
#include "rlc_am_pdu.h"
#include "srsran/adt/byte_buffer.h"

namespace srsran {

struct rlc_retx_element {
    uint32_t sn;
    uint32_t so;
    uint32_t length;
    bool is_valid = false;
    uint32_t retx_count = 0;
    byte_buffer data;
};

class rlc_retx_queue {
public:
    explicit rlc_retx_queue(size_t capacity = 4096)
        : capacity_(capacity), head_(0), tail_(0) {
        elements_.resize(capacity);
    }

    ~rlc_retx_queue() = default;

    bool push(rlc_retx_element&& element) {
        size_t current_tail = tail_;
        size_t next_tail = (current_tail + 1) % capacity_;

        if (next_tail == head_) {
            return false; // Queue full
        }

        elements_[current_tail] = std::move(element);
        elements_[current_tail].is_valid = true;
        tail_ = next_tail;
        return true;
    }

    std::optional<rlc_retx_element> pop() {
        if (head_ == tail_) {
            return std::nullopt; // Queue empty
        }

        size_t current_head = head_;
        head_ = (head_ + 1) % capacity_;

        if (!elements_[current_head].is_valid) {
            // Skip zombie elements
            return pop(); 
        }

        rlc_retx_element element = std::move(elements_[current_head]);
        elements_[current_head].is_valid = false;
        return element;
    }

    void mark_invalid(uint32_t sn) {
        for (size_t i = 0; i < capacity_; ++i) {
            if (elements_[i].is_valid && elements_[i].sn == sn) {
                elements_[i].is_valid = false;
            }
        }
    }

    void clear() {
        head_ = 0;
        tail_ = 0;
        for (auto& el : elements_) {
            el.is_valid = false;
        }
    }

    size_t size() const {
        if (tail_ >= head_) return tail_ - head_;
        return capacity_ - head_ + tail_;
    }

private:
    const size_t capacity_;
    std::vector<rlc_retx_element> elements_;
    size_t head_;
    size_t tail_;
};

} // namespace srsran

#endif // SRS_RLC_RETX_QUEUE_H
