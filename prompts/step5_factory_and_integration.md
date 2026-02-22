# RLC Implementation Step 5: Factory, Metrics, and Integration (Technical Specification)

Finalize the RLC subsystem by implementing the entity factory, common metrics infrastructure, and the integrated base classes.

## 1. RLC Factory (`rlc_factory.cpp`)
### Key Responsibilities:
- **Implement `create_rlc_entity(const rlc_entity_creation_message& msg)`**:
  - Switch on `msg.config.mode`.
  - Support `rlc_mode::tm`, `rlc_mode::um_unidir_dl`, `rlc_mode::um_unidir_ul`, `rlc_mode::um_bidir`, and `rlc_mode::am`.
  - Instantiate the correct entity class (`rlc_tm_entity`, `rlc_um_entity`, `rlc_am_entity`) using `std::make_unique`.
  - Pass all necessary collaborators: `upper_dn`, `lower_dn`, `metrics_notif`, `pcap_writer`, `executors`, and `timers`.

## 2. Common Metrics & Logging
- **Bearer Metrics Collector (`rlc_bearer_metrics_collector`)**:
  - Implement a thread-safe collector that aggregates metrics from both high-level (SDU) and low-level (PDU/MAC) paths.
  - Use `lockfree_triple_buffer` to forward lower metrics from the `pcell_executor` to the `ue_executor` without blocking.
  - Implement `push_report()` to combine `tx_high`, `tx_low`, and `rx_high` metrics into a single `rlc_metrics` object.
- **Metrics Containers**: Implement `rlc_tx_metrics_high_container`, `rlc_tx_metrics_low_container`, and `rlc_rx_metrics_container` with atomic-free updates for high-performance paths.

## 3. Base Entity Logic (`rlc_base_entity`)
- **Common Initialization**: Store `du_ue_index_t`, `rb_id_t`, and initialize the `rlc_bearer_logger`.
- **Implement `stop()`**:
  - Orchestrate the shutdown of both `tx` and `rx` entities.
  - Ensure all internal timers are stopped.
- **Implement Interface Getters**: Provide `rlc_tx_upper_layer_data_interface`, `rlc_tx_lower_layer_interface`, and `rlc_rx_lower_layer_interface`.

## 4. Lock-free SDU Queue (`rlc_sdu_queue_lockfree`)
- Implement the internal SPSC queue using `concurrent_queue`.
- Manage `sdu_states` and `sdu_sizes` arrays to support SDU discard by `pdcp_sn`.
- Use `std::atomic<uint64_t>` to store the combined state (n_sdus and n_bytes) for atomic updates and reads.

## Validation:
- Success is defined by passing:
  - `tests/unittests/rlc/rlc_bearer_metrics_collector_test.cpp`
  - `tests/unittests/rlc/rlc_sdu_queue_lockfree_test.cpp`
  - `tests/unittests/rlc/rlc_tx_metrics_test.cpp`
  - All existing unit tests (regression).
