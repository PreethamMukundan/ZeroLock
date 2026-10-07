# Weapons: predicted projectiles

There is **no weapon actor/component**. Primary fire is a GAS ability (`AZeroLockCharacter::PrimaryFireAbility`) that spawns **predicted projectiles**. No hitscan, no headshots/hit zones, no recoil code, no lag compensation of targets.
Paths below are under `Source/ZeroLock/`.

## Flow of one shot
1. `AZeroLockCharacter::PrimaryFirePressed` (`ZeroLockCharacter.cpp:303`) runs a **local looping timer** at the `FireRate` attribute (seconds) → `PrimaryFireTickFunction` (`:337`) → `TryActivateAbilityByClass(PrimaryFireAbility)` while `CurrentAmmo > 0`. `ChangeFireRate` (`:330`) restarts it on attribute change.
2. `UZL_GA_WeaponFire` (`Weapon/ZL_GA_WeaponFire`, LocalPredicted, InstancedPerExecution, blocked by `ZeroLock.Weapon.Reloading`) → `CommitAbility` (ammo cost GE set in BP).
   - `FireMode`: Single / Burst (`BurstCount` 3, `BurstInterval` 0.07, chained `WaitDelay`) / Shotgun (`PelletCount` 8, `SpreadAngle` 6°). `BulletSpreadAngle` (default 0) uses `VRandCone` **on the client**.
   - `GetAimData` (`:140`): spawn at `MuzzleSocketName` or actor + camera-forward × `SpawnForwardOffset` (100). Aim trace from the camera (projected to character depth) against WorldStatic, Pawn, `ECC_GameTraceChannel1` ("Hero"), `MaxAimDistance` 20000; converges unless the hit is behind the muzzle.
   - Remote server starts N spawn tasks with placeholder transforms, each consuming one client packet. Ability ends when all spawns report (`OnSpawnFinished`, `:189`).
3. `UZL_Task_SpawnPredictedProjectile` (`Weapon/ZL_Task_SpawnPredictedProjectile`):
   - **Client**: optionally delays the fake spawn by `PC->GetProjectileSleepTime()`; spawns the fake `ProjectileClass`, `InitProjectileId`, adds to `PC->FakeProjectiles`, sends `FGameplayAbilityTargetData_ProjectileSpawnInfo` (location, rotation, id) via `CallServerSetReplicatedTargetData` in a scoped prediction window.
   - **Server** (`OnSpawnDataReplicated`, `:241`): `TryConsumeClientReplicatedTargetData` (custom ASC helper: one packet per task) → spawn `ServerProjectileClass` **at the client transform** → fast-forward by `GetForwardPredictionTime()` → lifespan reduced (min 0.2).
   - Listen server / standalone local player: spawns the server class directly.
   - `OnTaskRejected`: destroys the fake.
4. `AProjectile` (`Weapon/Projectile`, abstract, sreitich-style projectile prediction):
   - Components: `CollisionComp` (root sphere, profile "Projectile", environment), `HitboxComp` (capsule, "MeshHitDetection", overlap + CCD, finds targets), `TrailComp` (Cascade), `ProjectileMovement` (substepped 0.0166, 16 iterations).
   - Owner client: authoritative copy links to its fake (`LinkFakeProjectile`), stays hidden; switches to auth after ping + 60 ms + `TearOffDelay` if the fake missed.
   - Simulated proxies: rewind to `SpawnTransform`, replay, detonate only when `DetonationInfo` arrives (`OnRep_DetonationInfo`, keeps it visible ≥ `MinLifetime`).
   - Hits: `OnHitboxOverlapBegin` (`:559`) ignores instigator, LOS trace on **`ECC_Vehicle`** (reused as LOS channel), `Detonate(true)`. `OnStop` → `Detonate(false)`.
   - `Detonate` (`:748`) on server: `ApplyEffectToTarget` → AOE overlap (`ECC_WorldDynamic` + LOS) → `DetonationInfo` → `bReplicateProjectileMovement=true` → `TearOff()` (used as "detonated" signal; clients detonate in `TornOff`) → `ShutDown` (kept alive ≥0.4 s, hidden).
   - FX: `HitCharacterParticle` / `HitWorldParticle` (Cascade), `FProjectileFX DetonationFX` (Niagara pooled, sounds, force feedback, decal; `ZeroAbilityTypes`).
5. Damage (server only): `ApplyEffectToTarget` (`:1018`) applies only on **direct hit with `bApplyWeaponDamage`** → `UBaseCharAbilitySystemComponent::ApplyWeaponDamage(TargetASC, WeaponDamage)` (`GAS/BaseCharAbilitySystemComponent.cpp:59`): `GE_WeaponClass` with SetByCaller `Zerolock.DamageCalc.Weapon`, checks `IsUntouchable`, sends `Event.WeaponHit`.
6. Ammo: when `CurrentAmmo <= 0`, `BaseCharAttributeSet.cpp:225-229` calls `TargetChar->Reload()` → reload ability (`UZero_BaseReloadAbility`, `UCalc_Reload` sets CurrentAmmo = MaxAmmo).

## Prediction tuning
`AZero_BasePlayerController` (`Zero_BasePlayerController.cpp:81-100`), values in `Config/DefaultGame.ini:42-46`:
`ForwardPredictionTime = 0.001 × ClientBiasPct(0.5) × clamp(ExactPing − (local ? 0 : PredictionLatencyReduction 20), 0, MaxPredictionPing 120)`.
Projectile ids from `FakeProjectileIdCounter` (starts at 1; `NULL_PROJECTILE_ID` = 0).

## AProjectile defaults (ctor `Projectile.cpp:83-124`)
`InitialLifeSpan` 5, replicates, `bReplicateProjectileMovement` false (true on detonation), `NetPriority` 2, `MinNetUpdateFrequency` 100, `bPredictFX` true, `MaximumBounces` 1, `WeaponDamage` 1, `bApplyWeaponDamage` false, `AreaRadius` 0, `TearOffDelay` 0.1, `MinLifetime` 0.05 (Config=Game, no ini section). Push-model replication: `ProjectileId`/`ZeroPlayerController` OwnerOnly; `SpawnTransform`/`DetonationInfo`/`InitialOwnerAimRotation` SimulatedOnly.
Also the base for hero projectiles (Pocket Barrage/FlyingCloak, Doorman CallBell…).

## Legacy
- `UZero_BaseWeaponAbility` + `AZero_BaseProjectile`: non-predicted, server-only spawn, `DrawDebugLine` every shot (`Zero_BaseWeaponAbility.cpp:64`), only `CommitAbilityCost`, damage hard-coded 1 (`Zero_BaseProjectile.cpp:88`), doesn't replicate by default. Still copied by Xayah's projectile.

## Known issues
- **Trust**: server spawns at the client-supplied transform with no distance/direction check (`ZL_Task_SpawnPredictedProjectile.cpp:259-274`); spread rolled on client; **fire rate is a client timer**, server limited only by `CommitAbility` (cooldown GE, if any, is BP-only).
- No friendly-fire check in `ApplyEffectToTarget` (check the GE execution).
- `ImpactGameplayEffect`, `AreaGameplayEffect`, `bUseFilter`, `MissedImpactFX`, `BounceFX` do nothing; `MakeEffectSpec` returns empty (`:1003`); AOE loop applies nothing (`:1020` returns when not direct).
- `Projectile.cpp:202` dead check (`if (!(!(cond), TEXT(...)))` comma operator); `:191` derefs `GetOwner()` when invalid; `:633-638` lambda captures a stack `FTimerHandle` by reference; `:214`, `:817`, `Zero_BasePlayerController.cpp:91` unchecked PlayerState deref; `:1081`, `:503` possible /0; `checkf(1, …)` at `:304`.
- Fake-position correction is a no-op when the auth projectile is ahead (negative speed into `VInterpConstantTo`, `:445-506`; only with `bCorrectFakeProjectilePositionOverTime`).
- Burst/shotgun server tasks share one target-data delegate; if the client sends fewer packets, the server ability waits on remote data until GAS times out.
- Magic numbers: 0.4 s post-detonation life (`:351`), 100 cm miss threshold (`:963`), +60 ms (`:817`), 0.0005×ping catch-up (`:214`), `ECC_GameTraceChannel1` = Hero (`ZL_GA_WeaponFire.cpp:166`).
