// Copyright Preetham Mukundan (C) 2026

#include "Online/ZL_SessionSubsystem.h"

#include "CommonUserSubsystem.h"
#include "CommonUserTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "UObject/Package.h"
#include "ZeroLock/ZeroLock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ZL_SessionSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogZLSession, Log, All);

namespace ZLSession
{
	void Print(const FString& Msg, const FColor& Color = FColor::Cyan)
	{
		UE_LOG(LogZLSession, Log, TEXT("%s"), *Msg);
		if (GEngine)
		{
			ZLOG_COLOR_TIME(Msg, Color, 8.f);
		}
	}

	void PrintError(const FString& Msg)
	{
		UE_LOG(LogZLSession, Warning, TEXT("%s"), *Msg);
		if (GEngine)
		{
			ZLOG_COLOR_TIME(Msg, FColor::Red, 10.f);
		}
	}

	UZL_SessionSubsystem* Get(UWorld* World)
	{
		UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		UZL_SessionSubsystem* Subsystem = GI ? GI->GetSubsystem<UZL_SessionSubsystem>() : nullptr;
		if (!Subsystem)
		{
			PrintError(TEXT("ZL.Online: no game instance / session subsystem in this world"));
		}
		return Subsystem;
	}

	bool ParseMode(const FString& Arg, ECommonSessionOnlineMode& OutMode)
	{
		if (Arg.Equals(TEXT("online"), ESearchCase::IgnoreCase)) { OutMode = ECommonSessionOnlineMode::Online; return true; }
		if (Arg.Equals(TEXT("lan"), ESearchCase::IgnoreCase)) { OutMode = ECommonSessionOnlineMode::LAN; return true; }
		if (Arg.Equals(TEXT("offline"), ESearchCase::IgnoreCase)) { OutMode = ECommonSessionOnlineMode::Offline; return true; }
		return false;
	}
}

//////////////////////////////////////////////////////////////////////
// Console commands

static FAutoConsoleCommandWithWorldAndArgs CmdLogin(
	TEXT("ZL.Online.Login"),
	TEXT("Log the first local player in for online play (EOS with -CustomConfig=EOS, Null otherwise)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (UZL_SessionSubsystem* Sessions = ZLSession::Get(World)) { Sessions->Login(); }
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdHost(
	TEXT("ZL.Online.Host"),
	TEXT("ZL.Online.Host [MapPath|-] [MaxPlayers] [online|lan|offline]. Host a listen-server match (logs in first if needed). No args = current map, '-' = default map."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UZL_SessionSubsystem* Sessions = ZLSession::Get(World);
		if (!Sessions) return;

		FString MapPath;
		if (Args.Num() == 0)
		{
			// Host on the map we're standing in (strip the PIE prefix so the travel URL is the real package)
			MapPath = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
		}
		else if (Args[0] != TEXT("-"))
		{
			MapPath = Args[0];
		}
		const int32 MaxPlayers = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;
		ECommonSessionOnlineMode Mode = Sessions->GetDefaultOnlineMode();
		if (Args.Num() > 2 && !ZLSession::ParseMode(Args[2], Mode))
		{
			ZLSession::PrintError(FString::Printf(TEXT("ZL.Online.Host: unknown mode '%s' (online|lan|offline)"), *Args[2]));
			return;
		}
		Sessions->HostMatchWithMode(MapPath, MaxPlayers, Mode);
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdQuickPlay(
	TEXT("ZL.Online.QuickPlay"),
	TEXT("ZL.Online.QuickPlay [MapPath]. Join the first match found, or host one if there is none."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UZL_SessionSubsystem* Sessions = ZLSession::Get(World)) { Sessions->QuickPlay(Args.Num() > 0 ? Args[0] : FString()); }
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdFind(
	TEXT("ZL.Online.Find"),
	TEXT("Search for matches and print them with their index (use ZL.Online.Join <Index>)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (UZL_SessionSubsystem* Sessions = ZLSession::Get(World)) { Sessions->FindMatches(); }
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdJoin(
	TEXT("ZL.Online.Join"),
	TEXT("ZL.Online.Join <Index>. Join a match from the last ZL.Online.Find."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UZL_SessionSubsystem* Sessions = ZLSession::Get(World)) { Sessions->JoinMatchByIndex(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0); }
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdLeave(
	TEXT("ZL.Online.Leave"),
	TEXT("Leave/destroy the current session and return to the main menu map."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (UZL_SessionSubsystem* Sessions = ZLSession::Get(World)) { Sessions->LeaveMatch(); }
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdStatus(
	TEXT("ZL.Online.Status"),
	TEXT("Print the online subsystem, login state and current session."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (UZL_SessionSubsystem* Sessions = ZLSession::Get(World)) { ZLSession::Print(Sessions->GetStatusString(), FColor::White); }
	}));

//////////////////////////////////////////////////////////////////////
// UZL_HostSessionRequest

FString UZL_HostSessionRequest::GetMapName() const
{
	return MapPath.IsEmpty() ? Super::GetMapName() : MapPath;
}

//////////////////////////////////////////////////////////////////////
// UZL_SessionSubsystem

void UZL_SessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UCommonUserSubsystem>();
	Super::Initialize(Collection);

	if (UCommonUserSubsystem* Users = GetGameInstance()->GetSubsystem<UCommonUserSubsystem>())
	{
		Users->OnUserInitializeComplete.AddDynamic(this, &ThisClass::HandleUserInitializeComplete);
		Users->OnHandleSystemMessage.AddDynamic(this, &ThisClass::HandleSystemMessage);
	}

	OnCreateSessionCompleteEvent.AddUObject(this, &ThisClass::HandleCreateSessionComplete);
	OnJoinSessionCompleteEvent.AddUObject(this, &ThisClass::HandleJoinSessionComplete);
	OnUserRequestedSessionEvent.AddUObject(this, &ThisClass::HandleUserRequestedSession);
	OnDestroySessionRequestedEvent.AddUObject(this, &ThisClass::HandleDestroySessionRequested);
}

void UZL_SessionSubsystem::Deinitialize()
{
	if (UCommonUserSubsystem* Users = GetGameInstance()->GetSubsystem<UCommonUserSubsystem>())
	{
		Users->OnUserInitializeComplete.RemoveAll(this);
		Users->OnHandleSystemMessage.RemoveAll(this);
	}
	OnCreateSessionCompleteEvent.RemoveAll(this);
	OnJoinSessionCompleteEvent.RemoveAll(this);
	OnUserRequestedSessionEvent.RemoveAll(this);
	OnDestroySessionRequestedEvent.RemoveAll(this);
	ActionsAfterLogin.Reset();

	Super::Deinitialize();
}

UCommonSession_HostSessionRequest* UZL_SessionSubsystem::CreateOnlineHostSessionRequest()
{
	UZL_HostSessionRequest* Request = NewObject<UZL_HostSessionRequest>(this);
	Request->OnlineMode = GetDefaultOnlineMode();
	Request->bUseLobbies = bUseLobbiesDefault;
	Request->bUseLobbiesVoiceChat = bUseLobbiesVoiceChatDefault;
	Request->bUsePresence = true;
	Request->ModeNameForAdvertisement = ModeName;
	Request->MapPath = DefaultMatchMap;
	Request->MaxPlayerCount = DefaultMaxPlayers;
	return Request;
}

UCommonSession_SearchSessionRequest* UZL_SessionSubsystem::CreateOnlineSearchSessionRequest()
{
	UCommonSession_SearchSessionRequest* Request = Super::CreateOnlineSearchSessionRequest();
	Request->OnlineMode = GetDefaultOnlineMode();
	return Request;
}

ECommonSessionOnlineMode UZL_SessionSubsystem::GetDefaultOnlineMode() const
{
	const IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	return (OSS && OSS->GetSubsystemName() != NULL_SUBSYSTEM) ? ECommonSessionOnlineMode::Online : ECommonSessionOnlineMode::LAN;
}

APlayerController* UZL_SessionSubsystem::GetLocalPC() const
{
	return GetGameInstance()->GetFirstLocalPlayerController();
}

//////////////////////////////////////////////////////////////////////
// Login

void UZL_SessionSubsystem::Login()
{
	RunAfterLogin([] {});
}

bool UZL_SessionSubsystem::IsLoggedInOnline() const
{
	const UCommonUserSubsystem* Users = GetGameInstance()->GetSubsystem<UCommonUserSubsystem>();
	return Users && Users->GetLocalPlayerInitializationState(0) == ECommonUserInitializationState::LoggedInOnline;
}

void UZL_SessionSubsystem::RunAfterLogin(TFunction<void()>&& Action)
{
	if (IsLoggedInOnline())
	{
		Action();
		return;
	}

	UCommonUserSubsystem* Users = GetGameInstance()->GetSubsystem<UCommonUserSubsystem>();
	if (!Users)
	{
		ZLSession::PrintError(TEXT("Online: CommonUserSubsystem missing"));
		return;
	}

	ActionsAfterLogin.Add(MoveTemp(Action));
	const ECommonUserInitializationState State = Users->GetLocalPlayerInitializationState(0);
	if (ActionsAfterLogin.Num() > 1 && (State == ECommonUserInitializationState::DoingInitialLogin || State == ECommonUserInitializationState::DoingNetworkLogin))
	{
		return; // already logging in, the action runs when it finishes
	}

	FCommonUserInitializeParams Params;
	Params.LocalPlayerIndex = 0;
	Params.RequestedPrivilege = ECommonUserPrivilege::CanPlayOnline;
	Params.OnlineContext = ECommonUserOnlineContext::Game;
	if (APlayerController* PC = GetLocalPC())
	{
		if (const ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			Params.PlatformUser = LP->GetPlatformUserId();
		}
	}

	const IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	ZLSession::Print(FString::Printf(TEXT("Online: logging in (%s)..."), OSS ? *OSS->GetSubsystemName().ToString() : TEXT("no OSS")));
	if (!Users->TryToInitializeUser(Params))
	{
		ActionsAfterLogin.Reset();
		ZLSession::PrintError(TEXT("Online: login could not start (see LogCommonUser)"));
		OnLoginFinished.Broadcast(false, NSLOCTEXT("ZLSession", "LoginStartFailed", "Login could not start."));
	}
}

void UZL_SessionSubsystem::HandleUserInitializeComplete(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext)
{
	if (RequestedPrivilege != ECommonUserPrivilege::CanPlayOnline)
	{
		return;
	}

	TArray<TFunction<void()>> Actions = MoveTemp(ActionsAfterLogin);
	ActionsAfterLogin.Reset();

	if (bSuccess)
	{
		ZLSession::Print(FString::Printf(TEXT("Online: logged in as %s"), UserInfo ? *UserInfo->GetNickname() : TEXT("?")), FColor::Green);
		for (TFunction<void()>& Action : Actions)
		{
			Action();
		}
	}
	else
	{
		ZLSession::PrintError(FString::Printf(TEXT("Online: login failed: %s"), *Error.ToString()));
	}
	OnLoginFinished.Broadcast(bSuccess, Error);
}

void UZL_SessionSubsystem::HandleSystemMessage(FGameplayTag MessageType, FText TitleText, FText BodyText)
{
	ZLSession::PrintError(FString::Printf(TEXT("Online [%s] %s: %s"), *MessageType.ToString(), *TitleText.ToString(), *BodyText.ToString()));
}

//////////////////////////////////////////////////////////////////////
// Host / join / leave

void UZL_SessionSubsystem::HostMatch(const FString& MapPath, int32 MaxPlayers)
{
	HostMatchWithMode(MapPath, MaxPlayers, GetDefaultOnlineMode());
}

void UZL_SessionSubsystem::HostMatchWithMode(const FString& MapPath, int32 MaxPlayers, ECommonSessionOnlineMode Mode)
{
	UZL_HostSessionRequest* Request = CastChecked<UZL_HostSessionRequest>(CreateOnlineHostSessionRequest());
	Request->OnlineMode = Mode;
	if (!MapPath.IsEmpty())
	{
		Request->MapPath = MapPath;
	}
	if (MaxPlayers > 0)
	{
		Request->MaxPlayerCount = MaxPlayers;
	}

	auto DoHost = [this, Request = TStrongObjectPtr<UZL_HostSessionRequest>(Request)]()
	{
		ZLSession::Print(FString::Printf(TEXT("Online: hosting %s (%s, %d players)"), *Request->MapPath,
			*UEnum::GetDisplayValueAsText(Request->OnlineMode).ToString(), Request->GetMaxPlayers()));
		HostSession(GetLocalPC(), Request.Get());
	};

	if (Mode == ECommonSessionOnlineMode::Offline)
	{
		DoHost();
	}
	else
	{
		RunAfterLogin(MoveTemp(DoHost));
	}
}

void UZL_SessionSubsystem::QuickPlay(const FString& MapPath)
{
	UZL_HostSessionRequest* Request = CastChecked<UZL_HostSessionRequest>(CreateOnlineHostSessionRequest());
	if (!MapPath.IsEmpty())
	{
		Request->MapPath = MapPath;
	}

	RunAfterLogin([this, Request = TStrongObjectPtr<UZL_HostSessionRequest>(Request)]()
	{
		ZLSession::Print(FString::Printf(TEXT("Online: quick play (%s)"), *Request->MapPath));
		QuickPlaySession(GetLocalPC(), Request.Get());
	});
}

void UZL_SessionSubsystem::FindMatches()
{
	RunAfterLogin([this]()
	{
		CurrentSearch = CreateOnlineSearchSessionRequest();
		CurrentSearch->OnSearchFinished.AddUObject(this, &ThisClass::HandleSearchFinished);
		ZLSession::Print(TEXT("Online: searching..."));
		FindSessions(GetLocalPC(), CurrentSearch);
	});
}

void UZL_SessionSubsystem::HandleSearchFinished(bool bSucceeded, const FText& ErrorMessage)
{
	LastSearchResults = CurrentSearch ? CurrentSearch->Results : TArray<TObjectPtr<UCommonSession_SearchResult>>();
	CurrentSearch = nullptr;

	// Some OSS layers report failure when there are simply no sessions, so treat an empty error as success (same as CommonUser's QuickPlay).
	const bool bOk = bSucceeded || ErrorMessage.IsEmpty();
	if (bOk)
	{
		ZLSession::Print(FString::Printf(TEXT("Online: found %d match(es)"), LastSearchResults.Num()), FColor::Green);
		for (int32 i = 0; i < LastSearchResults.Num(); ++i)
		{
			ZLSession::Print(FString::Printf(TEXT("  [%d] %s"), i, *DescribeSearchResult(LastSearchResults[i])), FColor::White);
		}
	}
	else
	{
		ZLSession::PrintError(FString::Printf(TEXT("Online: search failed: %s"), *ErrorMessage.ToString()));
	}
	OnFindMatchesFinished.Broadcast(bOk, ErrorMessage);
}

void UZL_SessionSubsystem::JoinMatch(UCommonSession_SearchResult* Result)
{
	if (!Result)
	{
		ZLSession::PrintError(TEXT("Online: JoinMatch with no result"));
		return;
	}

	RunAfterLogin([this, Result = TStrongObjectPtr<UCommonSession_SearchResult>(Result)]()
	{
		ZLSession::Print(FString::Printf(TEXT("Online: joining %s"), *DescribeSearchResult(Result.Get())));
		JoinSession(GetLocalPC(), Result.Get());
	});
}

void UZL_SessionSubsystem::JoinMatchByIndex(int32 Index)
{
	if (!LastSearchResults.IsValidIndex(Index))
	{
		ZLSession::PrintError(FString::Printf(TEXT("Online: no search result %d (have %d, run ZL.Online.Find)"), Index, LastSearchResults.Num()));
		return;
	}
	JoinMatch(LastSearchResults[Index]);
}

void UZL_SessionSubsystem::LeaveMatch()
{
	ZLSession::Print(TEXT("Online: leaving match"));
	CleanUpSessions();

	if (!MainMenuMap.IsEmpty())
	{
		UGameplayStatics::OpenLevel(GetGameInstance(), FName(*MainMenuMap));
	}
}

void UZL_SessionSubsystem::HandleCreateSessionComplete(const FOnlineResultInformation& Result)
{
	if (Result.bWasSuccessful)
	{
		ZLSession::Print(TEXT("Online: session created, travelling"), FColor::Green);

		// CommonUser calls ServerTravel(Map?listen) right after this event. A seamless travel keeps the current net setup,
		// so from a standalone world it never starts listening (clients connect to nothing). Use a hard travel for that hop;
		// the new world's game mode has its own bUseSeamlessTravel again.
		UWorld* World = GetWorld();
		if (World && World->GetNetMode() == NM_Standalone)
		{
			if (AGameModeBase* GM = World->GetAuthGameMode())
			{
				GM->bUseSeamlessTravel = false;
			}
		}
	}
	else
	{
		ZLSession::PrintError(FString::Printf(TEXT("Online: host failed: %s (%s)"), *Result.ErrorText.ToString(), *Result.ErrorId));
	}
}

void UZL_SessionSubsystem::HandleJoinSessionComplete(const FOnlineResultInformation& Result)
{
	if (Result.bWasSuccessful)
	{
		ZLSession::Print(TEXT("Online: joined session, travelling"), FColor::Green);
	}
	else
	{
		ZLSession::PrintError(FString::Printf(TEXT("Online: join failed: %s (%s)"), *Result.ErrorText.ToString(), *Result.ErrorId));
	}
}

void UZL_SessionSubsystem::HandleUserRequestedSession(const FPlatformUserId& LocalPlatformUserId, UCommonSession_SearchResult* RequestedSession, const FOnlineResultInformation& RequestedSessionResult)
{
	// Invite accepted / "join game" from the overlay or friends list (CommonGame's UCommonGameInstance does the same).
	if (!RequestedSession)
	{
		ZLSession::PrintError(FString::Printf(TEXT("Online: requested session invalid: %s"), *RequestedSessionResult.ErrorText.ToString()));
		return;
	}
	JoinMatch(RequestedSession);
}

void UZL_SessionSubsystem::HandleDestroySessionRequested(const FPlatformUserId& LocalPlatformUserId, const FName& SessionName)
{
	LeaveMatch();
}

//////////////////////////////////////////////////////////////////////
// Debug

FString UZL_SessionSubsystem::DescribeSearchResult(const UCommonSession_SearchResult* Result)
{
	if (!Result)
	{
		return TEXT("<null>");
	}

	FString MapName;
	bool bFound = false;
	Result->GetStringSetting(SETTING_MAPNAME, MapName, bFound);
	FString Mode;
	Result->GetStringSetting(SETTING_GAMEMODE, Mode, bFound);

	const int32 MaxPlayers = Result->GetMaxPublicConnections();
	const int32 Players = MaxPlayers - Result->GetNumOpenPublicConnections();
#if COMMONUSER_OSSV1
	const FString Host = Result->Result.Session.OwningUserName;
#else
	const FString Host = TEXT("?");
#endif
	return FString::Printf(TEXT("%s | %s %s | %d/%d | %d ms"), *Host, *Mode, *FPackageName::GetShortName(MapName), Players, MaxPlayers, Result->GetPingInMs());
}

FString UZL_SessionSubsystem::GetStatusString() const
{
	const IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	const UCommonUserSubsystem* Users = GetGameInstance()->GetSubsystem<UCommonUserSubsystem>();
	const UCommonUserInfo* User = Users ? Users->GetUserInfoForLocalPlayerIndex(0) : nullptr;

	FString Session = TEXT("none");
	if (OSS)
	{
		const IOnlineSessionPtr SessionInterface = OSS->GetSessionInterface();
		if (const FNamedOnlineSession* Named = SessionInterface ? SessionInterface->GetNamedSession(NAME_GameSession) : nullptr)
		{
			const int32 MaxPlayers = Named->SessionSettings.NumPublicConnections;
			Session = FString::Printf(TEXT("%s, %s, %d/%d players, lobby=%d"), EOnlineSessionState::ToString(Named->SessionState),
				Named->bHosting ? TEXT("host") : TEXT("client"), MaxPlayers - Named->NumOpenPublicConnections, MaxPlayers, Named->SessionSettings.bUseLobbiesIfAvailable);
		}
	}

	const UWorld* World = GetWorld();
	const TCHAR* NetMode = TEXT("?");
	if (World)
	{
		switch (World->GetNetMode())
		{
		case NM_Standalone: NetMode = TEXT("Standalone"); break;
		case NM_ListenServer: NetMode = TEXT("ListenServer"); break;
		case NM_DedicatedServer: NetMode = TEXT("DedicatedServer"); break;
		case NM_Client: NetMode = TEXT("Client"); break;
		default: break;
		}
	}

	return FString::Printf(TEXT("OSS=%s | default mode=%s | user=%s (%s) | session=%s | net=%s | map=%s"),
		OSS ? *OSS->GetSubsystemName().ToString() : TEXT("none"),
		*UEnum::GetDisplayValueAsText(GetDefaultOnlineMode()).ToString(),
		User ? *User->GetNickname() : TEXT("none"),
		Users ? *UEnum::GetValueAsString(Users->GetLocalPlayerInitializationState(0)) : TEXT("?"),
		*Session, NetMode,
		World ? *World->GetMapName() : TEXT("?"));
}
