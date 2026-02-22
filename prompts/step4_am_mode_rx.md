# RLC Implementation Step 4: AM Mode Receive & Status Reporting (Technical Specification)

Implement the Receive logic and Status Reporting mechanism for RLC Acknowledged Mode (AM).

## 1. RLC RX AM Entity (`rlc_rx_am_entity`)
### Key Responsibilities:
- **State Management**: Maintain `rx_next` (lower edge), `rx_next_status_trigger` (t-Reassembly trigger), `rx_highest_status` (highest ACK_SN), and `rx_next_highest`.
- **Implement `handle_pdu(byte_buffer_slice buf)`**:
  - Identify Control vs. Data PDUs.
  - **Control PDUs**: Forward to `status_handler->on_status_pdu`.
  - **Data PDUs**:
    - Validate SN against RX window.
    - Handle duplicates and reassemble full SDUs.
    - Update `rx_next_highest` and `rx_next`.
    - **Timer Management**: Start/Stop `reassembly_timer` (t-Reassembly) based on gaps and window state.
- **Implement `refresh_status_report()`**:
  - Iterate from `rx_next` to `stop_sn` (limited by `max_nof_sn_per_status_report`).
  - Detect gaps in `rx_window` and push NACKs to the status report.
  - Support both full SDU NACKs and segmented SDU NACKs with `so_start` and `so_end`.
  - Set `ack_sn` to the first SN not missing and not in the status report.
- **Implement `on_expired_reassembly_timer()`**:
  - Update `rx_highest_status`.
  - Trigger a status report and restart the timer if gaps remain.

## 2. Status PDU Structure (`rlc_am_status_pdu`)
- Implement `pack()` and `unpack()` for 12-bit and 18-bit SN formats.
- Support `push_nack` with merging logic:
  - Merge continuous NACKs into a range (up to 255).
  - Handle segment merging logic (SO boundaries).
- Implement `trim(uint32_t max_packed_size)` to fit the PDU into a grant.

## 3. Concurrency & Performance
- **Status Report Exchange**: Use a triple-buffer style approach with `status_for_exchange` (atomic pointer) to share reports with the TX entity without locks.
- **Timer Handling**: Execute timer callbacks on `ue_executor` to avoid blocking the real-time path.

## Validation:
- Success is defined by passing `tests/unittests/rlc/rlc_rx_am_test.cpp` and `tests/unittests/rlc/rlc_am_pdu_test.cpp`.
