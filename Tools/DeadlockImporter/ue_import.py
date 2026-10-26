"""
Deadlock hero importer (step 2 of 2): runs inside the Unreal editor.

    py "E:/Repo/ZeroLock/ZeroLock/Tools/DeadlockImporter/ue_import.py" Lash Apollo
    py ".../ue_import.py" Lash --no-anims        mesh + materials only
    py ".../ue_import.py" Lash --anims-only      re-import just the animations
    py ".../ue_import.py" Lash --anim-set-only   just (re)build DA_<Hero>_AnimSet for ABP_DeadlockHero
    py ".../ue_import.py" Lash --montages-only   just create Montages/<AbilityName>/AM_<Hero>_<AbilityName>_* (existing ones are kept)
    py ".../ue_import.py" Lash --hero-only       just BP_Deadlock_<Hero> + DA_<Hero>_Hero + hero list row (existing ones are kept)
    py ".../ue_import.py" Lash --camera-only     set the existing BP's spring arm/camera from Deadlock's camera settings
    py ".../ue_import.py" Lash --stats-only      (re)write GE_<Hero>_DefaultStats from Deadlock's stats, set it on the BP
    The --*-only flags combine. A full run on a hero that already has a BP also refreshes its camera, movement, weapon
    and stats GE (other BP edits are kept) - but it re-imports the mesh and every animation, which takes minutes.
    py ".../ue_import.py"                        every hero exported by deadlock_import.bat

Reads Saved/DeadlockImporter/<Hero>/manifest.json written by deadlock_import.py and creates:
    <dest>/SK_<Hero> (+ skeleton, physics asset)
    <dest>/Materials/MI_<Hero>_<material>, <dest>/Textures/T_...
    <dest>/Animations/A_<Hero>_<clip>
    <dest_root>/_Shared/M_DeadlockHero[_Additive|_Translucent|_Masked]   (master materials, built once)
"""
import json
import math
import os
import re
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(HERE, "config.json"), encoding="utf-8") as f:
    CFG = json.load(f)

# Deferred initialization to prevent thread-safety crashes when loaded via C++ console commands[cite: 1]
asset_tools = None
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log(f"[deadlock] {msg}")


def warn(msg):
    unreal.log_warning(f"[deadlock] {msg}")


def load(path):
    return eal.load_asset(path) if eal.does_asset_exist(path) else None


def _text_value(value):
    if isinstance(value, bool):
        return "True" if value else "False"
    if isinstance(value, str):
        return f'"{value}"'
    return repr(float(value)) if isinstance(value, float) else str(value)


def set_struct_fields(struct, fields):
    """Sets struct fields through the struct's text form. set_editor_property refuses EditDefaultsOnly fields on a
    struct ("cannot be edited on instances"), even when the struct lives on a CDO; import_text has no such check.
    fields: {"ExportedFieldName": value}, or {"Outer.Inner": value} for one level of nesting."""
    text = struct.export_text()
    for key, value in fields.items():
        *outer, name = key.split(".")
        prefix = "".join(rf"{o}=\((?:[^()]|\([^()]*\))*?" for o in outer)
        pattern = rf"([(,]{prefix}(?<=[(,]){name}=)(\"[^\"]*\"|[^,)]*)"
        text, hits = re.subn(pattern, lambda mt: mt.group(1) + _text_value(value), text, count=1)
        if not hits:
            raise RuntimeError(f"{type(struct).__name__} has no field {key} in: {text[:300]}")
    struct.import_text(text)
    return struct


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

ANIM_SET_SLOTS = {
    "stand_idle": ["weapon_stand_idle", "primary_stand_idle", "primary_idle", "out_of_combat_stand_idle", "item_stand_idle"],
    "crouch_idle": ["weapon_crouch_idle", "primary_crouch_idle", "out_of_combat_crouch_idle", "item_crouch_idle"],
    "jump_start": ["jump_ground", "jump_start"],
    "air_jump_start": ["jump_air", "jump_ground"],
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
    "stun_loop": ["standing_stunned", "crouching_stunned", "knockdown_medium_loop"],
    "heavy_melee_start": ["melee_start"],
    "heavy_melee_loop": ["melee_dash"],
    "heavy_melee_hit": ["melee_hit"],
    "heavy_melee_air_start": ["melee_in_air_start"],
    "heavy_melee_air_loop": ["melee_in_air_dash"],
    "heavy_melee_air_hit": ["melee_in_air_hit"],
}

# Deadlock's mantle clips are named by ledge height in Source units; the cm values are what the ZeroLock capsule
# (176 cm, mantles up to ~226 cm) reaches with each. The anim node picks the nearest.
MANTLE_CLIPS = [("mantle_32", 60.0), ("mantle_64", 110.0), ("mantle_96", 160.0), ("mantle_128", 210.0)]

ANIM_SET_MONTAGES = {
    "light_melee_montage": ["Melee/AM_{hero}_Melee_Quick1", "Melee/AM_{hero}_Melee_Quick2"],
    "reload_montage": ["Reload/AM_{hero}_Reload"],
    "shoot_montage": ["Shoot/AM_{hero}_Shoot"],
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

    additive = set(m.get("additive_clips", []))

    def clip(names, full_pose=True):
        """First existing clip. Full-pose slots skip Deadlock additive clips (e.g. landing_impact): composed over the
        bind pose they play as a T-pose flash. The aim slots take them (the node only uses Up/Down minus Center)."""
        for n in names:
            if full_pose and n in additive:
                continue
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

    mantles = []
    for name, height in MANTLE_CLIPS:
        seq = clip([name])
        if seq:
            entry = unreal.ZLMantleAnim()
            entry.set_editor_property("clip", seq)
            entry.set_editor_property("ledge_height", height)
            mantles.append(entry)
    aset.set_editor_property("mantles", mantles)
    if not mantles:
        missing.append("mantles")

    for prop, names in ANIM_SET_MONTAGES.items():
        montage = next((m for m in (load(f"{dest}/Montages/{n.format(hero=hero)}") for n in names) if m), None)
        aset.set_editor_property(prop, montage)
        if montage is None:
            missing.append(prop)

    for prop, bases in AIM_SLOTS.items():
        aim = aset.get_editor_property(prop)
        for field, suffix in (("center", ""), ("up", "_up"), ("down", "_down")):
            aim.set_editor_property(field, clip([b + suffix for b in bases], full_pose=False))
        aset.set_editor_property(prop, aim)
        if aim.get_editor_property("center") is None:
            missing.append(prop)

    eal.save_loaded_asset(aset)
    log(f"  anim set {path}" + (f" (no clip for: {', '.join(missing)})" if missing else ""))
    return aset


# --------------------------------------------------------------------------- ability montages

GENERIC_TOKENS = {"citadel", "ability", "the", "and", "of", "up", "down", "left", "right", "start", "end",
                  "loop", "in", "air", "aim", "hit", "dash", "n", "e", "s", "w", "x"}
MELEE_CLIPS = ["melee_quick_1", "melee_quick_2", "melee_start", "melee_dash", "melee_hit",
               "melee_in_air_start", "melee_in_air_dash", "melee_in_air_hit"]
# Upper-body weapon montages: (name, first clip found, anim set property). UpperBody slot, legs keep running.
WEAPON_MONTAGES = [
    ("Reload", ["reload_idle", "reload_stand_idle", "reload_quick_idle", "reload_run", "primary_stand_reload"], "reload_montage"),
    ("Shoot", ["shoot_idle", "shoot_run", "primary_stand_shoot"], "shoot_montage"),
]
WEAPON_MONTAGE_SLOT = "UpperBody"
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


def make_additive(m, seq):
    """Deadlock additive clips (manifest additive_clips, e.g. shoot_idle) come out of S2V composed over the bind pose,
    which plays as a near T-pose. Additive to the ref pose subtracts the bind pose again, so the montage layers just
    the delta over the live pose, as in Deadlock. Only for montage clips: the anim node samples its clips directly."""
    short = seq.get_name()[len(f"A_{m['hero']}_"):]
    if short not in m.get("additive_clips", []):
        return False
    if seq.get_editor_property("additive_anim_type") != unreal.AdditiveAnimationType.AAT_LOCAL_SPACE_BASE \
            or seq.get_editor_property("ref_pose_type") != unreal.AdditiveBasePoseType.ABPT_REF_POSE:
        seq.set_editor_property("additive_anim_type", unreal.AdditiveAnimationType.AAT_LOCAL_SPACE_BASE)
        seq.set_editor_property("ref_pose_type", unreal.AdditiveBasePoseType.ABPT_REF_POSE)
        eal.save_loaded_asset(seq)
    return True


def build_montages(m, dest):
    if "additive_clips" not in m:
        warn(f"  manifest has no additive_clips - run: deadlock_import.bat {m['hero']} --data-only "
             f"(else additive clips such as shoot_idle play as a T-pose)")
    hero = m["hero"]
    anim_dir, root = f"{dest}/Animations", f"{dest}/Montages"
    clips = {a["name"]: load(f"{anim_dir}/A_{hero}_{a['name']}") for a in m["animations"]}
    clips = {k: v for k, v in clips.items() if v}
    abilities = [a for a in hero_abilities(m) if a["slot"].startswith("ESlot_Signature")]
    hero_tokens = {m.get("codename", "").lower(), hero.lower()}

    plan = []
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
    additive = []
    for seq, folder, name in plan:
        montage, is_new = create_montage(seq, folder, name)
        created += is_new
        summary.setdefault(folder.rsplit("/", 1)[-1], []).append(name)
        if make_additive(m, seq):
            additive.append(name)
    for group, names in summary.items():
        log(f"  Montages/{group}: {', '.join(n.split('_', 2)[-1] for n in names)}")
    log(f"  {len(plan)} montages ({created} new) in {root}")
    if additive:
        log(f"  additive (Deadlock layers, ref pose base): {', '.join(additive)}")
    for name, names, prop in WEAPON_MONTAGES:
        build_weapon_montage(m, dest, clips, name, names, prop)
    return summary


def build_weapon_montage(m, dest, clips, name, names, prop):
    """Montages/<Name>/AM_<Hero>_<Name> on the UpperBody slot, also set on the existing anim set (prop)."""
    hero = m["hero"]
    seq = next((clips[c] for c in names if c in clips), None)
    if seq is None:
        warn(f"  no {name.lower()} clip (tried {names})")
        return None
    montage, _ = create_montage(seq, f"{dest}/Montages/{name}", f"AM_{hero}_{name}")
    if montage is None:
        return None
    additive = make_additive(m, seq)
    if hasattr(unreal, "ZL_EditorAssetLibrary"):
        unreal.ZL_EditorAssetLibrary.set_montage_slot(montage, WEAPON_MONTAGE_SLOT)
        eal.save_loaded_asset(montage)
    else:
        warn(f"  ZL_EditorAssetLibrary missing (rebuild the editor): set AM_{hero}_{name}'s slot to {WEAPON_MONTAGE_SLOT} by hand")
    aset = load(f"{dest}/DA_{hero}_AnimSet")
    if aset:
        aset.set_editor_property(prop, montage)
        eal.save_loaded_asset(aset)
    log(f"  {name.lower()} montage AM_{hero}_{name} ({seq.get_name()}{', additive' if additive else ''}, slot {WEAPON_MONTAGE_SLOT})")
    return montage


# --------------------------------------------------------------------------- hero BP, hero data, hero list

HERO_SETUP = CFG.get("hero_setup", {})


def duplicate_template(template, path, what):
    if not eal.does_asset_exist(template):
        warn(f"  {what}: template {template} not found (config.json hero_setup)")
        return None
    return eal.duplicate_asset(template, path)


SOURCE_UNIT_CM = 2.54  # S2V exports Source units (inches) as cm; the hero mesh scale shrinks them further
# Deadlock's citadel_camera_hero_fov 90 is a 4:3 horizontal FOV (Source convention). UE keeps the horizontal FOV,
# so the same view at 16:9 is wider.
DEADLOCK_FOV = 90.0
DEADLOCK_FOV_16_9 = math.degrees(2 * math.atan(math.tan(math.radians(DEADLOCK_FOV / 2)) * (16 / 9) / (4 / 3)))


def apply_camera(m, cdo):
    """Spring arm + camera from the model's CitadelCameraSettings_t: pivot at the camera height above the feet, arm
    length = back offset, side offset on the arm's socket (so the arm's collision probe covers it)."""
    cam = m.get("camera") or {}
    if "m_flCameraBackOffset" not in cam:
        warn(f"  no camera data in the manifest - run: deadlock_import.bat {m['hero']} --data-only")
        return False
    k = unit_scale(cdo)
    half_height = cdo.get_editor_property("capsule_component").get_unscaled_capsule_half_height()

    arm = cdo.get_editor_property("spring_arm_component")
    arm.set_editor_property("target_arm_length", round(cam["m_flCameraBackOffset"] * k, 1))
    arm.set_editor_property("socket_offset", unreal.Vector(0, round(-cam.get("m_flCameraSideOffset", 0) * k, 1), 0))
    arm.set_editor_property("relative_location",
                            unreal.Vector(0, 0, round(cam.get("m_flCameraHeightStanding", 0) * k - half_height, 1)))
    camera = cdo.get_editor_property("camera_component")
    camera.set_editor_property("relative_location", unreal.Vector(0, 0, 0))
    camera.set_editor_property("field_of_view", round(DEADLOCK_FOV_16_9, 2))
    log(f"  camera: arm {arm.get_editor_property('target_arm_length')} cm, side "
        f"{arm.get_editor_property('socket_offset').y}, pivot z {arm.get_editor_property('relative_location').z}, "
        f"FOV {round(DEADLOCK_FOV_16_9, 2)}")
    return True


# Deadlock starting stat / weapon field -> UBaseCharAttributeSet attributes it overrides in GE_<Hero>_DefaultStats.
STAT_ATTRIBUTES = {
    "EMaxHealth": ["CurrentHealth", "MaximumHealth"],
    "EBaseHealthRegen": ["HealthRegeneration"],
    "EStamina": ["MaxStamina"],
    "EStaminaRegenPerSecond": ["StaminaRegenRate"],
    "EBulletArmorDamageReduction": ["WeaponResistance"],
    "ETechArmorDamageReduction": ["SpiritResistance"],
    "ELightMeleeDamage": ["LightMeleeDamage"],
    "EHeavyMeleeDamage": ["HeavyMeleeDamage"],
}
WEAPON_ATTRIBUTES = {
    "m_flBulletDamage": ["BulletDamage"],
    "m_flCycleTime": ["FireRate"],
    "m_iClipSize": ["MaxAmmo", "CurrentAmmo"],
    "m_reloadDuration": ["ReloadTime"],
}
# Applied outside the GE (apply_movement, apply_weapon_stats), so not reported as missing.
HANDLED_STATS = {"EMaxMoveSpeed", "ECrouchSpeed", "EGroundDashDistanceInMeters"}
HANDLED_WEAPON = {"m_iBullets", "m_iBurstShotCount", "m_flIntraBurstCycleTime", "m_flDamageFalloffStartRange",
                  "m_flDamageFalloffEndRange", "m_flDamageFalloffEndScale"}


def unit_scale(cdo):
    """cm per Deadlock unit on this hero: S2V's inches->cm times the mesh scale."""
    return SOURCE_UNIT_CM * cdo.get_editor_property("mesh_component").get_editor_property("relative_scale3d").z


def apply_movement(m, cdo):
    """Mover walk / crouch / dash speed: our tuned values scaled by the hero's Deadlock stat over the base hero's
    (config hero_setup.movement_reference)."""
    stats = (m.get("stats") or {}).get("starting", {})
    ref = HERO_SETUP.get("movement_reference", {})

    def scaled(stat, ours):
        if stat in stats and ref.get(stat) and ours in ref:
            return round(ref[ours] * stats[stat] / ref[stat], 1)
        return None

    walk = scaled("EMaxMoveSpeed", "MaxSpeed")
    crouch = scaled("ECrouchSpeed", "CrouchingMaxSpeed")
    dash = scaled("EGroundDashDistanceInMeters", "DashSpeed")
    mover = cdo.get_editor_property("mover_component")
    for settings in mover.get_editor_property("shared_settings"):
        kind = settings.get_class().get_name() if settings else ""
        if kind == "CommonLegacyMovementSettings" and walk:
            settings.set_editor_property("max_speed", walk)
        elif kind == "StanceSettings" and crouch:
            settings.set_editor_property("crouching_max_speed", crouch)
    if dash:
        mover.set_editor_property("dash_speed", dash)
    log(f"  movement: walk {walk}, crouch {crouch}, dash {dash} cm/s")


UE_GRAVITY = 980.0


def build_projectile(m, dest, k):
    """BP_<Hero>_Projectile: a copy of template_projectile (tracer, damage from the shooter's stats) with the hero's
    Deadlock bullet speed, gravity and range. Made once, so speed and the rest can be tuned on the asset afterwards."""
    hero = m["hero"]
    path = f"{dest}/BP_{hero}_Projectile"
    if eal.does_asset_exist(path):
        return eal.load_blueprint_class(path)
    bp = duplicate_template(HERO_SETUP.get("template_projectile", ""), path, "projectile")
    if bp is None:
        return None
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cls = eal.load_blueprint_class(path)
    cdo = unreal.get_default_object(cls)

    weapon = (m.get("stats") or {}).get("weapon", {})
    speed = weapon.get("m_flBulletSpeed", 0) * k
    gravity = weapon.get("m_flBulletGravityScale", 0) * HERO_SETUP.get("deadlock_gravity", 800) * k / UE_GRAVITY
    move = cdo.get_editor_property("projectile_movement")
    if speed > 0:
        move.set_editor_property("initial_speed", round(speed))
        move.set_editor_property("max_speed", round(speed))
    move.set_editor_property("projectile_gravity_scale", round(gravity, 3))
    if speed > 0 and weapon.get("m_flRange", 0) > 0:
        cdo.set_editor_property("initial_life_span", round(max(0.1, weapon["m_flRange"] * k / speed), 3))
    eal.save_loaded_asset(bp)
    log(f"  projectile {path}: {round(speed)} cm/s, gravity x{round(gravity, 3)}, "
        f"life {cdo.get_editor_property('initial_life_span')} s")
    return cls


def apply_weapon_stats(m, dest, cdo):
    """AZeroLockCharacter::WeaponStats: pellets, burst and damage falloff (ranges in cm) from the Deadlock weapon."""
    weapon = (m.get("stats") or {}).get("weapon", {})
    if not weapon:
        return
    k = unit_scale(cdo)
    stats = cdo.get_editor_property("weapon_stats")
    bone = (m["stats"].get("muzzle") or {}).get("bone", "")
    mesh = cdo.get_editor_property("mesh_component").get_editor_property("skeletal_mesh_asset")
    bones = [str(b) for b in mesh.skeleton.get_reference_pose().get_bone_names()] if mesh and mesh.skeleton else []
    fields = {
        "bOverrideFireMode": True,
        "Pellets": max(1, int(weapon.get("m_iBullets", 1))),
        "BurstShots": max(1, int(weapon.get("m_iBurstShotCount", 1))),
        "FalloffStartRange": round(weapon.get("m_flDamageFalloffStartRange", 0) * k, 1),
        "FalloffEndRange": round(weapon.get("m_flDamageFalloffEndRange", 0) * k, 1),
        "FalloffEndScale": float(weapon.get("m_flDamageFalloffEndScale", 1.0)),
    }
    if bone and bone in bones:
        fields["MuzzleSocket"] = bone
    else:
        warn(f"  muzzle bone '{bone}' not on the mesh - shots keep leaving from in front of the hero")
    if weapon.get("m_flIntraBurstCycleTime", 0) > 0:
        fields["BurstInterval"] = float(weapon["m_flIntraBurstCycleTime"])
    if "ProjectileClass=" in stats.export_text():
        projectile = build_projectile(m, dest, k)
        if projectile:
            fields["ProjectileClass"] = projectile.get_path_name()
    else:
        warn("  WeaponStats has no ProjectileClass yet (rebuild the editor): no per-hero projectile")
    set_struct_fields(stats, fields)
    cdo.set_editor_property("weapon_stats", stats)
    stats = cdo.get_editor_property("weapon_stats")
    log(f"  weapon: muzzle {stats.get_editor_property('muzzle_socket')}, {stats.get_editor_property('pellets')} pellet(s), burst {stats.get_editor_property('burst_shots')}, "
        f"falloff {stats.get_editor_property('falloff_start_range')}-{stats.get_editor_property('falloff_end_range')} cm "
        f"to x{stats.get_editor_property('falloff_end_scale')}")


def stat_values(m):
    """{attribute: value} for the stats GE. Resistances default to 0 (Deadlock omits them when the hero has none)."""
    stats = m.get("stats") or {}
    values = {"WeaponResistance": 0.0, "SpiritResistance": 0.0}
    for source, mapping in ((stats.get("starting", {}), STAT_ATTRIBUTES), (stats.get("weapon", {}), WEAPON_ATTRIBUTES)):
        for key, attrs in mapping.items():
            if key in source:
                values.update({a: float(source[key]) for a in attrs})
    unmapped = {k: v for k, v in stats.get("starting", {}).items() if k not in STAT_ATTRIBUTES and k not in HANDLED_STATS}
    unmapped.update({k: v for k, v in stats.get("weapon", {}).items() if k not in WEAPON_ATTRIBUTES and k not in HANDLED_WEAPON})
    return values, unmapped


def build_stats_ge(m, dest, refresh):
    """GE_<Hero>_DefaultStats: a copy of the shared template (Instant, Override per attribute) with this hero's
    Deadlock values. An existing one is only rewritten with refresh (--stats-only)."""
    hero = m["hero"]
    path = f"{dest}/GE_{hero}_DefaultStats"
    if not (m.get("stats") or {}).get("starting"):
        warn(f"  no stats in the manifest - run: deadlock_import.bat {hero} --data-only")
        return eal.load_blueprint_class(path) if eal.does_asset_exist(path) else None
    exists = eal.does_asset_exist(path)
    if exists and not refresh:
        log(f"  {path} exists, kept")
        return eal.load_blueprint_class(path)

    bp = load(path) if exists else duplicate_template(HERO_SETUP.get("template_stats_ge", ""), path, "stats GE")
    if bp is None:
        return None
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cls = eal.load_blueprint_class(path)
    cdo = unreal.get_default_object(cls)

    values, unmapped = stat_values(m)
    # Struct elements read from an unreal.Array are copies, so build a new list and write it back.
    mods, written = [], set()
    for mod in cdo.get_editor_property("modifiers"):
        attr = str(mod.get_editor_property("attribute").get_editor_property("attribute_name"))
        if attr in values and attr not in written:
            set_struct_fields(mod, {"ModifierMagnitude.ScalableFloatMagnitude.Value": values[attr]})
            written.add(attr)
        mods.append(mod)
    cdo.set_editor_property("modifiers", mods)
    eal.save_loaded_asset(bp)

    log(f"  stats GE {path}: " + ", ".join(f"{a} {values[a]:g}" for a in sorted(written)))
    if set(values) - written:
        warn(f"  template has no modifier for: {sorted(set(values) - written)}")
    if unmapped:
        log(f"  Deadlock values with no attribute (not applied): " + ", ".join(f"{k} {v:g}" for k, v in unmapped.items()))
    return cls


def build_hero_bp(m, dest, aset, stats_ge=None):
    """BP_Deadlock_<Hero>: a copy of the template hero (Mover setup, weapon, abilities) with this hero's mesh,
    anim blueprint, anim set, melee montages, Deadlock camera and default stats GE. An existing BP is left alone
    (it may have hand edits)."""
    hero = m["hero"]
    path = f"{dest}/BP_Deadlock_{hero}"
    if eal.does_asset_exist(path):
        log(f"  {path} exists, kept")
        return eal.load_blueprint_class(path)

    bp = duplicate_template(HERO_SETUP.get("template_bp", ""), path, "hero BP")
    if bp is None:
        return None
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cls = eal.load_blueprint_class(path)
    cdo = unreal.get_default_object(cls)

    mesh = cdo.get_editor_property("mesh_component")
    mesh.set_editor_property("skeletal_mesh_asset", load(f"{dest}/SK_{hero}"))
    abp = eal.load_blueprint_class(HERO_SETUP.get("anim_blueprint", ""))
    if abp:
        mesh.set_editor_property("anim_class", abp)
    cdo.set_editor_property("deadlock_anim_set", aset)
    # A duplicate keeps the template's montages, which win over the anim set's: point them at this hero's.
    cdo.set_editor_property("light_melee_montage", load(f"{dest}/Montages/Melee/AM_{hero}_Melee_Quick1"))
    cdo.set_editor_property("heavy_melee_montage", load(f"{dest}/Montages/Melee/AM_{hero}_Melee_Hit"))
    apply_camera(m, cdo)
    apply_movement(m, cdo)
    apply_weapon_stats(m, dest, cdo)
    if stats_ge:
        cdo.set_editor_property("default_gameplay_effect", stats_ge)

    eal.save_loaded_asset(bp)
    log(f"  hero BP {path} (abilities copied from {HERO_SETUP.get('template_bp')})")
    return cls


def build_hero_data(m, dest, hero_class):
    """DA_<Hero>_Hero for hero select: a copy of the template's data (icon, colour, preview framing) with this hero's
    name, class, mesh and pose. An existing one is left alone."""
    hero = m["hero"]
    path = f"{dest}/DA_{hero}_Hero"
    if eal.does_asset_exist(path):
        log(f"  {path} exists, kept")
        return load(path)

    data = duplicate_template(HERO_SETUP.get("template_data", ""), path, "hero data")
    if data is None:
        return None
    data.set_editor_property("character_name", m["display"])
    data.set_editor_property("character_description", "")
    data.set_editor_property("character_class", hero_class)
    data.set_editor_property("display_seletal_mesh", load(f"{dest}/SK_{hero}"))
    pose = next((s for s in (load(f"{dest}/Animations/A_{hero}_{n}") for n in HERO_SETUP.get("display_anims", [])) if s), None)
    data.set_editor_property("display_animation", pose)
    data.set_editor_property("display_animation_pose_time", 0.0)

    eal.save_loaded_asset(data)
    log(f"  hero data {path} (pose {pose.get_name() if pose else 'none'}; icon, colour and preview framing are the "
        f"template's: change them on the asset)")
    return data


def add_to_hero_list(m, data):
    """Adds a <Hero> row pointing at DA_<Hero>_Hero to the hero select table, unless one exists."""
    table = load(HERO_SETUP.get("hero_list", ""))
    if table is None or data is None:
        warn(f"  hero list {HERO_SETUP.get('hero_list')} not found, row not added")
        return
    lib = unreal.DataTableFunctionLibrary
    hero = m["hero"]
    if hero in [str(n) for n in lib.get_data_table_row_names(table)]:
        log(f"  hero list already has {hero}")
        return
    rows = json.loads(lib.export_data_table_to_json_string(table))
    rows.append({"Name": hero, "HeroAsset": data.get_path_name()})
    lib.fill_data_table_from_json_string(table, json.dumps(rows))
    eal.save_loaded_asset(table)
    log(f"  hero list: added {hero} to {table.get_path_name()}")


def setup_hero(m, dest):
    if eal.does_asset_exist(f"{dest}/BP_Deadlock_{m['hero']}"):
        # Existing BP: keep its hand edits but refresh what the importer owns (camera, movement, weapon, stats GE).
        update_existing_bp(m, dest, camera=True, stats=True)
    aset = load(f"{dest}/DA_{m['hero']}_AnimSet")
    hero_class = build_hero_bp(m, dest, aset, build_stats_ge(m, dest, refresh=False))
    if hero_class is None:
        return
    add_to_hero_list(m, build_hero_data(m, dest, hero_class))


def update_existing_bp(m, dest, camera, stats):
    """--camera-only / --stats-only on a BP that already exists."""
    path = f"{dest}/BP_Deadlock_{m['hero']}"
    bp = load(path)
    if bp is None:
        warn(f"  {path} not found - run a full import or --hero-only first")
        return
    stats_ge = build_stats_ge(m, dest, refresh=True) if stats else None
    cdo = unreal.get_default_object(eal.load_blueprint_class(path))
    if camera:
        apply_camera(m, cdo)
    if stats:
        apply_movement(m, cdo)
        apply_weapon_stats(m, dest, cdo)
    if stats_ge:
        cdo.set_editor_property("default_gameplay_effect", stats_ge)
    eal.save_loaded_asset(bp)


# --------------------------------------------------------------------------- main

def import_hero(hero, no_anims=False, anims_only=False, anim_set_only=False, montages_only=False, hero_only=False,
                camera_only=False, stats_only=False):
    path = os.path.join(CFG["output_dir"], hero, "manifest.json")
    if not os.path.exists(path):
        raise RuntimeError(f"No export for '{hero}' - run: deadlock_import.bat {hero}")
    with open(path, encoding="utf-8") as f:
        m = json.load(f)
    dest = m["ue_dest"]
    log(f"=== {m['display']} -> {dest}")
    # The --*-only steps combine (e.g. --montages-only --camera-only --stats-only).
    if montages_only or anim_set_only or hero_only or camera_only or stats_only:
        if montages_only:
            build_montages(m, dest)
        if anim_set_only:
            build_anim_set(m, dest)
        if hero_only:
            setup_hero(m, dest)
        if camera_only or stats_only:
            update_existing_bp(m, dest, camera_only, stats_only)
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
    if m["animations"]:
        build_montages(m, dest)
    if m["animations"] and hasattr(unreal, "ZL_DeadlockAnimSet"):
        build_anim_set(m, dest)
    if not anims_only and not no_anims and m["animations"]:
        setup_hero(m, dest)

    eal.save_directory(dest, only_if_is_dirty=True, recursive=True)
    log(f"=== {m['display']} done")


def main(argv):
    global asset_tools
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    no_anims = "--no-anims" in argv
    anims_only = "--anims-only" in argv
    anim_set_only = "--anim-set-only" in argv
    montages_only = "--montages-only" in argv
    hero_only = "--hero-only" in argv
    camera_only = "--camera-only" in argv
    stats_only = "--stats-only" in argv
    heroes = [a for a in argv if not a.startswith("--")]
    if not heroes:
        out = CFG["output_dir"]
        heroes = sorted(d for d in os.listdir(out) if os.path.exists(os.path.join(out, d, "manifest.json")))
        log(f"Importing all exported heroes: {heroes}")
    for h in heroes:
        match = next((d for d in os.listdir(CFG["output_dir"]) if d.lower() == h.lower().replace(" ", "")), h)
        import_hero(match, no_anims, anims_only, anim_set_only, montages_only, hero_only, camera_only, stats_only)


if __name__ == "__main__":
    main(sys.argv[1:])