//Copyright Preetham Mukundan (C) 2026

#include "ZeroBaseCharacterMovementComp.h"
#include "ZeroLock/Public/ZeroBaseCharacterMovementComp.h"
#include "ZeroLock/ZeroLock.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "ZeroLock/ZeroLockCharacter.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "ZeroLock/Public/Movement/Zero_ZiplineActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

FVector GServerCharacterLoc[4];
#pragma region SavedMove
UZeroBaseCharacterMovementComp::FSavedMove_Zero::FSavedMove_Zero() {}

bool UZeroBaseCharacterMovementComp::FSavedMove_Zero::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
    FSavedMove_Zero* NewZeroMove = static_cast<FSavedMove_Zero*>(NewMove.Get());

    if(Saved_bWantsToSprint != NewZeroMove->Saved_bWantsToSprint) return false;
    if(Saved_bWantsToDash != NewZeroMove->Saved_bWantsToDash) return false;
    
    return FSavedMove_Character::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void UZeroBaseCharacterMovementComp::FSavedMove_Zero::Clear()
{
    FSavedMove_Character::Clear();
    Saved_bWantsToSprint = 0;
    Saved_bPrevWantsToCrouch = 0;
    Saved_bWantsToDash = 0;
    Saved_bTransitionFinished = 0;
    Saved_bPressedZeroJump = 0;
    Saved_bHadAnimRootMotion = 0;
}

uint8 UZeroBaseCharacterMovementComp::FSavedMove_Zero::GetCompressedFlags() const
{
    uint8 Result = Super::GetCompressedFlags();
    if(Saved_bWantsToSprint) Result |= FLAG_Sprint;
    if(Saved_bWantsToDash) Result |= FLAG_Dash;
    if(Saved_bPressedZeroJump) Result |= FLAG_JumpPressed;
    return Result;
}

void UZeroBaseCharacterMovementComp::FSavedMove_Zero::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData)
{
    FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
    UZeroBaseCharacterMovementComp* CharMovementComp = Cast<UZeroBaseCharacterMovementComp>(C->GetCharacterMovement());
    
    Saved_bWantsToSprint = CharMovementComp->Safe_bWantsToSprint;
    Saved_bPrevWantsToCrouch = CharMovementComp->Safe_bPrevWantsToCrouch;
    Saved_bWantsToDash = CharMovementComp->Safe_bWantsToDash;
    //Saved_bPressedZeroJump = CharMovementComp->ZeroCharacter_Owner->bPressedZeroJump;
    Saved_bHadAnimRootMotion = CharMovementComp->Safe_bHadAnimRootMotion;
    Saved_bTransitionFinished = CharMovementComp->Safe_bTransitionFinished;
}

void UZeroBaseCharacterMovementComp::FSavedMove_Zero::PrepMoveFor(ACharacter* C)
{
    FSavedMove_Character::PrepMoveFor(C);
    UZeroBaseCharacterMovementComp* CharMovementComp = Cast<UZeroBaseCharacterMovementComp>(C->GetCharacterMovement());
    
    CharMovementComp->Safe_bWantsToSprint = Saved_bWantsToSprint;
    CharMovementComp->Safe_bPrevWantsToCrouch = Saved_bPrevWantsToCrouch;
    CharMovementComp->Safe_bWantsToDash = Saved_bWantsToDash;
    //CharMovementComp->ZeroCharacter_Owner->bPressedZeroJump = Saved_bPressedZeroJump;
    CharMovementComp->Safe_bHadAnimRootMotion = Saved_bHadAnimRootMotion;
    CharMovementComp->Safe_bTransitionFinished = Saved_bTransitionFinished;
}

bool UZeroBaseCharacterMovementComp::FSavedMove_Zero::IsImportantMove(const FSavedMovePtr& LastAckedMove) const
{
	FSavedMove_Zero* LastAckedZeroMove = static_cast<FSavedMove_Zero*>(LastAckedMove.Get());
    
	if (LastAckedZeroMove)
	{
		if (Saved_bPressedZeroJump && !LastAckedZeroMove->Saved_bPressedZeroJump) return true;
	}

	return Super::IsImportantMove(LastAckedMove);
}
#pragma endregion

#pragma region PredictionData
UZeroBaseCharacterMovementComp::FNetworkPredictionData_Client_Zero::FNetworkPredictionData_Client_Zero(const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement) {}

FSavedMovePtr UZeroBaseCharacterMovementComp::FNetworkPredictionData_Client_Zero::AllocateNewMove()
{
    return FSavedMovePtr(new FSavedMove_Zero());
}
#pragma endregion

#pragma region CharMoveOverrides
UZeroBaseCharacterMovementComp::UZeroBaseCharacterMovementComp()
{
    Sprint_MaxSpeed = 1200;
    Walk_MaxSpeed = 600;
    NavAgentProps.bCanCrouch = true;

    SlideFriction = 0.75f;
    SlideMinSpeed = 250;
    SlideEnterImpulse = 500;
    SlideGravityForce = 5000;

    DashImpulse = 1750.0f;
    DashCoolDownDuration = 1.f;
    AuthDashCoolDownDuration = 0.9f;

    WallBounceImpluse = 1000.0f;

    MantleMaxDistance = 200.0f;
    MantleReachHeight = 50.0f;
    MinMantleDepth = 30.0f;
    MantleMinWallSteepnessAngle = 75.0f;
    MantleMaxSurfaceAngle = 40.0f;
    MantleMaxAlignmentAngle = 45.0f;

    ZiplineMinKeyPressTime = 0.15f;
    ZiplineCheckTickIntervel = 0.15f;
    ZiplineCheckSphereRadius = 110.0f;
    ZiplineCheckMaxDistance = 2000.0f;
    ZiplineSpeed = 1000.0f;

    QuickFallImpulse = 500.0f;
    bUseSeparateBrakingFriction = true;
}

void UZeroBaseCharacterMovementComp::InitializeComponent()
{
    Super::InitializeComponent();
    ZeroCharacter_Owner = Cast<AZeroLockCharacter>(GetOwner());
}

void UZeroBaseCharacterMovementComp::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    Safe_bWantsToSprint = (Flags & FSavedMove_Zero::FLAG_Sprint) != 0;
    Safe_bWantsToDash = (Flags & FSavedMove_Zero::FLAG_Dash) != 0;
}

void UZeroBaseCharacterMovementComp::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
    Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

    if(MovementMode == MOVE_Walking)
    {
       MaxWalkSpeed = Safe_bWantsToSprint ? Sprint_MaxSpeed : Walk_MaxSpeed;
    }
    Safe_bPrevWantsToCrouch = bWantsToCrouch;
	
#if WITH_EDITOR
	if (GetWorld()->GetNetMode() == NM_Client || GetWorld()->GetNetMode() == NM_ListenServer || GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		int32 PlayerIndex = GetStablePlayerIndex();
		if (PlayerIndex >= 0 && PlayerIndex < 4)
		{
			if (IsServer())
			{
				// Server writes its authoritative position
				GServerCharacterLoc[PlayerIndex] = UpdatedComponent->GetComponentLocation();
			}
			else if (CharacterOwner->IsLocallyControlled())
			{
				// Client reads the server position and draws a wireframe capsule
				FVector ServerLoc = GServerCharacterLoc[PlayerIndex];
				if (!ServerLoc.IsZero())
				{
					DrawDebugCapsule(GetWorld(), ServerLoc, CapHH(), CapR(), FQuat::Identity, FColor::Blue, false, -1.0f, 0, 2.0f);
				}
			}
		}
	}
#endif
}

void UZeroBaseCharacterMovementComp::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{/*
    if(ZeroCharacter_Owner->ZeroJumpHoldTIme > ZiplineMinKeyPressTime && ZeroCharacter_Owner->bStillJumpKeyDown)
    {
        if(TryZipLine())
        {
           SetMovementMode(MOVE_Custom, CMOVE_Zipline);
           if(!CharacterOwner->HasAuthority() && ZiplineActorRef)
           {
                 Server_EnterZipline(ZiplineActorRef->GetZiplineComponent(), bZiplineMoveingToEnd);
           }
           return;
        }
    }
    
    if(ZeroCharacter_Owner->bPressedZeroJump)
    {
       if(TryMantle())
       {
          ZeroCharacter_Owner->StopJumping();
       	  ZeroCharacter_Owner->bPressedZeroJump = false;
       }
       else if(TryWallBounce())
       {
          ZeroCharacter_Owner->StopJumping();
          Proxy_bWallBounce = !Proxy_bWallBounce;
       		ZeroCharacter_Owner->bPressedZeroJump = false;
       }
       else
       {
          CharacterOwner->bPressedJump = true;
          CharacterOwner->CheckJumpInput(DeltaSeconds);
          ZeroCharacter_Owner->bPressedZeroJump = false;
       }
    }
    
    bool bAuthProxy = CharacterOwner->HasAuthority() && !CharacterOwner->IsLocallyControlled();
    if(Safe_bWantsToDash && CanDash())
    {
       if(!bAuthProxy || GetWorld()->GetTimeSeconds() - DashStartTime > AuthDashCoolDownDuration)
       {
          PerformDash();
          Safe_bWantsToDash = false;
          Proxy_bDashStart = !Proxy_bDashStart;
       }
       else
       {
          UE_LOG(LogTemp, Warning, TEXT("Tried Cheating Dash"));
       }
    }
    
    if(MovementMode == MOVE_Walking && bWantsToCrouch && Safe_bPrevWantsToCrouch)
    {
    	// Give the server a 50 unit tolerance so jitter doesn't cause a rejection
    	float RequiredSpeed = CharacterOwner->IsLocallyControlled() ? SlideMinSpeed : (SlideMinSpeed - 50.f);
       FHitResult PotenialSurface;
       if(Velocity.SizeSquared() > pow(RequiredSpeed, 2) && GetSlideSurface(PotenialSurface))
       {
          SetMovementMode(MOVE_Custom, CMOVE_Slide);
          return;
       }
    }
    
    if(IsCustomMovementMode(CMOVE_Slide) && !bWantsToCrouch)
    {
       SetMovementMode(MOVE_Walking);
    }*/
    Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UZeroBaseCharacterMovementComp::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
    Super::UpdateCharacterStateAfterMovement(DeltaSeconds);
    
    if (GetRootMotionSourceByID(TransitionRMS_ID) && GetRootMotionSourceByID(TransitionRMS_ID)->Status.HasFlag(ERootMotionSourceStatusFlags::Finished))
    {
       Velocity = FVector::ZeroVector;
       SetMovementMode(MOVE_Walking);
       RemoveRootMotionSourceByID(TransitionRMS_ID);
       Safe_bTransitionFinished = true;
    }
    Safe_bHadAnimRootMotion = HasAnimRootMotion();
}

void UZeroBaseCharacterMovementComp::PhysCustom(float deltaTime, int32 Iterations)
{
    Super::PhysCustom(deltaTime, Iterations);
    switch (CustomMovementMode)
    {
    case CMOVE_Slide: PhysSlide(deltaTime, Iterations); break;
    case CMOVE_Zipline: PhysZipline(deltaTime, Iterations); break;
    case CMOVE_Melee: PhysMelee(deltaTime, Iterations); break;
    default: UE_LOG(LogTemp, Fatal, TEXT("InvalidMovement MOde"));
    }
}

void UZeroBaseCharacterMovementComp::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
    
    if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_Slide) ExitSlide();
    if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_Zipline) ExitZipline();
    if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_Melee) ZLOG("EndMelee");

    if (IsCustomMovementMode(CMOVE_Slide)) EnterSlide();
    if (IsCustomMovementMode(CMOVE_Zipline)) EnterZipline();
    if (IsCustomMovementMode(CMOVE_Melee)) ZLOG("MeleeStart");

    if (ZeroMovementModeChangedDelegate.IsBound())
    {
       ZeroMovementModeChangedDelegate.Broadcast();
    }
}

bool UZeroBaseCharacterMovementComp::IsMovingOnGround() const
{
    return Super::IsMovingOnGround() || IsCustomMovementMode(CMOVE_Slide);
}

bool UZeroBaseCharacterMovementComp::CanCrouchInCurrentState() const
{
    if (!CanEverCrouch()) return false;
    return (IsFalling() || IsMovingOnGround()) && UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
}
#pragma endregion 

#pragma region Slide
void UZeroBaseCharacterMovementComp::EnterSlide()
{
   // bWantsToCrouch = true;
    Velocity += Velocity.GetSafeNormal2D() * SlideEnterImpulse;
}

void UZeroBaseCharacterMovementComp::ExitSlide()
{
   // bWantsToCrouch = false;
    FQuat NewRot = FRotationMatrix::MakeFromXZ(UpdatedComponent->GetForwardVector().GetSafeNormal2D(), FVector::UpVector).ToQuat();
    FHitResult Hit;
    SafeMoveUpdatedComponent(FVector::ZeroVector, NewRot, true, Hit);
}

void UZeroBaseCharacterMovementComp::PhysSlide(float DeltaTime, int32 Iterations)
{
    if(DeltaTime < MIN_TICK_TIME) return;

    RestorePreAdditiveRootMotionVelocity();
	
	float ExitSpeed = CharacterOwner->IsLocallyControlled() ? SlideMinSpeed : (SlideMinSpeed - 50.f);

	FHitResult SurfaceHit;
	if (!GetSlideSurface(SurfaceHit) || Velocity.SizeSquared() < pow(ExitSpeed, 2))
	{
		SetMovementMode(MOVE_Walking);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

   

    Velocity += SlideGravityForce * FVector::DownVector * DeltaTime;

    if(FMath::Abs(FVector::DotProduct(Acceleration.GetSafeNormal(), UpdatedComponent->GetRightVector())) > .5)
    {
       Acceleration = Acceleration.ProjectOnTo(UpdatedComponent->GetRightVector());
    }
    else
    {
       Acceleration = FVector::ZeroVector;
    }

    if(!HasRootMotionSources() && !CurrentRootMotion.HasOverrideVelocity())
    {
       CalcVelocity(DeltaTime, SlideFriction, true, GetMaxBrakingDeceleration());
    }
    ApplyRootMotionToVelocity(DeltaTime);

    Iterations++;
    bJustTeleported = false;

    FVector OldLocation = UpdatedComponent->GetComponentLocation();
    FHitResult Hit(1.f);
    FVector Adjusted = Velocity * DeltaTime;
    FVector VelPlaneDir = FVector::VectorPlaneProject(Velocity, SurfaceHit.Normal).GetSafeNormal();
    FQuat NewRot = FRotationMatrix::MakeFromXZ(VelPlaneDir, SurfaceHit.Normal).ToQuat();
    SafeMoveUpdatedComponent(Adjusted, NewRot, true, Hit);

    if(Hit.Time < 1.f)
    {
       HandleImpact(Hit, DeltaTime, Adjusted);
       SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
    }

	FHitResult NewSurfaceHit;
	if (!GetSlideSurface(NewSurfaceHit) || Velocity.SizeSquared() < pow(ExitSpeed, 2))
	{
		SetMovementMode(MOVE_Walking);
	}
    
    if(!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
    {
       Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
    }
}

bool UZeroBaseCharacterMovementComp::GetSlideSurface(FHitResult& OutHit) const
{
    FVector Start = UpdatedComponent->GetComponentLocation();
    FVector End = Start + CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f * FVector::DownVector;
    FName ProfileName = TEXT("BlockAllDynamic");
    return GetWorld()->LineTraceSingleByProfile(OutHit, Start, End, ProfileName, ZeroCharacter_Owner->GetIgnoreCharacterParams());
}
#pragma endregion

#pragma region Dash
void UZeroBaseCharacterMovementComp::OnDashCoolDownFinished()
{
    Safe_bWantsToDash = true;
}

bool UZeroBaseCharacterMovementComp::CanDash() const
{
    return true;
}

void UZeroBaseCharacterMovementComp::PerformDash()
{
    DashStartTime = GetWorld()->GetTimeSeconds();
    FVector DashDirection = (Acceleration.IsNearlyZero() ? UpdatedComponent->GetForwardVector() : Acceleration).GetSafeNormal2D();
    Velocity = DashImpulse * (DashDirection + FVector::UpVector * 0.25f);

    FQuat NewRot = FRotationMatrix::MakeFromXZ(DashDirection, FVector::UpVector).ToQuat();
    FHitResult Hit;
    SafeMoveUpdatedComponent(FVector::ZeroVector, NewRot, false, Hit);

    SetMovementMode(MOVE_Falling);

    if(DashStartDelegate.IsBound())
    {
       DashStartDelegate.Broadcast();
    }
}

void UZeroBaseCharacterMovementComp::OnRep_DashStart()
{
    if(Proxy_bDashStart && DashStartDelegate.IsBound())
    {
       DashStartDelegate.Broadcast();
    }
}
#pragma endregion

#pragma region Mantle
void UZeroBaseCharacterMovementComp::OnRep_ShortMantle() {}
void UZeroBaseCharacterMovementComp::OnRep_TallMantle() {}

void UZeroBaseCharacterMovementComp::OnRep_WallBounce()
{
    if (Proxy_bWallBounce && WallBounceDelegate.IsBound())
    {
       WallBounceDelegate.Broadcast();
    }
}

bool UZeroBaseCharacterMovementComp::TryMantle()
{
    if (!(IsMovementMode(MOVE_Walking) && !IsCrouching()) && !IsMovementMode(MOVE_Falling)) return false;

    FVector BaseLoc = UpdatedComponent->GetComponentLocation() + FVector::DownVector * CapHH();
    FVector Fwd = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
    auto Params = ZeroCharacter_Owner->GetIgnoreCharacterParams();
    float MaxHeight = CapHH() * 2 + MantleReachHeight;
    float CosMMWSA = FMath::Cos(FMath::DegreesToRadians(MantleMinWallSteepnessAngle));
    float CosMMSA = FMath::Cos(FMath::DegreesToRadians(MantleMaxSurfaceAngle));
    float CosMMAA = FMath::Cos(FMath::DegreesToRadians(MantleMaxAlignmentAngle));

    FHitResult FrontHit;
	float CheckDistance = FMath::Clamp(Velocity | Fwd, CapR() + 30.f, MantleMaxDistance);
	
	if (!CharacterOwner->IsLocallyControlled())
	{
		CheckDistance += 20.0f; 
	}
    
	FVector FrontStart = BaseLoc + FVector::UpVector * (MaxStepHeight - 1);
    
    for (int i = 0; i < 6; i++)
    {
       if (GetWorld()->LineTraceSingleByProfile(FrontHit, FrontStart, FrontStart + Fwd * CheckDistance, "BlockAll", Params)) break;
       FrontStart += FVector::UpVector * (2.f * CapHH() - (MaxStepHeight - 1)) / 5;
    }
    if (!FrontHit.IsValidBlockingHit()) return false;
    
    float CosWallSteepnessAngle = FrontHit.Normal | FVector::UpVector;
    if (FMath::Abs(CosWallSteepnessAngle) > CosMMWSA || (Fwd | -FrontHit.Normal) < CosMMAA) return false;

    TArray<FHitResult> HeightHits;
    FHitResult SurfaceHit;
    FVector WallUp = FVector::VectorPlaneProject(FVector::UpVector, FrontHit.Normal).GetSafeNormal();
    float WallCos = FVector::UpVector | FrontHit.Normal;
    float WallSin = FMath::Sqrt(1 - WallCos * WallCos);
    FVector TraceStart = FrontHit.Location + Fwd + WallUp * (MaxHeight - (MaxStepHeight - 1)) / WallSin;
    
    if (!GetWorld()->LineTraceMultiByProfile(HeightHits, TraceStart, FrontHit.Location + Fwd, "BlockAll", Params)) return false;
    for (const FHitResult& Hit : HeightHits)
    {
       if (Hit.IsValidBlockingHit())
       {
          SurfaceHit = Hit;
          break;
       }
    }
    if (!SurfaceHit.IsValidBlockingHit() || (SurfaceHit.Normal | FVector::UpVector) < CosMMSA) return false;
    float Height = (SurfaceHit.Location - BaseLoc) | FVector::UpVector;
    if (Height > MaxHeight) return false;

    float SurfaceCos = FVector::UpVector | SurfaceHit.Normal;
    float SurfaceSin = FMath::Sqrt(1 - SurfaceCos * SurfaceCos);
    FVector ClearCapLoc = SurfaceHit.Location + Fwd * CapR() + FVector::UpVector * (CapHH() + 1 + CapR() * 2 * SurfaceSin);
    FCollisionShape CapShape = FCollisionShape::MakeCapsule(CapR(), CapHH());
    
    if (GetWorld()->OverlapAnyTestByProfile(ClearCapLoc, FQuat::Identity, "BlockAll", CapShape, Params)) return false;

    FVector TransitionTarget = ClearCapLoc;
    float UpSpeed = Velocity | FVector::UpVector;
    float TransDistance = FVector::Dist(TransitionTarget, UpdatedComponent->GetComponentLocation());

    TransitionQueuedMontageSpeed = FMath::GetMappedRangeValueClamped(FVector2D(-500, 750), FVector2D(.9f, 1.2f), UpSpeed);
    TransitionRMS.Reset();
    TransitionRMS = MakeShared<FRootMotionSource_MoveToForce>();
    TransitionRMS->AccumulateMode = ERootMotionAccumulateMode::Override;
    TransitionRMS->Duration = FMath::Clamp(TransDistance / 500.f, .1f, .25f);
    TransitionRMS->StartLocation = UpdatedComponent->GetComponentLocation();
    TransitionRMS->TargetLocation = TransitionTarget;
    
    Velocity = FVector::ZeroVector;
    SetMovementMode(MOVE_Flying);
    TransitionRMS_ID = ApplyRootMotionSource(TransitionRMS);
    TransitionName = "Mantle";
    return true;
}
#pragma endregion

#pragma region WallBounce
bool UZeroBaseCharacterMovementComp::TryWallBounce()
{
	if (!IsMovementMode(MOVE_Falling)) return false;
    
	FHitResult WallHit;
	
	float SweepMultiplier = CharacterOwner->IsLocallyControlled() ? 1.25f : 1.5f;
	FCollisionShape CapShape = FCollisionShape::MakeCapsule(CapR() * SweepMultiplier, CapHH() / 2);
    
	FVector TraceLocation = UpdatedComponent->GetComponentLocation();
	FQuat RotationX = FQuat::Identity;
    
	if(GetWorld()->SweepSingleByChannel(WallHit, TraceLocation, TraceLocation, RotationX, ECC_WorldStatic, CapShape, ZeroCharacter_Owner->GetIgnoreCharacterParams()))
	{
		FVector WallLaunchDir = WallHit.ImpactNormal.GetSafeNormal() + FVector::UpVector;
		Velocity = WallLaunchDir * WallBounceImpluse;
		if (WallBounceDelegate.IsBound()) WallBounceDelegate.Broadcast();
		SetMovementMode(MOVE_Falling);
		return true;
	}
	return false;
}
#pragma endregion

#pragma region Zipline
bool UZeroBaseCharacterMovementComp::TryZipLine()
{
    if(IsCustomMovementMode(CMOVE_Zipline)) return false;
    if(GetWorld()->TimeSeconds - ZiplineLastTickTime < ZiplineCheckTickIntervel) return false;
    ZiplineLastTickTime = GetWorld()->TimeSeconds;
    
    FCollisionShape ZipCap = FCollisionShape::MakeSphere(ZiplineCheckSphereRadius);
    FVector TraceLocation = CamLoc() + CamFV() * ZiplineCheckSphereRadius;
    FVector TraceEndLocation = CamLoc() + CamFV() * ZiplineCheckMaxDistance;
    FHitResult ZipHit;
    
    if(GetWorld()->SweepSingleByObjectType(ZipHit, TraceLocation, TraceEndLocation, CamQuat(), ECC_Vehicle, ZipCap, ZeroCharacter_Owner->GetIgnoreCharacterParams()))
    {
       if(Cast<AZero_ZiplineActor>(ZipHit.GetActor()))
       {
          ZiplineActorRef = Cast<AZero_ZiplineActor>(ZipHit.GetActor());
          ZiplineSplineComp = ZiplineActorRef->GetZiplineComponent();
          float ClosestDistancetoPlayer = ZiplineSplineComp->GetDistanceAlongSplineAtLocation(CharLocation(), ESplineCoordinateSpace::World);
          float maxDis = ZiplineSplineComp->GetSplineLength();
          FVector ZiplineDir = ZiplineSplineComp->GetDirectionAtDistanceAlongSpline(ClosestDistancetoPlayer, ESplineCoordinateSpace::World);
          float Angle = CharacterOwner->GetActorForwardVector() | ZiplineDir;
          
          bZiplineMoveingToEnd = (Angle > 0);
          return true;
       }
    }
    return false;
}

void UZeroBaseCharacterMovementComp::Server_EnterZipline_Implementation(USplineComponent* ZiplineToUse, bool InSplineDir)
{
    ZiplineSplineComp = ZiplineToUse;
    bZiplineMoveingToEnd = InSplineDir;
    SetMovementMode(MOVE_Custom, CMOVE_Zipline);
}

void UZeroBaseCharacterMovementComp::EnterZipline()
{
    Velocity = FVector::ZeroVector;
}

void UZeroBaseCharacterMovementComp::ExitZipline() {}

void UZeroBaseCharacterMovementComp::PhysZipline(float DeltaTime, int32 Iterations)
{
    if(DeltaTime < MIN_TICK_TIME) return;

    RestorePreAdditiveRootMotionVelocity();
    if(!ZiplineSplineComp) return;
    
    if(Safe_bWantsToDash || bWantsToCrouch)
    {
       SetMovementMode(MOVE_Falling);
       StartNewPhysics(DeltaTime, Iterations);
       return;
    }

    Iterations++;
    bJustTeleported = false;
    FVector OldLocation = UpdatedComponent->GetComponentLocation();
    float MaxDis = ZiplineSplineComp->GetSplineLength();
    float DistancetoPoint = ZiplineSplineComp->GetDistanceAlongSplineAtLocation(CharLocation(), ESplineCoordinateSpace::World);
    float DistanceToMoveAlongSpline = bZiplineMoveingToEnd ? ZiplineSpeed : (ZiplineSpeed * -1);
    FVector TargetLocation = ZiplineSplineComp->GetLocationAtDistanceAlongSpline(DistancetoPoint + DistanceToMoveAlongSpline, ESplineCoordinateSpace::World);
    FVector Adjusted = (TargetLocation - CharLocation()).GetSafeNormal() * DeltaTime * ZiplineSpeed;
    
    if((bZiplineMoveingToEnd && MaxDis <= DistancetoPoint) || (!bZiplineMoveingToEnd && DistancetoPoint <= 0.0f))
    {
       SetMovementMode(MOVE_Falling);
       StartNewPhysics(DeltaTime, Iterations);
       return;
    }

    FHitResult Hit(1.f);
    FVector FVofSplinePoint = ZiplineSplineComp->GetDirectionAtDistanceAlongSpline(DistancetoPoint, ESplineCoordinateSpace::World).GetSafeNormal();
    FVofSplinePoint.Z = 0;
    if(!bZiplineMoveingToEnd) FVofSplinePoint *= -1;
    
    FQuat NewRot = FRotationMatrix::MakeFromXZ(FVofSplinePoint, FVector::UpVector).ToQuat();
    SafeMoveUpdatedComponent(Adjusted, NewRot, true, Hit);

    if(!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
    {
       Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
    }
}

void UZeroBaseCharacterMovementComp::QuickFallDown() {}
#pragma endregion

#pragma region Helper Functions
bool UZeroBaseCharacterMovementComp::IsServer() const { return CharacterOwner->HasAuthority(); }
float UZeroBaseCharacterMovementComp::CapR() const { return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius(); }
float UZeroBaseCharacterMovementComp::CapHH() const { return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(); }
bool UZeroBaseCharacterMovementComp::IsMovementMode(EMovementMode inMovementMode) const { return inMovementMode == MovementMode; }
FVector UZeroBaseCharacterMovementComp::CharLocation() const { return UpdatedComponent->GetComponentLocation(); }
FRotator UZeroBaseCharacterMovementComp::CharRotation() const { return UpdatedComponent->GetComponentRotation(); }
FVector UZeroBaseCharacterMovementComp::CamFV() const { return ZeroCharacter_Owner->GetFollowCamera()->GetForwardVector(); }
FVector UZeroBaseCharacterMovementComp::CamLoc() const { return ZeroCharacter_Owner->GetFollowCamera()->GetComponentLocation(); }
FQuat UZeroBaseCharacterMovementComp::CamQuat() const { return ZeroCharacter_Owner->GetFollowCamera()->GetComponentRotation().Quaternion(); }

int32 UZeroBaseCharacterMovementComp::GetStablePlayerIndex() const
{
	AGameStateBase* GS = GetWorld()->GetGameState();
	if (GS && !GS->PlayerArray.IsEmpty() && CharacterOwner)
	{
		TArray<APlayerState*> SortedArray = GS->PlayerArray;
        
		SortedArray.Sort([](const APlayerState& A, const APlayerState& B) {
			return A.GetPlayerId() < B.GetPlayerId();
		});
        
		return SortedArray.IndexOfByKey(CharacterOwner->GetPlayerState());
	}
	return -1;
}
DEFINE_LOG_CATEGORY_STATIC(LogZeroMovement, Log, All);
void UZeroBaseCharacterMovementComp::OnClientCorrectionReceived(
	class FNetworkPredictionData_Client_Character& ClientData, float TimeStamp, FVector NewLocation,
	FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition,
	uint8 ServerMovementMode, FVector ServerGravityDirection)
{
	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName,
	                                  bHasBase, bBaseRelativePosition,
	                                  ServerMovementMode, ServerGravityDirection);
	
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
    
	UE_VLOG_SPHERE(this, LogZeroMovement, Error, OldLocation, 10.0f, FColor::Red, TEXT("Pre-correction"));
	UE_VLOG_ARROW(this, LogZeroMovement, Error, OldLocation, NewLocation, FColor::Yellow, TEXT("Correction Shift"));
	UE_VLOG_SPHERE(this, LogZeroMovement, Error, NewLocation, 10.0f, FColor::Green, TEXT("Post-correction"));
    
	UE_VLOG(this, LogZeroMovement, Error, TEXT("Correction! Client Mode: %d | Server Mode: %d"), MovementMode.GetIntValue(), ServerMovementMode);
}

bool UZeroBaseCharacterMovementComp::IsCustomMovementMode(ECustomMovementMode inCustomMode) const { return MovementMode == MOVE_Custom && CustomMovementMode == inCustomMode; }

void UZeroBaseCharacterMovementComp::PhysMelee(float DeltaTime, int32 Iterations)
{
    if (DeltaTime < MIN_TICK_TIME) return;

    RestorePreAdditiveRootMotionVelocity();
    Iterations++;
    bJustTeleported = false;

    FVector OldLocation = UpdatedComponent->GetComponentLocation();
    
    FVector MeleeDir = CharacterOwner->GetControlRotation().Vector();
    MeleeDir.Z = 0.f; 
    MeleeDir.Normalize();

    FVector MoveDelta = MeleeDir * (1000.f * DeltaTime); 
    FQuat NewRot = FRotationMatrix::MakeFromXZ(MeleeDir, FVector::UpVector).ToQuat();

    FHitResult PlayerHit;
    FCollisionShape CapShape = CharacterOwner->GetCapsuleComponent()->GetCollisionShape();
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(CharacterOwner);

    bool bHitPlayer = GetWorld()->SweepSingleByObjectType(
        PlayerHit, 
        OldLocation, 
        OldLocation + MoveDelta, 
        CharacterOwner->GetActorQuat(), 
        FCollisionObjectQueryParams(ECC_GameTraceChannel1), 
        CapShape, 
        Params
    );

    if (bHitPlayer)
    {
        MoveDelta *= PlayerHit.Time;
    }

    FHitResult WallHit(1.f);
    SafeMoveUpdatedComponent(MoveDelta, NewRot, true, WallHit);

    if (WallHit.Time < 1.f)
    {
        SlideAlongSurface(MoveDelta, (1.f - WallHit.Time), WallHit.Normal, WallHit, true);
    }

    if (bHitPlayer)
    {
        if (MeleeHitDelegate.IsBound())
        {
            MeleeHitDelegate.Broadcast();
        }
        SetMovementMode(MOVE_Falling);
    }

    if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
    {
        Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
    }
}
#pragma endregion

#pragma region Inputs
void UZeroBaseCharacterMovementComp::SprintPressed() { Safe_bWantsToSprint = true; }
void UZeroBaseCharacterMovementComp::SprintReleased() { Safe_bWantsToSprint = false; }
void UZeroBaseCharacterMovementComp::CrouchPressed() { bWantsToCrouch = true; }
void UZeroBaseCharacterMovementComp::CrouchReleased() { bWantsToCrouch = false; }

void UZeroBaseCharacterMovementComp::DashPressed()
{
    float currentTime = GetWorld()->GetTimeSeconds();
    if(currentTime - DashStartTime >= DashCoolDownDuration)
    {
       Safe_bWantsToDash = true;
    }
    else
    {
       GetWorld()->GetTimerManager().SetTimer(TimerHandle_DashCoolDown, this, &ThisClass::OnDashCoolDownFinished, DashCoolDownDuration - (currentTime - DashStartTime));
    }
}

void UZeroBaseCharacterMovementComp::DashReleased()
{
    GetWorld()->GetTimerManager().ClearTimer(TimerHandle_DashCoolDown);
    Safe_bWantsToDash = false;
}

void UZeroBaseCharacterMovementComp::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UZeroBaseCharacterMovementComp, Proxy_bDashStart, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UZeroBaseCharacterMovementComp, Proxy_bWallBounce, COND_SkipOwner);
}
#pragma endregion

class FNetworkPredictionData_Client* UZeroBaseCharacterMovementComp::GetPredictionData_Client() const
{
    check(PawnOwner != nullptr)
    if(ClientPredictionData == nullptr)
    {
       UZeroBaseCharacterMovementComp* MutableThis = const_cast<UZeroBaseCharacterMovementComp*>(this);
       MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Zero(*this);
       MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 256.f *2;
       MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 384.f*2;
    }
    return ClientPredictionData;
}