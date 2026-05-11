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
#include "rlc_rx_entity.h"

namespace srsran {

/// \brief Stub RX AM entity.
class rlc_rx_am_entity : public rlc_rx_entity, public rlc_rx_am_status_provider
{
public:
  rlc_rx_am_entity(gnb_du_id_t          du_index_,
                   du_ue_index_t         ue_index_,
                   rb_id_t               rb_id_,
                   const rlc_rx_am_config& config,
                   rlc_rx_upper_layer_data_notifier&  upper_layer_notifier_,
                   rlc_tx_am_status_notifier&         status_notifier_,
                   task_executor&        pcell_executor_,
                   timer_factory         timers_) :
    rlc_rx_entity(du_index_, ue_index_, rb_id_, upper_layer_notifier_, pcell_executor_, timers_),
    cfg(config),
    status_notifier(&status_notifier_)
  {
    metrics.mode_specific = rlc_am_rx_metrics{};
  }

  void handle_pdu(byte_buffer_slice pdu) override { logger.warning("RX AM PDU not implemented"); }
  rlc_am_status_pdu get_status_pdu() override { return rlc_am_status_pdu(cfg.sn_field_length); }

private:
  rlc_rx_am_config           cfg;
  rlc_tx_am_status_notifier* status_notifier = nullptr;
};

} // namespace srsran