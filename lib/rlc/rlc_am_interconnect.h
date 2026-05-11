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

#include "rlc_am_pdu.h"
#include <cstdint>

namespace srsran {

/// \brief Interface for RX AM entity to provide status information to TX AM entity.
class rlc_rx_am_status_provider
{
public:
  virtual ~rlc_rx_am_status_provider() = default;

  /// \brief Get the current STATUS PDU.
  virtual rlc_am_status_pdu get_status_pdu() = 0;
};

/// \brief Interface for TX AM entity to handle status information from RX AM entity.
class rlc_tx_am_status_handler
{
public:
  virtual ~rlc_tx_am_status_handler() = default;

  /// \brief Handle a STATUS PDU from the peer.
  virtual void on_status_pdu(rlc_am_status_pdu status) = 0;
};

/// \brief Notifier for TX AM entity to notify RX AM entity that a status report is required.
class rlc_tx_am_status_notifier
{
public:
  virtual ~rlc_tx_am_status_notifier() = default;

  /// \brief Called to request a status report.
  virtual void on_status_report_required() = 0;
};

} // namespace srsran