#!/usr/bin/env python3
"""Validate the Home Assistant side against a real Home Assistant install.

Checks, using Home Assistant's own validators and template engine:
  - homeassistant/packages/lite_brite.yaml (scripts)
  - homeassistant/automations/*.yaml (automations)
  - the arrival blueprint, and an automation created from it
  - the scripts' MQTT topic and payload templates render to valid messages
  - optionally, the sign's MQTT discovery payload against every entity schema

Needs Python 3.13+ and Home Assistant in the environment (use a separate
virtualenv: pip install homeassistant). The discovery payload comes from the
simulator:

    (cd firmware && pio run -e sim && .pio/build/sim/program --discovery) > discovery.json
    python tools/validate_ha.py --discovery discovery.json
"""

from __future__ import annotations

import argparse
import asyncio
import copy
import importlib
import json
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HA_DIR = ROOT / "homeassistant"

try:
    import voluptuous as vol
    from homeassistant import loader
    from homeassistant.components.automation import config as automation_config
    from homeassistant.components.automation.config import AUTOMATION_BLUEPRINT_SCHEMA
    from homeassistant.components.blueprint.models import Blueprint, BlueprintInputs
    from homeassistant.components.mqtt import discovery as mqtt_discovery
    from homeassistant.components.mqtt.schemas import DEVICE_DISCOVERY_SCHEMA, SHARED_OPTIONS
    from homeassistant.components.script import config as script_config
    from homeassistant.core import HomeAssistant
    from homeassistant.helpers.template import Template
    from homeassistant.util.yaml import loader as ha_yaml
except ImportError as exc:  # pragma: no cover
    sys.exit(f"needs Home Assistant installed (pip install homeassistant): {exc}")


class Checker:
    def __init__(self) -> None:
        self.failures = 0

    def report(self, ok: bool, what: str, detail: str = "") -> None:
        self.failures += 0 if ok else 1
        print(("ok    " if ok else "FAIL  ") + what + (f": {detail}" if detail and not ok else ""))

    async def expect_valid(self, what: str, coro) -> None:
        try:
            await coro
            self.report(True, what)
        except Exception as err:  # noqa: BLE001 - report every validation failure
            self.report(False, what, repr(err))


def render(hass, template: str, variables: dict) -> str:
    return Template(template, hass).async_render(variables, parse_result=False)


async def check_yaml(hass, c: Checker) -> None:
    pkg = ha_yaml.load_yaml(HA_DIR / "packages" / "lite_brite.yaml")
    for object_id, cfg in pkg["script"].items():
        await c.expect_valid(f"script.{object_id}", script_config.async_validate_config_item(hass, object_id, cfg))

    for path in sorted((HA_DIR / "automations").glob("*.yaml")):
        data = ha_yaml.load_yaml(path)
        for cfg in data if isinstance(data, list) else [data]:
            await c.expect_valid(
                f"automation '{cfg.get('alias')}' ({path.name})",
                automation_config.async_validate_config_item(hass, "check", cfg),
            )

    for path in sorted((HA_DIR / "blueprints").rglob("*.yaml")):
        rel = path.relative_to(HA_DIR / "blueprints" / "automation")
        try:
            bp = Blueprint(ha_yaml.load_yaml(path), expected_domain="automation", schema=AUTOMATION_BLUEPRINT_SCHEMA)
            c.report(True, f"blueprint {rel}")
        except Exception as err:  # noqa: BLE001
            c.report(False, f"blueprint {rel}", repr(err))
            continue
        # Fill in required inputs that have no default with a plausible value.
        required = {k: "person.someone" for k, v in bp.inputs.items() if not (v or {}).get("default")}
        inputs = BlueprintInputs(bp, {"use_blueprint": {"path": str(rel), "input": required}})
        inputs.validate()
        await c.expect_valid(
            f"automation from blueprint {rel}",
            automation_config.async_validate_config_item(hass, "check", inputs.async_substitute()),
        )


def check_templates(hass, c: Checker) -> None:
    pkg = ha_yaml.load_yaml(HA_DIR / "packages" / "lite_brite.yaml")
    seq = pkg["script"]["lite_brite_show"]["sequence"]
    cases = {
        "all fields": {
            "message": "Welcome home! :key: Keys away first :smile:",
            "slot": "Keys Away",
            "color": "orange",
            "effect": "flash",
            "duration": 30,
            "expires_in": 30,
        },
        "message only": {"message": "Hi"},
    }
    for name, variables in cases.items():
        variables = dict(variables)
        variables["expires_minutes"] = Template(seq[0]["variables"]["expires_minutes"], hass).async_render(variables)
        topic = render(hass, seq[1]["data"]["topic"], variables)
        payload = render(hass, seq[1]["data"]["payload"], variables)
        try:
            msg = json.loads(payload)
            ok = (
                topic.startswith("lite-brite/entryway/msg/")
                and msg["text"] == variables["message"]
                and (msg["expires"] > msg["sent"]) == ("expires_in" in variables)
            )
            c.report(ok, f"lite_brite_show payload ({name})", f"{topic} {payload}")
        except (ValueError, KeyError) as err:
            c.report(False, f"lite_brite_show payload ({name})", f"{err}: {payload!r}")

    clear = pkg["script"]["lite_brite_clear"]["sequence"][0]["data"]
    topic = render(hass, clear["topic"], {"slot": "Keys Away"})
    c.report(topic == "lite-brite/entryway/msg/keys_away", "lite_brite_clear topic", topic)


def check_discovery(path: Path, c: Checker) -> None:
    payload = mqtt_discovery.MQTTDiscoveryPayload(copy.deepcopy(json.loads(path.read_text())))
    mqtt_discovery._replace_all_abbreviations(payload)  # noqa: SLF001 - same steps as HA
    try:
        DEVICE_DISCOVERY_SCHEMA(payload)
        c.report(True, "device discovery payload")
    except vol.Invalid as err:
        c.report(False, "device discovery payload", str(err))
        return
    for component_id, component in payload["components"].items():
        config = dict(component)
        platform = config.pop("platform")
        config["device"] = payload["device"]
        config["origin"] = payload["origin"]
        for option in SHARED_OPTIONS:
            if option in payload and option not in config:
                config[option] = payload[option]
        schema = importlib.import_module(f"homeassistant.components.mqtt.{platform}").DISCOVERY_SCHEMA
        try:
            schema(config)
            c.report(True, f"  {platform} '{component_id}'")
        except vol.Invalid as err:
            c.report(False, f"  {platform} '{component_id}'", str(err))


async def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--discovery", type=Path, help="discovery JSON from `program --discovery`")
    args = ap.parse_args()

    c = Checker()
    hass = HomeAssistant(tempfile.mkdtemp())
    loader.async_setup(hass)
    await hass.config.async_set_time_zone("America/Los_Angeles")
    try:
        await check_yaml(hass, c)
        check_templates(hass, c)
        if args.discovery:
            check_discovery(args.discovery, c)
    finally:
        await hass.async_stop(force=True)
    print(f"{c.failures} failure(s)")
    return 1 if c.failures else 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
