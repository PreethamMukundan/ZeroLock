// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "ZL_OverHeadWidgetComponent.generated.h"

/** Overhead bar on heroes: world space, turned to face the local camera every tick. */
UCLASS()
class ZEROLOCK_API UZL_OverHeadWidgetComponent : public UWidgetComponent
{
	GENERATED_BODY()
public:
	UZL_OverHeadWidgetComponent();

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** cm added on top of the location set on the hero BP, once at BeginPlay (raises every hero's bar together). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overhead")
	float ExtraHeight = 40.f;

protected:
	virtual void BeginPlay() override;
	
};
