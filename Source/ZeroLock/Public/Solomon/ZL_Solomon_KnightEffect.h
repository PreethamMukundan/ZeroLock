// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ZL_Solomon_KnightEffect.generated.h"

/**
 * Timed tag carrier from Solomon's Kingside Knight: the status tags (Knight / King, or Untouchable while a knight is in
 * flight) go on the spec, for the spec's duration. Apply with Apply(). Also Rook Battery's root (ZeroLock.Movement.Rooted).
 */
UCLASS()
class ZEROLOCK_API UZL_Solomon_KnightEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZL_Solomon_KnightEffect();

	/** Server. Source applies it to Target for Duration seconds. */
	static FActiveGameplayEffectHandle Apply(UAbilitySystemComponent* Source, UAbilitySystemComponent* Target, float Duration,
		const FGameplayTagContainer& GrantedTags);
};
