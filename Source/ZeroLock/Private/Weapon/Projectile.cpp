// Copyright Preetham Mukundan (C) 2026


#include "Weapon/Projectile.h"
#include "Weapon//Projectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "DrawDebugHelpers.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Zero_BasePlayerController.h"
#include "Components/MeshComponent.h"
#include "Engine/Engine.h"
#include "Sound/SoundBase.h"
#include "ZeroLock/ZeroLock.h"

AProjectile::AProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Root component performs collision detection against the environment.
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComp->SetCollisionProfileName("Projectile");
	CollisionComp->CanCharacterStepUpOn = ECB_No;
	CollisionComp->bTraceComplexOnMove = true;
	CollisionComp->bReceivesDecals = false;
	SetRootComponent(CollisionComp);

	// Hitbox component performs collision detection against targets (pawns and damageable actors).
	HitboxComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("HitboxComponent"));
	HitboxComp->SetCollisionProfileName("MeshHitDetection"); // Some projectiles may prefer CapsuleHitDetection.
	HitboxComp->CanCharacterStepUpOn = ECB_No;
	HitboxComp->OnComponentBeginOverlap.AddDynamic(this, &AProjectile::OnHitboxOverlapBegin);
	HitboxComp->bTraceComplexOnMove = true;
	HitboxComp->SetUseCCD(true);
	HitboxComp->bReceivesDecals = false;
	HitboxComp->SetupAttachment(RootComponent);
	HitboxComp->SetShouldUpdatePhysicsVolume(false);

	// Set up our projectile movement.
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionComp);
	ProjectileMovement->bForceSubStepping = true; // Mitigates issues when fast-forwarding the projectile, especially when detonating on spawn.
	ProjectileMovement->MaxSimulationTimeStep = 0.0166;
	ProjectileMovement->MaxSimulationIterations = 16;
	ProjectileMovement->bInterpolationUseScopedMovement = false;
	ProjectileMovement->OnProjectileBounce.AddDynamic(this, &AProjectile::OnBounce);
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &AProjectile::OnStop);

#if WITH_EDITORONLY_DATA
	ArrowComp = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("ArrowComponent"));
	if (ArrowComp)
	{
		ArrowComp->ArrowColor = FColor(150, 200, 255);
		ArrowComp->bTreatAsASprite = true;
		ArrowComp->SetupAttachment(CollisionComp);
		ArrowComp->bIsScreenSizeScaled = true;
		ArrowComp->SetSimulatePhysics(false);
	}
#endif // WITH_EDITORONLY_DATA

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	InitialLifeSpan = 5.0f;

	// Networking.
	bReplicates = true;
	bReplicateProjectileMovement = false;
	NetPriority = 2.0f;
	SetMinNetUpdateFrequency(100.0f);

	bHasSpawnedFully = false;
	ProjectileId = NULL_PROJECTILE_ID;
	bIsFakeProjectile = false;
	LinkedFakeProjectile = nullptr;
	LinkedAuthProjectile = nullptr;
	bCorrectFakeProjectilePositionOverTime = false;
	InitialProjectileError = 0.0f;
	bLimitBounces = false;
	MaximumBounces = 1;
	bStraightenProjectileOnBounce = true;
	bDetonated = false;
	bInOverlap = false;
	bFinishedResim = false;
	ImpactGameplayEffect = nullptr;
	bUseFilter = true;
	ImpactEffectDirection = EEffectDirection::InProjectileDirection;
	AreaRadius = 0.0f;
	AreaOffset = FVector(0.0f);
	AreaGameplayEffect = nullptr;
	bSkipAreaEffectForImpactTarget = false;
	LifeSpanAfterDetonation = 0.0f;
	bDetonateOnLifeSpanEnd = false;
	bPredictFX = true;
	BounceVelocityFXThreshold = 100.0f;
	bTriggeredFX = false;
	ZeroPlayerController = nullptr;
	bHasBounced = false;
	BounceCount = 0;
	TearOffDelay = 0.1f;
	MinLifetime = 0.05f;
}

void AProjectile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	SpawnTransform = Transform;
}

void AProjectile::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	/* Cache our player controller in case our instigator dies during our projectile's lifetime. Replicate it because
	 * the server might need it before BeginPlay (if we detonate on spawn), but the clients won't have it yet. */
	if (GetInstigator())
	{
		ZeroPlayerController = GetInstigatorController<AZero_BasePlayerController>();
		
	}

	// Cache the owner's aim rotation at the time of spawning this projectile, if desired.
	if (ProjectileMovement->bShouldBounce && bStraightenProjectileOnBounce)
	{
		if (ZeroPlayerController.IsValid())
		{
			InitialOwnerAimRotation = ZeroPlayerController->GetControlRotation();
			InitialOwnerAimRotation.Normalize();
		}
		else if (GetInstigator())
		{
			// Disable correction.
			bHasBounced = true;
		}
	}

	// Set up rendering for this projectile's meshes.
	TArray<UMeshComponent*> MeshComponents;
	GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* MeshComp : MeshComponents)
	{
		MeshComp->bUseAsOccluder = false;
		MeshComp->SetCastShadow(false);
	}
}

void AProjectile::BeginPlay()
{
	Super::BeginPlay();

	/* Predicted projectiles must have an instigating player controller. We need it so we can retrieve their ping for
	 * prediction logic. We don't need a PC for non-predicted projectiles (or projectiles spawned on the server without
	 * an instigator, like environmental hazards), since the prediction logic will be skipped. */
	if ((!HasAuthority() && ProjectileId != NULL_PROJECTILE_ID) || bIsFakeProjectile)
	{
		if (!ensureAlwaysMsgf(GetInstigator() && ZeroPlayerController.IsValid(), TEXT("Instigating player controller could not be found for predicted projectile projectile (%s). Failed to find player controller with instigator (%s). Predicted projectiles must be spawned with an instigator with a valid player controller."), *GetName(), *GetNameSafe(GetOwner())))
		{
			Destroy();
			return;
		}
	}

	/** If this projectile has a valid instigator, we assume it was spawned by a player or AI, and thus want to apply
	 * gameplay effects using that instigator's ASC. To retrieve that ASC, we also need a valid owner. */
	if (GetInstigator())
	{
		if (!ensureAlwaysMsgf(IsValid(Owner), TEXT("Projectile spawned by (%s) does not have a valid owner. Projectiles spawned by players or AI must have an owner so their ASC can be found to apply gameplay effects. Otherwise, GEs will be applied without an instigator."), *GetOwner()->GetName()))
		{
			Destroy();
			return;
		}
	}


	/* Projectiles can't be spawned directly on clients. A client-authoritative projectile can't perform gameplay logic,
	 * so it would just be cosmetic, which is a waste of the projectile logic overhead. Just create a standalone
	 * projectile class derived from AActor instead. */
	if (!ensureAlwaysMsgf(!(GetNetMode() == NM_Client && HasAuthority() && !bIsFakeProjectile), TEXT("Spawned projectile (%s) directly on client, which is not allowed. Projectiles should be spawned on the server to be replicated to clients, or should be spawned predictively with the \"Spawn Predicted Projectile\" ability task."), *GetName()))
	{
		Destroy();
		return;
	}

	// Initialize the replicated authoritative projectile once it's replicated to the owning client.
	if (!HasAuthority() && (ProjectileId != NULL_PROJECTILE_ID))
	{
		/* Fast-forward the projectile to match where it is on the server to make up for replication time. We only want
		 * to do this if we're going to be correcting the fake projectile's position, because we want to correct it to
		 * where it is on the server. */
		float CatchupTickDelta = (ZeroPlayerController->PlayerState) ? (0.0005f * ZeroPlayerController->PlayerState->ExactPing) : 0.0f;
		if (CatchupTickDelta > 0.0f && bCorrectFakeProjectilePositionOverTime)
		{
			CatchupTick(CatchupTickDelta);
		}

	
		// Link to the associated fake projectile.
		if (ensureAlwaysMsgf(ZeroPlayerController->FakeProjectiles.Contains(ProjectileId), TEXT("Client-side authoritative projectile (%s) failed to find corresponding fake projectile with ID (%i)."), *GetNameSafe(this), ProjectileId))
		{
			LinkFakeProjectile(ZeroPlayerController->FakeProjectiles[ProjectileId]);
			ZeroPlayerController->FakeProjectiles.Remove(ProjectileId);
		}
	}

	/* On simulated proxies without a fake projectile (i.e. non-owning simulated proxies), projectiles will appear a
	 * noticeable distance ahead of where they spawned (or sometimes not at all) due to latency and forward prediction.
	 * To prevent this, we rewind the projectiles back to their spawn position and resimulate them to show their full
	 * lifetime. We also do this for projectiles (on all machines) that aren't spawned predictively. */
	if ((GetLocalRole() == ROLE_SimulatedProxy) && (ProjectileId == NULL_PROJECTILE_ID))
	{
		/** When projectiles detonate on spawn, we rewind and resimulate them using FinishedResimulationTimer, meaning
		 * we'll have already done this and don't need to do it again.
		 * @see OnRep_DetonationInfo */
		if (!bFinishedResim && !GetWorldTimerManager().IsTimerActive(FinishedResimulationTimer))
		{
			SetActorTransform(SpawnTransform);
		}
	}

	bHasSpawnedFully = true;
}

void AProjectile::CatchupTick(float CatchupTickDelta)
{
	if (ProjectileMovement)
	{
		ProjectileMovement->TickComponent(CatchupTickDelta, LEVELTICK_All, nullptr);
	}
}

void AProjectile::SetLifeSpan(float InLifespan)
{
	InitialLifeSpan = InLifespan;
	if (InLifespan > 0.0f)
	{
		GetWorldTimerManager().SetTimer(TimerHandle_LifeSpanExpired, this, &AActor::LifeSpanExpired, InLifespan);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(TimerHandle_LifeSpanExpired);
	}
}

void AProjectile::TornOff()
{
	/* The projectile is torn off when it detonates on the server, which forces detonation on clients if it hasn't
	 * already. */
	if (!bDetonated)
	{
		if (DetonationInfo.bDetonated)
		{
			// Server tore off a non-owning simulated proxy before it finished its local resimulation.
			if (ProjectileId == NULL_PROJECTILE_ID)
			{
				// BUG: Figure out why this is being called on every simulated proxy every time. Unless we detonated on
				// spawn, aren't we SUPPOSED to tear off before resimulation and wait for the resim timer before
				// detonating? It doesn't seem to be causing any problems regardless, probably because projectiles that
				// haven't finished resim have their detonation rejected in Detonate.
				// PROJECTILE_LOG(Warning, TEXT("Projectile (%s) on non-owning simulated proxy was torn off before it finished its resimulation. Is MinLifetime less than TearOffDelay? Detonating early with replicated detonation information..."), *GetName());

				DetonateWithDetonationInfo();
			}
			/*
			 * Predicting client's version of the authoritative projectile hasn't detonated yet. This happens when the
			 * projectile detonates on spawn, but the detonation is rejected because the projectile hasn't been fully
			 * initialized yet (@see AProjectile::Detonate), in which case, we try to detonate again once being torn
			 * off.
			 *
			 * This can also theoretically happen if the client's version of the authoritative projectile somehow misses
			 * whatever it hit on the server, though this is extremely rare.
			 */
			else
			{
				DetonateWithDetonationInfo();
			}
		}
		// Server tore off a non-owning simulated proxy without replicating the detonation.
		else if (ProjectileId == NULL_PROJECTILE_ID)
		{
			checkf(1, TEXT("Projectile (%s) on non-owning simulated proxy was torn off, but it never received any detonation information, meaning the server tore off the projectile without detonating it."), *GetName());
		}
	}
}

void AProjectile::LifeSpanExpired()
{
	/* If we want to detonate this projectile when its lifespan ends, and it hasn't detonated yet, force its detonation.
	 * This will eventually lead to this actor's destruction when ShutDown updates LifeSpan (to ensure enough time for
	 * replication), which will call this function again, leading to its other branch since bDetonated will now be
	 * true. */
	if (bDetonateOnLifeSpanEnd && !bDetonated)
	{
		const FHitResult Hit = FHitResult(nullptr, nullptr, CollisionComp->GetComponentLocation(), CollisionComp->GetComponentRotation().Vector());
		OnStop(Hit);
	}
	// Destroy this projectile.
	else
	{
		Super::LifeSpanExpired();
	}
}

void AProjectile::ShutDown()
{

	/* If we want to keep this projectile visible after its detonation (i.e. for post-detonation FX, like a landing
	 * animation), make sure it's not hidden. E.g. if we're not predicting FX, we need to unhide the authoritative
	 * projectile when it detonates, since authoritative projectiles are hidden by default on the owning client. */
	if (bTriggeredFX && (LifeSpanAfterDetonation > 0.0f))
	{
		SetActorHiddenInGame(false);
		TArray<USceneComponent*> Components;
		GetComponents<USceneComponent>(Components);
		for (int32 i = 0; i < Components.Num(); i++)
		{
			Components[i]->SetVisibility(true);
		}
	}

	if (!IsPendingKillPending())
	{
		// Disable movement & collision.
		SetActorEnableCollision(false);
		ProjectileMovement->SetActive(false);

		// Make sure we keep the projectile alive long enough to finish replicating.
		float NewLifeSpan = FMath::Max(LifeSpanAfterDetonation, 0.4f);

		// If we want to kill this projectile before it can be safely destroyed, disable and hide it until it can be.
		if (LifeSpanAfterDetonation == 0.0f)
		{
			DisableAndHide();
		}
		/* If the fake projectile isn't predicting FX, it should always be disabled and hidden after detonation. We'll
		 * be switching to the authoritative projectile for FX and post-detonation effects instead. We still want to
		 * keep the fake projectile alive for a bit, so the authoritative projectile has opportunity to check for missed
		 * predictions. */
		else if (bIsFakeProjectile && !bPredictFX)
		{
			DisableAndHide();
		}
		else if (LifeSpanAfterDetonation < NewLifeSpan)
		{
			GetWorldTimerManager().SetTimer(ShutDownTimer, FTimerDelegate::CreateUObject(this, &AProjectile::DisableAndHide), LifeSpanAfterDetonation, false);
		}

		SetLifeSpan(NewLifeSpan);

		K2_ShutDown();
	}
}

void AProjectile::Destroyed()
{
	// Redundancy for safety. Should never happen.
	if (LinkedFakeProjectile)
	{
		LinkedFakeProjectile->Destroy();
	}

	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::Destroyed();
}

void AProjectile::DisableAndHide()
{
	ForEachComponent(false, [](UActorComponent* InComponent)
	{
		// Stop VFX.
		if (UNiagaraComponent* NiagaraComp = Cast<UNiagaraComponent>(InComponent))
		{
			// Let VFX tick one last time before stopping.
			NiagaraComp->SetForceSolo(true);
			NiagaraComp->TickComponent(0.0f, LEVELTICK_All, nullptr);
			NiagaraComp->Deactivate();
			NiagaraComp->SetAutoDestroy(true);
		}
		// Stop looping audio (i.e. ambient sounds).
		else if (UAudioComponent* AudioComp = Cast<UAudioComponent>(InComponent))
		{
			if (AudioComp->GetSound() != nullptr && !AudioComp->GetSound()->IsOneShot())
			{
				AudioComp->Stop();
			}
		}
		// Hide scene components.
		else if (USceneComponent* SceneComp = Cast<USceneComponent>(InComponent))
		{
			SceneComp->SetHiddenInGame(true);
			SceneComp->SetVisibility(false);
		}
	});
}

void AProjectile::InitFakeProjectile(AZero_BasePlayerController* OwningPlayer, uint32 InProjectileId)
{
	if (InProjectileId == NULL_PROJECTILE_ID)
	{
		return;
	}

	bIsFakeProjectile = true;
	if (OwningPlayer)
	{
		OwningPlayer->FakeProjectiles.Add(InProjectileId, this);
	}
}

void AProjectile::LinkFakeProjectile(AProjectile* InFakeProjectile)
{
	LinkedFakeProjectile = InFakeProjectile;
	InFakeProjectile->LinkedAuthProjectile = this;

	InitialProjectileError = (GetActorLocation() - LinkedFakeProjectile->GetActorLocation()).Size();
	if (((GetActorLocation() - LinkedFakeProjectile->GetActorLocation()) | LinkedFakeProjectile->GetVelocity()) > 0.f)
	{
		InitialProjectileError *= -1.0f;
	}


	SetActorHiddenInGame(true);
	TArray<USceneComponent*> Components;
	GetComponents<USceneComponent>(Components);
	for (int32 i = 0; i < Components.Num(); i++)
	{
		Components[i]->SetVisibility(false);
	}
}

void AProjectile::SwitchToRealProjectile()
{
	// Unhide the authoritative projectile.
	SetActorHiddenInGame(false);
	TArray<USceneComponent*> Components;
	GetComponents<USceneComponent>(Components);
	for (int32 i = 0; i < Components.Num(); i++)
	{
		Components[i]->SetVisibility(true);
	}

	// Immediately destroy the fake projectile; it's no longer valid for prediction.
	if (LinkedFakeProjectile)
	{
		LinkedFakeProjectile->Destroy();
		LinkedFakeProjectile = nullptr;
	}
}

void AProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bCorrectFakeProjectilePositionOverTime)
	{
		CorrectionLerpTick(DeltaSeconds);
	}
}

void AProjectile::CorrectionLerpTick(float DeltaTime)
{
	if (LinkedFakeProjectile && !LinkedFakeProjectile->bDetonated && !LinkedFakeProjectile->IsPendingKillPending() && ProjectileMovement)
	{
		if (DeltaTime > UE_SMALL_NUMBER)
		{
			FVector Current = LinkedFakeProjectile->GetActorLocation();
			FVector Target = GetActorLocation();

			/* We choose to lerp the projectile over a duration of (2000 / its initial speed). E.g. a projectile that
			 * travels at 1000m/s will be lerped over 2.0s. Decrease this duration if the projectile is using physics,
			 * since it has a higher risk of becoming desynced. I don't think this is something designers should worry
			 * about, but we could expose it for configuration if we want to. */
			// TODO: Update this to adjust lerp-rate based on target delta.
			const float TargetLerpTime = (ProjectileMovement->bShouldBounce ? 200.0f : 2000.0f) / ProjectileMovement->InitialSpeed;
			const float StepDistance = (InitialProjectileError / TargetLerpTime);

			FVector NewLoc = FMath::VInterpConstantTo(Current, Target, DeltaTime, StepDistance);

			// We can just hack into RepMovement to safely send the location update.
			FRepMovement RepMovement(LinkedFakeProjectile->GetReplicatedMovement());
			RepMovement.Location = NewLoc;
			RepMovement.Rotation = LinkedFakeProjectile->GetActorRotation();
			LinkedFakeProjectile->SetReplicatedMovement(RepMovement);
			LinkedFakeProjectile->PostNetReceiveLocationAndRotation();

		}
	}
}

void AProjectile::OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	if (FMath::Abs(ImpactVelocity.Dot(ImpactResult.Normal)) > BounceVelocityFXThreshold)
	{
		// Trigger FX on owning client's auth projectile if we aren't predicting them.
		const bool bIsAuthOnOwner = ((!HasAuthority() || GetTearOff()) && (ProjectileId != NULL_PROJECTILE_ID));
		if (!bIsAuthOnOwner || !bPredictFX)
		{
			/* Trigger FX on fake projectile if we ARE predicting them, or if we don't have an auth projectile yet, since
			 * our auth projectile probably won't finish initializing before it was supposed to bounce. */
			if (!bIsFakeProjectile || bPredictFX || !LinkedAuthProjectile)
			{
				
			}
		}
	}

	/* Transform this projectile's velocity to align with its owner's original aim rotation to "straighten it out" after
	 * the first bounce. */
	if (bStraightenProjectileOnBounce && !bHasBounced)
	{
		const FVector CurrentVelocity = ProjectileMovement->Velocity;
		const FVector Normal = ImpactResult.ImpactNormal;
		const FVector ProjectedVelocity = CurrentVelocity - ((CurrentVelocity | Normal) * Normal);
		const FVector InitialAimDir = InitialOwnerAimRotation.Vector();
		const FVector ProjectedAimDir = (InitialAimDir - (InitialAimDir | Normal) * Normal).GetSafeNormal();
		const FQuat AlignmentRotation = FQuat::FindBetweenNormals(ProjectedVelocity.GetSafeNormal(), ProjectedAimDir);
		const FVector TargetVec = AlignmentRotation.RotateVector(CurrentVelocity);
		ProjectileMovement->Velocity = TargetVec;
		bHasBounced = true;
	}

	// Disable bouncing if this projectile has bounced the maximum number of times.
	BounceCount++;
	if (bLimitBounces && BounceCount == MaximumBounces)
	{
		ProjectileMovement->bShouldBounce = false;
	}
}

void AProjectile::OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!IsValid(OtherActor))
	{
		return;
	}

	// Never detonate against the actor that fired this projectile (usually a player character).
	AActor* Owningpawn = Cast<AActor>(GetInstigator());
	if (OtherActor == Owningpawn)
	{
		return;
	}

	if (!bInOverlap)
	{
		// Re-entrance guard.
		TGuardValue<bool> OverlapGuard(bInOverlap, true);

		

		// Check for line of sight.
		const FVector ImpactPoint = (bFromSweep ? FVector(SweepResult.ImpactPoint) : (OtherComp != nullptr ? OtherComp->GetComponentLocation() : OtherActor->GetActorLocation()));
		FCollisionQueryParams Params(FName(TEXT("HitboxOverlapTrace")), true, this);
		Params.AddIgnoredActor(OtherActor);
		if (GetWorld()->LineTraceTestByChannel(ImpactPoint, GetActorLocation(), ECC_Vehicle, Params))
		{
			return;
		}

		// Get a hit result to apply effects (VFX locations, damage direction, etc.).
		FHitResult Hit;

		// Use the sweep if we have one.
		if (bFromSweep)
		{
			Hit = SweepResult;
		}
		else
		{
			// Try to generate a hit by sweeping the hitbox component.
			OtherComp->SweepComponent(Hit, GetActorLocation() - (GetVelocity() * 10.0f), GetActorLocation() + GetVelocity(), HitboxComp->GetComponentQuat(), HitboxComp->GetCollisionShape(), HitboxComp->bTraceComplexOnMove);

			// Try to generate a hit by just tracing against the component. 
			if (Hit.GetActor() != OtherActor)
			{
				OtherComp->LineTraceComponent(Hit, GetActorLocation() - (GetVelocity() * 10.0f), GetActorLocation() + GetVelocity(), FCollisionQueryParams(GetClass()->GetFName(), false, this));
			}

			// If we STILL fail, we'll just construct the hit manually. This is fine, since it's just for visuals.
			if (Hit.GetActor() != OtherActor)
			{
				const FVector Normal = (ImpactPoint - GetActorLocation()).GetSafeNormal();
				Hit = FHitResult(OtherActor, OtherComp, ImpactPoint, Normal);
			}
		}

		// Detonate this projectile against the hit actor.
		Detonate(true, OtherActor, OtherComp, Hit.ImpactPoint, Hit.ImpactNormal);
	}
}

void AProjectile::OnStop(const FHitResult& Hit)
{
	// Projectiles only stop when hitting the environment, so we detonate without a direct hit target.
	Detonate(false, Hit.GetActor(), Hit.GetComponent(), Hit.Location, Hit.Normal);
}

void AProjectile::FellOutOfWorld(const class UDamageType& dmgType)
{
	/* Just to be safe, set a timer that should never be reached if this projectile behaves as expected, to destroy this
	 * actor if it isn't cleaned up properly. */
	if (HasAuthority() || GetLocalRole() == ROLE_None)
	{
		FTimerHandle TimerHandle;
		GetWorldTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateWeakLambda(this, [this, &TimerHandle]()
		{
			GetWorldTimerManager().ClearTimer(TimerHandle);
			Destroy();
		}), InitialLifeSpan, false);
	}

	ShutDown();
}

void AProjectile::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	if (bReplicateProjectileMovement && IsServerProjectile())
	{
		if (RootComponent && RootComponent->GetAttachParent())
		{
			Super::PreReplication(ChangedPropertyTracker);
		}
		else
		{
			GatherCurrentMovement();
		}
	}
}

void AProjectile::GatherCurrentMovement()
{
	if (RootComponent)
	{
		// If we're attached to something, use AttachmentReplication instead, which our super call can handle.
		if (RootComponent->GetAttachParent())
		{
			Super::GatherCurrentMovement();
		}
		else
		{
			ReplicatedProjectileMovement.Location = RootComponent->GetComponentLocation();
			ReplicatedProjectileMovement.Rotation = RootComponent->GetComponentRotation();
			ReplicatedProjectileMovement.LinearVelocity = GetVelocity();

			MARK_PROPERTY_DIRTY_FROM_NAME(AProjectile, ReplicatedProjectileMovement, this);
		}
	}
}

void AProjectile::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	ProjectileMovement->Velocity = NewVelocity;
}

FVector AProjectile::GetVelocity() const
{
	if (RootComponent != nullptr && (RootComponent->IsSimulatingPhysics() || ProjectileMovement == nullptr))
	{
		return GetRootComponent()->GetComponentVelocity();
	}
	else
	{
		return ProjectileMovement->Velocity;
	}
}

void AProjectile::OnRep_ReplicatedMovement()
{
	const FRepMovement& LocalRepMovement = GetReplicatedMovement();

	if (RootComponent)
	{
		/* Sync physics sim to match server. bRepPhysics should always be false for projectiles, so this shouldn't
		 * really be necessary. */
		if (RootComponent->IsSimulatingPhysics() != LocalRepMovement.bRepPhysics)
		{
			SyncReplicatedPhysicsSimulation();
		}

		RootComponent->OnReceiveReplicatedState(LocalRepMovement.Location, LocalRepMovement.Rotation.Quaternion(), LocalRepMovement.LinearVelocity, LocalRepMovement.AngularVelocity);

		if (!RootComponent->GetAttachParent())
		{
			/* Replicate the authoritative projectile's final position to the predicting client to make sure their
			 * fake projectile detonated in the correct position and to make sure their version of the authoritative
			 * projectile detonates in the correct position (for non-predicted FX and to check for missed
			 * predictions). */
			if (GetLocalRole() == ROLE_SimulatedProxy)
			{
				/* Don't replicate movement to non-owning simulated proxies. Due to their local resimulation, they'll
				 * intentionally be slightly behind the authoritative projectile. */
				if (ProjectileId != NULL_PROJECTILE_ID)
				{
					PostNetReceiveVelocity(LocalRepMovement.LinearVelocity);
					PostNetReceiveLocationAndRotation();
				}
			}
		}
	}
}

void AProjectile::OnRep_RepProjectileMovement()
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		FRepMovement NewMovement(GetReplicatedMovement());
		NewMovement.Location = ReplicatedProjectileMovement.Location;
		NewMovement.Rotation = ReplicatedProjectileMovement.Rotation;
		NewMovement.LinearVelocity = ReplicatedProjectileMovement.LinearVelocity;
		NewMovement.AngularVelocity = FVector(0.f);
		NewMovement.bSimulatedPhysicSleep = false;
		NewMovement.bRepPhysics = false;
		SetReplicatedMovement(NewMovement);

		OnRep_ReplicatedMovement();
	}
}

void AProjectile::Detonate(bool bHasDirectImpactTarget, AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	/** Never detonate non-owning simulated proxies until they've finished their local resimulation. */
	if (!IsServerProjectile() && ProjectileId == NULL_PROJECTILE_ID && !bFinishedResim)
	{
		return;
	}

	/* Replicated authoritative projectiles aren't allowed to detonate before they finish initializing (i.e. on spawn),
	 * because we can't check for missed predictions yet. Instead, we wait until we get torn off, then try to detonate
	 * again. */
	if (!bDetonated && (HasAuthority() || bHasSpawnedFully))
	{
	
		/* Don't trigger FX with the owning client's authoritative projectile if the fake projectile is allowed to
		 * predict them, unless the fake projectile had a missed prediction that needs to be corrected. */
		const bool bIsAuthOnOwner = ((!HasAuthority() || GetTearOff()) && (ProjectileId != NULL_PROJECTILE_ID));
		if (!bIsAuthOnOwner || ShouldAuthProjDetonateOnOwner(true))
		{
			// Don't trigger FX with fake projectiles if they don't predict them.
			if (!bIsFakeProjectile || bPredictFX)
			{
				bTriggeredFX = true;

				ZLOG( TEXT("\t\t... FX triggered."));

				const FVector Normal = (bHasDirectImpactTarget ? (GetActorLocation() - OtherActor->GetActorLocation()).GetSafeNormal() : HitNormal);

				// Always spawn detonation FX. Don't spawn decals if we hit a target (e.g. a player character).
				DetonationFX.ExecuteEffects(this, HitLocation, Normal.Rotation(), OtherComp, bHasDirectImpactTarget);

				/* Spawn missed impact FX if we didn't directly hit a target (i.e. detonated against the environment),
				 * unless we didn't actually hit anything (i.e. detonated from our lifespan expiring). */
				if (!bHasDirectImpactTarget && (IsValid(OtherActor) || IsValid(OtherComp)))
				{
					
				}

				/* Forward event to BP. We only do this when FX are triggered to ensure each client calls this exactly
				 * once per projectile (with the exception of missed predictions). */
				OnDetonate(bHasDirectImpactTarget, OtherActor, OtherComp, HitLocation, Normal);
			}

			/* If our authoritative projectile doesn't detonate soon, that means we predicted wrong and hit
			 * something we shouldn't have. If this happens, switch to our authoritative projectile in an effort to
			 * reconcile. */
			if (bIsFakeProjectile)
			{
				/* TearOffDelay is used here because when authoritative projectiles detonate on spawn, their detonation
				 * on clients will be delayed by TearOffDelay to ensure they've been fully initialized and have had
				 * enough time to initially replicate. */
				GetWorldTimerManager().SetTimer(SwitchToAuthTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					if (LinkedAuthProjectile)
					{
						if (!LinkedAuthProjectile->bDetonated)
						{
							LinkedAuthProjectile->SwitchToRealProjectile();
						}
					}
				}), (0.001 * (ZeroPlayerController->PlayerState->ExactPing + 60.0f)) + TearOffDelay, false);
			}
		}

		// Apply gameplay effects on the server.
		if (IsServerProjectile())
		{
			// If there was a direct impact on a target, apply the impact GE to them.
			if (ImpactGameplayEffect && bHasDirectImpactTarget && OtherActor)
			{
				const FVector Normal =  (ImpactEffectDirection == EEffectDirection::InProjectileDirection) ? GetActorRotation().Vector() :
										(ImpactEffectDirection == EEffectDirection::InVelocityDirection) ? ProjectileMovement->Velocity.GetSafeNormal() :
										/* FromProjectilePosition */ (OtherActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
				FHitResult Hit = FHitResult(OtherActor, OtherComp, HitLocation, Normal);
				Hit.bBlockingHit = true;
				Hit.TraceStart = OtherActor->GetActorLocation();
				ApplyEffectToTarget(true, OtherActor, Hit);
			}

			// Apply the AOE effect to nearby actors, excluding the impact target.
			if ((AreaRadius > 0.0f) && AreaGameplayEffect)
			{
				TArray<const AActor*> HitActors;
				TArray<FOverlapResult> OverlapResults;
				FVector Origin = GetAreaOfEffectOrigin();
				FCollisionQueryParams QueryParams;
				QueryParams.AddIgnoredActor(this);
				GetWorld()->OverlapMultiByChannel(
					OverlapResults,
					Origin,
					FQuat(),
					ECollisionChannel::ECC_WorldDynamic,
					FCollisionShape::MakeSphere(AreaRadius),
					QueryParams
				);

				for (const FOverlapResult& Overlap : OverlapResults)
				{
					if (AActor* ItTarget = Overlap.GetActor())
					{
						/* Don't hit the same actor multiple times (possible if we overlap actors with multiple scene
						 * components) */
						if (HitActors.Contains(ItTarget))
						{
							continue;
						}

						// Skip the impact target, if desired.
						if (bSkipAreaEffectForImpactTarget && (ItTarget == OtherActor))
						{
							continue;
						}

						

						// Check line of sight.
						FVector ImpactPoint = Overlap.OverlapObjectHandle.GetLocation();
						FCollisionQueryParams Params(FName(TEXT("AOEOverlapTrace")), true, this);
						Params.AddIgnoredActor(ItTarget);
						if (GetWorld()->LineTraceTestByChannel(ImpactPoint, Origin, ECC_Vehicle, Params))
						{
							continue;
						}

						// Apply the AOE effect. (We use the actor's location for the direction, not the overlap point).
						FHitResult OverlapHit = FHitResult(ItTarget, Overlap.GetComponent(), ImpactPoint, (ItTarget->GetActorLocation() - Origin).GetSafeNormal());
						OverlapHit.bBlockingHit = true;
						OverlapHit.TraceStart = ItTarget == OtherActor ? ItTarget->GetActorLocation() : Origin; // Used as effect origin, for attenuation distance calculation. Use the target's location if it was a direct impact.
						ApplyEffectToTarget(false, ItTarget, OverlapHit);
						HitActors.Add(ItTarget);
					}
				}
			}

			// Send detonation info to simulated proxies to use when they finish their local resimulation.
			DetonationInfo = FDetonationInfo(true, bHasDirectImpactTarget, OtherActor, OtherComp, HitLocation, HitNormal);
			MARK_PROPERTY_DIRTY_FROM_NAME(AProjectile, DetonationInfo, this);

			// Enable movement replication so the detonation is in the correct location for clients.
			bReplicateProjectileMovement = true;

			/** Tear off the server's projectile to trigger TornOff on all clients, which will force them to
			 * detonate if they haven't already. We're basically using this like an RPC that replicates the
			 * detonation event to clients, but waits until after they're replicated and initialized. Delay this if
			 * the projectile hasn't had enough time to initially replicate yet. */
			if (GetGameTimeSinceCreation() > TearOffDelay)
			{
				GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::TearOff));
			}
			else
			{
				GetWorldTimerManager().SetTimer(TearOffTimer, FTimerDelegate::CreateUObject(this, &ThisClass::TearOff), TearOffDelay, false);
			}
		}

		/* If the authoritative projectile triggered FX when it shouldn't have, that means there was a missed prediction
		 * that caused us to switch to the authoritative projectile. If this happens, we need to destroy the fake one
		 * (if it hasn't been destroyed already), since it's no longer valid for predicting. This is usually only 
		 * necessary when projectiles have post-detonation effects (i.e. LifeSpanAfterDetonation > 0). Otherwise, the
		 * fake projectile will likely have already been hidden or destroyed. */
		if (LinkedFakeProjectile && bTriggeredFX && bPredictFX)
		{
			LinkedFakeProjectile->Destroy();
			LinkedFakeProjectile = nullptr;
		}

		bDetonated = true;

		// Clean up and destroy this projectile.
		ShutDown();
		
	}
}

bool AProjectile::ShouldAuthProjDetonateOnOwner(bool bLog) const
{
	// If we aren't predicting FX, the authoritative projectile should trigger FX instead of the fake projectile.
	if (!bPredictFX)
	{
		return true;
	}

	if (LinkedFakeProjectile)
	{
		/*
		 * If the fake projectile hasn't detonated yet, we should destroy it (which is done at the end of Detonate), and
		 * switch to the authoritative projectile. This can occur if the fake projectile missed the correct target, or
		 * if the authoritative projectile was slightly ahead of the fake one, because we forward-predicted a little too
		 * far due to an inaccurate ping estimate.
		 *
		 * We could also handle this by correcting the fake projectile's position and forcing its detonation, but we
		 * choose to use the authoritative projectile instead because it's a cleaner solution; there's no point in using
		 * the fake (i.e. predicted) projectile if it's not actually predicting anything anymore. Plus, there will be a
		 * visual discrepancy either way (the fake projectile disappearing and the real one appearing, or the fake
		 * projectile teleporting).
		 */
		if (!LinkedFakeProjectile->bDetonated)
		{
		
			return true;
		}

		/* If the fake projectile already detonated, but did so inaccurately, we should correct it by replaying FX
		 * in the correct place with the authoritative projectile. */
		if (LinkedFakeProjectile->bDetonated)
		{
			constexpr float MaxFinalPositionDistance = 100.0f; // Centimeters
			if ((LinkedFakeProjectile->GetActorLocation() - GetActorLocation()).Size() > MaxFinalPositionDistance)
			{
				

				return true;
			}
		}
	}

	/*
	 * If we don't have a fake projectile at this point, that means either the fake projectile detonated so early
	 * that it's already been destroyed, or the authoritative projectile detonated so early that we haven't linked
	 * yet.
	 *
	 * For the first case, if we've already been fully initialized and still don't have a linked projectile, that
	 * means it detonated so long ago that it was already destroyed. The fake projectile will have already caught
	 * this (using the SwitchToAuthTimer timer) and switched to using this authoritative projectile on the client.
	 * All we have to do at this point is replay the detonation with the real projectile.
	 * 
	 * The latter case is handled by TornOff. We don't allow replicated projectiles to detonate if they're not fully
	 * spawned. Instead, we wait until they get torn off (which will always occur AFTER BeginPlay), then try to
	 * detonate them again. This second detonation should execute successfully, and will either result in a
	 * successful prediction (the owning client's authoritative projectile, which now has a linked fake projectile,
	 * detonated with sufficient accuracy to that fake projectile; i.e. the branch above) or a missed prediction
	 * that will be caught by one of the above branches.
	 */
	if (!LinkedFakeProjectile)
	{
		if (bHasSpawnedFully)
		{
			

			return true;
		}
	}

	return false;
}

FGameplayEffectSpecHandle AProjectile::MakeEffectSpec_Implementation(bool bDirectImpact, const AActor* Target, const FHitResult& Hit) const
{

	return FGameplayEffectSpecHandle();
}

FVector AProjectile::GetAreaOfEffectOrigin() const
{
	// XY in local space; Z in world space.
	const FVector LocalOffset2D = AreaOffset * FVector(1.0f, 1.0f, 0.0);
	const FVector WorldOffset2D = CollisionComp->GetComponentRotation().RotateVector(LocalOffset2D);
	const FVector FinalOffset = WorldOffset2D + FVector(0.0f, 0.0f, AreaOffset.Z);
	return (CollisionComp->GetComponentLocation() + FinalOffset);
}

void AProjectile::ApplyEffectToTarget(const bool bDirectImpact, const AActor* Target, const FHitResult& Hit) const
{
	
}

void AProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	/* NOTE: ProjectileId, CrashPlayerController, SpawnTransform, and InitialOwnerAimRotation don't need to be marked
	 * dirty; they're set before replication and should never change. */

	constexpr bool bUsePushModel = true;

	FDoRepLifetimeParams OwnerOnlyParams{COND_OwnerOnly, REPNOTIFY_Always, bUsePushModel};
	DOREPLIFETIME_WITH_PARAMS_FAST(AProjectile, ProjectileId, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AProjectile, ZeroPlayerController, OwnerOnlyParams);

	FDoRepLifetimeParams ReplicatedMovementParams{COND_SimulatedOrPhysics, REPNOTIFY_OnChanged, bUsePushModel};
	DOREPLIFETIME_WITH_PARAMS_FAST(AProjectile, ReplicatedProjectileMovement, ReplicatedMovementParams);

	FDoRepLifetimeParams SimulatedOnlyParams{COND_SimulatedOnly, REPNOTIFY_OnChanged, bUsePushModel};
	DOREPLIFETIME_WITH_PARAMS_FAST(AProjectile, SpawnTransform, SimulatedOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AProjectile, DetonationInfo, SimulatedOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AProjectile, InitialOwnerAimRotation, SimulatedOnlyParams);
}

void AProjectile::GetReplicatedCustomConditionState(FCustomPropertyConditionState& OutActiveState) const
{
	Super::GetReplicatedCustomConditionState(OutActiveState);

	DOREPCUSTOMCONDITION_ACTIVE_FAST(AActor, AttachmentReplication, bReplicateProjectileMovement);
}

void AProjectile::OnRep_DetonationInfo()
{
	if (!ensure(DetonationInfo.bDetonated))
	{
		return;
	}

	/* We don't do any kind of resimulation for the owning client because (A) It's important that they have an accurate
	 * version of the authoritative projectile (to check for missed predictions, get accurate hit detection and FX,
	 * etc.) and (B) It's okay if they never see their own projectile. We also don't do any resimulation for the server
	 * because it's also acting as the authoritative simulation of the projectile. */
	if (ProjectileId == NULL_PROJECTILE_ID)
	{
		float RemainingDistance = (DetonationInfo.HitLocation - GetActorLocation()).Length();
		const float CurrentSpeed = (GetVelocity().IsZero() ? ProjectileMovement->InitialSpeed : GetVelocity().Length());
		float RemainingTime = RemainingDistance / CurrentSpeed;

		/* If this projectile detonated on spawn, rewind and resimulate it at a modified velocity to ensure it never
		 * detonates without ever being seen by other players. */
		if (!bHasSpawnedFully)
		{
			// Rewind the projectile now (instead of waiting for BeginPlay).
			SetActorTransform(SpawnTransform);

			// Modify this projectile's velocity to ensure it stays visible for at least MinLifeTime.
			RemainingDistance = (DetonationInfo.HitLocation - SpawnTransform.GetLocation()).Length();
			RemainingTime = FMath::Max((RemainingDistance / CurrentSpeed), MinLifetime);
			const float DesiredSpeed = (RemainingDistance / RemainingTime);
			const float SpeedQuotient = (DesiredSpeed / CurrentSpeed);

			// If we detonated on spawn, we might not have initialized our velocity yet.
			if (GetVelocity().IsZero())
			{
				ProjectileMovement->InitialSpeed *= SpeedQuotient;
			}
			else
			{
				ProjectileMovement->Velocity *= SpeedQuotient;
			}

		}
		/* If this projectile detonated very close to its spawn, slow its velocity to ensure it stays visible long
		 * enough to be seen by players. */
		else if ((GetGameTimeSinceCreation() + RemainingTime) < MinLifetime)
		{
			RemainingTime = (MinLifetime - GetGameTimeSinceCreation());

			const float DesiredSpeed = (RemainingDistance / RemainingTime);
			const float SpeedQuotient = (DesiredSpeed / CurrentSpeed);

			ProjectileMovement->Velocity *= SpeedQuotient;

		}
		else
		{
		}

		// Account for processing time this frame.
		RemainingTime -= GetWorld()->GetDeltaSeconds();

		auto DetonateDelayedFunc = [this]()
		{
			/* Make sure we detonate in the correct final location, since we skip OnRep_ReplicatedMovement on non-owning
			 * simulated proxies. */
			if (RootComponent && !RootComponent->GetAttachParent())
			{
				PostNetReceiveVelocity(GetReplicatedMovement().LinearVelocity);
				PostNetReceiveLocationAndRotation();
			}

			bFinishedResim = true;
			DetonateWithDetonationInfo();
		};

		/* If the time left in our resimulation is so short that it will finish before the next frame, skip the timer
		 * and detonate immediately. */
		if (RemainingTime > GetWorld()->GetDeltaSeconds())
		{
			GetWorldTimerManager().SetTimer(FinishedResimulationTimer, FTimerDelegate::CreateWeakLambda(this, DetonateDelayedFunc), RemainingTime, false);
		}
		else
		{
			GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, DetonateDelayedFunc));
		}
	}
}

void AProjectile::DetonateWithDetonationInfo()
{
	if (!ensure(DetonationInfo.bDetonated))
	{
		return;
	}

	/* We do some data validation because this is usually called shortly after detonation info has been replicated, so
	 * there may be rare situations where the pointers are no longer valid. */
	Detonate
	(
		DetonationInfo.bHasDirectImpactTarget && IsValid(DetonationInfo.OtherActor),
		IsValid(DetonationInfo.OtherActor) ? DetonationInfo.OtherActor : nullptr,
		IsValid(DetonationInfo.OtherComp) ? DetonationInfo.OtherComp : nullptr,
		DetonationInfo.HitLocation,
		DetonationInfo.HitNormal
	);
}

bool AProjectile::IsServerProjectile() const
{
	return (GetWorld()->GetNetMode() != NM_Client);
}
