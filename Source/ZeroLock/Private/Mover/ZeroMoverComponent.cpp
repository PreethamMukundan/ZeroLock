// Copyright Preetham Mukundan (C) 2026


#include "Mover/ZeroMoverComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "DefaultMovementSet/LayeredMoves/MultiJumpLayeredMove.h"
#include "DefaultMovementSet/Settings/StanceSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Mover/ZeroMovementData.h" 
#include "Mover/ZeroMoverPawn.h"

UZeroMoverComponent::UZeroMoverComponent()
{
    bWantsInitializeComponent = true;
}

void UZeroMoverComponent::OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd)
{
    Super::OnMoverPreSimulationTick(TimeStep, InputCmd);

    const FCharacterDefaultInputs* FoundDefaults = InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
    const FZeroMovementInputs* FoundZeroInputs = InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>();

    if (!FoundDefaults || !FoundZeroInputs) return;

    FName CurrentMode = GetMovementModeName();
    
    if (FoundZeroInputs->bJumpHold)
    {
        if (TryMantle(*FoundDefaults,*FoundZeroInputs))
        {
            return;
        }
    }
    HandleCrouching(CurrentMode,*FoundZeroInputs);
    HandleAirJumpTracking(CurrentMode, *FoundZeroInputs);
   

    if (FoundZeroInputs->bWantsToDash)
    {
        HandleDashInputs(*FoundDefaults, *FoundZeroInputs);
    }
    
    if (FoundZeroInputs->bHasAbilityMove)
    {
        if (CurrentMode == DefaultModeNames::Walking)
        {
            QueueNextMode(DefaultModeNames::Falling);
        }
        TSharedPtr<FLayeredMove_LinearVelocity> AbilityMove = MakeShared<FLayeredMove_LinearVelocity>();
        AbilityMove->Velocity = FoundZeroInputs->AbilityVelocity;
        AbilityMove->DurationMs = FoundZeroInputs->AbilityMoveDuration * 1000.0f;
        AbilityMove->MixMode = EMoveMixMode::OverrideVelocity;
        
        QueueLayeredMove(AbilityMove);
    }
    if (FoundZeroInputs->bHasDynamicAbilityMove && FoundZeroInputs->DynamicTargetActor)
    {
        if (CurrentMode == DefaultModeNames::Walking)
        {
            QueueNextMode(DefaultModeNames::Falling);
        }
        
        TSharedPtr<FLayeredMove_MoveToDynamic> DynamicMove = MakeShared<FLayeredMove_MoveToDynamic>();
        DynamicMove->LocationActor = FoundZeroInputs->DynamicTargetActor;
        DynamicMove->StartLocation = UpdatedComponent->GetComponentLocation();
        DynamicMove->DurationMs = FoundZeroInputs->DynamicMoveDuration * 1000.0f;
        DynamicMove->MixMode = EMoveMixMode::OverrideVelocity;
        
        QueueLayeredMove(DynamicMove);
    }
}

void UZeroMoverComponent::InitializeComponent()
{
    Super::InitializeComponent();
    if (UStanceSettings* MyStanceSettings = FindSharedSettings_Mutable<UStanceSettings>())
    {
        MyStanceSettings->CrouchHalfHeight = 50.0f;
        
    }
}

void UZeroMoverComponent::PlayDirectionalDashMontage(const FVector& DashDirection)
{
    AZeroMoverPawn* OwnerCharacter = Cast<AZeroMoverPawn>(GetOwner());
    if (!OwnerCharacter)
    {
        return;
    }

    UAnimMontage* MontageToPlay = nullptr;
    FVector Forward = OwnerCharacter->GetActorForwardVector();
    FVector Right = OwnerCharacter->GetActorRightVector();

    float ForwardDot = FVector::DotProduct(DashDirection, Forward);
    float RightDot = FVector::DotProduct(DashDirection, Right);

    if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
    {
        MontageToPlay = (ForwardDot > 0.0f) ? DashForwardMontage : DashBackwardMontage;
    }
    else
    {
        MontageToPlay = (RightDot > 0.0f) ? DashRightMontage : DashLeftMontage;
    }
    
    if (MontageToPlay)
    {
        OwnerCharacter->GetSkeletalMeshComponent()->GetAnimInstance()->Montage_Play(MontageToPlay);
    }
}

void UZeroMoverComponent::HandleAirJumpTracking(const FName& CurrentMode, const FZeroMovementInputs& ZeroInputs)
{
    if (CurrentMode == DefaultModeNames::Walking || CurrentMode == TEXT("Sliding"))
    {
        LocalAirJumpsUsed = 0;
    }

    if (HandleWallBounceCheck(ZeroInputs, CurrentMode))
    {
        return;
    }
    
    if (ZeroInputs.bCustomJumpJustPressed && LocalAirJumpsUsed < MaxAirJumps)
    {
        TSharedPtr<FLayeredMove_MultiJump> JumpMove = MakeShared<FLayeredMove_MultiJump>();
        JumpMove->UpwardsSpeed = VerticalJumpForce;

        if (CurrentMode == DefaultModeNames::Falling)
        {
            JumpMove->UpwardsSpeed = VerticalJumpForce * 1.5f;

            // Only spawn cosmetic effects on local clients during non-resimulated steps
            if (VFX_AirJump && GetWorld()->IsNetMode(NM_DedicatedServer) == false)
            {
                if (!GetLastTimeStep().bIsResimulating)
                {
                    UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFX_AirJump, UpdatedComponent->GetComponentLocation(), FRotator::ZeroRotator);
                }
            }
        }

        JumpMove->MaximumInAirJumps = MaxAirJumps;
        JumpMove->MixMode = EMoveMixMode::OverrideVelocity;
        QueueLayeredMove(JumpMove);
        LocalAirJumpsUsed++;

        if (IsCrouching())
        {
            UnCrouch();
        }
    }
}

void UZeroMoverComponent::HandleCrouching(const FName& CurrentMode, const FZeroMovementInputs& ZeroInputs)
{
    if (CurrentMode != DefaultModeNames::Walking)
    {
        return;
    }
    if (ZeroInputs.bWantsToCrouch)
    {
        Crouch();
    }
    else
    {
        UnCrouch();
    }
}





bool UZeroMoverComponent::HandleWallBounceCheck(const FZeroMovementInputs& ZeroInputs, const FName& CurrentMode)
{
    if (CurrentMode != DefaultModeNames::Falling || !ZeroInputs.bCustomJumpJustPressed)
    {
        return false;
    }
    UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetUpdatedComponent());
    if (!Capsule) return false;

    FVector Start = Capsule->GetComponentLocation();
    float CheckRadius = Capsule->GetScaledCapsuleRadius() + WallBounceTracePadding;
    
    FCollisionShape Sphere = FCollisionShape::MakeSphere(CheckRadius);
    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredActor(GetOwner());

    FHitResult Hit;
    bool bHitWall = Capsule->GetWorld()->SweepSingleByProfile(
        Hit, Start, Start, FQuat::Identity, TEXT("BlockAllDynamic"), Sphere, TraceParams
    );
    
    if (bHitWall && FMath::Abs(Hit.Normal.Z) < 0.3f)
    {
        const FMoverDefaultSyncState* SyncState = GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
        FVector CurrentVelocity = SyncState ? SyncState->GetVelocity_WorldSpace() : FVector::ZeroVector;
     
        FVector WallNormal = Hit.Normal;
        
        if (FVector::DotProduct(CurrentVelocity, WallNormal) < 0.0f)
        {
            CurrentVelocity = FVector::VectorPlaneProject(CurrentVelocity, WallNormal);
        }
        
        FVector LaunchVelocity = CurrentVelocity;
        LaunchVelocity += WallNormal * WallJumpOffForce;
        LaunchVelocity.Z = WallJumpVerticalForce;

        if (VFX_WallBounce && GetWorld()->IsNetMode(NM_DedicatedServer) == false)
        {
            if (!GetLastTimeStep().bIsResimulating)
            {
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFX_WallBounce, Hit.Location, Hit.Normal.Rotation());
            }
        }
        
        TSharedPtr<FLayeredMove_LinearVelocity> WallBounceMove = MakeShared<FLayeredMove_LinearVelocity>();
        WallBounceMove->Velocity = LaunchVelocity;
        WallBounceMove->DurationMs = 0.15f * 1000.0f; 
        WallBounceMove->MixMode = EMoveMixMode::OverrideVelocity; 

        QueueLayeredMove(WallBounceMove);
        return true;
    }
    return false;
}




void UZeroMoverComponent::HandleDashInputs(const FCharacterDefaultInputs& DefaultInputs, const FZeroMovementInputs& ZeroInputs)
{
    TSharedPtr<FLayeredMove_LinearVelocity> DashMove = MakeShared<FLayeredMove_LinearVelocity>();
    
    FVector MoveIntent = DefaultInputs.GetMoveInput();
    FVector DashDirection = MoveIntent.IsNearlyZero() ? ZeroInputs.LookDir.Vector().GetSafeNormal2D() : MoveIntent.GetSafeNormal();
    
    if (DashDirection.IsNearlyZero())
    {
        DashDirection = GetOwner()->GetActorForwardVector();
    }

    if (GetWorld()->IsNetMode(NM_DedicatedServer) == false)
    {
        if (!GetLastTimeStep().bIsResimulating)
        {
            PlayDirectionalDashMontage(DashDirection);
        }
    }

    DashMove->Velocity = DashDirection * DashSpeed;
    DashMove->DurationMs = DashDuration * 1000.0f;
    DashMove->MixMode = EMoveMixMode::OverrideVelocity;

    QueueLayeredMove(DashMove);
}

bool UZeroMoverComponent::TryMantle(const FCharacterDefaultInputs& DefaultInputs, const FZeroMovementInputs& ZeroInputs)
{
  FVector MoveIntent = DefaultInputs.GetMoveInput();

    if (MoveIntent.IsNearlyZero() || FVector::DotProduct(MoveIntent.GetSafeNormal(), GetOwner()->GetActorForwardVector()) < 0.5f)
    {
        return false;
    }
    
    UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetUpdatedComponent());
    if (!Capsule) return false;

    FName CurrentMode = GetMovementModeName();
    if (CurrentMode != DefaultModeNames::Walking && CurrentMode != DefaultModeNames::Falling) 
        return false;
    
    const float CapHH = Capsule->GetScaledCapsuleHalfHeight();
    const float CapR = Capsule->GetScaledCapsuleRadius();
    const float MaxStepHeight = 35.0f; 

    FVector BaseLoc = Capsule->GetComponentLocation() + FVector::DownVector * CapHH;
    FVector Fwd = Capsule->GetForwardVector().GetSafeNormal2D();

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());

    float MaxHeight = (CapHH * 2.f) + MantleReachHeight;
    float CosMMWSA = FMath::Cos(FMath::DegreesToRadians(MantleMinWallSteepnessAngle));
    float CosMMSA = FMath::Cos(FMath::DegreesToRadians(MantleMaxSurfaceAngle));
    float CosMMAA = FMath::Cos(FMath::DegreesToRadians(MantleMaxAlignmentAngle));

    FHitResult FrontHit;
    const FMoverDefaultSyncState* SyncState = GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
    FVector CurrentVelocity = SyncState ? SyncState->GetVelocity_WorldSpace() : FVector::ZeroVector;
    float CheckDistance = FMath::Clamp(FVector::DotProduct(CurrentVelocity, Fwd), CapR + 30.f, MantleMaxDistance);

    FVector FrontStart = BaseLoc + FVector::UpVector * (MaxStepHeight - 1.f);
    
    for (int i = 0; i < 6; i++)
    {
       if (GetWorld()->LineTraceSingleByProfile(FrontHit, FrontStart, FrontStart + Fwd * CheckDistance, TEXT("BlockAll"), Params)) 
           break;
       FrontStart += FVector::UpVector * ((2.f * CapHH) - (MaxStepHeight - 1.f)) / 5.f;
    }
    
    if (!FrontHit.IsValidBlockingHit()) return false;
    
    float CosWallSteepnessAngle = FVector::DotProduct(FrontHit.Normal, FVector::UpVector);
    if (FMath::Abs(CosWallSteepnessAngle) > CosMMWSA || FVector::DotProduct(Fwd, -FrontHit.Normal) < CosMMAA) 
        return false;

    TArray<FHitResult> HeightHits;
    FHitResult SurfaceHit;
    FVector WallUp = FVector::VectorPlaneProject(FVector::UpVector, FrontHit.Normal).GetSafeNormal();
    float WallCos = FVector::DotProduct(FVector::UpVector, FrontHit.Normal);
    float WallSin = FMath::Sqrt(1.f - (WallCos * WallCos));

    if (FMath::IsNearlyZero(WallSin)) WallSin = 0.001f; 
    
    FVector TraceStart = FrontHit.Location + Fwd + WallUp * (MaxHeight - (MaxStepHeight - 1.f)) / WallSin;
    
    if (!GetWorld()->LineTraceMultiByProfile(HeightHits, TraceStart, FrontHit.Location + Fwd, TEXT("BlockAll"), Params)) 
        return false;
        
    for (const FHitResult& Hit : HeightHits)
    {
       if (Hit.IsValidBlockingHit())
       {
          SurfaceHit = Hit;
          break;
       }
    }
    
    if (!SurfaceHit.IsValidBlockingHit() || FVector::DotProduct(SurfaceHit.Normal, FVector::UpVector) < CosMMSA) 
        return false;
        
    float Height = FVector::DotProduct((SurfaceHit.Location - BaseLoc), FVector::UpVector);
    if (Height > MaxHeight) return false;

    float SurfaceCos = FVector::DotProduct(FVector::UpVector, SurfaceHit.Normal);
    float SurfaceSin = FMath::Sqrt(1.f - (SurfaceCos * SurfaceCos));
    FVector ClearCapLoc = SurfaceHit.Location + Fwd * CapR + FVector::UpVector * (CapHH + 1.f + CapR * 2.f * SurfaceSin);
    FCollisionShape CapShape = FCollisionShape::MakeCapsule(CapR, CapHH);
    
    if (GetWorld()->OverlapAnyTestByProfile(ClearCapLoc, FQuat::Identity, TEXT("BlockAll"), CapShape, Params)) 
        return false;

    FVector TransitionStart = Capsule->GetComponentLocation();
    FVector TransitionTarget = ClearCapLoc;
    float TransDistance = FVector::Dist(TransitionTarget, TransitionStart);
    float CalculateDuration = FMath::Clamp(TransDistance / 500.f, 0.1f, 0.25f);

    TSharedPtr<FLayeredMove_MoveTo> MantleMove = MakeShared<FLayeredMove_MoveTo>();
    
    MantleMove->StartLocation = TransitionStart; 
    MantleMove->TargetLocation = TransitionTarget;
    MantleMove->DurationMs = CalculateDuration * 1000.0f;
    MantleMove->MixMode = EMoveMixMode::OverrideVelocity;
    MantleMove->bRestrictSpeedToExpected = true; 

    QueueLayeredMove(MantleMove);
    
    return true;
}

void UZeroMoverComponent::RequestSafeAbilityMove(FVector Velocity, float Duration)
{
    bLatchedAbilityMove = true;
    LatchedAbilityVelocity = Velocity;
    LatchedAbilityDuration = Duration;
}

void UZeroMoverComponent::RequestSafeDynamicAbilityMove(AActor* TargetActor, float Duration)
{
    bLatchedDynamicMove = true;
    LatchedDynamicActor = TargetActor;
    LatchedDynamicDuration = Duration;
}
