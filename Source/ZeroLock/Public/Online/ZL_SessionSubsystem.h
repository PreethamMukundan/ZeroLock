// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonSessionSubsystem.h"
#include "GameplayTagContainer.h"
#include "ZL_SessionSubsystem.generated.h"

class UCommonUserInfo;
struct FOnlineResultInformation;
enum class ECommonUserPrivilege : uint8;
enum class ECommonUserOnlineContext : uint8;

/**
 * Host request that takes a map package path. Our maps aren't registered as "Map" primary assets,
 * so CommonUser's MapID lookup would fail; MapPath wins when set.
 */
UCLASS(BlueprintType)
class ZEROLOCK_API UZL_HostSessionRequest : public UCommonSession_HostSessionRequest
{
	GENERATED_BODY()

public:
	/** Long package name, e.g. /Game/ThirdPerson/Maps/ThirdPersonMap */
	UPROPERTY(BlueprintReadWrite, Category = Session)
	FString MapPath;

	virtual FString GetMapName() const override;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FZL_OnOnlineLoginFinished, bool, bSuccess, FText, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FZL_OnFindMatchesFinished, bool, bSuccess, FText, Error);

/**
 * Lyra-style online flow on top of CommonUser (replaces UZL_EOS_SubSystem).
 * Login -> Host / QuickPlay / Find + Join -> Leave. Works with whatever online subsystem the config picks:
 * Null (LAN) by default, EOS lobbies over P2P with -CustomConfig=EOS. Console commands: ZL.Online.*
 */
UCLASS(Config = Engine)
class ZEROLOCK_API UZL_SessionSubsystem : public UCommonSessionSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual UCommonSession_HostSessionRequest* CreateOnlineHostSessionRequest() override;
	virtual UCommonSession_SearchSessionRequest* CreateOnlineSearchSessionRequest() override;

	/** Logs the first local player in for online play (EOS: persistent auth, then account portal; -AUTH_TYPE= overrides). */
	UFUNCTION(BlueprintCallable, Category = "Online")
	void Login();

	UFUNCTION(BlueprintPure, Category = "Online")
	bool IsLoggedInOnline() const;

	/**
	 * Creates a session/lobby and travels to the map as a listen server, using GetDefaultOnlineMode().
	 * Empty MapPath = DefaultMatchMap, MaxPlayers <= 0 = DefaultMaxPlayers.
	 */
	UFUNCTION(BlueprintCallable, Category = "Online")
	void HostMatch(const FString& MapPath, int32 MaxPlayers = 0);

	/** HostMatch with an explicit mode (Offline = plain ServerTravel, no session). */
	UFUNCTION(BlueprintCallable, Category = "Online")
	void HostMatchWithMode(const FString& MapPath, int32 MaxPlayers, ECommonSessionOnlineMode Mode);

	/** Joins the first match on MapPath with a free slot, or hosts one if none is found. */
	UFUNCTION(BlueprintCallable, Category = "Online")
	void QuickPlay(const FString& MapPath);

	/** Searches for matches; results land in GetLastSearchResults() and OnFindMatchesFinished fires. */
	UFUNCTION(BlueprintCallable, Category = "Online")
	void FindMatches();

	UFUNCTION(BlueprintCallable, Category = "Online")
	void JoinMatch(UCommonSession_SearchResult* Result);

	/** Index into GetLastSearchResults(). */
	UFUNCTION(BlueprintCallable, Category = "Online")
	void JoinMatchByIndex(int32 Index);

	/** Destroys the session and goes back to MainMenuMap. */
	UFUNCTION(BlueprintCallable, Category = "Online")
	void LeaveMatch();

	UFUNCTION(BlueprintPure, Category = "Online")
	TArray<UCommonSession_SearchResult*> GetLastSearchResults() const { return ObjectPtrDecay(LastSearchResults); }

	/** Human readable login/session state, also printed by ZL.Online.Status. */
	UFUNCTION(BlueprintPure, Category = "Online")
	FString GetStatusString() const;

	/** Short one-line description of a search result (host, map, players, ping). */
	UFUNCTION(BlueprintPure, Category = "Online")
	static FString DescribeSearchResult(const UCommonSession_SearchResult* Result);

	/** Online when the default online subsystem is a real service (EOS), LAN when it's Null (PIE, local testing). */
	ECommonSessionOnlineMode GetDefaultOnlineMode() const;

	UPROPERTY(BlueprintAssignable, Category = "Online")
	FZL_OnOnlineLoginFinished OnLoginFinished;

	UPROPERTY(BlueprintAssignable, Category = "Online")
	FZL_OnFindMatchesFinished OnFindMatchesFinished;

	/** Map used by HostMatch/QuickPlay when no path is given. */
	UPROPERTY(Config, EditAnywhere, Category = "Online")
	FString DefaultMatchMap = TEXT("/Game/ThirdPerson/Maps/ThirdPersonMap");

	/** Map LeaveMatch returns to. */
	UPROPERTY(Config, EditAnywhere, Category = "Online")
	FString MainMenuMap = TEXT("/Game/ZERO/Levels/Lvl_MainMenu");

	UPROPERTY(Config, EditAnywhere, Category = "Online")
	int32 DefaultMaxPlayers = 12;

	/** Advertised game mode (SETTING_GAMEMODE). CommonUser's QuickPlay doesn't filter on it. */
	UPROPERTY(Config, EditAnywhere, Category = "Online")
	FString ModeName = TEXT("TDM");

private:
	/** Runs Action now if logged in online, otherwise logs in first and runs it on success. */
	void RunAfterLogin(TFunction<void()>&& Action);

	APlayerController* GetLocalPC() const;

	UFUNCTION()
	void HandleUserInitializeComplete(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext);

	UFUNCTION()
	void HandleSystemMessage(FGameplayTag MessageType, FText TitleText, FText BodyText);

	void HandleSearchFinished(bool bSucceeded, const FText& ErrorMessage);
	void HandleCreateSessionComplete(const FOnlineResultInformation& Result);
	void HandleJoinSessionComplete(const FOnlineResultInformation& Result);
	void HandleUserRequestedSession(const FPlatformUserId& LocalPlatformUserId, UCommonSession_SearchResult* RequestedSession, const FOnlineResultInformation& RequestedSessionResult);
	void HandleDestroySessionRequested(const FPlatformUserId& LocalPlatformUserId, const FName& SessionName);

	UPROPERTY(Transient)
	TObjectPtr<UCommonSession_SearchSessionRequest> CurrentSearch;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCommonSession_SearchResult>> LastSearchResults;

	TArray<TFunction<void()>> ActionsAfterLogin;
};
