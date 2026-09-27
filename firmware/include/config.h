#pragma once

// lite-brite build configuration.
//
// Every setting here is a default: override any of them for a board with a
// -D flag in platformio.ini (build_flags), e.g. -D LB_PIN_PIR=7.
// Things Home Assistant can change at runtime (brightness, speed, ...) are
// not here; they arrive over MQTT.

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "include/secrets.h is missing: using placeholder Wi-Fi/MQTT settings. Copy secrets.example.h to secrets.h."
#include "secrets.example.h"
#endif

// ---- Identity ---------------------------------------------------------------
#ifndef LB_DEVICE_ID
#define LB_DEVICE_ID "entryway"  // MQTT topics are lite-brite/<id>/...
#endif
#ifndef LB_DEVICE_NAME
#define LB_DEVICE_NAME "Entryway sign"  // device name in Home Assistant
#endif
#ifndef LB_TOPIC_PREFIX
#define LB_TOPIC_PREFIX "lite-brite"
#endif
#ifndef LB_DISCOVERY_PREFIX
#define LB_DISCOVERY_PREFIX "homeassistant"
#endif
#ifndef LB_BOARD_NAME
#define LB_BOARD_NAME "ESP32-S3"
#endif

// ---- LED matrix -------------------------------------------------------------
// Default: 8x32 WS2812B flexible panel, columns zig-zagging from the top-left.
// Chain panels side by side by increasing the width. If text looks mirrored,
// upside down or scrambled, press "Wiring test" in Home Assistant and adjust.
#ifndef LB_MATRIX_WIDTH
#define LB_MATRIX_WIDTH 32
#endif
#ifndef LB_MATRIX_HEIGHT
#define LB_MATRIX_HEIGHT 8
#endif
#ifndef LB_MATRIX_COLUMN_MAJOR
#define LB_MATRIX_COLUMN_MAJOR 1
#endif
#ifndef LB_MATRIX_SERPENTINE
#define LB_MATRIX_SERPENTINE 1
#endif
#ifndef LB_MATRIX_FLIP_X
#define LB_MATRIX_FLIP_X 0
#endif
#ifndef LB_MATRIX_FLIP_Y
#define LB_MATRIX_FLIP_Y 0
#endif
#ifndef LB_LED_COLOR_ORDER
#define LB_LED_COLOR_ORDER GRB  // RGB, RBG, GRB, GBR, BRG or BGR
#endif

// Power limiter: estimated LED current never exceeds LB_LED_MAX_MA (and less
// on a low battery). The model is conservative for WS2812B.
#ifndef LB_LED_MAX_MA
#define LB_LED_MAX_MA 1200
#endif
#ifndef LB_LED_MA_PER_CHANNEL
#define LB_LED_MA_PER_CHANNEL 16
#endif
#ifndef LB_LED_IDLE_UA
#define LB_LED_IDLE_UA 700
#endif

// ---- Pins -------------------------------------------------------------------
// GPIO numbers. Defaults match the wiring guide for the Seeed XIAO ESP32-S3
// (docs/hardware.md). Use -1 for anything that isn't fitted.
#ifndef LB_PIN_LED_DATA
#define LB_PIN_LED_DATA 4  // XIAO D3
#endif
#ifndef LB_PIN_LED_POWER
#define LB_PIN_LED_POWER 5  // XIAO D4: gate of the LED power switch
#endif
#ifndef LB_LED_POWER_ACTIVE_HIGH
#define LB_LED_POWER_ACTIVE_HIGH 1
#endif
#ifndef LB_PIN_PIR
#define LB_PIN_PIR 1  // XIAO D0. Must be an RTC GPIO (0-21 on ESP32-S3) to wake the chip.
#endif
#ifndef LB_PIR_ACTIVE_HIGH
#define LB_PIR_ACTIVE_HIGH 1
#endif
#ifndef LB_PIN_VBAT
#define LB_PIN_VBAT 2  // XIAO D1: battery voltage through a divider
#endif
#ifndef LB_VBAT_DIVIDER
#define LB_VBAT_DIVIDER 2.0f  // battery volts = pin volts * this (two equal resistors = 2.0)
#endif
#ifndef LB_PIN_VBUS
#define LB_PIN_VBUS -1  // optional: reads HIGH while USB power is connected
#endif

// ---- Power behaviour --------------------------------------------------------
#ifndef LB_ALWAYS_POWERED
#define LB_ALWAYS_POWERED 0  // 1 = mains/USB powered: never deep sleep
#endif
#ifndef LB_BATT_LOW_MV
#define LB_BATT_LOW_MV 3700
#endif
#ifndef LB_BATT_CRITICAL_MV
#define LB_BATT_CRITICAL_MV 3500
#endif
#ifndef LB_BATT_EMPTY_MV
#define LB_BATT_EMPTY_MV 3350
#endif
#ifndef LB_WIFI_TIMEOUT_MS
#define LB_WIFI_TIMEOUT_MS 10000
#endif
#ifndef LB_MAINTENANCE_MAX_MIN
#define LB_MAINTENANCE_MAX_MIN 30  // "Stay awake" turns itself off after this long on battery
#endif
#ifndef LB_CPU_MHZ_IDLE
#define LB_CPU_MHZ_IDLE 80  // CPU clock once connected (Wi-Fi needs >= 80)
#endif
#ifndef LB_NTP_SERVER
#define LB_NTP_SERVER "pool.ntp.org"
#endif
#ifndef LB_DEBUG
#define LB_DEBUG 1  // serial logging at 115200 baud
#endif
