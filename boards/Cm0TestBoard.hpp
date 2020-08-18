/*
 * Copyright (c) 2019-2020, University of Southampton and Contributors.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <systemc-ams>
#include <systemc>
#include "boards/Board.hpp"
#include "mcu/Cm0Microcontroller.hpp"
#include "mcu/Microcontroller.hpp"
#include "ps/DynamicEnergyChannel.hpp"
#include "ps/ExternalCircuitry.hpp"
#include "ps/PowerCombine.hpp"
#include "sd/DummySpiDevice.hpp"
#include "utilities/BoolLogicConverter.hpp"
#include "utilities/Config.hpp"
#include "utilities/IoSimulationStopper.hpp"

// Simple custom reset controller for this board
SC_MODULE(ResetCtrl) {
 public:
  // Ports
  sc_core::sc_in<double> vcc{"vcc"};
  sc_core::sc_out<bool> nReset{"nReset"};

  SC_CTOR(ResetCtrl) {
    m_vCore = Config::get().getDouble("CpuCoreVoltage");
    SC_METHOD(process);
    sensitive << vcc;
  }

 private:
  double m_vCore;
  void process() { nReset.write(vcc.read() > m_vCore); }
};

class Cm0TestBoard : public Board {
 public:
  /* ------ Public methods ------ */
  /**
   * @brief constructor
   */
  Cm0TestBoard(const sc_core::sc_module_name name);

  /**
   * @brief destructor closes vcd files
   */
  ~Cm0TestBoard();

  /**
   * @brief getMicrocontroller get a reference to the microcontroller
   */
  virtual Microcontroller &getMicrocontroller() override;

  /* ------ Channels & signals ------ */
  DynamicEnergyChannel dynamicConsumption{"dynamicConsumption"};
  sc_core::sc_signal<double> staticConsumption{"staticConsumption", 0.0};
  sc_core::sc_signal<double> staticConsumptionBoot{"staticConsumptionBoot",
                                                   0.0};
  sc_core::sc_signal<double> totMcuConsumption{"totMcuConsumption", 0.0};
  sc_core::sc_signal<double> vcc{"vcc", 0.0};
  sc_core::sc_signal<bool> nReset{"nReset"};
  sc_core::sc_signal<bool> chipSelectDummySpi{"chipSelectDummySpi", false};
  sc_core::sc_signal<bool> keepAliveBool{"keepAliveBool"};
  std::array<sc_core::sc_signal_resolved, 32> gpioPins;

  /* ------ Submodules ------ */
  ResetCtrl resetCtrl{"resetCtrl"};
  Cm0Microcontroller mcu{"mcu"};
  DummySpiDevice dummySpiDevice{"dummySpiDevice"};
  PowerCombine<2, 1> pwrCombinator{"PowerCombine"};
  ExternalCircuitry externalCircuitry{"externalCircuitry"};
  Utility::ResolvedInBoolOut keepAliveConverter{"keepAliveConverter"};

  /* ------ Tracing ------ */
  sca_util::sca_trace_file *vcdfile;
  sca_util::sca_trace_file *tabfile;
};