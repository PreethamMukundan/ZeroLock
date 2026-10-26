# Deadlock Hero Importer

Imports Deadlock heroes (mesh, skeleton, animations, materials) into Unreal for local prototyping.
Valve assets: keep them in `/Game/Developers/...` and never ship them.

## Usage

**1. Export** (Source2Viewer → Blender → FBX), from this folder:

```
deadlock_import.bat lash apollo        # by display name or codename
deadlock_import.bat --list             # every hero: display name, codename, model path
deadlock_import.bat lash --anims all   # all clips instead of the filtered set (or: none)
deadlock_import.bat lash --data-only   # just refresh camera + stats in an existing export (seconds)
```

**2. Import** in the Unreal editor console (`` ` `` key, Cmd mode). The export prints this line for you:

```
py "E:/Repo/ZeroLock/ZeroLock/Tools/DeadlockImporter/ue_import.py" Lash Apollo
py ".../ue_import.py" Lash --no-anims     # mesh + materials only (fast)
py ".../ue_import.py" Lash --anims-only   # just (re)import animations
py ".../ue_import.py" Lash --anim-set-only   # just (re)build DA_<Hero>_AnimSet (resets hand edits)
py ".../ue_import.py" Lash --hero-only    # just BP_Deadlock_<Hero> + DA_<Hero>_Hero + DT_HeroList row
py ".../ue_import.py" Lash --camera-only  # existing BP: spring arm + camera from Deadlock's camera settings
py ".../ue_import.py" Lash --stats-only   # (re)write GE_<Hero>_DefaultStats from Deadlock's stats, set it on the BP
```

Result, per hero, in `/Game/Developers/rpm86/Deadlock/<Hero>/`:
`SK_<Hero>` (+ `_Skeleton`, `_PhysicsAsset`), `Materials/MI_*`, `Textures/T_*`, `Animations/A_<Hero>_<clip>`.
Shared master materials live in `Deadlock/_Shared/`.

Timing (Lash, 233 clips): export ~3 min, Unreal import ~3 min.

## Master anim blueprint (Mover)

`/Game/ZERO/Animation/Deadlock/ABP_DeadlockHero` is one template anim blueprint for every Deadlock hero.
Its graph is a single **Deadlock Hero** node (C++: `FAnimNode_ZLDeadlockHero`) driven by
`UZL_DeadlockAnimInstance`, which reads the pawn's Mover component. The node handles:

- 8-way run / crouch-run cycles (phase-synced, play rate scaled to speed), idle blend
- jump, multi-jump, apex/fall loops, landing / hard landing, slide, dash (directional), wall jump, zipline
- aim offset from camera pitch (upper body only)
- montage slots: `UpperBody` (from `spine_1` up, legs keep running) and `DefaultSlot` (full body)
- strips the `root_motion` bone's movement so clips and montages play in place; Mover moves the pawn

Per hero, `ue_import.py` builds `DA_<Hero>_AnimSet` (clip names differ between heroes, so each slot tries a list
of aliases; missing slots fall back sensibly). To use a hero on a ZeroLock character:

1. Mesh component: Skeletal Mesh `SK_<Hero>`, Anim Class `ABP_DeadlockHero`, scale ~0.7 (heroes are ~2.6m tall)
2. Pawn defaults: **Deadlock Anim Set** = `DA_<Hero>_AnimSet` (property on `AZeroMoverPawn`)

`BP_Deadlock_Lash` / `BP_Deadlock_Apollo` (duplicates of `BP_Mover_Monkey`) are set up this way.
Tuning (blend times, reference speeds, aim range) lives on the anim set asset.

## Ability montages

`ue_import.py` also creates montages (DefaultSlot, existing ones are kept) grouped by the hero's real ability names,
read from the game data (`heroes.vdata` bound abilities + English localization):

```
<Hero>/Montages/<AbilityName>/AM_<Hero>_<AbilityName>[_<Part>]   e.g. Lash/Montages/DeathSlam/AM_Lash_DeathSlam_Throw
<Hero>/Montages/Melee/AM_<Hero>_Melee_<Quick1|Start|Dash|Hit|...>
<Hero>/Montages/Parry/AM_<Hero>_Parry_*
<Hero>/Montages/Other/          ability_* clips that didn't match an ability
```

Folder names match the ZeroLock ability classes (`ZL_Lash_DeathSlam` -> `DeathSlam`, `ZL_Apollo_ItaniLoSahn` -> `ItaniLoSahn`).
For the `UpperBody` layered slot (legs keep moving), change a montage's slot to `UpperBody`.
Re-run just this step with `--montages-only`.

## Playable hero + hero select

A full import also makes the hero playable and pickable (`config.json` → `hero_setup`). Existing assets are kept, so
re-importing keeps your edits, except that an existing BP's camera, movement, `WeaponStats` and stats GE are refreshed
from the Deadlock data:

- `BP_Deadlock_<Hero>`: a copy of `BP_Deadlock_Lash` (Mover setup, weapon, **Lash's abilities** until you give it its own)
  with this hero's mesh, `ABP_DeadlockHero`, `DA_<Hero>_AnimSet` and `AM_<Hero>_Melee_Quick1` / `_Hit` melee montages.
- `DA_<Hero>_Hero`: hero select data (name, class, mesh, `ui_hero_select`-style pose). Icon, icon colour and preview
  framing are copied from `DA_Lash_Hero`: change them on the asset.
- A `<Hero>` row in `DT_HeroList`.
- Camera: spring arm length, shoulder offset and pivot height from the hero's own Deadlock camera settings, FOV 106.26
  (Deadlock's 90 at 4:3 = 106.26 at 16:9).
- `GE_<Hero>_DefaultStats` (copy of `_Shared/GE_Deadlock_DefaultStats`) with the hero's Deadlock health, regen,
  stamina, resistances, bullet damage, fire rate, clip size, reload time and light/heavy melee damage, set as the BP's
  Default Gameplay Effect. The log lists the Deadlock values that have no ZeroLock attribute yet.
- `BP_<Hero>_Projectile` (copy of `BP_ProjectileTest`) with Deadlock's bullet speed, gravity and range, set as the
  BP's `WeaponStats.ProjectileClass`. Made once: later imports keep your edits, so tune the bullet on that asset.
- Weapon profile on the BP (`WeaponStats`): muzzle bone, pellets, burst, damage falloff. Mover walk/crouch/dash speed scaled by the
  hero's Deadlock speed (`movement_reference` in `config.json`).
- `Montages/Reload/AM_<Hero>_Reload` and `Montages/Shoot/AM_<Hero>_Shoot` (UpperBody slot), played by the reload and
  fire abilities through the anim set.

Already-imported hero, after a `--data-only` export:
`py ".../ue_import.py" Lash --montages-only --camera-only --stats-only` (the `--*-only` flags combine). Use the flags:
a plain run re-imports the mesh and every animation (minutes per hero).

Additive clips (e.g. `shoot_idle`, `landing_impact`) come out of S2V composed over the bind pose and look like a T-pose.
The export lists them in the manifest (`additive_clips`; refresh with `--data-only`); the Unreal step makes montage clips
additive to the ref pose and keeps them out of full-pose anim set slots. Details in `Docs/Claude/Animation.md`.

GE modifiers and `WeaponStats` are edited with `set_struct_fields` (struct `export_text`/`import_text`):
`set_editor_property` refuses `EditDefaultsOnly` fields inside a struct ("cannot be edited on instances"), even on a CDO.

For a hero that's already imported: `py ".../ue_import.py" Solomon --hero-only`.

## Config (`config.json`)

| Key | What |
|---|---|
| `deadlock_dir`, `source2viewer_cli`, `blender` | Tool paths |
| `output_dir` | Intermediate files (glb, FBX, textures, `.blend`, manifest) – under `Saved/`, not in git |
| `ue_dest_root` | Unreal folder |
| `normal_flip_green` | Flip normal map green channel in Unreal. Toggle if lighting on bumps looks inverted |
| `animations.include` / `exclude` | Regexes on clip names (e.g. `weapon_run_n`, `ability_flog`) for `filtered` mode |

## Notes

- Hero names/models are read from the game (`scripts/heroes.vdata`, English localization), so new heroes work
  automatically. Several unreleased heroes still point at the `gen_man` placeholder model.
- Deadlock skeletons have two root bones (`root_motion`, `pelvis`); the FBX adds a single `root` bone above them.
- When a clip exists both as a legacy sequence and an AnimGraph2 clip, the `/clips/` version is used.
  Additive clips are composed over the bind pose.
- Materials are an approximation: base color, ORM (AO/rough/metal), normal, self-illum. Deadlock's NPR
  outline/rim lighting is not reproduced.
- `<output_dir>/<Hero>/<Hero>.blend` has the cleaned-up scene if you want to edit in Blender.
