// Copyright Preetham Mukundan (C) 2026


#include "Solomon/ZL_Solomon_KingsideKnight.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "Mover/ZeroMoverComponent.h"
#include "Solomon/ZL_Solomon_KnightEffect.h"
#include "Solomon/ZL_Solomon_KnightPiece.h"
#include "Solomon/ZL_Solomon_MovePreview.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Solomon_KingsideKnight::UZL_Solomon_KingsideKnight()
{
	// Like Viscous's Cube: allies, self-cast by double tap or by confirming with nobody in the cone.
	TargetTeam = EZL_ConeTargetTeam::Allies;
	bSelfCastOnDoubleTap = true;
	bSelfCastWithoutTarget = true;
	// Deadlock AbilityCastRange 28 m (1 m = 70 cm), TargetingConeAngle 30, AbilityCooldown 25.
	Range = 1960.0f;
	ConeHalfAngle = 15.0f;
	CooldownDuration = 25.0f;
	PieceClass = AZL_Solomon_KnightPiece::StaticClass();
}

AZL_Solomon_KnightPiece* UZL_Solomon_KingsideKnight::FindMovableKing(const AActor* Avatar) const
{
	for (AZL_Solomon_ChessPiece* Piece : AZL_Solomon_ChessPiece::GetPiecesOf(Avatar, AZL_Solomon_KnightPiece::StaticClass()))
	{
		AZL_Solomon_KnightPiece* Knight = Cast<AZL_Solomon_KnightPiece>(Piece);
		if (Knight && Knight->IsKing() && Knight->CanMove())
		{
			return Knight;
		}
	}
	return nullptr;
}

bool UZL_Solomon_KingsideKnight::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	// The re-press that moves the king works while the cast's cooldown runs.
	if (FindMovableKing(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		return true;
	}
	return Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
}

void UZL_Solomon_KingsideKnight::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (AZL_Solomon_KnightPiece* King = FindMovableKing(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		StartKingMove(King);
		return;
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UZL_Solomon_KingsideKnight::StartKingMove(AZL_Solomon_KnightPiece* King)
{
	if (!IsLocallyControlled())
	{
		// Remote client's hero: wait for the aim.
		UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
		KingMoveDataHandle = ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
			.AddUObject(this, &UZL_Solomon_KingsideKnight::OnKingMoveData);
		ASC->CallReplicatedTargetDataDelegatesIfSet(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
		return;
	}

	const FVector Aim = AZL_Solomon_MovePreview::GetAimPoint(CurrentActorInfo->PlayerController.Get(), KingAimDistance, GetAvatarActorFromActorInfo());
	FVector Spot;
	if (!King->GetMoveTarget(Aim, Spot))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	if (HasAuthority(&CurrentActivationInfo))
	{
		// Listen server host / standalone.
		King->ServerStartMove(Aim);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	FHitResult Hit;
	Hit.bBlockingHit = true;
	Hit.Location = Aim;
	Hit.ImpactPoint = Aim;
	Hit.HitObjectHandle = FActorInstanceHandle(King);
	FGameplayAbilityTargetDataHandle Data(new FGameplayAbilityTargetData_SingleTargetHit(Hit));
	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	{
		FScopedPredictionWindow ScopedPrediction(ASC, IsPredictingClient());
		ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), Data, FGameplayTag(), ASC->ScopedPredictionKey);
	}
	// The server ends its side once it has handled the data.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UZL_Solomon_KingsideKnight::OnKingMoveData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag)
{
	if (UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}
	const FGameplayAbilityTargetData* Entry = Data.Get(0);
	const FHitResult* Hit = Entry ? Entry->GetHitResult() : nullptr;
	AZL_Solomon_KnightPiece* King = FindMovableKing(GetAvatarActorFromActorInfo());
	const bool bMoved = Hit && King && Hit->GetActor() == King && King->ServerStartMove(Hit->Location);
	if (!bMoved)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] server rejected king move"), *GetName());
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, !bMoved);
}

void UZL_Solomon_KingsideKnight::ConeTargetConfirmed(AZeroLockCharacter* Target, const FGameplayAbilityTargetDataHandle& Data)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (HasAuthority(&CurrentActivationInfo) && Hero && IsValid(Target) && PieceClass)
	{
		const int32 Level = GetAbilityLevel();
		const float Duration = BuffDuration.GetValueAtLevel(Level);
		const bool bKing = Target == Hero;

		// One knight at a time (Deadlock MaxPieces 1); the old buff runs out on its own.
		for (AZL_Solomon_ChessPiece* Old : AZL_Solomon_ChessPiece::GetPiecesOf(Hero, PieceClass))
		{
			Old->Destroy();
		}
		FActorSpawnParameters Params;
		Params.Owner = Hero;
		Params.Instigator = Hero;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AZL_Solomon_KnightPiece* Piece = GetWorld()->SpawnActor<AZL_Solomon_KnightPiece>(PieceClass, Target->GetActorTransform(), Params))
		{
			Piece->InitPromotion(Target, Duration, HopSpeed.GetValueAtLevel(Level));
		}

		if (UBaseCharAbilitySystemComponent* TargetASC = Target->GetMyAbilitySystemComp())
		{
			TargetASC->AddCombatBarrier((bKing ? KingBarrier : KnightBarrier).GetValueAtLevel(Level), Duration);
			FGameplayTagContainer Status;
			Status.AddTag(bKing ? ZerolockGameplayTagsForBinding::TAG_STATUS_KING : ZerolockGameplayTagsForBinding::TAG_STATUS_KNIGHT);
			UZL_Solomon_KnightEffect::Apply(Hero->GetAbilitySystemComponent(), TargetASC, Duration, Status);
		}
		// Knight: one stamina now (not max stamina; clamped to their max). Mover owns stamina, so it's granted in its sim.
		if (!bKing)
		{
			if (UZeroMoverComponent* Mover = Target->FindComponentByClass<UZeroMoverComponent>())
			{
				Mover->RequestStaminaGrant(BonusStamina.GetValueAtLevel(Level));
			}
		}
	}
	Super::ConeTargetConfirmed(Target, Data);
}

void UZL_Solomon_KingsideKnight::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (KingMoveDataHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).Remove(KingMoveDataHandle);
		}
		KingMoveDataHandle.Reset();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
