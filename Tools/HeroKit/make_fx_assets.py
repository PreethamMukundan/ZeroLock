# Copyright Preetham Mukundan (C) 2026
"""
Hero kit FX source assets. Editor Python, headless (editor closed) or in the editor:
  1. M_HeroKit_Ice: glossy material coloured by the Niagara particle colour, fresnel edge glow, Niagara mesh/sprite usage.
  2. Niagara copies of the Paragon (Cascade) effects listed in CONVERT, through Epic's CascadeToNiagaraConverter plugin
     (enabled in ZeroLock.uproject), moved to /Game/ZERO/HeroKit/FX/Niagara/NS_<name>. Existing ones are skipped.
The hero kit only uses Niagara; the Paragon Cascade assets are only the source for these conversions.
Logs: log_warning tagged HEROFX.
"""
import unreal

EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
FX = "/Game/ZERO/HeroKit/FX"
NIAGARA = FX + "/Niagara"

CONVERT = [
    "/Game/ParagonTwinblast/FX/Particles/SummerTime/FX/P_TwinBlast_Primary_Trail_Summer",
    "/Game/ParagonTwinblast/FX/Particles/SummerTime/FX/P_TwinBlast_Primary_HitCharacter_Summer",
    "/Game/ParagonTwinblast/FX/Particles/SummerTime/FX/P_TwinBlast_Primary_HitWorld_Summer",
    "/Game/ParagonTwinblast/FX/Particles/SummerTime/FX/P_TwinBlast_Nitro_Trail_Summer",
    "/Game/ParagonTwinblast/FX/Particles/SummerTime/FX/P_TwinBlast_Nitro_SteamRelease_Summertime",
    "/Game/ParagonTwinblast/FX/Particles/SummerTime/FX/P_TwinBlast_VortexGrenade_Explode_Summer",
    "/Game/ParagonTwinblast/FX/Particles/Abilities/VortexGrenade/FX/P_Twinblast_VortexGrenade_ExplodeWater",
    "/Game/ParagonTwinblast/FX/Particles/Abilities/VortexGrenade/FX/P_GrenadeBounceSplash",
    "/Game/ParagonSunWukong/FX/Particles/Wukong/Abilities/DoubleJump/FX/p_GroundSlam_Radius",
    # Tempest (wind): cloud puffs, swirl, trail, impact
    "/Game/ParagonSunWukong/FX/Particles/Wukong/Abilities/DoubleJump/FX/p_CloudBurst",
    "/Game/ParagonSunWukong/FX/Particles/Wukong/p_CloudLand",
    "/Game/ParagonSunWukong/FX/Particles/Wukong/p_CloudStep",
    "/Game/ParagonSunWukong/FX/Particles/Wukong/p_Spin",
    "/Game/ParagonSunWukong/FX/Particles/Wukong/Abilities/Poke/FX/p_SubtleSpeedTrail",
    "/Game/ParagonSunWukong/FX/Particles/Wukong/Abilities/Poke/FX/P_Wukong_Poke_Impact",
]


def log(msg):
    unreal.log_warning("HEROFX " + msg)


def ensure_dir(path):
    if not EAL.does_directory_exist(path):
        EAL.make_directory(path)


def make_ice_material():
    path = f"{FX}/M_HeroKit_Ice"
    if EAL.does_asset_exist(path):
        log("ice material exists")
        return
    ensure_dir(FX)
    mel = unreal.MaterialEditingLibrary
    mat = AT.create_asset("M_HeroKit_Ice", FX, unreal.Material, unreal.MaterialFactoryNew())
    color = mel.create_material_expression(mat, unreal.MaterialExpressionParticleColor, -700, 0)
    fresnel = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -700, 250)
    fresnel.set_editor_property("exponent", 3.0)
    strength = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -700, 420)
    strength.set_editor_property("r", 4.0)
    glow = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -450, 250)
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 150)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 500)
    rough.set_editor_property("r", 0.08)
    mel.connect_material_expressions(fresnel, "", glow, "A")
    mel.connect_material_expressions(strength, "", glow, "B")
    mel.connect_material_expressions(glow, "", tint, "A")
    mel.connect_material_expressions(color, "RGB", tint, "B")
    mel.connect_material_property(color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mat.set_editor_property("used_with_niagara_mesh_particles", True)
    mat.set_editor_property("used_with_niagara_sprites", True)
    mel.recompile_material(mat)
    EAL.save_loaded_asset(mat, False)
    log("created ice material")


def niagara_name(cascade_path):
    name = cascade_path.rsplit("/", 1)[1]
    return "NS_" + name[2:] if name.lower().startswith("p_") else "NS_" + name


def convert_all():
    try:
        import CascadeToNiagaraConverter as converter
    except Exception as exc:
        log(f"converter plugin not available: {exc}")
        return
    ensure_dir(NIAGARA)
    for cascade_path in CONVERT:
        target = f"{NIAGARA}/{niagara_name(cascade_path)}"
        if EAL.does_asset_exist(target):
            log(f"exists {target}")
            continue
        cascade = unreal.load_asset(cascade_path)
        if not cascade:
            log(f"missing {cascade_path}")
            continue
        results = unreal.new_object(unreal.ConvertCascadeToNiagaraResults)
        try:
            converter.convert_cascade_to_niagara(cascade, results)
        except Exception as exc:
            log(f"convert failed {cascade_path}: {exc}")
            continue
        converted = cascade_path + "_Converted"
        if not EAL.does_asset_exist(converted):
            log(f"no output for {cascade_path}")
            continue
        if EAL.rename_asset(converted, target):
            EAL.save_asset(target, False)
            log(f"converted {target}")
        else:
            log(f"rename failed {converted} -> {target}")


make_ice_material()
convert_all()
log("done")
