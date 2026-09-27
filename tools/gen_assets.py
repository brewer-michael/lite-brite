#!/usr/bin/env python3
"""Generate the firmware's font and icon tables from the ASCII-art sources.

Sources (edit these):
    firmware/assets/font.txt
    firmware/assets/icons.txt

Outputs (generated, committed so builds don't need Python):
    firmware/lib/litebrite_core/src/lb/generated/FontData.cpp
    firmware/lib/litebrite_core/src/lb/generated/IconData.cpp

Usage:
    python3 tools/gen_assets.py            # regenerate the C++ tables
    python3 tools/gen_assets.py --check    # exit 1 if the tables are stale (CI)
    python3 tools/gen_assets.py --preview sheet.png   # contact sheet (needs Pillow)
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "firmware" / "assets"
OUT_DIR = ROOT / "firmware" / "lib" / "litebrite_core" / "src" / "lb" / "generated"
HEIGHT = 8
MAX_ICON_WIDTH = 8


class AssetError(Exception):
    pass


@dataclass
class Glyph:
    codepoint: int
    name: str
    rows: list[str]

    @property
    def width(self) -> int:
        return len(self.rows[0])

    def columns(self) -> list[int]:
        cols = []
        for x in range(self.width):
            byte = 0
            for y, row in enumerate(self.rows):
                if row[x] == "#":
                    byte |= 1 << y
            cols.append(byte)
        return cols


@dataclass
class Icon:
    name: str
    aliases: list[str]
    emoji: list[tuple[int, int]]
    rows: list[str]
    line: int = 0

    @property
    def width(self) -> int:
        return len(self.rows[0])


@dataclass
class IconSet:
    palette: dict[str, tuple[int, int, int]] = field(default_factory=dict)
    palette_order: list[str] = field(default_factory=list)
    icons: list[Icon] = field(default_factory=list)


def _blocks(path: Path):
    """Yield (header, rows, line_no) for each '== ...' block in a source file."""
    lines = path.read_text(encoding="utf-8").splitlines()
    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        if line.startswith("== "):
            header, start = line[3:].strip(), i + 1
            rows = [r.rstrip() for r in lines[start : start + HEIGHT]]
            if len(rows) != HEIGHT or any(not r for r in rows):
                raise AssetError(f"{path.name}:{i + 1}: '{header}' needs {HEIGHT} non-empty rows")
            if len({len(r) for r in rows}) != 1:
                raise AssetError(f"{path.name}:{i + 1}: '{header}' rows have different widths")
            yield header, rows, i + 1
            i = start + HEIGHT
        else:
            i += 1


def parse_font(path: Path) -> list[Glyph]:
    glyphs: dict[int, Glyph] = {}
    for header, rows, line in _blocks(path):
        m = re.match(r"U\+([0-9A-Fa-f]{4,6})\s*(.*)$", header)
        if not m:
            raise AssetError(f"{path.name}:{line}: expected 'U+XXXX name', got '{header}'")
        cp = int(m.group(1), 16)
        if any(set(r) - {"#", "."} for r in rows):
            raise AssetError(f"{path.name}:{line}: glyph rows may only contain '#' and '.'")
        if cp in glyphs:
            raise AssetError(f"{path.name}:{line}: duplicate glyph U+{cp:04X}")
        glyphs[cp] = Glyph(cp, m.group(2).strip(), rows)
    for cp in range(0x20, 0x7F):
        if cp not in glyphs:
            raise AssetError(f"font is missing printable ASCII U+{cp:04X} '{chr(cp)}'")
    if 0xFFFD not in glyphs:
        raise AssetError("font is missing the U+FFFD replacement glyph")
    return [glyphs[cp] for cp in sorted(glyphs)]


def parse_icons(path: Path) -> IconSet:
    result = IconSet()
    for n, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if raw.startswith("@ "):
            parts = raw[2:].split()
            if len(parts) < 4 or len(parts[0]) != 1:
                raise AssetError(f"{path.name}:{n}: palette lines look like '@ c r g b comment'")
            ch, rgb = parts[0], tuple(int(v) for v in parts[1:4])
            if ch in ".t" or ch in result.palette:
                raise AssetError(f"{path.name}:{n}: palette character '{ch}' is reserved or duplicated")
            if any(not 0 <= v <= 255 for v in rgb):
                raise AssetError(f"{path.name}:{n}: color components must be 0-255")
            result.palette[ch] = rgb  # type: ignore[assignment]
            result.palette_order.append(ch)
    seen_names: set[str] = set()
    seen_emoji: dict[int, str] = {}
    for header, rows, line in _blocks(path):
        tokens = header.split()
        name, rest = tokens[0], tokens[1:]
        aliases, emoji = [], []
        for tok in rest:
            m = re.fullmatch(r"U\+([0-9A-Fa-f]{4,6})(?:-([0-9A-Fa-f]{4,6}))?", tok)
            if m:
                first = int(m.group(1), 16)
                last = int(m.group(2), 16) if m.group(2) else first
                if last < first:
                    raise AssetError(f"{path.name}:{line}: bad range {tok}")
                emoji.append((first, last))
            elif re.fullmatch(r"[a-z0-9_]+", tok):
                aliases.append(tok)
            else:
                raise AssetError(f"{path.name}:{line}: can't parse '{tok}'")
        for nm in [name, *aliases]:
            if not re.fullmatch(r"[a-z0-9_]+", nm):
                raise AssetError(f"{path.name}:{line}: icon names are lowercase a-z, 0-9, _")
            if nm in seen_names:
                raise AssetError(f"{path.name}:{line}: duplicate icon name '{nm}'")
            seen_names.add(nm)
        for first, last in emoji:
            for cp in range(first, last + 1):
                if cp in seen_emoji:
                    raise AssetError(f"{path.name}:{line}: U+{cp:04X} already maps to '{seen_emoji[cp]}'")
                seen_emoji[cp] = name
        if len(rows[0]) > MAX_ICON_WIDTH:
            raise AssetError(f"{path.name}:{line}: icon '{name}' is wider than {MAX_ICON_WIDTH}")
        for r in rows:
            for ch in r:
                if ch not in ".t" and ch not in result.palette:
                    raise AssetError(f"{path.name}:{line}: '{ch}' in icon '{name}' is not in the palette")
        result.icons.append(Icon(name, aliases, emoji, rows, line))
    if len(result.icons) > 255:
        raise AssetError("too many icons (max 255)")
    return result


HEADER = """// GENERATED by tools/gen_assets.py from firmware/assets/{src}. Do not edit.
// Edit the ASCII-art source and run: python3 tools/gen_assets.py
"""


def _hex_rows(values: list[int], per_line: int = 16, indent: str = "    ") -> str:
    out = []
    for i in range(0, len(values), per_line):
        out.append(indent + ", ".join(f"0x{v:02X}" for v in values[i : i + per_line]) + ",")
    return "\n".join(out)


def _c_comment(text: str) -> str:
    return text.replace("*/", "* /").replace("\\", "backslash")


def render_font_cpp(glyphs: list[Glyph]) -> str:
    columns: list[int] = []
    table = []
    for g in glyphs:
        table.append(
            f"    {{0x{g.codepoint:04X}, {len(columns)}, {g.width}}},  // {_c_comment(g.name)}"
        )
        columns.extend(g.columns())
    if len(columns) > 0xFFFF:
        raise AssetError("font column table exceeds 16-bit offsets")
    return (
        HEADER.format(src="font.txt")
        + '#include "lb/Font.h"\n\n'
        + "namespace lb {\nnamespace font_data {\n\n"
        + "// One byte per glyph column; bit 0 is the top row.\n"
        + f"const uint8_t kColumns[{len(columns)}] = {{\n{_hex_rows(columns)}\n}};\n\n"
        + "// Sorted by codepoint for binary search.\n"
        + f"const GlyphInfo kGlyphs[{len(glyphs)}] = {{\n"
        + "\n".join(table)
        + "\n};\n\n"
        + f"const size_t kGlyphCount = {len(glyphs)};\n\n"
        + "}  // namespace font_data\n}  // namespace lb\n"
    )


def render_icons_cpp(icons: IconSet) -> str:
    # Palette index 0 = transparent, 1 = text color, 2.. = palette entries.
    index = {".": 0, "t": 1}
    for i, ch in enumerate(icons.palette_order):
        index[ch] = i + 2
    pixels: list[int] = []
    infos, aliases, emoji = [], [], []
    for n, icon in enumerate(icons.icons):
        infos.append(f'    {{"{icon.name}", {len(pixels)}, {icon.width}}},')
        for row in icon.rows:
            pixels.extend(index[ch] for ch in row)
        for nm in [icon.name, *icon.aliases]:
            aliases.append((nm, n))
        for first, last in icon.emoji:
            emoji.append((first, last, n))
    aliases.sort()
    emoji.sort()
    for (a_first, a_last, _), (b_first, _, _) in zip(emoji, emoji[1:]):
        if b_first <= a_last:
            raise AssetError("emoji ranges overlap")
    palette_lines = [
        f"    {{{r}, {g}, {b}}},  // '{ch}'"
        for ch in icons.palette_order
        for (r, g, b) in [icons.palette[ch]]
    ]
    return (
        HEADER.format(src="icons.txt")
        + '#include "lb/Icons.h"\n\n'
        + "namespace lb {\nnamespace icon_data {\n\n"
        + "// Palette entries for pixel values >= 2 (0 = transparent, 1 = text color).\n"
        + f"const Rgb kPalette[{len(palette_lines)}] = {{\n"
        + "\n".join(palette_lines)
        + "\n};\n\n"
        + "// Row-major palette indices, width * 8 bytes per icon.\n"
        + f"const uint8_t kPixels[{len(pixels)}] = {{\n{_hex_rows(pixels)}\n}};\n\n"
        + f"const IconInfo kIcons[{len(icons.icons)}] = {{\n"
        + "\n".join(infos)
        + "\n};\n"
        + f"const size_t kIconCount = {len(icons.icons)};\n\n"
        + "// Names and aliases, sorted for binary search.\n"
        + f"const IconName kNames[{len(aliases)}] = {{\n"
        + "\n".join(f'    {{"{nm}", {n}}},' for nm, n in aliases)
        + "\n};\n"
        + f"const size_t kNameCount = {len(aliases)};\n\n"
        + "// Emoji codepoint ranges, sorted by first codepoint.\n"
        + f"const EmojiRange kEmoji[{len(emoji)}] = {{\n"
        + "\n".join(f"    {{0x{a:04X}, 0x{b:04X}, {n}}},  // {icons.icons[n].name}" for a, b, n in emoji)
        + "\n};\n"
        + f"const size_t kEmojiCount = {len(emoji)};\n\n"
        + "}  // namespace icon_data\n}  // namespace lb\n"
    )


def render_preview(glyphs: list[Glyph], icons: IconSet, out: Path) -> None:
    try:
        from PIL import Image, ImageDraw
    except ImportError as exc:  # pragma: no cover
        raise SystemExit("--preview needs Pillow: pip install pillow") from exc

    cell, gap, per_row = 10, 3, 24
    items: list[tuple[list[str], dict[str, tuple[int, int, int]]]] = []
    white = {"#": (255, 255, 255)}
    for g in glyphs:
        items.append((g.rows, white))
    pal = dict(icons.palette)
    pal["t"] = (255, 140, 0)
    for icon in icons.icons:
        items.append((icon.rows, pal))
    max_w = max(len(rows[0]) for rows, _ in items)
    box = (max_w + 1) * cell + gap
    rows_needed = (len(items) + per_row - 1) // per_row
    img = Image.new("RGB", (per_row * box + gap, rows_needed * (HEIGHT * cell + gap * 3) + gap), (18, 18, 22))
    draw = ImageDraw.Draw(img)
    for i, (rows, colors) in enumerate(items):
        ox = gap + (i % per_row) * box
        oy = gap + (i // per_row) * (HEIGHT * cell + gap * 3)
        for y, row in enumerate(rows):
            for x, ch in enumerate(row):
                c = colors.get(ch)
                fill = c if c else (38, 38, 44)
                r = cell // 2 - 1
                cx, cy = ox + x * cell + cell // 2, oy + y * cell + cell // 2
                draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=fill)
    img.save(out)
    print(f"wrote {out}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true", help="fail if generated files are out of date")
    ap.add_argument("--preview", type=Path, help="write a PNG contact sheet of all glyphs and icons")
    args = ap.parse_args()

    try:
        glyphs = parse_font(ASSETS / "font.txt")
        icons = parse_icons(ASSETS / "icons.txt")
        outputs = {
            OUT_DIR / "FontData.cpp": render_font_cpp(glyphs),
            OUT_DIR / "IconData.cpp": render_icons_cpp(icons),
        }
    except AssetError as err:
        print(f"error: {err}", file=sys.stderr)
        return 2

    if args.preview:
        render_preview(glyphs, icons, args.preview)

    stale = [p for p, text in outputs.items() if not p.exists() or p.read_text(encoding="utf-8") != text]
    if args.check:
        for p in stale:
            print(f"stale: {p.relative_to(ROOT)} (run python3 tools/gen_assets.py)", file=sys.stderr)
        return 1 if stale else 0
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for p in stale:
        p.write_text(outputs[p], encoding="utf-8")
        print(f"wrote {p.relative_to(ROOT)}")
    print(f"{len(glyphs)} glyphs, {len(icons.icons)} icons")
    return 0


if __name__ == "__main__":
    sys.exit(main())
