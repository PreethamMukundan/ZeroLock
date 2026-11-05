"""Builds items.json, the input of ue_import_items.py, from the item API.

  build_items.py [--refresh] [--show-partial]

The API response is cached in Saved/ItemImporter. It supplies stats, costs and upgrade links only:
display names come from names.json, which this script never changes, and descriptions are generated from the stats.
--show-partial also lists items in the shop whose effect isn't built yet (their stats still work).
"""
import json
import math
import os
import re
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
OUT_DIR = os.path.join(ROOT, 'Saved', 'ItemImporter')
RAW = os.path.join(OUT_DIR, 'items_raw.json')
API = 'https://api.deadlock-api.com/v1/assets/items'

# Move speed: our heroes walk 800 cm/s at the source's 7.2 m/s (Movement.md "Per-hero speed").
SPEED_CM_PER_M = 800 / 7.2
# Distance: 1 source metre = 39.37 units x 1.778 cm per unit.
DISTANCE_CM_PER_M = 70

# API stat -> EZL_ItemStat, with the label used in generated descriptions and, for speeds, the factor to our units.
STATS = {
    'MODIFIER_VALUE_HEALTH_MAX': ('MaxHealth', '+{} Max Health'),
    'MODIFIER_VALUE_WEAPON_DAMAGE_INCREASE': ('WeaponDamage', '+{}% Weapon Damage'),
    'MODIFIER_VALUE_MELEE_DAMAGE_INCREASE': ('MeleeDamage', '+{}% Melee Damage'),
    'MODIFIER_VALUE_TECH_POWER': ('SpiritPower', '+{} Spirit Power'),
    'MODIFIER_VALUE_BULLET_ARMOR_DAMAGE_RESIST': ('WeaponResist', '+{}% Bullet Resist'),
    'MODIFIER_VALUE_TECH_RESIST': ('SpiritResist', '+{}% Spirit Resist'),
    'MODIFIER_VALUE_MELEE_RESIST': ('MeleeResist', '+{}% Melee Resist'),
    'MODIFIER_VALUE_BULLET_LIFESTEAL': ('WeaponLifesteal', '+{}% Bullet Lifesteal'),
    'MODIFIER_VALUE_TECH_LIFESTEAL': ('SpiritLifesteal', '+{}% Spirit Lifesteal'),
    'MODIFIER_VALUE_COOLDOWN_REDUCTION_PERCENTAGE': ('CooldownReduction', '+{}% Cooldown Reduction'),
    'MODIFIER_VALUE_HEAL_AMP_CAST_PERCENT': ('HealingBonus', '+{}% Healing'),
    'MODIFIER_VALUE_STAMINA': ('MaxStamina', '+{} Stamina'),
    'MODIFIER_VALUE_AMMO_CLIP_SIZE': ('AmmoFlat', '+{} Ammo'),
    'MODIFIER_VALUE_AMMO_CLIP_SIZE_PERCENT': ('AmmoPercent', '+{}% Ammo'),
    'MODIFIER_VALUE_STAMINA_REGEN_PER_SECOND_PERCENTAGE': ('StaminaRegenPercent', '+{}% Stamina Recovery'),
    'MODIFIER_VALUE_FIRE_RATE': ('FireRatePercent', '+{}% Fire Rate'),
    'MODIFIER_VALUE_HEALTH_REGEN_PER_SECOND': ('HealthRegen', '+{} Health Regen'),
    'MODIFIER_VALUE_OUT_OF_COMBAT_HEALTH_REGEN': ('OutOfCombatRegen', '+{} Out of Combat Regen'),
    'MODIFIER_VALUE_MOVEMENT_SPEED_MAX': ('MoveSpeed', '+{} m/s Move Speed', SPEED_CM_PER_M),
    'MODIFIER_VALUE_SPRINT_SPEED_BONUS': ('SprintSpeed', '+{} m/s Out of Combat Speed', SPEED_CM_PER_M),
    'MODIFIER_VALUE_BONUS_BULLET_SPEED_PERCENT': ('BulletSpeedPercent', '+{}% Bullet Speed'),
    'MODIFIER_VALUE_STATUS_RESISTANCE': ('DebuffResist', '+{}% Debuff Resist'),
    'MODIFIER_VALUE_MOVEMENT_SLOW_RESISTANCE': ('SlowResist', '+{}% Slow Resist'),
    'MODIFIER_VALUE_MOVEMENT_SLIDE_DISTANCE_SCALE': ('SlideDistance', '+{}% Slide Distance'),
    'MODIFIER_VALUE_MELEE_TRAVEL_DISTANCE_PERCENTAGE': ('MeleeDistance', '+{}% Melee Distance'),
    'MODIFIER_VALUE_AIR_CONTROL_PERCENT': ('AirControl', '+{}% Air Control'),
    'MODIFIER_VALUE_AIR_CONTROL_ACCEL_PERCENT': ('AirControl', '+{}% Air Control'),
    'MODIFIER_VALUE_BONUS_ABILITY_DURATION_PERCENTAGE': ('AbilityDuration', '+{}% Ability Duration'),
    'MODIFIER_VALUE_TECH_RANGE_PERCENT': ('AbilityRange', '+{}% Ability Range'),
    'MODIFIER_VALUE_BONUS_ABILITY_CHARGES': ('AbilityCharges', '+{} Ability Charges'),
    'MODIFIER_VALUE_COOLDOWN_BETWEEN_CHARGE_REDUCTION_PERCENTAGE': ('ChargeCooldownReduction', '+{}% Faster Charges'),
    'MODIFIER_VALUE_TECH_POWER_PERCENT': ('SpiritPowerPercent', '+{}% Spirit Power'),
}

# How each active is cast. Anything not listed is Self for instant casts and Enemy for aimed ones.
ACTIVE_TARGETS = {
    'upgrade_targeted_silence': 'Enemy',
    'upgrade_health_stimpak': 'Ally',
    'upgrade_guardian_ward': 'Ally',
    'upgrade_divine_barrier': 'Ally',
    'upgrade_rescue_beam': 'Ally',
    'upgrade_thermal_detonator': 'Ground',
    'upgrade_shrink_ray': 'Ally',
    'upgrade_aoe_root': 'Ground',
    'upgrade_mystical_piano': 'Ground',
}

# Actives with a radius that help allies; other radii hit enemies.
ACTIVE_AREA_ALLIES = {'upgrade_health_nova', 'upgrade_restorative_locket', 'upgrade_dps_aura', 'upgrade_celestial_guidance'}

# The source names these differently per item; first one with a value wins.
RADIUS_PROPERTIES = ('Radius', 'AbilityRadius', 'ActiveRadius', 'AuraRadius', 'EndRadius', 'CaptureRadius')
DURATION_PROPERTIES = ('AbilityDuration', 'BuffDuration', 'SlowDuration')

# Active properties with no stat type: property -> FZL_ActiveItem field, or an EZL_ItemStat for the timed stats.
ACTIVE_FIELDS = {
    'Damage': 'damage',
    'ImpactDamage': 'damage',
    'HealOnActivate': 'heal',
    'CombatBarrier': 'barrier',
    'GuardianWardCombatBarrier': 'barrier',
}
ACTIVE_STAT_PROPERTIES = {
    'ActiveBonusFireRate': 'FireRatePercent',
}

# Stats that only exist on actives, always debuffs: API stat -> the EZL_ItemStats it lowers.
ACTIVE_DEBUFF_TYPES = {
    'MODIFIER_VALUE_TECH_RESIST_REDUCTION': ('SpiritResist',),
    'MODIFIER_VALUE_BULLET_AND_MELEE_RESIST_REDUCTION': ('WeaponResist', 'MeleeResist'),
    'MODIFIER_VALUE_FIRE_RATE_SLOW': ('FireRatePercent',),
    'MODIFIER_VALUE_MOVEMENT_SPEED_SLOW_PERCENT': ('MoveSlowPercent',),
}

# Only these actives get a key. Everything else waits for Phases 5 and 6: half an active misleads (a speed penalty with no
# immunity, ground speed with no flight), so an item has no key until its active makes sense.
# Complete: does all its item promises.
ACTIVE_COMPLETE = {
    'upgrade_grit',
    'upgrade_guardian_ward',
    'upgrade_divine_barrier',
    'upgrade_health_nova',
    'upgrade_health_stimpak',
    'upgrade_withering_whip',
    'upgrade_greater_withering_whip',
    'upgrade_spirit_sap',
    'upgrade_fury_trance',
    'upgrade_infuser',
    'upgrade_cold_front',
    'upgrade_dps_aura',
    'upgrade_targeted_silence',
    'upgrade_unstoppable',
    'upgrade_warp_stone',
}

# Works, but is missing something: a passive, or part of the active. The report keeps these as needing an effect.
ACTIVE_PARTIAL = {
    'upgrade_reduce_debuff_duration',  # heals even with nothing to remove
    'upgrade_containment',  # no dash reduction, no ability block
    'upgrade_focus_lens',  # no percent damage
    'upgrade_glitch',  # doesn't block items or strip buffs
    'upgrade_target_stun',  # no delay, no bonus against airborne targets
    'upgrade_metal_skin',  # no dash reduction or gravity
    'upgrade_cloaking_device_active',  # no reveal up close or on damage, no ambush bonus
    'upgrade_fleetfoot_boots',  # no passive
    'upgrade_surging_power',  # no reload speed
    'upgrade_rocket_booster',  # no drop back down
    'upgrade_phantom_strike',  # doesn't pull the target down
    'upgrade_shrink_ray',  # the capsule stays full size, no tunnels
    'upgrade_colossus',  # the capsule stays full size, no dash reduction
    'upgrade_rupture',  # max health, not current health
    'upgrade_mystical_piano',  # no stamina drain or daze
    'upgrade_thermal_detonator',  # steady damage, doesn't grow
    'upgrade_aoe_root',  # no dash reduction, no alt cast
    'upgrade_rescue_beam',  # one instant heal, not a channel; no pull
    'upgrade_celestial_guidance',  # no minimum heal, no stamina refill
    'upgrade_nullification_aura',  # doesn't strip buffs or stop stamina
    'upgrade_self_bubble',  # no lift off the ground, buffs start with it
    'upgrade_arctic_blast',  # no freeze
}


# Passive effects, as FZL_ItemProc recipes. A string is a property of the item, read from the source, with '-' to negate it;
# a number is used as it is. Stats are (EZL_ItemStat, value). Everything else about a proc defaults to off (PROC_DEFAULTS).
RESIST_DOWN = lambda value: [('WeaponResist', value), ('MeleeResist', value)]

PROC_RECIPES = {
    # Bullets
    'upgrade_hollow_point_rounds': [
        {'trigger': 'While', 'health_above': 'LifeThreshold', 'self_stats': [('WeaponDamage', 'BaseAttackDamagePercent')]},
        {'trigger': 'BulletHit', 'health_above': 'LifeThreshold', 'target_stats': RESIST_DOWN('BulletArmorReduction'), 'duration': 'DebuffDuration'},
    ],
    'upgrade_pristine_emblem': [{'trigger': 'BulletHit', 'target_health_above': 'EnemyLifeThreshold', 'bonus_damage_percent': 'BaseAttackDamagePercentBonus'}],
    'upgrade_close_range': [{'trigger': 'BulletHit', 'max_range': 'CloseRangeBonusDamageRange', 'bonus_damage_percent': 'CloseRangeBonusWeaponPower'}],
    'upgrade_long_range': [{'trigger': 'BulletHit', 'min_range': 'LongRangeBonusWeaponPowerMinRange', 'bonus_damage_percent': 'LongRangeBonusWeaponPower'}],
    'upgrade_sharpshooter': [{'trigger': 'BulletHit', 'min_range': 'LongRangeBonusWeaponPowerMinRange', 'bonus_damage_percent': 'LongRangeBonusWeaponPower'}],
    'upgrade_close_quarter_combat': [
        {'trigger': 'BulletHit', 'max_range': 'CloseRangeBonusDamageRange', 'bonus_damage_percent': 'CloseRangeBonusWeaponPower'},
        {'trigger': 'BulletHit', 'max_range': 'CloseRangeBonusDamageRange', 'target_stats': [('MoveSlowPercent', '-SlowPercent')], 'duration': 'SlowDuration'},
    ],
    'upgrade_slowing_bullets': [{'trigger': 'BulletHit', 'build_up_hits': 'BuildUpPerShot', 'target_stats': [('MoveSlowPercent', '-SlowPercent')], 'duration': 'SlowDuration'}],
    'upgrade_weighted_shots': [{'trigger': 'BulletHit', 'build_up_hits': 'BuildUpPerShot', 'target_stats': [('MoveSlowPercent', '-SlowPercent')], 'duration': 'SlowDuration'}],
    'upgrade_inhibitor': [{'trigger': 'BulletHit', 'build_up_hits': 'BuildUpPerShot', 'duration': 'DebuffDuration',
                           'target_stats': [('AllDamagePercent', 'OutgoingDamagePenaltyPercent'), ('HealingReduction', '-HealAmpReceivePenaltyPercent')]}],
    'upgrade_tech_defense_shredders': [{'trigger': 'BulletHit', 'target_stats': [('SpiritResist', 'TechArmorDamageReduction')], 'duration': 'DebuffDuration'}],
    'upgrade_proc_silence': [
        {'trigger': 'BulletHit', 'target_stats': [('SpiritPower', 'TechDamageReduction')], 'duration': 'DebuffDuration'},
        {'trigger': 'BulletHit', 'build_up_hits': 'BuildUpPerShot', 'cooldown': 'ImmunityDuration', 'status': 'Silence', 'status_duration': 'SilenceDuration'},
    ],
    'upgrade_crackshot': [{'trigger': 'BulletHit', 'cooldown': 'AbilityCooldown', 'spirit_damage': 'ProcBonusMagicDamage'}],
    'upgrade_critshot': [{'trigger': 'BulletHit', 'chance': 'ProcChance', 'bonus_damage_percent': 'CritDamagePercent'}],
    'upgrade_medic_bullets': [{'trigger': 'BulletHit', 'cooldown': 'AbilityCooldown', 'heal': 'HealFromHero'}],
    'upgrade_toxic_bullets': [{'trigger': 'BulletHit', 'build_up_hits': 'BuildUpPerShot', 'dot_max_health_percent': 'DotHealthPercent', 'dot_duration': 'DotDuration',
                               'target_stats': [('HealingReduction', '-HealAmpReceivePenaltyPercent')], 'duration': 'DotDuration'}],
    'upgrade_ricochet': [{'trigger': 'BulletHit', 'percent_of_hit': 'RicochetDamagePercent', 'radius': 'RicochetRadius', 'max_targets': 'RicochetTargets', 'skip_hit_target': True}],
    'upgrade_chain_lightning': [{'trigger': 'BulletHit', 'cooldown': 'ProcCooldown', 'chance': 'ProcChance', 'spirit_damage': 'DamagePerChain', 'radius': 'ChainRadius', 'max_targets': 'ChainCount'}],
    'upgrade_burst_fire': [{'trigger': 'BulletHit', 'cooldown': 'AbilityCooldown', 'duration': 'AbilityDuration',
                            'self_stats': [('FireRatePercent', 'ActivatedFireRate'), ('MoveSpeed', 'BonusMoveSpeed')]}],
    'upgrade_reinforcing_casings': [{'trigger': 'BulletHit', 'self_stats': [('WeaponResist', 'BulletResistPerStack')], 'max_stacks': 'MaxArmorStacks', 'duration': 'BulletResistDuration'}],
    'upgrade_tech_overflow': [{'trigger': 'BulletHit', 'build_up_hits': 'BuildUpPerShot', 'duration': 'AbilityDuration',
                               'self_stats': [('FireRatePercent', 'BonusFireRate'), ('SpiritPower', 'BonusSpirit'), ('SpiritLifesteal', 'BonusSpiritLifesteal')]}],
    'upgrade_weapon_backstabber': [{'trigger': 'BulletHit', 'cooldown': 'AbilityCooldown', 'max_range': 'ProcRadius', 'duration': 'DebuffDuration',
                                    'target_stats': RESIST_DOWN('BulletResistReduction'), 'dot_damage_per_second': 'DPS', 'dot_duration': 'DebuffDuration',
                                    'self_stats': [('MoveSpeed', 'BonusMoveSpeed')]}],
    # Spirit damage
    'upgrade_mystic_regeneration': [{'trigger': 'SpiritHit', 'self_stats': [('HealthRegen', 'Regeneration')], 'duration': 'RegenerationDuration'}],
    'upgrade_resonant_healing': [
        {'trigger': 'SpiritHit', 'self_stats': [('HealthRegen', 'Regeneration')], 'duration': 'RegenerationDuration'},
        {'trigger': 'AbilityCast', 'cooldown': 'AbilityCooldown', 'heal': 'HealingPerCast', 'self_stats': [('MoveSpeed', 'BonusMoveSpeed')], 'duration': 'AbilityDuration'},
    ],
    'upgrade_bullet_resist_shredder': [{'trigger': 'SpiritHit', 'target_stats': RESIST_DOWN('BulletArmorReduction'), 'duration': 'AbilityDuration'}],
    'upgrade_magic_vulnerability': [{'trigger': 'SpiritHit', 'target_stats': [('SpiritResist', 'TechArmorDamageReduction')], 'duration': 'AbilityDuration'}],
    'upgrade_suppressor': [{'trigger': 'SpiritHit', 'target_stats': [('FireRatePercent', '-FireRateSlow')], 'duration': 'AbilityDuration'}],
    'upgrade_magic_slow': [{'trigger': 'SpiritHit', 'target_stats': [('MoveSlowPercent', '-MovementSpeedSlow')], 'duration': 'AbilityDuration'}],
    'upgrade_healbane': [{'trigger': 'SpiritHit', 'target_stats': [('HealingReduction', '-HealAmpReceivePenaltyPercent')], 'duration': 'AbilityDuration'}],
    'upgrade_escalating_exposure': [{'trigger': 'SpiritHit', 'cooldown': 'ProcCooldown', 'target_stats': [('SpiritResist', '-MagicIncreasePerStack')], 'max_stacks': 'MaxStacks', 'duration': 'AbilityDuration'}],
    'upgrade_magic_burst': [{'trigger': 'SpiritHit', 'cooldown': 'AbilityCooldown', 'min_damage': 'MinimumDamage', 'spirit_damage': 'Damage'}],
    'upgrade_magic_shock': [{'trigger': 'SpiritHit', 'cooldown': 'AbilityCooldown', 'min_damage': 'MinimumDamage', 'spirit_damage': 'Damage', 'target_health_percent': 'CurrentHealthDamage'}],
    'upgrade_spirit_burn': [{'trigger': 'SpiritHit', 'cooldown': 'ImmunityDuration', 'min_damage': 'DamageThreshold', 'damage_window': 'DamageThresholdDuration', 'spirit_damage': 'ExplosionDamage',
                             'dot_damage_per_second': 'DPS', 'dot_duration': 'DebuffDuration', 'target_stats': [('HealingReduction', '-HealAmpReceivePenaltyPercent')], 'duration': 'DebuffDuration'}],
    # Melee and parry
    'upgrade_lifestrike_gauntlets': [{'trigger': 'MeleeHit', 'cooldown': 'AbilityCooldown', 'heal': 'LifestrikeHeal'}],
    'upgrade_boxing_glove': [{'trigger': 'MeleeHit', 'cooldown': 'AbilityCooldown', 'heal': 'LifestealHeal', 'heal_percent_of_damage': 'LifestealHealPercent',
                              'target_stats': [('MoveSlowPercent', '-SlowPercent')], 'duration': 'SlowDuration'}],
    'upgrade_acolytes_glove': [{'trigger': 'MeleeHit', 'cooldown': 'AbilityCooldown', 'spirit_damage': 'SpiritDamage', 'target_stats': [('SpiritResist', 'TechArmorDamageReduction')], 'duration': 'AbilityDuration'}],
    'upgrade_spirit_snatch': [{'trigger': 'MeleeHit', 'cooldown': 'AbilityCooldown', 'spirit_damage': 'SpiritDamage', 'duration': 'AbilityDuration',
                               'target_stats': [('SpiritResist', 'TechArmorDamageReduction'), ('SpiritPower', 'TechPowerReduction')],
                               'self_stats': [('SpiritResist', 'TechArmorGain'), ('SpiritPower', 'TechPowerGain')]}],
    'upgrade_melee_charge': [{'trigger': 'HeavyMeleeHit', 'cooldown': 'AbilityCooldown', 'bonus_damage_percent': 'BonusHeavyMeleeDamage'}],
    'upgrade_melee_rebuttal': [{'trigger': 'Parry', 'heal_percent_of_damage': 'ParrySuccessHealPercentage', 'percent_of_hit': 100, 'self_stats': [('AllDamagePercent', 'BonusDamagePercent')], 'duration': 'BuffDuration'}],
    # Health
    'upgrade_regenerating_bullet_shield': [{'trigger': 'While', 'health_above': 'LifeThreshold', 'self_stats': [('WeaponDamage', 'BaseAttackDamagePercent'), ('FireRatePercent', 'BonusFireRate')]}],
    'upgrade_magic_shield': [{'trigger': 'While', 'health_above': 'LifeThreshold', 'self_stats': [('SpiritPower', 'TechPower'), ('CooldownReduction', 'CooldownReduction')]}],
    'upgrade_tech_purge': [{'trigger': 'While', 'health_below': 'HealthThreshold', 'self_stats': [('SpiritResist', 'TechResistBelowThreshold')]}],
    'upgrade_improved_bullet_armor': [{'trigger': 'While', 'health_below': 'HealthThreshold', 'self_stats': [('WeaponResist', 'BulletResistBelowThreshold')]}],
    'upgrade_chonky': [{'trigger': 'While', 'health_above': 'HealthThreshold', 'self_stats': [('MoveSpeed', 'BonusMoveSpeed')]}],
    'upgrade_fervor': [{'trigger': 'While', 'health_below': 'LowHealthThreshold', 'cooldown': 'AbilityCooldown', 'duration': 'AbilityDuration',
                        'self_stats': [('MoveSpeed', 'FervorMovespeed'), ('FireRatePercent', 'FervorFireRate'), ('DebuffResist', 'FervorStatusResistancePercent')]}],
    # Being hit
    'upgrade_juggernaut': [{'trigger': 'BulletTaken', 'target_stats': [('FireRatePercent', '-FireRateSlow')], 'duration': 'FireRateSlowDuration'}],
    'upgrade_berserker': [{'trigger': 'DamageTaken', 'min_damage': 'DamageToStack', 'damage_window': 'DamageDuration', 'self_stats': [('WeaponDamage', 'WeaponPowerPerStack')],
                           'max_stacks': 'MaxStacks', 'duration': 'DamageDuration'}],
    'upgrade_weapon_shielding': [{'trigger': 'BulletTaken', 'cooldown': 'AbilityCooldown', 'min_damage': 'DamageThreshold', 'damage_window': 'DamageWindow', 'barrier': 'CombatBarrier',
                                  'barrier_duration': 'BarrierDuration', 'self_stats': [('WeaponResist', 'BulletResist')], 'duration': 'BarrierDuration'}],
    'upgrade_spirit_bubble': [{'trigger': 'SpiritTaken', 'cooldown': 'AbilityCooldown', 'min_damage': 'DamageThreshold', 'damage_window': 'DamageWindow', 'barrier': 'CombatBarrier',
                               'barrier_duration': 'BarrierDuration', 'self_stats': [('SpiritResist', 'TechResist')], 'duration': 'BarrierDuration'}],
    # Casting, time and auras
    'upgrade_diviners_kevlar': [{'trigger': 'UltimateCast', 'cooldown': 'AbilityCooldown', 'barrier': 'CombatBarrier', 'barrier_duration': 'BuffDuration',
                                 'self_stats': [('SpiritPower', 'TechPower')], 'duration': 'BuffDuration'}],
    'upgrade_enchanted_holsters': [{'trigger': 'AbilityCast', 'self_stats': [('FireRatePercent', 'BonusFireRate')], 'max_stacks': 'MaxStacks', 'duration': 'BuffDuration'}],
    'upgrade_tech_damage_pulse': [{'trigger': 'Periodic', 'cooldown': 'AbilityCooldown', 'spirit_damage': 'DamagePulseAmount', 'radius': 'DamagePulseRadius', 'max_targets': 2}],
    'upgrade_bullet_armor_reduction_aura': [{'trigger': 'Aura', 'radius': 'Radius', 'target_stats': RESIST_DOWN('BulletArmorReduction') + [('FireRatePercent', '-FireRateSlow')]}],
    'upgrade_dps_aura': [{'trigger': 'Aura', 'radius': 'Radius', 'area_team': 'Allies', 'target_stats': [('WeaponResist', 'BulletResist')]}],
    # Headshots
    'upgrade_headshot_booster': [{'trigger': 'Headshot', 'cooldown': 'AbilityCooldown', 'weapon_damage': 'HeadShotBonusDamage'}],
    'upgrade_headshot_booster2': [{'trigger': 'Headshot', 'target_stats': RESIST_DOWN('BulletResistReduction'), 'duration': 'DebuffDuration'}],
    'upgrade_headhunter': [{'trigger': 'Headshot', 'cooldown': 'AbilityCooldown', 'weapon_damage': 'HeadShotBonusDamage', 'heal_max_health_percent': 'HealPercentPerHeadshot',
                            'self_stats': [('MoveSpeed', 'BonusMoveSpeed')], 'duration': 'MovementSpeedBonusDuration'}],
    'upgrade_spellslinger_headshots': [
        {'trigger': 'BulletHit', 'target_stats': [('SpiritResist', 'TechArmorDamageReduction')], 'duration': 'DebuffDuration'},
        {'trigger': 'Headshot', 'cooldown': 'ProcCooldown', 'target_stats': [('SpiritResist', 'MagicResistReduction')], 'max_stacks': 'MaxStacks', 'duration': 'DebuffDuration'},
    ],
    'upgrade_banshee_slugs': [{'trigger': 'Headshot', 'duration': 'DebuffDuration',
                               'target_stats': RESIST_DOWN('BulletResistReduction') + [('SpiritResist', 'MagicResistReduction'), ('HealingReduction', '-HealAmpReceivePenaltyPercent')]}],
    # Imbued abilities
    'upgrade_magic_storm': [
        {'trigger': 'While', 'self_stats': [('SpiritPower', 'ImbuedTechPower')]},
        {'trigger': 'ImbuedCast', 'cooldown': 'AbilityCooldown', 'self_stats': [('FireRatePercent', 'FireRateBonus'), ('MoveSpeed', 'BonusMoveSpeed')], 'duration': 'MovementSpeedBonusDuration'},
    ],
    'upgrade_quick_silver': [
        {'trigger': 'ImbuedHit', 'cooldown': 'AbilityCooldown', 'spirit_damage': 'Damage'},
        {'trigger': 'ImbuedCast', 'cooldown': 'AbilityCooldown', 'self_stats': [('FireRatePercent', 'BonusFireRate')], 'duration': 'BuffDuration'},
    ],
    'upgrade_ethereal_bullets': [
        {'trigger': 'ImbuedHit', 'cooldown': 'AbilityCooldown', 'spirit_damage': 'Damage'},
        {'trigger': 'ImbuedCast', 'cooldown': 'AbilityCooldown', 'self_stats': [('FireRatePercent', 'BonusFireRate')], 'duration': 'BuffDuration'},
    ],
    'upgrade_shivas_bracelet': [
        {'trigger': 'While', 'self_stats': [('SpiritPower', 'ImbuedTechPower')]},
        {'trigger': 'ImbuedHit', 'cooldown': 'AbilityCooldown', 'spirit_damage': 'Damage', 'status': 'Stun', 'status_duration': 'FreezeDuration'},
    ],
    'upgrade_bulletshredimbue': [{'trigger': 'ImbuedHit', 'self_stats': [('WeaponDamage', 'WeaponPowerPerStack')], 'duration': 'AbilityDuration'}],
    'upgrade_mystic_reverb': [{'trigger': 'ImbuedHit', 'cooldown': 'AbilityCooldown', 'percent_of_hit': 'TechDamagePercent', 'radius': 'Radius', 'skip_hit_target': True,
                               'target_stats': [('MoveSlowPercent', '-MovementSpeedSlow')], 'duration': 'DelayDuration'}],
}

# Buying these asks for an ability: id -> bonuses for that ability alone, as (EZL_ItemStat, value). Empty when the procs do it all.
IMBUE_ITEMS = {
    'upgrade_magic_reach': [('AbilityRange', 'TechRangeMultiplier')],
    'upgrade_magic_tempo': [('CooldownReduction', 'CooldownReduction')],
    'upgrade_arcane_extension': [('AbilityDuration', 'BonusAbilityDurationPercent')],
    'upgrade_magic_storm': [],
    'upgrade_quick_silver': [],
    'upgrade_ethereal_bullets': [],
    'upgrade_shivas_bracelet': [('CooldownReduction', 'ImbuedCooldownReduction')],
    'upgrade_bulletshredimbue': [('AbilityRange', 'TechRangeMultiplier')],
    'upgrade_mystic_reverb': [],
}

# Recipes that leave out part of the item: the report keeps these as needing an effect.
PROC_PARTIAL = {
    'upgrade_slowing_bullets',  # no dash reduction
    'upgrade_weighted_shots',  # no dash reduction
    'upgrade_tech_defense_shredders',  # no lifesteal for allies
    'upgrade_proc_silence',  # the immunity is a cooldown, not per target
    'upgrade_weapon_backstabber',  # no debuff radius
    'upgrade_magic_slow',  # no dash reduction
    'upgrade_healbane',  # no heal on kill
    'upgrade_lifestrike_gauntlets',  # same cooldown for light melee
    'upgrade_boxing_glove',  # same cooldown for light melee
    'upgrade_acolytes_glove',  # same cooldown for light melee
    'upgrade_spirit_snatch',  # same strength for light melee
    'upgrade_chonky',  # no out of combat heal
    'upgrade_fervor',  # doesn't shorten debuffs already on you
    'upgrade_enchanted_holsters',  # no reload speed, not only in combat
    'upgrade_bullet_armor_reduction_aura',  # not doubled on a lone enemy
    'upgrade_headshot_booster2',  # no diminishing returns
    'upgrade_banshee_slugs',  # no diminishing returns
    'upgrade_spellslinger_headshots',  # no lifesteal for allies
    'upgrade_magic_reach',  # range only, no radius
    'upgrade_bulletshredimbue',  # range only, no stack limit
    'upgrade_magic_storm',  # the shooting penalty isn't removed
    'upgrade_quick_silver',  # no reload, no charge up over time
    'upgrade_ethereal_bullets',  # no reload, no bonus on bullets
    'upgrade_mystic_reverb',  # no delay, no lifesteal, weapon damage not spirit
}

PROC_DEFAULTS = {
    'trigger': 'None', 'cooldown': 0.0, 'chance': 0.0, 'min_damage': 0.0, 'damage_window': 0.0, 'build_up_hits': 0,
    'health_above': 0.0, 'health_below': 0.0, 'target_health_above': 0.0, 'min_range': 0.0, 'max_range': 0.0,
    'bonus_damage_percent': 0.0, 'spirit_damage': 0.0, 'weapon_damage': 0.0, 'target_health_percent': 0.0, 'percent_of_hit': 0.0,
    'heal': 0.0, 'heal_percent_of_damage': 0.0, 'heal_max_health_percent': 0.0, 'barrier': 0.0, 'barrier_duration': 0.0, 'self_stats': [], 'target_stats': [],
    'duration': 0.0, 'max_stacks': 0, 'radius': 0.0, 'max_targets': 0, 'area_team': 'Enemies', 'skip_hit_target': False,
    'dot_damage_per_second': 0.0, 'dot_max_health_percent': 0.0, 'dot_duration': 0.0, 'status': 'None', 'status_duration': 0.0, 'cleanse': False,
}
PROC_DISTANCES = {'min_range', 'max_range', 'radius'}
PROC_COUNTS = {'max_stacks', 'max_targets'}
PROC_TRIGGER_LABELS = {
    'BulletHit': 'On bullet hit', 'SpiritHit': 'On spirit damage', 'MeleeHit': 'On melee hit', 'HeavyMeleeHit': 'On heavy melee hit',
    'BulletTaken': 'When shot', 'SpiritTaken': 'When hit by spirit damage', 'DamageTaken': 'When damaged', 'Parry': 'On parry',
    'AbilityCast': 'On cast', 'UltimateCast': 'On ultimate', 'Periodic': 'Repeating', 'While': 'While', 'Aura': 'Aura',
    'Headshot': 'On headshot', 'ImbuedCast': 'On imbued cast', 'ImbuedHit': 'On imbued hit',
}

# Source build up is a tenth of the percent added per shot (0.7 = 7%): a reading of the numbers, tune here if it feels wrong.
BUILD_UP_PERCENT_SCALE = 10

# Actives that put a status on whoever they hit: id -> (statuses, property with the time).
ACTIVE_STATUSES = {
    'upgrade_targeted_silence': (['Silence'], 'AbilityDuration'),
    'upgrade_focus_lens': (['Silence'], 'AbilityDuration'),
    'upgrade_glitch': (['Silence', 'Disarm'], 'AbilityDuration'),
    'upgrade_target_stun': (['Stun'], 'StunDuration'),
    'upgrade_mystical_piano': (['Stun'], 'StunDuration'),
    'upgrade_phantom_strike': (['Disarm'], 'SlowDuration'),
    'upgrade_shrink_ray': (['Shrunk'], 'ShrinkDuration'),
    'upgrade_self_bubble': (['Invulnerable', 'Silence', 'Disarm'], 'AbilityDuration'),
    'upgrade_metal_skin': (['BulletImmune'], 'AbilityDuration'),
    'upgrade_unstoppable': (['Unstoppable'], 'AbilityDuration'),
    'upgrade_cloaking_device_active': (['Stealth'], 'AbilityDuration'),
}
ACTIVE_CLEANSE = {'upgrade_reduce_debuff_duration', 'upgrade_unstoppable', 'upgrade_celestial_guidance'}

# The rest of what an active does, in FZL_ActiveItem fields. Values read like PROC_RECIPES.
ACTIVE_EXTRAS = {
    'upgrade_warp_stone': {'dash_distance': 'AbilityCastRange', 'duration': 'CasterBuffDuration'},
    'upgrade_rocket_booster': {'launch_speed': 'JumpVelocityHidden'},
    'upgrade_phantom_strike': {'dash_to_target': True},
    'upgrade_shrink_ray': {'duration': 'ShrinkDuration'},
    'upgrade_colossus': {'caster_stats': [('WeaponResist', 'BuffBulletResist'), ('SpiritResist', 'BuffTechResist'), ('MeleeDamage', 'BonusMeleeDamagePercent')],
                         'caster_statuses': ['Enlarged'], 'status_duration': 'AbilityDuration'},
    'upgrade_rupture': {'dot_max_health_percent': 'DotHealthPercent', 'dot_duration': 'AbilityDuration'},
    'upgrade_target_stun': {'delay': 'StunDelay'},
    'upgrade_mystical_piano': {'delay': 'StunDelay'},
    'upgrade_thermal_detonator': {'zone_duration': 'AbilityDuration', 'zone_damage_per_second': 'DPS'},
    'upgrade_aoe_root': {'pull_to_point': True},
    'upgrade_rescue_beam': {'heal_max_health_percent': 'HealPercentAmount'},
    'upgrade_celestial_guidance': {'heal_max_health_percent': 'HealPercentAmount', 'duration': 'BuffDuration'},
    'upgrade_nullification_aura': {'timed_stats': [('HealingReduction', 100)], 'duration': 'AbilityDuration'},
}
ACTIVE_EXTRA_DISTANCES = {'dash_distance', 'launch_speed'}

# Items whose tooltip text only restates their stats.
TEXT_ONLY_RESTATES_STATS = {
    'upgrade_cooldown_reduction',
    'upgrade_healing_booster',
    'upgrade_debuff_reducer',
    'upgrade_cardio_calibrator',
    'upgrade_imbued_duration_extender',
    'upgrade_transcendent_cooldown',
    'upgrade_tech_range',
}


def load_raw(refresh):
    os.makedirs(OUT_DIR, exist_ok=True)
    if refresh or not os.path.exists(RAW):
        request = urllib.request.Request(API, headers={'User-Agent': 'ZeroLock-ItemImporter'})
        with urllib.request.urlopen(request, timeout=120) as response:
            data = response.read()
        with open(RAW, 'wb') as f:
            f.write(data)
    with open(RAW, encoding='utf-8') as f:
        return json.load(f)


def number(text):
    match = re.search(r'-?\d+(\.\d+)?', str(text))
    return float(match.group(0)) if match else 0.0


def section_properties(section):
    for attribute in section.get('section_attributes') or []:
        for key in ('properties', 'elevated_properties', 'important_properties'):
            yield from attribute.get(key) or []


def has_effect(item):
    """True when the item does more than its stats and its active, going by its tooltip."""
    source_id = item['class_name']
    if source_id in TEXT_ONLY_RESTATES_STATS:
        return False
    if source_id in PROC_RECIPES or source_id in IMBUE_ITEMS:
        return source_id in PROC_PARTIAL or (item.get('is_active_item') and source_id not in ACTIVE_COMPLETE)
    if item.get('is_active_item') and source_id not in ACTIVE_COMPLETE:
        return True
    for section in item.get('tooltip_sections') or []:
        if section.get('section_type') in ('innate', 'active'):
            continue
        if any(a.get('loc_string') for a in section.get('section_attributes') or []):
            return True
    return not item.get('is_active_item') and bool((item.get('description') or {}).get('desc'))


def read_stats(item):
    """(stats, description lines, API stats with no EZL_ItemStat yet)."""
    properties = item.get('properties') or {}
    restates = item['class_name'] in TEXT_ONLY_RESTATES_STATS
    stats, lines, missing = [], [], []
    seen = set()
    for section in item.get('tooltip_sections') or []:
        if section.get('section_type') != 'innate' and not restates:
            continue
        for key in section_properties(section):
            prop = properties.get(key)
            if key in seen or not isinstance(prop, dict):
                continue
            seen.add(key)
            kind = prop.get('provided_property_type')
            value = number(prop.get('value'))
            if not kind or value == 0:
                continue
            if kind in STATS:
                stat, label, *scale = STATS[kind]
                stats.append({'stat': stat, 'value': value * (scale[0] if scale else 1)})
                lines.append(label.format(f'{value:g}').replace('+-', '-'))
            else:
                missing.append(kind.replace('MODIFIER_VALUE_', ''))
    return stats, lines, missing


def property_value(properties, key):
    prop = properties.get(key)
    return number(prop.get('value')) if isinstance(prop, dict) else 0.0


def read_active(item):
    """The FZL_ActiveItem for an active item, None when it has no key yet."""
    source_id = item['class_name']
    if source_id not in ACTIVE_COMPLETE and source_id not in ACTIVE_PARTIAL:
        return None

    properties = item.get('properties') or {}
    active = {
        'target': ACTIVE_TARGETS.get(source_id, 'Self' if item.get('activation', '').startswith('instant') else 'Enemy'),
        'cooldown': property_value(properties, 'AbilityCooldown'),
        'duration': next((value for value in (property_value(properties, key) for key in DURATION_PROPERTIES) if value), 0.0),
        'range': property_value(properties, 'AbilityCastRange') * DISTANCE_CM_PER_M,
        'radius': next((value for value in (property_value(properties, key) for key in RADIUS_PROPERTIES) if value), 0.0) * DISTANCE_CM_PER_M,
        'area_team': 'Allies' if source_id in ACTIVE_AREA_ALLIES else 'Enemies',
        'damage': 0.0,
        'heal': 0.0,
        'barrier': 0.0,
        'barrier_duration': property_value(properties, 'BarrierDuration') or property_value(properties, 'BuffDuration'),
        'timed_stats': [],
        'statuses': [],
        'status_duration': 0.0,
        'cleanse': source_id in ACTIVE_CLEANSE,
        'delay': 0.0,
        'heal_max_health_percent': 0.0,
        'caster_stats': [],
        'caster_statuses': [],
        'dot_damage_per_second': 0.0,
        'dot_max_health_percent': 0.0,
        'dot_duration': 0.0,
        'zone_duration': 0.0,
        'zone_damage_per_second': 0.0,
        'dash_distance': 0.0,
        'dash_to_target': False,
        'launch_speed': 0.0,
        'pull_to_point': False,
    }

    if source_id in ACTIVE_STATUSES:
        active['statuses'], time_property = ACTIVE_STATUSES[source_id]
        active['status_duration'] = property_value(properties, time_property)

    for key, prop in properties.items():
        if not isinstance(prop, dict) or prop.get('tooltip_section') != 'active':
            continue
        value = number(prop.get('value'))
        kind = prop.get('provided_property_type')
        if not value:
            continue
        if key in ACTIVE_FIELDS:
            active[ACTIVE_FIELDS[key]] = value
        elif key in ACTIVE_STAT_PROPERTIES:
            active['timed_stats'].append({'stat': ACTIVE_STAT_PROPERTIES[key], 'value': value})
        elif kind in STATS:
            stat, _, *scale = STATS[kind]
            active['timed_stats'].append({'stat': stat, 'value': value * (scale[0] if scale else 1)})
        elif kind in ACTIVE_DEBUFF_TYPES:
            active['timed_stats'] += [{'stat': stat, 'value': -abs(value)} for stat in ACTIVE_DEBUFF_TYPES[kind]]
        elif kind == 'MODIFIER_VALUE_HEAL_AMP_RECEIVE_PERCENT':
            active['timed_stats'].append({'stat': 'HealingReduction', 'value': abs(value)})

    for field, ref in ACTIVE_EXTRAS.get(source_id, {}).items():
        if field in ('caster_stats', 'timed_stats'):
            active[field] += [{'stat': stat, 'value': proc_number(source_id, properties, value)} for stat, value in ref]
        elif field in ('caster_statuses', 'dash_to_target', 'pull_to_point'):
            active[field] = ref
        else:
            active[field] = proc_number(source_id, properties, ref, DISTANCE_CM_PER_M if field in ACTIVE_EXTRA_DISTANCES else 1.0)

    # Heal over time: the total spread over its own duration, as regen.
    total_regen = property_value(properties, 'TotalHealthRegen')
    regen_time = property_value(properties, 'RegenDuration')
    if total_regen and regen_time:
        active['timed_stats'].append({'stat': 'HealthRegen', 'value': total_regen / regen_time})
        active['duration'] = regen_time

    # Whoever an active hits gets all of it, so one that hits enemies can only carry debuffs.
    hits_enemies = active['target'] == 'Enemy' or (active['radius'] and active['area_team'] == 'Enemies')
    if hits_enemies:
        active['timed_stats'] = [entry for entry in active['timed_stats'] if entry['value'] < 0 or entry['stat'] == 'HealingReduction']
    elif active['damage']:
        raise ValueError(f'{source_id}: damage on an active that only hits allies')

    does_something = (active['damage'] or active['heal'] or active['heal_max_health_percent'] or active['barrier'] or active['statuses'] or active['cleanse']
                      or active['dash_distance'] or active['launch_speed'] or active['zone_duration'] or active['dot_duration'] or (active['timed_stats'] and active['duration']))
    if not does_something or not active['cooldown']:
        raise ValueError(f'{source_id}: listed as an active but has nothing to do')
    return active


def describe_active(active):
    """Original wording, built from the numbers."""
    parts = []
    if active['damage']:
        parts.append(f"{active['damage']:g} Spirit Damage")
    if active['heal']:
        parts.append(f"Heal {active['heal']:g}")
    if active['heal_max_health_percent']:
        parts.append(f"Heal {active['heal_max_health_percent']:g}% of max health")
    if active['barrier']:
        parts.append(f"{active['barrier']:g} Barrier")
    if active['dash_distance']:
        parts.append(f"Dash {active['dash_distance'] / DISTANCE_CM_PER_M:g}m")
    if active['dash_to_target']:
        parts.append('Dash to the target')
    if active['launch_speed']:
        parts.append('Launch upwards')
    if active['pull_to_point']:
        parts.append('Pull enemies together')
    if active['dot_max_health_percent']:
        parts.append(f"{active['dot_max_health_percent']:g}% max health per second for {active['dot_duration']:g}s")
    if active['zone_duration']:
        parts.append(f"{active['zone_damage_per_second']:g} Spirit Damage per second in the area for {active['zone_duration']:g}s")
    if active['delay']:
        parts.append(f"after {active['delay']:g}s")
    if active['caster_stats']:
        parts.append('You ' + describe_stats(active['caster_stats']))
    if active['timed_stats']:
        hits_enemies = active['target'] == 'Enemy' or active['area_team'] == 'Enemies' and active['radius']
        parts.append(f"Weakens for {active['duration']:g}s" if hits_enemies else f"Buffs for {active['duration']:g}s")
    if active['statuses']:
        parts.append(f"{' and '.join(active['statuses'])} for {active['status_duration']:g}s")
    if active['cleanse']:
        parts.append('Removes debuffs')
    return f"Active ({active['cooldown']:g}s cooldown): " + ', '.join(parts)


def proc_number(source_id, properties, ref, scale=1.0):
    """A recipe value: a number, or the property it names. A property with no value is a typo, so it stops the build."""
    if not isinstance(ref, str):
        return float(ref) * scale
    value = property_value(properties, ref.lstrip('-'))
    if not value:
        raise ValueError(f'{source_id}: no value for {ref}')
    return (-value if ref.startswith('-') else value) * scale


def read_procs(item):
    source_id = item['class_name']
    properties = item.get('properties') or {}
    procs = []
    for recipe in PROC_RECIPES.get(source_id, []):
        proc = dict(PROC_DEFAULTS)
        for field, ref in recipe.items():
            if field in ('trigger', 'area_team', 'status', 'skip_hit_target', 'cleanse'):
                proc[field] = ref
            elif field in ('self_stats', 'target_stats'):
                proc[field] = [{'stat': stat, 'value': proc_number(source_id, properties, value, SPEED_CM_PER_M if stat == 'MoveSpeed' else 1.0)} for stat, value in ref]
            elif field == 'build_up_hits':
                proc[field] = math.ceil(100 / (proc_number(source_id, properties, ref) * BUILD_UP_PERCENT_SCALE))
            elif field in PROC_COUNTS:
                proc[field] = int(proc_number(source_id, properties, ref))
            else:
                proc[field] = proc_number(source_id, properties, ref, DISTANCE_CM_PER_M if field in PROC_DISTANCES else 1.0)
        procs.append(proc)
    return procs


def describe_stats(stats):
    parts = []
    for entry in stats:
        if entry['stat'] == 'MoveSpeed':
            parts.append(f"{entry['value'] / SPEED_CM_PER_M:+.3g} m/s Move Speed")
        else:
            parts.append(f"{entry['value']:+g} {re.sub(r'(?<=[a-z])(?=[A-Z])', ' ', entry['stat'])}")
    return ', '.join(parts)


def describe_proc(proc):
    """Original wording, built from the numbers."""
    parts = []
    if proc['bonus_damage_percent']:
        parts.append(f"+{proc['bonus_damage_percent']:g}% damage")
    if proc['spirit_damage']:
        parts.append(f"{proc['spirit_damage']:g} Spirit Damage")
    if proc['weapon_damage']:
        parts.append(f"{proc['weapon_damage']:g} Weapon Damage")
    if proc['target_health_percent']:
        parts.append(f"{proc['target_health_percent']:g}% of current health as Spirit Damage")
    if proc['percent_of_hit']:
        parts.append(f"{proc['percent_of_hit']:g}% of the hit to others" if proc['skip_hit_target'] else f"Returns {proc['percent_of_hit']:g}% of the hit")
    if proc['dot_damage_per_second']:
        parts.append(f"{proc['dot_damage_per_second']:g} Spirit Damage per second for {proc['dot_duration']:g}s")
    if proc['dot_max_health_percent']:
        parts.append(f"{proc['dot_max_health_percent']:g}% max health per second for {proc['dot_duration']:g}s")
    if proc['heal']:
        parts.append(f"Heal {proc['heal']:g}")
    if proc['heal_percent_of_damage']:
        parts.append(f"Heal {proc['heal_percent_of_damage']:g}% of the damage")
    if proc['heal_max_health_percent']:
        parts.append(f"Heal {proc['heal_max_health_percent']:g}% of max health")
    if proc['barrier']:
        parts.append(f"{proc['barrier']:g} Barrier")
    if proc['target_stats']:
        parts.append('Target ' + describe_stats(proc['target_stats']))
    if proc['self_stats']:
        parts.append('You ' + describe_stats(proc['self_stats']))
    if proc['status'] != 'None':
        parts.append(f"{proc['status']} for {proc['status_duration']:g}s")

    conditions = []
    if proc['health_above']:
        conditions.append(f"above {proc['health_above']:g}% health")
    if proc['health_below']:
        conditions.append(f"below {proc['health_below']:g}% health")
    if proc['target_health_above']:
        conditions.append(f"target above {proc['target_health_above']:g}% health")
    if proc['max_range']:
        conditions.append(f"within {proc['max_range'] / DISTANCE_CM_PER_M:g}m")
    if proc['min_range']:
        conditions.append(f"beyond {proc['min_range'] / DISTANCE_CM_PER_M:g}m")
    if proc['chance']:
        conditions.append(f"{proc['chance']:g}% chance")
    if proc['build_up_hits']:
        conditions.append(f"after {proc['build_up_hits']} hits")
    if proc['min_damage']:
        window = f" in {proc['damage_window']:g}s" if proc['damage_window'] else ''
        conditions.append(f"{proc['min_damage']:g}+ damage{window}")
    if proc['duration']:
        parts.append(f"for {proc['duration']:g}s")
    if proc['max_stacks']:
        parts.append(f"stacks {proc['max_stacks']} times")
    if proc['radius']:
        parts.append(f"{proc['radius'] / DISTANCE_CM_PER_M:g}m radius")
    if proc['cooldown']:
        parts.append(f"{proc['cooldown']:g}s cooldown")

    label = PROC_TRIGGER_LABELS[proc['trigger']]
    condition = f" ({', '.join(conditions)})" if conditions else ''
    return f"{label}{condition}: " + ', '.join(parts)


def main():
    refresh = '--refresh' in sys.argv
    show_partial = '--show-partial' in sys.argv

    with open(os.path.join(HERE, 'names.json'), encoding='utf-8') as f:
        names = json.load(f)

    shop = [x for x in load_raw(refresh) if x.get('type') == 'upgrade' and x.get('shopable')]
    upgrades = {}
    for item in shop:
        for component in item.get('component_items') or []:
            upgrades.setdefault(component, []).append(item['class_name'])

    items = []
    for item in shop:
        source_id = item['class_name']
        stats, lines, missing = read_stats(item)
        active = read_active(item)
        if active:
            lines.append(describe_active(active))
        procs = read_procs(item)
        imbue_stats = [{'stat': stat, 'value': proc_number(source_id, item.get('properties') or {}, value)} for stat, value in IMBUE_ITEMS.get(source_id, [])]
        if source_id in IMBUE_ITEMS:
            lines.append('Imbue: pick an ability' + (': ' + describe_stats(imbue_stats) if imbue_stats else ''))
        lines += [describe_proc(proc) for proc in procs]
        effect = has_effect(item)
        works = stats or active or procs or imbue_stats
        status = 'complete' if not effect and not missing and works else 'partial' if works else 'unsupported'
        name = names.get(source_id)
        if not name:
            name = 'Unnamed ' + source_id.replace('upgrade_', '')
            status = 'unsupported'
        cost = int(item.get('cost') or 0)
        items.append({
            'id': source_id,
            'name': name,
            'type': item['item_slot_type'].capitalize(),
            'tier': int(item['item_tier']),
            'cost': cost,
            'status': status,
            'in_shop': status == 'complete' or (show_partial and status == 'partial'),
            'needs_effect': effect,
            'needs_stats': sorted(set(missing)),
            'stats': stats,
            'active': active,
            'procs': procs,
            'imbue': source_id in IMBUE_ITEMS,
            'imbue_stats': imbue_stats,
            'description': '\n'.join([f'{cost} Souls'] + lines),
            'upgrades_to': sorted(upgrades.get(source_id, [])),
        })
    items.sort(key=lambda x: (x['type'], x['tier'], x['name']))

    with open(os.path.join(HERE, 'items.json'), 'w', encoding='utf-8', newline='\n') as f:
        json.dump(items, f, indent=1)
        f.write('\n')

    report = ['# Item import report', '', '| Status | Shop | Name | Source ID | Tier | Cost | Still needs |', '|---|---|---|---|---|---|---|']
    for item in sorted(items, key=lambda x: (x['status'], x['type'], x['tier'], x['name'])):
        needs = (['effect'] if item['needs_effect'] else []) + item['needs_stats']
        report.append(f"| {item['status']} | {'yes' if item['in_shop'] else ''} | {item['name']} | {item['id']} | "
                      f"{item['type']} {item['tier']} | {item['cost']} | {', '.join(needs)} |")
    with open(os.path.join(OUT_DIR, 'report.md'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(report) + '\n')

    counts = {}
    for item in items:
        counts[item['status']] = counts.get(item['status'], 0) + 1
    print(f"{len(items)} items, {sum(x['in_shop'] for x in items)} in shop, {counts}")
    print('Report: ' + os.path.join(OUT_DIR, 'report.md'))


if __name__ == '__main__':
    main()
