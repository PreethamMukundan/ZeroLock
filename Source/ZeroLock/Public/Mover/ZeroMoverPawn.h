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

UCLASS()
class ZEROLOCK_API AZeroMoverPawn : public APawn, public IMoverInputProducerInterface
{
    GENERATED_BODY()

public:
    AZeroMoverPawn();

    UZeroMoverComponent* GetZeroMoverComponent() const { return MoverComponent; }
    USkeletalMeshComponent* GetSkeletalMeshComponent() const { return MeshComponent; }
    
    // --- Helper Functions to mimic ACharacter API ---
    FORCEINLINE USkeletalMeshComponent* GetMesh() const { return MeshComponent; }
    FORCEINLINE UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }
    FORCEINLINE UCameraComponent* GetFollowCamera() const { return CameraComponent; }
    FORCEINLINE USpringArmComponent* GetCameraBoom() const { return SpringArmComponent; }

protected:
    virtual void BeginPlay() override;
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

private:

    FVector2D CachedMoveInput = FVector2D::ZeroVector;
    bool bLocalJumpPressed = false;
    bool bWasJumpPressedLastFrame = false;
    bool bCachedWantsToCrouch = false;
    bool bLocalSlideIntentValid = false;
    bool bWantsToDashLatch = false;
    bool bWantsToHeavyMelee = false;
    bool bWantsToZipline = false;
};