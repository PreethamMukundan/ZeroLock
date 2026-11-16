# Copyright Preetham Mukundan (C) 2026
"""Makes M_HeroKit_WindShell: a copy of Paragon's m_WindSplines_Master with its World Position Offset tied to zero.
The original's WPO is a spline-thickening function that squashes any mesh into a camera-facing ribbon, so on the
Alley-oop sphere it read as a flat rectangle. Then re-parents MI_HeroKit_WindBall onto the copy and swaps its edge
masks (Vert/Horiz Edge Texture, UV-based fades that cut a sphere into a band) for plain white.

Run headless (editor closed): UnrealEditor-Cmd.exe ZeroLock.uproject -run=pythonscript -script="<abs path>" -unattended -nosplash -nullrhi
Not over MCP with the editor open: MaterialInstanceTools.set_parent on the in-use MI crashed the editor (renderer assert).
Logs: LogPython warnings tagged WINDSHELL. Re-running is safe.
"""
import unreal

SRC = "/Game/ParagonSunWukong/FX/Materials/Wind/m_WindSplines_Master"
DST = "/Game/ZERO/HeroKit/FX/Materials/M_HeroKit_WindShell"
MI = "/Game/ZERO/HeroKit/FX/Materials/MI_HeroKit_WindBall"
WHITE = "/Engine/EngineResources/WhiteSquareTexture"

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("WINDSHELL " + msg)


if not EAL.does_asset_exist(DST):
    EAL.duplicate_asset(SRC, DST)
    log("duplicated " + SRC)
mat = unreal.load_asset(DST)

current = MEL.get_material_property_input_node(mat, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
if isinstance(current, unreal.MaterialExpressionConstant3Vector):
    log("WPO already tied to zero")
else:
    zero = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -800, 1200)
    zero.set_editor_property("constant", unreal.LinearColor(0, 0, 0, 0))
    if MEL.connect_material_property(zero, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET):
        log("WPO tied to zero")
    else:
        log("ERROR could not connect WPO")
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat, False)
    log("saved " + DST)

mi = unreal.load_asset(MI)
MEL.set_material_instance_parent(mi, mat)
white = unreal.load_asset(WHITE)
for name in ("Vert Edge Texture", "Horiz Edge Texture"):
    MEL.set_material_instance_texture_parameter_value(mi, name, white)
MEL.update_material_instance(mi)
EAL.save_loaded_asset(mi, False)
log("MI_HeroKit_WindBall re-parented, edge masks white; done")
