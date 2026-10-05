//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "NativeGameplayTags.h"
#include "GameplayTagContainer.h"

namespace ZerolockGameplayTagsForBinding
{
	//Declare all custom native tags for UI
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_INPUT_SECONDRY)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_INPUT_ABILITY_1)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_INPUT_ABILITY_2)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_INPUT_ULTIMATE)

	//Declare all custom native tags

	/** Damage is ignored while the owner has this tag (see UBaseCharAbilitySystemComponent::IsUntouchable). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_UNTOUCHABLE)

	/** While the owner has this tag, the mover holds it in place in RootedModeName and ignores every movement input except teleports. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_MOVEMENT_ROOTED)

	/** Blocks activating any ability while the owner has it (it's under ZeroLock.Abilities, which every UBaseGameplayAbility
	 * has in ActivationBlockedTags). Abilities that are already active still get their input presses. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ABILITIES_BLOCKED)

}

