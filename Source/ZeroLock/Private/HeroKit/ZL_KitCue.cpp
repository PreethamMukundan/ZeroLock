// Copyright Preetham Mukundan (C) 2026

#include "HeroKit/ZL_KitCue.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameplayCueManager.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

FGameplayCueParameters ZLKitCue::MakeParams(AActor* Instigator, const FVector& Location, const FRotator& Facing, float Radius)
{
	FGameplayCueParameters Params;
	Params.Location = Location;
	Params.Normal = FRotator(0.f, Facing.Yaw, 0.f).Vector();
	Params.RawMagnitude = Radius;
	Params.Instigator = Instigator;
	Params.EffectCauser = Instigator;
	return Params;
}

void ZLKitCue::Play(UAbilitySystemComponent* ASC, const FZL_KitCue& Cue, const FVector& Location, const FRotator& Facing, float Radius)
{
	if (!ASC || !Cue.IsSet())
	{
		return;
	}
	const FGameplayCueParameters Params = MakeParams(ASC->GetAvatarActor(), Location, Facing, Radius);
	if (Cue.Duration <= 0.f)
	{
		ASC->ExecuteGameplayCue(Cue.Cue, Params);
		return;
	}

	// Timed loop: the server adds it (replicated to everyone) and takes it off again.
	if (!ASC->IsOwnerActorAuthoritative())
	{
		return;
	}
	ASC->AddGameplayCue(Cue.Cue, Params);
	UWorld* World = ASC->GetWorld();
	if (!World)
	{
		return;
	}
	TWeakObjectPtr<UAbilitySystemComponent> WeakASC(ASC);
	const FGameplayTag Tag = Cue.Cue;
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(ASC, [WeakASC, Tag]()
	{
		if (WeakASC.IsValid())
		{
			WeakASC->RemoveGameplayCue(Tag);
		}
	}), Cue.Duration, false);
}

void ZLKitCue::AddLocal(AActor* Target, const FGameplayTag& Cue, float Radius)
{
	if (!Target || !Cue.IsValid() || Target->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (UGameplayCueManager* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
	{
		Manager->AddGameplayCue_NonReplicated(Target, Cue, MakeParams(Target->GetInstigator(), Target->GetActorLocation(), Target->GetActorRotation(), Radius));
	}
}

void ZLKitCue::RemoveLocal(AActor* Target, const FGameplayTag& Cue)
{
	if (!Target || !Cue.IsValid() || Target->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (UGameplayCueManager* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
	{
		Manager->RemoveGameplayCue_NonReplicated(Target, Cue, FGameplayCueParameters());
	}
}

void ZLKitCue::ExecuteLocal(AActor* Target, const FGameplayTag& Cue, const FVector& Location, const FRotator& Facing, float Radius)
{
	if (!Target || !Cue.IsValid() || Target->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (UGameplayCueManager* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
	{
		Manager->ExecuteGameplayCue_NonReplicated(Target, Cue, MakeParams(Target->GetInstigator(), Location, Facing, Radius));
	}
}

// ---------------------------------------------------------------------------------------------------------------------

UNiagaraComponent* FZL_KitCueVisual::Spawn(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	UWorld* World = Target ? Target->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	const float Radius = Parameters.RawMagnitude;
	float FinalScale = Scale;
	if (bScaleWithRadius && Radius > 0.f && ReferenceRadius > 0.f)
	{
		FinalScale *= Radius / ReferenceRadius;
	}
	FVector Facing = Parameters.Normal.GetSafeNormal2D();
	if (Facing.IsNearlyZero())
	{
		Facing = Target->GetActorForwardVector().GetSafeNormal2D();
	}
	const FVector Base = Parameters.Location.IsNearlyZero() ? Target->GetActorLocation() : FVector(Parameters.Location);
	const FVector Where = Base + Facing * ForwardOffset + FVector::UpVector * UpOffset;

	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(Target, Sound, Where);
	}
	if (!NiagaraSystem)
	{
		return nullptr;
	}

	UNiagaraComponent* Component = nullptr;
	if (bAttachToTarget && Target->GetRootComponent())
	{
		Component = UNiagaraFunctionLibrary::SpawnSystemAttached(NiagaraSystem, Target->GetRootComponent(), NAME_None,
			Where - Target->GetActorLocation(), Facing.Rotation(), EAttachLocation::KeepRelativeOffset, true);
		if (Component)
		{
			// Relative offset was taken in world space above; put it back in the attach parent's space.
			Component->SetWorldLocationAndRotation(Where, Facing.Rotation());
			Component->SetWorldScale3D(FVector(FinalScale));
		}
	}
	else
	{
		Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, NiagaraSystem, Where, Facing.Rotation(), FVector(FinalScale), true);
	}
	if (!Component)
	{
		return nullptr;
	}
	for (const TPair<FName, float>& Parameter : FloatParameters)
	{
		Component->SetVariableFloat(Parameter.Key, Parameter.Value);
	}
	if (!RadiusParameter.IsNone() && Radius > 0.f)
	{
		// Niagara shapes are in the component's space, which FinalScale already scales.
		Component->SetVariableFloat(RadiusParameter, Radius / FMath::Max(FinalScale, 0.01f));
	}
	return Component;
}

bool UZL_GCN_KitBurst::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	Visual.Spawn(MyTarget, Parameters);
	return true;
}

AZL_GCN_KitLoop::AZL_GCN_KitLoop()
{
	bAutoDestroyOnRemove = true;
	AutoDestroyDelay = 2.f;
}

bool AZL_GCN_KitLoop::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	FZL_KitCueVisual Attached = Visual;
	Attached.bAttachToTarget = true;
	FGameplayCueParameters AtTarget = Parameters;
	// Loops sit on whoever they're added to.
	AtTarget.Location = MyTarget ? MyTarget->GetActorLocation() : FVector::ZeroVector;
	Spawned = Attached.Spawn(MyTarget, AtTarget);
	return true;
}

bool AZL_GCN_KitLoop::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (Spawned)
	{
		Spawned->Deactivate();
		Spawned = nullptr;
	}
	return true;
}
