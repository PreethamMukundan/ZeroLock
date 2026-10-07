"""
Deadlock hero importer (step 1 of 2): game files -> FBX + textures + manifest.

    deadlock_import.bat lash apollo        export heroes
    deadlock_import.bat --list             list every hero (display name -> codename)
    deadlock_import.bat lash --anims all   override the animation mode (filtered | all | none)

Pipeline per hero:
    Source2Viewer-CLI  -> <Hero>.glb (mesh, skeleton, filtered anims, base color/ORM textures)
    Source2Viewer-CLI  -> normal / self-illum textures (not wired into the glTF by S2V)
    Blender (this file runs inside it, headless) -> SK_<Hero>.fbx + one A_<Hero>_<anim>.fbx per clip
    manifest.json      -> consumed by ue_import.py inside the Unreal editor (step 2)
"""
import json
import os
import re
import struct
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(HERE, "config.json"), encoding="utf-8") as f:
    CFG = json.load(f)

VPK = os.path.join(CFG["deadlock_dir"], "game", "citadel", "pak01_dir.vpk")
OUT = CFG["output_dir"]
CACHE = os.path.join(OUT, "_cache")
SOURCE_FPS = 30


def log(msg):
    print(f"[deadlock] {msg}", flush=True)


def s2v(args):
    cmd = [CFG["source2viewer_cli"], "-i", VPK] + args
    r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        raise RuntimeError(f"Source2Viewer failed ({r.returncode}): {' '.join(args)}\n{r.stdout}\n{r.stderr}")
    return r.stdout


def is_stale(path):
    return not os.path.exists(path) or os.path.getmtime(path) < os.path.getmtime(VPK)


# --------------------------------------------------------------------------- hero registry

def norm(s):
    return re.sub(r"[^a-z0-9]", "", s.lower())


def load_heroes():
    """{codename: {key, codename, display, model}} from scripts/heroes.vdata + english localization."""
    os.makedirs(CACHE, exist_ok=True)
    cache = os.path.join(CACHE, "heroes.json")
    if not is_stale(cache):
        with open(cache, encoding="utf-8") as f:
            heroes = json.load(f)
        if all("abilities" in h for h in heroes.values()):
            return heroes

    log("Reading hero list from game files...")
    vdata = os.path.join(CACHE, "heroes.vdata")
    s2v(["-f", "scripts/heroes.vdata_c", "-d", "-o", vdata])
    with open(vdata, encoding="utf-8") as f:
        text = f.read()

    loc = os.path.join(CFG["deadlock_dir"], "game", "citadel", "resource", "localization",
                       "citadel_gc_hero_names", "citadel_gc_hero_names_english.txt")
    with open(loc, encoding="utf-8-sig") as f:
        names = dict(re.findall(r'"(hero_\w+):n"\s+"([^"]+)"', f.read()))

    # Ability display names ("ability_lash_flog" -> "Flog") live in the hero localization files.
    loc_dir = os.path.dirname(os.path.dirname(loc))
    ability_names = {}
    for sub in ("citadel_heroes", "citadel_gc", "citadel_main"):
        path = os.path.join(loc_dir, sub, f"{sub}_english.txt")
        if os.path.exists(path):
            with open(path, encoding="utf-8-sig", errors="replace") as f:
                for k, v in re.findall(r'^\s*"([A-Za-z0-9_]+)"\s+"([^"]*)"', f.read(), re.M):
                    ability_names.setdefault(k, v)

    keys = [(m.start(), m.group(1)) for m in re.finditer(r"(?m)^\t(hero_\w+)\s*=", text)]
    heroes = {}
    for i, (pos, key) in enumerate(keys):
        block = text[pos:keys[i + 1][0] if i + 1 < len(keys) else len(text)]
        m = re.search(r'm_strModelName\s*=\s*resource_name:"([^"]+)"', block)
        if not m or key in ("hero_base",):
            continue
        codename = key[len("hero_"):]
        abilities = []
        bound = re.search(r"m_mapBoundAbilities\s*=\s*\{(.*?)\}", block, re.S)
        for slot, ability in re.findall(r'(ESlot_(?:Signature_\d|Weapon_Melee))\s*=\s*"([^"]+)"', bound.group(1) if bound else ""):
            abilities.append({"slot": slot, "key": ability, "name": ability_names.get(ability, ability)})
        heroes[codename] = {"key": key, "codename": codename, "display": names.get(key, codename.title()),
                            "model": m.group(1), "abilities": abilities}

    with open(cache, "w", encoding="utf-8") as f:
        json.dump(heroes, f, indent=1)
    return heroes


def resolve_hero(heroes, query):
    q = norm(query)
    for h in heroes.values():
        if q in (norm(h["codename"]), norm(h["display"]), norm(h["key"])):
            return h
    close = [h["display"] for h in heroes.values() if q in norm(h["display"]) or q in norm(h["codename"])]
    hint = f" Did you mean: {', '.join(close)}?" if close else " Run with --list to see all heroes."
    raise SystemExit(f"Unknown hero '{query}'.{hint}")


def asset_name(hero):
    return re.sub(r"[^A-Za-z0-9]", "", hero["display"].title())


# --------------------------------------------------------------------------- glTF helpers

def read_glb_json(path):
    with open(path, "rb") as f:
        f.seek(12)
        length, _ = struct.unpack("<II", f.read(8))
        return json.loads(f.read(length))


def rename_glb_animations(path, short_by_full):
    """Rewrite animation names in the glb's JSON chunk. Blender truncates names to 63 chars, which
    mangles S2V's full clip paths, so give it the short names up front."""
    with open(path, "rb") as f:
        data = f.read()
    json_len = struct.unpack_from("<I", data, 12)[0]
    gltf = json.loads(data[20:20 + json_len])
    for anim in gltf.get("animations", []):
        anim["name"] = short_by_full.get(anim["name"], anim["name"].rsplit("/", 1)[-1])
    new_json = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    new_json += b" " * (-len(new_json) % 4)
    rest = data[20 + json_len:]
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(new_json) + len(rest)))
        f.write(struct.pack("<II", len(new_json), 0x4E4F534A))
        f.write(new_json)
        f.write(rest)
    return gltf


def select_animations(all_names):
    """Map short clip name -> exact glTF animation name, applying the config filter.

    S2V exports both the legacy sequences ('primary_run_n') and the current AnimGraph2 clips
    ('models/heroes_staging/lash_v2/clips/weapon_run_n'). When the short names collide the
    AG2 /clips/ version wins.
    """
    acfg = CFG["animations"]
    mode = acfg["mode"]
    if mode == "none":
        return {}
    inc = [re.compile(p, re.I) for p in acfg["include"]]
    exc = [re.compile(p, re.I) for p in acfg["exclude"]]

    def rank(name):
        return 2 if "/clips/" in name else 1 if "/" in name else 0

    chosen = {}
    for name in all_names:
        short = name.rsplit("/", 1)[-1]
        if any(e.search(short) for e in exc):
            continue
        if mode != "all" and not any(i.search(short) for i in inc):
            continue
        if short not in chosen or rank(name) > rank(chosen[short]):
            chosen[short] = name
    return dict(sorted(chosen.items()))


def list_animations(hero, name):
    """All animation names on the model (cached; needs one full S2V export the first time)."""
    cache = os.path.join(CACHE, name, "animations.json")
    if not is_stale(cache):
        with open(cache, encoding="utf-8") as f:
            return json.load(f)
    log("Listing animations (first run for this hero, ~15s)...")
    tmp = os.path.join(CACHE, name, "full")
    os.makedirs(tmp, exist_ok=True)
    glb = os.path.join(tmp, f"{name}.glb")
    s2v(["-f", hero["model"] + "_c", "-d", "-o", glb, "--gltf_export_format", "glb", "--gltf_export_animations"])
    names = [a["name"] for a in read_glb_json(glb).get("animations", [])]
    for fn in os.listdir(tmp):
        os.remove(os.path.join(tmp, fn))
    with open(cache, "w", encoding="utf-8") as f:
        json.dump(names, f, indent=1)
    return names


def export_glb(hero, name, anim_names, glb_dir):
    os.makedirs(glb_dir, exist_ok=True)
    glb = os.path.join(glb_dir, f"{name}.glb")
    args = ["-f", hero["model"] + "_c", "-d", "-o", glb, "--gltf_export_format", "glb",
            "--gltf_export_materials", "--gltf_textures_adapt"]
    if anim_names:
        args += ["--gltf_export_animations", "--gltf_compose_additive",
                 "--gltf_animation_list", ",".join(anim_names)]
    log(f"Exporting model{f' + {len(anim_names)} animations' if anim_names else ''} with Source2Viewer...")
    s2v(args)
    return glb


def extract_texture(vtex_path, out_png):
    if not os.path.exists(out_png):
        s2v(["-f", vtex_path + "_c", "-d", "-o", out_png])
    return out_png


def build_material_info(gltf, glb_dir, tex_dir):
    """Per-material texture + shading info for Unreal, from glTF materials and their vmat extras."""
    images = gltf.get("images", [])
    textures = gltf.get("textures", [])

    def image_of(ref):
        if not ref:
            return None
        uri = images[textures[ref["index"]]["source"]]["uri"]
        return os.path.join(glb_dir, uri).replace("\\", "/")

    def is_default(vtex):
        return not vtex or vtex.startswith("materials/default/")

    out = []
    for mat in gltf.get("materials", []):
        vmat = mat.get("extras", {}).get("vmat", {})
        ints, floats = vmat.get("IntParams", {}), vmat.get("FloatParams", {})
        vecs, texs = vmat.get("VectorParams", {}), vmat.get("TextureParams", {})
        pbr = mat.get("pbrMetallicRoughness", {})

        info = {
            "name": mat["name"],
            "vmat": vmat.get("Name"),
            "base_color": image_of(pbr.get("baseColorTexture")),
            "orm": image_of(pbr.get("metallicRoughnessTexture")),
            "normal": None,
            "emissive": None,
            "tint": vecs.get("g_vColorTint1", [1, 1, 1, 0])[:3],
            "blend": "additive" if ints.get("F_ADDITIVE_BLEND") else
                     "translucent" if ints.get("F_TRANSLUCENT") or mat.get("alphaMode") == "BLEND" else
                     "masked" if ints.get("F_ALPHA_TEST") or mat.get("alphaMode") == "MASK" else "opaque",
            "two_sided": bool(mat.get("doubleSided") or ints.get("F_RENDER_BACKFACES")),
            "emissive_tint": vecs.get("g_vSelfIllumTint1", [1, 1, 1, 0])[:3],
            "emissive_scale": floats.get("g_flSelfIllumScale1", 1.0),
            "outline_tint": vecs.get("g_vSolidOutlineTint", [0, 0, 0, 0])[:3],
        }
        nrm = texs.get("g_tNormalRoughness")
        if not is_default(nrm):
            info["normal"] = extract_texture(nrm, os.path.join(tex_dir, os.path.basename(nrm) + ".png")).replace("\\", "/")
        illum = texs.get("g_tSelfIllumMask")
        if ints.get("F_SELF_ILLUM") and not is_default(illum):
            info["emissive"] = extract_texture(illum, os.path.join(tex_dir, os.path.basename(illum) + ".png")).replace("\\", "/")
        out.append(info)
    return out


# --------------------------------------------------------------------------- Blender

def blender_convert(glb, name, wanted_anims, fbx_dir):
    """Import the glb, clean it up for Unreal, export SK_<name>.fbx and A_<name>_<clip>.fbx files."""
    import bpy

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.fps = SOURCE_FPS
    scene.render.fps_base = 1.0

    log("Importing into Blender...")
    t = time.time()
    bpy.ops.import_scene.gltf(filepath=glb)
    log(f"  imported in {time.time() - t:.0f}s")

    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    meshes = [o for o in bpy.data.objects if o.type == "MESH" and o.parent == arm]
    for o in list(bpy.data.objects):
        if o is not arm and o not in meshes:
            bpy.data.objects.remove(o, do_unlink=True)

    # Deadlock skeletons have two root bones (root_motion, pelvis). Unreal needs one, and Blender's
    # FBX exporter turns the armature object into a bone, so naming it "root" gives a single root.
    arm.name = "root"
    arm.data.name = f"{name}_Skeleton"
    for o in meshes:
        o.name = f"{name}_{o.name.rsplit('.', 1)[-1]}"

    actions = {}
    for act in sorted(bpy.data.actions, key=lambda a: a.name):
        if act.name not in wanted_anims:
            log(f"  skipping unexpected action '{act.name}'")
            bpy.data.actions.remove(act)
            continue
        act.use_fake_user = True
        actions[act.name] = act
    missing = set(wanted_anims) - set(actions)
    if missing:
        log(f"  WARNING: {len(missing)} animations did not come through: {', '.join(sorted(missing))}")

    ad = arm.animation_data_create()
    for tr in list(ad.nla_tracks):
        ad.nla_tracks.remove(tr)
    ad.action = None

    # Rest pose for the skeletal mesh export
    for pb in arm.pose.bones:
        pb.matrix_basis.identity()

    os.makedirs(fbx_dir, exist_ok=True)
    common = dict(
        use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
        add_leaf_bones=False, primary_bone_axis="Y", secondary_bone_axis="X",
        armature_nodetype="NULL", mesh_smooth_type="FACE", use_tspace=False,
        path_mode="STRIP", embed_textures=False,
    )

    def select(objs):
        bpy.ops.object.select_all(action="DESELECT")
        for o in objs:
            o.select_set(True)
        bpy.context.view_layer.objects.active = arm

    select([arm] + meshes)
    sk_fbx = os.path.join(fbx_dir, f"SK_{name}.fbx")
    bpy.ops.export_scene.fbx(filepath=sk_fbx, object_types={"ARMATURE", "MESH"}, bake_anim=False, **common)
    log(f"  wrote {os.path.basename(sk_fbx)} ({len(meshes)} meshes, {len(arm.data.bones)} bones)")

    anim_files = []
    select([arm])
    t = time.time()
    for i, (short, act) in enumerate(actions.items(), 1):
        ad.action = act
        if hasattr(ad, "action_slot") and getattr(act, "slots", None) and ad.action_slot is None:
            ad.action_slot = act.slots[0]
        start, end = (int(round(v)) for v in act.frame_range)
        scene.frame_start, scene.frame_end = start, max(end, start + 1)
        path = os.path.join(fbx_dir, f"A_{name}_{short}.fbx")
        bpy.ops.export_scene.fbx(
            filepath=path, object_types={"ARMATURE"}, bake_anim=True,
            bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
            bake_anim_force_startend_keying=True, bake_anim_step=1.0, bake_anim_simplify_factor=0.0,
            **common)
        anim_files.append({"name": short, "fbx": path.replace("\\", "/"), "frames": end - start + 1})
        if i % 25 == 0 or i == len(actions):
            log(f"  animations {i}/{len(actions)} ({time.time() - t:.0f}s)")
    ad.action = None

    blend = os.path.join(os.path.dirname(fbx_dir), f"{name}.blend")
    bpy.ops.wm.save_as_mainfile(filepath=blend, compress=True)
    return sk_fbx.replace("\\", "/"), anim_files, blend


# --------------------------------------------------------------------------- main

def process_hero(heroes, query, anim_mode):
    hero = resolve_hero(heroes, query)
    name = asset_name(hero)
    log(f"=== {hero['display']} ({hero['codename']}) -> {name}  [{hero['model']}]")
    hero_dir = os.path.join(OUT, name)
    glb_dir, tex_dir, fbx_dir = (os.path.join(hero_dir, d) for d in ("glb", "textures", "fbx"))
    for d in (glb_dir, fbx_dir):
        if os.path.isdir(d):
            for fn in os.listdir(d):
                os.remove(os.path.join(d, fn))
    os.makedirs(tex_dir, exist_ok=True)

    if anim_mode:
        CFG["animations"]["mode"] = anim_mode
    selected = {}
    if CFG["animations"]["mode"] != "none":
        selected = select_animations(list_animations(hero, name))
        log(f"Selected {len(selected)} animations ({CFG['animations']['mode']})")

    glb = export_glb(hero, name, list(selected.values()), glb_dir)
    gltf = rename_glb_animations(glb, {full: short for short, full in selected.items()})
    materials = build_material_info(gltf, glb_dir, tex_dir)

    sk_fbx, anim_files, blend = blender_convert(glb, name, set(selected), fbx_dir)

    manifest = {
        "hero": name,
        "display": hero["display"],
        "codename": hero["codename"],
        "model": hero["model"],
        "ue_dest": f"{CFG['ue_dest_root'].rstrip('/')}/{name}",
        "normal_flip_green": CFG.get("normal_flip_green", True),
        "abilities": hero.get("abilities", []),
        "skeletal_mesh_fbx": sk_fbx,
        "animations": anim_files,
        "materials": materials,
        "blend": blend.replace("\\", "/"),
        "exported_at": time.strftime("%Y-%m-%d %H:%M:%S"),
    }
    path = os.path.join(hero_dir, "manifest.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
    log(f"Done: {len(anim_files)} animations, {len(materials)} materials -> {hero_dir}")
    return name


def main(argv):
    if not os.path.exists(VPK):
        raise SystemExit(f"Deadlock VPK not found: {VPK} (check deadlock_dir in config.json)")
    heroes = load_heroes()

    if not argv or "--list" in argv:
        for h in sorted(heroes.values(), key=lambda h: h["display"].lower()):
            print(f"  {h['display']:<22} {h['codename']:<18} {h['model']}")
        return

    anim_mode = None
    if "--anims" in argv:
        i = argv.index("--anims")
        anim_mode = argv[i + 1]
        del argv[i:i + 2]

    done = [process_hero(heroes, q, anim_mode) for q in argv]
    ue_script = os.path.join(HERE, "ue_import.py").replace("\\", "/")
    print("\n" + "=" * 70)
    print("Now run this in the Unreal editor console (` key), or Tools > Execute Python Script:")
    print(f'    py "{ue_script}" {" ".join(done)}')
    print("=" * 70)


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
