# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project status

This is a new project. There's no code yet and no toolchain has been chosen. When the hardware platform, firmware framework, and build tooling are decided, replace this section with the real build, flash, and test commands and a short architecture overview.

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
