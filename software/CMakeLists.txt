# SPDX-License-Identifier: Apache-2.0

set(BOARD esp32)
set(CONF_FILE prj.conf)
set(DTC_OVERLAY_FILE esp32.overlay)

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(soil_respiration_2023)

include_directories(
                      inc/
                      lib/
                      )

target_sources(app PRIVATE
    src/main.c
    inc/wifi.c
    inc/mqtt.c
    #inc/sockets.c
    inc/sensor.c
    lib/scd30.c
    lib/sensirion_common.c
    inc/ota.c
    inc/motor.c
    inc/ble.c
)
