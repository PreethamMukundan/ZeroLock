// Copyright Preetham Mukundan (C) 2026


#include "Mover/ZeroMoverComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "NiagaraFunctionLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "DefaultMovementSet/Settings/StanceSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GAS/BaseCharAttributeSet.h"
#include "GAS/ZL_GameplayTags.h"
#include "Mover/ZeroCubedMode.h"
#include "Mover/ZeroGooBallMode.h"
#include "Mover/ZeroGooBallShapeModifier.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "Mover/ZeroDashMove.h"
#include "Mover/ZeroInstantMovementEffects.h"
#include "Mover/ZeroMovementData.h"
#include "Mover/ZeroMoverPawn.h"

UZeroMoverComponent::UZeroMoverComponent()
{
    bWantsInitializeComponent = true;

    // Jumps are handled in HandleAirJumpTracking; the base class would queue a second jump impulse on top.
    bHandleJump = false;

    PersistentSyncStateDataTypes.Add(FMoverDataPersistence(FZeroMoverSimState::StaticStruct(), true));
}

void UZeroMoverComponent::InitializeComponent()
{
    Super::InitializeComponent();

    // The modes live in the Blueprint (BP_Mover_Monkey); add the Cubed mode here so no Blueprint has to.
    if (!MovementModes.Contains(CubedModeName))
    {
        AddMovementModeFromClass(CubedModeName, CubedModeClass ? CubedModeClass.Get() : UZeroCubedMode::StaticClass());
    }
    if (!MovementModes.Contains(GooBallModeName))
    {
        AddMovementModeFromClass(GooBallModeName, GooBallModeClass ? GooBallModeClass.Get() : UZeroGooBallMode::StaticClass());
    }
    if (UStanceSettings* MyStanceSettings = FindSharedSettings_Mutable<UStanceSettings>())
    {
        MyStanceSettings->CrouchHalfHeight = 50.0f;

    }

    // A Blueprint that saved its own persistence list wouldn't have the entry added in the constructor.
    const bool bHasSimState = PersistentSyncStateDataTypes.ContainsByPredicate([](const FMoverDataPersistence& Entry)
    {
        return Entry.RequiredType == FZeroMoverSimState::StaticStruct();
    });
    if (!bHasSimState)
    {
        PersistentSyncStateDataTypes.Add(FMoverDataPersistence(FZeroMoverSimState::StaticStruct(), true));
    }

    OnPostMovement.AddUniqueDynamic(this, &UZeroMoverComponent::HandlePostMovement);
    OnPostSimulationRollback.AddUniqueDynamic(this, &UZeroMoverComponent::HandleSimRollback);
}

void UZeroMoverComponent::BeginPlay()
{
    Super::BeginPlay();

    // The base class only binds the pre-sim tick while it handles jumping or stances; all of our movement logic lives there.
    OnPreSimulationTick.AddUniqueDynamic(this, &UZeroMoverComponent::OnMoverPreSimulationTick);
}

void UZeroMoverComponent::OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd)
{
    BeginSimStep(TimeStep);

    const FCharacterDefaultInputs* FoundDefaults = InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
    const FZeroMovementInputs* FoundZeroInputs = InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>();

    FName CurrentMode = GetMovementModeName();

    // Must happen before Super, which turns the crouch request into a stance change this same frame.
    if (FoundZeroInputs)
    {
        UpdateCrouchIntent(CurrentMode, *FoundZeroInputs);
    }

    Super::OnMoverPreSimulationTick(TimeStep, InputCmd);

    if (!FoundDefaults || !FoundZeroInputs) return;

    if (FoundZeroInputs->bIsRooted)
    {
        HandleRooted(TimeStep, CurrentMode);
        HandleTeleportInput(*FoundZeroInputs);
        return;
    }

    if (SimStartState.bWasRooted && CurrentMode == RootedModeName)
    {
        QueueNextMode(RootedExitModeName);
    }

    // Before the cube: the ball's capsule stays while cubed inside the ball.
    UpdateGooBallShape(FoundZeroInputs->bIsGooBall);

    if (FoundZeroInputs->bIsCubed)
    {
        HandleCubed(*FoundZeroInputs, CurrentMode);
        return;
    }
    if (FoundZeroInputs->bIsGooBall)
    {
        // Also takes over from the Cubed mode when a cube ends while the ball still has time.
        HandleGooBall(*FoundDefaults, *FoundZeroInputs, CurrentMode);
        return;
    }
    if (CurrentMode == CubedModeName)
    {
        // The cube is gone: carry on with the same velocity.
        QueueNextMode(CubedExitModeName);
    }
    if (CurrentMode == GooBallModeName)
    {
        QueueNextMode(GooBallExitModeName);
    }

    const bool bSliding = CurrentMode == TEXT("Sliding");
    bPendingSliding = bSliding;

    // Air dash counter lives in the sync state (rolls back) and refills on the ground.
    const bool bOnGround = CurrentMode == DefaultModeNames::Walking || bSliding;
    if (bOnGround)
    {
        PendingAirDashesUsed = 0;
    }

    // No jump, dash or mantle during a melee lunge. During a dash, jump presses do nothing (no jump, no stamina).
    const bool bInMelee = CurrentMode == MeleeModeName;
    bool bDashing = PendingDashRemainingMs > 0.0f;

    // A mantle consumes the jump (and dash) this frame, but the latched ability moves below still apply.
    const bool bMantled = !bInMelee && !bDashing && FoundZeroInputs->bJumpHold && TryMantle(*FoundDefaults, *FoundZeroInputs);
    if (!bInMelee && !bMantled)
    {
        // Check the air dash limit before spending stamina so a refused dash is free.
        const bool bDashAllowed = bOnGround || MaxAirDashes < 0 || PendingAirDashesUsed < MaxAirDashes;
        if (FoundZeroInputs->bWantsToDash && bDashAllowed && TrySpendStamina(DashStaminaCost))
        {
            if (!bOnGround)
            {
                ++PendingAirDashesUsed;
            }
            PendingDashRemainingMs = HandleDashInputs(*FoundDefaults, *FoundZeroInputs, CurrentMode);
            bDashing = true;
        }
    }

    // Always runs so the jump counter still resets on the ground.
    HandleAirJumpTracking(CurrentMode, *FoundZeroInputs, !bInMelee && !bMantled && !bDashing);

    // After the dash, so a dash started this frame suppresses the slide-exit ease.
    HandleSlideExit(CurrentMode, *FoundDefaults);

    if (FoundZeroInputs->bHasAbilityMove)
    {
        if (bOnGround)
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
        if (bOnGround)
        {
            QueueNextMode(DefaultModeNames::Falling);
        }

        TSharedPtr<FLayeredMove_MoveToDynamic> DynamicMove = MakeShared<FLayeredMove_MoveToDynamic>();
        DynamicMove->LocationActor = FoundZeroInputs->DynamicTargetActor;
        DynamicMove->StartLocation = GetSimStartLocation();
        DynamicMove->DurationMs = FoundZeroInputs->DynamicMoveDuration * 1000.0f;
        DynamicMove->MixMode = EMoveMixMode::OverrideVelocity;

        QueueLayeredMove(DynamicMove);
    }
    if (FoundZeroInputs->bWantsRelease && CurrentMode == FName("Locked"))
    {
        QueueNextMode(DefaultModeNames::Falling);
    }

    if (FoundZeroInputs->bWantsStop)
    {
        TSharedPtr<FLayeredMove_LinearVelocity> StopMove = MakeShared<FLayeredMove_LinearVelocity>();
        StopMove->Velocity = FVector::ZeroVector;
        StopMove->DurationMs = 100.f;
        StopMove->MixMode = EMoveMixMode::OverrideVelocity;
        QueueLayeredMove(StopMove);
    }

    if (FoundZeroInputs->bHasPullMove)
    {
        if (bOnGround || CurrentMode == FName("Locked"))
        {
            QueueNextMode(DefaultModeNames::Falling);
        }

        TSharedPtr<FLayeredMove_MoveTo> Pull = MakeShared<FLayeredMove_MoveTo>();
        Pull->StartLocation  = GetSimStartLocation();
        Pull->TargetLocation = FoundZeroInputs->PullTarget;
        Pull->DurationMs     = FoundZeroInputs->PullDuration * 1000.f;
        Pull->MixMode        = EMoveMixMode::OverrideVelocity;
        QueueLayeredMove(Pull);
    }

    HandleTeleportInput(*FoundZeroInputs);
}

void UZeroMoverComponent::BeginSimStep(const FMoverTimeStep& TimeStep)
{
    // The TimeStep handed to the pre/post sim hooks never has bIsResimulating set, so track it here:
    // a step at or before the newest one we've already simulated is a replay after a correction.
    bStepIsResimulating = TimeStep.BaseSimTimeMs <= NewestSimTimeMs;
    StepSeconds = TimeStep.StepMs * 0.001f;

    PendingStaminaSpend = 0.0f;
    PendingAirJumpsUsed = SimStartState.AirJumpsUsed;
    PendingAirDashesUsed = SimStartState.AirDashesUsed;
    PendingDashRemainingMs = FMath::Max(0.0f, SimStartState.DashRemainingMs - TimeStep.StepMs);
    bPendingRooted = false;
    bPendingSliding = false;

    StepMaxStamina = DefaultMaxStamina;
    StepStaminaRegenRate = DefaultStaminaRegenRate;
    if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
    {
        if (ASC->HasAttributeSetForAttribute(UBaseCharAttributeSet::GetMaxStaminaAttribute()))
        {
            StepMaxStamina = ASC->GetNumericAttribute(UBaseCharAttributeSet::GetMaxStaminaAttribute());
            StepStaminaRegenRate = ASC->GetNumericAttribute(UBaseCharAttributeSet::GetStaminaRegenRateAttribute());
        }
    }
}

void UZeroMoverComponent::HandlePostMovement(const FMoverTimeStep& TimeStep, FMoverSyncState& SyncState, FMoverAuxStateContext& AuxState)
{
    FZeroMoverSimState& OutSimState = SyncState.SyncStateCollection.FindOrAddMutableDataByType<FZeroMoverSimState>();

    float NewStamina = bPendingStaminaRefill ? StepMaxStamina : GetStartStamina() - PendingStaminaSpend;
    bPendingStaminaRefill = false;
    NewStamina = FMath::Clamp(NewStamina + StepStaminaRegenRate * StepSeconds, 0.0f, StepMaxStamina);

    OutSimState.Stamina = NewStamina;
    OutSimState.AirJumpsUsed = PendingAirJumpsUsed;
    OutSimState.AirDashesUsed = PendingAirDashesUsed;
    OutSimState.DashRemainingMs = PendingDashRemainingMs;
    OutSimState.bWasRooted = bPendingRooted;
    OutSimState.bWasSliding = bPendingSliding;

    CacheSimStartState(SyncState);
    NewestSimTimeMs = FMath::Max(NewestSimTimeMs, TimeStep.BaseSimTimeMs);

    if (!bStepIsResimulating)
    {
        OnStaminaChanged.Broadcast(NewStamina, StepMaxStamina);
    }
}

void UZeroMoverComponent::HandleSimRollback(const FMoverTimeStep& NewBaseTimeStep, const FMoverTimeStep& ExpungedTimeStep)
{
    // The restored frame is now the cached sync state; the next steps resimulate from it.
    CacheSimStartState(GetSyncState());
}

void UZeroMoverComponent::CacheSimStartState(const FMoverSyncState& SyncState)
{
    if (const FZeroMoverSimState* SimState = SyncState.SyncStateCollection.FindDataByType<FZeroMoverSimState>())
    {
        SimStartState = *SimState;
    }
    if (const FMoverDefaultSyncState* DefaultState = SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
    {
        SimStartLocation = DefaultState->GetLocation_WorldSpace();
        SimStartVelocity = DefaultState->GetVelocity_WorldSpace();
        bHasSimStartCache = true;
    }
}

FVector UZeroMoverComponent::GetSimStartVelocity() const
{
    if (bHasSimStartCache)
    {
        return SimStartVelocity;
    }
    const FMoverDefaultSyncState* Sync = GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
    return Sync ? Sync->GetVelocity_WorldSpace() : FVector::ZeroVector;
}

FVector UZeroMoverComponent::GetSimStartLocation() const
{
    if (bHasSimStartCache)
    {
        return SimStartLocation;
    }
    return UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
}

float UZeroMoverComponent::GetStartStamina() const
{
    // Negative means the sim state was never filled (first frame): start full.
    return SimStartState.Stamina < 0.0f ? StepMaxStamina : SimStartState.Stamina;
}

bool UZeroMoverComponent::TrySpendStamina(float Cost)
{
    if (!bUseStamina || Cost <= 0.0f)
    {
        return true;
    }
    if (GetStartStamina() - PendingStaminaSpend + KINDA_SMALL_NUMBER < Cost)
    {
        return false;
    }
    PendingStaminaSpend += Cost;
    return true;
}

float UZeroMoverComponent::GetPredictedStamina() const
{
    return SimStartState.Stamina < 0.0f ? GetMaxStamina() : SimStartState.Stamina;
}

float UZeroMoverComponent::GetMaxStamina() const
{
    return StepMaxStamina > 0.0f ? StepMaxStamina : DefaultMaxStamina;
}

bool UZeroMoverComponent::IsRooted() const
{
    const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
    return ASC && ASC->HasMatchingGameplayTag(ZerolockGameplayTagsForBinding::TAG_MOVEMENT_ROOTED);
}

bool UZeroMoverComponent::IsStunned() const
{
    const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
    return ASC && ASC->HasMatchingGameplayTag(ZerolockGameplayTagsForBinding::TAG_STUN);
}

bool UZeroMoverComponent::IsGooBall() const
{
    const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
    return ASC && ASC->HasMatchingGameplayTag(ZerolockGameplayTagsForBinding::TAG_STATUS_GOOBALL);
}

bool UZeroMoverComponent::IsCubed() const
{
    const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
    return ASC && ASC->HasMatchingGameplayTag(ZerolockGameplayTagsForBinding::TAG_STATUS_CUBED);
}

float UZeroMoverComponent::GetMoveSpeedMultiplier() const
{
    const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
    bool bFound = false;
    const float Multiplier = ASC ? ASC->GetGameplayAttributeValue(UBaseCharAttributeSet::GetMoveSpeedMultiplierAttribute(), bFound) : 1.0f;
    return bFound && Multiplier > 0.0f ? FMath::Clamp(Multiplier, 0.1f, 1.0f) : 1.0f;
}

void UZeroMoverComponent::HandleRooted(const FMoverTimeStep& TimeStep, const FName& CurrentMode)
{
    bPendingRooted = true;

    if (CurrentMode != RootedModeName)
    {
        QueueNextMode(RootedModeName);
    }

    // Kill any momentum right away; the rooted mode ignores input, so the pawn stays put once it's at zero.
    if (!GetSimStartVelocity().IsNearlyZero())
    {
        TSharedPtr<FLayeredMove_LinearVelocity> StopMove = MakeShared<FLayeredMove_LinearVelocity>();
        StopMove->Velocity = FVector::ZeroVector;
        StopMove->DurationMs = TimeStep.StepMs;
        StopMove->MixMode = EMoveMixMode::OverrideVelocity;
        QueueLayeredMove(StopMove);
    }
}

void UZeroMoverComponent::HandleCubed(const FZeroMovementInputs& ZeroInputs, const FName& CurrentMode)
{
    if (CurrentMode != CubedModeName)
    {
        QueueNextMode(CubedModeName);
    }
    HandleForcedMovesAsImpulses(ZeroInputs);
}

void UZeroMoverComponent::HandleForcedMovesAsImpulses(const FZeroMovementInputs& ZeroInputs)
{
    // Pushes (melee, Puddle Punch, knockbacks) add to the momentum once instead of overriding it for a while.
    if (ZeroInputs.bHasAbilityMove)
    {
        TSharedPtr<FApplyVelocityEffect> Push = MakeShared<FApplyVelocityEffect>();
        Push->VelocityToApply = ZeroInputs.AbilityVelocity;
        Push->bAdditiveVelocity = true;
        QueueInstantMovementEffect(Push);
    }

    if (ZeroInputs.bWantsStop)
    {
        TSharedPtr<FLayeredMove_LinearVelocity> StopMove = MakeShared<FLayeredMove_LinearVelocity>();
        StopMove->Velocity = FVector::ZeroVector;
        StopMove->DurationMs = 100.f;
        StopMove->MixMode = EMoveMixMode::OverrideVelocity;
        QueueLayeredMove(StopMove);
    }

    if (ZeroInputs.bHasPullMove)
    {
        TSharedPtr<FLayeredMove_MoveTo> Pull = MakeShared<FLayeredMove_MoveTo>();
        Pull->StartLocation  = GetSimStartLocation();
        Pull->TargetLocation = ZeroInputs.PullTarget;
        Pull->DurationMs     = ZeroInputs.PullDuration * 1000.f;
        Pull->MixMode        = EMoveMixMode::OverrideVelocity;
        QueueLayeredMove(Pull);
    }

    HandleTeleportInput(ZeroInputs);
}

void UZeroMoverComponent::UpdateGooBallShape(bool bWantsBall)
{
    const FZeroGooBallShapeModifier* Shape = static_cast<const FZeroGooBallShapeModifier*>(FindMovementModifier(GooBallShapeHandle));
    // The handle isn't rolled back; after a correction find the modifier by type.
    if (!Shape)
    {
        Shape = FindMovementModifierByType<FZeroGooBallShapeModifier>();
    }

    if (bWantsBall && !Shape)
    {
        TSharedPtr<FZeroGooBallShapeModifier> NewShape = MakeShared<FZeroGooBallShapeModifier>();
        NewShape->BallRadius = GooBallRadius;
        GooBallShapeHandle = QueueMovementModifier(NewShape);
    }
    else if (!bWantsBall && Shape)
    {
        CancelModifierFromHandle(Shape->GetHandle());
        GooBallShapeHandle.Invalidate();
    }
}

void UZeroMoverComponent::HandleGooBall(const FCharacterDefaultInputs& DefaultInputs, const FZeroMovementInputs& ZeroInputs, const FName& CurrentMode)
{
    if (CurrentMode != GooBallModeName)
    {
        QueueNextMode(GooBallModeName);
    }

    // Jumps: ground + air, counted in the rolled-back sim state (shares the air jump counter), reset on the ground.
    bool bWalkable = false;
    FFloorCheckResult Floor;
    UFloorQueryUtils::TryFindFloor(this, bWalkable, Floor);
    const bool bGrounded = Floor.bBlockingHit && Floor.FloorDist <= 6.0f && Floor.HitResult.ImpactNormal.Z >= 0.5f;
    if (bGrounded)
    {
        PendingAirJumpsUsed = 0;
    }
    if (ZeroInputs.bCustomJumpJustPressed && PendingAirJumpsUsed < GooBallMaxJumps)
    {
        // A jump sets the upward speed (an air jump also cancels the fall).
        const float CurrentUp = GetSimStartVelocity().Z;
        TSharedPtr<FApplyVelocityEffect> Jump = MakeShared<FApplyVelocityEffect>();
        Jump->VelocityToApply = FVector(0.0f, 0.0f, GooBallJumpSpeed - FMath::Min(CurrentUp, 0.0f) - FMath::Max(CurrentUp, 0.0f) * 0.5f);
        Jump->bAdditiveVelocity = true;
        QueueInstantMovementEffect(Jump);
        // Leaving the ground counts the ground jump; jumping in the air without it counts both, like the normal air jump.
        PendingAirJumpsUsed = bGrounded ? 1 : FMath::Max(PendingAirJumpsUsed, 1) + 1;
    }

    HandleForcedMovesAsImpulses(ZeroInputs);
}

void UZeroMoverComponent::HandleTeleportInput(const FZeroMovementInputs& ZeroInputs)
{
    if (!ZeroInputs.bHasTeleport)
    {
        return;
    }

    // The target comes from the client's input, so reject anything out of range unless the server authorized that exact target.
    if (FVector::DistSquared(GetSimStartLocation(), ZeroInputs.TeleportTarget) <= FMath::Square(MaxTeleportDistance)
        || IsAuthorizedLongTeleport(ZeroInputs.TeleportTarget))
    {
        TSharedPtr<FTeleportEffect> Teleport;
        if (ZeroInputs.bTeleportSetsVelocity)
        {
            TSharedPtr<FZeroTeleportWithVelocityEffect> PortalTeleport = MakeShared<FZeroTeleportWithVelocityEffect>();
            PortalTeleport->Velocity = ZeroInputs.TeleportVelocity;
            Teleport = PortalTeleport;
        }
        else
        {
            Teleport = MakeShared<FTeleportEffect>();
        }
        Teleport->TargetLocation = ZeroInputs.TeleportTarget;
        Teleport->bUseActorRotation = true;
        QueueInstantMovementEffect(Teleport);
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

void UZeroMoverComponent::HandleAirJumpTracking(const FName& CurrentMode, const FZeroMovementInputs& ZeroInputs, bool bAllowJump)
{
    // The ground jump counts toward MaxAirJumps; the counter lives in the sync state so it rolls back.
    int32 JumpsUsed = SimStartState.AirJumpsUsed;
    if (CurrentMode == DefaultModeNames::Walking || CurrentMode == TEXT("Sliding"))
    {
        JumpsUsed = 0;
    }
    PendingAirJumpsUsed = JumpsUsed;

    if (!bAllowJump)
    {
        return;
    }

    if (HandleWallBounceCheck(ZeroInputs, CurrentMode))
    {
        return;
    }

    const bool bIsAirJump = CurrentMode == DefaultModeNames::Falling;
    if (bIsAirJump)
    {
        // Walking off a ledge spends the ground jump, so every airtime gets the same number of air jumps.
        JumpsUsed = FMath::Max(JumpsUsed, 1);
    }

    if (!ZeroInputs.bCustomJumpJustPressed || JumpsUsed >= MaxAirJumps)
    {
        return;
    }

    if (bIsAirJump && !TrySpendStamina(AirJumpStaminaCost))
    {
        return;
    }

    // One-shot impulse. FLayeredMove_MultiJump stays active after the jump and performs its own air jumps on later
    // presses (bypassing the counter and stamina), which gave a free third jump.
    TSharedPtr<FJumpImpulseEffect> JumpImpulse = MakeShared<FJumpImpulseEffect>();
    JumpImpulse->UpwardsSpeed = VerticalJumpForce;

    if (bIsAirJump)
    {
        JumpImpulse->UpwardsSpeed = VerticalJumpForce * 1.5f;

        // Only spawn cosmetic effects on local clients during non-resimulated steps
        if (VFX_AirJump && GetWorld()->IsNetMode(NM_DedicatedServer) == false && !bStepIsResimulating)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFX_AirJump, UpdatedComponent->GetComponentLocation(), FRotator::ZeroRotator);
        }
    }

    QueueInstantMovementEffect(JumpImpulse);
    PendingAirJumpsUsed = JumpsUsed + 1;
}

void UZeroMoverComponent::UpdateCrouchIntent(const FName& CurrentMode, const FZeroMovementInputs& ZeroInputs)
{
    // Derived only from this frame's mode and input, so a resimulated frame makes the same stance decision as the original.
    const bool bOnGround = CurrentMode == DefaultModeNames::Walking || CurrentMode == TEXT("Sliding");
    if (bOnGround && ZeroInputs.bWantsToCrouch && !ZeroInputs.bCustomJumpJustPressed)
    {
        Crouch();
    }
    else
    {
        UnCrouch();
    }
}


void UZeroMoverComponent::RequestSafePullTo(const FVector& Target, float Duration)
{
    if (ShouldIgnoreServerLatch()) return;
    bLatchedPullMove = true;
    LatchedPullTarget = Target;
    LatchedPullDuration = Duration;
}

void UZeroMoverComponent::RequestSafeStop()
{
    if (ShouldIgnoreServerLatch()) return;
    bLatchedStop = true;
}

void UZeroMoverComponent::RequestSafeRelease()
{
    if (ShouldIgnoreServerLatch()) return;
    bLatchedRelease = true;
}

void UZeroMoverComponent::RequestSafeTeleport(const FVector& Target)
{
    if (ShouldIgnoreServerLatch()) return;
    bLatchedTeleport = true;
    LatchedTeleportTarget = Target;
    bLatchedTeleportSetsVelocity = false;
}

void UZeroMoverComponent::RequestSafeTeleportWithVelocity(const FVector& Target, const FVector& Velocity)
{
    if (ShouldIgnoreServerLatch()) return;
    bLatchedTeleport = true;
    LatchedTeleportTarget = Target;
    bLatchedTeleportSetsVelocity = true;
    LatchedTeleportVelocity = Velocity;
}

void UZeroMoverComponent::AuthorizeLongTeleport(const FVector& Target, float ValidFor)
{
    if (const UWorld* World = GetWorld())
    {
        AuthorizedTeleportTarget = Target;
        AuthorizedTeleportExpireTime = World->GetTimeSeconds() + ValidFor;
    }
}

bool UZeroMoverComponent::IsAuthorizedLongTeleport(const FVector& Target) const
{
    const UWorld* World = GetWorld();
    return World && World->GetTimeSeconds() <= AuthorizedTeleportExpireTime
        && FVector::DistSquared(Target, AuthorizedTeleportTarget) <= FMath::Square(AuthorizedTeleportTolerance);
}

bool UZeroMoverComponent::ShouldIgnoreServerLatch() const
{
    const APawn* P = Cast<APawn>(GetOwner());
    return P && !P->IsLocallyControlled() && P->GetRemoteRole() == ROLE_AutonomousProxy;
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
        FVector CurrentVelocity = GetSimStartVelocity();

        FVector WallNormal = Hit.Normal;

        if (FVector::DotProduct(CurrentVelocity, WallNormal) < 0.0f)
        {
            CurrentVelocity = FVector::VectorPlaneProject(CurrentVelocity, WallNormal);
        }

        FVector LaunchVelocity = CurrentVelocity;
        LaunchVelocity += WallNormal * WallJumpOffForce;
        LaunchVelocity.Z = WallJumpVerticalForce;

        if (VFX_WallBounce && GetWorld()->IsNetMode(NM_DedicatedServer) == false && !bStepIsResimulating)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFX_WallBounce, Hit.Location, Hit.Normal.Rotation());
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




float UZeroMoverComponent::HandleDashInputs(const FCharacterDefaultInputs& DefaultInputs, const FZeroMovementInputs& ZeroInputs, const FName& CurrentMode)
{
    const bool bSliding = CurrentMode == TEXT("Sliding");
    const bool bOnGround = bSliding || CurrentMode == DefaultModeNames::Walking;

    TSharedPtr<FZeroLayeredMove_Dash> DashMove = MakeShared<FZeroLayeredMove_Dash>();

    FVector MoveIntent = DefaultInputs.GetMoveInput();
    FVector DashDirection = MoveIntent.IsNearlyZero() ? ZeroInputs.LookDir.Vector().GetSafeNormal2D() : MoveIntent.GetSafeNormal();

    if (DashDirection.IsNearlyZero())
    {
        // Looking straight up/down: use the look yaw (from input, so client and server agree).
        DashDirection = FRotator(0.0f, ZeroInputs.LookDir.Yaw, 0.0f).Vector();
    }

    if (GetWorld()->IsNetMode(NM_DedicatedServer) == false && !bStepIsResimulating)
    {
        ++DashCount;
        LastDashDirection = DashDirection;
        PlayDirectionalDashMontage(DashDirection);
    }

    DashMove->Velocity = DashDirection * DashSpeed;
    DashMove->DurationMs = DashDuration * 1000.0f;

    // Walking brakes anything above MaxSpeed almost instantly (friction 8 x2, decel 4000), so a ground dash used to stop
    // dead after DashDuration. Ease it down to MaxSpeed instead. In the air, Falling barely decelerates, so no ease-out.
    if (bOnGround && GroundDashEaseOutDuration > 0.0f)
    {
        const UCommonLegacyMovementSettings* LegacySettings = FindSharedSettings<UCommonLegacyMovementSettings>();
        DashMove->EaseOutMs = GroundDashEaseOutDuration * 1000.0f;
        DashMove->ExitSpeed = LegacySettings ? LegacySettings->MaxSpeed : 0.0f;
        if (bSliding)
        {
            // Sliding has no speed cap, so hand back at least the slide speed we came in with.
            DashMove->ExitSpeed = FMath::Max(DashMove->ExitSpeed, GetSimStartVelocity().Size2D());
        }
        DashMove->DurationMs += DashMove->EaseOutMs;
    }

    QueueLayeredMove(DashMove);
    return DashMove->DurationMs;
}

void UZeroMoverComponent::HandleSlideExit(const FName& CurrentMode, const FCharacterDefaultInputs& DefaultInputs)
{
    // A dash already eases itself out; don't stack a second velocity override on it.
    if (!SimStartState.bWasSliding || CurrentMode != DefaultModeNames::Walking || PendingDashRemainingMs > 0.0f)
    {
        return;
    }

    const UCommonLegacyMovementSettings* LegacySettings = FindSharedSettings<UCommonLegacyMovementSettings>();
    const FVector Velocity2D(GetSimStartVelocity().X, GetSimStartVelocity().Y, 0.0f);
    const float Speed2D = Velocity2D.Size();
    if (!LegacySettings || Speed2D <= UE_KINDA_SMALL_NUMBER)
    {
        return;
    }

    // Releasing crouch without steering: Walking with no input brakes to zero in ~0.1 s (friction 8 x2, decel 4000), which
    // felt like a dead stop. Brake at a constant rate instead; steering (once at walking speed) or jumping hands back control.
    if (DefaultInputs.GetMoveInput().IsNearlyZero() && SlideExitBrakingDeceleration > 0.0f)
    {
        TSharedPtr<FZeroLayeredMove_Dash> BrakeMove = MakeShared<FZeroLayeredMove_Dash>();
        BrakeMove->Velocity = Velocity2D;
        BrakeMove->DurationMs = Speed2D / SlideExitBrakingDeceleration * 1000.0f;
        BrakeMove->EaseOutMs = BrakeMove->DurationMs;
        BrakeMove->ExitSpeed = 0.0f;
        BrakeMove->CancelOnMoveInputBelowSpeed = LegacySettings->MaxSpeed;
        // Vertical velocity stays with the mode, so jumping out of the brake still works.
        BrakeMove->MixMode = EMoveMixMode::OverrideAllExceptVerticalVelocity;
        QueueLayeredMove(BrakeMove);
        return;
    }

    // With move input Walking keeps anything up to MaxSpeed on its own.
    if (Speed2D <= LegacySettings->MaxSpeed || SlideExitEaseOutDuration <= 0.0f)
    {
        return;
    }

    TSharedPtr<FZeroLayeredMove_Dash> EaseMove = MakeShared<FZeroLayeredMove_Dash>();
    EaseMove->Velocity = Velocity2D;
    EaseMove->DurationMs = SlideExitEaseOutDuration * 1000.0f;
    EaseMove->EaseOutMs = EaseMove->DurationMs;
    EaseMove->ExitSpeed = LegacySettings->MaxSpeed;
    // Vertical velocity stays with the mode, so jumping out of the ease still works.
    EaseMove->MixMode = EMoveMixMode::OverrideAllExceptVerticalVelocity;
    QueueLayeredMove(EaseMove);
}

bool UZeroMoverComponent::TryMantle(const FCharacterDefaultInputs& DefaultInputs, const FZeroMovementInputs& ZeroInputs)
{
    FVector MoveIntent = DefaultInputs.GetMoveInput();

    // Facing comes from the input's look yaw: the actor's rotation is driven by the controller outside the
    // simulation, so the server's copy can be tens of degrees off from the client's.
    const FVector Fwd = FRotator(0.0f, ZeroInputs.LookDir.Yaw, 0.0f).Vector();

    if (MoveIntent.IsNearlyZero() || FVector::DotProduct(MoveIntent.GetSafeNormal(), Fwd) < 0.5f)
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

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());

    float MaxHeight = (CapHH * 2.f) + MantleReachHeight;
    float CosMMWSA = FMath::Cos(FMath::DegreesToRadians(MantleMinWallSteepnessAngle));
    float CosMMSA = FMath::Cos(FMath::DegreesToRadians(MantleMaxSurfaceAngle));
    float CosMMAA = FMath::Cos(FMath::DegreesToRadians(MantleMaxAlignmentAngle));

    FHitResult FrontHit;
    FVector CurrentVelocity = GetSimStartVelocity();
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
    if (ShouldIgnoreServerLatch()) return;
    bLatchedAbilityMove = true;
    LatchedAbilityVelocity = Velocity;
    LatchedAbilityDuration = Duration;
}

void UZeroMoverComponent::RequestSafeDynamicAbilityMove(AActor* TargetActor, float Duration)
{
    if (ShouldIgnoreServerLatch()) return;
    bLatchedDynamicMove = true;
    LatchedDynamicActor = TargetActor;
    LatchedDynamicDuration = Duration;
}
