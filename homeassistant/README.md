# Home Assistant setup

The sign talks to Home Assistant over MQTT. Home Assistant decides when a
message should appear and what it says; the sign only displays what it's given.

## 1. MQTT

You need an MQTT broker and Home Assistant's MQTT integration. The easy route:

1. Settings → Add-ons → install **Mosquitto broker** and start it.
2. Settings → People → Users: add a user for the sign (e.g. `lite-brite`).
   The Mosquitto add-on accepts Home Assistant users as MQTT logins.
3. Settings → Devices & services: the MQTT integration is discovered; accept it.

Put the broker's IP address and that login in `firmware/include/secrets.h`.

Home Assistant 2024.12 or newer is needed (the sign uses MQTT *device*
discovery, which registers the sign and all of its entities in one message).

## 2. The sign shows up by itself

When the sign powers on it publishes a discovery message, and the **Entryway
sign** device appears under Settings → Devices & services → MQTT with:

| Entity | What it does |
| --- | --- |
| Message (text) | Type something; it shows the next time someone walks up, then the box empties |
| Waiting messages | How many messages are waiting |
| Battery, Battery voltage | Reported at every wake (hourly at least) |
| Motion | The sign's motion sensor, handy for other automations too |
| Message (event) | Fires `shown`, `expired` or `invalid` with the slot and text |
| Clear messages, Show test message | Buttons |
| Brightness, Scroll speed, Message duration, Listen after motion, Check-in interval | Settings |
| Stay awake | Keeps the sign online (for firmware updates); turns itself off after 30 minutes on battery |
| Awake, Wi-Fi signal, Last wake reason, Wiring test, Restart | Diagnostics |

The sign is asleep most of the time. Changes you make while it sleeps are
kept by the broker and picked up at the next wake, so the controls always work.
The **Awake** sensor shows when it's actually online.

## 3. Scripts

Copy [`packages/lite_brite.yaml`](packages/lite_brite.yaml) to your
`config/packages/` folder and make sure `configuration.yaml` has:

```yaml
homeassistant:
  packages: !include_dir_named packages
```

Restart. You now have `script.lite_brite_show` and `script.lite_brite_clear`,
usable from any automation with friendly fields (message, color, effect,
expiry, ...). Try one from Developer tools → Actions.

## 4. The keys reminder (the delivery test)

Either:

- **YAML:** paste [`automations/keys_away.yaml`](automations/keys_away.yaml)
  into a new automation (Edit in YAML) and replace `person.REPLACE_ME`, or
- **Blueprint:** import
  [`blueprints/automation/lite_brite/arrival_message.yaml`](blueprints/automation/lite_brite/arrival_message.yaml)
  (Settings → Automations & scenes → Blueprints → Import, using the file's
  GitHub URL), then create an automation from it and pick her in the Person field.

What happens:

1. Her phone tells Home Assistant she's home (usually as she pulls up).
2. The automation leaves `Welcome home! 🔑 Keys away first 🙂` for the sign,
   in orange, with the flash effect, expiring in 30 minutes.
3. She opens the door; the sign's motion sensor wakes it; within a couple of
   seconds it flashes three times and scrolls the message for 30 seconds.
4. The sign clears the message and reports it as `shown`.

Motion can't tell people apart: if someone else walks past the sign first,
they get her greeting. The 30-minute expiry keeps a stale greeting from
lingering (it needs the sign to reach an NTP time server; see
[docs/mqtt-api.md](../docs/mqtt-api.md#messages)). If that matters, add a door
sensor or a condition to the automation.

## More

- [`automations/more_examples.yaml`](automations/more_examples.yaml): trash
  night, umbrella, package delivered, and a "was it seen?" notification.
- [`dashboard_card.yaml`](dashboard_card.yaml): a dashboard card for the sign.
- [`../docs/mqtt-api.md`](../docs/mqtt-api.md): every message field, the topic
  layout, and how to drive the sign from Node-RED or anything else that speaks MQTT.
