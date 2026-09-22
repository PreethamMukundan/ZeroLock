//Copyright Preetham Mukundan (C) 2026


#include "Zero_BasePlayerController.h"

#include "Zero_BasePlayerState.h"
#include "Net/UnrealNetwork.h"
#include "ZeroLock/ZeroLockCharacter.h"

void AZero_BasePlayerController::ServerSetSelectedHero_Implementation(TSubclassOf<AZeroLockCharacter> HeroClass)
{
	SelectedHeroClass = HeroClass;
}

void AZero_BasePlayerController::ClientSelectHero(TSubclassOf<AZeroLockCharacter> HeroClass)
{
	ServerSetSelectedHero(HeroClass);
}

void AZero_BasePlayerController::ServerSetStartLocation_Implementation(FVector loc)
{
	SelectedStartLocation = loc;
}

void AZero_BasePlayerController::ClientSetStartLocation(FVector loc)
{
	ServerSetStartLocation(loc);
}

void AZero_BasePlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	AZero_BasePlayerState* PS = Cast<AZero_BasePlayerState>(PlayerState);
	if (!PS) return;

	if (OnPSInit.IsBound())
	{
		OnPSInit.Broadcast(PS);	
	}
}

void AZero_BasePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (AZero_BasePlayerState* PS = Cast<AZero_BasePlayerState>(PlayerState))
	{
		if (AZeroLockCharacter* hero = Cast<AZeroLockCharacter>(InPawn))
		{
			if (hero->Icon)
			{
				PS->SetPlayerIconImage(hero->Icon);
			}
		}
	}
	
}


void AZero_BasePlayerController::ShowDamageNumber_Implementation(float DamageAmount,
                                                                 AZeroLockCharacter* TargetCharacter, FGameplayTagContainer DamageNumberTags)
{
	if (IsValid(TargetCharacter))
	{
		TargetCharacter->AddDamageNumber(DamageAmount, DamageNumberTags);
	}
}

bool AZero_BasePlayerController::ShowDamageNumber_Validate(float DamageAmount, AZeroLockCharacter* TargetCharacter,
	FGameplayTagContainer DamageNumberTags)
{
	return true;
}

void AZero_BasePlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZero_BasePlayerController,SelectedHeroClass);
	DOREPLIFETIME(AZero_BasePlayerController,SelectedStartLocation);
}

float AZero_BasePlayerController::GetForwardPredictionTime() const
{
	/* Divide by 1000 to convert ping from ms to s, and by ClientBiasPct to put the projectile somewhere between where
 * the client wants it and where the server wants it, for fairness. */
	return (PlayerState && (GetNetMode() != NM_Standalone)) ? (0.001f * ClientBiasPct * FMath::Clamp(PlayerState->ExactPing - (IsLocalController() ? 0.0f : PredictionLatencyReduction), 0.0f, MaxPredictionPing)) : 0.f;
}

float AZero_BasePlayerController::GetProjectileSleepTime() const
{
	// At high latencies, projectiles won't be spawned until they can be forward-predicted at the maximum prediction ping.
	return 0.001f * FMath::Max(0.0f, PlayerState->ExactPing - PredictionLatencyReduction - MaxPredictionPing);
}

uint32 AZero_BasePlayerController::GenerateNewFakeProjectileId()
{
	const uint32 NextId = FakeProjectileIdCounter;
	FakeProjectileIdCounter = FakeProjectileIdCounter < UINT32_MAX ? FakeProjectileIdCounter + 1 : 1;
	checkf(!FakeProjectiles.Contains(NextId), TEXT("Generated invalid projectile ID! ID (%i) already used. Fake projectile map size: (%i)."), NextId, FakeProjectiles.Num());
	return NextId;
}

