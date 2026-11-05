// Copyright Preetham Mukundan (C) 2026

#include "Items/ZL_ItemProcComponent.h"

#include "EngineUtils.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/BaseCharAttributeSet.h"
#include "Items/ZL_GE_ItemStats.h"
#include "Items/Zero_Item_Inventory_Component.h"
#include "TimerManager.h"
#include "ZeroLock/ZeroLockCharacter.h"

namespace
{
	constexpr float UpdateInterval = 0.25f;

	/** Duration 0 lasts until it's removed. */
	FActiveGameplayEffectHandle ApplyStats(UBaseCharAbilitySystemComponent* Source, UAbilitySystemComponent* Target, const TArray<FZL_ItemStatValue>& Stats, float Duration, int32 Stacks)
	{
		const TSubclassOf<UGameplayEffect> EffectClass = Duration > 0.0f ? UZL_GE_ItemStatsTimed::StaticClass() : UZL_GE_ItemStats::StaticClass();
		const FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(EffectClass, 1.0f, Source->MakeEffectContext());
		if (!Spec.IsValid())
		{
			return FActiveGameplayEffectHandle();
		}

		TArray<FZL_ItemStatValue> Scaled = Stats;
		for (FZL_ItemStatValue& Stat : Scaled)
		{
			Stat.Value *= Stacks;
		}
		if (Duration > 0.0f)
		{
			Spec.Data->SetDuration(Duration, true);
		}
		UZL_GE_ItemStats::SetMagnitudes(*Spec.Data, Scaled);
		return Source->ApplyGameplayEffectSpecToTarget(*Spec.Data, Target);
	}

	/** Living heroes of a team around a point, nearest first. */
	TArray<AZeroLockCharacter*> FindHeroes(AZeroLockCharacter* Hero, const FVector& Center, float Radius, EZL_ActiveItemTeam Team, const AZeroLockCharacter* Excluded, int32 MaxTargets)
	{
		TArray<AZeroLockCharacter*> Heroes;
		for (TActorIterator<AZeroLockCharacter> It(Hero->GetWorld()); It; ++It)
		{
			AZeroLockCharacter* Candidate = *It;
			const bool bAlly = Candidate == Hero || Hero->IsOnSameTeam(Candidate);
			if (Candidate != Excluded && !Candidate->bIsDead && bAlly == (Team == EZL_ActiveItemTeam::Allies)
				&& FVector::DistSquared(Center, Candidate->GetActorLocation()) <= FMath::Square(Radius))
			{
				Heroes.Add(Candidate);
			}
		}

		Heroes.Sort([&Center](const AZeroLockCharacter& A, const AZeroLockCharacter& B)
		{
			return FVector::DistSquared(Center, A.GetActorLocation()) < FVector::DistSquared(Center, B.GetActorLocation());
		});
		if (MaxTargets > 0 && Heroes.Num() > MaxTargets)
		{
			Heroes.SetNum(MaxTargets);
		}
		return Heroes;
	}

	bool IsEffectActive(const UAbilitySystemComponent* ASC, const FActiveGameplayEffectHandle& Handle)
	{
		return ASC && Handle.IsValid() && ASC->GetActiveGameplayEffect(Handle);
	}
}

UZL_ItemProcComponent::UZL_ItemProcComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UZL_ItemProcComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(UpdateHandle, this, &UZL_ItemProcComponent::Update, UpdateInterval, true);
	}
}

void UZL_ItemProcComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (FRunningProc& Proc : Running)
	{
		Release(Proc);
	}
	Running.Reset();
	Super::EndPlay(EndPlayReason);
}

AZeroLockCharacter* UZL_ItemProcComponent::GetHero() const
{
	return Cast<AZeroLockCharacter>(GetOwner());
}

double UZL_ItemProcComponent::GetTime() const
{
	return GetWorld()->GetTimeSeconds();
}

float UZL_ItemProcComponent::GetHealthPercent(const AZeroLockCharacter* Hero) const
{
	const UBaseCharAttributeSet* Attributes = Hero ? Hero->GetMyAttributeSet() : nullptr;
	return Attributes && Attributes->GetMaximumHealth() > 0.0f ? Attributes->GetCurrentHealth() / Attributes->GetMaximumHealth() * 100.0f : 0.0f;
}

void UZL_ItemProcComponent::AddItem(const UZero_Item_data* Item)
{
	if (!Item || !GetOwner()->HasAuthority())
	{
		return;
	}
	for (const FZL_ItemProc& Proc : Item->Procs)
	{
		FRunningProc& Added = Running.AddDefaulted_GetRef();
		Added.Item = Item;
		Added.Proc = &Proc;
	}
}

void UZL_ItemProcComponent::RemoveItem(const UZero_Item_data* Item)
{
	for (int32 Index = Running.Num() - 1; Index >= 0; Index--)
	{
		if (Running[Index].Item == Item)
		{
			const FZL_ItemProc* Proc = Running[Index].Proc;
			Dots.RemoveAll([Proc](const FRunningDot& Dot) { return Dot.Proc == Proc; });
			Release(Running[Index]);
			Running.RemoveAt(Index);
		}
	}
}

void UZL_ItemProcComponent::Release(FRunningProc& Proc)
{
	const AZeroLockCharacter* Hero = GetHero();
	if (UAbilitySystemComponent* ASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr)
	{
		// Timed effects run out on their own; held and aura ones have no end.
		if (Proc.Proc->Duration <= 0.0f && IsEffectActive(ASC, Proc.SelfHandle))
		{
			ASC->RemoveActiveGameplayEffect(Proc.SelfHandle);
		}
	}
	if (Proc.Proc->Duration <= 0.0f)
	{
		for (TPair<TWeakObjectPtr<UAbilitySystemComponent>, FTargetState>& Target : Proc.Targets)
		{
			if (IsEffectActive(Target.Key.Get(), Target.Value.Handle))
			{
				Target.Key->RemoveActiveGameplayEffect(Target.Value.Handle);
			}
		}
	}
	Proc.SelfHandle.Invalidate();
	Proc.Targets.Reset();
}

void UZL_ItemProcComponent::ResetAfterRespawn()
{
	Dots.Reset();
	Zones.Reset();
	for (FRunningProc& Proc : Running)
	{
		Proc.SelfHandle.Invalidate();
		Proc.SelfStacks = 0;
		Proc.Targets.Reset();
		Proc.DamageLog.Reset();
	}
}

bool UZL_ItemProcComponent::PassesConditions(FRunningProc& Proc, AZeroLockCharacter* Other, float Damage, bool bCountHit)
{
	const FZL_ItemProc& Data = *Proc.Proc;
	const AZeroLockCharacter* Hero = GetHero();
	if (GetTime() < Proc.ReadyTime)
	{
		return false;
	}

	const float Health = GetHealthPercent(Hero);
	if ((Data.HealthAbove > 0.0f && Health <= Data.HealthAbove) || (Data.HealthBelow > 0.0f && Health >= Data.HealthBelow))
	{
		return false;
	}
	if (Other)
	{
		if (Data.TargetHealthAbove > 0.0f && GetHealthPercent(Other) <= Data.TargetHealthAbove)
		{
			return false;
		}
		const float Distance = FVector::Dist(Hero->GetActorLocation(), Other->GetActorLocation());
		if ((Data.MinRange > 0.0f && Distance < Data.MinRange) || (Data.MaxRange > 0.0f && Distance > Data.MaxRange))
		{
			return false;
		}
	}

	if (bCountHit)
	{
		if (Data.DamageWindow > 0.0f)
		{
			const double Oldest = GetTime() - Data.DamageWindow;
			Proc.DamageLog.RemoveAll([Oldest](const FDamageEntry& Entry) { return Entry.Time < Oldest; });
			Proc.DamageLog.Add({ GetTime(), Damage });

			float Total = 0.0f;
			for (const FDamageEntry& Entry : Proc.DamageLog)
			{
				Total += Entry.Damage;
			}
			if (Total < Data.MinDamage)
			{
				return false;
			}
			Proc.DamageLog.Reset();
		}
		else if (Damage < Data.MinDamage)
		{
			return false;
		}

		if (Data.BuildUpHits > 1 && Other)
		{
			FTargetState& State = Proc.Targets.FindOrAdd(TWeakObjectPtr<UAbilitySystemComponent>(Other->GetMyAbilitySystemComp()));
			if (++State.BuildUp < Data.BuildUpHits)
			{
				return false;
			}
			State.BuildUp = 0;
		}
	}

	return Data.Chance <= 0.0f || FMath::FRand() * 100.0f < Data.Chance;
}

float UZL_ItemProcComponent::GetDamageScale(EZL_ProcTrigger Trigger, AZeroLockCharacter* Target)
{
	float Bonus = 0.0f;
	if (!bFiring)
	{
		for (FRunningProc& Proc : Running)
		{
			if (Proc.Proc->Trigger == Trigger && Proc.Proc->BonusDamagePercent != 0.0f && PassesConditions(Proc, Target, 0.0f, false))
			{
				Bonus += Proc.Proc->BonusDamagePercent;
				Proc.ReadyTime = GetTime() + Proc.Proc->Cooldown;
			}
		}
	}
	return 1.0f + Bonus / 100.0f;
}

void UZL_ItemProcComponent::NotifyEvent(EZL_ProcTrigger Trigger, AZeroLockCharacter* Other, float Damage, int32 ImbueInput)
{
	const AZeroLockCharacter* Hero = GetHero();
	if (bFiring || !Hero || Hero->bIsDead)
	{
		return;
	}

	const UZero_Item_Inventory_Component* Inventory = Hero->GetInventoryComponent();
	for (FRunningProc& Proc : Running)
	{
		if (ImbueInput != 0 && (!Inventory || Inventory->GetImbueInput(Proc.Item) != ImbueInput))
		{
			continue;
		}
		if (Proc.Proc->Trigger == Trigger && Proc.Proc->BonusDamagePercent == 0.0f && PassesConditions(Proc, Other, Damage, true))
		{
			Fire(Proc, Other, Damage);
		}
	}
}

void UZL_ItemProcComponent::Fire(FRunningProc& Proc, AZeroLockCharacter* Other, float Damage)
{
	TGuardValue<bool> FiringGuard(bFiring, true);

	const FZL_ItemProc& Data = *Proc.Proc;
	AZeroLockCharacter* Hero = GetHero();
	Proc.ReadyTime = GetTime() + Data.Cooldown;

	TArray<AZeroLockCharacter*> Targets;
	if (Other && !Data.bSkipHitTarget)
	{
		Targets.Add(Other);
	}
	if (Data.Radius > 0.0f)
	{
		const FVector Center = Other ? Other->GetActorLocation() : Hero->GetActorLocation();
		Targets.Append(FindHeroes(Hero, Center, Data.Radius, Data.AreaTeam, Other, Data.MaxTargets));
	}

	for (AZeroLockCharacter* Target : Targets)
	{
		ApplyToTarget(Proc, Target, Damage);
	}
	ApplyToSelf(Proc, Damage);
}

void UZL_ItemProcComponent::ApplyToTarget(FRunningProc& Proc, AZeroLockCharacter* Target, float Damage)
{
	const FZL_ItemProc& Data = *Proc.Proc;
	UBaseCharAbilitySystemComponent* HeroASC = GetHero()->GetMyAbilitySystemComp();
	UBaseCharAbilitySystemComponent* TargetASC = Target->GetMyAbilitySystemComp();
	if (!TargetASC || Target->bIsDead)
	{
		return;
	}

	const UBaseCharAttributeSet* TargetAttributes = Target->GetMyAttributeSet();
	const float SpiritDamage = Data.SpiritDamage + TargetAttributes->GetCurrentHealth() * Data.TargetHealthPercent / 100.0f;
	if (SpiritDamage > 0.0f)
	{
		HeroASC->ApplySpiritDamage(TargetASC, SpiritDamage, this);
	}
	const float WeaponDamage = Data.WeaponDamage + Damage * Data.PercentOfHit / 100.0f;
	if (WeaponDamage > 0.0f)
	{
		HeroASC->ApplyWeaponDamage(TargetASC, WeaponDamage, this);
	}

	if (!Data.TargetStats.IsEmpty())
	{
		// One effect per target: a new hit refreshes it instead of stacking a copy, unless the proc has stacks to add.
		FTargetState& State = Proc.Targets.FindOrAdd(TWeakObjectPtr<UAbilitySystemComponent>(TargetASC));
		const bool bActive = IsEffectActive(TargetASC, State.Handle);
		State.Stacks = bActive ? FMath::Min(State.Stacks + 1, FMath::Max(Data.MaxStacks, 1)) : 1;
		if (bActive)
		{
			TargetASC->RemoveActiveGameplayEffect(State.Handle);
		}
		State.Handle = ApplyStats(HeroASC, TargetASC, Data.TargetStats, Data.Duration, State.Stacks);
	}

	if (Data.Status != EZL_ItemStatus::None)
	{
		ZLItemStatus::Apply(HeroASC, TargetASC, Data.Status, Data.StatusDuration);
	}

	if (Data.DotDamagePerSecond > 0.0f || Data.DotMaxHealthPercent > 0.0f)
	{
		const double EndTime = GetTime() + Data.DotDuration;
		FRunningDot* Dot = Dots.FindByPredicate([&Data, Target](const FRunningDot& Existing)
		{
			return Existing.Proc == &Data && Existing.Target == Target;
		});
		if (Dot)
		{
			Dot->EndTime = EndTime;
		}
		else
		{
			Dots.Add({ &Data, Target, Data.DotDamagePerSecond, Data.DotMaxHealthPercent, EndTime });
		}
	}
}

void UZL_ItemProcComponent::ApplyToSelf(FRunningProc& Proc, float Damage)
{
	const FZL_ItemProc& Data = *Proc.Proc;
	UBaseCharAbilitySystemComponent* HeroASC = GetHero()->GetMyAbilitySystemComp();

	const float MaxHealth = GetHero()->GetMyAttributeSet()->GetMaximumHealth();
	const float Heal = Data.Heal + Damage * Data.HealPercentOfDamage / 100.0f + MaxHealth * Data.HealMaxHealthPercent / 100.0f;
	if (Heal > 0.0f)
	{
		HeroASC->ApplyHeal(HeroASC, Heal);
	}
	if (Data.Barrier > 0.0f)
	{
		HeroASC->AddCombatBarrier(Data.Barrier, Data.BarrierDuration);
	}
	if (Data.bCleanse)
	{
		HeroASC->CleanseDebuffs();
	}

	if (!Data.SelfStats.IsEmpty())
	{
		const bool bActive = IsEffectActive(HeroASC, Proc.SelfHandle);
		Proc.SelfStacks = bActive ? FMath::Min(Proc.SelfStacks + 1, FMath::Max(Data.MaxStacks, 1)) : 1;
		if (bActive)
		{
			HeroASC->RemoveActiveGameplayEffect(Proc.SelfHandle);
		}
		Proc.SelfHandle = ApplyStats(HeroASC, HeroASC, Data.SelfStats, Data.Duration, Proc.SelfStacks);
	}
}

void UZL_ItemProcComponent::Update()
{
	const AZeroLockCharacter* Hero = GetHero();
	if (!Hero || Hero->bIsDead || !Hero->GetMyAbilitySystemComp())
	{
		return;
	}

	for (FRunningProc& Proc : Running)
	{
		switch (Proc.Proc->Trigger)
		{
		case EZL_ProcTrigger::While:
			if (Proc.Proc->Duration > 0.0f)
			{
				// Timed: a burst each time the condition holds and the cooldown is over.
				if (PassesConditions(Proc, nullptr, 0.0f, false))
				{
					Fire(Proc, nullptr, 0.0f);
				}
			}
			else
			{
				UpdateHeld(Proc);
			}
			break;
		case EZL_ProcTrigger::Aura:
			UpdateAura(Proc);
			break;
		case EZL_ProcTrigger::Periodic:
			if (PassesConditions(Proc, nullptr, 0.0f, false))
			{
				Fire(Proc, nullptr, 0.0f);
			}
			break;
		default:
			break;
		}
	}

	UpdateDots();
}

void UZL_ItemProcComponent::UpdateHeld(FRunningProc& Proc)
{
	UBaseCharAbilitySystemComponent* HeroASC = GetHero()->GetMyAbilitySystemComp();
	const bool bHeld = IsEffectActive(HeroASC, Proc.SelfHandle);
	const bool bWanted = PassesConditions(Proc, nullptr, 0.0f, false);
	if (bWanted && !bHeld)
	{
		Proc.SelfHandle = ApplyStats(HeroASC, HeroASC, Proc.Proc->SelfStats, 0.0f, 1);
	}
	else if (!bWanted && bHeld)
	{
		HeroASC->RemoveActiveGameplayEffect(Proc.SelfHandle);
		Proc.SelfHandle.Invalidate();
	}
}

void UZL_ItemProcComponent::UpdateAura(FRunningProc& Proc)
{
	AZeroLockCharacter* Hero = GetHero();
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	const FZL_ItemProc& Data = *Proc.Proc;

	const TArray<AZeroLockCharacter*> InRange = FindHeroes(Hero, Hero->GetActorLocation(), Data.Radius, Data.AreaTeam, nullptr, 0);
	TSet<UAbilitySystemComponent*> Wanted;
	for (AZeroLockCharacter* Target : InRange)
	{
		UBaseCharAbilitySystemComponent* TargetASC = Target->GetMyAbilitySystemComp();
		Wanted.Add(TargetASC);
		FTargetState& State = Proc.Targets.FindOrAdd(TWeakObjectPtr<UAbilitySystemComponent>(TargetASC));
		if (!IsEffectActive(TargetASC, State.Handle))
		{
			State.Handle = ApplyStats(HeroASC, TargetASC, Data.TargetStats, 0.0f, 1);
		}
	}

	for (auto It = Proc.Targets.CreateIterator(); It; ++It)
	{
		UAbilitySystemComponent* TargetASC = It.Key().Get();
		if (!Wanted.Contains(TargetASC))
		{
			if (IsEffectActive(TargetASC, It.Value().Handle))
			{
				TargetASC->RemoveActiveGameplayEffect(It.Value().Handle);
			}
			It.RemoveCurrent();
		}
	}
}

void UZL_ItemProcComponent::UpdateDots()
{
	TGuardValue<bool> FiringGuard(bFiring, true);

	UBaseCharAbilitySystemComponent* HeroASC = GetHero()->GetMyAbilitySystemComp();
	for (int32 Index = Dots.Num() - 1; Index >= 0; Index--)
	{
		const FRunningDot Dot = Dots[Index];
		AZeroLockCharacter* Target = Dot.Target.Get();
		if (!Target || Target->bIsDead || GetTime() >= Dot.EndTime)
		{
			Dots.RemoveAt(Index);
			continue;
		}

		const float MaxHealth = Target->GetMyAttributeSet()->GetMaximumHealth();
		const float PerSecond = Dot.PerSecond + MaxHealth * Dot.MaxHealthPercent / 100.0f;
		HeroASC->ApplySpiritDamage(Target->GetMyAbilitySystemComp(), PerSecond * UpdateInterval, this);
	}

	for (int32 Index = Zones.Num() - 1; Index >= 0; Index--)
	{
		const FRunningZone Zone = Zones[Index];
		if (GetTime() >= Zone.EndTime)
		{
			Zones.RemoveAt(Index);
			continue;
		}
		for (AZeroLockCharacter* Target : FindHeroes(GetHero(), Zone.Location, Zone.Radius, EZL_ActiveItemTeam::Enemies, nullptr, 0))
		{
			HeroASC->ApplySpiritDamage(Target->GetMyAbilitySystemComp(), Zone.PerSecond * UpdateInterval, this);
		}
	}
}

void UZL_ItemProcComponent::AddDot(AZeroLockCharacter* Target, float PerSecond, float MaxHealthPercent, float Duration)
{
	Dots.Add({ nullptr, Target, PerSecond, MaxHealthPercent, GetTime() + Duration });
}

void UZL_ItemProcComponent::AddZone(const FVector& Location, float Radius, float Duration, float PerSecond)
{
	Zones.Add({ Location, Radius, PerSecond, GetTime() + Duration });
}
