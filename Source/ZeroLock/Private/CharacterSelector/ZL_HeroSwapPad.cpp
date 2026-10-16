// Copyright Preetham Mukundan (C) 2026


#include "CharacterSelector/ZL_HeroSwapPad.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UI/HUD/ZL_HUDClass.h"

AZL_HeroSwapPad::AZL_HeroSwapPad()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(Trigger);
	PadMesh->SetCollisionProfileName(TEXT("BlockAll"));
	PadMesh->SetGenerateOverlapEvents(false);
}

bool AZL_HeroSwapPad::IsPawnOnPad(const APawn* Pawn) const
{
	if (!Pawn || !Trigger) return false;
	if (Trigger->IsOverlappingActor(Pawn)) return true;

	// The server's overlap state for a remote Mover pawn isn't reliable, so also accept by position, with slack for latency.
	const FVector Local = Trigger->GetComponentTransform().InverseTransformPositionNoScale(Pawn->GetActorLocation());
	const FVector Extent = Trigger->GetScaledBoxExtent() + FVector(ServerCheckTolerance);
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

void AZL_HeroSwapPad::BeginPlay()
{
	Super::BeginPlay();

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AZL_HeroSwapPad::HandleBeginOverlap);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &AZL_HeroSwapPad::HandleEndOverlap);
}

static AZL_HUDClass* GetLocalHeroHUD(const APawn* Hero)
{
	if (!Hero || !Hero->IsLocallyControlled()) return nullptr;
	const APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	return PC ? Cast<AZL_HUDClass>(PC->GetHUD()) : nullptr;
}

void AZL_HeroSwapPad::HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Hero = Cast<APawn>(OtherActor);

	// The pawn we just swapped into spawns on the pad; only actually stepping on should open it.
	if (!Hero || Hero->GetGameTimeSinceCreation() < IgnoreNewPawnTime) return;

	if (AZL_HUDClass* HUD = GetLocalHeroHUD(Hero))
	{
		if (!HUD->IsHeroSelectOpen())
		{
			HUD->OpenHeroSelect();
			bOpenedHeroSelect = HUD->IsHeroSelectOpen();
		}
	}
}

void AZL_HeroSwapPad::HandleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!bOpenedHeroSelect) return;

	if (AZL_HUDClass* HUD = GetLocalHeroHUD(Cast<APawn>(OtherActor)))
	{
		bOpenedHeroSelect = false;
		HUD->CloseHeroSelect();
	}
}
