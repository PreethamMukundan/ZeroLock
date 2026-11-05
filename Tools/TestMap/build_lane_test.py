# Copyright Preetham Mukundan (C) 2026
#
# Builds /Game/ZERO/Levels/Lvl_LaneTest: a circular, Deadlock-style three-lane test map.
#   - A round arena (radius R) with a round base attached on each side (Blue at -X, Red at +X).
#   - Three lanes run base to base and converge at the base gates. Deliberately not symmetric: Mid is a gentle
#     S-curve, North and South bulge by different amounts and lean toward opposite bases (LANE_SHAPES).
#   - A centre tower (one boolean-cut mesh) straddles the Mid lane: the lane runs through its ground gate, and the Mid
#     zipline climbs from the bases and threads a tunnel through the top of the spire.
#   - Between and beyond the lanes: procedurally placed box "mesas" (low 450 / tall 900 with ramps,
#     spires, crate clusters), all kept clear of the lanes. Fixed seed, so reruns give the same map.
#   - Ziplines follow each lane's curve and converge on the base hub (one station per lane).
#   - Underground: a pillared undercroft beneath mid, tunnels ramping up into the north and south
#     jungles, and drop holes cut out of the floor.
#   - Geometry is built with Geometry Script: every box/ramp/cylinder goes into a mesh bucket (material x collision x
#     60 m chunk) saved as a static mesh under MESH_DIR, so the map is a few dozen mesh actors instead of ~1000 cubes.
#     The floor is one slab: a disc with the openings boolean-subtracted, plus the base discs. Lanes are extruded strips.
#   - Night, tuned for frame rate: dim cool moon (the only shadow caster), dark height fog (no volumetric fog, no
#     clouds, no local fog volumes), sky light captured once, Lumen GI off and screen-space reflections for this map
#     (post process), exposure locked so night stays dark; warm street lamps (no shadows, distance-culled) carry it.
#
# Sublevels (all Always Loaded, under SUBLEVEL_DIR): _Geometry (merged meshes), _Lighting (sky, fog, lights),
# _Gameplay (spawns, ziplines, labels), _DoormanHotel (Doorman's hotel, where Hotel Guest looks for it). The
# persistent level only holds the world settings.
#
# Run in the editor console (Cmd mode):  py "E:/Repo/ZeroLock/ZeroLock/Tools/TestMap/build_lane_test.py"
# Re-running rebuilds only the actors tagged GEN_TAG; anything you place by hand stays.
# All sizes in cm.

import math
import random
import unreal

LEVEL_PATH = "/Game/ZERO/Levels/Lvl_LaneTest"
GAME_MODE = "/Game/ZERO/BP/GameMode/BP_TDMGameMOde"
PLAYER_START = "/Game/ZERO/BP/GameMode/BP_Zero_PlayerStart"
ZIPLINE = "/Game/ZERO/Core/BP_BaseZiplineActor"
MESH_DIR = "/Game/ZERO/Levels/LaneTest_Meshes"
SUBLEVEL_DIR = "/Game/ZERO/Levels/LaneTest"
SUBLEVELS = ("Geometry", "Lighting", "Gameplay", "DoormanHotel")
HOTEL_LOCATION = (0.0, 0.0, 30000.0)  # UZL_Doorman_HotelGuest::HotelSpawnLocation
CHUNK = 6000              # mesh buckets are split per CHUNK x CHUNK cm cell (culling, distance-field quality)
PROTO_MATS = "/Game/LevelPrototyping/Materials"
MAT_DIR = "/Game/ZERO/MATS/LaneTest"
GEN_TAG = "ZL_LaneTestGen"
SEED = 7

# --- Layout (Deadlock-like proportions; our walk speed 800 ~ Deadlock 7.2 m/s, heroes ~1.8 m) ---
R = 9000                  # arena radius (180 m across)
WALL_H = 1600
BASE_OFFSET = 10500       # base centre distance from the arena centre along X
BASE_R = 2500             # base radius
MINIMAP_MARGIN = 500      # map space kept around the bases and the arena wall
MINIMAP_CENTER_Z = 1500   # the minimap box spans Z -2000..5000: above the spire, well below the Doorman hotel
MINIMAP_HALF_Z = 3500
LANE_HALF = 700           # lanes are 14 m wide
GATE_X = 9000             # lanes start/end here (inside the base disc)
# Lane centre lines: y(t) = gate + bulge * cos(pi t / 2) + lean * sin(pi t), t in [-1, 1] (x = t * GATE_X).
LANE_SHAPES = {"North": (1500, 5000, 700), "Mid": (0, 0, 650), "South": (-1500, -4200, 500)}
LANES = {"North": 1, "Mid": 0, "South": -1}
LOW_H, TALL_H, STACK_H, SPIRE_H = 450, 900, 1350, 2000
SLAB = 30
MESA_SPACING = 900        # candidate spot grid for mesas
MESA_GAP = 350            # minimum walkway between two mesas
BRIDGE_RANGE = (1200, 3400)  # 9 m bridges link tall tops this far apart (also across lanes)
MAX_BRIDGES = 14
MESA_CLEAR = 250          # extra gap between a mesa and a lane edge
CENTER_CLEAR = 2700       # the centre tower and its ramps
TOWER_HALF = 1300         # centre tower base half size
TOWER_DECK = 1000         # deck height on top of the base
TOWER_GATE = (1600, 900)  # gate along the Mid lane (width, height): the lane and the Mid zipline pass through
TOWER_TOP = 3200          # spire top
TOWER_ZIP_Z = 2200        # the Mid zipline climbs to this and threads a tunnel through the spire
TUNNEL = (500, 500)       # spire tunnel (width, height); its floor is 320 below the cable (room for the hanging rider)
MANTLE_HEIGHTS = [60, 110, 160, 210, 300]  # importer mantle clips: 60/110/160/210 cm
ZIP_Z, ZIP_SAG = 750, 80  # North/South sag to 670 at mid
ZIP_END_Z = 800           # cable height above the base stations
STATION_R = 1100          # station distance from the base hub
STATION_H = 400
LAMP_SPACING = 2200
UNDER_Z = -600            # underground floor height
UNDERCROFT = 2500         # undercroft spans |x|, |y| < this
TUNNEL_HALF = 500
TUNNEL_END = 4500         # tunnels run from the undercroft to |y| = this, ramping up from |y| 2700
# Floor openings (x0, x1, y0, y1): tunnel mouths and drop holes, boolean-subtracted from the floor.
OPENINGS = [(-500, 500, 2500, TUNNEL_END), (-500, 500, -TUNNEL_END, -2500),
            (1500, 2500, 1500, 2500), (-2500, -1500, 1500, 2500), (1500, 2500, -2500, -1500), (-2500, -1500, -2500, -1500)]

# --- Atmosphere ---
MOON_PITCH, MOON_YAW, MOON_LUX, MOON_COLOR = -38.0, 35.0, 1.0, (0.55, 0.68, 1.0)
SKY_LIGHT_INTENSITY = 2.0  # night ambient
FOG_DENSITY, FOG_FALLOFF = 0.025, 0.25
FOG_COLOR = (0.03, 0.045, 0.09)     # inscattering: blue haze, so silhouettes read against it
EXPOSURE_EV100 = -1.5      # locked (min = max); lower = brighter night, higher = darker (free to change)
LAMP_CANDELA = 160
LIGHT_CULL_DISTANCE = 6000 # point lights stop drawing past this (fade over the last 1000)
LANE_COLORS = {"North": (0.15, 1.0, 0.3), "Mid": (1.0, 0.7, 0.08), "South": (0.55, 0.2, 1.0)}
TEAM_COLORS = {"Blue": (0.1, 0.4, 1.0), "Red": (1.0, 0.12, 0.05)}
LAMP_COLOR = (1.0, 0.75, 0.45)

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

MATS = {}
LEVELS = {}   # sublevel name -> level package short name (what set_current_level_by_name takes)
BUCKETS = {}  # (material, collide, chunk) -> list of callables that append geometry to a DynamicMesh
GSP = unreal.GeometryScript_Primitives
PRIM = unreal.GeometryScriptPrimitiveOptions()


def log(msg):
    unreal.log_warning("[LaneTest] " + msg)


def setp(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as e:
        log("could not set %s.%s: %s" % (obj.get_name(), name, e))


# ---------------------------------------------------------------- materials

def solid_mi(name, rgb):
    path = "%s/%s" % (PROTO_MATS, name)
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    mi = tools.create_asset(name, PROTO_MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, eal.load_asset(PROTO_MATS + "/M_Solid"))
    mel.set_material_instance_vector_parameter_value(mi, "Base Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    eal.save_loaded_asset(mi)
    return mi


def emissive_master():
    path = MAT_DIR + "/M_ZL_Emissive"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    mat = tools.create_asset("M_ZL_Emissive", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    power = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 250)
    power.set_editor_property("parameter_name", "Intensity")
    power.set_editor_property("default_value", 5.0)
    mult = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 100)
    mel.connect_material_expressions(color, "", mult, "A")
    mel.connect_material_expressions(power, "", mult, "B")
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(mult, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def emissive_mi(name, rgb, intensity):
    path = "%s/%s" % (MAT_DIR, name)
    if eal.does_asset_exist(path):
        mi = eal.load_asset(path)
    else:
        mi = tools.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, emissive_master())
    # Always reapply, so tuning the constants above updates the instances.
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Intensity", intensity)
    eal.save_loaded_asset(mi)
    return mi


def load_materials():
    MATS.update({
        "floor": eal.load_asset(PROTO_MATS + "/MI_PrototypeGrid_Gray"),
        "block": eal.load_asset(PROTO_MATS + "/MI_PrototypeGrid_TopDark"),
        "detail": eal.load_asset(PROTO_MATS + "/MI_PrototypeGrid_Gray_02"),
        "blue": eal.load_asset(PROTO_MATS + "/MI_Solid_Blue"),
        "red": solid_mi("MI_Solid_Red", (0.6, 0.02, 0.01)),
        "prop": solid_mi("MI_Solid_Crate", (0.35, 0.22, 0.12)),
        "metal": solid_mi("MI_Solid_Metal", (0.05, 0.05, 0.06)),
        "asphalt": solid_mi("MI_Solid_Asphalt", (0.07, 0.07, 0.08)),
        "lamp": emissive_mi("MI_ZL_Lamp", LAMP_COLOR, 25.0),
    })
    for lane, rgb in LANE_COLORS.items():
        MATS["lane_" + lane] = emissive_mi("MI_ZL_Lane_" + lane, rgb, 6.0)
    for team, rgb in TEAM_COLORS.items():
        MATS["team_" + team] = emissive_mi("MI_ZL_Team_" + team, rgb, 12.0)


# ---------------------------------------------------------------- level / spawning helpers

def open_level():
    if eal.does_asset_exist(LEVEL_PATH):
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        # Rebuild in place when it's already open, so unsaved hand edits aren't discarded or prompted for.
        if world.get_path_name().split(".")[0] != LEVEL_PATH:
            les.load_level(LEVEL_PATH)
        removed = 0
        for a in eas.get_all_level_actors():
            if GEN_TAG in [str(t) for t in a.tags]:
                eas.destroy_actor(a)
                removed += 1
        log("loaded existing level, removed %d generated actors" % removed)
    else:
        if not les.new_level(LEVEL_PATH):
            raise RuntimeError("new_level failed for " + LEVEL_PATH)
        log("created " + LEVEL_PATH)


def setup_sublevels():
    """Create (or add) the Always Loaded sublevels."""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    for name in SUBLEVELS:
        path = "%s/Lvl_LaneTest_%s" % (SUBLEVEL_DIR, name)
        streaming = unreal.GameplayStatics.get_streaming_level(world, path)
        if not streaming:
            if eal.does_asset_exist(path):
                streaming = unreal.EditorLevelUtils.add_level_to_world(world, path, unreal.LevelStreamingAlwaysLoaded)
            else:
                streaming = unreal.EditorLevelUtils.create_new_streaming_level(unreal.LevelStreamingAlwaysLoaded, path, False)
            log("sublevel added: " + path)
        LEVELS[name] = path.rsplit("/", 1)[-1]
    LEVELS["Persistent"] = LEVEL_PATH.rsplit("/", 1)[-1]
    use_level("Persistent")


CURRENT_LEVEL = [None]


def use_level(name):
    """Spawn the next actors into this sublevel."""
    if CURRENT_LEVEL[0] != name:
        if not les.set_current_level_by_name(LEVELS[name]):
            raise RuntimeError("can't make %s current" % LEVELS[name])
        CURRENT_LEVEL[0] = name


COUNT = [0]


def tag(actor, label, folder):
    actor.set_editor_property("tags", list(actor.tags) + [unreal.Name(GEN_TAG)])
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    COUNT[0] += 1
    return actor


def xform(x, y, z, yaw=0.0, pitch=0.0):
    return unreal.Transform(location=unreal.Vector(x, y, z), rotation=unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw),
                            scale=unreal.Vector(1, 1, 1))


def bucket(mat, collide, x, y):
    chunk = "%d_%d" % (math.floor(x / CHUNK), math.floor(y / CHUNK)) if x is not None else "all"
    return BUCKETS.setdefault((mat, collide, chunk), [])


def obox(label, folder, cx, cy, length, width, z0, z1, yaw, mat, collide=True):
    """Box centred at (cx, cy), `length` along `yaw`, `width` across it. (label/folder only document the call.)"""
    t = xform(cx, cy, (z0 + z1) * 0.5, yaw)
    bucket(mat, collide, cx, cy).append(lambda m: GSP.append_box(
        m, PRIM, t, length, width, z1 - z0, 0, 0, 0, unreal.GeometryScriptPrimitiveOriginMode.CENTER))


def cylinder(label, folder, x, y, z0, z1, diameter, mat, collide=True):
    t = xform(x, y, z0)
    steps = max(16, min(96, int(diameter / 60)))
    bucket(mat, collide, x, y).append(lambda m: GSP.append_cylinder(
        m, PRIM, t, diameter * 0.5, z1 - z0, steps, 0, True, unreal.GeometryScriptPrimitiveOriginMode.BASE))


def ramp(label, folder, a, b, width, mat):
    """Slab whose top surface runs from a (x,y,z) to b; width across the run."""
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    run = math.hypot(dx, dy)
    rot = unreal.Rotator(roll=0.0, pitch=math.degrees(math.atan2(dz, run)), yaw=math.degrees(math.atan2(dy, dx)))
    mid = unreal.Vector((a[0] + b[0]) * 0.5, (a[1] + b[1]) * 0.5, (a[2] + b[2]) * 0.5)
    loc = mid - rot.get_up_vector() * (SLAB * 0.5)
    t = unreal.Transform(location=loc, rotation=rot, scale=unreal.Vector(1, 1, 1))
    length = math.hypot(run, dz)
    bucket(mat, True, loc.x, loc.y).append(lambda m: GSP.append_box(
        m, PRIM, t, length, width, SLAB, 0, 0, 0, unreal.GeometryScriptPrimitiveOriginMode.CENTER))


def polygon_slab(mat, collide, pts, z0, z1):
    """Extrude a 2D outline (list of (x, y)) from z0 to z1. One bucket per material for the whole map."""
    area = sum(pts[i][0] * pts[i - 1][1] - pts[i - 1][0] * pts[i][1] for i in range(len(pts)))
    if area > 0:
        pts = list(reversed(pts))
    verts = [unreal.Vector2D(x, y) for x, y in pts]
    bucket(mat, collide, None, None).append(lambda m: GSP.append_simple_extrude_polygon(
        m, PRIM, xform(0, 0, z0), verts, z1 - z0, 0, True, unreal.GeometryScriptPrimitiveOriginMode.BASE))


def save_mesh(mesh, path, collide):
    if eal.does_asset_exist(path):
        sm = eal.load_asset(path)
        opts = unreal.GeometryScriptCopyMeshToAssetOptions()
        opts.set_editor_property("enable_recompute_normals", False)
        opts.set_editor_property("enable_recompute_tangents", True)
        unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh, sm, opts, unreal.GeometryScriptMeshWriteLOD(), True)
    else:
        opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
        opts.set_editor_property("enable_recompute_normals", False)
        opts.set_editor_property("enable_recompute_tangents", True)
        opts.set_editor_property("enable_collision", collide)
        opts.set_editor_property("collision_mode", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        sm, _ = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, path, opts)
    eal.save_loaded_asset(sm)
    return sm


def flush_meshes():
    """Build one static mesh per bucket and place it."""
    tris = 0
    made = set()
    for (mat, collide, chunk), ops in sorted(BUCKETS.items()):
        mesh = unreal.DynamicMesh()
        for op in ops:
            op(mesh)
        tris += mesh.get_triangle_count()
        name = "SM_LaneTest_%s_%s_%s" % (mat, "C" if collide else "N", chunk.replace("-", "m"))
        sm = save_mesh(mesh, "%s/%s" % (MESH_DIR, name), collide)
        made.add(name)
        use_level("Geometry")
        a = eas.spawn_actor_from_object(sm, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        comp = a.static_mesh_component
        comp.set_material(0, MATS[mat])
        if not collide:
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            setp(comp, "cast_shadow", False)
        tag(a, name, "Meshes/" + mat)
    # Meshes from earlier runs whose bucket no longer exists (nothing references them once their actors are gone).
    stale = [a for a in eal.list_assets(MESH_DIR, recursive=False) if a.split(".")[0].rsplit("/", 1)[-1] not in made]
    for a in stale:
        eal.delete_asset(a.split(".")[0])
    log("meshes: %d buckets, %d triangles, %d stale deleted" % (len(BUCKETS), tris, len(stale)))


def text(label, folder, loc, yaw, msg, size=60.0):
    use_level("Gameplay")
    t = eas.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(*loc), unreal.Rotator(0, 0, yaw))
    comp = t.text_render
    setp(comp, "text", msg)
    setp(comp, "world_size", size)
    setp(comp, "horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
    return tag(t, label, folder)


def point_light(label, folder, loc, rgb, candela, radius):
    use_level("Lighting")
    l = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*loc), unreal.Rotator(0, 0, 0))
    c = l.point_light_component
    setp(c, "intensity_units", unreal.LightUnits.CANDELAS)
    setp(c, "intensity", candela)
    setp(c, "attenuation_radius", radius)
    setp(c, "light_color", unreal.Color(r=int(rgb[0] * 255), g=int(rgb[1] * 255), b=int(rgb[2] * 255), a=255))
    setp(c, "cast_shadows", False)
    setp(c, "volumetric_scattering_intensity", 0.0)
    setp(c, "max_draw_distance", LIGHT_CULL_DISTANCE)
    setp(c, "max_distance_fade_range", 1000.0)
    return tag(l, label, folder)


def zipline(label, points):
    use_level("Gameplay")
    z = eas.spawn_actor_from_class(eal.load_blueprint_class(ZIPLINE), unreal.Vector(*points[0]), unreal.Rotator(0, 0, 0))
    spline = z.get_component_by_class(unreal.SplineComponent)
    spline.clear_spline_points(False)
    for p in points:
        spline.add_spline_point(unreal.Vector(*p), unreal.SplineCoordinateSpace.WORLD, False)
    spline.update_spline()
    setp(spline, "spline_has_been_edited", True)  # keep the points when the construction script reruns
    tag(z, label, "Ziplines")
    log("%s length %.0f cm" % (label, spline.get_spline_length()))


def player_start(label, loc, yaw, team):
    use_level("Gameplay")
    ps = eas.spawn_actor_from_class(eal.load_blueprint_class(PLAYER_START), unreal.Vector(*loc), unreal.Rotator(0, 0, yaw))
    setp(ps, "team_id", team)
    return tag(ps, label, "Spawns")


# ---------------------------------------------------------------- lane geometry

def lane_point(lane, t):
    """Lane centre line, t in [-1, 1] from the Blue gate to the Red gate."""
    gate, bulge, lean = LANE_SHAPES[lane]
    return (t * GATE_X, gate + bulge * math.cos(math.pi * t * 0.5) + lean * math.sin(math.pi * t))


def lane_yaw(lane, t):
    a, b = lane_point(lane, t - 0.002), lane_point(lane, t + 0.002)
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def lane_samples(lane, step_cm):
    n, length, prev = 400, 0.0, lane_point(lane, -1.0)
    for i in range(1, n + 1):
        p = lane_point(lane, -1.0 + 2.0 * i / n)
        length += math.hypot(p[0] - prev[0], p[1] - prev[1])
        prev = p
    count = max(2, int(length / step_cm))
    return [(-1.0 + 2.0 * i / count, lane_point(lane, -1.0 + 2.0 * i / count)) for i in range(count + 1)]


SEGS = []  # all lane centre-line segments, for distance checks


def seg_dist(p, a, b):
    ax, ay, bx, by = a[0], a[1], b[0], b[1]
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(p[0] - ax - t * dx, p[1] - ay - t * dy)


def lane_dist(p):
    return min(seg_dist(p, a, b) for a, b in SEGS)


def base_hubs():
    return {"Blue": (-BASE_OFFSET, 0.0), "Red": (BASE_OFFSET, 0.0)}


# ---------------------------------------------------------------- build

def build_floor(mesh):
    """Arena disc minus the openings, plus the base discs."""
    arena = unreal.DynamicMesh()
    GSP.append_cylinder(arena, PRIM, xform(0, 0, -50), R + 200, 50, 160, 0, True, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    bopts = unreal.GeometryScriptMeshBooleanOptions()
    for x0, x1, y0, y1 in OPENINGS:
        tool = unreal.DynamicMesh()
        GSP.append_box(tool, PRIM, xform((x0 + x1) * 0.5, (y0 + y1) * 0.5, -25), x1 - x0, y1 - y0, 400, 0, 0, 0,
                       unreal.GeometryScriptPrimitiveOriginMode.CENTER)
        unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(arena, unreal.Transform(), tool, unreal.Transform(),
                                                              unreal.GeometryScriptBooleanOperation.SUBTRACT, bopts)
    unreal.GeometryScript_Normals.compute_split_normals(arena, unreal.GeometryScriptSplitNormalsOptions(),
                                                        unreal.GeometryScriptCalculateNormalsOptions())
    unreal.GeometryScript_MeshEdits.append_mesh(mesh, arena, unreal.Transform())
    for hx, hy in base_hubs().values():
        GSP.append_cylinder(mesh, PRIM, xform(hx, hy, -50), BASE_R + 200, 50, 96, 0, True,
                            unreal.GeometryScriptPrimitiveOriginMode.BASE)


def build_ground():
    f = "Ground"
    BUCKETS.setdefault(("floor", True, "all"), []).append(build_floor)

    hubs = list(base_hubs().values())

    def ring(name, cx, cy, radius, segments, keep):
        step = 2 * math.pi / segments
        chord = 2 * radius * math.sin(step * 0.5) + 60
        for i in range(segments):
            a = i * step
            px, py = cx + (radius + 100) * math.cos(a), cy + (radius + 100) * math.sin(a)
            if keep(px, py):
                obox("%s_%d" % (name, i), f + "/Walls", px, py, 200, chord, 0, WALL_H, math.degrees(a), "block")

    ring("Wall_Arena", 0, 0, R, 120, lambda x, y: all(math.hypot(x - hx, y - hy) > BASE_R + 150 for hx, hy in hubs))
    for team, (hx, hy) in base_hubs().items():
        ring("Wall_Base_" + team, hx, hy, BASE_R, 64, lambda x, y: math.hypot(x, y) > R + 150)


def build_lanes():
    for lane in LANES:
        f = "Lanes/" + lane
        pts = [p for _, p in lane_samples(lane, 300)]
        SEGS.extend(zip(pts[:-1], pts[1:]))

        # Paving: a strip offset from the centre line, extruded.
        def offset_line(dist):
            out = []
            for k, p in enumerate(pts):
                a, b = pts[max(0, k - 1)], pts[min(len(pts) - 1, k + 1)]
                yaw = math.atan2(b[1] - a[1], b[0] - a[0])
                out.append((p[0] - math.sin(yaw) * dist, p[1] + math.cos(yaw) * dist))
            return out

        def strip(d0, d1):
            return offset_line(d0) + list(reversed(offset_line(d1)))

        polygon_slab("asphalt", True, strip(-LANE_HALF, LANE_HALF), 0, 1)

        # Street lamps along both edges.
        lf = "Atmosphere/Lamps"
        for i, (t_here, p) in enumerate(lane_samples(lane, LAMP_SPACING)[1:-1]):
            q = lane_point(lane, min(1.0, t_here + 0.01))
            yaw = math.atan2(q[1] - p[1], q[0] - p[0])
            nx, ny = -math.sin(yaw), math.cos(yaw)
            side = 1 if i % 2 == 0 else -1
            lx, ly = p[0] + nx * side * (LANE_HALF + 150), p[1] + ny * side * (LANE_HALF + 150)
            if math.hypot(lx, ly) < TOWER_HALF * 1.6:
                continue  # not inside the centre tower's gate or passage
            lbl = "Lamp_%s_%d" % (lane, i)
            obox(lbl + "_Post", lf, lx, ly, 20, 20, 0, 420, 0, "metal")
            obox(lbl + "_Head", lf, lx, ly, 60, 60, 420, 455, 0, "lamp", collide=False)
            point_light(lbl + "_Light", lf, (lx - nx * side * 60, ly - ny * side * 60, 400), LAMP_COLOR, LAMP_CANDELA, 1600)

        # A little cover inside each lane.
        for k, t in enumerate((-0.6, -0.3, 0.3, 0.6)):
            p = lane_point(lane, t)
            yaw = lane_yaw(lane, t)
            off = 350 if k % 2 == 0 else -350
            obox("%s_LaneCrate%d" % (lane, k), f, p[0] - math.sin(math.radians(yaw)) * off,
                 p[1] + math.cos(math.radians(yaw)) * off, 160, 160, 0, 110, yaw, "prop")


# Mesa templates in local (u, v), roughly centred; ("box", u0, u1, v0, v1, height, mat) / ("ramp", a, b, width).
TEMPLATES = {
    "low": [("box", -950, 50, -400, 400, LOW_H, "block"),
            ("ramp", (950, 0, 0), (50, 0, LOW_H), 400)],
    "tall": [("box", -1150, -450, -400, 400, TALL_H, "block"),
             ("box", -450, 350, -400, 400, LOW_H, "detail"),
             ("ramp", (1150, -200, 0), (350, -200, LOW_H), 400),
             ("ramp", (350, 200, LOW_H), (-450, 200, TALL_H), 400),
             ("light", -1060, 310, TALL_H)],  # a corner: bridges land on the top centre
    "stack": [("box", -1300, -700, -450, 450, STACK_H, "block"),
              ("box", -700, -100, -450, 450, TALL_H, "detail"),
              ("box", -100, 500, -450, 450, LOW_H, "detail"),
              ("ramp", (1300, -250, 0), (500, -250, LOW_H), 400),
              ("ramp", (500, 220, LOW_H), (-100, 220, TALL_H), 360),
              ("ramp", (-100, -220, TALL_H), (-700, -220, STACK_H), 360),
              ("light", -1210, 360, STACK_H)],
    "spire": [("box", -250, 250, -250, 250, SPIRE_H, "block"),
              ("box", 350, 650, -500, -200, 160, "prop"),
              ("box", -650, -400, 250, 500, 110, "prop")],
}
# Where bridges can land on each template: (u, v) of a 900 cm top.
BRIDGE_TOPS = {"tall": (-800, 0), "stack": (-400, 0)}
PASSES = [("stack", 0.5), ("tall", 0.8), ("low", 0.8), ("spire", 0.35), ("crates", 1.0)]  # biggest first; chance to try each spot


def footprint_points(elem):
    if elem[0] == "light":
        return []
    if elem[0] == "box":
        u0, u1, v0, v1 = elem[1:5]
    else:
        (au, av, _), (bu, bv, _), w = elem[1], elem[2], elem[3]
        u0, u1 = min(au, bu), max(au, bu)
        v0, v1 = min(av, bv) - w * 0.5, max(av, bv) + w * 0.5
    us = [u0 + (u1 - u0) * i / max(1, int((u1 - u0) / 300)) for i in range(max(1, int((u1 - u0) / 300)) + 1)]
    vs = [v0 + (v1 - v0) * i / max(1, int((v1 - v0) / 300)) for i in range(max(1, int((v1 - v0) / 300)) + 1)]
    return [(u, v) for u in us for v in vs]


def to_world(cx, cy, yaw_deg, u, v):
    c, s = math.cos(math.radians(yaw_deg)), math.sin(math.radians(yaw_deg))
    return (cx + u * c - v * s, cy + u * s + v * c)


def in_opening(p, margin=0.0):
    return any(x0 - margin < p[0] < x1 + margin and y0 - margin < p[1] < y1 + margin for x0, x1, y0, y1 in OPENINGS)


def area_ok(p):
    return (math.hypot(p[0], p[1]) < R - 400 and math.hypot(p[0], p[1]) > CENTER_CLEAR
            and lane_dist(p) > LANE_HALF + MESA_CLEAR and not in_opening(p, 300))


def place_template(name, elems, cx, cy, yaw):
    f = "Mesas"
    for k, e in enumerate(elems):
        if e[0] == "box":
            u0, u1, v0, v1, h, mat = e[1:]
            wx, wy = to_world(cx, cy, yaw, (u0 + u1) * 0.5, (v0 + v1) * 0.5)
            obox("%s_%d" % (name, k), f, wx, wy, u1 - u0, v1 - v0, 0, h, yaw, mat)
        elif e[0] == "light":
            # A lamp on the high ground: marks it at night and lights the top.
            wx, wy = to_world(cx, cy, yaw, e[1], e[2])
            obox("%s_LampPost%d" % (name, k), f, wx, wy, 20, 20, e[3], e[3] + 300, yaw, "metal")
            obox("%s_LampHead%d" % (name, k), f, wx, wy, 60, 60, e[3] + 300, e[3] + 335, yaw, "lamp", collide=False)
            point_light("%s_Light%d" % (name, k), f, (wx, wy, e[3] + 280), LAMP_COLOR, 200, 1800)
        else:
            a, b, w = e[1], e[2], e[3]
            wa, wb = to_world(cx, cy, yaw, a[0], a[1]), to_world(cx, cy, yaw, b[0], b[1])
            ramp("%s_%d" % (name, k), f, (wa[0], wa[1], a[2]), (wb[0], wb[1], b[2]), w, "detail")


def build_mesas():
    """Shuffled candidate spots; each gets a random kind and tries 8 headings. A placement must stay
    inside the arena, off the lanes and openings, and MESA_GAP away from everything placed before."""
    rng = random.Random(SEED)
    occupied = {}  # spatial hash of placed footprint points

    def free(pts):
        for x, y in pts:
            kx, ky = int(x // 500), int(y // 500)
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for ox, oy in occupied.get((kx + dx, ky + dy), ()):
                        if math.hypot(x - ox, y - oy) < MESA_GAP:
                            return False
        return True

    def occupy(pts):
        for x, y in pts:
            occupied.setdefault((int(x // 500), int(y // 500)), []).append((x, y))

    jobs = []
    for kind, chance in PASSES:
        step = MESA_SPACING if kind == "crates" else MESA_SPACING // 2
        spots = [(x, y, kind) for x in range(-R, R + 1, step) for y in range(-R, R + 1, step)]
        rng.shuffle(spots)
        jobs += [j for j in spots if rng.random() < chance]
    counts = {}
    tops = []  # (x, y, mesa index) of 900 cm tops, for bridges
    high = []  # (x, y, mesa index) footprint points of anything taller than a bridge's underside
    for idx, (sx, sy, kind) in enumerate(jobs):
        cx, cy = sx + rng.uniform(-200, 200), sy + rng.uniform(-200, 200)
        if not area_ok((cx, cy)):
            continue
        label = "Mesa_%d" % idx
        if kind == "crates":
            placed = 0
            for k in range(rng.randint(3, 5)):
                px, py = cx + rng.uniform(-800, 800), cy + rng.uniform(-800, 800)
                half, h = rng.uniform(80, 160), rng.choice((110, 160, 210))
                pts = [(px + a * half, py + b * half) for a in (-1, 0, 1) for b in (-1, 0, 1)]
                if all(area_ok(q) for q in pts) and free(pts):
                    obox("%s_Crate%d" % (label, k), "Mesas", px, py, 2 * half, 2 * half, 0, h, rng.uniform(0, 90), "prop")
                    occupy(pts)
                    placed += 1
            if placed:
                counts["crates"] = counts.get("crates", 0) + 1
                if counts["crates"] % 2:
                    point_light(label + "_CrateLight", "Mesas", (cx, cy, 320), LAMP_COLOR, 120, 1300)
            continue
        elems = TEMPLATES[kind]
        base_yaw = rng.uniform(0, 45)
        for turn in range(0, 360, 45):
            yaw = base_yaw + turn
            pts = [to_world(cx, cy, yaw, u, v) for e in elems for u, v in footprint_points(e)]
            if all(area_ok(q) for q in pts) and free(pts):
                place_template(label + "_" + kind, elems, cx, cy, yaw)
                occupy(pts)
                counts[kind] = counts.get(kind, 0) + 1
                if kind in BRIDGE_TOPS:
                    tops.append(to_world(cx, cy, yaw, *BRIDGE_TOPS[kind]) + (idx,))
                for e in elems:
                    if e[0] == "box" and e[5] > TALL_H - 100:
                        high.extend(to_world(cx, cy, yaw, u, v) + (idx,) for u, v in footprint_points(e))
                break
    log("mesas: %s" % counts)
    build_bridges(rng, tops, high)


def build_bridges(rng, tops, high):
    """9 m walkways between nearby tall tops, over lanes too, if nothing taller is in the way."""
    pairs = []
    for i, a in enumerate(tops):
        for b in tops[i + 1:]:
            d = math.hypot(b[0] - a[0], b[1] - a[1])
            if BRIDGE_RANGE[0] <= d <= BRIDGE_RANGE[1]:
                pairs.append((d, a, b))
    pairs.sort(key=lambda e: e[0])
    used, made = set(), 0
    for d, a, b in pairs:
        if made >= MAX_BRIDGES or a[2] in used and b[2] in used:
            continue
        n = int(d / 150)
        path = [(a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n) for k in range(n + 1)]
        blocked = any(math.hypot(px - hx, py - hy) < 250 for px, py in path for hx, hy, hi in high if hi not in (a[2], b[2]))
        if blocked or any(not (CENTER_CLEAR < math.hypot(px, py) < R - 300) or in_opening((px, py), 100) for px, py in path):
            continue
        # Crossing the Mid lane where its cable (and the rider under it) is near bridge height would block the ride.
        if any(abs(py - lane_point("Mid", px / GATE_X)[1]) < LANE_HALF + 200
               and abs(zip_z("Mid", px / GATE_X) - TALL_H) < 450 for px, py in path if abs(px) <= GATE_X):
            continue
        yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
        obox("Bridge_%d" % made, "Bridges", (a[0] + b[0]) * 0.5, (a[1] + b[1]) * 0.5, d, 300, TALL_H - SLAB, TALL_H, yaw, "detail")
        point_light("Bridge_%d_Light" % made, "Bridges", ((a[0] + b[0]) * 0.5, (a[1] + b[1]) * 0.5, TALL_H + 250), LAMP_COLOR, 150, 1500)
        used.update((a[2], b[2]))
        made += 1
    log("bridges: %d" % made)

def abox(label, folder, x0, x1, y0, y1, z0, z1, mat, collide=True):
    x0, x1 = sorted((x0, x1))
    y0, y1 = sorted((y0, y1))
    return obox(label, folder, (x0 + x1) * 0.5, (y0 + y1) * 0.5, x1 - x0, y1 - y0, z0, z1, 0, mat, collide)


def build_underground():
    f = "Underground"
    top = -50  # underside of the floor tiles
    u, t, w = UNDERCROFT, TUNNEL_HALF, 100
    # Undercroft: floor, walls (gaps for the tunnels), pillars, cover, lights.
    abox("Under_Floor", f, -u, u, -u, u, UNDER_Z - 50, UNDER_Z, "detail")
    abox("Under_Wall_E", f, u, u + w, -u - w, u + w, UNDER_Z, top, "block")
    abox("Under_Wall_W", f, -u - w, -u, -u - w, u + w, UNDER_Z, top, "block")
    for sy in (1, -1):
        for sx in (1, -1):
            abox("Under_Wall_%d_%d" % (sy, sx), f, sx * t, sx * u, sy * u, sy * (u + w), UNDER_Z, top, "block")
    for sx in (1, -1):
        for sy in (1, -1):
            abox("Under_Pillar_%d_%d" % (sx, sy), f, sx * 900 - 150, sx * 900 + 150, sy * 900 - 150, sy * 900 + 150,
                 UNDER_Z, top, "block")
            point_light("Under_Light_%d_%d" % (sx, sy), f, (sx * 1500, sy * 600, UNDER_Z + 400), (1.0, 0.45, 0.15), 200, 1800)
            abox("Under_Crate_%d_%d" % (sx, sy), f, sx * 400 - 80, sx * 400 + 80, sy * 1700 - 80, sy * 1700 + 80,
                 UNDER_Z, UNDER_Z + 110, "prop")
    abox("Under_Dais", f, -350, 350, -350, 350, UNDER_Z, UNDER_Z + 60, "metal")
    # Glowing trim along the undercroft walls, lane-coloured per side.
    for name, x0, x1, y0, y1, mat in (("E", u - 4, u, -u, u, "lane_Mid"), ("W", -u, -u + 4, -u, u, "lane_Mid"),
                                      ("N", -u, u, u - 4, u, "lane_North"), ("S", -u, u, -u, -u + 4, "lane_South")):
        abox("Under_Trim_" + name, f, x0, x1, y0, y1, UNDER_Z + 250, UNDER_Z + 265, mat, collide=False)

    # Tunnels north and south: flat, then a ramp up through the floor opening into the jungle.
    for sy, lane in ((1, "North"), (-1, "South")):
        tf = f + "/Tunnel_" + lane
        abox("Tunnel_%s_Floor" % lane, tf, -t, t, sy * u, sy * 2700, UNDER_Z - 50, UNDER_Z, "detail")
        ramp("Tunnel_%s_Ramp" % lane, tf, (0, sy * 2700, UNDER_Z), (0, sy * TUNNEL_END, 0), 2 * t, "detail")
        for sx in (1, -1):
            abox("Tunnel_%s_Wall_%d" % (lane, sx), tf, sx * t, sx * (t + w), sy * u, sy * TUNNEL_END, UNDER_Z, top, "block")
            abox("Tunnel_%s_Trim_%d" % (lane, sx), tf, sx * (t - 4), sx * t, sy * u, sy * TUNNEL_END,
                 UNDER_Z + 200, UNDER_Z + 215, "lane_" + lane, collide=False)
        abox("Tunnel_%s_EndWall" % lane, tf, -t - w, t + w, sy * TUNNEL_END, sy * (TUNNEL_END + w), UNDER_Z, top, "block")
        point_light("Tunnel_%s_Light" % lane, tf, (0, sy * 2600, UNDER_Z + 350), LANE_COLORS[lane], 150, 1500)



def build_tower(mesh):
    """Centre tower as one mesh: base block minus the gate (along the Mid lane at the centre) and a cross passage,
    a spire on the deck, and two ramps up to the deck."""
    gate_yaw = lane_yaw("Mid", 0.0)
    cy0 = lane_point("Mid", 0.0)[1]
    base = unreal.DynamicMesh()
    GSP.append_box(base, PRIM, xform(0, cy0, 0, gate_yaw), 2 * TOWER_HALF, 2 * TOWER_HALF, TOWER_DECK, 0, 0, 0,
                   unreal.GeometryScriptPrimitiveOriginMode.BASE)
    bopts = unreal.GeometryScriptMeshBooleanOptions()
    gw, gh = TOWER_GATE
    for length, width, height, yaw in ((2 * TOWER_HALF + 400, gw, gh, gate_yaw),
                                       (2 * TOWER_HALF + 400, 700, 450, gate_yaw + 90.0)):
        tool = unreal.DynamicMesh()
        GSP.append_box(tool, PRIM, xform(0, cy0, -100, yaw), length, width, height + 100, 0, 0, 0,
                       unreal.GeometryScriptPrimitiveOriginMode.BASE)
        unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(base, unreal.Transform(), tool, unreal.Transform(),
                                                              unreal.GeometryScriptBooleanOperation.SUBTRACT, bopts)
    # Spire on the deck, centred on the lane, with the Mid zipline's tunnel bored through it along the lane.
    spire = unreal.DynamicMesh()
    GSP.append_box(spire, PRIM, xform(0, cy0, TOWER_DECK, gate_yaw), 900, 900, TOWER_TOP - TOWER_DECK, 0, 0, 0,
                   unreal.GeometryScriptPrimitiveOriginMode.BASE)
    slot = unreal.DynamicMesh()
    GSP.append_box(slot, PRIM, xform(0, cy0, TOWER_ZIP_Z - 320, gate_yaw), 1400, TUNNEL[0], TUNNEL[1], 0, 0, 0,
                   unreal.GeometryScriptPrimitiveOriginMode.BASE)
    unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(spire, unreal.Transform(), slot, unreal.Transform(),
                                                          unreal.GeometryScriptBooleanOperation.SUBTRACT, bopts)
    for part in (base, spire):
        unreal.GeometryScript_Normals.compute_split_normals(part, unreal.GeometryScriptSplitNormalsOptions(),
                                                            unreal.GeometryScriptCalculateNormalsOptions())
        unreal.GeometryScript_MeshEdits.append_mesh(mesh, part, unreal.Transform())


def build_center():
    f = "Center"
    BUCKETS.setdefault(("block", True, "tower"), []).append(build_tower)
    # Ramps up to the deck along two sides (opposite directions), in the tower's frame.
    gate_yaw = lane_yaw("Mid", 0.0)
    cy0 = lane_point("Mid", 0.0)[1]
    for side, run_dir in ((1, -1), (-1, 1)):
        a = to_world(0, cy0, gate_yaw, run_dir * -TOWER_HALF, side * (TOWER_HALF + 200))
        b = to_world(0, cy0, gate_yaw, run_dir * (TOWER_HALF - 300), side * (TOWER_HALF + 200))
        ramp("Tower_Ramp_%d" % side, f, (a[0], a[1], 0), (b[0], b[1], TOWER_DECK), 400, "detail")
        land = to_world(0, cy0, gate_yaw, run_dir * TOWER_HALF - run_dir * 150, side * (TOWER_HALF + 200))
        obox("Tower_Landing_%d" % side, f, land[0], land[1], 300, 400, TOWER_DECK - SLAB, TOWER_DECK, gate_yaw, "detail")
    obox("Tower_Crown", f, 0, cy0, 960, 960, TOWER_TOP, TOWER_TOP + 60, gate_yaw, "lamp", collide=False)
    point_light("Tower_CrownLight", f, (0, cy0, TOWER_TOP + 300), (1.0, 0.55, 0.2), 1500, 5000)
    point_light("Tower_TunnelLight", f, (0, cy0, TOWER_ZIP_Z - 100), (1.0, 0.5, 0.15), 200, 900)
    for k, t in enumerate((-0.04, 0.04)):
        p = lane_point("Mid", t)
        point_light("Tower_GateLight_%d" % k, f, (p[0], p[1], 750), (1.0, 0.5, 0.15), 250, 1800)
    # Mantle course along the Mid lane's south edge, labelled, on both halves.
    for k, h in enumerate(MANTLE_HEIGHTS):
        for s in (1, -1):
            t = s * (3600 - k * 260) / GATE_X
            p = lane_point("Mid", t)
            yaw = lane_yaw("Mid", t)
            nx, ny = -math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
            bx, by = p[0] - nx * (LANE_HALF - 120), p[1] - ny * (LANE_HALF - 120)
            obox("Mantle_%d_%d" % (s, h), "MantleCourse", bx, by, 200, 200, 0, h, yaw, "detail")
            lx, ly = p[0] - nx * (LANE_HALF - 230), p[1] - ny * (LANE_HALF - 230)
            text("MantleLabel_%d_%d" % (s, h), "MantleCourse", (lx, ly, h + 40), yaw + 90.0, "%d cm" % h, 40.0)


def build_bases():
    for team_name, (hx, hy) in base_hubs().items():
        s = 1 if hx > 0 else -1
        f = "Base_" + team_name
        mat = "blue" if team_name == "Blue" else "red"
        team = unreal.TeamID.TEAM_BLUE if team_name == "Blue" else unreal.TeamID.TEAM_RED
        yaw_to_map = 180.0 if s > 0 else 0.0
        # Hub tower with a glowing cap.
        cylinder(team_name + "_Hub", f, hx, hy, 0, 1600, 700, mat)
        cylinder(team_name + "_HubCap", f, hx, hy, 1600, 1700, 760, "team_" + team_name, collide=False)
        point_light(team_name + "_HubLight", f, (hx - s * 600, hy, 1400), TEAM_COLORS[team_name], 2500, 6000)
        # One zipline station per lane, on the line from the hub to that lane's gate; ramp toward the map.
        for lane in LANES:
            g = lane_point(lane, float(s))
            ux, uy = g[0] - hx, g[1] - hy
            d = math.hypot(ux, uy)
            ux, uy = ux / d, uy / d
            ex, ey = hx + ux * STATION_R, hy + uy * STATION_R
            yaw = math.degrees(math.atan2(uy, ux))
            obox("%s_Station_%s" % (team_name, lane), f, ex, ey, 500, 500, 0, STATION_H, yaw, mat)
            obox("%s_StationTrim_%s" % (team_name, lane), f, ex, ey, 520, 520, STATION_H - 20, STATION_H - 5, yaw,
                 "lane_" + lane, collide=False)
            ramp("%s_StationRamp_%s" % (team_name, lane), f, (ex + ux * 250, ey + uy * 250, STATION_H),
                 (ex + ux * 1150, ey + uy * 1150, 0), 450, mat)
        # Five team starts behind the hub, facing the map.
        for k, ang in enumerate((-60, -30, 0, 30, 60)):
            a = math.radians(ang)
            px, py = hx + s * 1500 * math.cos(a), hy + 1500 * math.sin(a)
            player_start("%s_Start%d" % (team_name, k), (px, py, 100), yaw_to_map, team)
        text(team_name + "_Sign", f, (s * (GATE_X + 200), -1100, 650), yaw_to_map, team_name.upper() + " BASE", 140.0)


def zip_z(lane, t):
    """Cable height along a lane: North/South sag a little at mid, Mid climbs to the spire tunnel."""
    c = math.cos(math.pi * t * 0.5)
    if lane == "Mid":
        return ZIP_Z + (TOWER_ZIP_Z - ZIP_Z) * c * c
    return ZIP_Z - ZIP_SAG * c


def build_ziplines():
    hubs = base_hubs()
    for lane in LANES:
        pts = []
        for team_name in ("Blue", "Red"):
            hx, hy = hubs[team_name]
            s = 1 if hx > 0 else -1
            g = lane_point(lane, float(s))
            d = math.hypot(g[0] - hx, g[1] - hy)
            end = (hx + (g[0] - hx) / d * STATION_R, hy + (g[1] - hy) / d * STATION_R, ZIP_END_Z)
            if s < 0:
                pts.append(end)
                ts = [-1.0 + 0.2 * k for k in range(11)]
                if lane == "Mid":
                    ts = sorted(set(ts) | {-0.1, -0.05, 0.05, 0.1})  # pin the cable through the tunnel
                for t in ts:
                    p = lane_point(lane, t)
                    pts.append((p[0], p[1], zip_z(lane, t)))
            else:
                pts.append(end)
        zipline("Zip_" + lane, pts)


def build_minimap():
    """The area the HUD minimap shows; it draws the map itself at runtime (AZL_MinimapVolume)."""
    use_level("Gameplay")
    half_x = BASE_OFFSET + BASE_R + MINIMAP_MARGIN
    half_y = R + MINIMAP_MARGIN
    v = eas.spawn_actor_from_class(unreal.ZL_MinimapVolume, unreal.Vector(0, 0, MINIMAP_CENTER_Z), unreal.Rotator(0, 0, 0))
    v.get_editor_property("map_bounds").set_box_extent(unreal.Vector(half_x, half_y, MINIMAP_HALF_Z), True)
    tag(v, "MinimapVolume", "Gameplay")


def build_sky():
    f = "Atmosphere/Sky"
    use_level("Lighting")
    moon = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 3000),
                                      unreal.Rotator(roll=0.0, pitch=MOON_PITCH, yaw=MOON_YAW))
    c = moon.light_component
    setp(c, "atmosphere_sun_light", True)
    setp(c, "intensity", MOON_LUX)
    setp(c, "light_color", unreal.Color(r=int(MOON_COLOR[0] * 255), g=int(MOON_COLOR[1] * 255), b=int(MOON_COLOR[2] * 255), a=255))
    tag(moon, "Moon", f)
    tag(eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0)), "SkyAtmosphere", f)
    sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 2000), unreal.Rotator(0, 0, 0))
    setp(sky.light_component, "real_time_capture", False)  # captured once; real-time capture re-renders the sky every frame
    setp(sky.light_component, "intensity", SKY_LIGHT_INTENSITY)
    tag(sky, "SkyLight", f)
    fog = eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -100), unreal.Rotator(0, 0, 0))
    fc = fog.component
    setp(fc, "fog_density", FOG_DENSITY)
    setp(fc, "fog_height_falloff", FOG_FALLOFF)
    setp(fc, "start_distance", 1200.0)
    setp(fc, "enable_volumetric_fog", False)
    setp(fc, "fog_inscattering_luminance", unreal.LinearColor(FOG_COLOR[0], FOG_COLOR[1], FOG_COLOR[2], 1.0))
    setp(fc, "directional_inscattering_luminance", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
    tag(fog, "HeightFog", f)

    pp = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    setp(pp, "unbound", True)
    s = pp.settings
    for name, value in (("bloom_intensity", 1.0), ("vignette_intensity", 0.45), ("ambient_occlusion_intensity", 0.5),
                        ("motion_blur_amount", 0.0),
                        ("auto_exposure_min_brightness", EXPOSURE_EV100), ("auto_exposure_max_brightness", EXPOSURE_EV100),
                        ("auto_exposure_bias", 0.0),
                        ("dynamic_global_illumination_method", unreal.DynamicGlobalIlluminationMethod.NONE),
                        ("reflection_method", unreal.ReflectionMethod.SCREEN_SPACE)):
        try:
            s.set_editor_property("override_" + name, True)
            s.set_editor_property(name, value)
        except Exception as e:
            log("post process %s: %s" % (name, e))
    setp(pp, "settings", s)
    tag(pp, "PostProcess", f)


def build_hotel():
    """Doorman's hotel, always loaded, far above the map where Hotel Guest would otherwise spawn one."""
    use_level("DoormanHotel")
    hotel = eas.spawn_actor_from_class(unreal.ZL_Doorman_Hotel, unreal.Vector(*HOTEL_LOCATION), unreal.Rotator(0, 0, 0))
    tag(hotel, "DoormanHotel", "Doorman")


def setup_world_settings():
    ws = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_world_settings()
    setp(ws, "default_game_mode", eal.load_blueprint_class(GAME_MODE))
    setp(ws, "kill_z", -2000.0)


def main():
    load_materials()
    open_level()
    setup_sublevels()
    with unreal.ScopedEditorTransaction("Build Lvl_LaneTest"):
        build_ground()
        build_lanes()       # fills SEGS, needed by the mesa placement
        build_mesas()
        build_underground()
        build_center()
        build_bases()
        build_ziplines()
        build_minimap()
        build_sky()
        flush_meshes()
        build_hotel()
        use_level("Persistent")
        setup_world_settings()
    les.save_all_dirty_levels()
    log("done, %d actors, saved %s" % (COUNT[0], LEVEL_PATH))


main()
