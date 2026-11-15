# Copyright Preetham Mukundan (C) 2026
"""
Hero kit specs: the 19 heroes from the hero design sheet, as data for build_heroes.py.

Slot mapping (the sheet has 7 entries per hero; decided with the user 2026-11-13):
  normal attack -> PrimaryFireAbility (weapon BP + projectile BP copies)
  secondary     -> SecondryFireAbility (slot 1)
  ability 1     -> Ability_1 (slot 2)
  ability 2     -> Ability_2 (slot 3)
  ult           -> UltimateAbility
  f             -> built as a BP but NOT put on the hero (the user decides where it goes); right mouse stays aim zoom
  passive       -> DefaultAbilities

Each ability: (AssetName, ParentClass, {python property name: value}). Payload/area/zone values are dicts; build_heroes.py
turns them into the structs. None = needs new tech (Docs/Claude/HeroKit.md "Not built"). Every number is a first guess.
"""

KIT = "/Script/ZeroLock."


def stun(t):
    return {"statuses": ["Stun"], "status_duration": t}


def merge(*dicts):
    out = {}
    for d in dicts:
        out.update(d)
    return out


def slow(pct, t):
    return {"stats": [("MoveSlowPercent", -pct)], "stat_duration": t}


def cd(seconds):
    return {"cooldown_duration": seconds}


def proj(speed, life, gravity=0.0, radius=30.0, color=(1, 0.5, 0.1), **extra):
    """Settings for an AZL_Kit_Projectile BP child made next to the ability."""
    d = {"speed": speed, "life": life, "gravity": gravity, "hitbox": radius, "color": color}
    d.update(extra)
    return d


# Hero kit effects: Niagara only, played through gameplay cues. Sources: /Game/ZERO/HeroKit/FX/Niagara (NS_HeroKit_*
# made for the kit; NS_<name> = Niagara conversions of Epic's Paragon effects, made by make_fx_assets.py).
NS = "/Game/ZERO/HeroKit/FX/Niagara/"
ICE = NS + "NS_HeroKit_IceSpikes"


def cue(role, system=None, scale=1.0, radius=0.0, forward=0.0, up=0.0, attach=False, duration=0.0, sound=None,
        params=None, loop=False):
    """A gameplay cue. build_heroes.py makes the tag GameplayCue.HeroKit.<Hero>.<Ability>.<role> and the notify asset
    Heroes/<Hero>/FX/GCN_<Ability>_<role> (UZL_GCN_KitBurst, or AZL_GCN_KitLoop when loop or duration > 0) holding
    this visual. radius > 0 scales the effect with the hit's area (ReferenceRadius = radius). params = Niagara user
    floats. duration > 0: added to the hero it plays on and removed after that many seconds."""
    visual = {"scale": scale, "forward_offset": forward, "up_offset": up, "attach_to_target": attach,
              "scale_with_radius": False}
    if system:
        visual["niagara_system"] = system
    if sound:
        visual["sound"] = sound
    if radius > 0:
        visual["scale_with_radius"] = True
        visual["reference_radius"] = radius
    if params:
        visual["float_parameters"] = params
    return {"__cue__": role, "visual": visual, "duration": duration, "loop": loop or duration > 0}


def ice(role, count, height, spread=0.0, life=1.0, **kwargs):
    """Ice spikes bursting out of the floor (NS_HeroKit_IceSpikes). spread 0 = the hit's radius."""
    params = {"SpikeCount": count, "Height": height, "Lifetime": life}
    if spread:
        params["Radius"] = spread
    return cue(role, ICE, params=params, **kwargs)


def find_cues(value, found=None):
    """Every cue dict inside a spec value (for gen_tags.py and build_heroes.py)."""
    found = [] if found is None else found
    if isinstance(value, dict):
        if "__cue__" in value:
            found.append(value)
        else:
            for item in value.values():
                find_cues(item, found)
    elif isinstance(value, (list, tuple)):
        for item in value:
            find_cues(item, found)
    return found


def cue_tag(hero, context, role):
    return f"GameplayCue.HeroKit.{hero}.{context}.{role}"


def weapon(mode="Single", speed=8000, life=0.8, gravity=0.0, spread=1.0, pellets=1, pellet_spread=6.0, bounces=0):
    return {"fire_mode": mode, "speed": speed, "life": life, "gravity": gravity, "spread": spread,
            "pellets": pellets, "pellet_spread": pellet_spread, "bounces": bounces}


# Heroes that get a DT_HeroList row (hero select). The rest are built but hidden until they're ready (user, 2026-11-14).
SELECTOR_HEROES = {"KaiKoa"}

HEROES = {
    # ------------------------------------------------------------------------------------------------------------------
    "KaiKoa": {
        "display": "Kai Koa", "color": (0.1, 0.6, 1.0),
        "desc": "Water and ice brawler with razor blades of condensed water on his knuckle busters.",
        "weapon": merge(weapon("Shotgun", speed=6000, life=0.12, pellets=3, pellet_spread=6), {
            "trail_cue": cue("Trail", NS + "NS_TwinBlast_Primary_Trail_Summer", loop=True),
            "hit_character_cue": cue("HitHero", NS + "NS_TwinBlast_Primary_HitCharacter_Summer"),
            "hit_world_cue": cue("HitWorld", NS + "NS_TwinBlast_Primary_HitWorld_Summer")}),
        "secondary": ("SharkSharkShark", "ZL_Kit_ProjectileAbility", merge(cd(8), {
            "ability_name": "Shark Shark Shark", "count": 3, "fan_angle": 24,
            "projectile": proj(2200, 0.8, radius=40, color=(0.2, 0.7, 1), pierce=True, hide_placeholder=True,
                               trail_cue=cue("Trail", NS + "NS_TwinBlast_Nitro_Trail_Summer", scale=1.6, loop=True),
                               impact_cue=cue("Impact", NS + "NS_TwinBlast_Primary_HitWorld_Summer", scale=1.5)),
            "hit": {"direct_payload": {"damage": 45, "hit_cue": cue("HitHero", NS + "NS_TwinBlast_Primary_HitCharacter_Summer", scale=1.5)}}})),
        "ability1": ("TsunamiFist", "ZL_Kit_Ability", merge(cd(10), {
            "ability_name": "Tsunami Fist", "move_forward_speed": 1200, "move_duration": 0.15, "hit_delay": 0.15,
            "area": {"shape": "Cone", "range": 450, "angle": 40},
            "cast_cue": cue("Cast", NS + "NS_TwinBlast_Nitro_SteamRelease_Summertime", duration=0.5),
            "pulse_cue": cue("Wave", NS + "NS_Twinblast_VortexGrenade_ExplodeWater", radius=450, forward=220),
            "enemy_payload": {"damage": 70, "knockback_speed": 1300, "knock_up_speed": 350,
                              "hit_cue": cue("HitHero", NS + "NS_GrenadeBounceSplash", scale=1.2)}})),
        "ability2": ("ArcticBurst", "ZL_Kit_Ability", merge(cd(14), {
            "ability_name": "Arctic Burst", "forward_offset": 350, "pulse_count": 1,
            "area": {"shape": "Sphere", "range": 450},
            "pulse_cue": ice("Burst", 18, 120, life=1.2),
            "enemy_payload": merge({"damage": 60}, slow(40, 1.5),
                                   {"hit_cue": ice("Chill", 3, 70, spread=45, life=1.4, duration=1.4)})})),
        "f": None,  # Mist Shroud: vision effects
        "ult": ("AbsoluteZero", "ZL_Kit_Ability", merge(cd(90), {
            "ability_name": "Absolute Zero", "pulse_count": 1, "hit_delay": 0.2,
            "area": {"shape": "Sphere", "range": 900, "require_line_of_sight": False},
            "cast_cue": cue("Cast", NS + "NS_GroundSlam_Radius", scale=2.0, sound="/Game/StarterContent/Audio/Collapse_Cue"),
            "pulse_cue": ice("Strike", 40, 160, life=1.6),
            "enemy_payload": merge({"damage": 90, "statuses": ["Stun", "Root"], "status_duration": 3.0},
                                   {"hit_cue": ice("Frozen", 6, 170, spread=50, life=3.0, duration=3.0)})})),
        "passives": [("FrozenFist", "ZL_KaiKoa_FrozenFist", {
            "ability_name": "Frozen Fist",
            "ability_description": "Melee hits add frost to the enemy; all of it fades 3 s after the last hit. At 3 stacks the enemy freezes for 1.5 s and the frost clears.",
            "frost_triggers": ["MeleeHit", "HeavyMeleeHit"], "stacks_per_hit": 1, "stacks_to_freeze": 3,
            "frost_decay_time": 3.0, "stack_cooldown_per_target": 0.2,
            "on_hit_payload": None, "on_hit_chance": 0, "on_hit_cooldown": 0, "on_hit_triggers": None,
            "stack_cue": ice("Frost", 3, 60, spread=40, life=0.8),
            "freeze_payload": {"statuses": ["Stun", "Root"], "status_duration": 1.5,
                               "hit_cue": ice("Frozen", 5, 150, spread=45, life=1.5, duration=1.5)}})],
    },
    # ------------------------------------------------------------------------------------------------------------------
    "Tempest": {
        "display": "Tempest", "color": (0.7, 1.0, 0.8),
        "desc": "The guiding wind, in his 10th reincarnation.",
        "weapon": weapon("Single", speed=9000, life=0.8),
        "secondary": None,  # Rebound: redirecting missed attacks (hard)
        "ability1": None,  # Alley-oop: homing shots
        "ability2": ("PhantomDrive", "ZL_Kit_DashAbility", merge(cd(14), {
            "ability_name": "Phantom Drive", "dash_distance": 900, "dash_duration": 0.3,
            "self_payload": {"statuses": ["Stealth"], "status_duration": 3},
            "hit_payload": {"damage": 40}})),
        "f": ("SkyLeap", "ZL_Kit_Ability", merge(cd(16), {
            "ability_name": "Sky Leap", "ability_description": "Sheet name 'Air Jordan' (trademark: renamed).",
            "move_forward_speed": 600, "move_up_speed": 1400, "move_duration": 0.35, "is_movement_ability": True,
            "hit_delay": 0.9, "area": {"shape": "Sphere", "range": 400},
            "enemy_payload": merge({"damage": 70}, stun(0.8))})),
        "ult": ("IAmNumber10", "ZL_Kit_ProjectileAbility", merge(cd(100), {
            "ability_name": "I Am Number 10", "move_up_speed": 1600, "move_duration": 0.3, "stop_after_move": True,
            "activation_owned_tags": ["Zerolock.Status.Floating"], "fire_delay": 0.35,
            "volley_count": 10, "volley_interval": 0.45, "volley_interval_scale": 0.88, "area_radius_per_volley": 20,
            "projectile": proj(3500, 1.5, radius=35, color=(0.7, 1, 0.8)),
            "hit": {"direct_payload": {"damage": 35}, "area_radius": 150, "area_payload": {"damage": 35}}})),
        "passives": [("WindRider", "ZL_Tempest_WindRider", {"ability_name": "Wind Rider"})],
    },
    # ------------------------------------------------------------------------------------------------------------------
    "KaijuX": {
        "display": "Kaiju X", "color": (0.5, 0.1, 0.9),
        "desc": "The alpha. Plasma claws, plasma fists, and a kaiju's appetite.",
        "weapon": weapon("Single", speed=2500, life=0.6, spread=0.5),
        "secondary": ("PlasmaFist", "ZL_KaijuX_PlasmaFist", merge(cd(12), {
            "ability_name": "Plasma Fist", "move_forward_speed": 900, "move_duration": 0.15, "hit_delay": 0.15,
            "area": {"shape": "Cone", "range": 220, "angle": 50},
            "enemy_payload": {"damage": 30, "damage_type": "Melee"}})),
        "ability1": ("ImStarving", "ZL_KaijuX_ImStarving", merge(cd(18), {
            "ability_name": "I'm Starving", "range": 1200, "target_team": "Enemies",
            "target_payload": {"damage": 60, "dot_per_second": 15, "dot_duration": 4, "strip_barrier": True,
                               "statuses": ["Root"], "status_duration": 1.0}})),
        "ability2": None,  # Titan King: turrets
        "f": ("Camouflage", "ZL_Kit_Ability", merge(cd(22), {
            "ability_name": "Camouflage", "has_area_hit": False,
            "self_payload": {"statuses": ["Stealth"], "status_duration": 5,
                             "stats": [("MoveSpeed", 150), ("WeaponResist", 20), ("SpiritResist", 20)], "stat_duration": 5}})),
        "ult": None,  # Plasma Beam: beam weapon
        "passives": [("Alpha", "ZL_KaijuX_Alpha", {"ability_name": "The Alpha"})],
    },
    # ------------------------------------------------------------------------------------------------------------------
    "Ayiana": {
        "display": "Ayiana", "color": (0.3, 0.8, 0.2),
        "desc": "Heir of the Amazon and her lifelong partner Kong.",
        "weapon": weapon("Single", speed=3500, life=1.2, gravity=0.5),
        "secondary": ("ReelIn", "ZL_Kit_TargetAbility", merge(cd(12), {
            "ability_name": "Reel In", "range": 1400, "target_team": "Enemies",
            "move": "PullTargetToCaster", "move_time": 0.3, "move_offset": 150,
            "target_payload": merge({"damage": 40}, stun(0.4))})),
        "ability1": ("DeepFocus", "ZL_Kit_Ability", merge(cd(18), {
            "ability_name": "Deep Focus", "has_area_hit": False, "active_duration": 6,
            "self_payload": {"stats": [("WeaponDamage", 30), ("FireRatePercent", 25)], "stat_duration": 6}})),
        "ability2": None,  # Slippery Slope: slip into walls
        "f": None,  # Let's Swing: swing movement mode
        "ult": ("Playtime", "ZL_Kit_DashAbility", merge(cd(80), {
            "ability_name": "Playtime!", "dash_distance": 2200, "dash_duration": 1.2,
            "hit_payload": {"damage": 120, "knockback_speed": 1200, "knock_up_speed": 400}})),
        "passives": [],  # Monkey Bars: wall climbing
    },
    # ------------------------------------------------------------------------------------------------------------------
    "LadyGrimm": {
        "display": "Lady Grimm", "color": (0.8, 0.05, 0.15),
        "desc": "Death with a ruby scythe, harvesting souls to keep her allies alive.",
        "weapon": weapon("Shotgun", speed=7000, life=0.08, pellets=5, pellet_spread=12),
        "secondary": ("DoctorsApple", "ZL_Kit_ProjectileAbility", merge(cd(10), {
            "ability_name": "Doctor's Apple", "pitch_offset": 8,
            "projectile": proj(1800, 3.0, gravity=1.0, radius=30, color=(0.9, 0.1, 0.1)),
            "hit": {"direct_payload": {"damage": 30, "dot_per_second": 18, "dot_duration": 5},
                    "area_radius": 150, "area_payload": {"dot_per_second": 18, "dot_duration": 5}}})),
        "ability1": ("CursedAura", "ZL_LadyGrimm_CursedAura", merge(cd(20), {
            "ability_name": "Cursed Aura", "active_duration": 5, "pulse_count": 5, "pulse_interval": 1.0,
            "area": {"shape": "Sphere", "range": 500, "require_line_of_sight": False},
            "enemy_payload": {"damage": 18}, "ally_payload": {"heal": 8}})),
        "ability2": ("Necromancer", "ZL_LadyGrimm_Necromancer", merge(cd(6), {
            "ability_name": "Necromancer",
            "one_orb_payload": {"stats": [("HealthRegen", 8)], "stat_duration": 20},
            "two_orb_payload": {"heal_max_health_percent": 50},
            "three_orb_ally_payload": {"heal_max_health_percent": 30},
            "three_orb_enemy_payload": {"damage": 120}})),
        "f": ("AbyssalMist", "ZL_Kit_TargetAbility", merge(cd(16), {
            "ability_name": "Abyssal Mist", "range": 1500, "target_team": "Enemies", "is_movement_ability": True,
            "move": "TeleportBehind", "move_offset": 150})),
        "ult": ("SoulLink", "ZL_LadyGrimm_SoulLink", merge(cd(100), {
            "ability_name": "Soul Link", "range": 1800, "link_duration": 7, "share_multiplier": 1.0})),
        "passives": [],  # I Am Death: resurrect
    },
    # ------------------------------------------------------------------------------------------------------------------
    "Yatagarasu": {
        "display": "Yatagarasu", "color": (1.0, 0.8, 0.2),
        "desc": "The yokai of the sun, wearing the stolen Amaterasu suit.",
        "weapon": weapon("Single", speed=12000, life=0.5, spread=2.5),
        "secondary": None,  # Flash Bang: blind
        "ability1": None,  # Photon Shield: bullet-blocking wall
        "ability2": None,  # Yatagarasu's Wrath: sentry crow
        "f": ("SpectralForm", "ZL_Kit_Ability", merge(cd(18), {
            "ability_name": "Spectral Form", "has_area_hit": False, "active_duration": 2.5,
            "self_payload": {"statuses": ["Invulnerable", "Disarm"], "status_duration": 2.5,
                             "stats": [("MoveSpeed", 100)], "stat_duration": 2.5}})),
        "ult": None,  # Eclipse: darkness
        "passives": [("SunlitVigil", "ZL_Yatagarasu_SunlitVigil", {
            "ability_name": "Sunlit Vigil", "vigil_time": 90,
            "yokai_payload": {"stats": [("MoveSpeed", 150), ("WeaponDamage", 35)], "stat_duration": 60}})],
    },
    # ------------------------------------------------------------------------------------------------------------------
    "Scarab": {
        "display": "Scarab", "color": (0.85, 0.55, 0.15),
        "desc": "Armoured beetle warrior with earthen cannons and thruster wings.",
        "weapon": weapon("Single", speed=4000, life=1.5, gravity=0.4),
        "secondary": None,  # Skin Shed: bullet-blocking husk
        "ability1": ("Phloeodes", "ZL_Kit_Ability", merge(cd(24), {
            "ability_name": "Phloeodes Diabolicus", "has_area_hit": False, "active_duration": 5,
            "self_payload": {"statuses": ["Unstoppable"], "status_duration": 5,
                             "stats": [("WeaponResist", 35), ("SpiritResist", 35)], "stat_duration": 5}})),
        "ability2": ("Dynastinae", "ZL_Kit_Ability", merge(cd(12), {
            "ability_name": "Dynastinae", "move_forward_speed": 900, "move_duration": 0.2, "hit_delay": 0.15,
            "area": {"shape": "Cone", "range": 350, "angle": 45},
            "enemy_payload": {"damage": 60, "knockback_speed": 1600, "knock_up_speed": 900}})),
        "f": ("Cicindela", "ZL_Kit_DashAbility", merge(cd(14), {
            "ability_name": "Cicindela Hudsoni", "dash_distance": 1300, "dash_duration": 0.35,
            "hit_payload": {"damage": 50}})),
        "ult": ("Hercules", "ZL_Kit_Ability", merge(cd(90), {
            "ability_name": "Hercules", "has_area_hit": False, "active_duration": 8,
            "self_payload": {"stats": [("WeaponResist", 40), ("SpiritResist", 40), ("FireRatePercent", 40)], "stat_duration": 8},
            "while_active_hit_payload": {"knockback_speed": 700, "knock_up_speed": 150}})),
        "passives": [("HouseOfLife", "ZL_Kit_Passive", {
            "ability_name": "House of Life", "on_kill_chance": 30,
            "on_kill_payload": {"barrier": 200, "barrier_duration": 10}})],
    },
    # ------------------------------------------------------------------------------------------------------------------
    "VanHelsing": {
        "display": "Van Helsing", "color": (0.6, 0.6, 0.7),
        "desc": "Monster hunter with a stake-firing sniper.",
        "weapon": weapon("Single", speed=20000, life=1.0, spread=0.0),
        "secondary": ("SilverNet", "ZL_Kit_ProjectileAbility", merge(cd(12), {
            "ability_name": "Silver Net",
            "projectile": proj(2500, 1.2, gravity=0.2, radius=45, color=(0.85, 0.85, 0.9)),
            "hit": {"direct_payload": {"damage": 20, "statuses": ["Root"], "status_duration": 1.75}}})),
        "ability1": ("HolyWater", "ZL_Kit_ProjectileAbility", merge(cd(16), {
            "ability_name": "Holy Water", "pitch_offset": 10,
            "projectile": proj(1500, 3.0, gravity=1.0, color=(0.3, 0.9, 0.3)),
            "hit": {"zone": {"duration": 5, "radius": 350, "interval": 0.5, "color": (0.3, 0.9, 0.3),
                             "enemy_payload": {"dot_per_second": 30, "dot_duration": 0.5}}}})),
        "ability2": ("Trapmaster", "ZL_Kit_ProjectileAbility", merge(cd(14), {
            "ability_name": "Trapmaster", "count": 3, "fan_angle": 30, "pitch_offset": 12,
            "projectile": proj(1300, 3.0, gravity=1.0, radius=20, color=(0.4, 0.4, 0.4)),
            "hit": {"zone": {"duration": 30, "radius": 250, "trigger_once": True, "trigger_radius": 120, "arm_delay": 1.0,
                             "max_per_caster": 6, "color": (0.6, 0.1, 0.1), "enemy_payload": {"damage": 70}}}})),
        "f": ("JekyllsVial", "ZL_Kit_Ability", merge(cd(20), {
            "ability_name": "Dr. Jekyll's Vial", "has_area_hit": False, "is_movement_ability": True,
            "move_up_speed": 1500, "move_duration": 0.25, "self_payload": {"heal": 120}})),
        "ult": ("MonsterHunter", "ZL_Kit_Ability", merge(cd(90), {
            "ability_name": "Monster Hunter", "has_area_hit": False, "active_duration": 8,
            "self_payload": {"stats": [("FireRatePercent", 150)], "stat_duration": 8,
                             "timed_tags": ["Zerolock.Cheat.InfiniteAmmo"], "tag_duration": 8}})),
        "passives": [],  # Full Moon Night: thermal vision
    },
    # ------------------------------------------------------------------------------------------------------------------
    "Jet": {
        "display": "Jet", "color": (1.0, 0.35, 0.05),
        "desc": "Dragon-raised orphan cast into the future, flying on a scrap jetpack.",
        "weapon": weapon("Single", speed=3000, life=1.5, gravity=0.6),
        "secondary": ("DragonOil", "ZL_Kit_ProjectileAbility", merge(cd(5), {
            "ability_name": "Dragon Oil", "pitch_offset": 10,
            "projectile": proj(1600, 3.0, gravity=1.0, color=(1, 0.5, 0)),
            "hit": {"zone": {"duration": 6, "radius": 300, "interval": 0.5, "max_per_caster": 4, "color": (1, 0.4, 0),
                             "enemy_payload": {"dot_per_second": 30, "dot_duration": 0.5}}}})),
        "ability1": ("DragonKaboom", "ZL_Kit_ProjectileAbility", merge(cd(12), {
            "ability_name": "Dragon Kaboom", "pitch_offset": 8,
            "projectile": proj(1600, 2.5, gravity=1.0, radius=25, color=(0.2, 0.2, 0.2), bounce=0.5),
            "hit": {"area_radius": 400, "area_payload": {"damage": 140}}})),
        "ability2": None,  # Predator's Instinct: reveal
        "f": None,  # Dragons Can Fly: jetpack
        "ult": ("TheSoulRemembers", "ZL_Kit_ProjectileAbility", merge(cd(90), {
            "ability_name": "The Soul Remembers", "volley_count": 12, "volley_interval": 0.35,
            "projectile": proj(3000, 1.5, radius=40, color=(1, 0.3, 0)),
            "hit": {"direct_payload": {"damage": 50}, "area_radius": 200,
                    "area_payload": {"damage": 30, "dot_per_second": 10, "dot_duration": 2}}})),
        "passives": [("DragonsDesire", "ZL_Kit_Passive", {
            "ability_name": "A Dragon's Desire",
            "procs": [{"trigger": "SpiritTaken", "self_stats": [("WeaponDamage", 4)], "duration": 5, "max_stacks": 5}]})],
    },
    # ------------------------------------------------------------------------------------------------------------------
    "JA594": {
        "display": "JA594", "color": (0.4, 0.2, 1.0),
        "desc": "Unholy machine with electric chakras and a demonic katana.",
        "weapon": weapon("Single", speed=3500, life=1.2, bounces=2),
        "secondary": ("UnholyKirin", "ZL_Kit_ProjectileAbility", merge(cd(10), {
            "ability_name": "Unholy Kirin",
            "projectile": proj(3000, 2.0, radius=40, color=(0.5, 0.2, 1), pierce=True, return_after=0.6),
            "hit": {"direct_payload": {"damage": 55}}})),
        "ability1": None,  # Demonic Binding: linked trip fields
        "ability2": ("DemonsRage", "ZL_Kit_Ability", merge(cd(20), {
            "ability_name": "Demon's Rage", "area": {"shape": "Sphere", "range": 600},
            "enemy_payload": {"damage": 40, "statuses": ["Silence"], "status_duration": 2.5}})),
        "f": ("Gateway", "ZL_JA594_Gateway", merge(cd(18), {"ability_name": "Gateway", "gate_lifetime": 12})),
        "ult": ("HellsStorm", "ZL_Kit_Ability", merge(cd(100), {
            "ability_name": "Hell's Storm", "forward_offset": 600, "pulse_count": 16, "pulse_interval": 0.35,
            "random_strike_spread": 1000, "area": {"shape": "Sphere", "range": 220, "require_line_of_sight": False},
            "enemy_payload": merge({"damage": 55}, stun(0.3))})),
        "passives": [("UnholyMachine", "ZL_JA594_UnholyMachine", {
            "ability_name": "Unholy Machine", "kills_to_awaken": 3, "explosion_radius": 400,
            "explosion_payload": {"damage": 80}})],
    },
    # ------------------------------------------------------------------------------------------------------------------
    "Sobek": {
        "display": "Sobek", "color": (0.1, 0.5, 0.4),
        "desc": "God of the Nile, risen from the dark waters of Nun.",
        "weapon": weapon("Single", speed=5000, life=0.25, spread=4.0),
        "secondary": None,  # Breath of Life: heal beam
        "ability1": None,  # Crown of Wisdom: map reveal
        "ability2": ("DepthsOfNile", "ZL_Kit_TargetAbility", merge(cd(18), {
            "ability_name": "Depths of Nile", "range": 1400, "target_team": "Enemies",
            "target_payload": {"statuses": ["Root"], "status_duration": 2.5, "dot_per_second": 30, "dot_duration": 2.5}})),
        "f": None,  # Nun Descent: swim under the map
        "ult": None,  # Sebek-khu Stele: team-filtered tomb
        "passives": [],  # God of Water: no elemental damage yet
    },
    # ------------------------------------------------------------------------------------------------------------------
    "GenghisKhan": {
        "display": "Genghis Khan", "color": (0.9, 0.25, 0.1),
        "desc": "Khatan the Howler, master of the flaming bow.",
        "weapon": weapon("Single", speed=5000, life=1.5, gravity=0.3),
        "secondary": None,  # The King's Blade: stance switch
        "ability1": None,  # Tactician: illusions (hard)
        "ability2": None,  # Garid: homing eagle
        "f": None,  # Trusty Steed: mount
        "ult": ("HowlOfMongolia", "ZL_Kit_GroundAbility", merge(cd(100), {
            "ability_name": "Howl of Mongolia", "max_placement_range": 2500,
            "zone": {"duration": 6, "radius": 700, "interval": 0.15, "strike_radius": 180, "color": (1, 0.3, 0.1),
                     "enemy_payload": {"damage": 30, "dot_per_second": 8, "dot_duration": 2}}})),
        "passives": [("CruelWarmonger", "ZL_Genghis_CruelWarmonger", {
            "ability_name": "Cruel Warmonger", "max_stacks": 10,
            "stats_per_stack": [("WeaponDamage", 5), ("SpiritPower", 5)]})],
    },
    # ------------------------------------------------------------------------------------------------------------------
    "Ashura": {
        "display": "Ashura", "color": (0.8, 0.0, 0.3),
        "desc": "The slumbering core that resets the earth, playing with dice and fire.",
        "weapon": weapon("Single", speed=8000, life=0.8),
        "secondary": None,  # Let the Dice Roll: dice system
        "ability1": None,  # Mad Hounds: chasing summons
        "ability2": ("Fissure", "ZL_Kit_Ability", merge(cd(14), {
            "ability_name": "Fissure", "hit_delay": 0.3, "area": {"shape": "Line", "range": 1400, "angle": 120},
            "enemy_payload": {"damage": 90, "dot_per_second": 15, "dot_duration": 3}})),
        "f": None,  # Curse of Mexico: survive at 1 HP
        "ult": ("DeepCrimson", "ZL_Kit_Ability", merge(cd(100), {
            "ability_name": "Deep Crimson", "pulse_count": 4, "pulse_interval": 0.6,
            "area": {"shape": "Sphere", "range": 700, "require_line_of_sight": False},
            "enemy_payload": merge({"damage": 50, "dot_per_second": 10, "dot_duration": 3}, stun(0.4))})),
        "passives": [("Monarch", "ZL_Ashura_Monarch", {
            "ability_name": "Monarch", "kills_for_monarch": 5,
            "blaze_payload": {"dot_per_second": 15, "dot_duration": 8}})],
    },
    # ------------------------------------------------------------------------------------------------------------------
    "Ryu": {
        "display": "Ryu", "color": (0.2, 0.7, 1.0),
        "desc": "The heavenly beast, master of the four symbols.",
        "weapon": weapon("Shotgun", speed=6000, life=0.1, pellets=3, pellet_spread=10),
        "secondary": ("BlackTurtle", "ZL_Kit_DashAbility", merge(cd(14), {
            "ability_name": "Black Turtle", "dash_distance": 900, "dash_duration": 0.35,
            "self_payload": {"barrier": 150, "barrier_duration": 4},
            "hit_payload": {"damage": 40, "knockback_speed": 1400, "knock_up_speed": 300}})),
        "ability1": None,  # Vermillion Bird: scrambled controls
        "ability2": ("WhiteTiger", "ZL_Kit_Ability", merge(cd(20), {
            "ability_name": "White Tiger", "has_area_hit": False, "active_duration": 6,
            "self_payload": {"stats": [("FireRatePercent", 60)], "stat_duration": 6},
            "while_active_hit_chance": 15, "while_active_hit_payload": stun(0.5)})),
        "f": ("HeavenlyArtDisruption", "ZL_Kit_TargetAbility", merge(cd(16), {
            "ability_name": "Heavenly Art: Disruption", "range": 1800, "target_team": "Enemies",
            "is_movement_ability": True, "move": "SwapPlaces"})),
        "ult": ("AzureDragon", "ZL_Kit_ProjectileAbility", merge(cd(120), {
            "ability_name": "Azure Dragon", "ability_description": "Sheet: always a one-hit KO (ExecuteBelowHealthPercent 100). Balance risk.",
            "fire_delay": 1.0, "activation_owned_tags": ["ZeroLock.Movement.Rooted"],
            "projectile": proj(2800, 2.0, radius=70, color=(0.2, 0.5, 1)),
            "hit": {"direct_payload": {"damage": 100, "execute_below_health_percent": 100}}})),
        "passives": [("BeastAwakens", "ZL_Ryu_BeastAwakens", {"ability_name": "The Beast Awakens"})],
    },
    # ------------------------------------------------------------------------------------------------------------------
    "DrOoze": {
        "display": "Dr. Ooze", "color": (0.4, 1.0, 0.3),
        "desc": "Field medic with gummy bears, supplements and an ambulance on speed dial.",
        "weapon": weapon("Single", speed=4500, life=1.0, gravity=0.3),
        "secondary": ("Supplements", "ZL_Kit_ProjectileAbility", merge(cd(10), {
            "ability_name": "Supplements",
            "projectile": proj(2500, 1.5, gravity=0.5, radius=40, color=(0.9, 0.9, 1), hit_team="AlliesNotSelf"),
            "hit": {"direct_payload": {"heal": 40, "cleanse": True}, "area_radius": 150,
                    "area_ally_payload": {"heal": 40, "cleanse": True}}})),
        "ability1": ("ButterFingers", "ZL_Kit_GroundAbility", merge(cd(16), {
            "ability_name": "Butter Fingers", "max_placement_range": 1500,
            "zone": {"duration": 6, "radius": 300, "interval": 0.5, "color": (0.4, 1, 0.3),
                     "enemy_payload": {"damage": 12}, "ally_payload": {"heal": 12}}})),
        "ability2": None,  # Jello Monster: damage-intercepting blocker
        "f": None,  # Emergency Getaway: vehicle
        "ult": ("BotoxShot", "ZL_Kit_TargetAbility", merge(cd(90), {
            "ability_name": "Botox Shot", "range": 2500, "target_team": "Allies", "self_cast_on_double_tap": True,
            "move": "DashToTarget", "move_time": 0.35,
            "target_payload": {"cleanse": True, "stat_duration": 8,
                               "stats": [("WeaponDamage", 40), ("MoveSpeed", 150), ("WeaponResist", 30), ("SpiritResist", 30)]}})),
        "passives": [("Vaccinated", "ZL_Kit_Passive", {
            "ability_name": "Vaccinated", "permanent_tags": ["Zerolock.Status.Unstoppable"],
            "permanent_stats": [("DebuffResist", 50)]})],
    },
    # ------------------------------------------------------------------------------------------------------------------
    "Ryuga": {
        "display": "Ryuga", "color": (0.5, 0.0, 0.7),
        "desc": "WARNING: the sheet's Ryuga / L-Drago is Beyblade's character. Rename and redesign before shipping.",
        "weapon": weapon("Single", speed=3500, life=1.0),
        "secondary": None,  # Supreme Overload: homing
        "ability1": ("SoaringBiteStrike", "ZL_Kit_ProjectileAbility", merge(cd(14), {
            "ability_name": "Soaring Bite Strike",
            "projectile": proj(3000, 1.2, radius=50, color=(0.6, 0.1, 0.8), pierce=True),
            "hit": {"direct_payload": {"steal_barrier": True}}})),
        "ability2": ("DragonDescent", "ZL_Kit_TargetAbility", merge(cd(30), {
            "ability_name": "Dragon Descent", "range": 2500, "target_team": "Enemies",
            "pulse_count": 3, "pulse_interval": 0.8, "hit_delay": 0.5,
            "target_payload": {"cooldown_delay": 3}, "self_payload": {"cooldown_delay": -5}})),
        "f": None,  # Supreme Flight: mount
        "ult": None,  # Life Destructor: roaming AI dragon (hard)
        "passives": [],  # Dark Force: copy secondaries (hard)
    },
    # ------------------------------------------------------------------------------------------------------------------
    "Mephisto": {
        "display": "Mephisto Memphis", "color": (0.2, 0.1, 0.5),
        "desc": "Kin of space.",
        "weapon": weapon("Single", speed=6000, life=1.0),
        "secondary": None,  # Void: absorbs projectiles
        "ability1": ("Gravity", "ZL_Kit_Ability", merge(cd(18), {
            "ability_name": "Gravity", "forward_offset": 500, "area": {"shape": "Sphere", "range": 450},
            "enemy_payload": {"statuses": ["Root"], "status_duration": 1.5, "pull_to_origin": True, "pull_duration": 0.3}})),
        "ability2": ("SpatialRift", "ZL_Mephisto_SpatialRift", merge(cd(20), {
            "ability_name": "Spatial Rift", "range": 1800, "rift_window": 3, "rift_range": 2500})),
        "f": ("DistortedRealm", "ZL_Kit_Ability", merge(cd(20), {
            "ability_name": "Distorted Realm", "has_area_hit": False, "active_duration": 1.5,
            "self_payload": {"statuses": ["Invulnerable"], "status_duration": 1.5}})),
        "ult": ("Wormhole", "ZL_Kit_TargetAbility", merge(cd(100), {
            "ability_name": "Wormhole", "range": 2500, "target_team": "Enemies",
            "pulse_count": 8, "pulse_interval": 0.25, "hit_delay": 0.5, "target_payload": {"damage": 40}})),
        "passives": [("SpaceIsMyDomain", "ZL_Kit_Passive", {
            "ability_name": "Space Is My Domain", "permanent_tags": ["Zerolock.Status.IgnoreBarrier"]})],
    },
    # ------------------------------------------------------------------------------------------------------------------
    "Kronos": {
        "display": "Kronos", "color": (0.9, 0.8, 0.4),
        "desc": "The forbidden time.",
        "weapon": weapon("Single", speed=5000, life=1.0),
        "secondary": ("TimeWarp", "ZL_Kit_GroundAbility", merge(cd(18), {
            "ability_name": "Time Warp", "max_placement_range": 1500,
            "zone": {"duration": 6, "radius": 450, "interval": 0.25, "color": (0.9, 0.8, 0.4),
                     "enemy_payload": slow(30, 0.35),
                     "ally_payload": {"stats": [("MoveSpeed", 240)], "stat_duration": 0.35}}})),
        "ability1": None,  # Past Echo: decoy
        "ability2": ("PrisonerOfTime", "ZL_Kit_TargetAbility", merge(cd(20), {
            "ability_name": "Prisoner of Time", "range": 1500, "target_team": "Enemies", "target_payload": stun(2.0)})),
        "f": None,  # I Am Paradox: self rewind
        "ult": None,  # Rewritten Time: area rewind
        "passives": [("LoopingPast", "ZL_Kronos_LoopingPast", {"ability_name": "Looping Past"})],
    },
    # ------------------------------------------------------------------------------------------------------------------
    "WuKong": {
        "display": "WuKong", "color": (1.0, 0.75, 0.1),
        "desc": "Immortal ruler with the Ruyi Jingu Bang.",
        "weapon": weapon("Single", speed=3000, life=1.5, gravity=0.6),
        "secondary": ("RuyiBang", "ZL_Kit_Ability", merge(cd(10), {
            "ability_name": "Ruyi Bang", "hit_delay": 0.2, "area": {"shape": "Sphere", "range": 450},
            "enemy_payload": {"damage": 60, "knockback_speed": 1000, "knock_up_speed": 250}})),
        "ability1": ("RuyiBangUnleashed", "ZL_Kit_Ability", merge(cd(20), {
            "ability_name": "Ruyi Bang Unleashed", "hit_delay": 0.5, "area": {"shape": "Line", "range": 900, "angle": 100},
            "enemy_payload": merge({"damage": 150}, stun(1.0))})),
        "ability2": None,  # Flying Nimbus: steerable cloud with a passenger (hard)
        "f": None,  # Golden Hair: taunting clone
        "ult": ("ImmortalPeach", "ZL_Kit_Ability", merge(cd(120), {
            "ability_name": "Immortal Peach", "has_area_hit": False, "active_duration": 6,
            "self_payload": {"timed_tags": ["Zerolock.Status.Immortal", "Zerolock.Cheat.NoCooldowns"], "tag_duration": 6}})),
        "passives": [("Godspeed", "ZL_Kit_Passive", {
            "ability_name": "Godspeed",
            "procs": [{"trigger": "AbilityCast", "self_stats": [("MoveSpeed", 40)], "duration": 6, "max_stacks": 5}]})],
    },
}

SLOTS = {
    # sheet key: (hero BP property or None = not assigned, EGameplayAbilitySlot or None)
    "secondary": ("SecondryFireAbility", "AbilitySlot1"),
    "ability1": ("Ability_1", "AbilitySlot2"),
    "ability2": ("Ability_2", "AbilitySlot3"),
    "ult": ("UltimateAbility", "UltimateSlot"),
    "f": (None, None),
}
