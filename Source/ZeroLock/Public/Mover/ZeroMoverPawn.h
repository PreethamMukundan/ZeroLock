// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "MoverSimulationTypes.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoverTypes.h"
#include "Misc/MapErrors.h"
#include "ZeroMoverPawn.generated.h"

class UZeroMoverComponent;
class UCapsuleComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;

DECLARE_MULTICAST_DELEGATE(FOnJumpInputPressed);

UCLASS()
class ZEROLOCK_API AZeroMoverPawn : public APawn, public IMoverInputProducerInterface
{
    GENERATED_BODY()

public:
    AZeroMoverPawn();

    /** Broadcast on the locally controlled pawn when the jump input is pressed (e.g. so abilities can be cancelled by jumping). */
    FOnJumpInputPressed OnJumpInputPressed;

    /** Call from an OnJumpInputPressed listener to stop this press from reaching the mover (no jump, air jump or mantle). */
    void ConsumeJumpPress() { bJumpPressConsumed = true; }

    /** Clips used by ABP_DeadlockHero (UZL_DeadlockAnimInstance) for this pawn. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    TObjectPtr<class UZL_DeadlockAnimSet> DeadlockAnimSet;

    UZeroMoverComponent* GetZeroMoverComponent() const { return MoverComponent; }
    USkeletalMeshComponent* GetSkeletalMeshComponent() const { return MeshComponent; }
    
    // --- Helper Functions to mimic ACharacter API ---
    FORCEINLINE USkeletalMeshComponent* GetMesh() const { return MeshComponent; }
    FORCEINLINE UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }
    FORCEINLINE UCameraComponent* GetFollowCamera() const { return CameraComponent; }
    FORCEINLINE USpringArmComponent* GetCameraBoom() const { return SpringArmComponent; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCapsuleComponent> CapsuleComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USkeletalMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadwrite, Category = "Components")
    TObjectPtr<UZeroMoverComponent> MoverComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USpringArmComponent> SpringArmComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCameraComponent> CameraComponent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> CrouchAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> DashAction;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> MeleeAction;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> ZiplineAction;

    UFUNCTION()
    void OnMoverStanceChanged(EStanceMode OldStance, EStanceMode NewStance);

    UFUNCTION()
    void OnMovementModeChanged(const FName& PreviousMovementModeName, const FName& NewMovementModeName);

    void OnMove(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);
    void OnJumpPressed();
    void OnJumpReleased();
    void OnCrouchPressed();
    void OnCrouchReleased();
    void OnDashPressed();
    void OnHeavyMeleePressed();
    void OnZiplinePressed();
    
public:
    UFUNCTION(BlueprintCallable, Category = "Aiming")
    FRotator GetSyncedAimRotation() const;
    
    UFUNCTION(BlueprintCallable, Category = "Inputs")
    FVector2D GetSyncedInput() const;
    
    UFUNCTION(BlueprintCallable, Category = "Inputs")
    FVector GetInputWorldDir() const;

    /** Bumped each time the heavy melee hold triggers (local input). Light melee on the same key compares it across a
     * press to tell a tap from a hold (AZeroLockCharacter::LightMeleeReleased). */
    int32 GetHeavyMeleeTriggerCount() const { return HeavyMeleeTriggerCount; }

    /** Aim zoom (the default alt fire, UZL_GA_AimZoom). Local camera only: blends the camera FOV and the spring arm length
     * toward the zoomed values over BlendTime. FOVRelative is in Deadlock's 4:3 degrees (m_flZoomFOV_Relative, -40 = hero
     * FOV 90 → 50), converted to our 16:9 horizontal FOV. */
    void SetAimZoom(bool bZoom, float FOVRelative, float BlendTime);
    bool IsAimZooming() const { return bWantsAimZoom; }
    float GetAimZoomAlpha() const { return AimZoomAlpha; }

    /** Camera arm length while zoomed (Deadlock m_flCameraBackOffsetAiming × 1.778 cm). 0 = TargetArmLength × AimZoomArmLengthScale. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Aim Zoom")
    float AimZoomArmLength = 0.0f;

    /** Used when AimZoomArmLength is 0. Deadlock heroes zoom to 59–82% of the arm (Pocket 0.59, Solomon 0.82). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Aim Zoom", meta = (ClampMin = "0.1", ClampMax = "1.0"))
    float AimZoomArmLengthScale = 0.65f;

private:
    /** Blends FOV and arm length toward the aim zoom (locally controlled only). */
    void UpdateAimZoom(float DeltaSeconds);

    bool bWantsAimZoom = false;
    float AimZoomAlpha = 0.0f;
    float AimZoomFOVRelative = -40.0f;
    float AimZoomBlendTime = 0.15f;
    /** Camera FOV and arm length before the zoom started, restored once it has fully blended out. */
    bool bHasAimZoomBase = false;
    float AimZoomBaseFOV = 90.0f;
    float AimZoomBaseArmLength = 400.0f;


    /** Moves the camera boom with Mover's smoothed mesh offset (locally controlled only). */
    void UpdateSmoothedCamera();

    /** Smoothing offsets above this (cm) are teleports or corrections; the camera doesn't follow them. */
    static constexpr float MaxSmoothedCameraOffset = 150.0f;

    FVector SpringArmBaseLocation = FVector::ZeroVector;
    bool bHasSpringArmBaseLocation = false;

    int32 HeavyMeleeTriggerCount = 0;

    FVector2D CachedMoveInput = FVector2D::ZeroVector;
    bool bLocalJumpPressed = false;
    bool bJumpPressConsumed = false;
    bool bWasJumpPressedLastFrame = false;
    bool bCachedWantsToCrouch = false;
    bool bLocalSlideIntentValid = false;
    bool bWantsToDashLatch = false;
    bool bWantsToHeavyMelee = false;
    bool bWantsToZipline = false;
};