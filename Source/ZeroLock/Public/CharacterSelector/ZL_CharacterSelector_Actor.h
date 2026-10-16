//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CharacterSelector/ZL_Character_Data_Asset.h"
#include "ZL_CharacterSelector_Actor.generated.h"

class UPointLightComponent;
class UZl_CharacterSelectionVM;

UCLASS()
class ZEROLOCK_API AZL_CharacterSelector_Actor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AZL_CharacterSelector_Actor();

protected:
	UFUNCTION()
	void OnMousePosChanged(FVector2D NewMousePos);
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;


	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnViewModelFieldChanged(UZL_Character_Data_Asset* NewHero);


	// Components
	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* PreviewMesh;

	UPROPERTY(VisibleAnywhere)
	class USceneCaptureComponent2D* CaptureComp;

	UPROPERTY(VisibleAnywhere)
	class USpringArmComponent* SpringArm;
	FRotator TargetSpringArmRot;

	// Function to change mesh + animation at runtime. A null mesh clears the preview. PoseTime >= 0 freezes on that frame.
	void SetPreview(USkeletalMesh* NewMesh, class UAnimSequence* NewAnim, float PoseTime = -1.f);

	/** Spring arm rotation the mouse sway is applied around (from the current preview settings). */
	float BaseCameraYaw = -90.f;
	float BaseCameraPitch = 0.f;

	/** Moves the lights/camera and sets exposure for a hero (or the defaults). */
	void ApplyPreviewSettings(const FZL_HeroPreviewSettings& Settings, const FLinearColor& IconColor);

	// Preview-only lights: lighting channel 1, which only PreviewMesh receives, so level lights don't change the look per map.
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> RimLight;

	/** Used when no hero is shown or the hero's data asset doesn't override PreviewSettings. */
	UPROPERTY(EditAnywhere, Category = "Preview")
	FZL_HeroPreviewSettings DefaultPreviewSettings;


	UPROPERTY(EditAnywhere, Category = "Camera Control")
	float RotationIntensity = 5.0f; // Max degrees to rotate

	UPROPERTY(EditAnywhere, Category = "Camera Control")
	float InterpSpeed = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Camera Control")
	float Maxangle = 10.0f;

private:
	// Stored reference to the VM so we can access the data
	UPROPERTY(Transient)
	TObjectPtr<UZl_CharacterSelectionVM> BoundVM;
};
