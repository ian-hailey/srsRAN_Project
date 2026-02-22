# RLC Implementation Step 3: AM Mode Transmit & Retransmission (Technical Specification)

Implement the Transmit logic for RLC Acknowledged Mode (AM), focusing on state management, polling, and retransmission mechanisms.

## 1. RLC TX AM Entity (`rlc_tx_am_entity`) - Transmit logic
### Key Responsibilities:
- **State Management**: Maintain `tx_next_ack` (lower window edge), `tx_next` (next SN), `poll_sn`, `pdu_without_poll`, and `byte_without_poll`.
- **Implement `pull_pdu(span<uint8_t> rlc_pdu_buf)`**:
  - **Priority 1: Status Reporting**: If `status_provider->status_report_required()` is true, build and send a Status PDU. Handle trimming if the grant is too small.
  - **Priority 2: Retransmission**: If `retx_queue` is not empty, build a retransmission PDU using `build_retx_pdu`.
  - **Priority 3: SDU Segmentation**: If `sn_under_segmentation` is valid, continue segmenting the current SDU.
  - **Priority 4: New Data**: Pull a new SDU from `sdu_queue` and build a new PDU.
- **Implement `get_polling_bit(uint32_t sn, bool is_retx, uint32_t payload_size)`**:
  - Implement 3GPP polling criteria: `pollPDU`, `pollByte`, empty buffers, or window stall.
  - Start/Restart `poll_retransmit_timer` (t-PollRetransmit) when polling.
- **Implement `on_expired_poll_retransmit_timer()`**:
  - Trigger retransmission of the highest SN submitted or any unacknowledged SDU if buffers are empty or window is stalled.

## 2. Retransmission Mechanism
- **RLC Retransmission Queue (`rlc_retx_queue`)**:
  - Use a `ring_buffer` with `rlc_retx_queue_item`.
  - Support "zombie" elements for virtual removal of elements within the queue.
  - Track `retx_bytes`, `n_retx_so_zero` (small header), and `n_retx_so_nonzero` (large header) on the fly.
- **Implement `increment_retx_count(uint32_t sn)`**:
  - Increment `retx_count` in `tx_window`.
  - Trigger `upper_cn.on_max_retx()` if `max_retx_thresh` is reached.

## 3. Buffer State & Memory
- **Implement `get_buffer_state()`**:
  - Sum bytes from `sdu_queue`, `sn_under_segmentation`, `retx_queue`, and pending Status PDU.
  - Correctly account for header sizes (min vs max) based on segment offsets.
- **PDU Recycling**: Use `rlc_pdu_recycler` to defer deletion of acknowledged PDUs to `ue_executor`.

## Validation:
- Success is defined by passing `tests/unittests/rlc/rlc_tx_am_test.cpp`, `tests/unittests/rlc/rlc_retx_queue_test.cpp`, and `tests/unittests/rlc/rlc_pdu_recycler_test.cpp`.
