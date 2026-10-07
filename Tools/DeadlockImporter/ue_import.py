"""
Deadlock hero importer (step 2 of 2): runs inside the Unreal editor.

    py "E:/Repo/ZeroLock/ZeroLock/Tools/DeadlockImporter/ue_import.py" Lash Apollo
    py ".../ue_import.py" Lash --no-anims        mesh + materials only
    py ".../ue_import.py" Lash --anims-only      re-import just the animations
    py ".../ue_import.py" Lash --anim-set-only   just (re)build DA_<Hero>_AnimSet for ABP_DeadlockHero
    py ".../ue_import.py" Lash --montages-only   just create Montages/<AbilityName>/AM_<Hero>_<AbilityName>_* (existing ones are kept)
    py ".../ue_import.py"                        every hero exported by deadlock_import.bat

Reads Saved/DeadlockImporter/<Hero>/manifest.json written by deadlock_import.py and creates:
    <dest>/SK_<Hero> (+ skeleton, physics asset)
    <dest>/Materials/MI_<Hero>_<material>, <dest>/Textures/T_...
    <dest>/Animations/A_<Hero>_<clip>
    <dest_root>/_Shared/M_DeadlockHero[_Additive|_Translucent|_Masked]   (master materials, built once)
"""
import json
import os
import re
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(HERE, "config.json"), encoding="utf-8") as f:
    CFG = json.load(f)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log(f"[deadlock] {msg}")


def warn(msg):
    unreal.log_warning(f"[deadlock] {msg}")


def load(path):
    return eal.load_asset(path) if eal.does_asset_exist(path) else None


# --------------------------------------------------------------------------- master materials

SHARED = f"{CFG['ue_dest_root'].rstrip('/')}/_Shared"

DEFAULT_TEX = {
    "white": "/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture",
    "black": "/Engine/EngineResources/Black.Black",
    "normal": "/Engine/EngineMaterials/DefaultNormal.DefaultNormal",
}


def default_texture(kind):
    tex = unreal.load_asset(DEFAULT_TEX[kind])
    if tex is None and kind == "black":
        tex = unreal.load_asset("/Engine/EngineResources/BlackSquareTexture.BlackSquareTexture")
    return tex


def master_material(blend):
    """M_DeadlockHero (opaque/masked/translucent PBR) or M_DeadlockHero_Additive (unlit glow)."""
    suffix = {"opaque": "", "masked": "_Masked", "translucent": "_Translucent", "additive": "_Additive"}[blend]
    name = f"M_DeadlockHero{suffix}"
    existing = load(f"{SHARED}/{name}")
    if existing:
        return existing

    log(f"Creating master material {name}")
    mat = asset_tools.create_asset(name, SHARED, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", {
        "opaque": unreal.BlendMode.BLEND_OPAQUE, "masked": unreal.BlendMode.BLEND_MASKED,
        "translucent": unreal.BlendMode.BLEND_TRANSLUCENT, "additive": unreal.BlendMode.BLEND_ADDITIVE}[blend])
    mat.set_editor_property("used_with_skeletal_mesh", True)

    def tex_param(pname, sampler, default, x, y):
        n = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        n.set_editor_property("parameter_name", pname)
        n.set_editor_property("sampler_type", sampler)
        n.set_editor_property("texture", default_texture(default))
        return n

    def vec_param(pname, value, x, y):
        n = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
        n.set_editor_property("parameter_name", pname)
        n.set_editor_property("default_value", unreal.LinearColor(*value))
        return n

    def scalar_param(pname, value, x, y):
        n = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
        n.set_editor_property("parameter_name", pname)
        n.set_editor_property("default_value", value)
        return n

    def multiply(a, a_out, b, b_out, x, y):
        n = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, x, y)
        mel.connect_material_expressions(a, a_out, n, "A")
        mel.connect_material_expressions(b, b_out, n, "B")
        return n

    P = unreal.MaterialProperty
    S = unreal.MaterialSamplerType
    base = tex_param("BaseColor", S.SAMPLERTYPE_COLOR, "white", -900, -300)
    tint = vec_param("Tint", (1, 1, 1, 1), -900, -80)
    base_tinted = multiply(base, "RGB", tint, "", -500, -250)
    e_tint = vec_param("EmissiveTint", (1, 1, 1, 1), -900, 520)
    e_strength = scalar_param("EmissiveStrength", 0.0 if blend != "additive" else 1.0, -900, 720)

    if blend == "additive":
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property("two_sided", True)
        glow = multiply(base_tinted, "", e_tint, "", -300, 300)
        glow = multiply(glow, "", e_strength, "", -150, 300)
        mel.connect_material_property(glow, "", P.MP_EMISSIVE_COLOR)
    else:
        orm = tex_param("ORM", S.SAMPLERTYPE_LINEAR_COLOR, "white", -900, 100)
        normal = tex_param("Normal", S.SAMPLERTYPE_NORMAL, "normal", -900, 320)
        e_mask = tex_param("EmissiveMask", S.SAMPLERTYPE_LINEAR_COLOR, "black", -1250, 520)
        mel.connect_material_property(base_tinted, "", P.MP_BASE_COLOR)
        mel.connect_material_property(orm, "R", P.MP_AMBIENT_OCCLUSION)
        mel.connect_material_property(orm, "G", P.MP_ROUGHNESS)
        mel.connect_material_property(orm, "B", P.MP_METALLIC)
        mel.connect_material_property(normal, "RGB", P.MP_NORMAL)
        glow = multiply(e_mask, "RGB", e_tint, "", -500, 560)
        glow = multiply(glow, "", e_strength, "", -300, 560)
        mel.connect_material_property(glow, "", P.MP_EMISSIVE_COLOR)
        if blend == "masked":
            mel.connect_material_property(base, "A", P.MP_OPACITY_MASK)
        elif blend == "translucent":
            mel.connect_material_property(base, "A", P.MP_OPACITY)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


# --------------------------------------------------------------------------- textures

def import_texture(png, dest, kind, flip_green):
    """kind: color | linear | normal"""
    name = "T_" + os.path.splitext(os.path.basename(png))[0].replace(".vtex", "").replace(".", "_")
    path = f"{dest}/{name}"
    tex = load(path)
    if tex is None:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", png)
        task.set_editor_property("destination_path", dest)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        asset_tools.import_asset_tasks([task])
        tex = load(path)
        if tex is None:
            warn(f"Texture import failed: {png}")
            return None
    if kind == "normal":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", bool(flip_green))
    elif kind == "linear":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    else:
        tex.set_editor_property("srgb", True)
    eal.save_loaded_asset(tex)
    return tex


def build_materials(m, dest):
    tex_dir, mat_dir = f"{dest}/Textures", f"{dest}/Materials"
    flip = m.get("normal_flip_green", True)
    result = {}
    for info in m["materials"]:
        parent = master_material(info["blend"])
        mi_path = f"{mat_dir}/MI_{m['hero']}_{info['name']}"
        mi = load(mi_path) or asset_tools.create_asset(
            os.path.basename(mi_path), mat_dir, unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)

        def set_tex(param, png, kind):
            if png and os.path.exists(png):
                tex = import_texture(png, tex_dir, kind, flip)
                if tex:
                    mel.set_material_instance_texture_parameter_value(mi, param, tex)

        set_tex("BaseColor", info.get("base_color"), "color")
        if info["blend"] != "additive":
            set_tex("ORM", info.get("orm"), "linear")
            set_tex("Normal", info.get("normal"), "normal")
            if info.get("emissive"):
                set_tex("EmissiveMask", info["emissive"], "linear")
                mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", info.get("emissive_scale", 1.0))
        else:
            mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", info.get("emissive_scale", 1.0))
        mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*info.get("tint", [1, 1, 1]), 1))
        mel.set_material_instance_vector_parameter_value(
            mi, "EmissiveTint", unreal.LinearColor(*info.get("emissive_tint", [1, 1, 1]), 1))
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        result[info["name"]] = mi
    log(f"  {len(result)} material instances")
    return result


# --------------------------------------------------------------------------- Interchange imports

def make_pipeline(anim_only, skeleton=None, asset_name=""):
    p = unreal.InterchangeGenericAssetsPipeline()
    p.set_editor_property("use_source_name_for_asset", True)
    if asset_name:
        p.set_editor_property("asset_name", asset_name)

    common = p.get_editor_property("common_meshes_properties")
    common.set_editor_property("force_all_mesh_as_type", unreal.InterchangeForceMeshType.IFMT_SKELETAL_MESH)

    skel = p.get_editor_property("common_skeletal_meshes_and_animations_properties")
    skel.set_editor_property("import_only_animations", anim_only)
    if skeleton:
        skel.set_editor_property("skeleton", skeleton)

    mesh = p.get_editor_property("mesh_pipeline")
    mesh.set_editor_property("import_static_meshes", False)
    mesh.set_editor_property("import_skeletal_meshes", not anim_only)
    mesh.set_editor_property("create_physics_asset", not anim_only)
    mesh.set_editor_property("combine_skeletal_meshes_behavior",
                             unreal.InterchangeCombineSkeletalMeshesBehavior.BY_SKELETON)

    anim = p.get_editor_property("animation_pipeline")
    anim.set_editor_property("import_animations", anim_only)

    mat = p.get_editor_property("material_pipeline")
    mat.set_editor_property("import_materials", False)
    try:
        mat.get_editor_property("texture_pipeline").set_editor_property("import_textures", False)
    except Exception:
        pass
    return p


def interchange_import(fbx, dest, pipeline):
    source = unreal.InterchangeManager.create_source_data(fbx)
    params = unreal.ImportAssetParameters()
    params.set_editor_property("is_automated", True)
    try:
        params.override_pipelines.append(pipeline)
    except TypeError:
        params.override_pipelines.append(unreal.SoftObjectPath(pipeline.get_path_name()))
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    return manager.import_asset(dest, source, params)


def find_asset(folder, cls):
    for path in eal.list_assets(folder, recursive=False):
        data = unreal.EditorAssetLibrary.find_asset_data(path)
        if data.asset_class_path.asset_name == cls:
            return data.get_asset()
    return None


def import_skeletal_mesh(m, dest, materials):
    log("  importing skeletal mesh...")
    interchange_import(m["skeletal_mesh_fbx"], dest, make_pipeline(False, asset_name=f"SK_{m['hero']}"))
    skm = load(f"{dest}/SK_{m['hero']}") or find_asset(dest, "SkeletalMesh")
    if skm is None:
        raise RuntimeError(f"Skeletal mesh import failed for {m['skeletal_mesh_fbx']} (see Output Log)")

    # Struct elements read from an unreal.Array are copies, so build a new list and write it back.
    slots, unmatched = [], []
    for slot in skm.get_editor_property("materials"):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        mi = materials.get(slot_name) or next(
            (v for k, v in materials.items() if slot_name.lower().startswith(k.lower())), None)
        if mi:
            slot.set_editor_property("material_interface", mi)
        else:
            unmatched.append(slot_name)
        slots.append(slot)
    skm.set_editor_property("materials", slots)
    if unmatched:
        warn(f"  no material for slots: {unmatched}")
    eal.save_loaded_asset(skm)
    return skm


def import_animations(m, dest, skeleton):
    anim_dir = f"{dest}/Animations"
    anims = m["animations"]
    failed = []
    with unreal.ScopedSlowTask(len(anims), f"Importing {m['hero']} animations") as task:
        task.make_dialog(True)
        for a in anims:
            if task.should_cancel():
                warn("  cancelled")
                break
            name = f"A_{m['hero']}_{a['name']}"
            task.enter_progress_frame(1, name)
            interchange_import(a["fbx"], anim_dir, make_pipeline(True, skeleton=skeleton, asset_name=name))
            if not eal.does_asset_exist(f"{anim_dir}/{name}"):
                failed.append(a["name"])
    if failed:
        warn(f"  {len(failed)} animations failed: {failed[:10]}{'...' if len(failed) > 10 else ''}")
    log(f"  {len(anims) - len(failed)}/{len(anims)} animations")


# --------------------------------------------------------------------------- anim set (ABP_DeadlockHero)

DIRS = ["n", "ne", "e", "se", "s", "sw", "w", "nw"]

# Anim set property -> clip names to try, first match wins. Deadlock heroes don't all use the same names.
ANIM_SET_SLOTS = {
    "stand_idle": ["weapon_stand_idle", "primary_stand_idle", "primary_idle", "out_of_combat_stand_idle", "item_stand_idle"],
    "crouch_idle": ["weapon_crouch_idle", "primary_crouch_idle", "out_of_combat_crouch_idle", "item_crouch_idle"],
    "jump_start": ["jump_ground", "jump_start"],
    "air_jump_start": ["jump_air", "jump_ground"],
    # UE's Python names drop the "In" prefix: InAirApex -> air_apex
    "air_apex": ["in_air_apex_loop", "in_air_loop_apex", "jump_in_air_loop"],
    "air_fall": ["in_air_down_loop", "in_air_loop_down", "jump_in_air_loop"],
    "land": ["landing_impact", "land_impact_idle", "jump_landing"],
    "hard_land": ["hard_landing"],
    "wall_jump": ["wall_jump"],
    "slide_start": ["slide_start"],
    "slide_loop": ["slide_loop"],
    "dash_ground": ["dash_ground"],
    "dash_forward": ["dash_air_forward", "primary_airdash"],
    "dash_back": ["dash_air_back", "primary_airdash_bkward"],
    "dash_left": ["dash_air_left", "primary_airdash_left"],
    "dash_right": ["dash_air_right", "primary_airdash_right"],
    "zipline_start": ["zipline_attached_start", "zipline_grab"],
    "zipline_loop": ["zipline_attached_loop", "zipline_loop", "zipline_latched_loop"],
}
DIRECTIONAL_SLOTS = {
    "run": ["weapon_run_{d}", "primary_run_{d}", "out_of_combat_run_{d}"],
    "crouch_run": ["weapon_crouch_run_{d}", "primary_crouch_walk_{d}", "out_of_combat_crouch_run_{d}"],
}
AIM_SLOTS = {
    "stand_aim": ["aim_idle", "aim_weapon_idle", "idle_aim"],
    "crouch_aim": ["aim_crouch", "aim_weapon_crouch"],
    "run_aim": ["aim_run"],
}


def build_anim_set(m, dest):
    anim_dir = f"{dest}/Animations"
    hero = m["hero"]

    def clip(names):
        for n in names:
            seq = load(f"{anim_dir}/A_{hero}_{n}")
            if seq:
                return seq
        return None

    path = f"{dest}/DA_{hero}_AnimSet"
    aset = load(path)
    if aset is None:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.ZL_DeadlockAnimSet)
        aset = asset_tools.create_asset(f"DA_{hero}_AnimSet", dest, unreal.ZL_DeadlockAnimSet, factory)
    missing = []

    for prop, names in ANIM_SET_SLOTS.items():
        seq = clip(names)
        aset.set_editor_property(prop, seq)
        if seq is None:
            missing.append(prop)

    for prop, patterns in DIRECTIONAL_SLOTS.items():
        cycle = aset.get_editor_property(prop)
        found = 0
        for d in DIRS:
            seq = clip([p.format(d=d) for p in patterns])
            cycle.set_editor_property(d, seq)
            found += seq is not None
        aset.set_editor_property(prop, cycle)
        if found < 8:
            missing.append(f"{prop} ({found}/8)")

    for prop, bases in AIM_SLOTS.items():
        aim = aset.get_editor_property(prop)
        for field, suffix in (("center", ""), ("up", "_up"), ("down", "_down")):
            aim.set_editor_property(field, clip([b + suffix for b in bases]))
        aset.set_editor_property(prop, aim)
        if aim.get_editor_property("center") is None:
            missing.append(prop)

    eal.save_loaded_asset(aset)
    log(f"  anim set {path}" + (f" (no clip for: {', '.join(missing)})" if missing else ""))
    return aset


# --------------------------------------------------------------------------- ability montages

# Clip-name words that never identify an ability on their own.
GENERIC_TOKENS = {"citadel", "ability", "the", "and", "of", "up", "down", "left", "right", "start", "end",
                  "loop", "in", "air", "aim", "hit", "dash", "n", "e", "s", "w", "x"}
MELEE_CLIPS = ["melee_quick_1", "melee_quick_2", "melee_start", "melee_dash", "melee_hit",
               "melee_in_air_start", "melee_in_air_dash", "melee_in_air_hit"]
PARRY_CLIPS = ["parry", "parry_idle", "parry_success", "parry_in_air", "parry_in_air_success"]


def pascal(text):
    return "".join(w[:1].upper() + w[1:] for w in re.split(r"[^A-Za-z0-9]+", text) if w)


def hero_abilities(m):
    if m.get("abilities"):
        return m["abilities"]
    cache = os.path.join(CFG["output_dir"], "_cache", "heroes.json")
    if os.path.exists(cache):
        with open(cache, encoding="utf-8") as f:
            return json.load(f).get(m.get("codename", ""), {}).get("abilities", [])
    return []


def match_ability(core, abilities, hero_tokens):
    """Return (ability, leftover clip words) for an 'ability_<core>' clip, or (None, words)."""
    words = core.split("_")
    best, best_hits = None, set()
    for ability in abilities:
        name_words = [w.lower() for w in re.split(r"[^A-Za-z0-9]+", ability["name"]) if w]
        joined = "".join(name_words)
        tokens = {w for w in name_words + ability["key"].lower().split("_") if len(w) >= 3}
        tokens -= GENERIC_TOKENS | hero_tokens
        if "ultimate" in tokens:
            tokens.add("ult")
        hits = {w for w in words if w in tokens or (len(joined) >= 4 and w == joined)}
        if not hits and len(joined) >= 4 and core.startswith(joined):
            hits = {words[0]}
        if len(hits) > len(best_hits):
            best, best_hits = ability, hits
    return best, [w for w in words if w not in best_hits]


def create_montage(seq, folder, name):
    path = f"{folder}/{name}"
    existing = load(path)
    if existing:
        return existing, False
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("source_animation", seq)
    montage = asset_tools.create_asset(name, folder, unreal.AnimMontage, factory)
    if montage:
        eal.save_loaded_asset(montage)
    return montage, True


def build_montages(m, dest):
    """Montages/<AbilityName>/AM_<Hero>_<AbilityName>[_<Part>] for every ability clip, plus Melee/ and Parry/."""
    hero = m["hero"]
    anim_dir, root = f"{dest}/Animations", f"{dest}/Montages"
    clips = {a["name"]: load(f"{anim_dir}/A_{hero}_{a['name']}") for a in m["animations"]}
    clips = {k: v for k, v in clips.items() if v}
    abilities = [a for a in hero_abilities(m) if a["slot"].startswith("ESlot_Signature")]
    hero_tokens = {m.get("codename", "").lower(), hero.lower()}

    plan = []  # (seq, folder, montage name)
    for clip, seq in sorted(clips.items()):
        if not clip.startswith("ability_"):
            continue
        ability, rest = match_ability(clip[len("ability_"):], abilities, hero_tokens)
        group = pascal(ability["name"]) if ability else "Other"
        suffix = pascal("_".join(rest)) if ability else pascal(clip[len("ability_"):])
        plan.append((seq, f"{root}/{group}", f"AM_{hero}_{group}" + (f"_{suffix}" if suffix else "")))
    for group, names in (("Melee", MELEE_CLIPS), ("Parry", PARRY_CLIPS)):
        for clip in names:
            if clip in clips:
                plan.append((clips[clip], f"{root}/{group}", f"AM_{hero}_{group}_{pascal(clip.replace(group.lower(), '', 1))}".rstrip("_")))

    created = 0
    summary = {}
    for seq, folder, name in plan:
        montage, is_new = create_montage(seq, folder, name)
        created += is_new
        summary.setdefault(folder.rsplit("/", 1)[-1], []).append(name)
    for group, names in summary.items():
        log(f"  Montages/{group}: {', '.join(n.split('_', 2)[-1] for n in names)}")
    log(f"  {len(plan)} montages ({created} new) in {root}")
    return summary


# --------------------------------------------------------------------------- main

def import_hero(hero, no_anims=False, anims_only=False, anim_set_only=False, montages_only=False):
    path = os.path.join(CFG["output_dir"], hero, "manifest.json")
    if not os.path.exists(path):
        raise RuntimeError(f"No export for '{hero}' - run: deadlock_import.bat {hero}")
    with open(path, encoding="utf-8") as f:
        m = json.load(f)
    dest = m["ue_dest"]
    log(f"=== {m['display']} -> {dest}")
    if anim_set_only:
        build_anim_set(m, dest)
        return
    if montages_only:
        build_montages(m, dest)
        return

    if anims_only:
        skm = load(f"{dest}/SK_{m['hero']}")
        if skm is None:
            raise RuntimeError(f"SK_{m['hero']} not imported yet - run without --anims-only first")
    else:
        materials = build_materials(m, dest)
        skm = import_skeletal_mesh(m, dest, materials)

    if not no_anims and m["animations"]:
        import_animations(m, dest, skm.get_editor_property("skeleton"))
    if m["animations"] and hasattr(unreal, "ZL_DeadlockAnimSet"):
        build_anim_set(m, dest)
    if m["animations"]:
        build_montages(m, dest)

    eal.save_directory(dest, only_if_is_dirty=True, recursive=True)
    log(f"=== {m['display']} done")


def main(argv):
    no_anims = "--no-anims" in argv
    anims_only = "--anims-only" in argv
    anim_set_only = "--anim-set-only" in argv
    montages_only = "--montages-only" in argv
    heroes = [a for a in argv if not a.startswith("--")]
    if not heroes:
        out = CFG["output_dir"]
        heroes = sorted(d for d in os.listdir(out) if os.path.exists(os.path.join(out, d, "manifest.json")))
        log(f"Importing all exported heroes: {heroes}")
    for h in heroes:
        # manifests are keyed by asset name ("Lash", "GreyTalon"); accept any casing
        match = next((d for d in os.listdir(CFG["output_dir"]) if d.lower() == h.lower().replace(" ", "")), h)
        import_hero(match, no_anims, anims_only, anim_set_only, montages_only)


main(sys.argv[1:])
