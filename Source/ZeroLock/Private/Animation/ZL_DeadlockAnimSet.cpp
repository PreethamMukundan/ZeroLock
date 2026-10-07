// Copyright Preetham Mukundan (C) 2026

#include "Animation/ZL_DeadlockAnimSet.h"
#include "Animation/AnimSequence.h"

UAnimSequence* FZLDirectionalAnims::Get(int32 Index) const
{
	switch (((Index % 8) + 8) % 8)
	{
	case 0: return N;
	case 1: return NE;
	case 2: return E;
	case 3: return SE;
	case 4: return S;
	case 5: return SW;
	case 6: return W;
	default: return NW;
	}
}

UAnimSequence* FZLDirectionalAnims::GetWithFallback(int32 Index) const
{
	for (int32 Offset = 0; Offset <= 4; ++Offset)
	{
		if (UAnimSequence* Seq = Get(Index + Offset))
		{
			return Seq;
		}
		if (UAnimSequence* Seq = Get(Index - Offset))
		{
			return Seq;
		}
	}
	return nullptr;
}

bool FZLDirectionalAnims::HasAny() const
{
	return N || NE || E || SE || S || SW || W || NW;
}
