/*
 * Copyright (c) 2019-2020, University of Southampton and Contributors.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <spdlog/spdlog.h>
#include <systemc>
#include "sd/Bme280.hpp"

using namespace sc_core;

Bme280::Bme280(const sc_module_name name)
    : SpiDevice(name, ChipSelectPolarity::ActiveLow) {
  // Initialise memory mapped control registers.
  m_regs.addRegister(0);
  m_regs.addRegister(ADDR_HUM_LSB, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_HUM_MSB, 0x80, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_TEMP_XLSB, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_TEMP_LSB, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_TEMP_MSB, 0x80, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_PRESS_XLSB, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_PRESS_LSB, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_PRESS_MSB, 0x80, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CONFIG, 0, RegisterFile::AccessMode::READ_WRITE,
                     /*mask=*/~(1u << 1));
  m_regs.addRegister(ADDR_CTRL_MEAS, 0, RegisterFile::AccessMode::READ_WRITE);
  m_regs.addRegister(ADDR_STATUS, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CTRL_HUM, 0, RegisterFile::AccessMode::READ_WRITE);
  m_regs.addRegister(ADDR_RESET, 0, RegisterFile::AccessMode::READ_WRITE);
  m_regs.addRegister(ADDR_ID, 0x60, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_00, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_01, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_02, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_03, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_04, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_05, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_06, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_07, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_08, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_09, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_10, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_11, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_12, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_13, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_14, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_15, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_16, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_17, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_18, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_19, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_20, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_21, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_22, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_23, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_24, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_25, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_26, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_27, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_28, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_29, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_30, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_31, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_32, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_33, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_34, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_35, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_36, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_37, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_38, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_39, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_40, 0, RegisterFile::AccessMode::READ);
  m_regs.addRegister(ADDR_CALIB_41, 0, RegisterFile::AccessMode::READ);

void Bme280::end_of_elaboration() {
  SC_METHOD(spiInterface);
  sensitive << m_transactionEvent << chipSelect.posedge_event();
  dont_initialize();

  SC_THREAD(measurementLoop);
}

void Bme280::reset(void) {
  m_regs.reset();
  SpiDevice::reset();
}

void Bme280::spiInterface(void) {
  // First word after chip select is address, remaining are data
  const unsigned READ_BIT = (1u << 7);
  const auto payload = readSlaveIn();
  spdlog::info("{:s}: @{:s} Received 0x{:08x}", this->name(),
               sc_time_stamp().to_string(), payload);

  if (enabled()) {  // Chip select active
    switch (m_spiState) {
      case SpiState::Address:
        m_activeAddress = payload | READ_BIT;
        m_isWriteAccess = !(payload & READ_BIT);
        m_spiState = SpiState::Data;
        break;
      case SpiState::Data:
        if (m_isWriteAccess) {  // Write access
          switch (m_activeAddress) {
            case ADDR_CONFIG:
              m_regs.write(m_activeAddress, payload);
              break;
            case ADDR_CTRL_HUM:
              m_regs.write(m_activeAddress, payload);
              break;
            case ADDR_CTRL_MEAS:
              m_regs.write(m_activeAddress, payload);
              m_modeUpdateEvent.notify(SC_ZERO_TIME);
              break;
            case ADDR_RESET:
              if (payload == 0xB6) {  // Magic number to trigger reset
                reset();
              } else {
                // otherwise, ignore
              }
              break;
            default:
              SC_REPORT_FATAL(
                  this->name(),
                  fmt::format("Invalid write access to address 0x{:08x}, which "
                              "is not a writeable register.",
                              m_activeAddress)
                      .c_str());
              break;
          }
          m_spiState = SpiState::Address;  // Next payload will be an address
        }
        break;
    }

    // Prepare response
    if ((!m_isWriteAccess) && m_regs.contains(m_activeAddress)) {
      // Valid read-access
      spdlog::info("{:s}: setting response to regs[0x{:02x}] = 0x{:2x}",
                   this->name(), m_activeAddress, m_regs.read(m_activeAddress));
      writeSlaveOut(m_regs.read(m_activeAddress));
      ++m_activeAddress;  // Auto-increment address
    } else {              // Write access, or invalid address
      writeSlaveOut(0);
    }
  } else if (!enabled()) {  // Chip select inactive
    m_spiState = SpiState::Address;
  }
}

void Bme280::measurementLoop() {
  wait(m_modeUpdateEvent);

  while (1) {
    // State machine model
    auto mode = m_regs.read(ADDR_CTRL_MEAS) & 0b11u;
    switch (m_measurementState) {
      case MeasurementState::PowerOff:
        // Check VDD and VDDIO
        break;
      case MeasurementState::Sleep:
        if (mode == 1 || mode == 2) {
          m_measurementState = MeasurementState::Forced;
        } else if (mode == 3) {
          m_measurementState = MeasurementState::Normal;
        }
        break;
      case MeasurementState::Normal:
        if ((mode == 0) || (mode == 0)) {
          m_measurementState == MeasurementState::Sleep;
        }
        break;
      case MeasurementState::Forced:
        m_measurementState == MeasurementState::Sleep;
        break;
    }

    // Take a series of measurements
    if (m_measurementState == MeasurementState::Normal ||
        m_measurementState == MeasurementState::Forced) {
      unsigned result = 0;
      wait(sc_time(1, SC_MS));  // Constant part of t_measure (datasheet)

      // Lambda for calculating oversampling factor
      auto nSamples = [](unsigned samplingFactor) -> unsigned {
        if (samplingFactor == 0) {
          return 0u;
        } else if (samplingFactor < 5) {
          return 1u << (samplingFactor - 1);
        } else {
          return 16u;
        }
      };

      // Lambda for running iir filter
      unsigned filter_coeff = nSamples((m_regs.read(ADDR_CONFIG) & 0x1c) >> 2);
      auto iir = [filter_coeff](unsigned oldval, unsigned newval) -> unsigned {
        if (filter_coeff == 0) {
          return newval;
        } else {
          return (oldval * (filter_coeff - 1) + newval) / filter_coeff;
        }
      };

      m_regs.setBit(ADDR_STATUS, 3, true);  // Indicate measurement
      spdlog::info("{:s}: @{} starting measurement", this->name(),
                   sc_time_stamp().to_string());

      // Measure temperature
      auto osrs_t = (m_regs.read(ADDR_CTRL_MEAS) & 0xe0) >> 5;
      if (osrs_t == 0) {
        result = 0x8000;
      } else {
        for (int i = 0; i < nSamples(osrs_t); ++i) {
          spdlog::info("{:s}: @{} Temperature sample = {}", this->name(),
                       sc_time_stamp().to_string());
          result += 300;            // Should be read from csv
          wait(sc_time(2, SC_MS));  // Sampling time
        }
        result /= nSamples(osrs_t);  // Average of oversampling
        auto oldval = m_regs.read(ADDR_TEMP_XLSB) |
                      (m_regs.read(ADDR_TEMP_LSB) << 4) |
                      (m_regs.read(ADDR_TEMP_MSB) << 12);
        result = iir(oldval, result);
      }
      m_regs.write(ADDR_TEMP_XLSB, result & 0x0f, true);
      m_regs.write(ADDR_TEMP_LSB, (result & 0x0ff0) >> 4, true);
      m_regs.write(ADDR_TEMP_MSB, (result & 0xff000) >> 12, true);

      // Measure pressure
      auto osrs_p = (m_regs.read(ADDR_CTRL_MEAS) & 0x1c) >> 2;
      if (osrs_p == 0) {
        result = 0x8000;
      } else {
        result = 0;
        for (int i = 0; i < nSamples(osrs_p); ++i) {
          spdlog::info("{:s}: @{} Pressure sample", this->name(),
                       sc_time_stamp().to_string());
          result += 300;            // Should be read from csv
          wait(sc_time(2, SC_MS));  // Sampling time
        }
        wait(sc_time(0.5, SC_MS));   // Constant part of pressure sampling time
        result /= nSamples(osrs_p);  // Average of oversampling
        auto oldval = m_regs.read(ADDR_PRESS_XLSB) |
                      (m_regs.read(ADDR_PRESS_LSB) << 4) |
                      (m_regs.read(ADDR_PRESS_MSB) << 12);
        result = iir(oldval, result);
      }
      m_regs.write(ADDR_PRESS_XLSB, result & 0x0f, true);
      m_regs.write(ADDR_PRESS_LSB, (result & 0x0ff0) >> 4, true);
      m_regs.write(ADDR_PRESS_MSB, (result & 0xff000) >> 12, true);

      // Measure humidity
      auto osrs_h = m_regs.read(ADDR_CTRL_HUM);
      if (osrs_h == 0) {
        result = 0x8000;
      } else {
        for (int i = 0; i < nSamples(osrs_h); ++i) {
          spdlog::info("{:s}: @{} Humidity sample", this->name(),
                       sc_time_stamp().to_string());
          unsigned newval = 300;    // Should read from csv
          wait(sc_time(2, SC_MS));  // Sampling time
          // no iir for humidity
        }
        wait(sc_time(0.5, SC_MS));  // Constant part of humidity sampling time
      }
      // Store result
      m_regs.write(ADDR_HUM_LSB, result & 0xff, true);
      m_regs.write(ADDR_HUM_MSB, (result & 0xff00) >> 8, true);
      m_regs.clearBit(ADDR_STATUS, 3, true);

      // Standby time
      if (m_measurementState == MeasurementState::Normal) {
        const int STANDBY_DELAY_US[] = {500,    62500,   125000, 250000,
                                        500000, 1000000, 10000,  20000};
        wait(sc_time(STANDBY_DELAY_US[(m_regs.read(ADDR_CONFIG) & 0xe0) >> 5],
                     SC_US));
      }
    } else {  // Inactive
      wait(m_modeUpdateEvent);
    }
  }
}

// Override parent b_transport to check for compatible spi settings
