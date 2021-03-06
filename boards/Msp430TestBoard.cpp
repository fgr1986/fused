/*
 * Copyright (c) 2019-2020, University of Southampton and Contributors.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <spdlog/spdlog.h>
#include <systemc-ams>
#include <systemc>
#include "boards/Msp430TestBoard.hpp"
#include "utilities/BoolLogicConverter.hpp"
#include "utilities/Config.hpp"

using namespace sc_core;

Msp430TestBoard::Msp430TestBoard(const sc_module_name name)
    : Board(name),
      powerModelChannel(
          "powerModelChannel", /*logfile=*/
          Config::get().getString("OutputDirectory"),
          sc_time::from_seconds(Config::get().getDouble("LogTimestep"))) {
  /* ------ Bind ------ */
  // Reset
  mcu.pmm->pwrGood.bind(nReset);
  mcu.nReset.bind(nReset);

  // IO ports
  for (unsigned i = 0; i < DIOAPins.size(); i++) {
    mcu.portA->pins[i].bind(DIOAPins[i]);
    mcu.portB->pins[i].bind(DIOBPins[i]);
    mcu.portC->pins[i].bind(DIOCPins[i]);
    mcu.portD->pins[i].bind(DIODPins[i]);
  }

  // off-chip serial devices
  spiLoopBack.nReset.bind(nReset);
  spiLoopBack.chipSelect.bind(chipSelectSpiWire);
  spiLoopBack.tSocket.bind(mcu.euscib->iEusciSocket);
  spiLoopBack.powerModelPort.bind(powerModelChannel);

  // Power circuitry
  mcu.powerModelPort.bind(powerModelChannel);
  powerModelBridge.powerModelPort.bind(powerModelChannel);
  powerModelBridge.i_out.bind(icc);
  powerModelBridge.v_in.bind(vcc);
  mcu.vcc.bind(vcc);

  // External circuits (capacitor + supply voltage supervisor etc.)
  externalCircuitry.i_out.bind(icc);
  externalCircuitry.vcc.bind(vcc);
  externalCircuitry.v_warn.bind(DIOBPins[0]);

  // KeepAlive -- bind to IO via converter
  keepAliveConverter.in.bind(DIOCPins[8 + 0]);  // P6.0 as keepAlive
  keepAliveConverter.out.bind(keepAliveBool);
  externalCircuitry.keepAlive.bind(keepAliveConverter.out);

  // Stop simulation after <configurable> io toggles
  simStopper.in(DIOAPins[2]);

  // Print memory map
  std::cout << "------ MCU construction complete ------\n" << mcu.bus;

  /* ------- Signal tracing ------ */
  // Creates a value-change dump
  vcdfile = sca_util::sca_create_vcd_trace_file(
      (Config::get().getString("OutputDirectory") + "/ext.vcd").c_str());

  for (int i = 0; i < DIOAPins.size(); ++i) {
    sca_trace(vcdfile, DIOAPins[i], fmt::format("PA{:02d}", i));
    sca_trace(vcdfile, DIOBPins[i], fmt::format("PB{:02d}", i));
    sca_trace(vcdfile, DIOCPins[i], fmt::format("PC{:02d}", i));
    sca_trace(vcdfile, DIODPins[i], fmt::format("PD{:02d}", i));
  }
  for (size_t i = 0; i < mcu.dmaTrigger.size(); ++i) {
    sca_trace(vcdfile, mcu.dmaTrigger[i], fmt::format("dmatrigger{:02d}", i));
  }
  for (int i = 0; i < Dma::NCHANNELS; ++i) {
    sca_trace(vcdfile, mcu.dma->m_channels[i]->trigger,
              fmt::format("dma_channel{:02d}_trigger", i));
  }
  sca_trace(vcdfile, mcu.cpuStall, "cpuStall");

  // Creates a csv-like file
  tabfile = sca_util::sca_create_tabular_trace_file(
      (Config::get().getString("OutputDirectory") + "/ext.tab").c_str());

  sca_trace(tabfile, vcc, "vcc");
  sca_trace(tabfile, icc, "icc");
  sca_trace(tabfile, nReset, "nReset");
  sca_trace(tabfile, externalCircuitry.v_cap, "externalCircuitry.v_cap");
  sca_trace(tabfile, externalCircuitry.keepAlive,
            "externalCircuitry.keepAlive");
  sca_trace(tabfile, externalCircuitry.i_supply, "externalCircuitry.i_supply");
}

Msp430TestBoard::~Msp430TestBoard() {
  sca_util::sca_close_vcd_trace_file(vcdfile);
  sca_util::sca_close_tabular_trace_file(tabfile);
}

Microcontroller &Msp430TestBoard::getMicrocontroller() { return mcu; }
