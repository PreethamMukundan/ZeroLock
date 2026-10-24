// Copyright Preetham Mukundan (C) 2026


#include "Viscous/ZL_Viscous_TheCube.h"

#include "AbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "Viscous/ZL_Viscous_Cube.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Viscous_TheCube::UZL_Viscous_TheCube()
{
	TargetTeam = EZL_ConeTargetTeam::Allies;
	bSelfCastOnDoubleTap = true;
	bSelfCastWithoutTarget = true;
	CubeClass = AZL_Viscous_Cube::StaticClass();
}

bool UZL_Viscous_TheCube::IsTargetAllowed(const AZeroLockCharacter* Target) const
{
	const UAbilitySystemComponent* TargetASC = Target ? Target->GetAbilitySystemComponent() : nullptr;
	return TargetASC && !TargetASC->HasMatchingGameplayTag(ZerolockGameplayTagsForBinding::TAG_STATUS_CUBED);
}

void UZL_Viscous_TheCube::ConeTargetConfirmed(AZeroLockCharacter* Target, const FGameplayAbilityTargetDataHandle& Data)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (HasAuthority(&CurrentActivationInfo) && Hero && IsValid(Target) && CubeClass)
	{
		FActorSpawnParameters Params;
		Params.Owner = Hero;
		Params.Instigator = Hero;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AZL_Viscous_Cube* Cube = GetWorld()->SpawnActor<AZL_Viscous_Cube>(CubeClass, Target->GetActorTransform(), Params))
		{
			const int32 Level = GetAbilityLevel();
			Cube->StartCube(Hero, Target, Duration.GetValueAtLevel(Level), HealPerSecond.GetValueAtLevel(Level));
		}
	}

	Super::ConeTargetConfirmed(Target, Data);
}
