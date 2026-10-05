// Copyright Preetham Mukundan (C) 2026


#include "Pocket/ZL_Pocket_EnchantersSatchel.h"

#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/Tasks/ZL_WaitDelay_Task.h"
#include "Mover/ZeroMoverComponent.h"
#include "Pocket/ZL_Pocket_SuitcaseActor.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Pocket_EnchantersSatchel::UZL_Pocket_EnchantersSatchel()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	SuitcaseClass = AZL_Pocket_SuitcaseActor::StaticClass();

	// Held for the whole ability: no damage taken, the mover keeps Pocket in place (teleports still go through), and no other
	// ability can be activated. A Flying Cloak thrown before going in is already active, so its teleport press still works.
	ActivationOwnedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_UNTOUCHABLE);
	ActivationOwnedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_MOVEMENT_ROOTED);
	ActivationOwnedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_ABILITIES_BLOCKED);
}

void UZL_Pocket_EnchantersSatchel::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                   const FGameplayAbilityActorInfo* ActorInfo,
                                                   const FGameplayAbilityActivationInfo ActivationInfo,
                                                   const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bInSuitcase = true;
	SpawnSuitcase(Hero);

	MaxDurationTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this, MaxDuration.GetValueAtLevel(GetAbilityLevel()));
	MaxDurationTask->OnProgress.AddDynamic(this, &UZL_Pocket_EnchantersSatchel::UpdateProgressionTimer);
	MaxDurationTask->OnStarted.AddDynamic(this, &UZL_Pocket_EnchantersSatchel::StartProgressionTimer);
	MaxDurationTask->OnEnd.AddDynamic(this, &UZL_Pocket_EnchantersSatchel::StopProgressionTimer);
	MaxDurationTask->OnFinished.AddDynamic(this, &UZL_Pocket_EnchantersSatchel::OnMaxDurationFinished);
	MaxDurationTask->ReadyForActivation();

	ListenForExit(Hero);
}

void UZL_Pocket_EnchantersSatchel::SpawnSuitcase(AZeroLockCharacter* Hero)
{
	if (!SuitcaseClass)
	{
		return;
	}

	// The server's suitcase replicates to everyone but the owner; a remote owner spawns its own local one.
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Hero;
	SpawnParams.Instigator = Hero;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	SuitcaseActor = GetWorld()->SpawnActor<AZL_Pocket_SuitcaseActor>(SuitcaseClass, Hero->GetActorTransform(), SpawnParams);
}

void UZL_Pocket_EnchantersSatchel::ListenForExit(AZeroLockCharacter* Hero)
{
	// Pressing the ability input again. Works on the client and (through input replication) the server.
	InputPressedTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	InputPressedTask->OnPress.AddDynamic(this, &UZL_Pocket_EnchantersSatchel::OnAbilityInputPressed);
	InputPressedTask->ReadyForActivation();

	// Jump only exists as mover input, so only the owning client sees it. Its EndAbility is replicated to the server.
	if (IsLocallyControlled())
	{
		JumpPressedHandle = Hero->OnJumpInputPressed.AddUObject(this, &UZL_Pocket_EnchantersSatchel::OnJumpPressed);
	}

	// A Flying Cloak teleport carries the suitcase to the cloak; Pocket comes out as soon as it lands.
	if (UZeroMoverComponent* MoverComp = Hero->GetZeroMoverComponent())
	{
		MoverComp->OnTeleportSucceeded.AddDynamic(this, &UZL_Pocket_EnchantersSatchel::OnTeleportSucceeded);
		MoverComp->OnTeleportFailed.AddDynamic(this, &UZL_Pocket_EnchantersSatchel::OnTeleportFailed);
	}
}

void UZL_Pocket_EnchantersSatchel::OnJumpPressed()
{
	// Jump only opens the suitcase; swallow the press so it doesn't also trigger a jump.
	if (AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()))
	{
		Hero->ConsumeJumpPress();
	}
	ExitSuitcase(true);
}

void UZL_Pocket_EnchantersSatchel::OnAbilityInputPressed(float TimeWaited)
{
	ExitSuitcase(true);
}

void UZL_Pocket_EnchantersSatchel::OnMaxDurationFinished()
{
	ExitSuitcase(true);
}

void UZL_Pocket_EnchantersSatchel::OnTeleportSucceeded(const FVector& FromLocation, const FQuat& FromRotation, const FVector& ToLocation, const FQuat& ToRotation)
{
	/* A remote client doesn't replicate this end: its EndAbility RPC could reach the server before the movement input
	 * carrying the teleport, which would make the server come out (and deal damage) at the old location. The server
	 * runs the same teleport from that input and comes out on its own. */
	const bool bIsRemoteClient = !HasAuthority(&CurrentActivationInfo);
	ExitSuitcase(!bIsRemoteClient);
}

void UZL_Pocket_EnchantersSatchel::OnTeleportFailed(const FVector& FromLocation, const FQuat& FromRotation, const FVector& ToLocation, const FQuat& ToRotation, ETeleportFailureReason TeleportFailureReason)
{
	// The teleport didn't happen, so come out where we are.
	ExitSuitcase(true);
}

void UZL_Pocket_EnchantersSatchel::ExitSuitcase(bool bReplicateEndAbility)
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), bReplicateEndAbility, false);
	}
}

void UZL_Pocket_EnchantersSatchel::FindExitTargets(AZeroLockCharacter* Hero,TSet<AZeroLockCharacter*>& OutVillans) const
{
	const float Radius = ExitDamageRadius.GetValueAtLevel(GetAbilityLevel());
	FCollisionQueryParams QueryParams(FName(TEXT("SatchelExit")), false, Hero);

	TArray<FOverlapResult> Overlaps;
	// Heroes use the Hero object type (ECC_GameTraceChannel1), and the Hero profile ignores the Hero trace channel, so query by object type.
	GetWorld()->OverlapMultiByObjectType(Overlaps, Hero->GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1), FCollisionShape::MakeSphere(Radius), QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
		if (IsValid(Villan) && Villan != Hero && !Hero->IsOnSameTeam(Villan) && Villan->GetMyAbilitySystemComp())
		{
			OutVillans.Add(Villan);
		}
	}
}

void UZL_Pocket_EnchantersSatchel::ApplyExitDamage(AZeroLockCharacter* Hero) const
{
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	if (!HeroASC)
	{
		return;
	}

	TSet<AZeroLockCharacter*> Villans;
	FindExitTargets(Hero, Villans);

	const float Damage = SpiritDamage.GetValueAtLevel(GetAbilityLevel());
	for (AZeroLockCharacter* Villan : Villans)
	{
		HeroASC->ApplySpiritDamage(Villan->GetMyAbilitySystemComp(), Damage);
	}

	DrawExitDebug(Hero, Villans.Num() > 0);
}

void UZL_Pocket_EnchantersSatchel::DrawExitDebug(const AZeroLockCharacter* Hero, bool bHitAnyone) const
{
	if (bDrawDebugExitDamage)
	{
		DrawDebugSphere(GetWorld(), Hero->GetActorLocation(), ExitDamageRadius.GetValueAtLevel(GetAbilityLevel()), 24, bHitAnyone ? FColor::Green : FColor::Red, false, 2.0f);
	}
}

void UZL_Pocket_EnchantersSatchel::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                              const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UZeroMoverComponent* MoverComp = Hero ? Hero->GetZeroMoverComponent() : nullptr;

	// Coming out of the suitcase, for any reason, always pops: damage around wherever Pocket is now.
	if (bInSuitcase)
	{
		bInSuitcase = false;

		if (Hero && HasAuthority(&ActivationInfo))
		{
			ApplyExitDamage(Hero);
		}
		else if (Hero && bDrawDebugExitDamage && IsLocallyControlled())
		{
			// A remote client doesn't deal damage, but draws the sphere colored by what its own overlap finds.
			TSet<AZeroLockCharacter*> Villans;
			FindExitTargets(Hero, Villans);
			DrawExitDebug(Hero, Villans.Num() > 0);
		}

		// The rooted tag is removed by Super, so the mover takes this mode on its next tick.
		if (MoverComp)
		{
			MoverComp->QueueNextMode(ExitModeName);
		}
	}

	if (MoverComp)
	{
		MoverComp->OnTeleportSucceeded.RemoveDynamic(this, &UZL_Pocket_EnchantersSatchel::OnTeleportSucceeded);
		MoverComp->OnTeleportFailed.RemoveDynamic(this, &UZL_Pocket_EnchantersSatchel::OnTeleportFailed);
	}

	if (JumpPressedHandle.IsValid())
	{
		if (Hero)
		{
			Hero->OnJumpInputPressed.Remove(JumpPressedHandle);
		}
		JumpPressedHandle.Reset();
	}

	if (AZL_Pocket_SuitcaseActor* Suitcase = SuitcaseActor.Get())
	{
		Suitcase->Destroy();
	}
	SuitcaseActor.Reset();

	if (MaxDurationTask && MaxDurationTask->IsActive())
	{
		MaxDurationTask->OnFinished.RemoveDynamic(this, &UZL_Pocket_EnchantersSatchel::OnMaxDurationFinished);
		MaxDurationTask->EndTask();
	}
	MaxDurationTask = nullptr;

	if (InputPressedTask && InputPressedTask->IsActive())
	{
		InputPressedTask->OnPress.RemoveDynamic(this, &UZL_Pocket_EnchantersSatchel::OnAbilityInputPressed);
		InputPressedTask->EndTask();
	}
	InputPressedTask = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
