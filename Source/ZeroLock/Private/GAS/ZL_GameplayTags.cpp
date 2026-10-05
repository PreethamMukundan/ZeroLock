//Copyright Preetham Mukundan (C) 2026


#include "GAS/ZL_GameplayTags.h"

namespace ZerolockGameplayTagsForBinding
{

	UE_DEFINE_GAMEPLAY_TAG(TAG_INPUT_SECONDRY, "Zerolock.InputBindTags.Secondry");
	UE_DEFINE_GAMEPLAY_TAG(TAG_INPUT_ABILITY_1, "Zerolock.InputBindTags.Ability1");
	UE_DEFINE_GAMEPLAY_TAG(TAG_INPUT_ABILITY_2, "Zerolock.InputBindTags.Ability2");
	UE_DEFINE_GAMEPLAY_TAG(TAG_INPUT_ULTIMATE, "Zerolock.InputBindTags.Ultimate");

	UE_DEFINE_GAMEPLAY_TAG(TAG_UNTOUCHABLE, "Zerolock.Untouchable");
	UE_DEFINE_GAMEPLAY_TAG(TAG_MOVEMENT_ROOTED, "ZeroLock.Movement.Rooted");
	UE_DEFINE_GAMEPLAY_TAG(TAG_ABILITIES_BLOCKED, "ZeroLock.Abilities.Blocked");

	UE_DEFINE_GAMEPLAY_TAG(TAG_SETBYCALLER_SPIRIT, "Zerolock.DamageCalc.Spirit");
	UE_DEFINE_GAMEPLAY_TAG(TAG_DAMAGE_NONLETHAL, "Damage.Tag.NonLethal");
	UE_DEFINE_GAMEPLAY_TAG(TAG_STATUS_AFFLICTED, "Zerolock.Status.Afflicted");
	UE_DEFINE_GAMEPLAY_TAG(TAG_STATUS_HEAL_BLOCKED, "Zerolock.Status.HealBlocked");
	UE_DEFINE_GAMEPLAY_TAG(TAG_EVENT_STATUS_TIMER, "Zerolock.Event.StatusTimer");
}