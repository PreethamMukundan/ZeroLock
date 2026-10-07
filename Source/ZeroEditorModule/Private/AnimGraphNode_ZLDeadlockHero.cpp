// Copyright Preetham Mukundan (C) 2026

#include "AnimGraphNode_ZLDeadlockHero.h"

#define LOCTEXT_NAMESPACE "ZLDeadlockHero"

FText UAnimGraphNode_ZLDeadlockHero::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "Deadlock Hero");
}

FText UAnimGraphNode_ZLDeadlockHero::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Full Deadlock hero pose from the Mover state and the hero's Deadlock Anim Set: locomotion, jumps, slide, dash, zipline, aim offset, UpperBody/DefaultSlot montages, root motion stripped.");
}

FString UAnimGraphNode_ZLDeadlockHero::GetNodeCategory() const
{
	return TEXT("ZeroLock");
}

FLinearColor UAnimGraphNode_ZLDeadlockHero::GetNodeTitleColor() const
{
	return FLinearColor(0.85f, 0.45f, 0.1f);
}

#undef LOCTEXT_NAMESPACE
