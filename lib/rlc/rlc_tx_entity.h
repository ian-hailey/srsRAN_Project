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

#include "srsran/ran/gnb_du_id.h"
#include "srsran/rlc/rlc_config.h"
#include "srsran/rlc/rlc_entity.h"
#include "srsran/rlc/rlc_tx.h"
#include "srsran/rlc/rlc_tx_metrics.h"
#include "srsran/support/executors/task_executor.h"
#include "srsran/support/timers.h"
#include "srsran/srslog/srslog.h"
#include <utility>

namespace srsran {

/// \brief Base class for TX entities.
class rlc_tx_entity : public rlc_tx_upper_layer_data_interface,
                      public rlc_tx_lower_layer_interface,
                      public rlc_tx_metrics_interface
{
public:
  rlc_tx_entity(gnb_du_id_t          du_index_,
                du_ue_index_t         ue_index_,
                rb_id_t               rb_id_,
                const rlc_tx_metrics& metrics_,
                rlc_tx_lower_layer_notifier&  lower_layer_notifier_,
                rlc_tx_upper_layer_data_notifier&  upper_layer_notifier_,
                rlc_tx_upper_layer_control_notifier&  upper_layer_ctrl_notifier_,
                task_executor&        pcell_executor_,
                timer_factory         timers_) :
    logger(srslog::fetch_basic_logger("RLC")),
    du_index(du_index_),
    ue_index(ue_index_),
    rb_id(rb_id_),
    lower_layer_notifier(&lower_layer_notifier_),
    upper_layer_notifier(&upper_layer_notifier_),
    upper_layer_ctrl_notifier(&upper_layer_ctrl_notifier_),
    pcell_executor(&pcell_executor_),
    timers(timers_)
  {
  }

  ~rlc_tx_entity() override = default;

  void stop() {}

  rlc_tx_metrics get_metrics() override { return metrics; }
  rlc_tx_metrics get_and_reset_metrics() override
  {
    auto ret = metrics;
    reset_metrics();
    return ret;
  }
  void reset_metrics() override { metrics = {}; }

protected:
  srslog::basic_logger& logger;

  gnb_du_id_t          du_index;
  du_ue_index_t         ue_index;
  rb_id_t               rb_id;
  rlc_tx_metrics        metrics;

  rlc_tx_lower_layer_notifier*         lower_layer_notifier = nullptr;
  rlc_tx_upper_layer_data_notifier*    upper_layer_notifier = nullptr;
  rlc_tx_upper_layer_control_notifier* upper_layer_ctrl_notifier = nullptr;

  task_executor* pcell_executor = nullptr;
  timer_factory  timers;
};

} // namespace srsran