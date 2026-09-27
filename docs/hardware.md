# Hardware

The reference build is a 320 × 80 mm ticker: an 8 × 32 WS2812B LED panel on
a small frame, with a Seeed Studio XIAO ESP32-S3, a PIR motion sensor and a
single-cell lithium battery behind it.

## Why a ticker and not a 4" square

The goal is text you can read at a glance from across an entryway. On a
32-pixel-wide, 10 mm-pitch panel the 8-pixel font makes letters about 7 cm
tall, readable from well over 5 m, with four to six letters on screen and
the rest scrolling past like a stock ticker.

A 4" (10 cm) square panel is 16 × 16 pixels at about 6 mm pitch. It would
show two or three letters at a time, 4 cm tall, which is hard to read as it
scrolls. The firmware supports other sizes (see [Other panels](#other-panels)),
but the 8 × 32 strip reads much better.

## Parts

| Part | Example | Why |
| --- | --- | --- |
| Microcontroller | Seeed Studio XIAO ESP32-S3 | Wi-Fi, USB-C, built-in LiPo charger, low deep-sleep current |
| LED panel | 8 × 32 WS2812B flexible matrix, 10 mm pitch (320 × 80 mm) | Bright, readable, cheap, one data wire |
| Motion sensor | AM312 mini PIR | Runs on 3.3 V and draws about 15 µA, so it can stay on while the sign sleeps |
| Battery | 1S Li-ion/LiPo **with protection circuit**, 3000-6000 mAh | e.g. two 18650 cells in a protected parallel holder, or a flat 5000 mAh pouch |
| LED power switch | AO3401A P-MOSFET + 2N7002 N-MOSFET (SOT-23) | The panel draws ~180 mA even when dark, so it must be switched off |
| Resistors | 2 × 100 kΩ, 10 kΩ, 470 Ω, 2 × 470 kΩ | Switch biasing, data line, battery divider |
| Capacitors | 1000 µF 6.3 V electrolytic, 3 × 100 nF | Panel supply reservoir, soft-start, PIR and ADC decoupling |
| Optional | Slide switch in the battery lead, smoked or frosted acrylic diffuser | Storage/shipping; much nicer-looking text |

Any 1S lithium cell works; bigger just lasts longer (see [power.md](power.md)).
Use a cell with a built-in protection circuit (PCM): it cuts off at a
safe voltage if the firmware's own low-battery handling is ever bypassed.

## Wiring

GPIO numbers are the firmware defaults; change them in
`firmware/include/config.h` (or with `-D` flags in `platformio.ini`) if you
wire differently. The motion sensor must be on an RTC-capable GPIO (0-21 on
the ESP32-S3) to wake the chip from deep sleep.

| XIAO pin | GPIO | Goes to |
| --- | --- | --- |
| D0 | 1 | PIR `OUT` |
| D1 | 2 | Battery divider midpoint |
| D3 | 4 | 470 Ω resistor → panel `DIN` |
| D4 | 5 | LED switch control (2N7002 gate) |
| 3V3 | | PIR `VCC` |
| GND | | PIR `GND`, divider bottom, 2N7002 source, panel `GND` |
| BAT+ pad (underside) | | Battery +, AO3401A source, divider top |
| BAT− pad (underside) | | Battery − |

```
 BAT+ ──────┬──────────┬───────────────────┐
            │          │                   │ S
          100 kΩ     100 nF          ┌─────┴─────┐
            │          │             │  AO3401A  │  P-MOSFET
            └────┬─────┘             │           │
                 ├────────────── G ──┤           │
               10 kΩ                 └─────┬─────┘
                 │ D                       │ D
           ┌─────┴─────┐                   ├──────────► panel +V   (red)
 D4 ── G ──┤  2N7002   │                1000 µF
 (GPIO5)   └─────┬─────┘                   │
    │            │ S                      GND ────────► panel GND  (white)
  100 kΩ        GND
    │
   GND

 D3 (GPIO4) ── 470 Ω ─────────────────────────────────► panel DIN  (green)

 BAT+ ── 470 kΩ ──┬── 470 kΩ ── GND       3V3 ──┬───── PIR VCC
                  │                             │
             D1 (GPIO2)                      100 nF
                  │                             │
                100 nF ── GND             GND ──┴───── PIR GND
                                          D0 (GPIO1) ─ PIR OUT
```

How the switch works: D4 low (or floating while the chip sleeps, thanks to
the 100 kΩ pull-down) keeps the 2N7002 off, so the 100 kΩ pull-up holds the
AO3401A's gate at the battery voltage and the panel is unpowered. D4 high
pulls the gate down through the 10 kΩ and the panel switches on. The 100 nF
across gate and source slows the switch-on to about a millisecond, so
charging the 1000 µF reservoir doesn't make the battery voltage dip and reset
the microcontroller.

The panel runs straight from the battery (3.3-4.2 V). WS2812B LEDs work down
to about 3.5 V, and with a supply at battery voltage the ESP32's 3.3 V data
signal is a valid logic high without a level shifter. Below 3.5 V blue and
green get dim; the firmware stops lighting the panel there anyway (see
[power.md](power.md)).

Notes:

- Most 8 × 32 panels have power wires at both ends. Connect both ends to the
  switched supply for even brightness.
- Keep the data wire short, and put the 470 Ω resistor near the panel.
- Mount the PIR looking at the door, at least 10 cm from the XIAO's antenna:
  cheap PIR sensors can false-trigger on Wi-Fi transmissions. The firmware
  copes with a PIR that stays triggered, but every false trigger costs battery.
- The XIAO keeps its 3.3 V regulator on in deep sleep, which is what powers
  the PIR.
- The XIAO's built-in charger is low-current. Charging a big pack from empty
  can take a day or more through its USB-C port; use an external charger for
  the cells if that's a problem.

## Assembly tips

- A diffuser makes the biggest difference to how friendly the sign looks: a
  sheet of smoked or frosted acrylic 5-10 mm in front of the LEDs blends the
  dots into smooth letters. Smoked acrylic also gives a black background.
- Mount the sign at eye level where someone walking in faces it. The PIR's
  field of view (about 100°, 3-5 m) should cover the doorway.
- Power up with USB connected and open the serial monitor
  (`pio device monitor`) to watch the first connection. Without a computer,
  the sign shows a green ✓ when it has connected, or a red Wi-Fi symbol / "No
  MQTT" if it couldn't.
- The XIAO's USB serial port disappears whenever the chip sleeps. After
  power-on the sign stays awake for a minute; for longer debugging sessions
  turn on **Stay awake** in Home Assistant, or temporarily add
  `-D LB_ALWAYS_POWERED=1` to the environment's `build_flags` so it never
  sleeps.

## Checking the panel wiring

Panels differ in which corner the data enters and which way the rows or
columns zig-zag. Press **Wiring test** in Home Assistant (a disabled-by-default
diagnostic button; enable it on the device page). The top-left LED lights
red, top-right green, bottom-left blue, and a white dot walks along each row
left to right, top to bottom. If it doesn't look like that:

| What you see | Fix (in `platformio.ini` `build_flags`) |
| --- | --- |
| Red is top-right (mirrored) | `-D LB_MATRIX_FLIP_X=1` |
| Red is bottom-left (upside down) | `-D LB_MATRIX_FLIP_Y=1` |
| The dot jumps around | Toggle `LB_MATRIX_SERPENTINE` or `LB_MATRIX_COLUMN_MAJOR` |
| Colors are wrong (red shows green) | `-D LB_LED_COLOR_ORDER=RGB` (or another order) |

## Other panels

- **Longer tickers:** chain a second 8 × 32 panel after the first and set
  `-D LB_MATRIX_WIDTH=64`. Raise `LB_LED_MAX_MA` only if your battery and
  switch can supply it.
- **Taller panels:** 16-row panels double the font size automatically
  (`-D LB_MATRIX_HEIGHT=16`).
- **Mains or USB power:** build the `esp32s3_devkitc` environment (or add
  `-D LB_ALWAYS_POWERED=1`) and the sign never sleeps. If you then run the
  panel from 5 V, add a 74AHCT125 level shifter on the data line.
- **Other boards:** any ESP32-S3 works. Set the pins, and `-D LB_PIN_VBAT=-1`
  if you don't have a battery divider. ESP32-C3/C6 boards should work too but
  haven't been tried; their motion wake uses a different GPIO mechanism
  (already handled in `firmware/src/hw/Board.cpp`).
