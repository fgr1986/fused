﻿#
# Copyright (c) 2019-2020, University of Southampton and Contributors.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.12)

include(cmake/Utilities.cmake)

option(INSTALL_DEPENDENCIES "Download, build & install dependencies only" OFF)
option(ENABLE_TESTS "Build tests" ON)
option(GDB_SERVER "Link gdb server library" ON)
option(INSTALL_TARGET_TOOLCHAINS "Download & install target toolchains" ON)

set(EP_INSTALL_DIR $ENV{HOME}/.local CACHE STRING
                "Installation directory for dependencies")

IF(INSTALL_DEPENDENCIES)
  project(INSTALL_DEPENDENCIES NONE)
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  include(cmake/InstallDependencies.cmake)
  return()
ELSE()
  project(fused)
ENDIF()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#Enable  sanitizer on debug build
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_RELEASE}
#       -march=native -fsanitize=address,undefined,leak")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS}
  -Wall -Wextra -Wno-reorder -Wno-unused -Wno-sign-compare")

configure_file(config/config.yaml.in config.yaml)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_EXE_LINKER_FLAGS -static)

LIST(APPEND CMAKE_PREFIX_PATH ${EP_INSTALL_DIR})
find_package(spdlog REQUIRED )
find_package(ihex-parser "0.2.0" REQUIRED)
find_package(gdb-server REQUIRED)
find_package(yaml-cpp REQUIRED)

include_directories(.)
link_directories(${EP_INSTALL_DIR}/lib)
include_directories(${EP_INSTALL_DIR}/include)

add_subdirectory(mcu)
add_subdirectory(ps)
add_subdirectory(utilities)
add_subdirectory(sd)
add_subdirectory(boards)

add_executable(
  fused
  main.cpp
)

target_link_libraries(
  fused
  PRIVATE
    Cm0TestBoard
    Cm0SensorNode
    Msp430TestBoard
    IntelHexParser::ihex-parser
)

IF(GDB_SERVER)
  target_link_libraries(fused PRIVATE gdb-server::gdb-server)
  target_compile_definitions(fused PRIVATE GDB_SERVER)
ENDIF()

IF(ENABLE_TESTS)
  enable_testing()

  # Extra command to check outputs of failing tests
  add_custom_target(testcheck COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)

  # ---- "unit-tests" ------
  add_subdirectory(test)
  add_test(NAME PowerModelChannel COMMAND testPowerModelChannel)
  add_test(NAME ClockSourceChannel COMMAND testClockSourceChannel)
  add_test(NAME Cm0RegisterFile COMMAND testCm0RegisterFile)
  add_test(NAME Msp430RegisterFile COMMAND testMsp430RegisterFile)
  add_test(NAME Accelerometer COMMAND testAccelerometer)
  add_test(NAME Bme280 COMMAND testBme280)
  add_test(NAME Nrf24Radio COMMAND testNrf24Radio)
  add_test(NAME DigitalIo COMMAND testDigitalIo)
  add_test(NAME Msp430fr5xxCpu COMMAND testMsp430fr5xxCpu)
  add_test(NAME Msp430Cache COMMAND testMsp430Cache)
  add_test(NAME Msp430fr5xxClockSystem COMMAND testMsp430fr5xxClockSystem)
  add_test(NAME Msp430fr5xxTimerA COMMAND testMsp430fr5xxTimerA)
  add_test(NAME Msp430fr5xxeUsciB COMMAND testMsp430fr5xxeUsciB)
  add_test(NAME Msp430fr5xxDma COMMAND testMsp430fr5xxDma)
  add_test(NAME Cm0SysTick COMMAND testCm0SysTick)
  add_test(NAME Cm0Nvic COMMAND testCm0Nvic)
  add_test(NAME Cm0Spi COMMAND testCm0Spi)
  add_test(NAME Cm0Dma COMMAND testCm0Dma)

  # ---- Software tests ------
  execute_process(
    COMMAND ./buildall.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/sw
    OUTPUT_FILE ${CMAKE_BINARY_DIR}/buildall.log
    ERROR_FILE ${CMAKE_BINARY_DIR}/buildall.log
  )

  # Add sw/validation tests
  add_sw_tests(Msp430TestBoard)
  add_sw_tests(Cm0TestBoard)
  add_sw_tests(Cm0SensorNode)
ENDIF()
