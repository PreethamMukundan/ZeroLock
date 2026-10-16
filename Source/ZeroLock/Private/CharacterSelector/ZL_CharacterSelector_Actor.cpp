//Copyright Preetham Mukundan (C) 2026


#include "CharacterSelector/ZL_CharacterSelector_Actor.h"

#include "CharacterSelector/ZL_CharacterSelectionSubsystem.h"
#include"CharacterSelector/ZL_CharacterSelectionVM.h"
#include "CharacterSelector/ZL_Character_Data_Asset.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "ZeroLock/ZeroLock.h"
#include "ZeroLock/ZeroLockCharacter.h"

// Sets default values
AZL_CharacterSelector_Actor::AZL_CharacterSelector_Actor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
	RootComponent = PreviewMesh;


	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	
	// Create capture component
	CaptureComp = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CaptureComp"));
	CaptureComp->SetupAttachment(SpringArm);

	// Fixed exposure: auto exposure on a nearly empty frame pumps per hero.
	CaptureComp->PostProcessSettings.bOverride_AutoExposureMethod = true;
	CaptureComp->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	CaptureComp->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	CaptureComp->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	CaptureComp->PostProcessSettings.bOverride_AutoExposureBias = true;
	CaptureComp->PostProcessSettings.AutoExposureBias = 0.f;
	CaptureComp->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
	CaptureComp->PostProcessSettings.DepthOfFieldFocalDistance = 0.f;

	// Only the preview lights (channel 1) light the preview mesh.
	PreviewMesh->LightingChannels.bChannel0 = false;
	PreviewMesh->LightingChannels.bChannel1 = true;

	auto MakeLight = [this](const TCHAR* Name)
	{
		UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(Name);
		Light->SetupAttachment(RootComponent);
		Light->LightingChannels.bChannel0 = false;
		Light->LightingChannels.bChannel1 = true;
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetAttenuationRadius(1000.f);
		Light->SetSourceRadius(20.f);
		Light->SetSoftSourceRadius(40.f);
		Light->bAffectsWorld = true;
		return Light;
	};
	KeyLight = MakeLight(TEXT("KeyLight"));
	FillLight = MakeLight(TEXT("FillLight"));
	FillLight->SetCastShadows(false);
	RimLight = MakeLight(TEXT("RimLight"));
}

void AZL_CharacterSelector_Actor::ApplyPreviewSettings(const FZL_HeroPreviewSettings& Settings, const FLinearColor& IconColor)
{
	BaseCameraYaw = Settings.CameraYaw;
	BaseCameraPitch = Settings.CameraPitch;
	TargetSpringArmRot = FRotator(BaseCameraPitch, BaseCameraYaw, 0.f);

	if (SpringArm)
	{
		SpringArm->TargetArmLength = Settings.ArmLength;
		SpringArm->bDoCollisionTest = false;
		FVector ArmLocation = SpringArm->GetRelativeLocation();
		ArmLocation.Z = Settings.CameraHeight;
		SpringArm->SetRelativeLocation(ArmLocation);
		// Snap instead of easing across the whole turn when the hero (and so the camera side) changes.
		SpringArm->SetRelativeRotation(TargetSpringArmRot);
	}

	// Light locations are authored for CameraYaw -90; turn them with the camera so the setup reads the same from any side.
	const FRotator LightRig(0.f, Settings.CameraYaw + 90.f, 0.f);
	if (CaptureComp)
	{
		CaptureComp->FOVAngle = Settings.FOV;
		CaptureComp->PostProcessSettings.AutoExposureBias = Settings.ExposureBias;
	}

	auto SetLight = [&LightRig](UPointLightComponent* Light, const FVector& Location, const FLinearColor& Color, float Intensity)
	{
		if (!Light) return;
		Light->SetRelativeLocation(LightRig.RotateVector(Location));
		Light->SetLightColor(Color);
		Light->SetIntensity(Intensity);
	};
	SetLight(KeyLight, Settings.KeyLocation, Settings.KeyColor, Settings.KeyIntensity);
	SetLight(FillLight, Settings.FillLocation, Settings.FillColor, Settings.FillIntensity);
	SetLight(RimLight, Settings.RimLocation, Settings.bRimUsesIconColor ? IconColor : Settings.RimColor, Settings.RimIntensity);


}

void AZL_CharacterSelector_Actor::OnMousePosChanged(FVector2D NewMousePos)
{
	float MaxYaw = 20.0f;
	float MaxPitch = 15.0f;
	
	float AlphaX = (NewMousePos.X ) ;
	AlphaX = FMath::Clamp(AlphaX, -1, 1);
	float AlphaY = (NewMousePos.Y - 0.5f) * 2.0f;
	float finalX = BaseCameraYaw +(AlphaX*MaxYaw);
	TargetSpringArmRot = FRotator(BaseCameraPitch, finalX , 0.0f);
}

// Called when the game starts or when spawned
void AZL_CharacterSelector_Actor::BeginPlay()
{
	Super::BeginPlay();
	CaptureComp->ShowOnlyComponent(PreviewMesh);

	// Start empty until a hero is hovered.
	SetPreview(nullptr, nullptr);
	ApplyPreviewSettings(DefaultPreviewSettings, FLinearColor::White);


	if (UGameInstance* GI = GetGameInstance())
	{
		if (auto* Subsystem = GI->GetSubsystem<UZL_CharacterSelectionSubsystem>())
		{
			BoundVM = Subsystem->SelectionVM;

			if (BoundVM)
			{
				BoundVM->OnCharacterChanged.AddDynamic(this,&AZL_CharacterSelector_Actor::OnViewModelFieldChanged);
				OnViewModelFieldChanged(BoundVM->CurrentCharacter);

				BoundVM->OnMousePosChanged.AddDynamic(this,&AZL_CharacterSelector_Actor::OnMousePosChanged);
			}
		}
	}
}

// Called every frame
void AZL_CharacterSelector_Actor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


	if (SpringArm)
	{
		FRotator CurrentRot = SpringArm->GetRelativeRotation();
		
		//FRotator SmoothedRot = FMath::RInterpTo(CurrentRot, TargetSpringArmRot, DeltaTime, 8.0f);
		FRotator SmoothedRot = UKismetMathLibrary::REase(CurrentRot,TargetSpringArmRot,0.1,true,EEasingFunc::CircularIn);
        
		SpringArm->SetRelativeRotation(SmoothedRot);
		//GEngine->AddOnScreenDebugMessage(10, 2.f ? 2.f : -1.f, FColor::Yellow, SmoothedRot.ToString());
	}
}

void AZL_CharacterSelector_Actor::OnViewModelFieldChanged(UZL_Character_Data_Asset* NewHero)
{
	if (!NewHero)
	{
		// Nothing hovered: no mesh, no name, no ability icons.
		SetPreview(nullptr, nullptr);
		ApplyPreviewSettings(DefaultPreviewSettings, FLinearColor::White);
		if (BoundVM)
		{
			BoundVM->SetCharacterName(FText::GetEmpty());
			BoundVM->SetAbility1(nullptr);
			BoundVM->SetAbility2(nullptr);
			BoundVM->SetAbility3(nullptr);
			BoundVM->SetAbility4(nullptr);
		}
		return;
	}

	{
		SetPreview(NewHero->DisplaySeletalMesh, NewHero->DisplayAnimation, NewHero->DisplayAnimationPoseTime);
		ApplyPreviewSettings(NewHero->bOverridePreviewSettings ? NewHero->PreviewSettings : DefaultPreviewSettings, FLinearColor(NewHero->IconColor));
		if (!BoundVM) return;
		BoundVM->SetCharacterName(FText::FromString(NewHero->CharacterName));
			// 2. Access the Class Default Object (CDO)
		BoundVM->SetAbility1(nullptr);
		BoundVM->SetAbility2(nullptr);
		BoundVM->SetAbility3(nullptr);
		BoundVM->SetAbility4(nullptr);
		BoundVM->SetColorSelected(NewHero->IconColor);
		if (!NewHero->CharacterClass) return;
		if (AZeroLockCharacter* DefaultChar =  NewHero->CharacterClass->GetDefaultObject<AZeroLockCharacter>())
		{
			if (DefaultChar->SecondryFireAbility)
			{
				if (UBaseGameplayAbility* secon = DefaultChar->SecondryFireAbility->GetDefaultObject<UBaseGameplayAbility>())
				{
				
					BoundVM->SetAbility1(secon->IconImage);

				}
			}
			if (DefaultChar->Ability_1)
			{
				if (UBaseGameplayAbility* secon = DefaultChar->Ability_1->GetDefaultObject<UBaseGameplayAbility>())
				{
			
					BoundVM->SetAbility2(secon->IconImage);
			
				}
			}
			if (DefaultChar->Ability_2)
			{
				if (UBaseGameplayAbility* secon = DefaultChar->Ability_2->GetDefaultObject<UBaseGameplayAbility>())
				{
				
					BoundVM->SetAbility3(secon->IconImage);
			
				}
			}
			if (DefaultChar->UltimateAbility)
			{
				if (UBaseGameplayAbility* secon = DefaultChar->UltimateAbility->GetDefaultObject<UBaseGameplayAbility>())
				{
				
					BoundVM->SetAbility4(secon->IconImage);
				
				}
			}
			
			
		}
	}
}



void AZL_CharacterSelector_Actor::SetPreview(USkeletalMesh* NewMesh, class UAnimSequence* NewAnim, float PoseTime)
{
	PreviewMesh->SetSkeletalMesh(NewMesh);
	if (!NewMesh) return;

	if (NewAnim)
	{
		PreviewMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		PreviewMesh->SetAnimation(NewAnim);
		if (PoseTime >= 0.f)
		{
			// Freeze on one frame: an action pose.
			PreviewMesh->SetPosition(FMath::Min(PoseTime, NewAnim->GetPlayLength()), false);
			PreviewMesh->Stop();
		}
		else
		{
			PreviewMesh->Play(true); // loop
		}
	}
}

