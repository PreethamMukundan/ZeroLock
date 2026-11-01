// Copyright Preetham Mukundan (C) 2026


#include "Solomon/ZL_Solomon_MovePiece.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ZL_GameplayTags.h"
#include "Solomon/ZL_Solomon_ChessPiece.h"
#include "Solomon/ZL_Solomon_MovePreview.h"
#include "Solomon/ZL_Solomon_PieceSelector.h"

UZL_Solomon_MovePiece::UZL_Solomon_MovePiece()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	// Each piece has its own move cooldown; nothing to commit here.
	CooldownGameplayEffectClass = nullptr;
	PreviewClass = AZL_Solomon_MovePreview::StaticClass();
	// He floats while aiming (Deadlock movechesspiece AirSpeedMax / FallSpeedMax): Mover reads the tag through the input.
	ActivationOwnedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_STATUS_FLOATING);
}

void UZL_Solomon_MovePiece::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	// The hover highlight runs from the selector, before Move Piece is ever pressed.
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		UZL_Solomon_PieceSelector::Get(ActorInfo->AvatarActor.Get());
	}
}

float UZL_Solomon_MovePiece::GetSelectRange() const
{
	const UZL_Solomon_PieceSelector* Selector = UZL_Solomon_PieceSelector::Get(GetAvatarActorFromActorInfo());
	return Selector ? Selector->GetSelectRange() : 3500.0f;
}

float UZL_Solomon_MovePiece::GetAimDistance() const
{
	// Far enough to reach past the furthest piece by a full move.
	return GetSelectRange() + 2000.0f;
}

void UZL_Solomon_MovePiece::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bConfirmed = false;
	SelectedPiece = nullptr;

	if (!IsLocallyControlled())
	{
		// Remote client's hero: wait for its piece + aim.
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		TargetDataHandle = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
			.AddUObject(this, &UZL_Solomon_MovePiece::OnServerTargetData);
		ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationInfo.GetActivationPredictionKey());
		return;
	}

	// No piece to pick: he still floats until release, which then just ends it (ConfirmMove).
	UZL_Solomon_PieceSelector* Selector = UZL_Solomon_PieceSelector::Get(GetAvatarActorFromActorInfo());
	AZL_Solomon_ChessPiece* Piece = Selector ? Selector->FindPieceToMove() : nullptr;
	if (Piece)
	{
		SelectedPiece = Piece;
		Selector->LockSelection(Piece);
	}

	if (Piece && PreviewClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Preview = GetWorld()->SpawnActor<AZL_Solomon_MovePreview>(PreviewClass, Piece->GetActorTransform(), SpawnParams);
		if (Preview)
		{
			Preview->Init(Piece, ActorInfo->PlayerController.Get(), GetAimDistance());
		}
	}

	// Released before the ability got going: move toward where the crosshair is now.
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	if (Spec && !Spec->InputPressed)
	{
		ConfirmMove();
	}
}

void UZL_Solomon_MovePiece::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (IsActive() && IsLocallyControlled())
	{
		ConfirmMove();
	}
}

void UZL_Solomon_MovePiece::ConfirmMove()
{
	if (bConfirmed)
	{
		return;
	}
	bConfirmed = true;

	AZL_Solomon_ChessPiece* Piece = SelectedPiece.Get();
	FVector Square;
	const FVector Aim = AZL_Solomon_MovePreview::GetAimPoint(CurrentActorInfo->PlayerController.Get(), GetAimDistance(), GetAvatarActorFromActorInfo());
	if (!Piece || !Piece->GetMoveTarget(Aim, Square))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&CurrentActivationInfo))
	{
		// Listen server host / standalone.
		ExecuteMove(Piece, Aim);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	FHitResult Hit;
	Hit.bBlockingHit = true;
	Hit.Location = Aim;
	Hit.ImpactPoint = Aim;
	Hit.HitObjectHandle = FActorInstanceHandle(Piece);
	FGameplayAbilityTargetDataHandle Data(new FGameplayAbilityTargetData_SingleTargetHit(Hit));

	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	{
		FScopedPredictionWindow ScopedPrediction(ASC, IsPredictingClient());
		ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), Data, FGameplayTag(), ASC->ScopedPredictionKey);
	}
	// The server ends its side once it has handled the data.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UZL_Solomon_MovePiece::OnServerTargetData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag)
{
	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC)
	{
		ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}
	const FGameplayAbilityTargetData* Entry = Data.Get(0);
	const FHitResult* Hit = Entry ? Entry->GetHitResult() : nullptr;
	AZL_Solomon_ChessPiece* Piece = Hit ? Cast<AZL_Solomon_ChessPiece>(Hit->GetActor()) : nullptr;
	if (Piece)
	{
		ExecuteMove(Piece, Hit->Location);
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, Piece == nullptr);
}

void UZL_Solomon_MovePiece::ExecuteMove(AZL_Solomon_ChessPiece* Piece, const FVector& Aim)
{
	const AActor* Hero = GetAvatarActorFromActorInfo();
	bool bOk = Hero && IsValid(Piece) && Piece->GetInstigator() == Hero && Piece->CanMove()
		&& FVector::DistSquared(Hero->GetActorLocation(), Piece->GetActorLocation()) <= FMath::Square(GetSelectRange() + ServerRangeSlack);
	// One piece slides at a time.
	if (bOk)
	{
		for (const AZL_Solomon_ChessPiece* Other : AZL_Solomon_ChessPiece::GetPiecesOf(Hero))
		{
			bOk &= Other == Piece || !Other->IsMoving();
		}
	}
	if (!bOk || !Piece->ServerStartMove(Aim))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] server rejected move of %s"), *GetName(), *GetNameSafe(Piece));
	}
}

void UZL_Solomon_MovePiece::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (TargetDataHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).Remove(TargetDataHandle);
		}
		TargetDataHandle.Reset();
	}
	if (SelectedPiece.IsValid())
	{
		if (UZL_Solomon_PieceSelector* Selector = UZL_Solomon_PieceSelector::Get(GetAvatarActorFromActorInfo()))
		{
			Selector->LockSelection(nullptr);
		}
	}
	SelectedPiece = nullptr;
	if (Preview)
	{
		Preview->Destroy();
		Preview = nullptr;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
