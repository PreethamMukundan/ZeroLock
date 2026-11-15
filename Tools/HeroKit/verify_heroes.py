# Copyright Preetham Mukundan (C) 2026
"""Read-only check of the hero kit assets build_heroes.py made. Logs lines tagged HEROVERIFY."""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hero_specs import HEROES  # noqa: E402

EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("HEROVERIFY " + msg)


def cls_name(c):
    return c.get_name() if c else "None"


table = EAL.load_asset("/Game/ZERO/UI/BP/CharacterSelector/DT_HeroList")
rows = [str(r) for r in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
log("hero list rows: " + ", ".join(rows))
missing = [h for h in HEROES if h not in rows]
log("missing rows: " + (", ".join(missing) if missing else "none"))

for hero in HEROES:
    bp_class = EAL.load_blueprint_class(f"/Game/ZERO/Heroes/{hero}/BP_{hero}")
    if not bp_class:
        log(f"{hero}: NO BP")
        continue
    hero_cdo = unreal.get_default_object(bp_class)
    slots = [cls_name(hero_cdo.get_editor_property(p)) for p in
             ("primary_fire_ability", "secondry_fire_ability", "ability_1", "ability_2", "ultimate_ability", "alt_fire_ability")]
    passives = [cls_name(c) for c in hero_cdo.get_editor_property("default_abilities") if c and "/Heroes/" in c.get_path_name()]
    log(f"{hero}: {' | '.join(slots)} | passives {passives}")

# Spot checks on nested values.
shark = unreal.get_default_object(EAL.load_blueprint_class("/Game/ZERO/Heroes/KaiKoa/Abilities/SharkSharkShark/BP_KaiKoa_SharkSharkShark"))
hit = shark.get_editor_property("hit")
log(f"Shark count={shark.get_editor_property('count')} fan={shark.get_editor_property('fan_angle')} "
    f"damage={hit.get_editor_property('direct_payload').get_editor_property('damage')} "
    f"cooldown={shark.get_editor_property('cooldown_duration').get_editor_property('value')} "
    f"tags={shark.get_editor_property('cooldown_tags')} slot={shark.get_editor_property('slot')} "
    f"proj={cls_name(shark.get_editor_property('projectile_class'))}")
proj = unreal.get_default_object(shark.get_editor_property("projectile_class"))
log(f"Shark projectile pierce={proj.get_editor_property('pierce')} speed={proj.get_editor_property('projectile_movement').get_editor_property('initial_speed')} "
    f"life={proj.get_editor_property('initial_life_span')}")
zero = unreal.get_default_object(EAL.load_blueprint_class("/Game/ZERO/Heroes/KaiKoa/Abilities/AbsoluteZero/BP_KaiKoa_AbsoluteZero"))
payload = zero.get_editor_property("enemy_payload")
log(f"AbsoluteZero pulses={zero.get_editor_property('pulse_count')} statuses={list(payload.get_editor_property('statuses'))} "
    f"stun={payload.get_editor_property('status_duration')} area={zero.get_editor_property('area')}")
frozen = unreal.get_default_object(EAL.load_blueprint_class("/Game/ZERO/Heroes/KaiKoa/Abilities/FrozenFist/BP_KaiKoa_FrozenFist"))
log(f"FrozenFist procs={[(p.get_editor_property('trigger'), p.get_editor_property('chance'), p.get_editor_property('status')) for p in frozen.get_editor_property('procs')]}")
trap = unreal.get_default_object(EAL.load_blueprint_class("/Game/ZERO/Heroes/VanHelsing/Abilities/Trapmaster/BP_VanHelsing_Trapmaster"))
zone = trap.get_editor_property("hit").get_editor_property("zone")
log(f"Trapmaster zone mine={zone.get_editor_property('trigger_once')} max={zone.get_editor_property('max_per_caster')} dmg={zone.get_editor_property('enemy_payload').get_editor_property('damage')}")
ten = unreal.get_default_object(EAL.load_blueprint_class("/Game/ZERO/Heroes/Tempest/Abilities/IAmNumber10/BP_Tempest_IAmNumber10"))
log(f"Number10 owned tags={ten.get_editor_property('activation_owned_tags')} volleys={ten.get_editor_property('volley_count')}")
peach = unreal.get_default_object(EAL.load_blueprint_class("/Game/ZERO/Heroes/WuKong/Abilities/ImmortalPeach/BP_WuKong_ImmortalPeach"))
log(f"Peach tags={peach.get_editor_property('self_payload').get_editor_property('timed_tags')}")
weapon = unreal.get_default_object(EAL.load_blueprint_class("/Game/ZERO/Heroes/KaiKoa/Weapon/BP_KaiKoa_Weapon"))
log(f"Kai weapon mode={weapon.get_editor_property('fire_mode')} pellets={weapon.get_editor_property('pellet_count')} proj={cls_name(weapon.get_editor_property('projectile_class'))}")
da = EAL.load_asset("/Game/ZERO/Heroes/KaiKoa/DA_KaiKoa_Hero")
log(f"Kai DA name={da.get_editor_property('character_name')} class={cls_name(da.get_editor_property('character_class'))}")
