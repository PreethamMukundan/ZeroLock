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



void AZeroMoverPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
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
    }

    DefaultInputs.SetMoveInput(EMoveInputType::DirectionalIntent, ControlVector); 

    if (Controller)
    {
        DefaultInputs.ControlRotation = Controller->GetControlRotation();
        DefaultInputs.OrientationIntent = DefaultInputs.ControlRotation.Vector().GetSafeNormal();
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
    bLocalJumpPressed = true;
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
}

void AZeroMoverPawn::OnZiplinePressed()
{
    bWantsToZipline = true;
}

FRotator AZeroMoverPawn::GetSyncedAimRotation() const
{
    if (IsLocallyControlled() && GetController())
    {
        return GetController()->GetControlRotation();
    }
    
    if (HasAuthority() && MoverComponent)
    {
        const FMoverSyncState& SyncState = MoverComponent->GetSyncState();
        
        if (const FZeroMovementInputs* ZeroInputs = SyncState.SyncStateCollection.FindDataByType<FZeroMovementInputs>())
        {
            return ZeroInputs->LookDir;
        }
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