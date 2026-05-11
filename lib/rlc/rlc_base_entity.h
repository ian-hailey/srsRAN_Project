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

#include "rlc_bearer_metrics_collector.h"
#include "rlc_rx_entity.h"
#include "rlc_tx_entity.h"
#include "srsran/rlc/rlc_entity.h"
#include <memory>

namespace srsran {

/// \brief Base class for combined RLC entities (TX + RX).
class rlc_base_entity : public rlc_entity
{
public:
  rlc_base_entity(du_ue_index_t         ue_index_,
                  rb_id_t               rb_id_,
                  task_executor&        ue_executor_,
                  timer_factory         timers_) :
    ue_index(ue_index_),
    rb_id(rb_id_),
    ue_executor(&ue_executor_),
    timers(timers_)
  {
  }

  ~rlc_base_entity() override = default;

  void stop() override
  {
    if (tx_entity) {
      tx_entity->stop();
    }
    if (rx_entity) {
      rx_entity->stop();
    }
  }

  rlc_tx_upper_layer_data_interface* get_tx_upper_layer_data_interface() override
  {
    return tx_entity.get();
  }

  rlc_tx_lower_layer_interface* get_tx_lower_layer_interface() override
  {
    return tx_entity.get();
  }

  rlc_rx_lower_layer_interface* get_rx_lower_layer_interface() override
  {
    return rx_entity.get();
  }

  rlc_metrics get_metrics() override
  {
    rlc_metrics m = {};
    m.ue_index    = ue_index;
    m.rb_id       = rb_id;
    m.counter     = 0;
    if (metrics_collector) {
      m = metrics_collector->get_metrics();
    }
    if (tx_entity) {
      m.tx = tx_entity->get_metrics();
    }
    if (rx_entity) {
      m.rx = rx_entity->get_metrics();
    }
    return m;
  }

protected:
  du_ue_index_t                       ue_index;
  rb_id_t                             rb_id;
  std::unique_ptr<rlc_tx_entity>      tx_entity;
  std::unique_ptr<rlc_rx_entity>      rx_entity;

  task_executor*                      ue_executor = nullptr;
  timer_factory                       timers;

  std::unique_ptr<rlc_bearer_metrics_collector> metrics_collector;
};

} // namespace srsran