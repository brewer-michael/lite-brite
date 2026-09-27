# First power-up checklist

The firmware has been built and its logic unit-tested, but this is the first
time it meets real hardware. These steps check one piece at a time, so if
something is off you know which piece.

## 1. Board only (USB, no panel, no battery)

1. Copy `firmware/include/secrets.example.h` to `secrets.h` and fill it in.
2. `cd firmware && pio run -t upload && pio device monitor`
3. The log should show `Wi-Fi up`, `MQTT connected`, `synced` and
   `published Home Assistant discovery`.
4. In Home Assistant, the **Entryway sign** device appears under
   Settings → Devices & services → MQTT.

If Wi-Fi or MQTT fails, the log says which. The sign also retries on every wake.

## 2. Motion sensor

Wire the PIR (D0/GPIO1). Watch the **Motion** entity in Home Assistant while
the sign is awake (for the first minute after a reset, or with **Stay awake**
on) and wave at the sensor. It should turn on within a second or two.

## 3. Battery reading

Connect the battery and divider. Compare the **Battery voltage** sensor with a
multimeter across the battery. If it's off by more than a few percent, adjust
`LB_VBAT_DIVIDER` in `platformio.ini` (measured ÷ reported × 2.0).

## 4. LED panel

Wire the switch and panel (docs/hardware.md), then:

1. Enable and press **Wiring test** (a diagnostic button on the device page).
   Red should be top-left, green top-right, blue bottom-left, with a white dot
   walking the rows. Fix mirroring, rotation or color order with the flags in
   the hardware guide.
2. Press **Show test message**: a rainbow "Hello from lite-brite!" flashes
   and scrolls.
3. While nothing is showing, the panel must be unpowered: measure 0 V between
   the panel's power wires. If it's at battery voltage, the switch is wired
   wrong, and the sign will drain the battery in about a day.

## 5. Sleep current

With everything connected and the sign asleep (**Awake** is off), measure the
current with a multimeter in series with the battery. Expect roughly
35-60 µA. Much more than that usually means the panel isn't switched off, the
PIR isn't a low-power type, or something is holding the board awake (check
the **Awake** sensor and the **Stay awake** switch).

## 6. Wake on motion

With the sign asleep, publish a test message and walk up to it:

```sh
mosquitto_pub -h <broker> -u <user> -P <pass> -r -t lite-brite/entryway/msg/test \
  -m '{"text": "Hello! :smile:", "effect": "flash"}'
```

It should appear within about 2-3 seconds of the sensor seeing you, then
clear itself. A **static IP** in `secrets.h` shortens the delay.

## 7. The delivery test

Run `script.lite_brite_show` from Developer tools → Actions with slot `keys`
and her reminder text, walk in, and check it shows. Then set up the arrival
automation (homeassistant/README.md) and try it for real when she gets home.

Once it works, note what you measured (sleep current, how long a message
takes to appear) in `docs/power.md`, replacing the estimates there.
