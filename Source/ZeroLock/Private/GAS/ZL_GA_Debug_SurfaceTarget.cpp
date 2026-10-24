// Copyright Preetham Mukundan (C) 2026


#include "GAS/ZL_GA_Debug_SurfaceTarget.h"

#include "DrawDebugHelpers.h"
#include "ZeroLock/ZeroLock.h"

UZL_GA_Debug_SurfaceTarget::UZL_GA_Debug_SurfaceTarget()
{
	bDrawTargetingDebug = true;
}

void UZL_GA_Debug_SurfaceTarget::PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data)
{
	const FString NetSide = HasAuthority(&CurrentActivationInfo) ? TEXT("Server") : TEXT("Client");
	DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 30.f, 12, FColor::Cyan, false, DrawDuration);
	DrawDebugDirectionalArrow(GetWorld(), Hit.ImpactPoint, Hit.ImpactPoint + Hit.ImpactNormal * 100.f, 20.f, FColor::Cyan, false, DrawDuration, 0, 2.f);
	ZLOG_COLOR_TIME(FString::Printf(TEXT("[%s] SurfaceTarget %s normal %s on %s"), *NetSide, *Hit.ImpactPoint.ToString(), *Hit.ImpactNormal.ToString(), *GetNameSafe(Hit.GetActor())), FColor::Cyan, DrawDuration);
	UE_LOG(LogTemp, Log, TEXT("[%s] SurfaceTarget %s normal %s on %s"), *NetSide, *Hit.ImpactPoint.ToString(), *Hit.ImpactNormal.ToString(), *GetNameSafe(Hit.GetActor()));
	Super::PlacementConfirmed(Hit, Placement, Data);
}

void UZL_GA_Debug_SurfaceTarget::AbilityCancelledAction(const FGameplayAbilityTargetDataHandle& Data)
{
	const FString NetSide = HasAuthority(&CurrentActivationInfo) ? TEXT("Server") : TEXT("Client");
	ZLOG_COLOR_TIME(FString::Printf(TEXT("[%s] SurfaceTarget cancelled"), *NetSide), FColor::Orange, DrawDuration);
	Super::AbilityCancelledAction(Data);
}
