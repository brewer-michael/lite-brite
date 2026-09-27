#!/usr/bin/env python3
"""Check that the Home Assistant YAML files parse (HA's !tags allowed)."""

from __future__ import annotations

import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent


class Loader(yaml.SafeLoader):
    pass


# !input, !include_dir_named, !secret ... are resolved by Home Assistant.
Loader.add_multi_constructor("!", lambda loader, suffix, node: f"<{suffix}>")


def main() -> int:
    failed = 0
    files = sorted((ROOT / "homeassistant").rglob("*.yaml"))
    for path in files:
        try:
            with path.open(encoding="utf-8") as fh:
                yaml.load(fh, Loader=Loader)
        except yaml.YAMLError as err:
            print(f"{path.relative_to(ROOT)}: {err}", file=sys.stderr)
            failed += 1
    print(f"{len(files) - failed}/{len(files)} Home Assistant YAML files OK")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
