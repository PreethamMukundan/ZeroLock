// Copyright Preetham Mukundan (C) 2026

#include "Mover/ZeroMoverPawn.h"
#include "Mover/ZeroMovementData.h" 
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "Chaos/ChaosPerfTest.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/Settings/StanceSettings.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/KeybindManagerSubsystem.h"
#include "Mover/ZeroMoverComponent.h"

AZeroMoverPawn::AZeroMoverPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicatingMovement(false);

    CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
    CapsuleComponent->InitCapsuleSize(34.0f, 88.0f);
    CapsuleComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
    CapsuleComponent->bDynamicObstacle = true;
    RootComponent = CapsuleComponent;

    MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(RootComponent);
    MeshComponent->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -88.0f), FRotator(0.0f, -90.0f, 0.0f));
    
    // Instantiating our newly renamed Custom Zero Mover Component
    MoverComponent = CreateDefaultSubobject<UZeroMoverComponent>(TEXT("MoverComponent"));
    MoverComponent->SetIsReplicated(true);
  
 
    SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
    SpringArmComponent->SetupAttachment(RootComponent);
    SpringArmComponent->TargetArmLength = 400.0f; 
    SpringArmComponent->bUsePawnControlRotation = true; 

    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
    CameraComponent->SetupAttachment(SpringArmComponent, USpringArmComponent::SocketName);
    CameraComponent->bUsePawnControlRotation = false; 
}

void AZeroMoverPawn::BeginPlay()
{
    Super::BeginPlay();
    
    if (MoverComponent)
    {
        MoverComponent->OnStanceChanged.AddUniqueDynamic(this, &AZeroMoverPawn::OnMoverStanceChanged);
        MoverComponent->OnMovementModeChanged.AddUniqueDynamic(this, &AZeroMoverPawn::OnMovementModeChanged);
    }
}

void AZeroMoverPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateSmoothedCamera();
}

void AZeroMoverPawn::UpdateSmoothedCamera()
{
    if (!SpringArmComponent || !MeshComponent || !MoverComponent || !IsLocallyControlled())
    {
        return;
    }
    if (!bHasSpringArmBaseLocation)
    {
        SpringArmBaseLocation = SpringArmComponent->GetRelativeLocation();
        bHasSpringArmBaseLocation = true;
    }

    /* The sim moves the capsule in fixed 60 Hz steps; Mover's VisualComponentOffset smoothing (applied before actor ticks)
     * moves only the mesh between them. Shift the camera boom by the same offset, or the camera steps with the capsule
     * and the smoothly moving hero shakes on screen. The spring arm ticks after us (PostPhysics). */
    FVector Offset = FVector::ZeroVector;
    if (MoverComponent->SmoothingMode == EMoverSmoothingMode::VisualComponentOffset && MoverComponent->GetPrimaryVisualComponent() == MeshComponent)
    {
        const FTransform& ActorTransform = GetActorTransform();
        const FVector UnsmoothedMeshLocation = ActorTransform.TransformPosition(MoverComponent->GetBaseVisualComponentTransform().GetLocation());
        Offset = ActorTransform.InverseTransformVector(MeshComponent->GetComponentLocation() - UnsmoothedMeshLocation);
        // A teleport or big correction isn't smoothing: don't drag the camera across the map.
        if (Offset.SizeSquared() > FMath::Square(MaxSmoothedCameraOffset))
        {
            Offset = FVector::ZeroVector;
        }
    }
    SpringArmComponent->SetRelativeLocation(SpringArmBaseLocation + Offset);
}



void AZeroMoverPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }

        // Settings > KeyBinding lists the remappable mappings of this context (and applies saved remaps).
        if (UKeybindManagerSubsystem* Keybinds = ULocalPlayer::GetSubsystem<UKeybindManagerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            Keybinds->InitializeKeybinds(DefaultMappingContext);
        }
    }

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AZeroMoverPawn::OnMove);
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AZeroMoverPawn::OnMove);
        
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AZeroMoverPawn::OnJumpPressed);
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AZeroMoverPawn::OnJumpReleased);
        
        EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &AZeroMoverPawn::OnCrouchPressed);
        EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AZeroMoverPawn::OnCrouchReleased);
        
        EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started, this, &AZeroMoverPawn::OnDashPressed);
        
        EnhancedInputComponent->BindAction(MeleeAction, ETriggerEvent::Triggered, this, &AZeroMoverPawn::OnHeavyMeleePressed);
        EnhancedInputComponent->BindAction(ZiplineAction, ETriggerEvent::Triggered, this, &AZeroMoverPawn::OnZiplinePressed);
        
        
        EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AZeroMoverPawn::OnLook);
    }
}

void AZeroMoverPawn::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
   FCharacterDefaultInputs& DefaultInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

    /* Stunned (also while carried by Doorman's Luggage Cart): none of the player's own movement. Latched forced moves
     * (pulls, teleports) below still go through. Dropping the cached input also keeps keys released during the stun
     * from sticking (Stunned's DisableInput eats the release). */
    // Cubed (Viscous's Cube) too: the cube slides on its own; jump only asks to escape (AZeroLockCharacter).
    const bool bCubed = MoverComponent && MoverComponent->IsCubed();
    if (MoverComponent && (MoverComponent->IsStunned() || bCubed))
    {
        CachedMoveInput = FVector2D::ZeroVector;
        bLocalJumpPressed = false;
        bCachedWantsToCrouch = false;
        bWantsToDashLatch = false;
        bWantsToHeavyMelee = false;
        bWantsToZipline = false;
    }

    DefaultInputs.bIsJumpJustPressed = (!bWasJumpPressedLastFrame && bLocalJumpPressed);
    DefaultInputs.bIsJumpPressed = bLocalJumpPressed;
    bWasJumpPressedLastFrame = bLocalJumpPressed;

    FVector ControlVector = FVector::ZeroVector;
    if (Controller && !CachedMoveInput.IsZero())
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);

        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

        ControlVector = (ForwardDirection * CachedMoveInput.Y) + (RightDirection * CachedMoveInput.X);
        ControlVector.Normalize();
        // Slows: the Mover's desired speed is MaxSpeed × input length, so a shorter input walks slower (predicted with the input)
        if (MoverComponent)
        {
            ControlVector *= MoverComponent->GetMoveSpeedMultiplier();
        }
    }

    DefaultInputs.SetMoveInput(EMoveInputType::DirectionalIntent, ControlVector); 

    if (Controller)
    {
        DefaultInputs.ControlRotation = Controller->GetControlRotation();
        // Yaw only: the body turns with the camera but never pitches (looking straight up would leave no direction).
        DefaultInputs.OrientationIntent = FRotator(0.0f, DefaultInputs.ControlRotation.Yaw, 0.0f).Vector();
    }
    else
    {
        DefaultInputs.OrientationIntent = GetActorForwardVector();
    }

    FZeroMovementInputs& ZeroInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FZeroMovementInputs>();
    
    ZeroInputs.bWantsToCrouch = bCachedWantsToCrouch;
    ZeroInputs.bSlideIntentValid = bLocalSlideIntentValid;
    ZeroInputs.bJumpHold = bLocalJumpPressed;
    ZeroInputs.bCustomJumpJustPressed = DefaultInputs.bIsJumpJustPressed;
    ZeroInputs.bWantsToDash = bWantsToDashLatch;
    ZeroInputs.bWantsToMelee = bWantsToHeavyMelee;
    ZeroInputs.bWantsToZipline = bWantsToZipline;
    
    
    
    if (Controller)
    {
        ZeroInputs.LookDir = Controller->GetControlRotation();
    }
    ZeroInputs.bIsRooted = MoverComponent && MoverComponent->IsRooted();
    ZeroInputs.bIsCubed = bCubed;
    // Goo Ball: steer and jump only; no dash, crouch/slide, melee lunge or zipline.
    ZeroInputs.bIsGooBall = MoverComponent && MoverComponent->IsGooBall();
    if (ZeroInputs.bIsGooBall)
    {
        ZeroInputs.bWantsToCrouch = false;
        ZeroInputs.bWantsToDash = false;
        ZeroInputs.bWantsToMelee = false;
        ZeroInputs.bWantsToZipline = false;
    }

    if (MoverComponent && MoverComponent->bLatchedAbilityMove)
    {
        ZeroInputs.bHasAbilityMove = true;
        ZeroInputs.AbilityVelocity = MoverComponent->LatchedAbilityVelocity;
        ZeroInputs.AbilityMoveDuration = MoverComponent->LatchedAbilityDuration;
        MoverComponent->bLatchedAbilityMove = false;
    }
    else
    {
        ZeroInputs.bHasAbilityMove = false;
    }
    if (MoverComponent && MoverComponent->bLatchedDynamicMove)
    {
        ZeroInputs.bHasDynamicAbilityMove = true;
        ZeroInputs.DynamicTargetActor = MoverComponent->LatchedDynamicActor;
        ZeroInputs.DynamicMoveDuration = MoverComponent->LatchedDynamicDuration;
        MoverComponent->bLatchedDynamicMove = false;
    }
    else
    {
        ZeroInputs.bHasDynamicAbilityMove = false;
    }
    if (MoverComponent && MoverComponent->bLatchedPullMove)
    {
        ZeroInputs.bHasPullMove = true;
        ZeroInputs.PullTarget = MoverComponent->LatchedPullTarget;
        ZeroInputs.PullDuration = MoverComponent->LatchedPullDuration;
        MoverComponent->bLatchedPullMove = false;
    }
    else
    {
        ZeroInputs.bHasPullMove = false;
    }

    if (MoverComponent && MoverComponent->bLatchedStop)
    {
        ZeroInputs.bWantsStop = true;
        MoverComponent->bLatchedStop = false;
    }
    else
    {
        ZeroInputs.bWantsStop = false;
    }

    if (MoverComponent && MoverComponent->bLatchedRelease)
    {
        ZeroInputs.bWantsRelease = true;
        MoverComponent->bLatchedRelease = false;
    }
    else
    {
        ZeroInputs.bWantsRelease = false;
    }

    if (MoverComponent && MoverComponent->bLatchedTeleport)
    {
        ZeroInputs.bHasTeleport = true;
        ZeroInputs.TeleportTarget = MoverComponent->LatchedTeleportTarget;
        ZeroInputs.bTeleportSetsVelocity = MoverComponent->bLatchedTeleportSetsVelocity;
        ZeroInputs.TeleportVelocity = MoverComponent->LatchedTeleportVelocity;
        MoverComponent->bLatchedTeleport = false;
        MoverComponent->bLatchedTeleportSetsVelocity = false;
    }
    else
    {
        ZeroInputs.bHasTeleport = false;
        ZeroInputs.bTeleportSetsVelocity = false;
    }
    
    bWantsToDashLatch = false;
    bWantsToHeavyMelee = false;
    bWantsToZipline = false;
}

void AZeroMoverPawn::OnMove(const FInputActionValue& Value)
{
    CachedMoveInput = Value.Get<FVector2D>();
}
void AZeroMoverPawn::OnLook(const FInputActionValue& Value) 
{
    FVector2D LookAxisVector = Value.Get<FVector2D>();
    AddControllerYawInput(LookAxisVector.X);
    AddControllerPitchInput(LookAxisVector.Y);
}

void AZeroMoverPawn::OnJumpPressed()
{
    bJumpPressConsumed = false;
    OnJumpInputPressed.Broadcast();

    // A listener (e.g. an ability cancelled by jumping) can swallow the press so it never becomes a jump.
    if (!bJumpPressConsumed)
    {
        bLocalJumpPressed = true;
    }
}
void AZeroMoverPawn::OnJumpReleased()
{
    bLocalJumpPressed = false;
}
void AZeroMoverPawn::OnDashPressed()
{
    bWantsToDashLatch = true;
}

void AZeroMoverPawn::OnHeavyMeleePressed()
{
    bWantsToHeavyMelee = true;
    ++HeavyMeleeTriggerCount;
}

void AZeroMoverPawn::OnZiplinePressed()
{
    bWantsToZipline = true;
}

FRotator AZeroMoverPawn::GetSyncedAimRotation() const
{
 
        const FMoverSyncState& SyncState = MoverComponent->GetSyncState();
        
        if (const FZeroMovementInputs* ZeroInputs = SyncState.SyncStateCollection.FindDataByType<FZeroMovementInputs>())
        {
            return ZeroInputs->LookDir;
        }
    
    
    return GetActorRotation();
}

FVector2D AZeroMoverPawn::GetSyncedInput() const
{
    return CachedMoveInput;
}

FVector AZeroMoverPawn::GetInputWorldDir() const
{
  
    UZeroMoverComponent* MoverComp = GetZeroMoverComponent();
    if (!MoverComp)
    {
        return FVector::ZeroVector;
    }

    if (IsLocallyControlled())
    {
        FRotator ControlRot = GetControlRotation();
        FRotator YawRotation(0.0f, ControlRot.Yaw, 0.0f);

        FVector LocalInput(GetSyncedInput().Y, GetSyncedInput().X, 0.0f);
        FVector WorldDirection = ControlRot.RotateVector(LocalInput);
		
        return WorldDirection.GetSafeNormal();
    }
 
    if (HasAuthority())
    {
        const FMoverSyncState& SyncState = MoverComp->GetSyncState();
        if (const FCharacterDefaultInputs* DefaultInputs = SyncState.SyncStateCollection.FindDataByType<FCharacterDefaultInputs>())
        {
            FVector SyncedMoveDir = DefaultInputs->GetMoveInput();
            return SyncedMoveDir;
        }
    }

    return FVector::ZeroVector;
}

void AZeroMoverPawn::OnCrouchPressed()
{
    bCachedWantsToCrouch = true;
    //if (MoverComponent) MoverComponent->Crouch();
}

void AZeroMoverPawn::OnCrouchReleased()
{
    bCachedWantsToCrouch = false;
    bLocalSlideIntentValid = true;
   // if (MoverComponent) MoverComponent->UnCrouch();
}

void AZeroMoverPawn::OnMoverStanceChanged(EStanceMode OldStance, EStanceMode NewStance) 
{}
void AZeroMoverPawn::OnMovementModeChanged(const FName& PreviousMovementModeName, const FName& NewMovementModeName)
{}