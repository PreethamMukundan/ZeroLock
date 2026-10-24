// Copyright Preetham Mukundan (C) 2026


#include "Viscous/ZL_Viscous_Cube.h"

#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/ZL_StatusTimerAbility.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Viscous/ZL_Viscous_CubeEffect.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Viscous_Cube::AZL_Viscous_Cube()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);

	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	SetRootComponent(Blocker);
	Blocker->SetBoxExtent(CubeExtent);
	Blocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Blocker->SetCollisionObjectType(ECC_Pawn);
	Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Other heroes' movement sweeps use their capsule's object type: Hero (ECC_GameTraceChannel1) or Pawn.
	Blocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Blocker->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	Blocker->SetGenerateOverlapEvents(false);
	Blocker->CanCharacterStepUpOn = ECB_No;

	CubeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CubeMesh"));
	CubeMesh->SetupAttachment(Blocker);
	CubeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CubeMesh->SetGenerateOverlapEvents(false);
	CubeMesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BoxMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (BoxMesh.Succeeded())
	{
		CubeMesh->SetStaticMesh(BoxMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		CubeMesh->SetMaterial(0, ShapeMaterial.Object);
	}

	CubeEffect = UZL_Viscous_CubeEffect::StaticClass();
}

void AZL_Viscous_Cube::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZL_Viscous_Cube, CubedHero);
}

void AZL_Viscous_Cube::BeginPlay()
{
	Super::BeginPlay();

	Blocker->SetBoxExtent(CubeExtent);
	// Engine Cube: 100 cm, centered.
	CubeMesh->SetRelativeScale3D(CubeExtent * 2.0f / 100.0f);
	if (UMaterialInstanceDynamic* MID = CubeMesh->GetMaterial(0) ? CubeMesh->CreateDynamicMaterialInstance(0) : nullptr)
	{
		MID->SetVectorParameterValue(TEXT("Color"), CubeColor);
	}
	AttachToHero();
}

void AZL_Viscous_Cube::OnRep_CubedHero()
{
	AttachToHero();
}

void AZL_Viscous_Cube::AttachToHero()
{
	AZeroLockCharacter* Hero = CubedHero;
	if (!IsValid(Hero))
	{
		return;
	}
	// On clients the attachment usually replicates before this runs: still set up the ignore and the pose below,
	// or the hero's capsule collides with its own cube (penetration fix-ups shove it around and the server corrects it).
	if (GetAttachParentActor() != Hero)
	{
		AttachToActor(Hero, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	if (UPrimitiveComponent* HeroRoot = Cast<UPrimitiveComponent>(Hero->GetRootComponent()))
	{
		HeroRoot->IgnoreActorWhenMoving(this, true);
	}
	// Frozen in the goo: the pose holds until the cube ends.
	if (USkeletalMeshComponent* HeroMesh = Hero->FindComponentByClass<USkeletalMeshComponent>())
	{
		HeroMesh->bPauseAnims = true;
	}
}

void AZL_Viscous_Cube::StartCube(AZeroLockCharacter* InHealer, AZeroLockCharacter* InHero, float InDuration, float InHealPerSecond)
{
	UBaseCharAbilitySystemComponent* HeroASC = IsValid(InHero) ? InHero->GetMyAbilitySystemComp() : nullptr;
	if (!HasAuthority() || !HeroASC || !CubeEffect)
	{
		Destroy();
		return;
	}

	CubedHero = InHero;
	Healer = InHealer;
	HealPerSecond = InHealPerSecond;
	AttachToHero();

	// From Viscous's ASC when he's around, else the hero's own (self-cast, or Viscous gone).
	UBaseCharAbilitySystemComponent* SourceASC = IsValid(InHealer) && InHealer->GetMyAbilitySystemComp() ? InHealer->GetMyAbilitySystemComp() : HeroASC;
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(this);
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(CubeEffect, 1.0f, Context);
	if (Spec.IsValid())
	{
		Spec.Data->DynamicGrantedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_UNTOUCHABLE);
		Spec.Data->DynamicGrantedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_STATUS_CUBED);
		EffectHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), HeroASC);
	}

	UZL_StatusTimerAbility::SendStatusTimer(InHealer ? static_cast<AActor*>(InHealer) : InHero, HeroASC, ZerolockGameplayTagsForBinding::TAG_STATUS_CUBED, InDuration);

	EscapeHandle = InHero->OnServerJumpEscapeRequested.AddUObject(this, &AZL_Viscous_Cube::EndCube);
	PreDeathHandle = InHero->OnServerPreDeath.AddUObject(this, &AZL_Viscous_Cube::HandleHeroPreDeath);

	GetWorldTimerManager().SetTimer(EndTimer, this, &AZL_Viscous_Cube::EndCube, FMath::Max(InDuration, 0.1f), false);
	if (HealPerSecond > 0.0f && HealInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(HealTimer, this, &AZL_Viscous_Cube::HealTick, HealInterval, true);
	}
}

void AZL_Viscous_Cube::HealTick()
{
	AZeroLockCharacter* Hero = CubedHero;
	UBaseCharAbilitySystemComponent* HeroASC = IsValid(Hero) && !Hero->bIsDead ? Hero->GetMyAbilitySystemComp() : nullptr;
	if (!HeroASC)
	{
		return;
	}
	AZeroLockCharacter* HealerHero = Healer.Get();
	UBaseCharAbilitySystemComponent* SourceASC = IsValid(HealerHero) && HealerHero->GetMyAbilitySystemComp() ? HealerHero->GetMyAbilitySystemComp() : HeroASC;
	SourceASC->ApplyHeal(HeroASC, HealPerSecond * HealInterval);
}

void AZL_Viscous_Cube::HandleHeroPreDeath(AZeroLockCharacter* DeadHero)
{
	EndCube();
}

void AZL_Viscous_Cube::EndCube()
{
	if (!HasAuthority())
	{
		return;
	}
	CleanUpCube();
	Destroy();
}

void AZL_Viscous_Cube::CleanUpCube()
{
	if (!HasAuthority() || bEnded)
	{
		return;
	}
	bEnded = true;

	GetWorldTimerManager().ClearTimer(HealTimer);
	GetWorldTimerManager().ClearTimer(EndTimer);

	if (AZeroLockCharacter* Hero = CubedHero)
	{
		Hero->OnServerJumpEscapeRequested.Remove(EscapeHandle);
		Hero->OnServerPreDeath.Remove(PreDeathHandle);
		if (UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp())
		{
			HeroASC->RemoveActiveGameplayEffect(EffectHandle);
			UZL_StatusTimerAbility::ClearStatusTimer(Healer.Get() ? static_cast<AActor*>(Healer.Get()) : Hero, HeroASC, ZerolockGameplayTagsForBinding::TAG_STATUS_CUBED);
		}
	}
}

void AZL_Viscous_Cube::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanUpCube();

	AZeroLockCharacter* Hero = CubedHero;
	if (UPrimitiveComponent* HeroRoot = IsValid(Hero) ? Cast<UPrimitiveComponent>(Hero->GetRootComponent()) : nullptr)
	{
		HeroRoot->IgnoreActorWhenMoving(this, false);
	}
	if (USkeletalMeshComponent* HeroMesh = IsValid(Hero) ? Hero->FindComponentByClass<USkeletalMeshComponent>() : nullptr)
	{
		HeroMesh->bPauseAnims = false;
	}
	Super::EndPlay(EndPlayReason);
}
