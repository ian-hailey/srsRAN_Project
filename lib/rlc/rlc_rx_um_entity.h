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

#include "rlc_rx_entity.h"

namespace srsran {

/// \brief Stub RX UM entity.
class rlc_rx_um_entity : public rlc_rx_entity
{
public:
  rlc_rx_um_entity(gnb_du_id_t          du_index_,
                   du_ue_index_t         ue_index_,
                   rb_id_t               rb_id_,
                   const rlc_rx_um_config& config,
                   rlc_rx_upper_layer_data_notifier&  upper_layer_notifier_,
                   task_executor&        pcell_executor_,
                   timer_factory         timers_) :
    rlc_rx_entity(du_index_, ue_index_, rb_id_, upper_layer_notifier_, pcell_executor_, timers_),
    cfg(config)
  {
    metrics.mode_specific = rlc_um_rx_metrics{};
  }

  void handle_pdu(byte_buffer_slice pdu) override { logger.warning("RX UM PDU not implemented"); }

private:
  rlc_rx_um_config cfg;
};

} // namespace srsran