# UI (CommonUI + MVVM)

Code: `Source/ZeroLock/{Public,Private}/UI/` (65 classes). Widget BPs: `Content/ZERO/UI/BP/`. Styles: `Content/ZERO/UI/CommonStyles` (`CBS_*`, `CS_*`).

## Layering
- `AZL_HUDClass` (`UI/HUD/ZL_HUDClass`): local controller only. BeginPlay creates `BaseUILayer` (`WBP_BaseUILayout`) as `RootLayer`, then virtual `PushHUD()` pushes `PlayerInGameHUD` onto `UI.Layer.Game`.
- `AZL_MainMenu_HUD_Class` overrides `PushHUD` → `UI.Layer.Menu`, shows cursor.
- `UZL_BaseUILayout` (root, `UCommonActivatableWidget`): four BindWidget stacks `GameStack`, `MenuStack`, `ItemShopStack`, `PopUpStack` mapped by `FUITag` in `NativeOnInitialized`.
  - `PushWidgetToLayer(Tag, Class)`: pushes only if the layer is empty or its active widget is a different class (no toggle/remove).
  - `GetActivatedWidgetInLayer(Tag)`.
  - Input config: `ECommonInputMode::All`, mouse `CapturePermanently`.
- Layer tags (native, `ZL_UI_Tags.cpp`): `UI.Layer.Game`, `UI.Layer.Itemshop`, `UI.Layer.Menu`, `UI.Layer.PopUp`.
- Item shop / keybinding screens are pushed from BP, not C++. No pause, scoreboard or lobby screens exist.

## MVVM
Two ways to attach a viewmodel:
1. **Resolvers** (widget BP Viewmodels panel, creation type Resolver):

| Resolver | Returns |
|---|---|
| `UZL_VMCR_Attributes` | owning pawn's `AZeroLockCharacter::GetVM_Attributes()` |
| `UZL_AbilitiesResolver` | `AbilityUIManager->GetAbilitiesViewModel()` |
| `UZL_AbilitesTimerResolver` (sic) | `AbilityUIManager->GetAbilityProgressionStack()` |
| `UZL_GameStateResolver` | `AZero_BaseGameState::GetGameStateVM()` |
| `UZL_VMCR_KeyBindingScreen` | new `UZL_VM_KeyBindingList` for the local player |

2. **Manual injection**: `SetViewModel(VM)` → `UMVVMSubsystem::GetViewFromUserWidget(this)->SetViewModel(FName("<VMName>"), VM)`. The FName must match the BP's viewmodel name: `ZL_VM_Attributes`, `ZL_VM_AbilityIcon`, `ZL_VM_ChargePercent`, `ZL_VM_AbilityTimerProgressBar`, `ZL_VM_PlayerInfo`, `ZL_VM_KeyBindingRow`, `Zl_CharacterSelectionVM`. Used for list entries, player boxes, world-space widgets, tooltips.

VM property pattern: `UPROPERTY(BlueprintReadOnly, FieldNotify, Setter, Getter)` + `UE_MVVM_SET_PROPERTY_VALUE`; derived values are `UFUNCTION(BlueprintPure, FieldNotify)` re-broadcast from setters.

### Who owns which VM
| VM | Owner | Fed by |
|---|---|---|
| `UZL_VM_Attributes` (Health, MaxHealth, Ammo, MaxAmmo, IsInfiniteAmmo, VM_ChargePhase; `GetHealthPercentage`, `GetFinalAmmoText`) | `AZeroLockCharacter` (`CreateVM_Att`), mirrored to `AZero_BasePlayerState::CurrentVM` | GAS attribute callbacks `HealthAttributeChanged`, `AmmoAttributeChange` in `ZeroLockCharacter.cpp` |
| `UZL_VM_AbilitiesContainer` (4 `UZL_VM_AbilityIcon` slots), `UZL_VM_ProgressionStack` | `UZL_AbilityUIManagerComponent` on the character | ASC events (below) and `BaseGameplayAbility` timers |
| `UZL_VM_GameState` (AllyTeam, EnemyTeam, FormattedGameTime), `UZL_VM_PlayerInfo` per player | `AZero_BaseGameState` | game state |
| `UZl_CharacterSelectionVM` | `UZL_CharacterSelectionSubsystem` (game instance) | hero select |
| `UZL_VM_ChargePercent` | set by `BaseGameplayAbility` into `VM_Attributes->VM_ChargePhase` | charged abilities (perfect window) |

### `UZL_AbilityUIManagerComponent`
- BeginPlay: creates 4 slots (Secondary, Ability1, Ability2, Ultimate) keyed by input tags `ZerolockGameplayTagsForBinding::TAG_INPUT_SECONDRY/ABILITY_1/ABILITY_2/ULTIMATE`. Locally controlled: binds ASC `OnAbilityUpgraded`, `OnNewAbilityAdded`, `OnActiveGameplayEffectAddedDelegateToSelf`, then replays existing specs.
- `OnAbilityAdded`: slot found by spec `DynamicSpecSourceTags`; fills icon, name, descriptions (base + L1–3), charges (`AbilityCharges_N`/`MaxCharges_N` by `EGameplayAbilitySlot`), cooldown tags, `StackTag`.
- Cooldowns: 0.033 s looping `RefreshCooldowns` timer while any cooldown tag is active.
- Upgrades: click HUD icon → `IncrementAbilityLevel` → `Server_UpgradeAbility(Class, Level)` → server sets `Spec.Level`, `MarkAbilitySpecDirty`.
- Ability timers: `BaseGameplayAbility` calls `AddProgressBarVM` / `SetProgressionLevel` / `RemoveProgressBarVM`.

## Data paths
- Health/ammo: GAS → character → `UZL_VM_Attributes` → BP bindings. Overhead bar for remote characters via `InitializeFloatingStatusBar` → `UZL_OverHeadDisplay`.
- Stat panels (`UZL_WeaponStat`, `UZL_SpiritStat`, `UZL_VitalityStat` with `UZL_Stat_Box`): **not MVVM**, bind `ASC->GetGameplayAttributeValueChangeDelegate` directly on `GetPlayerPawn(0)` at init.
- K/D/A: `UZL_KDA_Bar` ← `AZero_BasePlayerState::OnKills/Deaths/AssistsChanged` (binds immediately on authority, else via `AZero_BasePlayerController::OnPSInit`).
- Timer: `UZL_HUD_GameTimer` ← `AZero_BaseGameState::OnGameTimeUpdated`.
- Team bar: `UZL_GameState_Bar` (`OnAllyTeamChanged/OnEnemyTeamChanged`) → `UZL_PlayerInfoBox`. `UZL_HUD_TeamList_View` is the old, effectively dead version.
- Damage numbers: attribute set → client RPC `AZero_BasePlayerController::ShowDamageNumber` → `AZeroLockCharacter::AddDamageNumber` → `UZL_BaseDamageWidgetComponent` → `UZL_DamagePopUpBase` (1 s accumulate; BP event `AddindividualDamageNumber`, TypeID 0 spirit / 1 weapon / 2 other).
- Crosshair: `UZL_HUD_PlayerBase::CF_ChargePhaseInit(VM)` swaps gun crosshair ↔ `UZL_AbilityChargePhase` in `CrosshairSwitcher`.

## Screens
- **Main menu**: `UZL_MainMenu` (Login → Main via `MainSwitcher`), `UZL_LoginPage` (BP wires EOS login), `UZL_MainScreen` (Play/Settings/Quit). `ShowLobbyScreen`/`ShowSettingsScreen` empty; lobby classes are stubs.
- **Hero select**: `UZL_HeroSelectionScreen` (tile view of `UZL_Character_Data_Asset`; hover sets current character, mouse move rotates the 3D preview `AZL_CharacterSelector_Actor`; click handler empty). `UZL_CharacterIcon` tiles.
- **Item shop**: `UZL_ItemShop_Base` (async-loads `DT_ItemList` rows `FItemTableRow{TSoftObjectPtr<UZero_Item_data>}`; All/Weapon/Spirit/Vitality views; `ItemPressed` → `ServerBuyItem`/`ServerSellItem`), `UZL_ItemShopCategory` (4 tier tile views), `UZL_ITemIcon` (`EItemState`: Default, **Sold = owned**, ReadyToUpgrade, Blocked), `UZL_ItemTooltipWidget`, `UZL_Hero_HUD_Inventory` (2-row slots from `ItemDelegate`).
- **Keybinds**: `UKeybindManagerSubsystem` (LocalPlayer, `Input/`) → `UZL_VM_KeyBindingList` → `UZL_KeyBindingScreen` / `UZL_KeyBindingingRow` (sic).
- `UImageDownloader`: dev-only HTTP image fetcher (shop images from `Content/Scripts/response.json`) living in the runtime module.

## Conventions
- Prefix `ZL_` (`UZL_*` widgets/VMs, `AZL_*` HUDs); game framework uses `Zero_`. VMs `UZL_VM_<Name>`; resolvers named inconsistently (`UZL_VMCR_*` / `UZL_*Resolver`).
- Almost all widgets derive from `UCommonActivatableWidget`; buttons `UCommonButtonBase`; children via `meta=(BindWidget)` (`BTN_*`, `B_*`).
- Assets: `WBP_` widgets, `CBS_` border styles, `CS_` text styles, `DT_` tables.
- Renaming a `UCLASS` needs a CoreRedirect (BPs reference them). Known typo names: `ZL_KeyBindingingRow`, `ZL_AbilitesTimerResolver`, `ZL_ITemIcon`, `AddindividualDamageNumber`, `LoginResultRecieved`, `TAG_INPUT_SECONDRY`.

## Known issues
- `ZL_AbilityUIManagerComponent.cpp:43,84` deref `Find`/`FindKey` results unchecked (crash on missing slot). `:123-125`, `:288` missing null checks.
- **Server trusts client ability upgrades**: `Server_UpgradeAbility_Validate` returns true; no level cap or skill-point check server-side (`:70-74`, cap only client-side `:38`).
- `UZL_VM_AbilityIcon::GetChargePercent` (`ZL_VM_AbilityIcon.h:108`) integer division and /0; `UZL_VM_Attributes::GetHealthPercentage` (`ZL_VM_Attributes.h:28`) /0 before attributes arrive.
- `Zero_BaseGameState.cpp:164-168` writes `UZL_VM_PlayerInfo` fields directly (no FieldNotify), Assists never set, and `UZL_PlayerInfoBox` snapshots once → team-bar K/D/A likely doesn't update live.
- Widgets grabbing `GetPlayerPawn(0)` once at init (stat panels, inventory, shop) never rebind on respawn/hero swap; ASC delegates never removed; `UZL_ItemShop_Base::ItemPressed:171` crashes without a pawn.
- Shop buy/sell state machine duplicated in `UZL_ItemShopCategory::OnItemSelected` and `UZL_ItemShop_Base::ItemPressed`; buying from a category tab depends on the All list having a generated entry (tile views virtualize).
- `UZL_HUD_AbilityIcon::NativeOnMouseButtonUp` levels up on any mouse release.
- `UZL_KDA_Bar` on pure clients may never bind if the PlayerState replicated before the widget existed.
- `UZL_OverHeadWidgetComponent` ticks every frame per character to billboard.
- `UZL_BaseDamageWidgetComponent` ctor `WidgetClass = MyWidgetClass` is a no-op; set `WidgetClass` in BP. `DamageTotalAmount` int truncates.
- `UZL_HUD_PlayerBase::ZL_AbilityChargePhase` raw non-UPROPERTY pointer; `CF_ChargePhaseInit` doesn't remove the previous charge widget.
- `ImageDownloader::SanitizeFilename` (`ImageDownloader.cpp:32`) reads past a single TCHAR.
- `Config/DefaultEngine.ini:43` `GameViewportClientClassName` value has `[/Script/Engine.GameEngine]` glued on, so the CommonUI viewport client may not be applied. Verify in editor.
- Empty / stub classes: `UZL_HUD_Weapon`, `UMyZL_BasePlayerHealthBar`, `UZL_BaseProgressBar`, `UZL_PlayerStatsBar`, `UZL_HeroStatScreen`, `UZL_ItemInventory`, `UZL_KeyBindingCategory`, 4 lobby classes.
