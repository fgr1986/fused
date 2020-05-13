/*
 * Copyright (c) 2019-2020, University of Southampton and Contributors.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sd/DummySpiDevice.hpp"
#include <systemc>

DummySpiDevice::DummySpiDevice(const sc_core::sc_module_name name)
    : SpiDevice(name) {
  // Initialise memory mapped control registers.
  // In this case just make a single dummy.
  c_reg reg0 = {0x0000, 0x00};
  c_regs.push_back(reg0);
}

void DummySpiDevice::reset(void) {
  // Clear the memory mapped control registers.
  std::cout << "DummySpiDevice Reset" << std::endl;
  for (int i = 0; i < n_regs(); i++) {
    c_regs[i].val = 0;
  }
  // Reset SPI shift registers.
  SpiDevice::reset();
}

void DummySpiDevice::process(void) {
  if (pwrOn.read()) {
    // Read from si_reg and decode. Skip for now.
    uint8_t payload = readSiReg();
    std::cout << "DummySpiDevice received payload: " << payload << std::endl;
    // Prepare next reponse in so_reg.
    writeSoReg(c_regs[0].val | payload);
  }
}

uint32_t DummySpiDevice::n_regs(void) { return c_regs.size(); }

