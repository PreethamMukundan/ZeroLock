// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ZL_TelemetrySubsystem.generated.h"

class FJsonObject;
class APlayerState;
class AZeroLockCharacter;
class UGameplayAbility;
class UZero_Item_data;

DECLARE_LOG_CATEGORY_EXTERN(LogZLTelemetry, Log, All);

/**
 * Records gameplay stats and sends them in batches (HTTP POST, JSON) to the telemetry API in Tools/Telemetry,
 * plus a local JSON-lines copy in Saved/Telemetry/. See Docs/Claude/Telemetry.md.
 *
 * - Gameplay events (damage, deaths, abilities, items, hero picks, positions) are recorded on the server (host).
 * - Every instance, clients included, records its own performance (FPS, frame time, ping).
 * - Frequent stats (damage, healing, ability uses) are summed in memory and sent as one row per combination per flush,
 *   so a match writes a few thousand rows instead of one per bullet.
 */
UCLASS(Config=Game)
class ZEROLOCK_API UZL_TelemetrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UZL_TelemetrySubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Match (server) ---

	/** Starts a match and returns its id (the game mode copies it to the game state so clients tag their perf samples). */
	FString BeginMatch(UWorld* World, const FString& ModeName);
	void EndMatch(int32 RedScore, int32 BlueScore);
	bool IsInMatch() const { return !MatchId.IsEmpty(); }

	// --- Gameplay (server) ---

	void RecordPlayerJoin(APlayerState* PS);
	void RecordPlayerLeave(APlayerState* PS);
	void RecordHeroPick(APlayerState* PS, UClass* HeroClass, bool bInitialPick);
	void RecordDamage(AZeroLockCharacter* Source, AZeroLockCharacter* Target, const FString& SourceName, const FString& DamageType, float Amount, bool bKillingBlow);
	void RecordHeal(AZeroLockCharacter* Source, AZeroLockCharacter* Target, float Amount);
	void RecordAbilityUse(AZeroLockCharacter* Hero, const UGameplayAbility* Ability);
	void RecordDeath(AZeroLockCharacter* Victim, AZeroLockCharacter* Killer, int32 NumAssists);
	void RecordItem(AZeroLockCharacter* Hero, const UZero_Item_data* Item, bool bBought);

	/** Any other event (from BP too). Attributes go into the event's "extra" object. */
	UFUNCTION(BlueprintCallable, Category="Telemetry")
	void RecordCustomEvent(const FString& EventType, const TMap<FString, FString>& Attributes);

	/** Sends everything buffered now (also runs every FlushInterval and at match end). */
	UFUNCTION(BlueprintCallable, Category="Telemetry")
	void Flush();

	/** Readable names used in the data: class name without BP_ / Deadlock_ prefixes and the _C suffix. */
	static FString CleanClassName(const UClass* Class);

	// --- Config: [/Script/ZeroLock.ZL_TelemetrySubsystem] in DefaultGame.ini; IngestUrl/IngestKey from Telemetry.ini (gitignored) ---

	UPROPERTY(Config)
	bool bEnabled = true;

	/** API base URL, e.g. http://127.0.0.1:8787 (wrangler dev) or https://zerolock-telemetry.<account>.workers.dev. Empty = local file only. */
	UPROPERTY(Config)
	FString IngestUrl;

	/** Sent as "Authorization: Bearer <key>". Ships inside the game, so it only allows writing events. */
	UPROPERTY(Config)
	FString IngestKey;

	UPROPERTY(Config)
	bool bWriteLocalFile = true;

	UPROPERTY(Config)
	float FlushInterval = 10.f;

	UPROPERTY(Config)
	float PerfSampleInterval = 10.f;

	/** Server samples every hero's position this often (heatmaps). 0 = off. */
	UPROPERTY(Config)
	float PositionSampleInterval = 5.f;

	UPROPERTY(Config)
	int32 MaxEventsPerRequest = 500;

	/** Oldest events are dropped past this (API unreachable for a long time). */
	UPROPERTY(Config)
	int32 MaxBufferedEvents = 20000;

private:
	struct FDamageTotal
	{
		TSharedPtr<FJsonObject> Base;
		double Amount = 0.0;
		int32 Hits = 0;
		int32 Kills = 0;
	};

	struct FCountTotal
	{
		TSharedPtr<FJsonObject> Base;
		double Amount = 0.0;
		int32 Count = 0;
	};

	void LoadLocalOverrides();
	bool Tick(float DeltaTime);
	void SamplePerf();
	void SamplePositions();
	void FlushTotals();
	void SendBatch(TArray<TSharedPtr<FJsonObject>>&& Batch);
	void WriteLocal(const TArray<TSharedPtr<FJsonObject>>& Batch);

	UWorld* GetGameWorld() const;
	/** Server: MatchId. Clients: the id replicated on AZero_BaseGameState. */
	FString GetCurrentMatchId() const;
	double GetMatchTime() const;
	FString GetRole() const;

	TSharedRef<FJsonObject> MakeEvent(const FString& Type) const;
	void AddEvent(const TSharedRef<FJsonObject>& Event);
	/** Writes <Prefix>player, <Prefix>hero, <Prefix>team. */
	static void AddHero(const TSharedRef<FJsonObject>& Event, const AZeroLockCharacter* Hero, const FString& Prefix);
	static void AddLocation(const TSharedRef<FJsonObject>& Event, const FVector& Location, const FString& Prefix);
	static FString PlayerKey(const APlayerState* PS);
	static FString TeamName(const APlayerState* PS);

	FTSTicker::FDelegateHandle TickerHandle;

	/** Random id per game launch: groups one machine's perf samples. */
	FString SessionId;
	FString BuildVersion;
	FString LocalFilePath;

	FString MatchId;
	double MatchStartTime = 0.0;
	TWeakObjectPtr<UWorld> MatchWorld;

	TArray<TSharedPtr<FJsonObject>> Events;
	/** Sends that failed with a retryable error; resent with the next flush (not written to the local file again). */
	TArray<TSharedPtr<FJsonObject>> RetryEvents;
	TMap<FString, FDamageTotal> DamageTotals;
	TMap<FString, FCountTotal> HealTotals;
	TMap<FString, FCountTotal> AbilityTotals;

	/** Last damage source and damage type per victim (the killing blow), for the death event. */
	TMap<TWeakObjectPtr<AZeroLockCharacter>, TPair<FString, FString>> LastDamageSource;

	float FlushTimer = 0.f;
	float PerfTimer = 0.f;
	float PositionTimer = 0.f;

	int32 PerfFrames = 0;
	double PerfFrameTimeSum = 0.0;
	double PerfFrameTimeMax = 0.0;
	int32 PerfHitches = 0;

	int32 RequestsInFlight = 0;
	bool bLoggedHttpFailure = false;
};
