# Copyright Preetham Mukundan (C) 2026
"""
Builds the hero kit heroes from hero_specs.py. Unreal editor Python. Editor closed (headless):

  "Z:\\epicgames\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "E:\\Repo\\ZeroLock\\ZeroLock\\ZeroLock.uproject"
      -run=pythonscript -script="E:\\Repo\\ZeroLock\\ZeroLock\\Tools\\HeroKit\\build_heroes.py" -unattended -nosplash -nullrhi

or in the editor console: py "E:/Repo/ZeroLock/ZeroLock/Tools/HeroKit/build_heroes.py" [Hero ...]
Run Tools/HeroKit/gen_tags.py first (cooldown tags). Re-running updates the existing assets in place.

Per hero, under /Game/ZERO/Heroes/<Hero>/ (Deadlock import layout):
  BP_<Hero> (copy of BP_SCombo), DA_<Hero>_Hero, GE_<Hero>_DefaultStats (copy of SCombo's),
  Weapon/BP_<Hero>_Weapon + BP_<Hero>_Bullet + BP_<Hero>_Bullet_Server,
  Abilities/<Ability>/BP_<Hero>_<Ability> (+ _Projectile), empty Montages/, Materials/, Textures/,
  and a row <Hero> in DT_HeroList.
Logs go through log_warning with the tag HEROKIT (commandlet output hides unreal.log).
"""
import json
import os
import re
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hero_specs  # noqa: E402
import importlib  # noqa: E402

importlib.reload(hero_specs)
from hero_specs import HEROES, SLOTS  # noqa: E402

ROOT = "/Game/ZERO/Heroes"
TEMPLATE = "/Game/ZERO/HeroAbilities/SComboTest"
HERO_TEMPLATE = TEMPLATE + "/BP_SCombo"
DA_TEMPLATE = TEMPLATE + "/DA_SCombo_Hero"
GE_TEMPLATE = TEMPLATE + "/GE_SCombo_DefaultStats"
WEAPON_TEMPLATE = TEMPLATE + "/Weapon/BP_SCombo_Weapon"
BULLET_TEMPLATE = "/Game/ZERO/UI/BP/Weapon/BP_ProjectileTest"
BULLET_SERVER_TEMPLATE = "/Game/ZERO/UI/BP/Weapon/BP_ProjectileTest1"
HERO_LIST = "/Game/ZERO/UI/BP/CharacterSelector/DT_HeroList"
SCOMBO_PASSIVE = "BP_SCombo_Combo"

EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
BEL = unreal.BlueprintEditorLibrary

# Array properties whose element type can't be read off an empty unreal.Array.
ARRAY_TYPES = {
    "stats": "ZL_ItemStatValue", "permanent_stats": "ZL_ItemStatValue", "stats_per_stack": "ZL_ItemStatValue",
    "self_stats": "ZL_ItemStatValue", "target_stats": "ZL_ItemStatValue",
    "statuses": "ZL_ItemStatus", "while_active_triggers": "ZL_ProcTrigger", "on_hit_triggers": "ZL_ProcTrigger",
    "frost_triggers": "ZL_ProcTrigger",
    "procs": "ZL_ItemProc",
}
SKIP_KEYS = {"projectile"}
errors = []
# (hero, context) of the ability being built: names its gameplay cues.
CONTEXT = ["", ""]
CUES_DONE = set()


def log(msg):
    unreal.log_warning("HEROKIT " + msg)


def fail(msg):
    errors.append(msg)
    unreal.log_error("HEROKIT ERROR " + msg)


# ---- value conversion ------------------------------------------------------------------------------------------------

def enum_value(enum_type, name):
    wanted = name.replace("_", "").lower()
    for attr in dir(enum_type):
        if attr.isupper() or "_" in attr:
            if attr.replace("_", "").lower() == wanted:
                return getattr(enum_type, attr)
    raise ValueError(f"{enum_type.__name__} has no value {name}")


def tag_container(tags):
    container = unreal.GameplayTagContainer()
    if tags:
        inner = ",".join(f'(TagName="{t}")' for t in tags)
        container.import_text(f"(GameplayTags=({inner}))")
    return container


def make_element(type_name, value):
    cls = getattr(unreal, type_name)
    if issubclass(cls, unreal.EnumBase):
        return enum_value(cls, value)
    if type_name == "ZL_ItemStatValue":
        stat, amount = value
        element = unreal.ZL_ItemStatValue()
        element.set_editor_property("stat", enum_value(unreal.ZL_ItemStat, stat))
        element.set_editor_property("value", float(amount))
        return element
    element = cls()
    set_struct(element, value)
    return element


def make_tag(tag):
    result = unreal.GameplayTag()
    result.import_text(f'(TagName="{tag}")')
    return result


def register_cue(cue):
    """Creates/updates the notify asset for a spec cue and returns its tag name."""
    hero, context = CONTEXT
    role = cue["__cue__"]
    tag = hero_specs.cue_tag(hero, context, role)
    if tag in CUES_DONE:
        return tag
    CUES_DONE.add(tag)
    folder = f"{ROOT}/{hero}/FX"
    ensure_dir(folder)
    parent = kit_class("ZL_GCN_KitLoop" if cue["loop"] else "ZL_GCN_KitBurst")
    bp = blueprint(folder, f"GCN_{context}_{role}", parent)
    if not bp:
        return tag
    obj = cdo(bp)
    try:
        obj.set_editor_property("gameplay_cue_tag", make_tag(tag))
        visual = obj.get_editor_property("visual")
        set_struct(visual, cue["visual"])
        obj.set_editor_property("visual", visual)
    except Exception as exc:
        fail(f"cue {tag}: {exc}")
    save(bp)
    return tag


def convert(name, current, value):
    if isinstance(value, dict) and "__cue__" in value:
        tag = register_cue(value)
        if isinstance(current, unreal.GameplayTag):
            return make_tag(tag)
        kit_cue = unreal.ZL_KitCue()
        kit_cue.set_editor_property("cue", make_tag(tag))
        kit_cue.set_editor_property("duration", float(value["duration"]))
        return kit_cue
    if name == "float_parameters":
        params = unreal.Map(unreal.Name, float)
        for key, number in value.items():
            params[unreal.Name(key)] = float(number)
        return params
    if isinstance(value, str) and (value.startswith("/Game/") or value.startswith("/Engine/")):
        asset = unreal.load_asset(value)
        if not asset:
            raise ValueError(f"no asset {value}")
        return asset
    if name in ARRAY_TYPES:
        array = unreal.Array(getattr(unreal, ARRAY_TYPES[name]))
        for item in value:
            array.append(make_element(ARRAY_TYPES[name], item))
        return array
    if isinstance(current, unreal.ScalableFloat):
        current.set_editor_property("value", float(value))
        return current
    if isinstance(current, unreal.EnumBase):
        return enum_value(type(current), value)
    if isinstance(current, unreal.GameplayTagContainer):
        return tag_container(value)
    if isinstance(current, unreal.LinearColor):
        r, g, b = value[:3]
        return unreal.LinearColor(r, g, b, value[3] if len(value) > 3 else 1.0)
    if isinstance(current, unreal.StructBase) and isinstance(value, dict):
        set_struct(current, value)
        return current
    if isinstance(current, bool):
        return bool(value)
    if isinstance(current, float):
        return float(value)
    if isinstance(current, int) and not isinstance(current, bool):
        return int(value)
    return value


def set_struct(struct, values):
    for key, value in values.items():
        current = struct.get_editor_property(key)
        struct.set_editor_property(key, convert(key, current, value))


def set_props(obj, values, where, native=None):
    """native: the C++ parent class; a None value resets that key to its default."""
    for key, value in values.items():
        if key in SKIP_KEYS:
            continue
        try:
            if value is None:  # back to the C++ class default (clears overrides a reparented BP kept)
                if not native:
                    fail(f"{where}: {key}: no native class to reset from")
                    continue
                obj.set_editor_property(key, unreal.get_default_object(native).get_editor_property(key))
                continue
            current = obj.get_editor_property(key)
            obj.set_editor_property(key, convert(key, current, value))
        except Exception as exc:  # keep going: one bad property shouldn't stop the hero
            fail(f"{where}: {key} = {value!r}: {exc}")


# ---- assets ----------------------------------------------------------------------------------------------------------

def snake(name):
    """C++ property name to its Python name (SecondryFireAbility -> secondry_fire_ability)."""
    return re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", name).lower()


def asset_path(folder, name):
    return f"{folder}/{name}"


def duplicate(source, folder, name):
    path = asset_path(folder, name)
    if EAL.does_asset_exist(path):
        return EAL.load_asset(path)
    asset = AT.duplicate_asset(name, folder, EAL.load_asset(source))
    if not asset:
        fail(f"couldn't duplicate {source} to {path}")
    return asset


def blueprint(folder, name, parent):
    path = asset_path(folder, name)
    if EAL.does_asset_exist(path):
        bp = EAL.load_asset(path)
        current = bp.get_editor_property("parent_class")
        if parent and current != parent:
            BEL.reparent_blueprint(bp, parent)
            log(f"reparented {path} to {parent.get_name()}")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        bp = AT.create_asset(name, folder, unreal.Blueprint, factory)
        if not bp:
            fail(f"couldn't create {path}")
            return None
    BEL.compile_blueprint(bp)
    return bp


def cdo(bp):
    return unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(bp.get_path_name()))


def generated(bp):
    return unreal.EditorAssetLibrary.load_blueprint_class(bp.get_path_name())


def kit_class(name):
    cls = unreal.load_class(None, hero_specs.KIT + name)
    if not cls:
        fail(f"no class {name}: did the C++ build run?")
    return cls


def save(asset):
    EAL.save_loaded_asset(asset, False)


def ensure_dir(path):
    if not EAL.does_directory_exist(path):
        EAL.make_directory(path)


# ---- per hero --------------------------------------------------------------------------------------------------------

def tune_projectile(bp, settings, where):
    obj = cdo(bp)
    movement = obj.get_editor_property("projectile_movement")
    movement.set_editor_property("initial_speed", float(settings["speed"]))
    movement.set_editor_property("max_speed", float(settings["speed"]))
    movement.set_editor_property("projectile_gravity_scale", float(settings.get("gravity", 0.0)))
    bounces = settings.get("bounces", 0)
    bounce = settings.get("bounce", 0.0)
    if bounces or bounce:
        movement.set_editor_property("should_bounce", True)
        movement.set_editor_property("bounciness", float(bounce or 0.6))
    if bounces:
        set_props(obj, {"limit_bounces": True, "maximum_bounces": bounces}, where)
    set_props(obj, {"initial_life_span": settings["life"]}, where)


def bullet_fx(hero, bp, weapon, where):
    """Weapon bullets (AProjectile copies): Niagara through gameplay cues (TrailCue, HitCharacterCue, HitWorldCue).
    The template's Cascade trail and hit particles are cleared: heroes use Niagara only."""
    obj = cdo(bp)
    CONTEXT[0], CONTEXT[1] = hero, "Weapon"
    try:
        obj.get_editor_property("trail_comp").set_editor_property("template", None)
        obj.set_editor_property("hit_character_particle", None)
        obj.set_editor_property("hit_world_particle", None)
    except Exception as exc:
        fail(f"{where} clear cascade: {exc}")
    set_props(obj, {k: weapon[k] for k in ("trail_cue", "hit_character_cue", "hit_world_cue") if k in weapon}, where)


def ensure_ice_material():
    """M_HeroKit_Ice: glossy, a "Color" parameter for base colour, fresnel glow at the edges. Used by ice spikes."""
    folder, name = "/Game/ZERO/HeroKit/FX", "M_HeroKit_Ice"
    if EAL.does_asset_exist(f"{folder}/{name}"):
        return
    ensure_dir(folder)
    mel = unreal.MaterialEditingLibrary
    mat = AT.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.55, 0.85, 1.0, 1.0))
    fresnel = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -600, 250)
    fresnel.set_editor_property("exponent", 3.0)
    glow = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 200)
    strength = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -600, 400)
    strength.set_editor_property("r", 4.0)
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, 250)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 450)
    rough.set_editor_property("r", 0.08)
    mel.connect_material_expressions(fresnel, "", glow, "A")
    mel.connect_material_expressions(strength, "", glow, "B")
    mel.connect_material_expressions(glow, "", tint, "A")
    mel.connect_material_expressions(color, "", tint, "B")
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    save(mat)
    log("created M_HeroKit_Ice")


def build_weapon(hero, spec, folder):
    weapon = spec["weapon"]
    bullet = duplicate(BULLET_TEMPLATE, folder, f"BP_{hero}_Bullet")
    bullet_server = duplicate(BULLET_SERVER_TEMPLATE, folder, f"BP_{hero}_Bullet_Server")
    for bp in (bullet, bullet_server):
        tune_projectile(bp, weapon, f"{hero} bullet")
        bullet_fx(hero, bp, weapon, f"{hero} bullet")
        save(bp)
    weapon_bp = duplicate(WEAPON_TEMPLATE, folder, f"BP_{hero}_Weapon")
    set_props(cdo(weapon_bp), {
        "projectile_class": generated(bullet), "server_projectile_class": generated(bullet_server),
        "fire_mode": weapon["fire_mode"], "pellet_count": weapon["pellets"], "spread_angle": weapon["pellet_spread"],
        "bullet_spread_angle": weapon["spread"], "ability_name": f"{spec['display']} weapon",
    }, f"{hero} weapon")
    save(weapon_bp)
    return generated(weapon_bp)


def build_ability(hero, spec, entry, slot, with_cooldown=True):
    name, parent_name, props = entry
    CONTEXT[0], CONTEXT[1] = hero, name
    folder = f"{ROOT}/{hero}/Abilities/{name}"
    ensure_dir(folder)
    parent = kit_class(parent_name)
    if not parent:
        return None
    bp = blueprint(folder, f"BP_{hero}_{name}", parent)
    if not bp:
        return None
    obj = cdo(bp)
    where = f"{hero}.{name}"

    values = dict(props)
    if "projectile" in props:
        proj_bp = blueprint(folder, f"BP_{hero}_{name}_Projectile", kit_class("ZL_Kit_Projectile"))
        p = props["projectile"]
        proj_obj = cdo(proj_bp)
        tune_projectile(proj_bp, p, where + " projectile")
        extra = {"placeholder_color": p["color"]}
        for key in ("pierce", "return_after", "hit_team", "hide_placeholder", "trail_cue", "impact_cue"):
            if key in p:
                extra[key] = p[key]
        set_props(proj_obj, extra, where + " projectile")
        try:
            proj_obj.get_editor_property("hitbox_comp").set_capsule_size(float(p["hitbox"]), float(p["hitbox"]))
            scale = max(p["hitbox"] / 100.0, 0.15)
            proj_obj.get_editor_property("placeholder_mesh").set_editor_property("relative_scale3d", unreal.Vector(scale, scale, scale))
        except Exception as exc:
            fail(f"{where} projectile hitbox: {exc}")
        save(proj_bp)
        values["projectile_class"] = generated(proj_bp)

    if with_cooldown:
        values["cooldown_tags"] = [f"ZerolockAbilities.Cooldown.{hero}.{name}"]
    if slot is not None:
        values["slot"] = slot
    set_props(obj, values, where, parent)
    save(bp)
    log(f"{where}: {parent_name}")
    return generated(bp)


def add_hero_row(hero, da):
    table = EAL.load_asset(HERO_LIST)
    rows = [str(r) for r in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
    if hero not in hero_specs.SELECTOR_HEROES:
        return  # built but hidden from hero select (hero_specs.SELECTOR_HEROES)
    if hero in rows:
        return
    try:
        data = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    except Exception as exc:
        fail(f"DT_HeroList export: {exc}")
        return
    template = next((r for r in data if r.get("Name") == "SCombo"), data[0])
    row = dict(template)
    row["Name"] = hero
    for key in row:
        if key.lower() == "heroasset":
            row[key] = f"{da.get_path_name()}"
    data.append(row)
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(data)):
        fail("DT_HeroList fill failed")
        return
    save(table)


def build_hero(hero, spec):
    folder = f"{ROOT}/{hero}"
    for sub in ("", "/Abilities", "/Weapon", "/Montages", "/Materials", "/Textures"):
        ensure_dir(folder + sub)

    hero_bp = duplicate(HERO_TEMPLATE, folder, f"BP_{hero}")
    da = duplicate(DA_TEMPLATE, folder, f"DA_{hero}_Hero")
    ge = duplicate(GE_TEMPLATE, folder, f"GE_{hero}_DefaultStats")
    weapon_class = build_weapon(hero, spec, folder + "/Weapon")

    hero_cdo = cdo(hero_bp)
    values = {"primary_fire_ability": weapon_class, "default_gameplay_effect": generated(ge)}
    for key, (prop, slot) in SLOTS.items():
        entry = spec.get(key)
        cls = build_ability(hero, spec, entry, slot) if entry else None
        if prop:  # f abilities are built but not assigned
            values[snake(prop)] = cls
    values["alt_fire_ability"] = unreal.load_class(None, "/Script/ZeroLock.ZL_GA_AimZoom")

    defaults = [c for c in hero_cdo.get_editor_property("default_abilities") if c and SCOMBO_PASSIVE not in c.get_name()
                and f"/Heroes/{hero}/" not in c.get_path_name()]
    for entry in spec.get("passives", []):
        cls = build_ability(hero, spec, entry, None, with_cooldown=False)
        if cls:
            defaults.append(cls)
    values["default_abilities"] = defaults
    for key, value in values.items():
        try:
            hero_cdo.set_editor_property(key, value)
        except Exception as exc:
            fail(f"{hero} BP {key}: {exc}")
    save(hero_bp)

    set_props(da, {"character_name": spec["display"], "character_description": spec["desc"],
                   "character_class": generated(hero_bp), "icon_color": spec["color"]}, f"{hero} DA")
    save(da)
    save(ge)
    add_hero_row(hero, da)
    log(f"{hero}: done")


def main(only):
    for hero, spec in HEROES.items():
        if only and hero not in only:
            continue
        try:
            build_hero(hero, spec)
        except Exception as exc:
            fail(f"{hero}: {exc}")
    log(f"finished with {len(errors)} error(s)")
    for e in errors:
        log("  " + e)


main([a for a in sys.argv[1:] if not a.startswith("-")])
