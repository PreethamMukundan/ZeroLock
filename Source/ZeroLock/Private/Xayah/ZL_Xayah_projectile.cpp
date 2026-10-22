// Copyright Preetham Mukundan (C) 2026


#include "Xayah/ZL_Xayah_projectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "ZeroLock/ZeroLockCharacter.h"


AZL_Xayah_projectile::AZL_Xayah_projectile()
{
	SetReplicates(true);
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(GetRootComponent());

}


void AZL_Xayah_projectile::StartReturn(AActor* TargetActor)
{
	if (!HasAuthority() || !TargetActor) return;

	bIsReturning = true;
	
	SetLifeSpan(2.0f);
	
	ProjectileMovement->UpdatedComponent = CollisionComp;
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Overlap);
	const float Speed = 20000.f;
	ProjectileMovement->MaxSpeed = Speed;
	FVector Direction = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	ProjectileMovement->Velocity = Direction * Speed;
	ProjectileMovement->bIsHomingProjectile = true;
	ProjectileMovement->HomingTargetComponent = TargetActor->GetRootComponent();
	ProjectileMovement->HomingAccelerationMagnitude = 10000.f;
	ProjectileMovement->Activate(true);
	ProjectileMovement->UpdateComponentVelocity();

}

void AZL_Xayah_projectile::SetAutoPull(bool bBool)
{
	if (bBool == bIsActivatedToPull)return;
	if (GetLocalRole() < ROLE_Authority) return;

	bIsActivatedToPull = bBool;

	OnRep_IsActivatedToPull();
}

void AZL_Xayah_projectile::BeginPlay()
{
	SetActorTickEnabled(true);
	Super::BeginPlay();
	SetReplicateMovement(true);
	bIsReturning =false;
	CheckISAutoActiavted();
}

void AZL_Xayah_projectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZL_Xayah_projectile, bIsReturning);
	DOREPLIFETIME(AZL_Xayah_projectile, bIsActivatedToPull);
}

void AZL_Xayah_projectile::OnRep_IsActivatedToPull()
{
	if (bIsActivatedToPull)
	{
		if (HighlightMaterial && MeshComp)
		{
			MeshComp->SetMaterial(0, HighlightMaterial);
		}
	}
}

void AZL_Xayah_projectile::SendGameplayEventToACtor()
{
	SetAutoPull(true);
	ProjectileMovement->StopMovementImmediately();
	if (AActor* MyOwner = GetOwner())
	{
		FGameplayEventData Payload;
		Payload.OptionalObject = this;
		Payload.Target = MyOwner;
		Payload.EventTag = FGameplayTag::RequestGameplayTag("Event.Xayah.FeatherDown",false);
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MyOwner,FGameplayTag::RequestGameplayTag(FName("Event.Xayah.FeatherDown")),Payload);
	}
}

void AZL_Xayah_projectile::CheckISAutoActiavted()
{
	if (bIsActivatedToPull) return;
	if (AZeroLockCharacter* myHero = Cast<AZeroLockCharacter>(GetOwner()))
	{
		OwnerCharacter = myHero;
		if (OwnerCharacter ->GetAbilitySystemComponent()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Event.Xayah.ActivateFeather"),false)))
		{
			if (HasAuthority())
			{
				SetAutoPull(true);
				OwnerCharacter->GetMyAbilitySystemComp()->RemoveActiveGameplayEffectBySourceEffect(FeatherDownClassClass,OwnerCharacter->GetMyAbilitySystemComp(),1);
			}
		}
	}

}

void AZL_Xayah_projectile::HitEventCallBack(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                            UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bIsActivatedToPull)
	{
		SendGameplayEventToACtor();
	}
}


void AZL_Xayah_projectile::OverlapEventCallBack(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{

	if (!OwnerCharacter)
	{
		if (AZeroLockCharacter* Char = Cast<AZeroLockCharacter>(GetOwner()))
		{
			OwnerCharacter = Char;
		}
		else
		{
			Destroy();
			return;
		}
	}
	if (OwnerCharacter == OtherActor && bIsReturning)
	{
		Destroy();
	}
	if (AZeroLockCharacter* enemy = Cast<AZeroLockCharacter>(OtherActor))
	{
		if (bIsReturning)
		{
			if (enemy == OwnerCharacter)
			{
				Destroy();
			}
			else
			{
				OwnerCharacter->GetMyAbilitySystemComp()->ApplySpiritDamage(enemy->GetMyAbilitySystemComp(),5, this);
			}
		}
		else
		{
			
			OwnerCharacter->GetMyAbilitySystemComp()->ApplyWeaponDamage(enemy->GetMyAbilitySystemComp(),1, this);

			if (!bIsActivatedToPull)
			{
				Destroy();
			}
		}
		
	}
	
}

