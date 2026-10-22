// Copyright Preetham Mukundan (C) 2026


#include "Telemetry/ZL_TelemetrySubsystem.h"

#include "Abilities/GameplayAbility.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "GAS/BaseCharAttributeSet.h"
#include "GameFramework/PlayerController.h"
#include "Gamemode/Zero_BaseGameState.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Items/Zero_Item_data.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Zero_BasePlayerState.h"
#include "ZeroLock/ZeroLockCharacter.h"

DEFINE_LOG_CATEGORY(LogZLTelemetry);

namespace
{
	/** Frames longer than this count as a hitch in the perf samples. */
	constexpr double HitchSeconds = 0.05;

	FString ToJsonLine(const TSharedRef<FJsonObject>& Object)
	{
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Object, Writer);
		return Out;
	}

	double Round1(double Value)
	{
		return FMath::RoundToDouble(Value * 10.0) / 10.0;
	}
}

UZL_TelemetrySubsystem* UZL_TelemetrySubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UZL_TelemetrySubsystem>() : nullptr;
}

void UZL_TelemetrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadLocalOverrides();

	SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12).ToLower();
	GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), BuildVersion, GGameIni);
	LocalFilePath = FPaths::ProjectSavedDir() / TEXT("Telemetry") / FString::Printf(TEXT("%s_%s.jsonl"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), *SessionId);

	if (!bEnabled)
	{
		UE_LOG(LogZLTelemetry, Log, TEXT("Telemetry disabled"));
		return;
	}

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UZL_TelemetrySubsystem::Tick));
	UE_LOG(LogZLTelemetry, Log, TEXT("Telemetry on. Target %s, API: %s, local file: %s"), *IngestTarget,
		IngestUrl.IsEmpty() ? TEXT("(none)") : *IngestUrl, bWriteLocalFile ? *LocalFilePath : TEXT("(off)"));
}

void UZL_TelemetrySubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	// Requests still in flight may be cut off at shutdown; the local file keeps everything.
	Flush();
	Super::Deinitialize();
}

void UZL_TelemetrySubsystem::LoadLocalOverrides()
{
	// The API address and key stay out of the public repo: Config/Telemetry.ini (editor) or Telemetry.ini next to
	// the packaged game's project folder, section [Telemetry]. Command line wins: -TelemetryUrl= -TelemetryKey= -NoTelemetry.
	const FString Candidates[] = { FPaths::ProjectConfigDir() / TEXT("Telemetry.ini"), FPaths::ProjectDir() / TEXT("Telemetry.ini") };
	for (const FString& Path : Candidates)
	{
		if (!FPaths::FileExists(Path)) continue;

		FConfigFile File;
		File.Read(Path);
		File.GetString(TEXT("Telemetry"), TEXT("IngestUrl"), IngestUrl);
		File.GetString(TEXT("Telemetry"), TEXT("IngestKey"), IngestKey);
		File.GetString(TEXT("Telemetry"), TEXT("LocalUrl"), LocalUrl);
		File.GetString(TEXT("Telemetry"), TEXT("LocalKey"), LocalKey);
		File.GetString(TEXT("Telemetry"), TEXT("Target"), IngestTarget);
		File.GetBool(TEXT("Telemetry"), TEXT("bEnabled"), bEnabled);
		break;
	}

	CloudUrl = IngestUrl;
	CloudKey = IngestKey;

	// Target: the last ZL.Telemetry.Target choice on this machine, else Telemetry.ini, else Local in the editor
	// (playtests stay off the live data) and Cloud in packaged builds (friends' builds report online).
	GConfig->GetString(TEXT("ZL.Telemetry"), TEXT("Target"), IngestTarget, GGameUserSettingsIni);
	if (IngestTarget.IsEmpty())
	{
		IngestTarget = GIsEditor ? TEXT("Local") : TEXT("Cloud");
	}

	if (FParse::Value(FCommandLine::Get(), TEXT("TelemetryUrl="), CloudUrl))
	{
		IngestTarget = TEXT("Cloud");
	}
	FParse::Value(FCommandLine::Get(), TEXT("TelemetryKey="), CloudKey);
	FParse::Value(FCommandLine::Get(), TEXT("TelemetryTarget="), IngestTarget);
	if (FParse::Param(FCommandLine::Get(), TEXT("NoTelemetry")))
	{
		bEnabled = false;
	}

	ApplyTarget();
}

void UZL_TelemetrySubsystem::ApplyTarget()
{
	if (IngestTarget.Equals(TEXT("Local"), ESearchCase::IgnoreCase))
	{
		IngestTarget = TEXT("Local");
		IngestUrl = LocalUrl;
		IngestKey = LocalKey;
	}
	else if (IngestTarget.Equals(TEXT("File"), ESearchCase::IgnoreCase))
	{
		IngestTarget = TEXT("File");
		IngestUrl.Reset();
		IngestKey.Reset();
	}
	else
	{
		IngestTarget = TEXT("Cloud");
		IngestUrl = CloudUrl;
		IngestKey = CloudKey;
	}

	// Unquoted ini values are cut at "//", which leaves just "http:". The value must be quoted in Telemetry.ini.
	if (!IngestUrl.IsEmpty() && !((IngestUrl.StartsWith(TEXT("http://")) || IngestUrl.StartsWith(TEXT("https://"))) && IngestUrl.Len() > 8))
	{
		UE_LOG(LogZLTelemetry, Error, TEXT("%s URL '%s' is not a full http(s) URL; quote it in Telemetry.ini: IngestUrl=\"https://...\". Local file only."), *IngestTarget, *IngestUrl);
		IngestUrl.Reset();
	}
}

void UZL_TelemetrySubsystem::SetTarget(const FString& NewTarget)
{
	// Buffered events belong to the old target: send them there first. Failed sends of the old target are dropped
	// (they are already in the local file).
	Flush();
	RetryEvents.Reset();
	bLoggedHttpFailure = false;
	TargetGeneration++;

	IngestTarget = NewTarget;
	ApplyTarget();
	UE_LOG(LogZLTelemetry, Display, TEXT("Telemetry target: %s (API: %s)"), *IngestTarget, IngestUrl.IsEmpty() ? TEXT("none, local file only") : *IngestUrl);
}

namespace
{
	FAutoConsoleCommand TelemetryTargetCommand(
		TEXT("ZL.Telemetry.Target"),
		TEXT("ZL.Telemetry.Target [Local|Cloud|File]: where telemetry is sent. Local = wrangler dev (http://127.0.0.1:8787), ")
		TEXT("Cloud = the live API in Telemetry.ini, File = Saved/Telemetry only. Saved per machine. No argument prints the current target."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.IsEmpty())
			{
				FString Saved;
				GConfig->GetString(TEXT("ZL.Telemetry"), TEXT("Target"), Saved, GGameUserSettingsIni);
				UE_LOG(LogZLTelemetry, Display, TEXT("Saved telemetry target: %s"), Saved.IsEmpty() ? TEXT("(default: Local in editor, Cloud packaged)") : *Saved);
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					if (const UZL_TelemetrySubsystem* Telemetry = Context.OwningGameInstance ? Context.OwningGameInstance->GetSubsystem<UZL_TelemetrySubsystem>() : nullptr)
					{
						UE_LOG(LogZLTelemetry, Display, TEXT("%s: %s"), *Context.OwningGameInstance->GetName(), *Telemetry->GetTarget());
					}
				}
				return;
			}

			const FString& NewTarget = Args[0];
			if (!NewTarget.Equals(TEXT("Local"), ESearchCase::IgnoreCase) && !NewTarget.Equals(TEXT("Cloud"), ESearchCase::IgnoreCase)
				&& !NewTarget.Equals(TEXT("File"), ESearchCase::IgnoreCase))
			{
				UE_LOG(LogZLTelemetry, Warning, TEXT("ZL.Telemetry.Target: expected Local, Cloud or File, got '%s'"), *NewTarget);
				return;
			}

			// Remembered for the next launch / PIE session, and applied to every running game instance (all PIE windows).
			GConfig->SetString(TEXT("ZL.Telemetry"), TEXT("Target"), *NewTarget, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
			int32 NumApplied = 0;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (UZL_TelemetrySubsystem* Telemetry = Context.OwningGameInstance ? Context.OwningGameInstance->GetSubsystem<UZL_TelemetrySubsystem>() : nullptr)
				{
					Telemetry->SetTarget(NewTarget);
					NumApplied++;
				}
			}
			UE_LOG(LogZLTelemetry, Display, TEXT("ZL.Telemetry.Target %s saved (%d running game instance(s) switched)"), *NewTarget, NumApplied);
		}));
}

bool UZL_TelemetrySubsystem::Tick(float DeltaTime)
{
	PerfFrames++;
	PerfFrameTimeSum += DeltaTime;
	PerfFrameTimeMax = FMath::Max(PerfFrameTimeMax, static_cast<double>(DeltaTime));
	if (DeltaTime > HitchSeconds) PerfHitches++;

	PerfTimer += DeltaTime;
	if (PerfTimer >= PerfSampleInterval)
	{
		SamplePerf();
		PerfTimer = 0.f;
	}

	if (PositionSampleInterval > 0.f)
	{
		PositionTimer += DeltaTime;
		if (PositionTimer >= PositionSampleInterval)
		{
			SamplePositions();
			PositionTimer = 0.f;
		}
	}

	FlushTimer += DeltaTime;
	if (FlushTimer >= FlushInterval)
	{
		Flush();
		FlushTimer = 0.f;
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Match

FString UZL_TelemetrySubsystem::BeginMatch(UWorld* World, const FString& ModeName)
{
	if (!bEnabled || !World) return FString();
	if (IsInMatch()) EndMatch(0, 0);

	MatchId = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	MatchStartTime = World->GetTimeSeconds();
	MatchWorld = World;

	const TSharedRef<FJsonObject> Event = MakeEvent(TEXT("match_start"));
	const TSharedRef<FJsonObject> Extra = MakeShared<FJsonObject>();
	Extra->SetStringField(TEXT("map"), UWorld::RemovePIEPrefix(World->GetMapName()));
	Extra->SetStringField(TEXT("mode"), ModeName);
	Event->SetObjectField(TEXT("extra"), Extra);
	AddEvent(Event);

	UE_LOG(LogZLTelemetry, Log, TEXT("Match %s started (%s)"), *MatchId, *ModeName);
	return MatchId;
}

void UZL_TelemetrySubsystem::EndMatch(int32 RedScore, int32 BlueScore)
{
	if (!IsInMatch()) return;

	const TSharedRef<FJsonObject> Event = MakeEvent(TEXT("match_end"));
	Event->SetNumberField(TEXT("value"), Round1(GetMatchTime()));
	const TSharedRef<FJsonObject> Extra = MakeShared<FJsonObject>();
	Extra->SetNumberField(TEXT("red_score"), RedScore);
	Extra->SetNumberField(TEXT("blue_score"), BlueScore);
	Extra->SetStringField(TEXT("winner"), RedScore == BlueScore ? TEXT("draw") : (RedScore > BlueScore ? TEXT("red") : TEXT("blue")));
	Event->SetObjectField(TEXT("extra"), Extra);
	AddEvent(Event);

	UE_LOG(LogZLTelemetry, Log, TEXT("Match %s ended (red %d, blue %d)"), *MatchId, RedScore, BlueScore);

	// Totals still carry this match's id.
	Flush();
	MatchId.Reset();
	MatchWorld.Reset();
	LastDamageSource.Reset();
}

// ---------------------------------------------------------------------------------------------------------------------
// Gameplay

void UZL_TelemetrySubsystem::RecordPlayerJoin(APlayerState* PS)
{
	if (!IsInMatch() || !PS) return;

	const TSharedRef<FJsonObject> Event = MakeEvent(TEXT("player_join"));
	Event->SetStringField(TEXT("player"), PlayerKey(PS));
	Event->SetStringField(TEXT("team"), TeamName(PS));
	const TSharedRef<FJsonObject> Extra = MakeShared<FJsonObject>();
	Extra->SetStringField(TEXT("name"), PS->GetPlayerName());
	Event->SetObjectField(TEXT("extra"), Extra);
	AddEvent(Event);
}

void UZL_TelemetrySubsystem::RecordPlayerLeave(APlayerState* PS)
{
	if (!IsInMatch() || !PS) return;

	const TSharedRef<FJsonObject> Event = MakeEvent(TEXT("player_leave"));
	Event->SetStringField(TEXT("player"), PlayerKey(PS));
	Event->SetStringField(TEXT("team"), TeamName(PS));
	AddEvent(Event);
}

void UZL_TelemetrySubsystem::RecordHeroPick(APlayerState* PS, UClass* HeroClass, bool bInitialPick)
{
	if (!IsInMatch() || !PS || !HeroClass) return;

	const TSharedRef<FJsonObject> Event = MakeEvent(TEXT("hero_pick"));
	Event->SetStringField(TEXT("player"), PlayerKey(PS));
	Event->SetStringField(TEXT("hero"), CleanClassName(HeroClass));
	Event->SetStringField(TEXT("team"), TeamName(PS));
	const TSharedRef<FJsonObject> Extra = MakeShared<FJsonObject>();
	Extra->SetBoolField(TEXT("initial"), bInitialPick);
	Event->SetObjectField(TEXT("extra"), Extra);
	AddEvent(Event);
}

void UZL_TelemetrySubsystem::RecordDamage(AZeroLockCharacter* Source, AZeroLockCharacter* Target, const FString& SourceName,
	const FString& DamageType, float Amount, bool bKillingBlow)
{
	if (!IsInMatch() || !Source || !Target || Amount <= 0.f) return;

	const TSharedRef<FJsonObject> Probe = MakeShared<FJsonObject>();
	AddHero(Probe, Source, TEXT(""));
	AddHero(Probe, Target, TEXT("target_"));
	Probe->SetStringField(TEXT("ability"), SourceName);
	Probe->SetStringField(TEXT("damage_type"), DamageType);

	// One total per attacker, victim and source.
	const FString Key = ToJsonLine(Probe);
	FDamageTotal& Total = DamageTotals.FindOrAdd(Key);
	if (!Total.Base) Total.Base = Probe;
	Total.Amount += Amount;
	Total.Hits++;
	if (bKillingBlow) Total.Kills++;

	LastDamageSource.Add(Target, TPair<FString, FString>(SourceName, DamageType));
}

void UZL_TelemetrySubsystem::RecordHeal(AZeroLockCharacter* Source, AZeroLockCharacter* Target, float Amount)
{
	if (!IsInMatch() || !Target || Amount <= 0.f) return;

	const TSharedRef<FJsonObject> Probe = MakeShared<FJsonObject>();
	AddHero(Probe, Source, TEXT(""));
	AddHero(Probe, Target, TEXT("target_"));

	FCountTotal& Total = HealTotals.FindOrAdd(ToJsonLine(Probe));
	if (!Total.Base) Total.Base = Probe;
	Total.Amount += Amount;
	Total.Count++;
}

void UZL_TelemetrySubsystem::RecordAbilityUse(AZeroLockCharacter* Hero, const UGameplayAbility* Ability)
{
	if (!IsInMatch() || !Hero || !Ability) return;

	const TSharedRef<FJsonObject> Probe = MakeShared<FJsonObject>();
	AddHero(Probe, Hero, TEXT(""));
	Probe->SetStringField(TEXT("ability"), CleanClassName(Ability->GetClass()));

	FCountTotal& Total = AbilityTotals.FindOrAdd(ToJsonLine(Probe));
	if (!Total.Base) Total.Base = Probe;
	Total.Count++;
}

void UZL_TelemetrySubsystem::RecordDeath(AZeroLockCharacter* Victim, AZeroLockCharacter* Killer, int32 NumAssists)
{
	if (!IsInMatch() || !Victim) return;

	// "player" is the killer (the one acting), "target" the victim, like damage rows. x/y/z = where the victim died.
	const TSharedRef<FJsonObject> Event = MakeEvent(TEXT("death"));
	AddHero(Event, Killer, TEXT(""));
	AddHero(Event, Victim, TEXT("target_"));
	if (const TPair<FString, FString>* KillingBlow = LastDamageSource.Find(Victim))
	{
		Event->SetStringField(TEXT("ability"), KillingBlow->Key);
		Event->SetStringField(TEXT("damage_type"), KillingBlow->Value);
	}
	Event->SetNumberField(TEXT("count"), NumAssists);
	AddLocation(Event, Victim->GetActorLocation(), TEXT(""));
	if (Killer)
	{
		const TSharedRef<FJsonObject> Extra = MakeShared<FJsonObject>();
		AddLocation(Extra, Killer->GetActorLocation(), TEXT("killer_"));
		Extra->SetNumberField(TEXT("distance"), FMath::RoundToInt(FVector::Dist(Killer->GetActorLocation(), Victim->GetActorLocation())));
		Event->SetObjectField(TEXT("extra"), Extra);
	}
	AddEvent(Event);

	LastDamageSource.Remove(Victim);
}

void UZL_TelemetrySubsystem::RecordItem(AZeroLockCharacter* Hero, const UZero_Item_data* Item, bool bBought)
{
	if (!IsInMatch() || !Hero || !Item) return;

	const TSharedRef<FJsonObject> Event = MakeEvent(bBought ? TEXT("item_buy") : TEXT("item_sell"));
	AddHero(Event, Hero, TEXT(""));
	Event->SetStringField(TEXT("item"), Item->ItemName.IsEmpty() ? Item->ItemID.ToString() : Item->ItemName);
	Event->SetNumberField(TEXT("value"), Item->Cost);
	AddEvent(Event);
}

void UZL_TelemetrySubsystem::RecordCustomEvent(const FString& EventType, const TMap<FString, FString>& Attributes)
{
	if (!bEnabled || EventType.IsEmpty()) return;

	const TSharedRef<FJsonObject> Event = MakeEvent(EventType);
	const TSharedRef<FJsonObject> Extra = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Pair : Attributes)
	{
		Extra->SetStringField(Pair.Key, Pair.Value);
	}
	Event->SetObjectField(TEXT("extra"), Extra);
	AddEvent(Event);
}

// ---------------------------------------------------------------------------------------------------------------------
// Sampling

void UZL_TelemetrySubsystem::SamplePerf()
{
	const UWorld* World = GetGameWorld();
	if (PerfFrames > 0 && World && World->GetNetMode() != NM_DedicatedServer && !GetCurrentMatchId().IsEmpty())
	{
		const double AvgFrame = PerfFrameTimeSum / PerfFrames;

		const TSharedRef<FJsonObject> Event = MakeEvent(TEXT("perf"));
		const APlayerController* PC = World->GetFirstPlayerController();
		AddHero(Event, PC ? Cast<AZeroLockCharacter>(PC->GetPawn()) : nullptr, TEXT(""));
		if (!Event->HasField(TEXT("player")) && PC && PC->PlayerState)
		{
			Event->SetStringField(TEXT("player"), PlayerKey(PC->PlayerState));
		}
		Event->SetNumberField(TEXT("value"), Round1(AvgFrame > 0.0 ? 1.0 / AvgFrame : 0.0));

		const TSharedRef<FJsonObject> Extra = MakeShared<FJsonObject>();
		Extra->SetNumberField(TEXT("frame_ms"), Round1(AvgFrame * 1000.0));
		Extra->SetNumberField(TEXT("frame_ms_max"), Round1(PerfFrameTimeMax * 1000.0));
		Extra->SetNumberField(TEXT("hitches"), PerfHitches);
		Extra->SetNumberField(TEXT("ping_ms"), PC && PC->PlayerState ? FMath::RoundToInt(PC->PlayerState->GetPingInMilliseconds()) : 0);
		Event->SetObjectField(TEXT("extra"), Extra);
		AddEvent(Event);
	}

	PerfFrames = 0;
	PerfFrameTimeSum = 0.0;
	PerfFrameTimeMax = 0.0;
	PerfHitches = 0;
}

void UZL_TelemetrySubsystem::SamplePositions()
{
	UWorld* World = GetGameWorld();
	if (!IsInMatch() || !World || World->GetNetMode() == NM_Client) return;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		AZeroLockCharacter* Hero = PC ? Cast<AZeroLockCharacter>(PC->GetPawn()) : nullptr;
		if (!Hero || !Hero->IsAlive()) continue;

		const TSharedRef<FJsonObject> Event = MakeEvent(TEXT("pos"));
		AddHero(Event, Hero, TEXT(""));
		AddLocation(Event, Hero->GetActorLocation(), TEXT(""));
		// Health and souls over time for the match timeline.
		if (const UAbilitySystemComponent* ASC = Hero->GetAbilitySystemComponent())
		{
			const TSharedRef<FJsonObject> Extra = MakeShared<FJsonObject>();
			Extra->SetNumberField(TEXT("hp"), FMath::RoundToInt(ASC->GetNumericAttribute(UBaseCharAttributeSet::GetCurrentHealthAttribute())));
			Extra->SetNumberField(TEXT("max_hp"), FMath::RoundToInt(ASC->GetNumericAttribute(UBaseCharAttributeSet::GetMaximumHealthAttribute())));
			Extra->SetNumberField(TEXT("souls"), FMath::RoundToInt(ASC->GetNumericAttribute(UBaseCharAttributeSet::GetSoulAttribute())));
			Event->SetObjectField(TEXT("extra"), Extra);
		}
		AddEvent(Event);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Sending

void UZL_TelemetrySubsystem::FlushTotals()
{
	auto Emit = [this](const FString& Type, const TSharedPtr<FJsonObject>& Base, double Amount, int32 Count) -> TSharedRef<FJsonObject>
	{
		const TSharedRef<FJsonObject> Event = MakeEvent(Type);
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Base->Values)
		{
			Event->SetField(Field.Key, Field.Value);
		}
		Event->SetNumberField(TEXT("value"), Round1(Amount));
		Event->SetNumberField(TEXT("count"), Count);
		AddEvent(Event);
		return Event;
	};

	for (const TPair<FString, FDamageTotal>& Pair : DamageTotals)
	{
		const TSharedRef<FJsonObject> Event = Emit(TEXT("damage"), Pair.Value.Base, Pair.Value.Amount, Pair.Value.Hits);
		if (Pair.Value.Kills > 0)
		{
			const TSharedRef<FJsonObject> Extra = MakeShared<FJsonObject>();
			Extra->SetNumberField(TEXT("kills"), Pair.Value.Kills);
			Event->SetObjectField(TEXT("extra"), Extra);
		}
	}
	for (const TPair<FString, FCountTotal>& Pair : HealTotals)
	{
		Emit(TEXT("heal"), Pair.Value.Base, Pair.Value.Amount, Pair.Value.Count);
	}
	for (const TPair<FString, FCountTotal>& Pair : AbilityTotals)
	{
		Emit(TEXT("ability_use"), Pair.Value.Base, Pair.Value.Count, Pair.Value.Count);
	}

	DamageTotals.Reset();
	HealTotals.Reset();
	AbilityTotals.Reset();
}

void UZL_TelemetrySubsystem::Flush()
{
	if (!bEnabled) return;

	FlushTotals();

	// Only new events go to the local file; failed sends wait in RetryEvents, which is already on disk.
	TArray<TSharedPtr<FJsonObject>> New = MoveTemp(Events);
	Events.Reset();
	if (bWriteLocalFile && !New.IsEmpty())
	{
		WriteLocal(New);
	}
	if (IngestUrl.IsEmpty()) return;

	TArray<TSharedPtr<FJsonObject>> All = MoveTemp(RetryEvents);
	RetryEvents.Reset();
	All.Append(MoveTemp(New));
	if (All.IsEmpty()) return;

	for (int32 Start = 0; Start < All.Num(); Start += MaxEventsPerRequest)
	{
		const int32 Num = FMath::Min(MaxEventsPerRequest, All.Num() - Start);
		SendBatch(TArray<TSharedPtr<FJsonObject>>(All.GetData() + Start, Num));
	}
}

void UZL_TelemetrySubsystem::SendBatch(TArray<TSharedPtr<FJsonObject>>&& Batch)
{
	// Body: {"session": "...", "build": "...", "role": "host", "events": [ {...}, {...} ]}
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("session"), SessionId);
	Body->SetStringField(TEXT("build"), BuildVersion);
	Body->SetStringField(TEXT("role"), GetRole());
	TArray<TSharedPtr<FJsonValue>> EventValues;
	EventValues.Reserve(Batch.Num());
	for (const TSharedPtr<FJsonObject>& Event : Batch)
	{
		EventValues.Add(MakeShared<FJsonValueObject>(Event));
	}
	Body->SetArrayField(TEXT("events"), EventValues);

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(IngestUrl / TEXT("v1/events"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + IngestKey);
	Request->SetContentAsString(ToJsonLine(Body));
	Request->SetTimeout(15.f);

	// Runs on the game thread when the server answers (or the request fails). The lambda keeps the batch for a retry.
	Request->OnProcessRequestComplete().BindWeakLambda(this, [this, Batch = MoveTemp(Batch), Generation = TargetGeneration](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		RequestsInFlight--;
		if (Generation != TargetGeneration) return;
		const int32 Code = Response ? Response->GetResponseCode() : 0;
		if (bConnected && EHttpResponseCodes::IsOk(Code))
		{
			if (bLoggedHttpFailure)
			{
				UE_LOG(LogZLTelemetry, Log, TEXT("API reachable again"));
				bLoggedHttpFailure = false;
			}
			return;
		}

		// No connection, server error or rate limit: put the events back and try with the next flush.
		// Anything else (bad key, rejected data) would fail again, so drop it.
		const bool bRetry = !bConnected || Code >= 500 || Code == 429;
		if (!bLoggedHttpFailure)
		{
			UE_LOG(LogZLTelemetry, Warning, TEXT("POST %s failed (%s, HTTP %d): %s. %s"), *(IngestUrl / TEXT("v1/events")),
				bConnected ? TEXT("connected") : TEXT("no connection"), Code,
				Response ? *Response->GetContentAsString().Left(200) : TEXT(""),
				bRetry ? TEXT("Will retry.") : TEXT("Dropping batch."));
			bLoggedHttpFailure = true;
		}
		if (bRetry && RetryEvents.Num() + Batch.Num() <= MaxBufferedEvents)
		{
			RetryEvents.Append(Batch);
		}
	});

	RequestsInFlight++;
	Request->ProcessRequest();
}

void UZL_TelemetrySubsystem::WriteLocal(const TArray<TSharedPtr<FJsonObject>>& Batch)
{
	FString Lines;
	for (const TSharedPtr<FJsonObject>& Event : Batch)
	{
		Lines += ToJsonLine(Event.ToSharedRef());
		Lines += TEXT("\n");
	}
	FFileHelper::SaveStringToFile(Lines, *LocalFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

// ---------------------------------------------------------------------------------------------------------------------
// Helpers

UWorld* UZL_TelemetrySubsystem::GetGameWorld() const
{
	return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
}

FString UZL_TelemetrySubsystem::GetCurrentMatchId() const
{
	if (IsInMatch()) return MatchId;
	const UWorld* World = GetGameWorld();
	const AZero_BaseGameState* GS = World ? World->GetGameState<AZero_BaseGameState>() : nullptr;
	return GS ? GS->TelemetryMatchId : FString();
}

double UZL_TelemetrySubsystem::GetMatchTime() const
{
	if (IsInMatch())
	{
		return MatchWorld.IsValid() ? MatchWorld->GetTimeSeconds() - MatchStartTime : 0.0;
	}
	const UWorld* World = GetGameWorld();
	const AZero_BaseGameState* GS = World ? World->GetGameState<AZero_BaseGameState>() : nullptr;
	return GS ? GS->myGameTime : 0.0;
}

FString UZL_TelemetrySubsystem::GetRole() const
{
	const UWorld* World = GetGameWorld();
	switch (World ? World->GetNetMode() : NM_Standalone)
	{
	case NM_ListenServer: return TEXT("host");
	case NM_DedicatedServer: return TEXT("server");
	case NM_Client: return TEXT("client");
	default: return TEXT("standalone");
	}
}

TSharedRef<FJsonObject> UZL_TelemetrySubsystem::MakeEvent(const FString& Type) const
{
	const TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
	Event->SetStringField(TEXT("match_id"), GetCurrentMatchId());
	Event->SetNumberField(TEXT("t"), Round1(GetMatchTime()));
	Event->SetStringField(TEXT("type"), Type);
	return Event;
}

void UZL_TelemetrySubsystem::AddEvent(const TSharedRef<FJsonObject>& Event)
{
	if (Events.Num() >= MaxBufferedEvents)
	{
		Events.RemoveAt(0, Events.Num() - MaxBufferedEvents + 1);
	}
	Events.Add(Event);
}

void UZL_TelemetrySubsystem::AddHero(const TSharedRef<FJsonObject>& Event, const AZeroLockCharacter* Hero, const FString& Prefix)
{
	if (!Hero) return;
	const APlayerState* PS = Hero->GetPlayerState();
	// "player" / "target" (not "target_player"): the API column names.
	Event->SetStringField(Prefix.IsEmpty() ? TEXT("player") : Prefix.LeftChop(1), PlayerKey(PS));
	Event->SetStringField(Prefix + TEXT("hero"), CleanClassName(Hero->GetClass()));
	Event->SetStringField(Prefix + TEXT("team"), TeamName(PS));
}

void UZL_TelemetrySubsystem::AddLocation(const TSharedRef<FJsonObject>& Event, const FVector& Location, const FString& Prefix)
{
	Event->SetNumberField(Prefix + TEXT("x"), FMath::RoundToInt(Location.X));
	Event->SetNumberField(Prefix + TEXT("y"), FMath::RoundToInt(Location.Y));
	Event->SetNumberField(Prefix + TEXT("z"), FMath::RoundToInt(Location.Z));
}

FString UZL_TelemetrySubsystem::PlayerKey(const APlayerState* PS)
{
	if (!PS) return FString();
	// Hash of the online id: stable across matches for the same account, without sending the id itself.
	const FString NetId = PS->GetUniqueId().IsValid() ? PS->GetUniqueId().ToString() : FString();
	if (NetId.IsEmpty()) return FString::Printf(TEXT("p%d"), PS->GetPlayerId());
	return FMD5::HashAnsiString(*NetId).Left(12);
}

FString UZL_TelemetrySubsystem::TeamName(const APlayerState* PS)
{
	const AZero_BasePlayerState* ZPS = Cast<AZero_BasePlayerState>(PS);
	if (!ZPS) return TEXT("none");
	switch (ZPS->TeamID)
	{
	case ETeamID::TeamRed: return TEXT("red");
	case ETeamID::TeamBlue: return TEXT("blue");
	default: return TEXT("none");
	}
}

FString UZL_TelemetrySubsystem::CleanClassName(const UClass* Class)
{
	if (!Class) return FString();
	FString Name = Class->GetName();
	Name.RemoveFromEnd(TEXT("_C"));
	// C++ classes and their BPs should read the same: ZL_Pocket_X and BP_Pocket_X are both Pocket_X.
	const TCHAR* Prefixes[] = { TEXT("BP_"), TEXT("ZL_"), TEXT("GA_"), TEXT("Deadlock_"), TEXT("_") };
	for (bool bStripped = true; bStripped;)
	{
		bStripped = false;
		for (const TCHAR* Prefix : Prefixes)
		{
			bStripped |= Name.RemoveFromStart(Prefix);
		}
	}
	return Name.IsEmpty() ? Class->GetName() : Name;
}
