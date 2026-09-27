# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build, flash and test

The firmware is a PlatformIO project in `firmware/` (run `pio` commands from there). It uses the pioarduino platform (Arduino-ESP32 3.x on ESP-IDF 5.5); ArduinoJson is the only external library.

- `pio test -e native`: unit tests for `lib/litebrite_core` on the host (Unity). Run after any core change.
- `pio run -e xiao_esp32s3`: firmware for the reference board. `esp32s3_devkitc` is a USB-powered build that never sleeps. Build both after changing anything in `src/`, which has no unit tests.
- `pio run -t upload`, `pio device monitor`: flash over USB, serial log at 115200.
- `pio run -e xiao_esp32s3_ota -t upload`: over-the-air update. Needs the sign's "Stay awake" switch on in Home Assistant and `LB_OTA_PASSWORD` in the environment.
- `pio run -e sim`, then from the repo root `python3 tools/preview.py --out x.gif '<payload>'`: render messages to a GIF with the firmware's own drawing code (needs Pillow).
- `python3 tools/gen_assets.py`: regenerate the font and icon tables after editing `firmware/assets/*.txt`. CI runs it with `--check`.
- `python3 tools/check_ha_yaml.py`: checks that the Home Assistant YAML parses.
- Credentials: copy `firmware/include/secrets.example.h` to `secrets.h` (git-ignored). Builds without it use placeholders and print a warning.

## Architecture

- **Hardware:** Seeed XIAO ESP32-S3, 8×32 WS2812B panel, AM312 PIR, 1S Li-ion. The panel runs from the battery through a MOSFET high-side switch (GPIO5); data goes out on GPIO4 via RMT. The PIR is on GPIO1 (ext0 deep-sleep wake) and the battery divider on GPIO2. Pins, panel geometry and power thresholds are defaults in `firmware/include/config.h`. See `docs/hardware.md`.
- **Core vs device:** `firmware/lib/litebrite_core` is portable C++17 with no Arduino/IDF includes: message parsing, queue, font/icons, text layout with `:icon:`/`{color}` markup, playback timeline and effects, gamma/brightness/power limiting, battery model, sleep policy, Home Assistant discovery JSON. `firmware/src/` is ESP32-only and kept thin: `App.cpp` (state machine), `hw/` (LED strip, board I/O, deep sleep), `net/` (Wi-Fi, NTP, esp-mqtt wrapper), `sim/` (desktop simulator). Put logic in the core, where it can be tested.
- **Battery strategy:** the sign deep-sleeps and wakes on the PIR or an hourly timer. Home Assistant leaves retained MQTT messages per slot (`lite-brite/<id>/msg/<slot>`). On wake the sign reconnects using a cached BSSID/channel, subscribes, and publishes a sync token to itself; the token's echo means all retained state has arrived. It shows messages only when someone is present (motion in the last 30 s) unless a message says `when: now`. `once` messages are cleared with an empty retained publish, and QoS 1 acks are awaited before sleeping. It lingers after the last motion, then sleeps. The LED supply is off except while showing. See `docs/power.md`.
- **Home Assistant:** MQTT device discovery (one retained config message). Settings are retained `config/<key>` topics backing number/switch entities, so a sleeping sign still gets changes. The scripts, blueprint and delivery-test automation are in `homeassistant/`. The payload format is documented in `docs/mqtt-api.md`; keep the docs, the scripts and `Message.cpp` in sync.

Gotchas:
- On Xtensa `uint32_t` is `unsigned long`, so `std::min`/`std::max` with mixed types compile on the host but fail on the ESP32. Give explicit template arguments.
- `RtcState` in `App.cpp` lives in `RTC_NOINIT_ATTR` memory and must stay a trivial type (no constructors or default member initializers), validated by a magic number.
- Rendering is a pure function of time. The app renders at scheduled frame times, not `millis()`, so scroll steps stay even; frame intervals divide the scroll step exactly.

## What this is

"lite-brite" is a small battery-powered color LED sign that shows instructions when someone arrives home. It will be about 4" across, or a longer, stock-ticker-style scrolling strip if that reads better.

Hard requirements:
- **Wi-Fi** connectivity.
- **Home Assistant integration.** HA decides when to show a message and what it says. The sign should be controllable from HA automations (for example, triggered by a person's presence changing to `home`).
- **Battery powered.** This makes power draw a first-class design constraint. Always-on Wi-Fi and bright full-color LEDs both drain a battery quickly, so weigh any design choice against battery life.
- **Large, friendly, bright, color text** that's readable at a glance from across an entryway.

## Delivery test (acceptance condition)

When the user's wife arrives home, the sign must flash a large, bright, friendly message telling her to put her keys away first.

The sign will support other, similar arrival or prompt messages later. Keep message content and triggers configurable from Home Assistant rather than hard-coded in the firmware.
