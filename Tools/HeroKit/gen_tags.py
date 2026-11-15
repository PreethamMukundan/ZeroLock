# Copyright Preetham Mukundan (C) 2026
"""Writes Config/Tags/HeroKitTags.ini: a cooldown tag per slotted hero kit ability and a gameplay cue tag per cue in
hero_specs.py. Run with any Python 3 before build_heroes.py (tags must exist when the editor loads, or the BPs'
CooldownTags and cue tags come out empty)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hero_specs import HEROES, SLOTS, cue_tag, find_cues  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def cooldown_tag(hero, ability):
    return f"ZerolockAbilities.Cooldown.{hero}.{ability}"


def hero_cue_tags(hero, spec):
    """(tag, comment) for every cue: context Weapon for the primary, the ability's name otherwise."""
    tags = []
    contexts = [("Weapon", spec.get("weapon"))]
    for key in SLOTS:
        if spec.get(key):
            contexts.append((spec[key][0], spec[key][2]))
    for entry in spec.get("passives", []):
        contexts.append((entry[0], entry[2]))
    for context, value in contexts:
        for found in find_cues(value):
            tag = cue_tag(hero, context, found["__cue__"])
            if tag not in [t for t, _ in tags]:
                tags.append((tag, f"Hero kit cue: {spec['display']} {context} {found['__cue__']}"))
    return tags


def main():
    lines = ["[/Script/GameplayTags.GameplayTagsList]"]
    count = 0
    for hero, spec in HEROES.items():
        for key in SLOTS:
            entry = spec.get(key)
            if entry:
                lines.append(f'GameplayTagList=(Tag="{cooldown_tag(hero, entry[0])}",DevComment="Hero kit: {spec["display"]} {entry[0]}")')
                count += 1
        for tag, comment in hero_cue_tags(hero, spec):
            lines.append(f'GameplayTagList=(Tag="{tag}",DevComment="{comment}")')
            count += 1
    path = os.path.join(ROOT, "Config", "Tags", "HeroKitTags.ini")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"wrote {count} tags to {path}")


if __name__ == "__main__":
    main()
