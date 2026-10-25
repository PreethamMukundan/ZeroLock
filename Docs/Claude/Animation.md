# Animation: Deadlock hero anim system

One template anim blueprint, `/Game/ZERO/Animation/Deadlock/ABP_DeadlockHero`, drives every imported Deadlock hero.
Its AnimGraph is a single **Deadlock Hero** node → Output Pose. All logic is C++.

```
AZeroMoverPawn ──DeadlockAnimSet──► DA_<Hero>_AnimSet (UZL_DeadlockAnimSet: clips + tuning)
   └ Mesh, Anim Class = ABP_DeadlockHero (parent UZL_DeadlockAnimInstance)
        NativeUpdateAnimation reads UZeroMoverComponent → FZLDeadlockAnimState
        AnimGraph: FAnimNode_ZLDeadlockHero → layers → aim offset → montage slots → strip root motion
```

## Files

| File | Role |
|---|---|
| `Source/ZeroLock/Public/Animation/ZL_DeadlockAnimSet.h/.cpp` | Per-hero data asset. Clip slots, `FZLDirectionalAnims` (8-way, N=0 clockwise, `GetWithFallback` searches ±4), `FZLAimAnims` (Center/Up/Down), tuning. |
| `Source/ZeroLock/Public/Animation/ZL_DeadlockAnimInstance.h/.cpp` | ABP parent class. Game thread. Mover → `State`. Never touches bones. |
| `Source/ZeroLock/Public/Animation/AnimNode_ZLDeadlockHero.h/.cpp` | Runtime node (~680 lines). Whole pose. |
| `Source/ZeroLockUncooked/.../AnimGraphNode_ZLDeadlockHero.h/.cpp` | Graph node ("Deadlock Hero", category ZeroLock) in the **UncookedOnly** module `ZeroLockUncooked` (AnimGraph, AnimGraphRuntime, BlueprintGraph). It must not live in an Editor module: uncooked `UnrealEditor.exe -game` runs don't load Editor modules but do recompile anim blueprints on load (KismetCompiler), so the ABP compiled without its only node and every hero had no animation. CoreRedirect from `/Script/ZeroEditorModule.AnimGraphNode_ZLDeadlockHero` in `DefaultEngine.ini`. |
| `UZeroMoverComponent::DashCount / LastDashDirection` | Bumped in `HandleDashInputs` (skipped while resimulating) so the anim can see dashes, which are layered moves, not a mode. |
| `AZeroMoverPawn::DeadlockAnimSet` | Per-pawn anim set (Class Defaults). |

## Anim instance (`UZL_DeadlockAnimInstance`)
- `ResolveAnimSet()`: instance `AnimSet` override, else pawn `DeadlockAnimSet`. On change it measures:
  - run / crouch authored speed from `root_motion` bone travel of the N clip (`MeasureCycleSpeed`), falling back to `ReferenceRunSpeed` 600 / `ReferenceCrouchSpeed` 300 when the clip is in place;
  - `SlideFromAirStartTime`: the asset value, or (when -1) the lowest pelvis point in the first 60% of `SlideStart` (`MeasureSlideLandTime`).
- `NativeUpdateAnimation`: velocity (and dash direction) in the space of the **drawn** facing: mesh rotation × inverse of Mover's base visual rotation, because smoothing turns the mesh between sim steps while the actor only turns on them (Movement.md "Facing and smoothing"), ground/vertical speed, aim pitch (`GetBaseAimRotation`), mode flags by name (`Sliding`, `Dashing`, `Ziplining`, `WallJumping`, `AirJumping`).
- `bStunned`: the pawn's ASC has `ZeroLock.Stun` (`TAG_STUN`; replicated tag, so simulated proxies see it). The node then plays `StunStart` (optional, once) into `StunLoop` full body, no aim offset, no dash layer. A set without either clip shows no stun pose. The importer fills `stun_loop` from `standing_stunned` / `crouching_stunned` / `knockdown_medium_loop`; `StunStart` is set by hand.
- `bHeavyMelee`: Mover mode `MeleeModeName` ("Melee", the heavy melee lunge, `MaxDuration` 0.5 s / `LungeSpeed` 750 on the hero BPs) → `HeavyMeleeStart` (`melee_start`, 0.47 s) squeezed into `HeavyMeleeStartTime` 0.15 s (sped up, never slowed), then `HeavyMeleeLoop` (`melee_dash`) until the lunge ends, blended in over `JumpBlendTime`. Leaving the Melee mode bumps `HeavyMeleeEndCount` → `HeavyMeleeHit` (`melee_hit`, 0.67 s) one-shot. It plays at the end of **every** lunge, hit or whiff, because the strike sweep (`AZeroLockCharacter::MeleeStrike`) runs either way. `bHeavyMeleeInAir` (set on entering the mode, from the previous frame's ground state) picks `HeavyMeleeAir{Start,Loop,Hit}` (`melee_in_air_*`), each falling back to the ground clip. The hero BPs' `HeavyMeleeMontage` (`AM_<Hero>_Melee_Hit`) is only used by the unused `UZeroBase_HeavyMelee`. `LightMeleeMontage` (`AM_<Hero>_Melee_Quick1`) is used by `UZeroBase_LightMelee`. Importer: `ANIM_SET_SLOTS` + `ANIM_SET_MONTAGES`; a full import now builds montages before the anim set. Fill existing sets with `py ".../ue_import.py" Lash Pocket Apollo --anim-set-only` (resets hand edits to those sets).
- One-frame events are **counters** (`JumpCount`, `LandCount`, `DashCount`) so the worker thread can't miss them:
  - Dash: Mover `DashCount` changed (local/server), or fallback speed burst > `DashSpeed × 0.75` with 0.35 s debounce (simulated proxies).
  - Jump: took off going up (>100), air kick (+250 in one frame), or entered AirJumping/WallJumping. Not within 0.3 s of a mantle or wall bounce (their kick isn't a jump).
  - Mantle / wall bounce: `FZeroMoverSimState::MantleCount` (+ `MantleHeight`) / `WallJumpCount` changed (Movement.md). These are layered moves, not modes, so there's no mode to watch. The counters live in the sync state, so they roll back with resimulation and reach simulated proxies.
  - Land: touched ground after >0.2 s airborne; `LastLandingSpeed` = max fall speed.

## Node (`FAnimNode_ZLDeadlockHero`)
**Layer stack** (max 4). A state change pushes a layer that fades in over its blend time; older layers share the remaining weight proportionally and are removed under 0.001. `PushLayer(State, BlendTime, Clip, StartTime)`.

**Update** order: counters → `TimeSinceAirborne` → dash (`ChooseDashClip` by dominant axis; held for `DashHoldTime` 0.45 s or while bursting, jump cancels) → `ChooseState()` priority Stunned > HeavyMelee > Zipline > Slide > WallJump > Air > Crouch/Ground → per-state push rules:
- One-shot events (before `ChooseState`): mantle → **Mantle** layer with `ChooseMantleClip()` (the `Mantles` entry whose `LedgeHeight` is nearest the mantle's ledge height; importer: `mantle_32/64/96/128` at 60/110/160/210 cm) played at `MantlePlayRate` 1.5. Wall bounce → **WallJump** layer (`WallJump`, else `AirJumpStart`, else `JumpStart`; only Lash and Viscous have `wall_jump`), which runs into the in-air loops and stays until landing or the next jump. Lunge end → **HeavyMeleeHit**. Mantle and HeavyMeleeHit hold until the clip ends. Jump, dash, stun, lunge, slide or zipline end them at once. Moving on the ground ends them once `MantleMoveExitFraction` 0.6 of the clip has played (mantle) or after `HeavyMeleeHitMoveExitTime` 0.3 s (hit).
- Air: new jump restarts with `JumpStart`/`AirJumpStart` (0.08 s); ledge walk-off has no clip (0.2 s).
- Ground/Crouch: Land layer (0.06 s, `HardLand` above `HardLandSpeed` 1400) only when landing nearly stopped; Land exits after min(clip, `LandHoldTime` 0.35) or 0.1 s if moving.
- Slide: within 0.25 s of being airborne → start `SlideStart` at `SlideFromAirStartTime` with 0.08 s blend (skips the clip's hop). From a run → from 0 with 0.2 s.
- Then: layer clocks, weights, `UpdateLocomotion` (MoveDir angle eased at 12/s, shortest way round; easing the vector and renormalizing got stuck on 180° reversals; MoveAlpha toward speed/150; shared stride `Phase` with play rate clamp 0.65–1.6), aim weight/pitch, slot weight bookkeeping (UpperBody, DefaultSlot).

**Evaluate**: each layer → blend; `ApplyAimOffset` (Up/Down minus Center as additive, × |pitch/80°| × AimWeight, from `spine_0` up) → `ApplySlots` (UpperBody per-bone from `spine_1`, then DefaultSlot full body) → `StripRootMotion` (subtract `root_motion` offset from it and its siblings `pelvis`/IK targets). Clips are sampled with `GetAnimationPose` directly; no sequence players.

## Hooking up a hero
**The importer does steps 1-3 and the hero select entry** (`setup_hero` in `ue_import.py`, on a full import or `--hero-only`; config `hero_setup` in `config.json`). It never overwrites an existing BP or data asset:
- `BP_Deadlock_<Hero>` = duplicate of `template_bp` (`BP_Deadlock_Lash`: Mover setup, Cubed/GooBall mode classes, `TurningRate` -1, weapon and **Lash's abilities**, kept on purpose so the hero is playable at once), compiled, then on the CDO: mesh `SK_<Hero>`, anim class `ABP_DeadlockHero`, `DeadlockAnimSet`, `LightMeleeMontage` = `AM_<Hero>_Melee_Quick1`, `HeavyMeleeMontage` = `AM_<Hero>_Melee_Hit`.
- `DA_<Hero>_Hero` (`UZL_Character_Data_Asset`) = duplicate of `template_data` (`DA_Lash_Hero`: icon, icon colour and preview framing still Lash's, change them) with name = manifest display name, empty description, class, mesh, pose = first of `display_anims` (`ui_hero_select`, `ui_pose`, `hero_pose`, idles) at pose time 0.
- A `<Hero>` row → `DA_<Hero>_Hero` in `DT_HeroList` (export to JSON, append, `fill_data_table_from_json_string`), skipped if the row exists. This puts Valve assets in a shipping table, by the user's choice (2026-10-25), like the five heroes already listed.

By hand:
1. Duplicate `BP_Deadlock_Lash` (or `BP_Mover_Monkey`) → `BP_Deadlock_<Hero>`.
2. Mesh: `SK_<Hero>`, Anim Class `ABP_DeadlockHero`, scale ≈0.7.
3. Class Defaults: Deadlock Anim Set = `DA_<Hero>_AnimSet`. Also fix or clear **Light/Heavy Melee Montage** (Animation/melee): a duplicate keeps the source hero's, and they win over the anim set's `LightMeleeMontage` (BP_Deadlock_Pocket played Lash's light melee until 2026-10-19).
4. Abilities play `AM_<Hero>_<Ability>_*` (DefaultSlot). Switch a montage to `UpperBody` to keep legs running.
- ABP Root Motion Mode is "Root Motion from Montages Only"; the node strips root motion in the pose anyway.
- Debug: `ShowDebug Animation` prints set, speed, MoveAlpha, aim, layer weights. Animation Insights shows state + set.

## Clip data facts (measured, Lash/Pocket)
- `weapon_run_*` are in place; `primary_walk/sprint_*` and crouch runs carry clean root motion on `root_motion` (332 / 1246 / 340 cm/s).
- `pelvis` is a sibling of `root_motion`, not a child; both move together.
- `slide_start` opens with a ~0.3 s hop (feet 80–115 cm up) before dropping into the slide.
- No start / pivot / plant clips exist. **Motion matching was evaluated and rejected (2026-10-06)**: data doesn't support it. Improve smoothness with inertialization, stride/orientation warping, distance-matched stops, foot IK instead.

## Importer
`Tools/DeadlockImporter/` (see its README). Export with `deadlock_import.bat <hero>` (Source2Viewer-CLI → Blender headless → FBX), import with `py ".../ue_import.py" <Hero>` in the editor console. Builds `SK_`, materials, `A_` clips, `DA_<Hero>_AnimSet` (alias tables in `ANIM_SET_SLOTS`) and ability montages under `<Hero>/Montages/<AbilityName>/`.
Output goes to `/Game/Developers/rpm86/Deadlock/<Hero>/`. **Valve assets: prototyping only, never ship or move out of Developers.**
