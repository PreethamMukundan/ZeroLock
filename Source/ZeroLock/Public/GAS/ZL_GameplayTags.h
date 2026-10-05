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

	/** SetByCaller tag for the damage that UCalc_Spirit_Damage turns into spirit damage. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SETBYCALLER_SPIRIT)

	/** Asset tag for damage that can bring the target down to 1 health but never kill it (see UBaseCharAttributeSet::PostGameplayEffectExecute). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DAMAGE_NONLETHAL)

	/** Granted while Pocket's Affliction DOT is ticking on the owner. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_AFFLICTED)

	/** UCalc_Healing heals nothing while the target has this tag. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_HEAL_BLOCKED)

	/** Triggers UZL_StatusTimerAbility on the victim (send it with UZL_StatusTimerAbility::SendStatusTimer). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_EVENT_STATUS_TIMER)

}

