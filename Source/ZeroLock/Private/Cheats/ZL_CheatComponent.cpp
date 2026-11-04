// Copyright Preetham Mukundan (C) 2026

#include "Cheats/ZL_CheatComponent.h"

#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Cheats/ZL_DummyController.h"
#include "CharacterSelector/ZL_Character_Data_Asset.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Mover/ZeroMovementData.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/BaseCharAttributeSet.h"
#include "GAS/BaseGameplayAbility.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/ZL_GE_BaseCooldown.h"
#include "Gamemode/Zero_BaseGameInstance.h"
#include "Gamemode/Zero_BaseGameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Mover/ZeroMoverComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "ZeroLock/ZeroLockCharacter.h"
#include "Zero_BasePlayerController.h"
#include "Zero_BasePlayerState.h"

static TAutoConsoleVariable<int32> CVarAllowCheats(TEXT("ZL.Cheat.Allow"), 1,
	TEXT("Server: accept cheat requests from players (UZL_CheatComponent). Always off in Shipping."));

namespace ZLCheats
{
	static FGameplayTag ToTag(EZL_CheatToggle Cheat)
	{
		switch (Cheat)
		{
		case EZL_CheatToggle::GodMode:      return ZerolockGameplayTagsForBinding::TAG_CHEAT_GODMODE;
		case EZL_CheatToggle::NoClip:       return ZerolockGameplayTagsForBinding::TAG_CHEAT_NOCLIP;
		case EZL_CheatToggle::NoCooldowns:  return ZerolockGameplayTagsForBinding::TAG_CHEAT_NO_COOLDOWNS;
		case EZL_CheatToggle::InfiniteAmmo: return ZerolockGameplayTagsForBinding::TAG_CHEAT_INFINITE_AMMO;
		}
		return FGameplayTag();
	}

	static constexpr EZL_CheatToggle AllToggles[] = { EZL_CheatToggle::GodMode, EZL_CheatToggle::NoClip, EZL_CheatToggle::NoCooldowns, EZL_CheatToggle::InfiniteAmmo };
}

UZL_CheatComponent::UZL_CheatComponent()
{
	// Ticks only on the local player (BeginPlay), for the debug readout.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

UZL_CheatComponent* UZL_CheatComponent::FindLocal()
{
	UZL_CheatComponent* Fallback = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer)
		{
			continue;
		}
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			UZL_CheatComponent* Cheats = PC && PC->IsLocalController() ? PC->FindComponentByClass<UZL_CheatComponent>() : nullptr;
			if (Cheats)
			{
				if (World->GetNetMode() == NM_Client)
				{
					return Cheats;
				}
				Fallback = Fallback ? Fallback : Cheats;
			}
		}
	}
	return Fallback;
}

bool UZL_CheatComponent::AreCheatsAllowed()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return CVarAllowCheats.GetValueOnGameThread() != 0;
#endif
}

TArray<UZL_Character_Data_Asset*> UZL_CheatComponent::LoadHeroList(const UObject* WorldContext)
{
	TArray<UZL_Character_Data_Asset*> Heroes;
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UZero_BaseGameInstance* GI = World ? World->GetGameInstance<UZero_BaseGameInstance>() : nullptr;
	if (!GI || !GI->CharacterDataTable)
	{
		return Heroes;
	}
	TArray<FHeroTableRow*> Rows;
	GI->CharacterDataTable->GetAllRows<FHeroTableRow>(TEXT("ZL_Cheats"), Rows);
	for (const FHeroTableRow* Row : Rows)
	{
		UZL_Character_Data_Asset* Hero = Row ? Row->HeroAsset.LoadSynchronous() : nullptr;
		if (Hero && Hero->CharacterClass)
		{
			Heroes.Add(Hero);
		}
	}
	return Heroes;
}

void UZL_CheatComponent::BeginPlay()
{
	Super::BeginPlay();

	// Tags are only needed where the pawn is simulated with them: the server and the owning client.
	const APlayerController* PC = GetPC();
	if (PC && (GetOwner()->HasAuthority() || PC->IsLocalController()))
	{
		GetWorld()->GetTimerManager().SetTimer(ApplyTagsTimer, this, &UZL_CheatComponent::ApplyCheatTags, 0.25f, true);
	}
#if !UE_BUILD_SHIPPING
	SetComponentTickEnabled(PC && PC->IsLocalController());
#endif
}

void UZL_CheatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AZeroLockCharacter* Hero = GetHero();
	const UZeroMoverComponent* Mover = Hero ? Hero->GetZeroMoverComponent() : nullptr;
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (Now - RollbackSampleTime >= 1.0)
	{
		const int32 Count = Mover ? Mover->GetRollbackCount() : 0;
		// A new pawn starts its count at 0.
		RollbacksPerSecond = Count >= RollbackSampleCount && RollbackSampleTime > 0.0 ? (Count - RollbackSampleCount) / (Now - RollbackSampleTime) : 0.f;
		RollbackSampleCount = Count;
		RollbackSampleTime = Now;
	}

	if (bShowDebugOverlay && GEngine)
	{
		// Keyed, so it replaces itself each frame instead of stacking.
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(reinterpret_cast<UPTRINT>(this)), 0.f, FColor(140, 255, 160), BuildDebugReadout());
	}
}

void UZL_CheatComponent::SetNetEmulationProfile(const FString& Profile)
{
	APlayerController* PC = GetPC();
	if (!PC)
	{
		return;
	}
	PC->ConsoleCommand(Profile == TEXT("Off") ? FString(TEXT("NetEmulation.Off")) : FString::Printf(TEXT("NetEmulation.PktEmulationProfile %s"), *Profile));
	NetEmulationProfile = Profile;
}

FString UZL_CheatComponent::BuildDebugReadout() const
{
	const APlayerController* PC = GetPC();
	const AZeroLockCharacter* Hero = GetHero();
	if (!PC)
	{
		return FString();
	}

	FString NetMode;
	switch (GetWorld()->GetNetMode())
	{
	case NM_Standalone:      NetMode = TEXT("Standalone"); break;
	case NM_ListenServer:    NetMode = TEXT("Listen server (host)"); break;
	case NM_DedicatedServer: NetMode = TEXT("Dedicated server"); break;
	case NM_Client:          NetMode = TEXT("Client"); break;
	default:                 NetMode = TEXT("?"); break;
	}

	FString Out = FString::Printf(TEXT("NET  %s | emulation %s"), *NetMode, *NetEmulationProfile);
	if (const APlayerState* PS = PC->GetPlayerState<APlayerState>())
	{
		Out += FString::Printf(TEXT(" | ping %.0f ms"), PS->GetPingInMilliseconds());
	}
	if (const UNetConnection* Connection = PC->GetNetConnection())
	{
		Out += FString::Printf(TEXT("\n     in %.1f KB/s  out %.1f KB/s | loss in %.1f%%  out %.1f%%"),
			Connection->InBytesPerSecond / 1024.f, Connection->OutBytesPerSecond / 1024.f,
			Connection->GetInLossPercentage().GetAvgLossPercentage() * 100.f, Connection->GetOutLossPercentage().GetAvgLossPercentage() * 100.f);
	}

	const UZeroMoverComponent* Mover = Hero ? Hero->GetZeroMoverComponent() : nullptr;
	if (!Mover)
	{
		return Out + TEXT("\nMOVER  no hero");
	}

	const TCHAR* Role = Hero->GetLocalRole() == ROLE_Authority ? TEXT("Authority")
		: Hero->GetLocalRole() == ROLE_AutonomousProxy ? TEXT("Autonomous proxy") : TEXT("Simulated proxy");
	Out += FString::Printf(TEXT("\n     role %s | rollbacks %d (%.1f/s)"), Role, Mover->GetRollbackCount(), RollbacksPerSecond);

	const FVector Velocity = Mover->GetVelocity();
	const FVector Location = Hero->GetActorLocation();
	Out += FString::Printf(TEXT("\nMOVER  mode %s | speed %.0f (flat %.0f, z %.0f)"),
		*Mover->GetMovementModeName().ToString(), Velocity.Size(), Velocity.Size2D(), Velocity.Z);
	Out += FString::Printf(TEXT("\n     pos %.0f %.0f %.0f"), Location.X, Location.Y, Location.Z);

	if (const FZeroMoverSimState* Sim = Mover->GetSyncState().SyncStateCollection.FindDataByType<FZeroMoverSimState>())
	{
		Out += FString::Printf(TEXT("\n     stamina %.2f / %.0f | air jumps %d | air dashes %d | dash left %.0f ms"),
			Sim->Stamina, Mover->GetMaxStamina(), Sim->AirJumpsUsed, Sim->AirDashesUsed, Sim->DashRemainingMs);
	}

	TArray<FString> Flags;
	if (Mover->IsRooted())    Flags.Add(TEXT("Rooted"));
	if (Mover->IsStunned())   Flags.Add(TEXT("Stunned"));
	if (Mover->IsCubed())     Flags.Add(TEXT("Cubed"));
	if (Mover->IsGooBall())   Flags.Add(TEXT("GooBall"));
	if (Mover->IsFloating())  Flags.Add(TEXT("Floating"));
	if (Mover->IsNoClip())    Flags.Add(TEXT("NoClip"));
	if (Hero->bIsDead)        Flags.Add(TEXT("Dead"));
	Out += FString::Printf(TEXT("\n     flags %s"), Flags.IsEmpty() ? TEXT("-") : *FString::Join(Flags, TEXT(", ")));
	return Out;
}

void UZL_CheatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UZL_CheatComponent, CheatFlags, COND_OwnerOnly);
}

void UZL_CheatComponent::OnRep_CheatFlags()
{
	ApplyCheatTags();
}

bool UZL_CheatComponent::IsCheatOn(EZL_CheatToggle Cheat) const
{
	return (CheatFlags & (1 << static_cast<uint8>(Cheat))) != 0;
}

void UZL_CheatComponent::ToggleCheat(EZL_CheatToggle Cheat)
{
	ServerSetCheat(Cheat, !IsCheatOn(Cheat));
}

void UZL_CheatComponent::ApplyCheatTags()
{
	AZeroLockCharacter* Hero = GetHero();
	UAbilitySystemComponent* ASC = Hero ? Hero->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}
	// Explicit counts, so repeating this every poll changes nothing.
	for (const EZL_CheatToggle Cheat : ZLCheats::AllToggles)
	{
		ASC->SetLooseGameplayTagCount(ZLCheats::ToTag(Cheat), IsCheatOn(Cheat) ? 1 : 0);
	}
}

AZeroLockCharacter* UZL_CheatComponent::GetHero() const
{
	const APlayerController* PC = GetPC();
	return PC ? Cast<AZeroLockCharacter>(PC->GetPawn()) : nullptr;
}

APlayerController* UZL_CheatComponent::GetPC() const
{
	return Cast<APlayerController>(GetOwner());
}

bool UZL_CheatComponent::CanCheat(const TCHAR* What) const
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}
	if (!AreCheatsAllowed())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheats] %s refused: cheats are off on this server (ZL.Cheat.Allow)"), What);
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[Cheats] %s: %s"), *GetNameSafe(GetOwner()), What);
	return true;
}

void UZL_CheatComponent::ServerSetCheat_Implementation(EZL_CheatToggle Cheat, bool bOn)
{
	if (!CanCheat(TEXT("SetCheat")))
	{
		return;
	}
	const uint8 Bit = 1 << static_cast<uint8>(Cheat);
	CheatFlags = bOn ? (CheatFlags | Bit) : (CheatFlags & ~Bit);
	ApplyCheatTags();

	if (bOn && Cheat == EZL_CheatToggle::NoCooldowns)
	{
		ClearCooldowns(GetHero());
	}
}

void UZL_CheatComponent::ClearCooldowns(AZeroLockCharacter* Hero) const
{
	UBaseCharAbilitySystemComponent* ASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr;
	UBaseCharAttributeSet* Attributes = Hero ? Hero->GetMyAttributeSet() : nullptr;
	if (!ASC || !Attributes)
	{
		return;
	}

	// Every granted ability's cooldown tags, plus the charge recharge timers.
	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(ZerolockGameplayTagsForBinding::TAG_CHARGE_RECHARGE_SLOT1);
	CooldownTags.AddTag(ZerolockGameplayTagsForBinding::TAG_CHARGE_RECHARGE_SLOT2);
	CooldownTags.AddTag(ZerolockGameplayTagsForBinding::TAG_CHARGE_RECHARGE_SLOT3);
	CooldownTags.AddTag(ZerolockGameplayTagsForBinding::TAG_CHARGE_RECHARGE_ULTIMATE);
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability)
		{
			if (const FGameplayTagContainer* Tags = Spec.Ability->GetCooldownTags())
			{
				CooldownTags.AppendTags(*Tags);
			}
		}
	}
	ASC->RemoveActiveEffectsWithGrantedTags(CooldownTags);
	ASC->RemoveActiveGameplayEffectBySourceEffect(UZL_GE_BaseCooldown::StaticClass(), nullptr);

	// Removing a recharge early doesn't give the charge back, so fill them.
	ASC->SetNumericAttributeBase(UBaseCharAttributeSet::GetAbilityCharges_1Attribute(), Attributes->GetMaxCharges_1());
	ASC->SetNumericAttributeBase(UBaseCharAttributeSet::GetAbilityCharges_2Attribute(), Attributes->GetMaxCharges_2());
	ASC->SetNumericAttributeBase(UBaseCharAttributeSet::GetAbilityCharges_3Attribute(), Attributes->GetMaxCharges_3());
	ASC->SetNumericAttributeBase(UBaseCharAttributeSet::GetAbilityCharges_4Attribute(), Attributes->GetMaxCharges_4());
}

void UZL_CheatComponent::ServerRefill_Implementation()
{
	AZeroLockCharacter* Hero = GetHero();
	if (!Hero || !CanCheat(TEXT("Refill")))
	{
		return;
	}
	UBaseCharAbilitySystemComponent* ASC = Hero->GetMyAbilitySystemComp();
	const UBaseCharAttributeSet* Attributes = Hero->GetMyAttributeSet();
	if (!ASC || !Attributes)
	{
		return;
	}
	ASC->SetNumericAttributeBase(UBaseCharAttributeSet::GetCurrentHealthAttribute(), Attributes->GetMaximumHealth());
	ASC->SetNumericAttributeBase(UBaseCharAttributeSet::GetCurrentAmmoAttribute(), Attributes->GetMaxAmmo());
	if (UZeroMoverComponent* Mover = Hero->GetZeroMoverComponent())
	{
		Mover->RequestStaminaRefill();
	}
	ClearCooldowns(Hero);
}

void UZL_CheatComponent::ServerAddSouls_Implementation(int32 Amount)
{
	AZeroLockCharacter* Hero = GetHero();
	UAbilitySystemComponent* ASC = Hero ? Hero->GetAbilitySystemComponent() : nullptr;
	if (!ASC || !CanCheat(TEXT("AddSouls")))
	{
		return;
	}
	const float Souls = ASC->GetNumericAttributeBase(UBaseCharAttributeSet::GetSoulAttribute());
	ASC->SetNumericAttributeBase(UBaseCharAttributeSet::GetSoulAttribute(), FMath::Max(Souls + Amount, 0.0f));
}

void UZL_CheatComponent::ServerMaxAbilityLevels_Implementation()
{
	AZeroLockCharacter* Hero = GetHero();
	UBaseCharAbilitySystemComponent* ASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr;
	if (!ASC || !CanCheat(TEXT("MaxAbilityLevels")))
	{
		return;
	}
	FGameplayTagContainer SlotTags;
	SlotTags.AddTag(ZerolockGameplayTagsForBinding::TAG_INPUT_SECONDRY);
	SlotTags.AddTag(ZerolockGameplayTagsForBinding::TAG_INPUT_ABILITY_1);
	SlotTags.AddTag(ZerolockGameplayTagsForBinding::TAG_INPUT_ABILITY_2);
	SlotTags.AddTag(ZerolockGameplayTagsForBinding::TAG_INPUT_ULTIMATE);

	TArray<UGameplayAbility*> ToUpgrade;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Level < MaxAbilityLevel && Spec.GetDynamicSpecSourceTags().HasAny(SlotTags))
		{
			ToUpgrade.Add(Spec.Ability);
		}
	}
	for (UGameplayAbility* Ability : ToUpgrade)
	{
		ASC->LevelUpAbility(Ability, MaxAbilityLevel);
	}
}

void UZL_CheatComponent::ServerKillSelf_Implementation()
{
	AZeroLockCharacter* Hero = GetHero();
	if (!Hero || Hero->bIsDead || !CanCheat(TEXT("KillSelf")))
	{
		return;
	}
	if (UAbilitySystemComponent* ASC = Hero->GetAbilitySystemComponent())
	{
		ASC->SetNumericAttributeBase(UBaseCharAttributeSet::GetCurrentHealthAttribute(), 0.0f);
	}
	Hero->HandleDeath();
}

void UZL_CheatComponent::TeleportToCrosshair()
{
	APlayerController* PC = GetPC();
	AZeroLockCharacter* Hero = GetHero();
	if (!PC || !Hero)
	{
		return;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZLCheatTeleport), false, Hero);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * 50000.f, ECC_Visibility, Params))
	{
		return;
	}
	// Off the surface and up by the capsule's half height, so a floor hit lands standing on it.
	ServerTeleport(Hit.ImpactPoint + Hit.ImpactNormal * 60.f + FVector(0.f, 0.f, 90.f));
}

void UZL_CheatComponent::ServerTeleport_Implementation(FVector Location)
{
	AZeroLockCharacter* Hero = GetHero();
	if (!Hero || !CanCheat(TEXT("Teleport")))
	{
		return;
	}
	Hero->ServerForceTeleport(Location, GetPC()->GetControlRotation());
}

void UZL_CheatComponent::ServerSpawnDummy_Implementation(TSubclassOf<AZeroLockCharacter> HeroClass, bool bEnemy, bool bImmortal, bool bStrafe)
{
	APlayerController* PC = GetPC();
	AZeroLockCharacter* Hero = GetHero();
	if (!PC || !CanCheat(TEXT("SpawnDummy")))
	{
		return;
	}
	if (!HeroClass)
	{
		HeroClass = Hero ? Hero->GetClass() : nullptr;
	}
	if (!HeroClass || HeroClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector Origin = Hero ? Hero->GetActorLocation() : ViewLocation;
	const FVector Desired = Origin + FRotator(0.f, ViewRotation.Yaw, 0.f).Vector() * DummySpawnDistance;

	// Stand it on the ground under that spot.
	const float HalfHeight = HeroClass->GetDefaultObject<APawn>()->GetDefaultHalfHeight();
	FVector SpawnLocation = Desired;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZLCheatDummy), false, Hero);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Desired + FVector(0.f, 0.f, 500.f), Desired - FVector(0.f, 0.f, 5000.f), ECC_Visibility, Params))
	{
		SpawnLocation = Hit.ImpactPoint + FVector(0.f, 0.f, HalfHeight + 5.f);
	}
	const FRotator Facing(0.f, (Origin - SpawnLocation).Rotation().Yaw, 0.f);
	const FTransform SpawnTransform(Facing, SpawnLocation);

	AZeroLockCharacter* Dummy = GetWorld()->SpawnActorDeferred<AZeroLockCharacter>(HeroClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Dummy)
	{
		return;
	}
	Dummy->AutoPossessAI = EAutoPossessAI::Disabled;
	Dummy->FinishSpawning(SpawnTransform);

	FActorSpawnParameters ControllerParams;
	ControllerParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZL_DummyController* DummyController = GetWorld()->SpawnActor<AZL_DummyController>(AZL_DummyController::StaticClass(), SpawnTransform, ControllerParams);
	if (!DummyController)
	{
		Dummy->Destroy();
		return;
	}
	DummyController->bImmortal = bImmortal;
	DummyController->bStrafe = bStrafe;

	// Enemy = the other team (Red when I have none), ally = mine. Damage needs different teams (UBaseCharAttributeSet).
	const AZero_BasePlayerState* MyPS = PC->GetPlayerState<AZero_BasePlayerState>();
	const ETeamID MyTeam = MyPS ? MyPS->TeamID : ETeamID::TeamNull;
	const ETeamID DummyTeam = bEnemy ? (MyTeam == ETeamID::TeamRed ? ETeamID::TeamBlue : ETeamID::TeamRed) : MyTeam;
	if (AZero_BasePlayerState* DummyPS = DummyController->GetPlayerState<AZero_BasePlayerState>())
	{
		DummyPS->SetTeamID(DummyTeam);
		DummyPS->SetPlayerName(FString::Printf(TEXT("Dummy %s"), *HeroClass->GetName().Replace(TEXT("BP_"), TEXT("")).Replace(TEXT("_C"), TEXT(""))));
	}
	DummyController->SetControlRotation(Facing);
	DummyController->Possess(Dummy);
}

void UZL_CheatComponent::ServerRemoveDummies_Implementation()
{
	if (!CanCheat(TEXT("RemoveDummies")))
	{
		return;
	}
	for (TActorIterator<AZL_DummyController> It(GetWorld()); It; ++It)
	{
		if (APawn* Pawn = It->GetPawn())
		{
			Pawn->Destroy();
		}
		It->Destroy();
	}
}

void UZL_CheatComponent::ServerPlayAsHero_Implementation(TSubclassOf<AZeroLockCharacter> HeroClass)
{
	AZero_BasePlayerController* PC = Cast<AZero_BasePlayerController>(GetPC());
	AZero_BaseGameModeBase* GM = GetWorld()->GetAuthGameMode<AZero_BaseGameModeBase>();
	if (!PC || !HeroClass || HeroClass->HasAnyClassFlags(CLASS_Abstract) || !CanCheat(TEXT("PlayAsHero")))
	{
		return;
	}
	if (!GM)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheats] PlayAsHero needs an AZero_BaseGameModeBase game mode"));
		return;
	}
	GM->SwapHero(PC, HeroClass);
}

#if !UE_BUILD_SHIPPING
namespace ZLCheatCommands
{
	static void Toggle(const TArray<FString>& Args, EZL_CheatToggle Cheat)
	{
		UZL_CheatComponent* Cheats = UZL_CheatComponent::FindLocal();
		if (!Cheats)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Cheats] no local player with a cheat component"));
			return;
		}
		// No argument toggles; 1/0 sets.
		Cheats->ServerSetCheat(Cheat, Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !Cheats->IsCheatOn(Cheat));
	}

	/** Hero class by display name or class name (part of it, any case); empty = None (my current hero). */
	static TSubclassOf<AZeroLockCharacter> FindHeroClass(const UZL_CheatComponent* Cheats, const FString& Name)
	{
		if (Name.IsEmpty())
		{
			return nullptr;
		}
		for (const UZL_Character_Data_Asset* Hero : UZL_CheatComponent::LoadHeroList(Cheats))
		{
			if (Hero->CharacterName.Contains(Name) || Hero->CharacterClass->GetName().Contains(Name))
			{
				return Hero->CharacterClass;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[Cheats] no hero matching '%s' in the hero table"), *Name);
		return nullptr;
	}

	static FAutoConsoleCommand GodCommand(TEXT("ZL.Cheat.God"), TEXT("ZL.Cheat.God [1|0]: toggle god mode (no health loss)."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Toggle, EZL_CheatToggle::GodMode));

	static FAutoConsoleCommand NoClipCommand(TEXT("ZL.Cheat.NoClip"), TEXT("ZL.Cheat.NoClip [1|0]: toggle noclip flight (jump = up, crouch = down)."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Toggle, EZL_CheatToggle::NoClip));

	static FAutoConsoleCommand NoCooldownsCommand(TEXT("ZL.Cheat.NoCooldowns"), TEXT("ZL.Cheat.NoCooldowns [1|0]: toggle no ability cooldowns or charge use."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Toggle, EZL_CheatToggle::NoCooldowns));

	static FAutoConsoleCommand InfiniteAmmoCommand(TEXT("ZL.Cheat.InfiniteAmmo"), TEXT("ZL.Cheat.InfiniteAmmo [1|0]: toggle infinite ammo."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Toggle, EZL_CheatToggle::InfiniteAmmo));

	static FAutoConsoleCommand RefillCommand(TEXT("ZL.Cheat.Refill"), TEXT("ZL.Cheat.Refill: full health, stamina, ammo, charges; clear cooldowns."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (UZL_CheatComponent* Cheats = UZL_CheatComponent::FindLocal()) Cheats->ServerRefill();
		}));

	static FAutoConsoleCommand SoulsCommand(TEXT("ZL.Cheat.Souls"), TEXT("ZL.Cheat.Souls [Amount=10000]: add souls."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (UZL_CheatComponent* Cheats = UZL_CheatComponent::FindLocal()) Cheats->ServerAddSouls(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 10000);
		}));

	static FAutoConsoleCommand MaxAbilitiesCommand(TEXT("ZL.Cheat.MaxAbilities"), TEXT("ZL.Cheat.MaxAbilities: max out ability levels."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (UZL_CheatComponent* Cheats = UZL_CheatComponent::FindLocal()) Cheats->ServerMaxAbilityLevels();
		}));

	static FAutoConsoleCommand TeleportCommand(TEXT("ZL.Cheat.Teleport"), TEXT("ZL.Cheat.Teleport: teleport to where the crosshair points."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (UZL_CheatComponent* Cheats = UZL_CheatComponent::FindLocal()) Cheats->TeleportToCrosshair();
		}));

	static FAutoConsoleCommand DummyCommand(TEXT("ZL.Cheat.Dummy"),
		TEXT("ZL.Cheat.Dummy [Hero|-] [enemy|ally] [strafe] [mortal]: spawn a target dummy in front of you (- = my hero)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			UZL_CheatComponent* Cheats = UZL_CheatComponent::FindLocal();
			if (!Cheats)
			{
				return;
			}
			const FString Name = Args.Num() > 0 && Args[0] != TEXT("-") ? Args[0] : FString();
			const bool bAlly = Args.ContainsByPredicate([](const FString& A) { return A.Equals(TEXT("ally"), ESearchCase::IgnoreCase); });
			const bool bStrafe = Args.ContainsByPredicate([](const FString& A) { return A.Equals(TEXT("strafe"), ESearchCase::IgnoreCase); });
			const bool bMortal = Args.ContainsByPredicate([](const FString& A) { return A.Equals(TEXT("mortal"), ESearchCase::IgnoreCase); });
			Cheats->ServerSpawnDummy(FindHeroClass(Cheats, Name), !bAlly, !bMortal, bStrafe);
		}));

	static FAutoConsoleCommand ClearDummiesCommand(TEXT("ZL.Cheat.ClearDummies"), TEXT("ZL.Cheat.ClearDummies: remove every target dummy."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (UZL_CheatComponent* Cheats = UZL_CheatComponent::FindLocal()) Cheats->ServerRemoveDummies();
		}));

	static FAutoConsoleCommand PlayAsCommand(TEXT("ZL.Cheat.PlayAs"), TEXT("ZL.Cheat.PlayAs <Hero>: swap to that hero anywhere."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			UZL_CheatComponent* Cheats = UZL_CheatComponent::FindLocal();
			if (Cheats && Args.Num() > 0)
			{
				if (TSubclassOf<AZeroLockCharacter> HeroClass = FindHeroClass(Cheats, Args[0]))
				{
					Cheats->ServerPlayAsHero(HeroClass);
				}
			}
		}));
}
#endif
