# Power and battery life

Battery power drives most of the design. Two things drain a battery fast:
Wi-Fi left on, and LEDs left lit. The sign avoids both.

## The idea: only be awake when someone is there

A sign in an entryway only matters when someone is standing in front of it.
So the sign spends nearly all its time in deep sleep, drawing about as much as
the battery loses to self-discharge. A PIR motion sensor, which draws about
15 µA, stays powered and wakes the chip when someone walks up.

Home Assistant never needs to reach the sign directly. It leaves messages on
the MQTT broker as **retained** messages, which wait there. When the sign
wakes it connects, collects whatever is waiting, shows it, and goes back to
sleep. A message published hours earlier is shown the moment someone walks in.

```
 HA automation ──publish (retained)──► broker ◄──── collect on wake ──── sign
 "she's home"                          waits                  motion sensor fires
```

It also wakes on a timer (hourly by default) to report its battery level and
pick up setting changes, then sleeps again straight away.

For comparison, keeping Wi-Fi connected all the time costs 30-80 mA on
average. A 5000 mAh battery would last days, not months.

## Where the current goes

All figures are estimates from datasheets and typical measurements, not
measurements of this build. Measure yours with a USB power meter or a
multimeter in series with the battery; the settings below let you tune it.

| State | Draw | Notes |
| --- | --- | --- |
| Deep sleep | ~35 µA | XIAO ESP32-S3 ~14 µA, AM312 PIR ~15 µA, battery divider ~5 µA, switch leakage |
| Waking and connecting | ~100 mA for ~1.5 s | Faster with a static IP; the cached access point skips the Wi-Fi scan |
| Listening after motion | ~30 mA | CPU at 80 MHz, Wi-Fi in modem-sleep between beacons |
| Showing a message | ~0.5 A | 256 LEDs idle at ~0.7 mA each (~180 mA) plus the lit text |

The LED panel's supply is cut completely when nothing is on screen:
WS2812B LEDs draw ~0.7 mA each even when dark, which would flatten a battery
in about a day on its own.

A 30-second message costs about 4.5 mAh at the default brightness. A walk-by
with nothing to show costs about 0.2 mAh, almost all of it the 20 seconds
spent listening in case Home Assistant is about to send something.

## Estimated battery life

| Day | Walk-bys | Messages shown | Per day | 3000 mAh | 5000 mAh |
| --- | --- | --- | --- | --- | --- |
| Quiet | 10 | 1 | ~8 mAh | ~10 months | ~16 months |
| Typical | 30 | 2 | ~17 mAh | ~5 months | ~8 months |
| Busy | 60 | 4 | ~32 mAh | ~2.5 months | ~4.5 months |

(85 % of rated capacity counted as usable; hourly check-ins included.)

## Tuning

All from the sign's device page in Home Assistant:

| Setting | Default | Effect on battery |
| --- | --- | --- |
| Brightness | 50 % | The biggest lever while a message is showing. The scale is perceptual: 30 % still reads well indoors and uses about a third of the text current. |
| Message duration | 30 s | Display time is the biggest cost per message. Long messages always finish their scroll pass. |
| Listen after motion | 20 s | How long the sign stays connected after someone walks past, in case Home Assistant is a few seconds late. Shorter saves the most on a busy hallway. |
| Check-in interval | 60 min | How often it reports the battery. Longer saves a little; settings changes also wait longer. |

Other things that help:

- **Static IP** (`LB_STATIC_IP` in `secrets.h`): skips DHCP, so every wake
  is shorter.
- **Broker by IP address**, not a `.local` name.
- **PIR placement**: aim it at the doorway, not at a busy hallway or a
  heating vent, and keep it away from the Wi-Fi antenna. False triggers are
  the hidden cost.

## Low battery

The firmware reads the battery at every wake, before the radio starts, and
reports percentage and voltage to Home Assistant:

| Resting voltage | Level | Behavior |
| --- | --- | --- |
| ≥ 3.70 V | OK | Normal |
| < 3.70 V | Low | After showing messages, blinks a red battery icon; LED current is tapered down from 3.8 V |
| < 3.50 V | Critical | Stops lighting the LEDs (they can't show blue/green properly this low), stops waking on motion, reports every 4 hours |
| < 3.35 V | Empty | Reports every 12 hours to protect the cell |

Messages keep waiting in Home Assistant's broker while the battery is
critical, so nothing is lost; they show once the sign is charged. The
thresholds are in `firmware/include/config.h`.

A power limiter also caps the estimated LED current (1.2 A by default,
`LB_LED_MAX_MA`), so a full-screen flash can't brown out the battery.

## Plugged in

When the sign runs on USB or mains power (a board with USB sensing wired to
`LB_PIN_VBUS`, or a build with `LB_ALWAYS_POWERED=1`), it stays connected and
shows messages the moment someone walks up, with no connection delay. The
"Stay awake" switch in Home Assistant does the same temporarily on battery
(for firmware updates) and turns itself off after 30 minutes.
