# Copyright Preetham Mukundan (C) 2026
"""Makes M_HeroKit_EnergyCore (unlit additive sphere core: faint centre, bright fresnel rim) and gives Tempest's
Alley-oop ball its core (AZL_Kit_Projectile CoreMaterial/CoreScale/CoreColor, under the MI_HeroKit_WindBall shell).

Editor closed, after building the editor target (CoreMesh/CoreMaterial are C++):
  UnrealEditor-Cmd.exe ZeroLock.uproject -run=pythonscript -script="<abs path>/make_energy_core.py" -unattended -nosplash -nullrhi
Tempest actually uses the instance MI_HeroKit_EnergyCore_Wind (CoreGlow 0.06, Intensity 2, RimPower 3: the base defaults
read as a flat white disc); this script assigns the base material, so rerunning it means re-pointing CoreMaterial.
Logs use log_warning with the tag ENERGYCORE (commandlet output hides unreal.log). Re-running updates in place.
"""
import unreal

FOLDER = "/Game/ZERO/HeroKit/FX/Materials"
NAME = "M_HeroKit_EnergyCore"
PROJECTILE = "/Game/ZERO/Heroes/Tempest/Abilities/AlleyOop/BP_Tempest_AlleyOop_Projectile"

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("ENERGYCORE " + msg)


def make_material():
    path = f"{FOLDER}/{NAME}"
    if EAL.does_asset_exist(path):
        mat = unreal.load_asset(path)
        MEL.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", False)

    def expr(cls, x, y):
        return MEL.create_material_expression(mat, cls, x, y)

    def scalar(name, value, x, y):
        e = expr(unreal.MaterialExpressionScalarParameter, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", value)
        return e

    rim_power = scalar("RimPower", 2.5, -900, -100)
    fresnel = expr(unreal.MaterialExpressionFresnel, -650, -100)
    fresnel.set_editor_property("base_reflect_fraction", 0.0)
    MEL.connect_material_expressions(rim_power, "", fresnel, "ExponentIn")

    core_glow = scalar("CoreGlow", 0.25, -650, -300)
    lerp = expr(unreal.MaterialExpressionLinearInterpolate, -400, -200)
    lerp.set_editor_property("const_b", 1.0)
    MEL.connect_material_expressions(core_glow, "", lerp, "A")
    MEL.connect_material_expressions(fresnel, "", lerp, "Alpha")

    color = expr(unreal.MaterialExpressionVectorParameter, -400, 50)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    tint = expr(unreal.MaterialExpressionMultiply, -200, -100)
    MEL.connect_material_expressions(lerp, "", tint, "A")
    MEL.connect_material_expressions(color, "", tint, "B")

    intensity = scalar("Intensity", 3.0, -200, 100)
    out = expr(unreal.MaterialExpressionMultiply, 0, -50)
    MEL.connect_material_expressions(tint, "", out, "A")
    MEL.connect_material_expressions(intensity, "", out, "B")
    MEL.connect_material_property(out, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat, False)
    log(f"material {path} saved")
    return mat


def assign(mat):
    bp = unreal.load_asset(PROJECTILE)
    if not bp:
        log(f"ERROR no {PROJECTILE}")
        return
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    try:
        cdo.set_editor_property("core_material", mat)
        cdo.set_editor_property("core_scale", 0.75)
        cdo.set_editor_property("core_color", unreal.LinearColor(0.5, 1.6, 1.0, 1.0))
    except Exception as exc:
        log(f"ERROR projectile core (C++ built?): {exc}")
        return
    EAL.save_loaded_asset(bp, False)
    log(f"{PROJECTILE} core set")


assign(make_material())
log("done")
