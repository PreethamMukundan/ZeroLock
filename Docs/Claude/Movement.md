# Movement: Mover stack, input, networking

Paths under `Source/ZeroLock/`. Heroes are **Mover pawns, not ACharacters** (no CharacterMovementComponent).

## Stack
`AZeroLockCharacter` → `AZeroMoverPawn` (APawn + `IMoverInputProducerInterface`) → `UZeroMoverComponent` (`UCharacterMoverComponent`).
- Modes and transitions are configured in **Blueprint** (`Content/ZERO/monkeyBoi/BP_Mover_Monkey` MoverComponent; also `BP_ZeromoverPawn`, `BP_Deadlock_*`): modes `walking`, `falling`, `flying`, `sliding` (`UZeroSlideMode`), `melee` (`UZeroMeleeMode`), `ziplining` (`UZeroZiplineMode`), `locked` and `floating` (`UZeroFloatMode`); **component-level** transitions (checked every tick in every mode) `UZeroSlideTransition`, `UZeroMeleeTransition`, `UZeroZiplineTransition`. `StartingMovementMode` Falling.
- Per frame: Enhanced Input caches state on the pawn → `ProduceInput_Implementation` (fixed tick) writes `FCharacterDefaultInputs` + `FZeroMovementInputs` → `UZeroMoverComponent::OnMoverPreSimulationTick` applies layered moves / effects / mode queues → active mode simulates.
- Network prediction (`Config/DefaultNetworkPrediction.ini`): Fixed tick 60 Hz, simulated proxies Interpolated (100 ms buffer), `FixedTickInputSendCount=6`. Pawn `bReplicates`, `SetReplicatingMovement(false)`.

## AZeroMoverPawn (`Mover/ZeroMoverPawn`)
- Capsule 34/88, mesh at (0,0,−88) yaw −90, SpringArm 400 (pawn control rotation).
- Input assets (BP): `DefaultMappingContext`, `MoveAction`, `LookAction`, `JumpAction`, `CrouchAction`, `DashAction`, `MeleeAction`, `ZiplineAction`. Melee and Zipline are bound on **Triggered** (every frame while held).
- `OnJumpInputPressed` native multicast + `ConsumeJumpPress()` lets abilities swallow a jump (Pocket Barrage, EnchantersSatchel).
- `DeadlockAnimSet` (Animation.md).
- `GetSyncedAimRotation`, `GetInputWorldDir`: **buggy**, see issues.

## UZeroMoverComponent (`Mover/ZeroMoverComponent`)
`OnMoverPreSimulationTick` order: Rooted (+teleport) → mantle (jump held; **returns on success**) → crouch → air jump / wall bounce → dash → ability linear move → dynamic move-to-actor → release → stop → pull → teleport.

| Feature | Where / how | Tuning (C++ defaults) |
|---|---|---|
| Jump / multi-jump | `HandleAirJumpTracking`: `FLayeredMove_MultiJump` on `bCustomJumpJustPressed`, OverrideVelocity; resets in Walking/Sliding; uncrouches | `MaxAirJumps` 2 (ground jump counts), `VerticalJumpForce` 400 (×1.5 when Falling) |
| Wall bounce | `HandleWallBounceCheck`: Falling + jump, overlap `radius+35`, wall `|N.z|<0.3`, 150 ms LinearVelocity | `WallJumpOffForce` 800, `WallJumpVerticalForce` 600 |
| Dash | `HandleDashInputs`: dir = move input / flat look / forward; LinearVelocity Override; `DashCount++`, `LastDashDirection`, directional montage (not during resim) | `DashSpeed` 1500, `DashDuration` 0.2. **No cooldown in C++** |
| Crouch | `HandleCrouching`, Walking only | `CrouchHalfHeight` 50 (stand 88) |
| Mantle | `TryMantle`: 6 stepped traces, steepness/alignment checks, `FLayeredMove_MoveTo` 0.1–0.25 s | `MantleReachHeight` 50, `MantleMaxDistance` 100, angles 75/45/45 |
| Slide | `UZeroSlideTransition` (ground + crouch + speed ≥ `MinSpeedToSlide` 300) → `UZeroSlideMode` | boost 500, gravity 1200, steering 2500, friction 0.5 (×500 = 250 cm/s²), min speed 150 |
| Melee lunge | `UZeroMeleeTransition` (`bWantsToMelee`, any mode) → `UZeroMeleeMode`: flat lunge along look yaw, capsule sweep on Hero channel, hit → Falling, timeout → Walking. `MeleeHitDelegate` BP hook. | `LungeSpeed` 1500, `MaxDuration` 0.25 |
| Zipline | `UZeroZiplineTransition` (sweep r50 to 500 from actor on Vehicle channel, hit `AZero_ZiplineActor`) → `UZeroZiplineMode` (spline follow; exit at end, jump or crouch) | `ZiplineSpeed` 1200 |
| Rooted | ASC tag `ZeroLock.Movement.Rooted` → mode `RootedModeName` "Locked" + zero velocity; only teleport allowed | |
| Locked / Float | `UZeroFloatMode` (UFlyingMode): no input, decelerate; `bPreserveMomentum` + steering for Pocket float | `SteerAcceleration` 800, `MaxSteerSpeed` 300 |
| Teleport | `RequestSafeTeleport` → `FTeleportEffect` | `MaxTeleportDistance` 10000 |

**"Safe latch" API** (how gameplay code moves a pawn): `RequestSafeAbilityMove(Velocity, Duration)`, `RequestSafeDynamicAbilityMove(Actor, Duration)`, `RequestSafePullTo`, `RequestSafeStop` (100 ms zero), `RequestSafeRelease` (Locked → Falling), `RequestSafeTeleport`. Each sets a latch that the next `ProduceInput` copies into `FZeroMovementInputs`, so it is predicted. `ShouldIgnoreServerLatch()` makes the **server ignore these for remote-client-controlled pawns**: the owning client must make the same request.
Abilities also call `QueueNextMode("Locked"/"Falling"/FloatingModeName)` directly (Apollo ItaniLoSahn/Riposte, Lash DeathSlam, Pocket Barrage/EnchantersSatchel, `ZL_WaitChargeRelease_Task`). That isn't part of the predicted input stream.

## Data structs (`Mover/ZeroMovementData.h`)
- `FZeroMovementInputs` (custom NetSerialize): `LookDir`, `bWantsToCrouch`, `bCustomJumpJustPressed`, `bJumpHold`, `bWantsToZipline`, `bWantsToDash`, `bWantsToMelee`, ability move (velocity, duration), dynamic move (actor, duration), pull (target, duration), `bWantsStop`, `bWantsRelease`, teleport (target). `MoveInput` and `bSlideIntentValid` are never used.
- Sync state: `FZeroMeleeSyncState{ElapsedTimeMs}`, `FZeroZipliningState{ZiplineActor, bMovingToEnd}`. `FZeroSlideState`, `FZeroDashState` unused.

## Dead / legacy
- Modes `UZeroAirJumpMode`, `UZeroDashMode`, `UZeroWallJumpMode` (Settings always null) and transitions `UZeroAirJumpTransition`, `UZeroDashTransition`, `UZeroWallJumpTransition`, `UZeroJumpTransition`. `UZeroMovementSettings`.
- `UZeroBaseCharacterMovementComp` (CMC path, ~790 lines): unused at runtime.
- `APredictedProjectile` / `ARealProjectile` (`network/`): only used by `UZL_BaseProjectileThrowAbility`.
- `AZeroLockGameMode`: template leftover.

## Core framework
- `AZeroLockCharacter` (`ZeroLockCharacter.h/.cpp`): GAS owner (GAS.md), inputs, death/respawn, `bUseControllerRotationYaw=true`, Stun/Parry call `DisableInput`.
- `AZero_BasePlayerController`: hero selection RPCs, `ShowDamageNumber` client RPC, projectile prediction helpers (Weapons.md), `OnPSInit`.
- `AZero_BasePlayerState`: replicated Kills/Assists/Deaths/TeamID/PlayerImage; UI VM refs.
- `AZeroPlayerCameraManager`: crouch camera offset blend (hard-coded 45).
- `UKeybindManagerSubsystem` (`Input/`, LocalPlayer): Enhanced Input user settings remap (UI.md).
- `AZero_ZiplineActor` (`Movement/`): spline; collision must respond on the **Vehicle** object type.
- Channel `ECC_GameTraceChannel1` = "Hero" (`DefaultEngine.ini:122`); pawn BPs must use the "Hero" profile for melee/weapon hits. `ECC_Vehicle` is reused as a line-of-sight / zipline channel.

## Known issues
- Slide: landing buffer unreachable (`ZeroSlideTransition.cpp:22`, `IsOnGround()` false in Falling; landing goes Falling → Walking → Sliding and needs ≥300 cm/s). Slide mode has no gameplay tags (`IsOnGround()` false while sliding), no floor/ledge check (keeps sliding in mid-air, air jumps keep resetting), no max duration; `SlideJumpImpulse` unused; header defaults disagree with ctor.
- `GetSyncedAimRotation` reads input from the sync state → always actor rotation (projectile aim has no pitch where used: `Zero_BaseWeaponAbility.cpp:36`, `ZL_Apollo_FlawlessAdvance.cpp:174`). Use `GetLastInputCmd()`.
- `GetInputWorldDir`: server returns zero for remote clients; local path includes pitch (`ZeroMoverPawn.cpp:283-299`).
- Melee/zipline sync state not reset on re-entry (second lunge ends after one tick; zipline reuses the old actor). Zipline transition and mode trace differently (mode radius 1000 to 10000); `ZeroInputs` null deref (`ZeroZiplineMode.cpp:142`); debug sphere in mode.
- Mantle success returns early and drops one-shot latches already cleared in `ProduceInput`.
- Prediction hazards: `LocalAirJumpsUsed` is a component member mutated in sim (not rolled back); `IsRooted()` reads ASC tags in sim; many reads of component/world state inside sim instead of `StartState`; direct `QueueNextMode` from abilities.
- Trust: latched moves aren't validated (except teleport distance). Server-only `RequestSafe*` on remote victims is dropped (`ZL_Lash_GroundStrike.cpp:170-177`, which also passes a displacement as velocity).
- Stun/Parry `DisableInput` can leave cached input stuck. `CurrentSpeed` attribute doesn't affect Mover. Assist map never inserts (`ZeroLockCharacter.cpp:426-430`). `LastHitCharacter`/`AssistListCharacters` marked Replicated but not registered.
- `FZeroMovementInputs` is large and sent 6× per input. Game and editor targets disagree on build settings / include order.
