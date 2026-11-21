# Hero effects (what's left of the hero kit)

**2026-11-19: the data-driven hero kit was removed.** That covered the payload system, the `UZL_Kit_*` ability, projectile and zone classes, `ZL_HeroPassives`, the Ryu, KaijuX and LadyGrimm C++, all 19 sheet heroes (BPs, DAs, weapons, abilities, icons, hero-select rows), their cooldown tags, and `Tools/HeroKit` `hero_specs.py`, `build_heroes.py`, `gen_tags.py`, `make_icons.py` and `verify_heroes.py`. The user is rebuilding heroes by hand on the base abilities (`UBaseGameplayAbility`, `UZL_BaseDashAbility`, `UZL_BaseSurfaceTargetAbility`, `UZL_BaseConeTargetAbility`; Pocket pattern, Heroes.md). Only the effects survived and are meant to be reused.

Kept from the kit, still in the engine: `EZL_ItemStatus::Root` (`ZeroLock.Movement.Rooted`), the native tags `Zerolock.Status.Immortal` (hero damage stops at 1 HP) and `Zerolock.Status.IgnoreBarrier` (on the attacker: its damage skips the combat barrier), and `AZeroMoverPawn::IsJumpInputHeld()`.

## Effects (`HeroKit/ZL_KitCue.h`): Niagara through gameplay cues
Abilities shouldn't spawn effects directly. Hold an `FZL_KitCue` (`Cue` tag, `Duration`); the notify asset with that tag owns the look:
- `UZL_GCN_KitBurst` (`UGameplayCueNotify_Static`, executed) or `AZL_GCN_KitLoop` (`AGameplayCueNotify_Actor`, added/removed, attached to the target).
- Both use `FZL_KitCueVisual`: `NiagaraSystem`, `Sound`, `Scale` (× radius / `ReferenceRadius` with `bScaleWithRadius`; radius = the cue's `RawMagnitude`), `ForwardOffset`/`UpOffset` along the cue's facing (`Normal`), `bAttachToTarget`, `FloatParameters` (Niagara user floats), `RadiusParameter` (default `Radius`: the cue radius ÷ scale, since Niagara shapes are in component space).

`ZLKitCue::Play(ASC, Cue, Location, Facing, Radius)`: `Duration` 0 = `ExecuteGameplayCue` on that ASC (predicted inside an ability's activation on the owner; the server's multicast skips the owner); `Duration` > 0 (server) = `AddGameplayCue` + `RemoveGameplayCue` after it (replicated loop on that hero). `AddLocal`/`RemoveLocal`/`ExecuteLocal` = `UGameplayCueManager::*_NonReplicated`, for actors every machine already simulates.

**`AProjectile`** (all projectiles, weapon bullets too) has `TrailCue` (local loop, added in `BeginPlay`, removed in `ShutDown`/`Destroyed`) and `HitCharacterCue`/`HitWorldCue` (local execute in `Detonate`, next to the Cascade particles).

## Assets
- Cue notifies: `Content/ZERO/Heroes/KaiKoa/FX/GCN_*` (Absolute Zero, Arctic Burst, Frozen Fist, Shark Shark Shark, Tsunami Fist, Weapon hit/trail; used again by the hand-built Kai Koa, Heroes.md) and `Content/ZERO/Heroes/Tempest/FX/GCN_*` (Alley-oop, I Am Number 10, Phantom Drive, Setting a Pick, Side Step, Weapon hits). Tags `GameplayCue.HeroKit.<Hero>.<Ability>.<Role>` in `Config/Tags/HeroKitTags.ini` (now cue tags only, edited by hand since `gen_tags.py` is gone).
- Niagara sources in `/Game/ZERO/HeroKit/FX/`:
  - `M_HeroKit_Ice`: particle colour, fresnel glow, Niagara mesh/sprite usage. Made headlessly by `Tools/HeroKit/make_fx_assets.py`.
  - `Niagara/NS_HeroKit_IceSpikes`: one mesh emitter (engine cone, pivot at its base, ice material). User params: `SpikeCount` (burst), `Radius` (flattened sphere shape), `Height` (ScaleMeshSize = (0.32, 0.32, 1) × Height/100; rises over the first 11% of life and sinks over the last 20%), `Lifetime`.
  - Wind assets: `M_HeroKit_WindShell`, `M_HeroKit_EnergyCore` with their instances, `NS_HeroKit_WindRibbon`/`WindTrail`, and the Cloud systems. Made by `make_wind_shell.py` and `make_energy_core.py`.
  - `Niagara/NS_<name>`: Niagara conversions of Paragon effects (the `make_fx_assets.py` `CONVERT` list, through the **CascadeToNiagaraConverter** plugin, enabled in `ZeroLock.uproject`). The conversion needs the editor UI: run `py "E:/Repo/ZeroLock/ZeroLock/Tools/HeroKit/make_fx_assets.py"` in the editor, because the commandlet asserts in `Finalize`.
- **Converted systems must be pruned and their materials flagged.** The converter copies every Cascade emitter (the grenade explosion had 31). Paragon materials also lack the Niagara usage flags (`bUsedWithNiagaraSprites/Ribbons/MeshParticles`), so the game draws the default grey material and logs "missing usage flag NiagaraSprites!".
  - The Twinblast water systems keep only their water emitters. **They have no mesh emitters**, because `M_Shockwave_Refract` and the splash/foam mesh materials read Dynamic Parameters that the conversion doesn't drive, so they render as bare cones.
  - For a newly converted system, `RemoveEmitter` the noise, then set the flag on the **base** material (walk `Parent` from the instances), recompile and save.
  - `NS_TwinBlast_VortexGrenade_Explode_Summer` is unpruned and unused.
- `Tools/HeroKit/verify_fx.py` checks the FX assets.
