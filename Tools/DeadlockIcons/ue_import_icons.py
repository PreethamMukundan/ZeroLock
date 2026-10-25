"""
Deadlock icon importer (step 2 of 2): imports the PNGs deadlock_icons.bat downloaded to Saved/DeadlockIcons/ into
/Game/Data/Images (ability + item icons) and /Game/Data/ShopImages (shop art), replacing same-named textures in place
so everything that references them gets the new art.

    py "E:/Repo/ZeroLock/ZeroLock/Tools/DeadlockIcons/ue_import_icons.py"     (editor console)
deadlock_icons.bat runs it headlessly itself when the editor is closed.
"""
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
STAGE = os.path.normpath(os.path.join(HERE, "..", "..", "Saved", "DeadlockIcons"))

# Folder -> texture settings, matching the textures already in the project.
KINDS = {
    "Images": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
    "ShopImages": unreal.TextureCompressionSettings.TC_DEFAULT,
}


def log(msg):
    # log_warning: plain unreal.log is filtered out of commandlet output.
    unreal.log_warning(f"[icons] {msg}")


def import_folder(folder, compression):
    src = os.path.join(STAGE, folder)
    if not os.path.isdir(src):
        return 0
    pngs = sorted(f for f in os.listdir(src) if f.lower().endswith(".png"))
    if not pngs:
        return 0
    dest = f"/Game/Data/{folder}"

    tasks = []
    for png in pngs:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(src, png))
        task.set_editor_property("destination_path", dest)
        task.set_editor_property("destination_name", os.path.splitext(png)[0])
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    done = 0
    for png in pngs:
        path = f"{dest}/{os.path.splitext(png)[0]}"
        tex = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if not isinstance(tex, unreal.Texture2D):
            log(f"  failed: {path}")
            continue
        tex.set_editor_property("compression_settings", compression)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("never_stream", True)
        tex.set_editor_property("srgb", True)
        unreal.EditorAssetLibrary.save_loaded_asset(tex, False)
        done += 1
    log(f"{dest}: {done}/{len(pngs)} imported")
    return done


def main():
    if not os.path.isdir(STAGE):
        log(f"nothing staged in {STAGE}: run Tools/DeadlockIcons/deadlock_icons.bat first")
        return
    total = sum(import_folder(folder, compression) for folder, compression in KINDS.items())
    log(f"done: {total} icons")


main()
