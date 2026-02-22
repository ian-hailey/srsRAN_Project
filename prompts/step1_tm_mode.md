# RLC Implementation Step 1: TM Mode

Implement the RLC Transparent Mode (TM) entity for both Transmission (TX) and Reception (RX).

## Requirements:
1. Implement `rlc_tx_tm_entity`:
   - FIFO queue for SDUs.
   - Transparently pass SDUs to lower layers in `pull_pdu` without headers.
   - Handle buffer state updates and notifications.
   - PCAP integration for tracing.
   - Metrics collection.
2. Implement `rlc_rx_tm_entity`:
   - Receive PDUs from lower layers and deliver them as SDUs to upper layers.
   - Transparently handle data without RLC headers.
   - Metrics collection.
3. Use the common base classes and logging helpers provided in the project.

## Validation:
- Pass all tests in `tests/unittests/rlc/rlc_tx_tm_test.cpp` and `tests/unittests/rlc/rlc_rx_tm_test.cpp`.
