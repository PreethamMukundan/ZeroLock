# Copyright Preetham Mukundan (C) 2026
"""
Placeholder ability icons for hero kit heroes. Editor Python (in the editor: py "<this file>" [Hero ...], or headless).
Draws a 256 px PNG per ability (a disc in the hero colour, darker ring, the ability's initials in white), imports it as
/Game/ZERO/Heroes/<Hero>/Icons/T_<Hero>_<Ability> (UI texture group, no mips) and sets the ability BP's IconImage.
Replace a texture with real art by reimporting over it; the BPs keep pointing at it. Logs: log_warning tagged HEROICON.
"""
import os
import struct
import sys
import zlib

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hero_specs  # noqa: E402
import importlib  # noqa: E402

importlib.reload(hero_specs)

ROOT = "/Game/ZERO/Heroes"
OUT = os.path.join(unreal.Paths.project_saved_dir(), "HeroIcons")
SIZE = 256
EAL = unreal.EditorAssetLibrary

# Text on the icon per ability (default: capitals of the asset name, at most 2).
LABELS = {
    "SharkSharkShark": "SH", "IAmNumber10": "10", "SettingAPick": "PK",
}
ICON_HEROES = ["KaiKoa", "Tempest"]

# 5x7 bitmap font: each glyph 7 rows of 5 bits.
FONT = {
    "A": "01110 10001 10001 11111 10001 10001 10001", "B": "11110 10001 10001 11110 10001 10001 11110",
    "C": "01111 10000 10000 10000 10000 10000 01111", "D": "11110 10001 10001 10001 10001 10001 11110",
    "E": "11111 10000 10000 11110 10000 10000 11111", "F": "11111 10000 10000 11110 10000 10000 10000",
    "G": "01111 10000 10000 10011 10001 10001 01111", "H": "10001 10001 10001 11111 10001 10001 10001",
    "I": "11111 00100 00100 00100 00100 00100 11111", "J": "00111 00010 00010 00010 00010 10010 01100",
    "K": "10001 10010 10100 11000 10100 10010 10001", "L": "10000 10000 10000 10000 10000 10000 11111",
    "M": "10001 11011 10101 10101 10001 10001 10001", "N": "10001 11001 10101 10011 10001 10001 10001",
    "O": "01110 10001 10001 10001 10001 10001 01110", "P": "11110 10001 10001 11110 10000 10000 10000",
    "Q": "01110 10001 10001 10001 10101 10010 01101", "R": "11110 10001 10001 11110 10100 10010 10001",
    "S": "01111 10000 10000 01110 00001 00001 11110", "T": "11111 00100 00100 00100 00100 00100 00100",
    "U": "10001 10001 10001 10001 10001 10001 01110", "V": "10001 10001 10001 10001 10001 01010 00100",
    "W": "10001 10001 10001 10101 10101 10101 01010", "X": "10001 10001 01010 00100 01010 10001 10001",
    "Y": "10001 10001 01010 00100 00100 00100 00100", "Z": "11111 00001 00010 00100 01000 10000 11111",
    "0": "01110 10001 10011 10101 11001 10001 01110", "1": "00100 01100 00100 00100 00100 00100 01110",
    "2": "01110 10001 00001 00010 00100 01000 11111", "3": "11110 00001 00001 01110 00001 00001 11110",
    "4": "00010 00110 01010 10010 11111 00010 00010", "5": "11111 10000 11110 00001 00001 10001 01110",
    "6": "00110 01000 10000 11110 10001 10001 01110", "7": "11111 00001 00010 00100 01000 01000 01000",
    "8": "01110 10001 10001 01110 10001 10001 01110", "9": "01110 10001 10001 01111 00001 00010 01100",
}


def log(msg):
    unreal.log_warning("HEROICON " + msg)


def label_for(name):
    if name in LABELS:
        return LABELS[name]
    caps = [c for c in name if c.isupper() or c.isdigit()]
    return "".join(caps[:2]) or name[:2].upper()


def write_png(path, pixels):
    raw = b"".join(b"\x00" + bytes(row) for row in pixels)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def draw_icon(path, color, text):
    """Disc in the hero colour (lighter at the top), dark ring, initials with a drop shadow. RGBA rows."""
    r0, g0, b0 = [max(0.0, min(1.0, c)) for c in color]
    c = SIZE / 2.0
    radius = SIZE / 2.0 - 2
    ring = 14.0
    dark_text = 0.2126 * r0 + 0.7152 * g0 + 0.0722 * b0 > 0.6  # bright discs get dark letters
    rows = [[0] * (SIZE * 4) for _ in range(SIZE)]

    # Text layout: glyphs 5x7 cells, 1 cell apart.
    cell = 15 if len(text) <= 2 else 11
    text_w = (len(text) * 6 - 1) * cell
    text_h = 7 * cell
    tx0 = int(c - text_w / 2)
    ty0 = int(c - text_h / 2)

    def text_hit(x, y):
        gx, gy = x - tx0, y - ty0
        if gx < 0 or gy < 0 or gy >= text_h or gx >= text_w:
            return False
        index, col = divmod(gx // cell, 6)
        if col == 5 or index >= len(text):
            return False
        glyph = FONT.get(text[index])
        if not glyph:
            return False
        return glyph.split()[gy // cell][col] == "1"

    for y in range(SIZE):
        for x in range(SIZE):
            d = ((x + 0.5 - c) ** 2 + (y + 0.5 - c) ** 2) ** 0.5
            if d > radius + 1:
                continue
            alpha = max(0.0, min(1.0, radius + 1 - d))
            shade = 1.15 - 0.45 * (y / SIZE)  # light from above
            if d > radius - ring:
                r, g, b = r0 * 0.35, g0 * 0.35, b0 * 0.35
            else:
                r, g, b = min(1.0, r0 * shade), min(1.0, g0 * shade), min(1.0, b0 * shade)
            if text_hit(x - 5, y - 5):
                r, g, b = (r * 0.25, g * 0.25, b * 0.25) if not dark_text else (min(1, r * 1.3), min(1, g * 1.3), min(1, b * 1.3))
            if text_hit(x, y):
                r, g, b = (0.08, 0.1, 0.12) if dark_text else (0.97, 0.97, 0.97)
            i = x * 4
            rows[y][i:i + 4] = [int(r * 255), int(g * 255), int(b * 255), int(alpha * 255)]
    write_png(path, rows)


def import_texture(png, folder, name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", png)
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(f"{folder}/{name}")
    if texture:
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        EAL.save_loaded_asset(texture, False)
    return texture


def abilities_of(spec):
    for key in ("secondary", "ability1", "ability2", "f", "ult"):
        if spec.get(key):
            yield spec[key][0]
    for entry in spec.get("passives", []):
        yield entry[0]


def main(heroes):
    os.makedirs(OUT, exist_ok=True)
    done = 0
    for hero in heroes:
        spec = hero_specs.HEROES.get(hero)
        if not spec:
            log(f"no spec for {hero}")
            continue
        folder = f"{ROOT}/{hero}/Icons"
        for name in abilities_of(spec):
            bp_path = f"{ROOT}/{hero}/Abilities/{name}/BP_{hero}_{name}"
            if not EAL.does_asset_exist(bp_path):
                log(f"skip {hero}.{name}: no BP")
                continue
            png = os.path.join(OUT, f"T_{hero}_{name}.png")
            draw_icon(png, spec["color"], label_for(name))
            texture = import_texture(png, folder, f"T_{hero}_{name}")
            if not texture:
                log(f"import failed {png}")
                continue
            bp = EAL.load_asset(bp_path)
            cls = EAL.load_blueprint_class(bp_path)
            cdo = unreal.get_default_object(cls)
            cdo.set_editor_property("icon_image", texture)
            EAL.save_loaded_asset(bp, False)
            done += 1
            log(f"{hero}.{name}: {label_for(name)}")
    log(f"done {done}")


main([a for a in sys.argv[1:] if not a.startswith("-")] or ICON_HEROES)
