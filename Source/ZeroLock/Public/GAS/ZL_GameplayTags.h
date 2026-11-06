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

	/** SetByCaller tag for a cooldown GE's duration (UBaseGameplayAbility::ApplyCooldown sets it, UZL_GE_BaseCooldown reads it). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SETBYCALLER_COOLDOWN_DURATION)

	/** SetByCaller tag for UZL_GE_AmmoCost: ammo added to CurrentAmmo (negative = spent). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SETBYCALLER_AMMO)

	/** Granted while UZero_BaseReloadAbility runs (ActivationOwnedTags): blocks primary fire (UZL_GA_WeaponFire). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_WEAPON_RELOADING)

	/** On a hero who hasn't dealt or taken hero damage for AZeroLockCharacter::OutOfCombatDelay. Replicated loose tag, set by the server. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_OUT_OF_COMBAT)

	// Item statuses (ZLItemStatus). Silenced: no hero abilities. Disarmed: no primary fire. Unstoppable blocks and clears crowd control.
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_SILENCED)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_DISARMED)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_BULLET_IMMUNE)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_UNSTOPPABLE)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_STEALTHED)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_ENLARGED)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_SHRUNK)

	/** Cooldown of the active item on each key. The cooldown stays with the key, so an upgrade keeps it; selling clears it. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_COOLDOWN_ITEM_SLOT_1)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_COOLDOWN_ITEM_SLOT_2)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_COOLDOWN_ITEM_SLOT_3)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_COOLDOWN_ITEM_SLOT_4)

	/** SetByCaller tag for UZL_GE_MoveSlow: the MoveSpeedMultiplier the slow sets (e.g. 0.6 = 40% slower). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SETBYCALLER_MOVE_SPEED_MULTIPLIER)

	/** Granted while a UZL_GE_MoveSlow is on the owner (on the spec, by whoever applies it). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_SLOWED)

	/** Encased in Viscous's Cube (UZL_Viscous_CubeEffect): no own abilities (UBaseGameplayAbility::CanActivateAbility, event-triggered
	 * ones still run), no own movement input, Mover in the Cubed mode (pushes are impulses), jump asks the server to escape. Also the HUD status bar's tag. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_CUBED)

	/** Viscous is a Goo Ball (UZL_Viscous_GooBall's ActivationOwnedTags, predicted): Mover GooBall mode + sphere capsule, no weapon. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_GOOBALL)

	/** Solomon floats (UZL_Solomon_MovePiece's ActivationOwnedTags, predicted): in the air the Mover caps his fall and air speed and
	 * cancels part of gravity, fading over UZeroMoverComponent::FloatFadeDuration of air time (FZeroMovementInputs::bIsFloating). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_FLOATING)

	/** Promoted by Solomon's Kingside Knight (UZL_Solomon_KnightEffect): an ally knight, or Solomon himself as king. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_KNIGHT)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_KING)
	/** Xayah has Clean Cut shots ready. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_CLEAN_CUT)
	/** SetByCaller multiplier on FireRate (seconds between shots). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SETBYCALLER_FIRE_RATE_MULTIPLIER)

	/** SetByCaller tags for UZL_Viscous_GooBallEffect: added WeaponResistance / SpiritResistance (percent). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SETBYCALLER_BULLET_RESIST)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SETBYCALLER_SPIRIT_RESIST)

	/** Granted by the recharge timer of a charged ability in that EGameplayAbilitySlot (UBaseCharAbilitySystemComponent::ConsumeAbilityCharge);
	 * the HUD reads the next charge's time from it. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_CHARGE_RECHARGE_SLOT1)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_CHARGE_RECHARGE_SLOT2)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_CHARGE_RECHARGE_SLOT3)
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_CHARGE_RECHARGE_ULTIMATE)

	/** Asset tag for damage that can bring the target down to 1 health but never kill it (see UBaseCharAttributeSet::PostGameplayEffectExecute). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DAMAGE_NONLETHAL)

	/** Granted while Pocket's Affliction DOT is ticking on the owner. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_AFFLICTED)

	/** UCalc_Healing heals nothing while the target has this tag. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_HEAL_BLOCKED)

	/** Granted while the owner is a guest in Doorman's hotel (UZL_Doorman_HotelGuestEffect). Also the HUD status bar's tag ("HotelGuest"). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_HOTEL_GUEST)

	/** Abilities with UBaseGameplayAbility::bIsMovementAbility can't activate while the owner has this tag (Mover dash/jump still work).
	 * Not under ZeroLock.Abilities on purpose: that branch blocks every ability. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STATUS_MOVEMENT_ABILITIES_BLOCKED)

	/** Stunned: no new abilities; AZeroLockCharacter::Stunned cancels the running ones with bCancelOnStun and disables input; ABP_DeadlockHero plays StunLoop. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_STUN)

	/** Parrying (BPGE_Parry): a melee strike that lands on the owner stuns the attacker instead (AZeroLockCharacter::MeleeStrike). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_MELEE_PARRY)

	/** Sent to the owner by a light melee montage notify: UZeroBase_LightMelee strikes on it. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_EVENT_MELEE_HIT)

	/** Triggers UZL_StatusTimerAbility on the victim (send it with UZL_StatusTimerAbility::SendStatusTimer). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_EVENT_STATUS_TIMER)

	// Cheats: UZL_CheatComponent sets them as loose tags on the pawn's ASC, on the server and the owning client.

	/** Damage still shows numbers but takes no health (UBaseCharAttributeSet::PostGameplayEffectExecute). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_CHEAT_GODMODE)

	/** Damage can't take the owner below 1 health (target dummies). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_CHEAT_BUDDHA)

	/** Mover NoClip mode: free flight along the camera through everything (FZeroMovementInputs::bIsNoClip, predicted). */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_CHEAT_NOCLIP)

	/** UBaseGameplayAbility applies no cooldown and charged abilities keep their charges. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_CHEAT_NO_COOLDOWNS)

	/** UZL_GA_WeaponFire spends no ammo. */
	ZEROLOCK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_CHEAT_INFINITE_AMMO)

}

