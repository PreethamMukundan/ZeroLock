//Copyright Preetham Mukundan (C) 2026


#include "Gamemode/Zero_BaseGameModeBase.h"

#include "Zero_BasePlayerController.h"
#include "Zero_BasePlayerState.h"
#include "Gamemode/Zero_BaseGameState.h"
#include "Gamemode/Zero_BasePlayerStart.h"
#include "GAS/BaseCharAttributeSet.h"
#include "Kismet/GameplayStatics.h"
#include "Telemetry/ZL_TelemetrySubsystem.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZero_BaseGameModeBase::AZero_BaseGameModeBase()
{
    PlayerControllerClass = AZero_BasePlayerController::StaticClass();
    PlayerStateClass = AZero_BasePlayerState::StaticClass();
    GameStateClass = AZero_BaseGameState::StaticClass();
    bUseSeamlessTravel =true;

    // Optionally set DefaultPawnClass if you want a fallback, but your SpawnDefaultPawnFor_Implementation handles this.
}

void AZero_BaseGameModeBase::BeginPlay()
{
    Super::BeginPlay();
    ZLOG_COLOR_TIME("Gamemode Begin play", FColor::Red,10);
    AZero_BaseGameState* GS = GetGameState<AZero_BaseGameState>();
    if (!GS) return;

    if (UZL_TelemetrySubsystem* Telemetry = UZL_TelemetrySubsystem::Get(this))
    {
        GS->TelemetryMatchId = Telemetry->BeginMatch(GetWorld(), UZL_TelemetrySubsystem::CleanClassName(GetClass()));
        // Players who logged in and spawned before BeginPlay (e.g. the host).
        for (APlayerState* PS : GS->PlayerArray)
        {
            Telemetry->RecordPlayerJoin(PS);
            if (const APawn* Pawn = PS ? PS->GetPawn() : nullptr)
            {
                Telemetry->RecordHeroPick(PS, Pawn->GetClass(), true);
            }
        }
    }

    // Bind with parameter
    FTimerDelegate TimerDel;
    TimerDel.BindUFunction(this, FName("UpdateGameTime"), GS);

    // Run every 1 second
    GetWorldTimerManager().SetTimer(GameTimerHandle, TimerDel, 1.0f, true);
}

void AZero_BaseGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UZL_TelemetrySubsystem* Telemetry = UZL_TelemetrySubsystem::Get(this))
    {
        const AZero_BaseGameState* GS = GetGameState<AZero_BaseGameState>();
        Telemetry->EndMatch(GS ? GS->TeamRedScore : 0, GS ? GS->TeamBlueScore : 0);
    }
    Super::EndPlay(EndPlayReason);
}

void AZero_BaseGameModeBase::Logout(AController* Exiting)
{
    if (UZL_TelemetrySubsystem* Telemetry = UZL_TelemetrySubsystem::Get(this))
    {
        Telemetry->RecordPlayerLeave(Exiting ? Exiting->PlayerState : nullptr);
    }
    Super::Logout(Exiting);
}

void AZero_BaseGameModeBase::UpdateGameTime(AZero_BaseGameState* GS)
{
   if (!GS) return;
    GS->IncrementGameTime();
}

/** Called when a player logs in (server-side) */
void AZero_BaseGameModeBase::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (!NewPlayer)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GM] PostLogin: NewPlayer is null"));
        return;
    }

    AZero_BasePlayerState* PS = NewPlayer->GetPlayerState<AZero_BasePlayerState>();
    if (PS)
    {
        UE_LOG(LogTemp, Log, TEXT("[GM] PostLogin: Assigning team for %s"), *NewPlayer->GetName());
        AssignTeam(PS);

        // Debug server-side current team counts
        int32 TeamRedCount = 0;
        int32 TeamBlueCount = 0;
        for (APlayerState* Player : GameState->PlayerArray)
        {
            if (AZero_BasePlayerState* ZPS = Cast<AZero_BasePlayerState>(Player))
            {
                if (ZPS->TeamID == ETeamID::TeamRed) TeamRedCount++;
                else if (ZPS->TeamID ==ETeamID::TeamBlue) TeamBlueCount++;
            }
        }
        UE_LOG(LogTemp, Log, TEXT("[GM] PostLogin: Team counts -> Red: %d, Blue: %d"), TeamRedCount, TeamBlueCount);

        if (UZL_TelemetrySubsystem* Telemetry = UZL_TelemetrySubsystem::Get(this))
        {
            Telemetry->RecordPlayerJoin(PS);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[GM] PostLogin: PlayerState was null or not AZero_BasePlayerState for %s"), *NewPlayer->GetName());
    }
}

/** Return a spawn point that matches the player's team if available */
AActor* AZero_BaseGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
    if (!Player)
    {
        return Super::ChoosePlayerStart_Implementation(Player);
    }

    AZero_BasePlayerState* PS = Player->GetPlayerState<AZero_BasePlayerState>();
    if (!PS)
    {
        // Fallback to default behavior if PlayerState isn't available yet
        UE_LOG(LogTemp, Warning, TEXT("[GM] ChoosePlayerStart: PlayerState not available for controller %s"), *Player->GetName());
        return Super::ChoosePlayerStart_Implementation(Player);
    }

    // Collect all team-specific starts
    TArray<AActor*> AllStarts;
    UGameplayStatics::GetAllActorsOfClass(this, AZero_BasePlayerStart::StaticClass(), AllStarts);

    TArray<AZero_BasePlayerStart*> TeamStarts;
    for (AActor* A : AllStarts)
    {
        if (AZero_BasePlayerStart* TS = Cast<AZero_BasePlayerStart>(A))
        {
            if (TS->TeamID == PS->TeamID)
                TeamStarts.Add(TS);
        }
    }

    if (TeamStarts.Num() > 0)
    {
        int32 Index = FMath::RandRange(0, TeamStarts.Num() - 1);
        AZeroLockCharacter* Character = Cast<AZeroLockCharacter>(Player->GetPawn());
        if (Character)
        {
            Character->StartLocation = TeamStarts[Index]->GetActorLocation();
            
        }
        if (AZero_BasePlayerController* myPC = Cast<AZero_BasePlayerController>(Player))
        {
            myPC->ServerSetStartLocation(TeamStarts[Index]->GetActorLocation());
        }
        return TeamStarts[Index];
    }

    return Super::ChoosePlayerStart_Implementation(Player);
}


APawn* AZero_BaseGameModeBase::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
    AZero_BasePlayerController* PC = Cast<AZero_BasePlayerController>(NewPlayer);

    TSubclassOf<AZeroLockCharacter> PawnToSpawn = nullptr;

    if (PC && PC->SelectedHeroClass)
    {
        PawnToSpawn = PC->SelectedHeroClass;
    }
    else
    {
        PawnToSpawn = DefaultHeroClass;
    }

    APawn* NewPawn = SpawnPawnDefault(PawnToSpawn, NewPlayer, StartSpot);
    if (UZL_TelemetrySubsystem* Telemetry = UZL_TelemetrySubsystem::Get(this))
    {
        if (NewPawn && NewPlayer)
        {
            Telemetry->RecordHeroPick(NewPlayer->PlayerState, NewPawn->GetClass(), true);
        }
    }
    return NewPawn;
}

/** Handle death: update stats and schedule respawn via controller (safe) */
void AZero_BaseGameModeBase::HandlePlayerDeath(AZeroLockCharacter* DeadPawn, AController* DeadController)
{
    if (!DeadController)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GM] HandlePlayerDeath: DeadController is null"));
        return;
    }

    if (!DeadPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GM] HandlePlayerDeath: DeadPawn is null for controller %s"), *DeadController->GetName());
        // Still attempt to respawn the controller
    }

    // Before anything else (e.g. Doorman's hotel sends its guest back to the map).
    if (DeadPawn)
    {
        DeadPawn->OnServerPreDeath.Broadcast(DeadPawn);
    }

    // Update deaths on server
    AZero_BasePlayerState* DeadPS = DeadController->GetPlayerState<AZero_BasePlayerState>();
    if (DeadPS)
    {
        DeadPS->AddDeath();
        UE_LOG(LogTemp, Log, TEXT("[GM] HandlePlayerDeath: Incremented death for %s (Team %d)"), *DeadController->GetName(), DeadPS->TeamID);
    }

    // Kill credit (use LastHitCharacter from pawn if valid)
    if (DeadPawn && DeadPawn->LastHitCharacter)
    {
        AController* KillerController = DeadPawn->LastHitCharacter->GetController();
        if (KillerController)
        {
            AZero_BasePlayerState* KillerPS = KillerController->GetPlayerState<AZero_BasePlayerState>();
            if (KillerPS)
            {
                KillerPS->AddKill();
                UE_LOG(LogTemp, Log, TEXT("[GM] HandlePlayerDeath: %s got a kill (Team %d)"), *KillerController->GetName(), KillerPS->TeamID);

                if (AZero_BaseGameState* GS = GetGameState<AZero_BaseGameState>())
                {
                    GS->AddKill(KillerPS->TeamID);
                }
            }
        }
    }

    // Assist credits
    if (DeadPawn)
    {
        int32 NumAssists = 0;
        for (AZeroLockCharacter* AssistPawn : DeadPawn->AssistListCharacters)
        {
            if (!AssistPawn) continue;
            if (AssistPawn == DeadPawn->LastHitCharacter) continue;

            if (AController* AssistController = AssistPawn->GetController())
            {
                if (AZero_BasePlayerState* AssistPS = AssistController->GetPlayerState<AZero_BasePlayerState>())
                {
                    AssistPS->AddAssist();
                    NumAssists++;
                    UE_LOG(LogTemp, Log, TEXT("[GM] HandlePlayerDeath: %s got an assist (Team %d)"), *AssistController->GetName(), AssistPS->TeamID);
                }
            }
        }

        if (UZL_TelemetrySubsystem* Telemetry = UZL_TelemetrySubsystem::Get(this))
        {
            Telemetry->RecordDeath(DeadPawn, DeadPawn->LastHitCharacter, NumAssists);
        }

        // Trigger the pawn's death functionality (visuals, ragdoll, etc.)
        DeadPawn->Death();
    }

    // Schedule respawn using the controller (safe even if pawn is destroyed)
    FTimerHandle RespawnTimer;
    FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &AZero_BaseGameModeBase::RespawnPlayer, DeadController);
    GetWorldTimerManager().SetTimer(RespawnTimer, Del, 3.f, false);
}

/** Respawn the player by restarting them (handles choose start & pawn creation) */
void AZero_BaseGameModeBase::RespawnPlayer(AController* Controller)
{
    if (!Controller)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GM] RespawnPlayer: Controller is null"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[GM] RespawnPlayer: Restarting player %s"), *Controller->GetName());

    AZeroLockCharacter* RespawnCharacter = Cast<AZeroLockCharacter>(Controller->GetPawn());
    if (RespawnCharacter)
    {
        RespawnCharacter->ResetCharacter();
    }
}

void AZero_BaseGameModeBase::InitGameState()
{
    Super::InitGameState();

    if (AZero_BaseGameState* GS = GetGameState<AZero_BaseGameState>())
    {
        GS->bAllowHeroSwap = bAllowHeroSwap;
        GS->bPickHeroOnJoin = bPickHeroOnJoin;
    }
}

bool AZero_BaseGameModeBase::IsWaitingForHeroPick(const AController* Player) const
{
    const AZero_BasePlayerController* PC = Cast<AZero_BasePlayerController>(Player);
    return bPickHeroOnJoin && PC && !PC->GetPawn() && !PC->SelectedHeroClass;
}

bool AZero_BaseGameModeBase::PlayerCanRestart_Implementation(APlayerController* Player)
{
    // No pawn until the hero is picked (SwapHero restarts them).
    if (IsWaitingForHeroPick(Player)) return false;
    return Super::PlayerCanRestart_Implementation(Player);
}

void AZero_BaseGameModeBase::GenericPlayerInitialization(AController* C)
{
    // Runs for PostLogin and seamless travel; a controller kept across travel would otherwise skip the pick with its old hero.
    if (bPickHeroOnJoin)
    {
        if (AZero_BasePlayerController* PC = Cast<AZero_BasePlayerController>(C))
        {
            PC->SelectedHeroClass = nullptr;
        }
    }
    Super::GenericPlayerInitialization(C);
}

void AZero_BaseGameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);

    // Waiting for the hero pick: look out from a team start meanwhile. Player starts are map actors, so clients can resolve them;
    // possessing the picked hero switches the view target to it (AutoManageActiveCameraTarget).
    if (IsWaitingForHeroPick(NewPlayer))
    {
        if (AActor* Start = FindPlayerStart(NewPlayer))
        {
            NewPlayer->ClientSetViewTarget(Start);
        }
    }
}

void AZero_BaseGameModeBase::SwapHero(AZero_BasePlayerController* PC, TSubclassOf<AZeroLockCharacter> HeroClass)
{
    if (!PC || !HeroClass) return;

    const bool bInitialPick = IsWaitingForHeroPick(PC);
    PC->SelectedHeroClass = HeroClass;

    if (bInitialPick)
    {
        // Normal spawn at a team start; SpawnDefaultPawnFor uses SelectedHeroClass.
        RestartPlayer(PC);
        UE_LOG(LogTemp, Log, TEXT("[GM] SwapHero: %s picked %s"), *PC->GetName(), *GetNameSafe(HeroClass));
        return;
    }

    APawn* OldPawn = PC->GetPawn();
    if (!OldPawn || OldPawn->GetClass() == HeroClass) return;

    const FVector Loc = OldPawn->GetActorLocation();
    const FRotator Rot(0.f, PC->GetControlRotation().Yaw, 0.f);
    const AZeroLockCharacter* OldHero = Cast<AZeroLockCharacter>(OldPawn);
    const FVector OldStartLocation = OldHero ? OldHero->StartLocation : Loc;

    PC->UnPossess();
    OldPawn->Destroy();

    FActorSpawnParameters Params;
    Params.Owner = PC;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    Params.ObjectFlags |= RF_Transient;

    AZeroLockCharacter* NewHero = GetWorld()->SpawnActor<AZeroLockCharacter>(HeroClass, Loc, Rot, Params);
    if (!NewHero)
    {
        UE_LOG(LogTemp, Error, TEXT("[GM] SwapHero: Failed to spawn %s for %s"), *GetNameSafe(HeroClass), *PC->GetName());
        RestartPlayer(PC);
        return;
    }

    // Keep the team respawn point, not the pad.
    NewHero->StartLocation = OldStartLocation;
    PC->Possess(NewHero);
    PC->SetControlRotation(Rot);

    if (UZL_TelemetrySubsystem* Telemetry = UZL_TelemetrySubsystem::Get(this))
    {
        Telemetry->RecordHeroPick(PC->PlayerState, HeroClass, false);
    }

    UE_LOG(LogTemp, Log, TEXT("[GM] SwapHero: %s is now %s"), *PC->GetName(), *GetNameSafe(HeroClass));
}

/** Assign teams by counting current players on each team (server-only) */
void AZero_BaseGameModeBase::AssignTeam(class AZero_BasePlayerState* PS)
{
    if (!PS)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GM] AssignTeam: PlayerState is null"));
        return;
    }

    // Count existing players per team (server-side)
    int32 TeamRedCount = 0;
    int32 TeamBlueCount = 0;
    AZero_BaseGameState* GS = GetGameState<AZero_BaseGameState>();
    if (!GS) return;
    
    for (APlayerState* Player : GS->PlayerArray)
    {
        if (AZero_BasePlayerState* TeamPlayerPS = Cast<AZero_BasePlayerState>(Player))
        {
            if (TeamPlayerPS->TeamID == ETeamID::TeamRed)
            {
                TeamRedCount++;
            }
            else if (TeamPlayerPS->TeamID == ETeamID::TeamBlue)
            {
                TeamBlueCount++;
            }
        }
    }

    // Assign to the smaller team
    const ETeamID NewTeam = (TeamRedCount <= TeamBlueCount) ? ETeamID::TeamRed : ETeamID::TeamBlue;
    PS->SetTeamID(NewTeam);
    GS->AddPSToTeamArray(PS, NewTeam);

}

/** Spawn the given pawn class at the location with the specified owner */
APawn* AZero_BaseGameModeBase::SpawnPawnDefault(TSubclassOf<AZeroLockCharacter> PawnClass, AController* Controller, AActor* StartSpot)
{
    if (!PawnClass) return nullptr;

    FActorSpawnParameters Params;
    Params.Owner = Controller;
    Params.Instigator = Controller ? Controller->GetPawn() : nullptr;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    FVector Loc = StartSpot ? StartSpot->GetActorLocation() : FVector::ZeroVector;
    FRotator Rot = StartSpot ? StartSpot->GetActorRotation() : FRotator::ZeroRotator;

    AZeroLockCharacter* NewPawn = GetWorld()->SpawnActor<AZeroLockCharacter>(PawnClass, Loc, Rot, Params);
    if (!NewPawn)
    {
        UE_LOG(LogTemp, Error, TEXT("[GM] SpawnPawnDefault: Failed to spawn pawn for controller %s"), Controller ? *Controller->GetName() : TEXT("Unknown"));
    }
    if (AZero_BasePlayerController* PC = Cast<AZero_BasePlayerController>(Controller))
    {
       
        PC->ServerSetStartLocation(Loc);
    }
    NewPawn->StartLocation = Loc;
    return NewPawn;
}

void AZero_BaseGameModeBase::Killed(AController* Killer, AController* Victim)
{

}
