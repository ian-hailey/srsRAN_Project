# RLC Implementation Prompts

This document contains a series of prompts that can be used to implement a 5G NR RLC entity functionally identical to the one in `lib/rlc`.

## Step 1: RLC AM and UM PDU Headers and Status PDU Structures

### Implementation Goal
Implement the data structures and basic pack/unpack functionality for RLC AM and UM PDUs, as defined in 3GPP TS 38.322.

### Prompt
Implement a C++ header file `rlc_am_pdu.h` and a UM equivalent `rlc_um_pdu.h` that define the following:

1.  **Constants**: Define `INVALID_RLC_SN` (0xffffffff) and sizes for various PDU fields (header sizes for 12-bit and 18-bit SNs, status PDU field sizes).
2.  **`rlc_am_pdu_header` Struct**:
    *   Fields: `dc` (Data/Control), `p` (Polling), `si` (Segmentation Info), `sn_size`, `sn` (Sequence Number), `so` (Segment Offset).
    *   Method: `get_packed_size()` to calculate size based on SI.
3.  **`rlc_am_status_nack` Struct**:
    *   Fields: `nack_sn`, `has_so`, `so_start`, `so_end`, `has_nack_range`, `nack_range`.
4.  **`rlc_am_status_pdu` Class**:
    *   Manage a vector of NACKs and the `ack_sn`.
    *   Methods: `pack()`, `unpack()`, `is_control_pdu()`, `push_nack()`, `trim(max_size)`.
    *   Implementation should handle both 12-bit and 18-bit SN formats.
5.  **Helper Functions**:
    *   `rlc_am_read_data_pdu_header()` and `rlc_am_write_data_pdu_header()`.
    *   `rlc_um_read_data_pdu_header()` and `rlc_um_write_data_pdu_header()`.
6.  **Formatters**: Provide `fmt::formatter` specializations for all structures to support structured logging.

Ensure the implementation strictly follows the bit-level formats described in TS 38.322 Section 6.2.

## Step 2: RLC Entity Core Interfaces and Base Class

### Implementation Goal
Define the abstract interfaces and the base class that all RLC entities will inherit from.

### Prompt
Implement the following C++ header files:

1.  **`rlc_tx_entity.h`**:
    *   Define `rlc_tx_entity` inheriting from `rlc_tx_upper_layer_data_interface`, `rlc_tx_lower_layer_interface`, and `rlc_tx_metrics`.
    *   Constructor should initialize logging, metrics containers, and timer factories.
    *   Provide `stop()` and `get_metrics()` methods.
2.  **`rlc_rx_entity.h`**:
    *   Define `rlc_rx_entity` inheriting from `rlc_rx_lower_layer_interface`.
    *   Constructor should initialize logging, metrics, and timer factories.
    *   Provide `stop()` and `get_metrics()` methods.
3.  **`rlc_base_entity.h`**:
    *   Define `rlc_base_entity` inheriting from `rlc_entity`.
    *   Contain common members: `ue_index`, `rb_id`, `tx_entity`, `rx_entity`, and `metrics_collector`.
    *   Implement `stop()`, `get_tx_upper_layer_data_interface()`, `get_tx_lower_layer_interface()`, `get_rx_lower_layer_interface()`, and `get_metrics()`.
4.  **`rlc_am_interconnect.h`**:
    *   Define `rlc_rx_am_status_provider`, `rlc_tx_am_status_handler`, and `rlc_tx_am_status_notifier` interfaces for interaction between TX and RX AM entities.

Use `prefixed_logger` for logging and ensure all constructors take necessary dependencies (IDs, configs, task executors, timers).

## Step 3: Support Structures: Queues, Recycler, and Metrics

### Implementation Goal
Implement the core data structures used by RLC entities for SDU buffering, retransmission management, and performance tracking.

### Prompt
Implement the following components:

1.  **`rlc_sdu_queue_lockfree.h`**:
    *   A single-producer single-consumer (SPSC) lock-free queue for SDUs.
    *   Support for SDU discard based on PDCP SN.
    *   Track number of SDUs and total bytes atomically.
2.  **`rlc_retx_queue.h`**:
    *   A queue to manage RLC AMD retransmissions.
    *   Use a `ring_buffer` internally to avoid dynamic allocations on the critical path.
    *   Support marking elements as "invalid" (zombies) for deferred removal.
3.  **`rlc_pdu_recycler.h`**:
    *   A lock-free mechanism to offload PDU deletion to a non-real-time task executor.
4.  **`rlc_bearer_metrics_collector.h/cpp`**:
    *   Collect and aggregate metrics from both TX and RX entities.
    *   Use triple buffering to pass metrics between executors without allocation.

Ensure all structures are thread-safe where necessary, emphasizing performance on the L2 critical path.

## Step 4: Transparent Mode (TM) Entities

### Implementation Goal
Implement the simplest RLC mode, where PDUs are passed through without modification or headers.

### Prompt
Implement `rlc_tx_tm_entity.h/cpp` and `rlc_rx_tm_entity.h/cpp`:

1.  **`rlc_tx_tm_entity`**:
    *   Inherit from `rlc_tx_entity`.
    *   Handle SDUs by placing them in a lock-free queue.
    *   Implement `pull_pdu()`: Read SDU from queue and copy payload directly to MAC buffer (no RLC header).
    *   Manage buffer state updates and defer them to the `pcell_executor`.
2.  **`rlc_rx_tm_entity`**:
    *   Inherit from `rlc_rx_entity`.
    *   Implement `handle_pdu()`: Convert received MAC PDU directly to RLC SDU and notify the upper layer.
3.  **Testing**: Ensure the implementation passes `rlc_tx_tm_test` and `rlc_rx_tm_test`.

## Step 5: Unacknowledged Mode (UM) Entities

### Implementation Goal
Implement UM with support for segmentation and reassembly, using SNs for reordering (if configured).

### Prompt
Implement `rlc_tx_um_entity.h/cpp` and `rlc_rx_um_entity.h/cpp`:

1.  **`rlc_tx_um_entity`**:
    *   Implement UM TX logic including segmentation.
    *   Support 6-bit and 12-bit SN sizes.
    *   Manage `next_so` (segment offset) for SDU segmentation across multiple MAC grants.
    *   Notify upper layers on SDU transmission.
2.  **`rlc_rx_um_entity`**:
    *   Implement UM RX logic with reassembly.
    *   Use an `sdu_window` to manage received segments.
    *   Implement `t-Reassembly` timer logic to detect lost PDUs.
    *   Support reassembly of fragmented SDUs.
3.  **Testing**: Validate against `rlc_um_test` and `rlc_um_pdu_test`.

## Step 6: Acknowledged Mode (AM) Entities

### Implementation Goal
Implement the full AM protocol including ARQ (Automatic Repeat Request), polling logic, and status reporting.

### Prompt
Implement `rlc_tx_am_entity.h/cpp` and `rlc_rx_am_entity.h/cpp`:

1.  **`rlc_tx_am_entity`**:
    *   Implement AM TX state machine: `TX_Next`, `TX_Next_Ack`, `POLL_SN`.
    *   Handle status PDUs from peer: update TX window, schedule retransmissions via `retx_queue`.
    *   Implement polling logic: `PDU_WITHOUT_POLL`, `BYTE_WITHOUT_POLL`, and `t-PollRetransmit`.
    *   Manage SDU segmentation and retransmission of NACK'ed segments.
2.  **`rlc_rx_am_entity`**:
    *   Implement AM RX state machine: `RX_Next`, `RX_Highest_Status`, `RX_Next_Highest`.
    *   Manage RX window and segment reassembly.
    *   Generate status reports based on missing segments/SDUs.
    *   Implement `t-StatusProhibit` and `t-Reassembly` timers.
3.  **Testing**: Extensive validation using `rlc_tx_am_test`, `rlc_rx_am_test`, and `rlc_am_pdu_test`.

## Step 7: RLC Factory and Integration

### Implementation Goal
Provide a unified entry point for creating RLC entities based on configuration.

### Prompt
Implement `rlc_factory.cpp`:

1.  Implement `create_rlc_entity(msg)` which switches on `msg.config.mode`.
2.  Instantiate `rlc_tm_entity`, `rlc_um_entity`, or `rlc_am_entity` accordingly.
3.  Ensure proper interconnection between TX and RX components (especially for AM).

Perform a final integration check by ensuring all RLC unit tests pass and providing a report on test coverage.
