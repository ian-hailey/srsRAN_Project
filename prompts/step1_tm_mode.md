# RLC Implementation Step 1: TM Mode (Technical Specification)

Implement the RLC Transparent Mode (TM) entity for Transmission (TX) and Reception (RX), following the 3GPP TS 38.322 specifications and matching the existing srsRAN architecture.

## 1. Data Structures and Dependencies
- **rlc_sdu**: A struct containing `byte_buffer buf`, `std::chrono::time_point time_of_arrival`, and `bool is_retx`.
- **rlc_tx_tm_config** / **rlc_rx_tm_config**: Configuration structures for queue sizes and timers.
- **rlc_sdu_queue_lockfree**: A single-producer single-consumer lock-free queue for managing pending SDUs.

## 2. RLC TX TM Entity (`rlc_tx_tm_entity`)
### Key Responsibilities:
- Manage a `rlc_sdu_queue_lockfree` for incoming SDUs.
- Implement `handle_sdu(byte_buffer sdu_buf, bool is_retx)`:
  - Capture `time_of_arrival`.
  - Write to `sdu_queue`.
  - Trigger `handle_changed_buffer_state()` if successful.
  - Log failures and update `metrics_high`.
- Implement `pull_pdu(span<uint8_t> mac_sdu_buf)`:
  - **Thread Safety**: Must be `SRSRAN_RTSAN_NONBLOCKING` (runs on `pcell_executor`).
  - Read from `sdu_queue`.
  - Check if SDU fits in the provided grant. If not, log a "small grant" warning and return 0.
  - Notify upper layer of transmission via `upper_dn.on_transmitted_sdu`.
  - Copy SDU segments to `mac_sdu_buf`.
  - **Memory Management**: Defer SDU buffer deletion to the `ue_executor` to keep the `pcell_executor` fast.
  - Push PDU to `pcap` and update `metrics_low`.
- Implement `get_buffer_state()`:
  - Calculate `pending_bytes` based on queue size and current SDU.
  - Track `hol_toa` (Head of Line Time of Arrival).
- Implement `handle_changed_buffer_state()`:
  - Use `std::atomic_flag` (`pending_buffer_state`) to avoid redundant updates.
  - Defer `update_mac_buffer_state()` to `pcell_executor`.

## 3. RLC RX TM Entity (`rlc_rx_tm_entity`)
### Key Responsibilities:
- Implement `handle_pdu(byte_buffer_slice buf)`:
  - Update `metrics.num_pdus`.
  - Push PDU to `pcap` using `pcap_rlc_pdu_context`.
  - Create a `byte_buffer_chain` from the slice.
  - Deliver to upper layer via `upper_dn.on_new_sdu`.
- Implement `stop()`:
  - Stop the `high_metrics_timer`.

## 4. Integration Logic
- Inherit from `rlc_tx_entity` and `rlc_rx_entity` respectively.
- Use `rlc_bearer_logger` with prefix "RLC" and context `{gnb_du_id, ue_index, rb_id, direction}`.

## Validation:
- Success is defined by passing `tests/unittests/rlc/rlc_tx_tm_test.cpp` and `tests/unittests/rlc/rlc_rx_tm_test.cpp`.
- Functional identity requires matching hex-dump logging behavior for PDU content.
