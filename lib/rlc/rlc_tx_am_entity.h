/*
 *
 * Copyright 2021-2026 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#pragma once

#include "rlc_am_interconnect.h"
#include "rlc_tx_entity.h"

namespace srsran {

/// \brief Stub TX AM entity.
class rlc_tx_am_entity : public rlc_tx_entity, public rlc_tx_am_status_handler
{
public:
  rlc_tx_am_entity(gnb_du_id_t          du_index_,
                   du_ue_index_t         ue_index_,
                   rb_id_t               rb_id_,
                   const rlc_tx_am_config& config,
                   rlc_tx_upper_layer_data_notifier&    upper_layer_notifier_,
                   rlc_tx_upper_layer_control_notifier& upper_layer_ctrl_notifier_,
                   rlc_tx_lower_layer_notifier&         lower_layer_notifier_,
                   rlc_tx_am_status_notifier&           status_notifier_,
                   task_executor&        pcell_executor_,
                   timer_factory         timers_) :
    rlc_tx_entity(du_index_,
                  ue_index_,
                  rb_id_,
                  rlc_tx_metrics{},
                  lower_layer_notifier_,
                  upper_layer_notifier_,
                  upper_layer_ctrl_notifier_,
                  pcell_executor_,
                  timers_),
    cfg(config),
    status_notifier(&status_notifier_)
  {
    metrics.tx_low.mode_specific = rlc_am_tx_metrics_lower{};
  }

  void handle_sdu(byte_buffer sdu_buf, bool is_retx) override { logger.warning("TX AM SDU not implemented"); }
  void discard_sdu(uint32_t pdcp_sn) override { logger.warning("TX AM discard not implemented"); }
  size_t pull_pdu(span<uint8_t> rlc_pdu_buf) override { return 0; }
  rlc_buffer_state get_buffer_state() override { return {}; }
  void on_status_pdu(rlc_am_status_pdu status) override { logger.warning("TX AM status not implemented"); }

private:
  rlc_tx_am_config           cfg;
  rlc_tx_am_status_notifier* status_notifier = nullptr;
};

} // namespace srsran