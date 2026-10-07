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
| `Source/ZeroEditorModule/.../AnimGraphNode_ZLDeadlockHero.h/.cpp` | Editor wrapper ("Deadlock Hero", category ZeroLock). Needs AnimGraph, AnimGraphRuntime, BlueprintGraph in `ZeroEditorModule.Build.cs`. |
| `UZeroMoverComponent::DashCount / LastDashDirection` | Bumped in `HandleDashInputs` (skipped while resimulating) so the anim can see dashes, which are layered moves, not a mode. |
| `AZeroMoverPawn::DeadlockAnimSet` | Per-pawn anim set (Class Defaults). |

## Anim instance (`UZL_DeadlockAnimInstance`)
- `ResolveAnimSet()`: instance `AnimSet` override, else pawn `DeadlockAnimSet`. On change it measures:
  - run / crouch authored speed from `root_motion` bone travel of the N clip (`MeasureCycleSpeed`), falling back to `ReferenceRunSpeed` 600 / `ReferenceCrouchSpeed` 300 when the clip is in place;
  - `SlideFromAirStartTime`: the asset value, or (when -1) the lowest pelvis point in the first 60% of `SlideStart` (`MeasureSlideLandTime`).
- `NativeUpdateAnimation`: velocity in actor space, ground/vertical speed, aim pitch (`GetBaseAimRotation`), mode flags by name (`Sliding`, `Dashing`, `Ziplining`, `WallJumping`, `AirJumping`).
- One-frame events are **counters** (`JumpCount`, `LandCount`, `DashCount`) so the worker thread can't miss them:
  - Dash: Mover `DashCount` changed (local/server), or fallback speed burst > `DashSpeed × 0.75` with 0.35 s debounce (simulated proxies).
  - Jump: took off going up (>100), air kick (+250 in one frame), or entered AirJumping/WallJumping.
  - Land: touched ground after >0.2 s airborne; `LastLandingSpeed` = max fall speed.

## Node (`FAnimNode_ZLDeadlockHero`)
**Layer stack** (max 4). A state change pushes a layer that fades in over its blend time; older layers share the remaining weight proportionally and are removed under 0.001. `PushLayer(State, BlendTime, Clip, StartTime)`.

**Update** order: counters → `TimeSinceAirborne` → dash (`ChooseDashClip` by dominant axis; held for `DashHoldTime` 0.45 s or while bursting, jump cancels) → `ChooseState()` priority Zipline > Slide > WallJump > Air > Crouch/Ground → per-state push rules:
- Air: new jump restarts with `JumpStart`/`AirJumpStart` (0.08 s); ledge walk-off has no clip (0.2 s).
- Ground/Crouch: Land layer (0.06 s, `HardLand` above `HardLandSpeed` 1400) only when landing nearly stopped; Land exits after min(clip, `LandHoldTime` 0.35) or 0.1 s if moving.
- Slide: within 0.25 s of being airborne → start `SlideStart` at `SlideFromAirStartTime` with 0.08 s blend (skips the clip's hop). From a run → from 0 with 0.2 s.
- Then: layer clocks, weights, `UpdateLocomotion` (MoveDir eased at 12/s; MoveAlpha toward speed/150; shared stride `Phase` with play rate clamp 0.65–1.6), aim weight/pitch, slot weight bookkeeping (UpperBody, DefaultSlot).

**Evaluate**: each layer → blend; `ApplyAimOffset` (Up/Down minus Center as additive, × |pitch/80°| × AimWeight, from `spine_0` up) → `ApplySlots` (UpperBody per-bone from `spine_1`, then DefaultSlot full body) → `StripRootMotion` (subtract `root_motion` offset from it and its siblings `pelvis`/IK targets). Clips are sampled with `GetAnimationPose` directly; no sequence players.

## Hooking up a hero
1. Duplicate `BP_Deadlock_Lash` (or `BP_Mover_Monkey`) → `BP_Deadlock_<Hero>`.
2. Mesh: `SK_<Hero>`, Anim Class `ABP_DeadlockHero`, scale ≈0.7.
3. Class Defaults: Deadlock Anim Set = `DA_<Hero>_AnimSet`.
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
