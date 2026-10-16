//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ZL_Character_Data_Asset.generated.h"

class AZeroLockCharacter;

USTRUCT(BlueprintType)
struct FHeroTableRow : public FTableRowBase
{
	GENERATED_BODY()

	// Pick your DataAsset in the DataTable field
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UZL_Character_Data_Asset> HeroAsset;
};
/** Lighting and framing for the hero select preview (AZL_CharacterSelector_Actor). Light locations are relative to the preview mesh
 *  as seen with CameraYaw -90 (camera on +Y looking toward -Y); they rotate with CameraYaw so key/fill/rim keep their place
 *  relative to the camera. Light intensities are in candelas with manual exposure (EV 0). */
USTRUCT(BlueprintType)
struct FZL_HeroPreviewSettings
{
	GENERATED_BODY()

	/** Spring arm yaw the mouse sway centres on. -90: camera on +Y. 90: camera on -Y (Deadlock Apollo/Lash poses face -Y). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraYaw = -90.f;

	/** Spring arm pitch. Positive lowers the camera and looks up at the hero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraPitch = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ArmLength = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraHeight = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float FOV = 110.f;

	/** Exposure compensation in stops (+ brighter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ExposureBias = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	FLinearColor KeyColor = FLinearColor(1.f, 0.95f, 0.88f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	float KeyIntensity = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	FVector KeyLocation = FVector(120.f, 160.f, 190.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	FLinearColor FillColor = FLinearColor(0.75f, 0.85f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	float FillIntensity = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	FVector FillLocation = FVector(-140.f, 140.f, 110.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	FLinearColor RimColor = FLinearColor(1.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	float RimIntensity = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	FVector RimLocation = FVector(-40.f, -170.f, 210.f);

	/** Use the hero's IconColor for the rim light instead of RimColor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	bool bRimUsesIconColor = false;
};

/**
 *
 */
UCLASS()
class ZEROLOCK_API UZL_Character_Data_Asset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString CharacterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString CharacterDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AZeroLockCharacter> CharacterClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USkeletalMesh> DisplaySeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UAnimSequence> DisplayAnimation;

	/** Seconds into DisplayAnimation to freeze on (an action pose). Negative: loop the animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DisplayAnimationPoseTime = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FColor IconColor;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTexture2D> Icon;

	/** Off: the preview actor's DefaultPreviewSettings are used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
	bool bOverridePreviewSettings = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview", meta = (EditCondition = "bOverridePreviewSettings"))
	FZL_HeroPreviewSettings PreviewSettings;

	
};
