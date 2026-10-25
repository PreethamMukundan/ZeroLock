// Copyright Preetham Mukundan (C) 2026

#include "Animation/ZL_DeadlockAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/ZL_DeadlockAnimSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/ZL_GameplayTags.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Mover/ZeroMoverComponent.h"
#include "Mover/ZeroMovementData.h"
#include "Mover/ZeroMoverPawn.h"

void UZL_DeadlockAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwnerPawn = Cast<AZeroMoverPawn>(TryGetPawnOwner());
	Mover = OwnerPawn.IsValid() ? OwnerPawn->FindComponentByClass<UCharacterMoverComponent>() : nullptr;
	if (const UZeroMoverComponent* ZeroMover = Cast<UZeroMoverComponent>(Mover.Get()))
	{
		SeenMoverDashCount = ZeroMover->DashCount;
	}
	ResolveAnimSet();
}

void UZL_DeadlockAnimInstance::SetAnimSet(UZL_DeadlockAnimSet* NewAnimSet)
{
	AnimSet = NewAnimSet;
	ResolveAnimSet();
}

void UZL_DeadlockAnimInstance::ResolveAnimSet()
{
	UZL_DeadlockAnimSet* Resolved = AnimSet;
	if (!Resolved && OwnerPawn.IsValid())
	{
		Resolved = OwnerPawn->DeadlockAnimSet;
	}
	if (Resolved == ActiveAnimSet)
	{
		return;
	}

	ActiveAnimSet = Resolved;
	if (ActiveAnimSet)
	{
		const float RunSpeed = ActiveAnimSet->Run.AuthoredSpeed > 0.f ? ActiveAnimSet->Run.AuthoredSpeed : MeasureCycleSpeed(ActiveAnimSet->Run);
		const float CrouchSpeed = ActiveAnimSet->CrouchRun.AuthoredSpeed > 0.f ? ActiveAnimSet->CrouchRun.AuthoredSpeed : MeasureCycleSpeed(ActiveAnimSet->CrouchRun);
		State.RunAuthoredSpeed = RunSpeed > 1.f ? RunSpeed : ActiveAnimSet->ReferenceRunSpeed;
		State.CrouchAuthoredSpeed = CrouchSpeed > 1.f ? CrouchSpeed : ActiveAnimSet->ReferenceCrouchSpeed;
		State.SlideFromAirStartTime = ActiveAnimSet->SlideFromAirStartTime >= 0.f ? ActiveAnimSet->SlideFromAirStartTime : MeasureSlideLandTime(ActiveAnimSet->SlideStart);
	}
}

float UZL_DeadlockAnimInstance::MeasureSlideLandTime(const UAnimSequence* Seq) const
{
	// Deadlock's slide intro hops up before dropping into the slide. The lowest pelvis point is where the body lands
	// in the slide, which is the frame to start from when the character is already coming down from the air.
	if (!Seq || !Seq->GetSkeleton() || Seq->GetPlayLength() <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	const FReferenceSkeleton& RefSkeleton = Seq->GetSkeleton()->GetReferenceSkeleton();
	const int32 PelvisIndex = RefSkeleton.FindBoneIndex(PelvisBoneName);
	if (PelvisIndex == INDEX_NONE)
	{
		return 0.f;
	}
	const int32 RootMotionIndex = RefSkeleton.FindBoneIndex(RootMotionBoneName);

	auto ComponentHeight = [&](int32 BoneIndex, double Time)
	{
		FTransform Result = FTransform::Identity;
		for (int32 Index = BoneIndex; Index != INDEX_NONE; Index = RefSkeleton.GetParentIndex(Index))
		{
			FTransform Local;
			Seq->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Index), FAnimExtractContext(Time), false);
			Result = Result * Local;
		}
		return Result.GetTranslation().Z;
	};

	// Only the first part of the clip: the drop happens early, the rest is the slide settling into its loop.
	constexpr double SampleStep = 1.0 / 30.0;
	const double SearchEnd = Seq->GetPlayLength() * 0.6;
	double BestTime = 0.0;
	double BestHeight = TNumericLimits<double>::Max();
	for (double Time = 0.0; Time <= SearchEnd; Time += SampleStep)
	{
		double Height = ComponentHeight(PelvisIndex, Time);
		if (RootMotionIndex != INDEX_NONE)
		{
			Height -= ComponentHeight(RootMotionIndex, Time);
		}
		if (Height < BestHeight)
		{
			BestHeight = Height;
			BestTime = Time;
		}
	}
	return static_cast<float>(BestTime);
}

float UZL_DeadlockAnimInstance::MeasureCycleSpeed(const FZLDirectionalAnims& Cycle) const
{
	// Deadlock bakes forward motion into the root_motion bone for some cycles; others are authored in place.
	const UAnimSequence* Seq = Cycle.N ? Cycle.N.Get() : Cycle.GetWithFallback(0);
	if (!Seq || !Seq->GetSkeleton() || Seq->GetPlayLength() <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	const int32 BoneIndex = Seq->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(RootMotionBoneName);
	if (BoneIndex == INDEX_NONE)
	{
		return 0.f;
	}

	FTransform Start, End;
	Seq->GetBoneTransform(Start, FSkeletonPoseBoneIndex(BoneIndex), FAnimExtractContext(0.0), false);
	Seq->GetBoneTransform(End, FSkeletonPoseBoneIndex(BoneIndex), FAnimExtractContext(static_cast<double>(Seq->GetPlayLength())), false);
	return FVector::Dist2D(Start.GetTranslation(), End.GetTranslation()) / Seq->GetPlayLength();
}

void UZL_DeadlockAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerPawn.IsValid())
	{
		OwnerPawn = Cast<AZeroMoverPawn>(TryGetPawnOwner());
		Mover = OwnerPawn.IsValid() ? OwnerPawn->FindComponentByClass<UCharacterMoverComponent>() : nullptr;
	}
	ResolveAnimSet();

	const UCharacterMoverComponent* MoverComp = Mover.Get();
	const AZeroMoverPawn* Pawn = OwnerPawn.Get();
	if (!MoverComp || !Pawn)
	{
		return;
	}

	// Facing as drawn: Mover's smoothing turns the mesh between fixed sim steps, the actor only on them.
	FQuat Facing = Pawn->GetActorQuat();
	const USkeletalMeshComponent* MeshComp = GetSkelMeshComponent();
	if (MeshComp && MoverComp->GetPrimaryVisualComponent() == MeshComp)
	{
		Facing = MeshComp->GetComponentQuat() * MoverComp->GetBaseVisualComponentTransform().GetRotation().Inverse();
	}

	const FVector Velocity = MoverComp->GetVelocity();
	State.LocalVelocity = Facing.UnrotateVector(Velocity);
	State.GroundSpeed = Velocity.Size2D();
	State.VerticalSpeed = Velocity.Z;
	State.AimPitch = FRotator::NormalizeAxis(Pawn->GetBaseAimRotation().Pitch);

	const FName Mode = MoverComp->GetMovementModeName();
	State.MovementMode = Mode;
	State.bOnGround = MoverComp->IsOnGround();
	State.bCrouching = MoverComp->IsCrouching();
	State.bSliding = Mode == SlidingModeName;

	// UZeroMoverComponent dashes with a short (~0.2s) LinearVelocity layered move, not a movement mode.
	// Where the dash is simulated (local player, server) it bumps DashCount, which can't be missed at any
	// frame rate. Simulated proxies don't run that code, so they fall back to spotting the speed burst.
	const UZeroMoverComponent* ZeroMover = Cast<UZeroMoverComponent>(MoverComp);
	TimeSinceDash += DeltaSeconds;
	if (ZeroMover && ZeroMover->DashCount != SeenMoverDashCount)
	{
		SeenMoverDashCount = ZeroMover->DashCount;
		++State.DashCount;
		State.DashDirection = Facing.UnrotateVector(ZeroMover->LastDashDirection).GetSafeNormal2D();
		TimeSinceDash = 0.f;
	}
	const float DashThreshold = (ZeroMover ? ZeroMover->DashSpeed : 1500.f) * DashDetectSpeedRatio;
	const bool bDashBurst = State.GroundSpeed > DashThreshold;
	if (bDashBurst && !bWasDashBurst && TimeSinceDash > 0.35f)
	{
		++State.DashCount;
		State.DashDirection = State.LocalVelocity.GetSafeNormal2D();
		TimeSinceDash = 0.f;
	}
	bWasDashBurst = bDashBurst;
	State.bDashing = bDashBurst || Mode == DashingModeName;
	State.bZiplining = Mode == ZipliningModeName;
	State.bWallJumping = Mode == WallJumpingModeName;
	State.bHeavyMelee = Mode == MeleeModeName;
	if (State.bHeavyMelee && PrevMode != MeleeModeName)
	{
		State.bHeavyMeleeInAir = !bWasOnGround;
	}
	else if (!State.bHeavyMelee && PrevMode == MeleeModeName)
	{
		++State.HeavyMeleeEndCount;
	}

	// Mantles and wall bounces are layered moves; the sim counts them in its sync state (rolls back, replicates).
	TimeSinceClimb += DeltaSeconds;
	if (const FZeroMoverSimState* SimState = MoverComp->GetSyncState().SyncStateCollection.FindDataByType<FZeroMoverSimState>())
	{
		if (!bHasSimCounters)
		{
			SeenSimMantleCount = SimState->MantleCount;
			SeenSimWallJumpCount = SimState->WallJumpCount;
			bHasSimCounters = true;
		}
		if (SimState->MantleCount != SeenSimMantleCount)
		{
			SeenSimMantleCount = SimState->MantleCount;
			++State.MantleCount;
			State.MantleHeight = SimState->MantleHeight;
			TimeSinceClimb = 0.f;
		}
		if (SimState->WallJumpCount != SeenSimWallJumpCount)
		{
			SeenSimWallJumpCount = SimState->WallJumpCount;
			++State.WallJumpCount;
			TimeSinceClimb = 0.f;
		}
	}

	if (!OwnerASC.IsValid())
	{
		OwnerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	}
	State.bStunned = OwnerASC.IsValid() && OwnerASC->HasMatchingGameplayTag(ZerolockGameplayTagsForBinding::TAG_STUN);

	const bool bInAir = !State.bOnGround && !State.bSliding && !State.bZiplining;
	if (bInAir)
	{
		AirTime += DeltaSeconds;
		MaxFallSpeed = FMath::Max(MaxFallSpeed, -State.VerticalSpeed);

		// Jumps: leaving the ground upwards, or a sudden upward kick while airborne (multi-jump). A mantle's or wall
		// bounce's kick isn't one: those have their own counters.
		const bool bTookOff = bWasOnGround && State.VerticalSpeed > 100.f;
		const bool bAirKick = !bWasOnGround && State.VerticalSpeed > 100.f && State.VerticalSpeed - PrevVerticalSpeed > 250.f;
		const bool bEnteredJumpMode = Mode != PrevMode && (Mode == AirJumpingModeName || Mode == WallJumpingModeName);
		if ((bTookOff || bAirKick || bEnteredJumpMode) && TimeSinceClimb > 0.3f)
		{
			++State.JumpCount;
			State.bLastJumpWasAirJump = !bTookOff;
			MaxFallSpeed = 0.f;
		}
	}
	else
	{
		if (!bWasOnGround && State.bOnGround && AirTime > 0.2f)
		{
			++State.LandCount;
			State.LastLandingSpeed = MaxFallSpeed;
		}
		AirTime = 0.f;
		MaxFallSpeed = 0.f;
	}

	bWasOnGround = !bInAir;
	PrevVerticalSpeed = State.VerticalSpeed;
	PrevMode = Mode;
}
