# Copyright Preetham Mukundan (C) 2026
"""Read-only check of a hero's gameplay cue effects (default KaiKoa). Logs HEROFXCHECK lines."""
import sys

import unreal

EAL = unreal.EditorAssetLibrary
hero = next((a for a in sys.argv[1:] if not a.startswith("-")), "KaiKoa")
root = f"/Game/ZERO/Heroes/{hero}"


def log(msg):
    unreal.log_warning("HEROFXCHECK " + msg)


def cdo(path):
    cls = EAL.load_blueprint_class(path)
    return unreal.get_default_object(cls) if cls else None


def tag_name(tag):
    text = tag.export_text() if hasattr(tag, "export_text") else str(tag)
    return text.split('"')[1] if '"' in text else text


problems = 0
for path in sorted(EAL.list_assets(f"{root}/FX", recursive=False)):
    path = path.split(".")[0]
    obj = cdo(path)
    if not obj:
        log(f"BAD {path}: no class")
        problems += 1
        continue
    visual = obj.get_editor_property("visual")
    system = visual.get_editor_property("niagara_system")
    params = {str(k): v for k, v in visual.get_editor_property("float_parameters").items()}
    tag = tag_name(obj.get_editor_property("gameplay_cue_tag"))
    ok = bool(system) and tag.startswith("GameplayCue.HeroKit.")
    problems += 0 if ok else 1
    log(f"{'OK ' if ok else 'BAD'} {path.rsplit('/', 1)[1]} [{obj.get_class().get_name()}] tag={tag} system={system.get_name() if system else None} "
        f"scale={visual.get_editor_property('scale')} params={params}")

checks = [
    ("Abilities/SharkSharkShark/BP_KaiKoa_SharkSharkShark_Projectile", ["trail_cue"], ["impact_cue"]),
    ("Abilities/TsunamiFist/BP_KaiKoa_TsunamiFist", [], ["cast_cue", "pulse_cue"]),
    ("Abilities/ArcticBurst/BP_KaiKoa_ArcticBurst", [], ["pulse_cue"]),
    ("Abilities/AbsoluteZero/BP_KaiKoa_AbsoluteZero", [], ["cast_cue", "pulse_cue"]),
    ("Weapon/BP_KaiKoa_Bullet", ["trail_cue", "hit_character_cue", "hit_world_cue"], []),
    ("Weapon/BP_KaiKoa_Bullet_Server", ["trail_cue", "hit_character_cue", "hit_world_cue"], []),
] if hero == "KaiKoa" else []
for rel, tag_props, cue_props in checks:
    obj = cdo(f"{root}/{rel}")
    if not obj:
        log(f"BAD {rel}: missing")
        problems += 1
        continue
    parts = [f"{p}={tag_name(obj.get_editor_property(p))}" for p in tag_props]
    for p in cue_props:
        c = obj.get_editor_property(p)
        parts.append(f"{p}={tag_name(c.get_editor_property('cue'))}/{c.get_editor_property('duration')}")
    if "Bullet" in rel:
        parts.append(f"cascade_trail={obj.get_editor_property('trail_comp').get_editor_property('template')}")
        parts.append(f"cascade_hits={obj.get_editor_property('hit_character_particle')},{obj.get_editor_property('hit_world_particle')}")
    log(f"{rel.rsplit('/', 1)[1]}: " + " ".join(parts))

if hero == "KaiKoa":
    zero = cdo(f"{root}/Abilities/AbsoluteZero/BP_KaiKoa_AbsoluteZero")
    hc = zero.get_editor_property("enemy_payload").get_editor_property("hit_cue")
    log(f"AbsoluteZero hit_cue={tag_name(hc.get_editor_property('cue'))}/{hc.get_editor_property('duration')}")
    ff = cdo(f"{root}/Abilities/FrozenFist/BP_KaiKoa_FrozenFist")
    fc = ff.get_editor_property("on_hit_payload").get_editor_property("hit_cue")
    log(f"FrozenFist chance={ff.get_editor_property('on_hit_chance')} hit_cue={tag_name(fc.get_editor_property('cue'))}/{fc.get_editor_property('duration')}")
log(f"problems={problems}")
