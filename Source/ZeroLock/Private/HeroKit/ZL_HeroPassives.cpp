// Copyright Preetham Mukundan (C) 2026

#include "HeroKit/ZL_HeroPassives.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/BaseCharAttributeSet.h"
#include "GAS/ZL_GameplayTags.h"
#include "Items/ZL_GE_ItemStats.h"
#include "Mover/ZeroMoverComponent.h"
#include "Mover/ZeroMoverPawn.h"
#include "TimerManager.h"
#include "Zero_BasePlayerState.h"
#include "ZeroLock/ZeroLock.h"
#include "ZeroLock/ZeroLockCharacter.h"

// ---- Yatagarasu: Sunlit Vigil -----------------------------------------------------------------------------------------

void UZL_Yatagarasu_SunlitVigil::OnPassiveStarted()
{
	StartVigil();
}

void UZL_Yatagarasu_SunlitVigil::OnPassiveEnded()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VigilTimer);
	}
}

void UZL_Yatagarasu_SunlitVigil::StartVigil()
{
	GetWorld()->GetTimerManager().SetTimer(VigilTimer, FTimerDelegate::CreateUObject(this, &UZL_Yatagarasu_SunlitVigil::Transform), FMath::Max(VigilTime, 1.0f), false);
}

void UZL_Yatagarasu_SunlitVigil::Transform()
{
	AZeroLockCharacter* Hero = GetHero();
	if (!Hero || Hero->bIsDead)
	{
		return;
	}
	ZLKit::ApplyPayload(this, Hero, Hero, Hero->GetActorLocation(), YokaiPayload);
	// The hourglass starts again once yokai mode is over.
	const float YokaiTime = FMath::Max3(YokaiPayload.StatDuration, YokaiPayload.StatusDuration, YokaiPayload.TagDuration);
	GetWorld()->GetTimerManager().SetTimer(VigilTimer, FTimerDelegate::CreateUObject(this, &UZL_Yatagarasu_SunlitVigil::Transform),
		FMath::Max(VigilTime, 1.0f) + YokaiTime, false);
}

// ---- JA594: Gateway ---------------------------------------------------------------------------------------------------

UZL_JA594_Gateway::UZL_JA594_Gateway()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bIsMovementAbility = true;
}

bool UZL_JA594_Gateway::HasGate() const
{
	return GateExpireTime > 0.0 && GetWorld() && GetWorld()->GetTimeSeconds() < GateExpireTime;
}

void UZL_JA594_Gateway::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!HasGate())
	{
		// First press: open the gate here. Free: the cooldown starts when it's used.
		GateLocation = Hero->GetActorLocation();
		GateExpireTime = GetWorld()->GetTimeSeconds() + GateLifetime;
		if (HasAuthority(&ActivationInfo))
		{
			FZL_KitZoneSpec Marker;
			Marker.Duration = GateLifetime;
			Marker.Radius = 60.0f;
			Marker.Interval = 10.0f;
			Marker.Color = GateColor;
			GateMarker = AZL_Kit_Zone::Spawn(nullptr, Hero, AZL_Kit_Zone::SnapToFloor(GetWorld(), GateLocation), Marker, 1.0f, 1.0f, GetClass()->GetFName());
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// Second press: back through the gate. The owner moves; the server checks the distance like any teleport.
	if (IsLocallyControlled())
	{
		if (UZeroMoverComponent* Mover = Hero->GetZeroMoverComponent())
		{
			Mover->RequestSafeTeleport(GateLocation);
		}
	}
	if (GateMarker.IsValid())
	{
		GateMarker->Destroy();
	}
	GateMarker.Reset();
	GateExpireTime = 0.0;
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ---- JA594: Unholy Machine --------------------------------------------------------------------------------------------

void UZL_JA594_UnholyMachine::OnPassiveStarted()
{
	Streak = 0;
	AwakenUntil = 0.0;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBaseCharAttributeSet::GetCurrentHealthAttribute())
			.AddUObject(this, &UZL_JA594_UnholyMachine::OnHealthChanged);
	}
}

void UZL_JA594_UnholyMachine::OnPassiveEnded()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UBaseCharAttributeSet::GetCurrentHealthAttribute()).Remove(HealthHandle);
	}
	HealthHandle.Reset();
}

void UZL_JA594_UnholyMachine::OnKill(AActor* Victim)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < AwakenUntil)
	{
		return;
	}
	if (++Streak >= KillsToAwaken)
	{
		Streak = 0;
		AwakenUntil = Now + AwakenDuration;
		ZLOG_COLOR_TIME(TEXT("JA594: awakened demon lord"), FColor::Purple, 3.0f);
	}
}

void UZL_JA594_UnholyMachine::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (bApplyingCost || Data.NewValue >= Data.OldValue || Data.NewValue <= 0.0f)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= AwakenUntil || Now < NextExplodeTime)
	{
		return;
	}
	NextExplodeTime = Now + ExplodeCooldown;
	Explode();
}

void UZL_JA594_UnholyMachine::Explode()
{
	AZeroLockCharacter* Hero = GetHero();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Hero || !ASC)
	{
		return;
	}
	FZL_KitArea Area;
	Area.Range = ExplosionRadius;
	TArray<AZeroLockCharacter*> Villans;
	ZLKit::FindHeroes(Hero, Hero->GetActorLocation(), Hero->GetActorForwardVector(), Area, 1.0f, EZL_KitTeam::Enemies, Villans);
	for (AZeroLockCharacter* Villan : Villans)
	{
		ZLKit::ApplyPayload(this, Hero, Villan, Hero->GetActorLocation(), ExplosionPayload);
	}
	ZLKit::DrawArea(GetWorld(), Hero->GetActorLocation(), FVector::ForwardVector, Area, 1.0f, FColor::Purple);

	// The blast costs him a share of his health, never the last point.
	const UBaseCharAttributeSet* Attributes = Hero->GetMyAttributeSet();
	if (Attributes)
	{
		TGuardValue<bool> Guard(bApplyingCost, true);
		const float NewHealth = FMath::Max(1.0f, Attributes->GetCurrentHealth() - Attributes->GetMaximumHealth() * SelfHealthCostPercent / 100.0f);
		ASC->SetNumericAttributeBase(UBaseCharAttributeSet::GetCurrentHealthAttribute(), NewHealth);
	}
}

// ---- Genghis Khan: Cruel Warmonger ------------------------------------------------------------------------------------

void UZL_Genghis_CruelWarmonger::OnPassiveStarted()
{
	AppliedStacks = 0;
	GetWorld()->GetTimerManager().SetTimer(CheckTimer, FTimerDelegate::CreateUObject(this, &UZL_Genghis_CruelWarmonger::Check), 1.0f, true);
}

void UZL_Genghis_CruelWarmonger::OnPassiveEnded()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimer);
	}
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo(); ASC && BuffHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(BuffHandle);
	}
	BuffHandle.Invalidate();
	AppliedStacks = 0;
}

void UZL_Genghis_CruelWarmonger::Check()
{
	AZeroLockCharacter* Hero = GetHero();
	const AZero_BasePlayerState* MyState = Hero ? Hero->GetPlayerState<AZero_BasePlayerState>() : nullptr;
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!MyState || !GameState || !ASC)
	{
		return;
	}

	int32 OwnKills = 0;
	int32 EnemyKills = 0;
	for (const APlayerState* State : GameState->PlayerArray)
	{
		if (const AZero_BasePlayerState* ZeroState = Cast<AZero_BasePlayerState>(State))
		{
			(ZeroState->TeamID == MyState->TeamID ? OwnKills : EnemyKills) += ZeroState->Kills;
		}
	}
	const int32 Difference = EnemyKills - OwnKills;
	const int32 Stacks = Difference >= DifferenceToStart ? FMath::Min(Difference - DifferenceToStart + 1, MaxStacks) : 0;
	if (Stacks == AppliedStacks)
	{
		return;
	}

	if (BuffHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(BuffHandle);
		BuffHandle.Invalidate();
	}
	AppliedStacks = Stacks;
	if (Stacks > 0)
	{
		TArray<FZL_ItemStatValue> Scaled = StatsPerStack;
		for (FZL_ItemStatValue& Stat : Scaled)
		{
			Stat.Value *= Stacks;
		}
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UZL_GE_ItemStats::StaticClass(), 1.0f, ASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			UZL_GE_ItemStats::SetMagnitudes(*Spec.Data, Scaled);
			BuffHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}
}

// ---- Ashura: Monarch --------------------------------------------------------------------------------------------------

void UZL_Ashura_Monarch::OnKill(AActor* Victim)
{
	if (++Kills < KillsForMonarch)
	{
		return;
	}
	Kills = 0;
	AZeroLockCharacter* Hero = GetHero();
	if (!Hero)
	{
		return;
	}
	for (TActorIterator<AZeroLockCharacter> It(GetWorld()); It; ++It)
	{
		if (ZLKit::MatchesTeam(Hero, *It, EZL_KitTeam::Enemies))
		{
			ZLKit::ApplyPayload(this, Hero, *It, It->GetActorLocation(), BlazePayload);
		}
	}
	ZLOG_COLOR_TIME(TEXT("Ashura: MONARCH - the battlefield burns"), FColor::Red, 3.0f);
}

// ---- Ryu: The Beast Awakens -------------------------------------------------------------------------------------------

void UZL_Ryu_BeastAwakens::OnPassiveStarted()
{
	Used.Reset();
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		CommittedHandle = ASC->AbilityCommittedCallbacks.AddUObject(this, &UZL_Ryu_BeastAwakens::OnAbilityCommitted);
	}
}

void UZL_Ryu_BeastAwakens::OnPassiveEnded()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AbilityCommittedCallbacks.Remove(CommittedHandle);
	}
	CommittedHandle.Reset();
	Used.Reset();
}

void UZL_Ryu_BeastAwakens::OnAbilityCommitted(UGameplayAbility* Ability)
{
	const FGameplayAbilitySpec* Spec = Ability ? Ability->GetCurrentAbilitySpec() : nullptr;
	if (!Spec)
	{
		return;
	}
	switch (static_cast<EGASAbilityInputID>(Spec->InputID))
	{
	case EGASAbilityInputID::Secondry_Attack:
	case EGASAbilityInputID::Ability_1:
	case EGASAbilityInputID::Ability_2:
	case EGASAbilityInputID::Ultimate:
		break;
	default:
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastUse > ChainWindow)
	{
		Used.Reset();
	}
	LastUse = Now;
	Used.Add(Spec->InputID);
	if (Used.Num() >= 4)
	{
		Used.Reset();
		if (AZeroLockCharacter* Hero = GetHero())
		{
			FZL_KitPayload Heal;
			Heal.HealMaxHealthPercent = HealMaxHealthPercent;
			ZLKit::ApplyPayload(this, Hero, Hero, Hero->GetActorLocation(), Heal);
			ZLOG_COLOR_TIME(TEXT("Ryu: the beast awakens"), FColor::Cyan, 3.0f);
		}
	}
}

// ---- Kronos: Looping Past ---------------------------------------------------------------------------------------------

void UZL_Kronos_LoopingPast::OnPassiveStarted()
{
	History.Reset();
	NextLoop = GetWorld()->GetTimeSeconds() + LoopInterval;
	GetWorld()->GetTimerManager().SetTimer(SampleTimer, FTimerDelegate::CreateUObject(this, &UZL_Kronos_LoopingPast::Sample), 0.25f, true);
}

void UZL_Kronos_LoopingPast::OnPassiveEnded()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SampleTimer);
	}
	History.Reset();
}

void UZL_Kronos_LoopingPast::Sample()
{
	AZeroLockCharacter* Hero = GetHero();
	const UBaseCharAttributeSet* Attributes = Hero ? Hero->GetMyAttributeSet() : nullptr;
	if (!Attributes || Hero->bIsDead)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const float Health = Attributes->GetCurrentHealth();
	History.Add({ Now, Health });
	History.RemoveAll([Now, this](const TPair<double, float>& Entry) { return Now - Entry.Key > LookBack + 1.0; });

	if (Now < NextLoop)
	{
		return;
	}
	NextLoop = Now + LoopInterval;

	// The sample closest to LookBack seconds ago.
	float Past = Health;
	double BestError = TNumericLimits<double>::Max();
	for (const TPair<double, float>& Entry : History)
	{
		const double Error = FMath::Abs(Now - Entry.Key - LookBack);
		if (Error < BestError)
		{
			BestError = Error;
			Past = Entry.Value;
		}
	}
	if (Past > Health)
	{
		FZL_KitPayload Rewind;
		Rewind.Heal = Past - Health;
		ZLKit::ApplyPayload(this, Hero, Hero, Hero->GetActorLocation(), Rewind);
		ZLOG_COLOR_TIME(TEXT("Kronos: looping past"), FColor::Yellow, 2.0f);
	}
}

// ---- Mephisto: Spatial Rift -------------------------------------------------------------------------------------------

UZL_Mephisto_SpatialRift::UZL_Mephisto_SpatialRift()
{
	TargetTeam = EZL_ConeTargetTeam::Any;
}

void UZL_Mephisto_SpatialRift::ConeTargetConfirmed(AZeroLockCharacter* Target, const FGameplayAbilityTargetDataHandle& Data)
{
	// Not Super: it would end the ability. The rift stays open for the second press.
	Marked = Target;
	if (!Target)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}
	if (HasAuthority(&CurrentActivationInfo))
	{
		if (AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()))
		{
			ZLKit::ApplyPayload(this, Hero, Target, Hero->GetActorLocation(), TargetPayload, PowerScale.GetValueAtLevel(GetAbilityLevel()));
		}
	}

	UAbilityTask_WaitInputPress* PressTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	PressTask->OnPress.AddDynamic(this, &UZL_Mephisto_SpatialRift::OnRiftPressed);
	PressTask->ReadyForActivation();

	UAbilityTask_WaitDelay* Timeout = UAbilityTask_WaitDelay::WaitDelay(this, RiftWindow);
	Timeout->OnFinish.AddDynamic(this, &UZL_Mephisto_SpatialRift::OnRiftExpired);
	Timeout->ReadyForActivation();
}

void UZL_Mephisto_SpatialRift::OnRiftPressed(float TimeWaited)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	AZeroLockCharacter* Target = Marked.Get();
	if (Hero && Target && !Target->bIsDead && HasAuthority(&CurrentActivationInfo))
	{
		// Server: the aim it was sent, from his eye, onto the floor.
		const FVector Eye = Hero->GetActorLocation() + FVector(0, 0, 60);
		const FVector End = Eye + GetHeroAimDirection().GetSafeNormal() * RiftRange * GetAbilityRangeScale();
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ZLRift), false, Hero);
		FVector Point = GetWorld()->LineTraceSingleByObjectType(Hit, Eye, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params)
			? Hit.ImpactPoint + Hit.ImpactNormal * 60.0f : End;
		Point = AZL_Kit_Zone::SnapToFloor(GetWorld(), Point) + FVector(0, 0, 95);
		const FRotator Facing = Target->GetController() ? Target->GetController()->GetControlRotation() : Target->GetActorRotation();
		Target->ServerForceTeleport(Point, Facing);
	}
	Marked.Reset();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZL_Mephisto_SpatialRift::OnRiftExpired()
{
	Marked.Reset();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// ---- Tempest: Wind Rider ----------------------------------------------------------------------------------------------

UZL_Tempest_WindRider::UZL_Tempest_WindRider()
{
	TargetStyle = EGASTargetConfirmationStyle::Passive;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UZL_Tempest_WindRider::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	if (ActorInfo && ActorInfo->IsLocallyControlled())
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
	}
}

void UZL_Tempest_WindRider::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bGliding = false;
	GetWorld()->GetTimerManager().SetTimer(UpdateTimer, FTimerDelegate::CreateUObject(this, &UZL_Tempest_WindRider::Update), 0.05f, true);
}

void UZL_Tempest_WindRider::Update()
{
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const UZeroMoverComponent* Mover = Hero ? Hero->GetZeroMoverComponent() : nullptr;
	if (!Hero || !ASC || !Mover)
	{
		return;
	}
	const float VerticalSpeed = Mover->GetVelocity().Z;
	const bool bWant = !Hero->bIsDead && Hero->IsJumpInputHeld() && (bGliding ? VerticalSpeed <= 0.0f : VerticalSpeed < -MinFallSpeed);
	if (bWant == bGliding)
	{
		return;
	}
	bGliding = bWant;
	if (bGliding)
	{
		ASC->AddLooseGameplayTag(ZerolockGameplayTagsForBinding::TAG_STATUS_FLOATING);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(ZerolockGameplayTagsForBinding::TAG_STATUS_FLOATING);
	}
}

void UZL_Tempest_WindRider::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
	}
	if (bGliding)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(ZerolockGameplayTagsForBinding::TAG_STATUS_FLOATING);
		}
		bGliding = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---- Kai Koa: Frozen Fist ---------------------------------------------------------------------------------------------

UZL_GE_KaiKoa_Frost::UZL_GE_KaiKoa_Frost()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(3.0f));
	// The setters are editor-only, and a native effect needs these in game builds too.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 20;
}

int32 UZL_GE_KaiKoa_Frost::GetStacks(UAbilitySystemComponent* TargetASC)
{
	return TargetASC ? TargetASC->GetGameplayEffectCount(UZL_GE_KaiKoa_Frost::StaticClass(), nullptr) : 0;
}

void UZL_KaiKoa_FrozenFist::OnPassiveStarted()
{
	if (UBaseCharAbilitySystemComponent* ASC = GetHero() ? GetHero()->GetMyAbilitySystemComp() : nullptr)
	{
		FrostHandle = ASC->OnDamageDealt.AddUObject(this, &UZL_KaiKoa_FrozenFist::OnFrostHit);
	}
}

void UZL_KaiKoa_FrozenFist::OnPassiveEnded()
{
	if (UBaseCharAbilitySystemComponent* ASC = GetHero() ? GetHero()->GetMyAbilitySystemComp() : nullptr)
	{
		ASC->OnDamageDealt.Remove(FrostHandle);
	}
	FrostHandle.Reset();
	LastStackTime.Reset();
	FrozenUntil.Reset();
}

void UZL_KaiKoa_FrozenFist::OnFrostHit(AZeroLockCharacter* Target, float Damage, EZL_ProcTrigger Trigger, const UObject* DamageSource)
{
	AZeroLockCharacter* Hero = GetHero();
	if (DamageSource == this || !Hero || !IsValid(Target) || Target->bIsDead || !FrostTriggers.Contains(Trigger) || Hero->IsOnSameTeam(Target))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Until = FrozenUntil.Find(Target); Until && Now < *Until)
	{
		return;
	}
	if (const double* Last = LastStackTime.Find(Target); Last && Now - *Last < StackCooldownPerTarget)
	{
		return;
	}
	LastStackTime.Add(Target, Now);

	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
	if (!HeroASC || !TargetASC)
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = HeroASC->MakeOutgoingSpec(UZL_GE_KaiKoa_Frost::StaticClass(), 1.0f, HeroASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetDuration(FMath::Max(FrostDecayTime, 0.1f), true);
	Spec.Data->DynamicGrantedTags.AddTag(DebuffStackTag.IsValid() ? DebuffStackTag : FGameplayTag(ZerolockGameplayTagsForBinding::TAG_DEBUFFSTACK_FROST));
	for (int32 i = 0; i < FMath::Max(StacksPerHit, 1); ++i)
	{
		HeroASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
	}

	if (UZL_GE_KaiKoa_Frost::GetStacks(TargetASC) >= FMath::Max(StacksToFreeze, 1))
	{
		TargetASC->RemoveActiveGameplayEffectBySourceEffect(UZL_GE_KaiKoa_Frost::StaticClass(), nullptr);
		FrozenUntil.Add(Target, Now + FreezePayload.StatusDuration);
		ZLKit::ApplyPayload(this, Hero, Target, Hero->GetActorLocation(), FreezePayload);
	}
	else if (StackCue.IsSet())
	{
		ZLKitCue::Play(TargetASC, StackCue, Target->GetActorLocation(), (Target->GetActorLocation() - Hero->GetActorLocation()).Rotation());
	}
}
