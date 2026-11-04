// Copyright Preetham Mukundan (C) 2026

#include "Cheats/ZL_DummyController.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerState.h"
#include "GAS/BaseCharAttributeSet.h"
#include "GAS/ZL_GameplayTags.h"
#include "TimerManager.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_DummyController::AZL_DummyController()
{
	// A PlayerState gives the dummy a team: damage between heroes without different teams is ignored.
	bWantsPlayerState = true;
}

void AZL_DummyController::InitPlayerState()
{
	Super::InitPlayerState();
	// Before possession, so the pawn already sees a bot (no local-player HUD setup, overhead bar on the host).
	if (PlayerState)
	{
		PlayerState->SetIsABot(true);
	}
}

void AZL_DummyController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(InPawn))
	{
		if (UAbilitySystemComponent* ASC = Hero->GetAbilitySystemComponent())
		{
			ASC->SetLooseGameplayTagCount(ZerolockGameplayTagsForBinding::TAG_CHEAT_BUDDHA, bImmortal ? 1 : 0);
		}
	}
	LastHealth = -1.f;
	StrafeFlipTime = GetWorld()->GetTimeSeconds() + StrafeInterval * 0.5f;
	GetWorldTimerManager().SetTimer(UpdateTimer, this, &AZL_DummyController::UpdateDummy, 0.1f, true);
}

void AZL_DummyController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(UpdateTimer);
	Super::OnUnPossess();
}

void AZL_DummyController::UpdateDummy()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetPawn());
	UAbilitySystemComponent* ASC = Hero ? Hero->GetAbilitySystemComponent() : nullptr;
	const UBaseCharAttributeSet* Attributes = Hero ? Hero->GetMyAttributeSet() : nullptr;
	if (!ASC || !Attributes)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();

	if (Hero->bIsDead)
	{
		Hero->SetBotMoveInput(FVector2D::ZeroVector);
		if (DeathTime <= 0.0)
		{
			DeathTime = Now;
		}
		else if (Now - DeathTime >= RespawnDelay)
		{
			DeathTime = 0.0;
			LastHealth = -1.f;
			Hero->ResetCharacter();
		}
		return;
	}

	const float Health = Attributes->GetCurrentHealth();
	const float MaxHealth = Attributes->GetMaximumHealth();
	if (LastHealth >= 0.f && Health < LastHealth)
	{
		LastDamageTime = Now;
	}
	LastHealth = Health;
	if (Health < MaxHealth && Now - LastDamageTime >= HealDelay)
	{
		ASC->SetNumericAttributeBase(UBaseCharAttributeSet::GetCurrentHealthAttribute(), MaxHealth);
		LastHealth = MaxHealth;
	}

	if (bStrafe && Now >= StrafeFlipTime)
	{
		StrafeDirection = -StrafeDirection;
		StrafeFlipTime = Now + StrafeInterval;
	}
	Hero->SetBotMoveInput(bStrafe ? FVector2D(StrafeDirection, 0.f) : FVector2D::ZeroVector);
}
