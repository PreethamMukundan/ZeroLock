// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "ZL_DebuffStacks.generated.h"

class UAbilitySystemComponent;
struct FActiveGameplayEffect;

/**
 * Debuff stacks for the HUD. Any gameplay effect that grants a child of Zerolock.DebuffStack (definition or dynamic granted
 * tags, e.g. Zerolock.DebuffStack.Frost) is listed on the victim's UBaseCharAbilitySystemComponent::DebuffStacks
 * (server-built, replicated to everyone), one entry per tag and instigator. UZL_VM_DebuffStacks mirrors it for widgets
 * (UZL_DebuffStackIcons): the victim's own HUD shows every entry, an overhead bar only the local player's.
 */
USTRUCT(BlueprintType)
struct ZEROLOCK_API FZL_DebuffStackEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag Tag;

	/** Who applied it (the effect context's instigator, a hero pawn). */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> Instigator = nullptr;

	UPROPERTY(BlueprintReadOnly)
	int32 Stacks = 0;

	/** Seconds the current stack timer runs; 0 = no timer (infinite). */
	UPROPERTY(BlueprintReadOnly)
	float Duration = 0.f;

	/** Server world time (AGameStateBase::GetServerWorldTimeSeconds) the timer runs out. */
	UPROPERTY(BlueprintReadOnly)
	double EndServerTime = 0.0;

	bool operator==(const FZL_DebuffStackEntry& Other) const
	{
		return Tag == Other.Tag && Instigator == Other.Instigator && Stacks == Other.Stacks
			&& FMath::IsNearlyEqual(Duration, Other.Duration) && FMath::IsNearlyEqual(EndServerTime, Other.EndServerTime);
	}
};

/** One debuff stack entry (one tag from one instigator). */
UCLASS(BlueprintType)
class ZEROLOCK_API UZL_VM_DebuffStack : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Apply(const FZL_DebuffStackEntry& Entry);

	FGameplayTag GetTag() const { return Tag; }
	void SetTag(FGameplayTag InTag) { UE_MVVM_SET_PROPERTY_VALUE(Tag, InTag); }

	AActor* GetInstigator() const { return Instigator; }
	void SetInstigator(AActor* InInstigator) { UE_MVVM_SET_PROPERTY_VALUE(Instigator, InInstigator); }

	int32 GetStackCount() const { return StackCount; }
	void SetStackCount(int32 InCount) { UE_MVVM_SET_PROPERTY_VALUE(StackCount, InCount); }

	float GetDuration() const { return Duration; }
	void SetDuration(float InDuration) { UE_MVVM_SET_PROPERTY_VALUE(Duration, InDuration); }

	double GetEndServerTime() const { return EndServerTime; }
	void SetEndServerTime(double InTime) { UE_MVVM_SET_PROPERTY_VALUE(EndServerTime, InTime); }

	/** 1 = just applied, 0 = about to fade; 1 when there's no timer. */
	UFUNCTION(BlueprintPure, Category = "Debuff")
	float GetRemainingPercent(double ServerTime) const;

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Setter, meta = (AllowPrivateAccess = true))
	FGameplayTag Tag;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Setter, meta = (AllowPrivateAccess = true))
	TObjectPtr<AActor> Instigator = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Setter, meta = (AllowPrivateAccess = true))
	int32 StackCount = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Setter, meta = (AllowPrivateAccess = true))
	float Duration = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Setter, meta = (AllowPrivateAccess = true))
	double EndServerTime = 0.0;
};

/** Every debuff stack on one hero. Owned by that hero's ASC, on every machine. */
UCLASS(BlueprintType)
class ZEROLOCK_API UZL_VM_DebuffStacks : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Reuses the entry viewmodel per (tag, instigator); broadcasts Entries when the set changes. */
	void Update(const TArray<FZL_DebuffStackEntry>& InEntries);

	const TArray<TObjectPtr<UZL_VM_DebuffStack>>& GetEntries() const { return Entries; }

private:
	void SetEntries(const TArray<TObjectPtr<UZL_VM_DebuffStack>>& InEntries) { UE_MVVM_SET_PROPERTY_VALUE(Entries, InEntries); }

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, meta = (AllowPrivateAccess = true))
	TArray<TObjectPtr<UZL_VM_DebuffStack>> Entries;
};

namespace ZLDebuffStack
{
	/** The Zerolock.DebuffStack child this active effect grants, or an invalid tag. */
	ZEROLOCK_API FGameplayTag FindStackTag(const FActiveGameplayEffect& Effect);

	/** Builds the list from ASC's active effects, sorted by tag then instigator name (stable icon order). */
	ZEROLOCK_API void Gather(const UAbilitySystemComponent* ASC, TArray<FZL_DebuffStackEntry>& Out);
}
