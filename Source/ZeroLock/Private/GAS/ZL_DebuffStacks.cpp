// Copyright Preetham Mukundan (C) 2026

#include "GAS/ZL_DebuffStacks.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GAS/ZL_GameplayTags.h"

void UZL_VM_DebuffStack::Apply(const FZL_DebuffStackEntry& Entry)
{
	SetTag(Entry.Tag);
	SetInstigator(Entry.Instigator);
	SetStackCount(Entry.Stacks);
	SetDuration(Entry.Duration);
	SetEndServerTime(Entry.EndServerTime);
}

float UZL_VM_DebuffStack::GetRemainingPercent(double ServerTime) const
{
	if (Duration <= 0.f)
	{
		return 1.f;
	}
	return FMath::Clamp(static_cast<float>((EndServerTime - ServerTime) / Duration), 0.f, 1.f);
}

void UZL_VM_DebuffStacks::Update(const TArray<FZL_DebuffStackEntry>& InEntries)
{
	TArray<TObjectPtr<UZL_VM_DebuffStack>> NewEntries;
	NewEntries.Reserve(InEntries.Num());
	for (const FZL_DebuffStackEntry& Entry : InEntries)
	{
		const TObjectPtr<UZL_VM_DebuffStack>* Existing = Entries.FindByPredicate([&Entry](const UZL_VM_DebuffStack* VM)
		{
			return VM && VM->GetTag() == Entry.Tag && VM->GetInstigator() == Entry.Instigator;
		});
		UZL_VM_DebuffStack* VM = Existing ? Existing->Get() : NewObject<UZL_VM_DebuffStack>(this);
		VM->Apply(Entry);
		NewEntries.Add(VM);
	}
	SetEntries(NewEntries);
}

FGameplayTag ZLDebuffStack::FindStackTag(const FActiveGameplayEffect& Effect)
{
	FGameplayTagContainer Granted;
	Effect.Spec.GetAllGrantedTags(Granted);
	const FGameplayTag Root = ZerolockGameplayTagsForBinding::TAG_DEBUFFSTACK;
	for (const FGameplayTag& Tag : Granted)
	{
		if (Tag != Root && Tag.MatchesTag(Root))
		{
			return Tag;
		}
	}
	return FGameplayTag();
}

void ZLDebuffStack::Gather(const UAbilitySystemComponent* ASC, TArray<FZL_DebuffStackEntry>& Out)
{
	Out.Reset();
	if (!ASC)
	{
		return;
	}
	for (auto It = ASC->GetActiveGameplayEffects().CreateConstIterator(); It; ++It)
	{
		const FActiveGameplayEffect& Effect = *It;
		if (Effect.IsPendingRemove)
		{
			continue;
		}
		const FGameplayTag Tag = FindStackTag(Effect);
		if (!Tag.IsValid())
		{
			continue;
		}
		AActor* Instigator = Effect.Spec.GetContext().GetInstigator();
		const float Duration = Effect.GetDuration();
		const bool bTimed = Duration > 0.f;
		const int32 Stacks = FMath::Max(Effect.Spec.GetStackCount(), 1);
		if (FZL_DebuffStackEntry* Same = Out.FindByPredicate([&](const FZL_DebuffStackEntry& E) { return E.Tag == Tag && E.Instigator == Instigator; }))
		{
			Same->Stacks += Stacks;
			if (bTimed && Effect.GetEndTime() > Same->EndServerTime)
			{
				Same->EndServerTime = Effect.GetEndTime();
				Same->Duration = Duration;
			}
			continue;
		}
		FZL_DebuffStackEntry& Entry = Out.AddDefaulted_GetRef();
		Entry.Tag = Tag;
		Entry.Instigator = Instigator;
		Entry.Stacks = Stacks;
		Entry.Duration = bTimed ? Duration : 0.f;
		Entry.EndServerTime = bTimed ? Effect.GetEndTime() : 0.0;
	}
	Out.Sort([](const FZL_DebuffStackEntry& A, const FZL_DebuffStackEntry& B)
	{
		if (A.Tag != B.Tag)
		{
			return A.Tag.GetTagName().LexicalLess(B.Tag.GetTagName());
		}
		return GetNameSafe(A.Instigator) < GetNameSafe(B.Instigator);
	});
}
