# RLC Implementation Step 2: UM Mode (Technical Specification)

Implement the RLC Unacknowledged Mode (UM) entity for Transmission (TX) and Reception (RX), following the 3GPP TS 38.322 specifications and matching the existing srsRAN architecture.

## 1. RLC TX UM Entity (`rlc_tx_um_entity`)
### Key Responsibilities:
- **State Management**: Maintain `tx_next` (UM send state variable).
- **Header Structure**: Use `rlc_um_pdu_header` with 6-bit or 12-bit SN.
- **Implement `handle_sdu(byte_buffer sdu_buf, bool is_retx)`**:
  - Extract `pdcp_sn` from the buffer using `get_pdcp_sn`.
  - Write to `sdu_queue` and trigger buffer state update.
- **Implement `pull_pdu(span<uint8_t> mac_sdu_buf)`**:
  - **Segmentation Logic**: Support splitting an SDU into `first_segment`, `middle_segment`, and `last_segment` based on the MAC grant size.
  - Track `next_so` (segment offset) for the current SDU being segmented.
  - Update `tx_next` only after submitting the `last_segment` or a `full_sdu`.
  - Perform `SRSRAN_RTSAN_NONBLOCKING` operations on `pcell_executor`.
- **Implement `get_buffer_state()`**:
  - Account for headers in pending byte calculation.
  - Support `suspend_bs_notif_barring` logic to handle large buffer state transitions.

## 2. RLC RX UM Entity (`rlc_rx_um_entity`)
### Key Responsibilities:
- **State Management**: Maintain `rx_next_reassembly`, `rx_timer_trigger`, and `rx_next_highest`.
- **Reassembly Window**: Implement `sn_in_reassembly_window` and `sn_invalid_for_rx_buffer` logic using a modulus base.
- **Implement `handle_pdu(byte_buffer_slice buf)`**:
  - Unpack `rlc_um_pdu_header`.
  - If `full_sdu`, deliver immediately to upper layer.
  - If segmented, use `sdu_window<rlc_rx_um_sdu_info>` to buffer segments.
  - **Overlapping Logic**: Implement `store_segment` to handle overlapping bytes by trimming or dropping segments.
  - **Timer Handling**: Manage `reassembly_timer` (t-Reassembly) to detect and handle gaps.
- **Implement `reassemble_sdu`**:
  - Chain `byte_buffer_slice` objects into a `byte_buffer_chain` once all segments are received.

## 3. Integration & Logging
- Use `rlc_um_read_data_pdu_header` and `rlc_um_write_data_pdu_header` helpers.
- Hex dump PDU content in logs for debugging.

## Validation:
- Success is defined by passing `tests/unittests/rlc/rlc_um_test.cpp` and `tests/unittests/rlc/rlc_um_pdu_test.cpp`.
