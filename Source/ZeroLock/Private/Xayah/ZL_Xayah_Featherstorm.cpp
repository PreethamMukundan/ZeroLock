// Copyright Preetham Mukundan (C) 2026


#include "Xayah/ZL_Xayah_Featherstorm.h"

#include "Abilities/Tasks/AbilityTask_WaitConfirmCancel.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Engine/World.h"
#include "GAS/Tasks/ZL_WaitDelay_Task.h"
#include "GAS/ZL_GameplayTags.h"
#include "Mover/ZeroMoverComponent.h"
#include "Xayah/ZL_Xayah_StormPreview.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Xayah_Featherstorm::UZL_Xayah_Featherstorm()
{
	ActivationOwnedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_UNTOUCHABLE);
	ActivationOwnedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_ABILITIES_BLOCKED);
	// Hangs in the air while she aims (Mover Float, sampled from the tag into the input, so predicted).
	ActivationOwnedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_STATUS_FLOATING);
	Slot = EGameplayAbilitySlot::UltimateSlot;
	CooldownDuration = 100.0f;
	SpiritDamage = 90.0f;
	BladeCount = 10;
	PreviewClass = AZL_Xayah_StormPreview::StaticClass();
	AbilityName = TEXT("Featherstorm");
	AbilityDescription = TEXT("Leap back, untouchable, pick a spot, and spray a cone of blades that each leave a feather.");
}

void UZL_Xayah_Featherstorm::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bThrown = false;

	const float Level = GetAbilityLevel();
	if (UZeroMoverComponent* MoverComp = Hero->GetZeroMoverComponent())
	{
		const FVector Back = -GetHeroAimDirection().GetSafeNormal2D();
		MoverComp->RequestSafeAbilityMove(Back * LeapBackSpeed.GetValueAtLevel(Level) + FVector::UpVector * LeapUpSpeed.GetValueAtLevel(Level),
			LeapDuration.GetValueAtLevel(Level));
	}

	// The remote server's spawn tasks start now and wait for the client's blades.
	if (!IsLocallyControlled())
	{
		ThrowFan();
		return;
	}

	UAbilityTask_WaitDelay* LeapTask = UAbilityTask_WaitDelay::WaitDelay(this, LeapDuration.GetValueAtLevel(Level));
	LeapTask->OnFinish.AddDynamic(this, &UZL_Xayah_Featherstorm::OnLeapFinished);
	LeapTask->ReadyForActivation();

	if (PreviewClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Hero;
		SpawnParams.Instigator = Hero;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Preview = GetWorld()->SpawnActor<AZL_Xayah_StormPreview>(PreviewClass, Hero->GetActorTransform(), SpawnParams);
		if (Preview)
		{
			Preview->InitPreview(ConeHalfAngle, PreviewRange);
		}
	}

	TargetTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this, TargetWindow.GetValueAtLevel(Level));
	TargetTask->OnProgress.AddDynamic(this, &UZL_Xayah_Featherstorm::UpdateProgressionTimer);
	TargetTask->OnStarted.AddDynamic(this, &UZL_Xayah_Featherstorm::StartProgressionTimer);
	TargetTask->OnEnd.AddDynamic(this, &UZL_Xayah_Featherstorm::StopProgressionTimer);
	TargetTask->OnFinished.AddDynamic(this, &UZL_Xayah_Featherstorm::ThrowNow);
	TargetTask->ReadyForActivation();

	ConfirmTask = UAbilityTask_WaitConfirmCancel::WaitConfirmCancel(this);
	ConfirmTask->OnConfirm.AddDynamic(this, &UZL_Xayah_Featherstorm::ThrowNow);
	ConfirmTask->OnCancel.AddDynamic(this, &UZL_Xayah_Featherstorm::ThrowNow);
	ConfirmTask->ReadyForActivation();
}

void UZL_Xayah_Featherstorm::OnLeapFinished()
{
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (UZeroMoverComponent* MoverComp = Hero ? Hero->GetZeroMoverComponent() : nullptr)
	{
		MoverComp->RequestSafeStop();
	}
}

void UZL_Xayah_Featherstorm::ThrowNow()
{
	if (bThrown || !IsActive())
	{
		return;
	}
	bThrown = true;
	DestroyPreview();
	ThrowFan();
}

FRotator UZL_Xayah_Featherstorm::GetBladeRotation(const FRotator& AimRotation, int32 Index) const
{
	// Sunflower spiral: even coverage of the disc for any blade count, and the same pattern every cast.
	const float Radius = ConeHalfAngle * FMath::Sqrt((Index + 0.5f) / FMath::Max(BladeCount, 1));
	const float Angle = Index * 2.39996f;
	return AimRotation + FRotator(Radius * FMath::Sin(Angle), Radius * FMath::Cos(Angle), 0.0f);
}

void UZL_Xayah_Featherstorm::DestroyPreview()
{
	if (Preview)
	{
		Preview->Destroy();
		Preview = nullptr;
	}
}

void UZL_Xayah_Featherstorm::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	DestroyPreview();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
