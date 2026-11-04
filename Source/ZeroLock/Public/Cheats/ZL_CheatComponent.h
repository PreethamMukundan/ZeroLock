// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZL_CheatComponent.generated.h"

class AZeroLockCharacter;
class UZL_Character_Data_Asset;

UENUM(BlueprintType)
enum class EZL_CheatToggle : uint8
{
	GodMode,
	NoClip,
	NoCooldowns,
	InfiniteAmmo,
};

/**
 * Dev cheats for one player, on AZero_BasePlayerController. Everything runs on the server through RPCs, so it works
 * for listen-server clients too. Rejected in Shipping builds and while the server's ZL.Cheat.Allow is 0.
 * Toggles are replicated flags that become loose Zerolock.Cheat.* tags on the current pawn's ASC (server and owning
 * client, re-applied after respawn / hero swap). UI: UZL_CheatMenu (key 0, AZL_HUDClass). Console: ZL.Cheat.*.
 */
UCLASS(ClassGroup = (Cheats), meta = (BlueprintSpawnableComponent))
class ZEROLOCK_API UZL_CheatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZL_CheatComponent();

	/** The cheat component of the local player (a networked client's in PIE, else the first local one). */
	static UZL_CheatComponent* FindLocal();

	/** Server-side check: not Shipping and ZL.Cheat.Allow != 0. */
	static bool AreCheatsAllowed();

	/** Heroes from the game instance's CharacterDataTable (DT_HeroList), loaded synchronously. */
	static TArray<UZL_Character_Data_Asset*> LoadHeroList(const UObject* WorldContext);

	UFUNCTION(BlueprintPure, Category = "Cheats")
	bool IsCheatOn(EZL_CheatToggle Cheat) const;

	UFUNCTION(BlueprintCallable, Category = "Cheats")
	void ToggleCheat(EZL_CheatToggle Cheat);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Cheats")
	void ServerSetCheat(EZL_CheatToggle Cheat, bool bOn);

	/** Full health, stamina, ammo and ability charges; clears ability cooldowns. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Cheats")
	void ServerRefill();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Cheats")
	void ServerAddSouls(int32 Amount);

	/** Sets the four slotted abilities (secondary, 1, 2, ultimate) to MaxAbilityLevel. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Cheats")
	void ServerMaxAbilityLevels();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Cheats")
	void ServerKillSelf();

	/** Local: trace from the camera and teleport to the surface under the crosshair. */
	UFUNCTION(BlueprintCallable, Category = "Cheats")
	void TeleportToCrosshair();

	UFUNCTION(Server, Reliable)
	void ServerTeleport(FVector Location);

	/** Spawns HeroClass (None = my current hero) DummySpawnDistance in front of me, driven by AZL_DummyController. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Cheats")
	void ServerSpawnDummy(TSubclassOf<AZeroLockCharacter> HeroClass, bool bEnemy, bool bImmortal, bool bStrafe);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Cheats")
	void ServerRemoveDummies();

	/** Hero swap that ignores the game mode's bAllowHeroSwap / pad rules (AZero_BaseGameModeBase::SwapHero). */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Cheats")
	void ServerPlayAsHero(TSubclassOf<AZeroLockCharacter> HeroClass);

	/** Options for the next spawned dummy (local; the menu and ZL.Cheat.Dummy read them). */
	UPROPERTY(BlueprintReadWrite, Category = "Cheats|Dummy")
	bool bDummyImmortal = true;

	UPROPERTY(BlueprintReadWrite, Category = "Cheats|Dummy")
	bool bDummyStrafe = false;

	UPROPERTY(EditDefaultsOnly, Category = "Cheats|Dummy")
	float DummySpawnDistance = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Cheats")
	int32 MaxAbilityLevel = 3;

	/** Local: multi-line mover + network state of this player's pawn (net mode, role, ping, bandwidth, loss, rollbacks,
	 *  mover mode, speed, stamina, jump/dash counters, status flags). The menu's Debug tab and the overlay show it. */
	FString BuildDebugReadout() const;

	/** Local: draw BuildDebugReadout on screen every frame, also with the menu closed. */
	UPROPERTY(BlueprintReadWrite, Category = "Cheats|Debug")
	bool bShowDebugOverlay = false;

	/** Local: last network emulation profile picked in the menu ("Off", "Average", "Bad", ...). */
	UPROPERTY(BlueprintReadOnly, Category = "Cheats|Debug")
	FString NetEmulationProfile = TEXT("Off");

	/** Local: NetEmulation.PktEmulationProfile <Profile> (or NetEmulation.Off) on this machine's net driver. */
	UFUNCTION(BlueprintCallable, Category = "Cheats|Debug")
	void SetNetEmulationProfile(const FString& Profile);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	/** Rollbacks per second, sampled once a second in TickComponent. */
	int32 RollbackSampleCount = 0;
	double RollbackSampleTime = 0.0;
	float RollbacksPerSecond = 0.f;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** One bit per EZL_CheatToggle. */
	UPROPERTY(ReplicatedUsing = OnRep_CheatFlags)
	uint8 CheatFlags = 0;

	UFUNCTION()
	void OnRep_CheatFlags();

	/** Sets the Zerolock.Cheat.* loose tags on the current pawn (server and owning client). Polled, so new pawns get them too. */
	void ApplyCheatTags();
	FTimerHandle ApplyTagsTimer;

	void ClearCooldowns(AZeroLockCharacter* Hero) const;

	AZeroLockCharacter* GetHero() const;
	APlayerController* GetPC() const;
	/** Server RPC guard: authority, cheats allowed (logs when not). */
	bool CanCheat(const TCHAR* What) const;
};
