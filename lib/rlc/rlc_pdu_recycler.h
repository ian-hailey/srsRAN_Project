#ifndef SRS_RLC_PDU_RECYCLER_H
#define SRS_RLC_PDU_RECYCLER_H

#include <vector>
#include <mutex>
#include <memory>
#include "srsran/adt/byte_buffer.h"

namespace srsran {

class rlc_pdu_recycler {
public:
    rlc_pdu_recycler() = default;
    ~rlc_pdu_recycler() {
        flush();
    }

    void recycle(byte_buffer&& buffer) {
        std::lock_guard<std::mutex> lock(mutex_);
        recycle_bin_.push_back(std::move(buffer));
    }

    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        recycle_bin_.clear();
    }

private:
    std::mutex mutex_;
    std::vector<byte_buffer> recycle_bin_;
};

} // namespace srsran

#endif // SRS_RLC_PDU_RECYCLER_H
