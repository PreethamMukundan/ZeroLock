# Game flow: game mode, teams, hero select, EOS online

Paths under `Source/ZeroLock/` unless stated.

## Game mode: `AZero_BaseGameModeBase` (`Gamemode/`)
- `AGameModeBase` (no MatchState/warmup/rounds). `bUseSeamlessTravel=true`. TDM BP: `Content/ZERO/BP/GameMode/BP_TDMGameMOde`.
- `BeginPlay`: non-editor builds only (`#if !WITH_EDITOR`) → `UZL_EOS_SubSystem::Login()`. 1 s looping `UpdateGameTime` → `GS->IncrementGameTime()` (count-up, no limit).
- `PostLogin` → `AssignTeam` (smaller team, Red wins ties) → `PS->SetTeamID`, `GS->AddPSToTeamArray`.
- `ChoosePlayerStart` (`:93`): random `AZero_BasePlayerStart` with matching `TeamID`; writes `Character->StartLocation`, `PC->ServerSetStartLocation`.
- `SpawnDefaultPawnFor` (`:142`): `PC->SelectedHeroClass` or `DefaultHeroClass`.
- Death: `AZeroLockCharacter::HandleDeath` (`ZeroLockCharacter.cpp:513`) → server RPC `ServerHandleDeath(PC)` → `GM->HandlePlayerDeath` (`:161`): victim death, kill to `DeadPawn->LastHitCharacter` + `GS->AddKill(team)`, assists from `AssistListCharacters` (`AssistWindow` 5 declared, unused), `DeadPawn->Death()`, **3 s hard-coded** respawn timer → `RespawnPlayer` → `ResetCharacter()` on the **same pawn** (never destroyed/respawned).
- No win condition, score limit or match end; `Killed()` empty.
- `GlobalDefaultGameMode` in `DefaultEngine.ini` is the template `ZeroLockGameMode` (only `InitGlobalData`, BP_ThirdPersonCharacter). The real GM is set per map.

## Game state: `AZero_BaseGameState`
- Replicated: `TeamRedScore`/`TeamBlueScore` (no OnRep), `TeamRedArray`/`TeamBlueArray` (OnRep broadcasts only `.Last()`), `myGameTime` (OnRep formats MM:SS into `UZL_VM_GameState`).
- `RefreshTeamLists`/`ProcessTeamUpdate`: one `UZL_VM_PlayerInfo` per PS, split ally/enemy relative to the first local PC's PS.
- `ETeamID { TeamNull=0, TeamBlue, TeamRed }` (`ZeroLock.h:46`). `AZero_BasePlayerStart` adds `TeamID`.
- `UZero_BaseGameInstance` (BP `BP_ZeroGameInstance`): holds `CharacterDataTable`.

## Hero select
- Data: `FHeroTableRow{TSoftObjectPtr<UZL_Character_Data_Asset>}`; asset has CharacterName, Description, `CharacterClass`, DisplaySeletalMesh (sic), DisplayAnimation, Icon, IconColor (`CharacterSelector/ZL_Character_Data_Asset.h`). Table `Content/ZERO/UI/BP/CharacterSelector/DT_HeroList`.
- `UZL_CharacterSelectionSubsystem` (GameInstance) async-loads rows → `UZl_CharacterSelectionVM` (CoreRedirect for the lowercase l in `DefaultEngine.ini:78`).
- `AZL_CharacterSelector_Actor`: preview mesh + SceneCapture2D; on change sets mesh/anim and ability icons from the CDO (`SecondryFireAbility`, `Ability_1`, `Ability_2`, `UltimateAbility` `IconImage`); mouse X rotates the spring arm.
- **Server path is not wired**: `AZero_BasePlayerController::ServerSetSelectedHero` → replicated `SelectedHeroClass` → `SpawnDefaultPawnFor`. Nothing calls it, it isn't BlueprintCallable, no content references it, so everyone spawns `DefaultHeroClass`. The pawn is spawned at login, so a selection must arrive before the first `RestartPlayer`.

## EOS / online
Config (`Config/DefaultEngine.ini`): `DefaultPlatformService=EOS`, EAS + Connect + RTC, `bUseEOSSessions`, overlays. Net driver `NetDriverEOS` (P2P sockets) with IpNetDriver fallback; AESGCM + EOS packet handlers. `MAXPOSITIONERRORSQUARED=100`. `GameDefaultMap=/Game/ThirdPerson/Maps/ThirdPersonMap`.
**EOS client credentials/artifacts are committed in `DefaultEngine.ini` (~lines 62, 67-68). Never copy them into docs, logs or chat.**

`P2PMODE=1` (`ZeroLock.Build.cs` PrivateDefinitions) gates the lobby path in `UZL_EOS_SubSystem`. With 0 it falls back to the dedicated-server tutorial path (Browse to hard-coded `127.0.0.1:7777`) and **doesn't compile** (`UpdateLobbyConnectionString` uses `LobbyName` outside the `#if`).

Live flow, `UZL_EOS_SubSystem` (GameInstance subsystem; packaged builds only, PIE uses normal networking):
1. GM BeginPlay → `Login()` (`-AUTH_TYPE=` → AutoLogin, else AccountPortal).
2. `HandleLoginCompleted` → `FindSessions("Zerolock","Preetham")` (lobby search, max 20).
3. None found → `CreateLobby` (2 public connections, presence, lobby voice, session name `"LobbyName"`) → `GetWorld()->Listen(URL)` (current world becomes listen server) → `SetupNotifications`. Found → join first result → resolved connect string → `ClientTravel` over EOS P2P.
4. Host calls `ServerTravelAfterPlayersJoin()` (BlueprintCallable) → `ServerTravel("/Game/ThirdPerson/Maps/ThirdPersonMap?listen")`. `UpdateLobbyConnectionString()` re-publishes the connect string.

Tutorial / dev-only: `AZL_EOS_GameSession` (dedicated server, used only by `Developers/rpm86/EOSTEST/BP_EOSTESTGM`), `AZL_EOS_PlayerController` (used only by `BP_EOSPLayerController` in EOSTEST), `AZL_EOSGameSession` (**unreferenced, dead**), `UZL_TelemetrySubsystem` (empty).

## Known issues
- Unvalidated server RPCs: `ServerSetSelectedHero` (any class), `ServerSetStartLocation` (any vector) (`Zero_BasePlayerController.cpp:10,20`), **`ServerHandleDeath` lets a client trigger death/kill credit/respawn** (`ZeroLockCharacter.h:256`, `.cpp:533`).
- `SpawnPawnDefault` derefs `NewPawn` on failure (`Zero_BaseGameModeBase.cpp:309`); `Instigator` is the old pawn (`:293`).
- Seamless travel: teams assigned only in `PostLogin` (no `HandleSeamlessTravelPlayer` override); no `Logout` override → stale team array entries. Team arrays can replicate before PS `TeamID` (`Zero_BaseGameState.cpp:149`).
- `AddKill` has no authority guard (`Zero_BaseGameState.cpp:12`).
- Character table loaded twice (`ZL_CharacterSelectionSubsystem.cpp:20-26` and `Zero_BaseGameInstance.cpp:24-26`). Null deref if `CharacterClass` unset (`ZL_CharacterSelector_Actor.cpp:106`). Frame-rate dependent `REase` (`:86`).
- EOS: `SessionToJoin` points at a loop copy (dangling, uninitialised) (`ZL_EOS_SubSystem.cpp:230-251`); participant delegates never removed (`:433-460`); joiners use `"SessionName"` vs host `"LobbyName"` (`:104`, `:343`); lobby size 2 hard-coded (`:369`); Listen URL map path wrong (`:403`). `EndSession` failure clears the wrong delegate handle (`ZL_EOS_GameSession.cpp:223`). Memory bugs in `ZL_EOS_PlayerController.cpp:315, 345-366`.
- `Config/DefaultEngine.ini:43` malformed (`GameViewportClientClassName=...CommonGameViewportClient[/Script/Engine.GameEngine]`): CommonUI viewport client and the `NetDriverDefinitions` section placement may not apply as intended. Verify in editor.
