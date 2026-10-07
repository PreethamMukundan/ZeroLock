// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "Animation/AnimNode_ZLDeadlockHero.h"
#include "AnimGraphNode_ZLDeadlockHero.generated.h"

/** Editor node for FAnimNode_ZLDeadlockHero. */
UCLASS()
class UAnimGraphNode_ZLDeadlockHero : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_ZLDeadlockHero Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetNodeCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
};
