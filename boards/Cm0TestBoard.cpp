/*
 * Copyright (c) 2019-2020, University of Southampton and Contributors.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

//! Maximum size of a RSP packet is used to return the value of all the
//! registers, each of which takes 8 chars. There are a total of 32 GPRs plus
//! PPC, SR and NPC. Plus one byte for end of string marker.
#define RSP_MAX_PKT_SIZE ((32 + 3) * 8 + 1)

//! Default port for RSP to listen on
#define DEFAULT_RSP_PORT 51000

#include <spdlog/spdlog.h>
#include <chrono>
#include <cstdlib>
#include <ihex-parser/IntelHexFile.hpp>
#include <memory>
#include <string>
#include <systemc-ams>
#include <systemc>
#include <thread>
#include "mcu/Cm0Microcontroller.hpp"
#include "mcu/Microcontroller.hpp"
#include "ps/DynamicEnergyChannel.hpp"
#include "ps/EventLog.hpp"
#include "ps/ExternalCircuitry.hpp"
#include "ps/PowerCombine.hpp"
#include "sd/DummySpiDevice.hpp"
#include "utilities/Config.hpp"
#include "utilities/IoSimulationStopper.hpp"
#include "utilities/SimulationController.hpp"

#ifdef GDB_SERVER
#include <gdb-server/GdbServer.hpp>
#endif

using namespace sc_core;

// clang-format off
// A dummy module to implement the end_of_simulation callback.
SC_MODULE(DummyModule){
  public:
  DummyModule(sc_module_name nm, SimulationController *simCtrl)
    : sc_module(nm), m_simCtrl(simCtrl) {};

  /**
   * @brief end_of_simulation callback. Called by systemc after sc_stop(), but
   * before module destruction. Use this to stop debugging server before
   * module destruction.
   */
  virtual void end_of_simulation() override {
    if (Config::get().getBool("GdbServer")) {
      m_simCtrl->stopServer();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      while(m_simCtrl->isServerRunning()) {
        spdlog::info("Waiting for debug server to exit...");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }
    }
  }
  private:
  SimulationController *m_simCtrl;
};
// clang-format on

// Simple custom reset controller for this module
SC_MODULE(ResetCtrl) {
 public:
  // Ports
  sc_in<double> vcc{"vcc"};
  sc_out<bool> nReset{"nReset"};

  SC_CTOR(ResetCtrl) {
    m_vCore = Config::get().getDouble("CpuCoreVoltage");
    SC_METHOD(process);
    sensitive << vcc;
  }

 private:
  double m_vCore;
  void process() { nReset.write(vcc.read() > m_vCore); }
};

int sc_main(int argc, char *argv[]) {
  // sc_set_time_resolution(1, SC_NS);
  int rspPort = DEFAULT_RSP_PORT;

  // Parse CLI arguments & config file
  auto &config = Config::get();
  config.parseCli(argc, argv);
  config.parseFile();

  DynamicEnergyChannel dynamicConsumption("dynamicConsumption");

  sc_signal<double> staticConsumption{"staticConsumption", 0.0};
  sc_signal<double> staticConsumptionBoot{"staticConsumptionBoot", 0.0};
  sc_signal<double> totMcuConsumption{"totMcuConsumption", 0.0};
  sc_signal<double> vcc{"vcc", 0.0};
  sc_signal<bool> nReset{"nReset"};
  sc_signal<bool> chipSelectDummySpi{"chipSelectDummySpi", false};

  // Instantiate microcontroller
  ResetCtrl resetCtrl("resetCtrl");
  resetCtrl.vcc.bind(vcc);
  resetCtrl.nReset.bind(nReset);
  Cm0Microcontroller *mcu = new Cm0Microcontroller("mcu");

  // IO/interrupts ports
  std::array<sc_signal<bool>, 16> externalIrq;
  std::array<sc_signal<bool>, 32> mcuOutputPort;
  for (unsigned i = 0; i < mcuOutputPort.size(); i++) {
    mcu->outputPort->pins[i].bind(mcuOutputPort[i]);
  }
  for (unsigned i = 0; i < mcu->externalIrq.size(); i++) {
    mcu->externalIrq[i].bind(externalIrq[i]);
  }

  // Instantiate off-chip serial devices
  DummySpiDevice *dummySpiDevice = new DummySpiDevice("dummySpiDevice");
  dummySpiDevice->nReset.bind(nReset);
  dummySpiDevice->chipSelect.bind(chipSelectDummySpi);
  dummySpiDevice->tSocket.bind(mcu->spi->spiSocket);

  // Print memory map
  std::cout << "------ MCU construction complete ------\n" << mcu->bus;

  mcu->vcc.bind(vcc);
  mcu->nReset.bind(nReset);
  mcu->staticPower.bind(staticConsumptionBoot);

  // Power circuitry
  // Static + dynamic -> static
  PowerCombine<2, 1> pwrCombinator("PowerCombine");
  pwrCombinator.staticConsumers[0].bind(staticConsumption);
  pwrCombinator.staticConsumers[1].bind(staticConsumptionBoot);
  pwrCombinator.dynamicConsumers[0].bind(dynamicConsumption);
  pwrCombinator.sum.bind(totMcuConsumption);
  pwrCombinator.vcc.bind(vcc);
  pwrCombinator.pwrGood.bind(nReset);

  // MCU consumption
  EventLog::getInstance().dynamicEnergy.bind(dynamicConsumption);
  EventLog::getInstance().staticPower.bind(staticConsumption);

  // External circuits (capacitor + supply voltage supervisor etc.)
  ExternalCircuitry ext("ext");
  ext.i_out.bind(totMcuConsumption);
  ext.vcc.bind(vcc);
  ext.keepAlive(mcuOutputPort[0]);
  ext.v_warn.bind(externalIrq[0]);

  // Set up output folder
  // When <filesystem> is available:
  // std::filesystem::create_directories(config.getString("OutputDirectory"))
  auto sysStatus = system(
      std::string("mkdir -p " + config.getString("OutputDirectory")).c_str());
  if (sysStatus) {
    spdlog::error("Failed to create output directory at {} ... exiting",
                  config.getString("OutputDirectory"));
    exit(1);
  }

  /* ------- Signal tracing ------ */
  // Creates a value-change dump
  auto *vcdfile = sca_util::sca_create_vcd_trace_file(
      (Config::get().getString("OutputDirectory") + "/ext.vcd").c_str());

  for (int i = 0; i < mcuOutputPort.size(); ++i) {
    sca_trace(vcdfile, mcuOutputPort[i], fmt::format("P{:02d}", i));
  }
  for (int i = 0; i < mcu->nvic->irq.size(); ++i) {
    sca_trace(vcdfile, mcu->nvic->irq[i], fmt::format("NVIC.irqIn[{:02d}]", i));
  }
  sca_trace(vcdfile, mcu->nvic_pending, "NVIC.pendingIrq");
  sca_trace(vcdfile, mcu->cpu_active_exception, "CPU.ActiveException");
  sca_trace(vcdfile, mcu->cpu_returning_exception, "CPU.ReturningException");
  sca_trace(vcdfile, mcu->spi->irq, "SPI.irq");
  sca_trace(vcdfile, mcu->systick_irq, "SysTick.irq");

  // Creates a csv-like file
  auto *tabfile = sca_util::sca_create_tabular_trace_file(
      (Config::get().getString("OutputDirectory") + "/ext.tab").c_str());

  sca_trace(tabfile, vcc, "vcc");
  sca_trace(tabfile, totMcuConsumption, "icc");
  sca_trace(tabfile, nReset, "nReset");
  sca_trace(tabfile, ext.v_cap, "ext.v_cap");
  sca_trace(tabfile, ext.keepAlive, "ext.keepAlive");
  sca_trace(tabfile, ext.i_supply, "ext.i_supply");
  sca_trace(tabfile, totMcuConsumption, "icc");

  /* ------ Simulation control ------ */
  SimulationController simCtrl(mcu);
  [[maybe_unused]] DummyModule d(
      "dummy", &simCtrl);  // Used to access end_of_simulation callback

#ifdef GDB_SERVER
  GdbServer *gdbServer;
  std::thread *dbgThread;
#endif

  if (Config::get().getBool("GdbServer")) {
#ifdef GDB_SERVER
    spdlog::info("Starting gdb server");
    gdbServer = new GdbServer(&simCtrl, rspPort);
    dbgThread = new std::thread(&GdbServer::serverThread, gdbServer);
#else
    spdlog::error("'GdbServer' true in config, but GDB_SERVER is undefined.");
    exit(1);
#endif
  } else {
    // Load binary to mcu
    auto fn = Config::get().getString("ProgramHexFile");
    if (fn.find(".hex") == std::string::npos) {
      spdlog::error(
          "-x: Invalid file format for input file {:s}, must be '.hex'", fn);
      return 1;
    }
    IntelHexFile programFile(fn);
    sc_start(SC_ZERO_TIME);  // Finish elaboration before programming
    for (const auto &s : programFile.getProgramData()) {
      simCtrl.writeMem(&s.second[0], s.first, s.second.size());
    }
    simCtrl.unstall();
  }

  spdlog::info("Starting simulation.");
  sc_start(sc_time::from_seconds(Config::get().getDouble("SimTimeLimit")));

  if (!sc_end_of_simulation_invoked()) {
    spdlog::error("Simulation stopped without explicit sc_stop()");
    sc_stop();
  }

#ifdef GDB_SERVER
  if (Config::get().getBool("GdbServer")) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
    dbgThread->join();
#pragma GCC diagnostic pop
  }
#endif

  EventLog::getInstance().dumpCsv();
  sca_util::sca_close_vcd_trace_file(vcdfile);
  sca_util::sca_close_tabular_trace_file(tabfile);

  return 0;
}