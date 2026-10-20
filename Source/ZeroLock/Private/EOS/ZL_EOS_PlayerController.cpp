//Copyright Preetham Mukundan (C) 2026


#include "EOS/ZL_EOS_PlayerController.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystemTypes.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "ZeroLock/ZeroLock.h"
AZL_EOS_PlayerController::AZL_EOS_PlayerController()
{
    // Tutorial 2: Including constructor here for clarity. Nothing added in derived class for this tutorial.
}

void AZL_EOS_PlayerController::BeginPlay()
{
	Super::BeginPlay();
    ZLOG("PlayerController EOS");
   //Login();
}

void AZL_EOS_PlayerController::Login()
{
    /*
    Tutorial 2: This function will access the EOS OSS via the OSS identity interface to log first into Epic Account Services, and then into Epic Game Services.
    It will bind a delegate to handle the callback event once login call succeeeds or fails. 
    All functions that access the OSS will have this structure: 1-Get OSS interface, 2-Bind delegate for callback and 3-Call OSS interface function (which will call the correspongin EOS OSS function)
    */
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface(); // This is the generic OSS interface that will access the EOS OSS.

    // If you're logged in, don't try to login again.
    // This can happen if your player travels to a dedicated server or different maps as BeginPlay() will be called each time.
  
    FUniqueNetIdPtr NetId = Identity->GetUniquePlayerId(0);

    if (NetId != nullptr && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
    {
        return; 
    }
    
    /* This binds a delegate so we can run our function when the callback completes. 0 represents the player number.
    You should parametrize this Login function and pass the parameter here for splitscreen. 
    */
    LoginDelegateHandle = 
    Identity->AddOnLoginCompleteDelegate_Handle(
        0,
        FOnLoginCompleteDelegate::CreateUObject(
            this,
            &ThisClass::HandleLoginCompleted));

    // Grab command line parameters. If empty call hardcoded login function - Hardcoded login function useful for Play In Editor. 
    FString AuthType; 
    FParse::Value(FCommandLine::Get(), TEXT("AUTH_TYPE="), AuthType);

    if (!AuthType.IsEmpty()) //If parameter is NOT empty we can autologin.
    {
        /* 
        In most situations you will want to automatically log a player in using the parameters passed via CLI.
        For example, using the exchange code for the Epic Games Store.
        */
        UE_LOG(LogTemp, Log, TEXT("Logging into EOS...")); // Log to the UE logs that we are trying to log in. 
      
        if (!Identity->AutoLogin(0))
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to login... ")); // Log to the UE logs that failed to log in. 
            // Clear our handle and reset the delegate.
            Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginDelegateHandle);
            LoginDelegateHandle.Reset();
        }
    }
    else
    {
        /* 
        Fallback if the CLI parameters are empty.Useful for PIE.
        The type here could be developer if using the DevAuthTool, ExchangeCode if the game is launched via the Epic Games Launcher, etc...
        */
        FOnlineAccountCredentials Credentials("AccountPortal","", "");

        UE_LOG(LogTemp, Log, TEXT("Logging into EOS...")); // Log to the UE logs that we are trying to log in. 
        
        if (!Identity->Login(0, Credentials))
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to login... ")); // Log to the UE logs that we failed to log in. 

            // Clear our handle and reset the delegate. 
            Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginDelegateHandle);
            LoginDelegateHandle.Reset();
        }        
    }
}

void AZL_EOS_PlayerController::HandleLoginCompleted(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
    /*
    Tutorial 2: This function handles the callback from logging in. You should not proceed with any EOS features until this function is called.
    This function will remove the delegate that was bound in the Login() function.
    */
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
    if (bWasSuccessful)
    {
        UE_LOG(LogTemp, Log, TEXT("Login callback completed!"));
        UE_LOG(LogTemp, Log, TEXT("Loading cloud data and searching for a session..."));
        
        // LoadTitleData(); // Load any game related data (in this case a string output to logs)
       //  LoadPlayerData(); // Load save game data 
         FindSessions(); // For convenience a session is found in sequence here. In a real game this would be done via game UI. Goal here is to show EOS functionality, not game design. 
    }
    else //Login failed
    {
        // If your game is online only, you may want to return an errror to the user and return to a menu that uses a different GameMode/PlayerController.

        UE_LOG(LogTemp, Warning, TEXT("EOS login failed.")); //Print sign in failure in logs as a warning.
    }

    // Clear our handle and reset the delegate. 
    Identity->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginDelegateHandle);
    LoginDelegateHandle.Reset();
}

void AZL_EOS_PlayerController::FindSessions(FName SearchKey, FString SearchValue) //put default value for example 
{
    // Tutorial 4: This function will find our EOS Session that was created by our DedicatedServer.
    // Tutorial 7: This function will find our EOS lobby. Note that at the OSS layer we are using a Session that is marked as a lobby.  Code is similar with minor tweaks

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
    TSharedRef<FOnlineSessionSearch> Search = MakeShared<FOnlineSessionSearch>();

    // Remove the default search parameters that FOnlineSessionSearch sets up.
    Search->QuerySettings.SearchParams.Empty();

    Search->QuerySettings.Set(SearchKey, SearchValue, EOnlineComparisonOp::Equals); // Seach using our Key/Value pair

    Search->QuerySettings.Set("ZeroLockLobbies", true, EOnlineComparisonOp::Equals);

    FindSessionsDelegateHandle =
        Session->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(
            this,
            &ThisClass::HandleFindSessionsCompleted,
            Search));

    UE_LOG(LogTemp, Log, TEXT("Finding lobby."));


    if (!Session->FindSessions(0, Search))
    {

        UE_LOG(LogTemp, Log, TEXT("Finding lobby failed."));

        // Clear our handle and reset the delegate. 
        Session->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
        FindSessionsDelegateHandle.Reset();
    }
}

void AZL_EOS_PlayerController::HandleFindSessionsCompleted(bool bWasSuccessful, TSharedRef<FOnlineSessionSearch> Search)
{
    // Tutorial 4: This function is triggered via the callback we set in FindSession once the session is found (or there is a failure). 
    // Tutorial 7: This function will triggered via the callback we set in FindSession once the lobby is found (or there is a failure). Finding the lobby here has the similar code as finding a session.  

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();

    if (bWasSuccessful)
    {
        // added code here to not run into issues when searching for sessions is succesfull, but the number of sessions is 0
        if (Search->SearchResults.Num() == 0)
        {

            // If we're in P2P mode and we can't find a lobby on startup, create one. 
            CreateLobby();

            return; 
        }

        UE_LOG(LogTemp, Log, TEXT("Found lobby."));
 
        for (auto SessionInSearchResult : Search->SearchResults)
        {
            // Typically you want to check if the session is valid before joining. There is a bug in the EOS OSS where IsValid() returns false when the session is created on a DS. 
            // Instead of customizing the engine for this tutorial, we're simply not checking if the session is valid. The code below should go in this if statement once the bug is fixed. 
            /*
            if (SessionInSearchResult.IsValid()) 
            {

                
            }
            */
           
            //Ensure the connection string is resolvable and store the info in ConnectString and in SessionToJoin
            if (Session->GetResolvedConnectString(SessionInSearchResult, NAME_GamePort, ConnectString))
            {
                SessionToJoin = &SessionInSearchResult; 
            }

            // For this course we will join the first session found automatically. Usually you would loop through all the sessions and determine which one is best to join. 
            break;            
        }
        JoinSession();  
    }
    else
    {

        UE_LOG(LogTemp, Log, TEXT("Find lobby failed."));

    }

    // Clear our handle and reset the delegate. 
    Session->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
    FindSessionsDelegateHandle.Reset();
}

void AZL_EOS_PlayerController::JoinSession()
{
    // Tutorial 4: Join the session.
    // Tutorial 7: Same code is used to join the lobby - just some tweaks to the logging 

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();

    JoinSessionDelegateHandle = 
        Session->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(
            this,
            &ThisClass::HandleJoinSessionCompleted));

    UE_LOG(LogTemp, Log, TEXT("Joining Lobby."));

    if (!Session->JoinSession(0, "SessionName", *SessionToJoin))
    {

        UE_LOG(LogTemp, Log, TEXT("Join Lobby failed."));


        // Clear our handle and reset the delegate. 
        Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
        JoinSessionDelegateHandle.Reset();
    } 
}

void AZL_EOS_PlayerController::HandleJoinSessionCompleted(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    // Tutorial 4: This function is triggered via the callback we set in JoinSession once the session is joined (or there is a failure)

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();

    if (Result == EOnJoinSessionCompleteResult::Success)
    {
        UE_LOG(LogTemp, Log, TEXT("Joined lobby."));
        ClientTravel(ConnectString, TRAVEL_Absolute);
        SetupNotifications(); // Setup our listeners for lobby event notifications
    }


    // Clear our handle and reset the delegate. 
    Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
    JoinSessionDelegateHandle.Reset();
}

void AZL_EOS_PlayerController::LoadTitleData()
{
    /*
    Tutorial 6: This function is triggered by the login callback once the player has logged in (client only).
    Files must first be enumerated before they can be read. In this course we will read from the 1st file. 
    */ 

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineTitleFilePtr TitleFile = Subsystem->GetTitleFileInterface(); 
    
    EnumTitleFilesDelegateHandle = TitleFile->AddOnEnumerateFilesCompleteDelegate_Handle(FOnEnumerateFilesCompleteDelegate::CreateUObject(
        this,
        &ThisClass::HandleEnumTitleFilesCompleted
    ));

    if (!TitleFile->EnumerateFiles())
    {
        UE_LOG(LogTemp, Error, TEXT("Error enumerating title storage files."));

        // Clear our handle and reset the delegate. 
        TitleFile->ClearOnEnumerateFilesCompleteDelegate_Handle(EnumTitleFilesDelegateHandle);
        EnumTitleFilesDelegateHandle.Reset();
    }
}

void AZL_EOS_PlayerController::HandleEnumTitleFilesCompleted(bool bWasSuccessfull, const FString& Error)
{
    // Tutorial 6: Callback function for enumerating title data storage files. 
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineTitleFilePtr TitleFile = Subsystem->GetTitleFileInterface();

    if (bWasSuccessfull)
    {
        // Set an array of files we can populate
        TArray<FCloudFileHeader> TitleFiles;
        TitleFile->GetFileList(TitleFiles);

        // Get the the filenames from our array of files
        TArray<FString> TitleFileNames; 
        for (const auto& File : TitleFiles)
        {
            // Add all the title storage file names to array 
            TitleFileNames.Add(File.FileName); 
        }

        ReadTitleFileDelegateHandle = TitleFile->AddOnReadFileCompleteDelegate_Handle(FOnReadFileCompleteDelegate::CreateUObject(
            this,
            &ThisClass::HandleReadTitleFileCompleted
        ));

        // To keep things simple we are only reading the 1st file which is a .txt file. We will output the file content to the logs.  
        if (!TitleFile->ReadFile(TitleFileNames[0]))
        {
            UE_LOG(LogTemp, Error, TEXT("Error reading title storage file %s."), *TitleFileNames[0]);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Error enumerating title storage files. Error is: %s"), *Error); 
    }

    // Clear our handle and reset the delegate. 
    TitleFile->ClearOnEnumerateFilesCompleteDelegate_Handle(EnumTitleFilesDelegateHandle); 
    EnumTitleFilesDelegateHandle.Reset(); 
}

void AZL_EOS_PlayerController::HandleReadTitleFileCompleted(bool bWasSuccessfull, const FString& FileName)
{
    // Tutorial 6: Callback function for reading 1st file in title data storage. 
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineTitleFilePtr TitleFile = Subsystem->GetTitleFileInterface();

    if (bWasSuccessfull)
    {
        TArray<uint8> FileContents; // We need an array to output the serialized title data storage file data.  
        if (TitleFile->GetFileContents(FileName, FileContents))
        {
            // The file data isn't null-terminated, so convert with an explicit length (no raw buffer; game builds have no C++ exceptions).
            const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(FileContents.GetData()), FileContents.Num());
            const FString FileDataAsFString(Converted.Length(), Converted.Get());

            // Check file contents and hardcode log outputs to prevent log injection
            if (FileDataAsFString.Equals("Game data"))
            {
                UE_LOG(LogTemp, Log, TEXT("File contents are: Game data"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to get file contents for file named: %s."), *FileName);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Error reading title storage file %s."), *FileName);
    }
    
    // Clear our handle and reset the delegate. 
    TitleFile->ClearOnReadFileCompleteDelegate_Handle(ReadTitleFileDelegateHandle);
    ReadTitleFileDelegateHandle.Reset();
}

void AZL_EOS_PlayerController::WritePlayerDataStorage(FString FileName, TArray<uint8> FileData)
{
    // Tutorial 6: Function called to save game. This is called when the ESC key is pressed. See the Quit() function in the character class. Only called on Clients. 

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
    IOnlineUserCloudPtr UserCloud = Subsystem->GetUserCloudInterface();

    // Create delagate handle
    WritePlayerDataStorageDelegateHandle =
        UserCloud->AddOnWriteUserFileCompleteDelegate_Handle(
            FOnWriteUserFileCompleteDelegate::CreateUObject(
                this,
                &ThisClass::HandleWritePlayerDataStorageCompleted));

    // Check if player is online before trying to write to player data storage
    FUniqueNetIdPtr NetId = Identity->GetUniquePlayerId(0);

    if (!NetId || Identity->GetLoginStatus(*NetId) != ELoginStatus::LoggedIn)
    {
        UE_LOG(LogTemp, Log , TEXT("Game won't be saved as player isn't logged in."));
        return;
    }

    if (!UserCloud->WriteUserFile(*NetId, FileName,FileData))
    {
        UE_LOG(LogTemp, Error, TEXT("Error writing file with name: %s to player data storage"),*FileName);
        
        // Clear our handle and reset the delegate. 
        UserCloud->ClearOnWriteUserFileCompleteDelegate_Handle(WritePlayerDataStorageDelegateHandle); 
        WritePlayerDataStorageDelegateHandle.Reset();
    }
}

void AZL_EOS_PlayerController::HandleWritePlayerDataStorageCompleted(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName)
{
    // Tutorial 6: callback function when file write has completed
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineUserCloudPtr UserCloud = Subsystem->GetUserCloudInterface();

    if (bWasSuccessful)
    {
        // If saving the game was a success quit
        ConsoleCommand(TEXT("quit"));
    }
    else
    {
        // This means that the game wasn't saved. The game should notify the player and not just quit.
        UE_LOG(LogTemp, Error, TEXT("Error writing file with name: %s to player data storage"), *FileName);
    }

    // Clear our handle and reset the delegate. 
    UserCloud->ClearOnWriteUserFileCompleteDelegate_Handle(WritePlayerDataStorageDelegateHandle);
    WritePlayerDataStorageDelegateHandle.Reset();
}

void AZL_EOS_PlayerController::LoadPlayerData()
{
    /*
    Tutorial 6: This function is triggered by the login callback once the player has logged in (client only).
    Files must first be enumerated before they can be read. In this course we will read from the 1st file only.
   */

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
    IOnlineUserCloudPtr PlayerData = Subsystem->GetUserCloudInterface();

    EnumPlayerFilesDelegateHandle = PlayerData->AddOnEnumerateUserFilesCompleteDelegate_Handle(FOnEnumerateUserFilesCompleteDelegate::CreateUObject(
        this,
        &ThisClass::HandleEnumPlayerFilesCompleted
    ));

    // Check if player is online before trying to write to player data storage
    FUniqueNetIdPtr NetId = Identity->GetUniquePlayerId(0);

    if (!NetId || Identity->GetLoginStatus(*NetId) != ELoginStatus::LoggedIn)
    {
        return;
    }

    PlayerData->EnumerateUserFiles(*NetId);
}

void AZL_EOS_PlayerController::HandleEnumPlayerFilesCompleted(bool bWasSuccessfull, const FUniqueNetId& NetId)
{
    // Tutorial 6: Callback function for enumerating player data storage files.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineUserCloudPtr PlayerData = Subsystem->GetUserCloudInterface();

    if (bWasSuccessfull)
    {
        // Set an array of files we can populate
        TArray<FCloudFileHeader> PlayerFiles;
        PlayerData->GetUserFileList(NetId, PlayerFiles);

        if (PlayerFiles.Num() == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("No player files in player data storage."));
            return; 
        }

        TArray<FString> PlayerFileNames;
        
        for (const auto& File : PlayerFiles)
        {
            // Add all the player data storage file names to array 
            PlayerFileNames.Add(File.FileName);
        }

        ReadPlayerDataFileDelegateHandle = PlayerData->AddOnReadUserFileCompleteDelegate_Handle(FOnReadUserFileCompleteDelegate::CreateUObject(
            this,
            &ThisClass::HandleReadPlayerFileCompleted
        ));

        // To keep things simple we are only reading the 1st file. The player's initial location will change to the location of when the player quit the last instance. 

        if (!PlayerData->ReadUserFile(NetId, PlayerFileNames[0]))
        {
            UE_LOG(LogTemp, Error, TEXT("Error reading player data storage file, filename: %s."), *PlayerFileNames[0]);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Error enumerating player data storage files."));
    }

    PlayerData->ClearOnEnumerateUserFilesCompleteDelegate_Handle(EnumPlayerFilesDelegateHandle);
    EnumPlayerFilesDelegateHandle.Reset();
}

void AZL_EOS_PlayerController::HandleReadPlayerFileCompleted(bool bWasSuccessfull, const FUniqueNetId& UserId, const FString& FileName)
{
    // Tutorial 6: Callback function for reading 1st file in player data storage. 

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineUserCloudPtr PlayerData = Subsystem->GetUserCloudInterface();

    if (bWasSuccessfull)
    {
        TArray<uint8> FileContents;
        if (PlayerData->GetFileContents(UserId, FileName, FileContents))
        {
            // Should use a USaveGame
            //LoadGame(FileContents);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to get file contents for file named: %s."), *FileName);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Error reading player data storage file %s."), *FileName);
    }
    PlayerData->ClearOnReadUserFileCompleteDelegate_Handle(ReadPlayerDataFileDelegateHandle);
    ReadPlayerDataFileDelegateHandle.Reset();
}

// Tutorial 7: This code will only be included if P2PMode is enabled 



void AZL_EOS_PlayerController::CreateLobby(FName KeyName, FString KeyValue) 
{
    // Tutorial 7: Create lobby - this code is similar to creating session, notice that bIsDedicated is false, bUseLobbiesIfAvailable and UseLobbiesVoiceChatIfAvailable is true

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();

    CreateLobbyDelegateHandle =
        Session->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(
            this,
            &ThisClass::HandleCreateLobbyCompleted));

    TSharedRef<FOnlineSessionSettings> SessionSettings = MakeShared<FOnlineSessionSettings>();
    SessionSettings->NumPublicConnections = 2; //We will test our sessions with 2 players to keep things simple
    SessionSettings->bShouldAdvertise = true; //This creates a public match and will be searchable.
    SessionSettings->bUsesPresence = false;   //No presence on dedicated server. This requires a local user.
    SessionSettings->bAllowJoinViaPresence = false;
    SessionSettings->bAllowJoinViaPresenceFriendsOnly = false;
    SessionSettings->bAllowInvites = false;    //Allow inviting players into session. This requires presence and a local user. 
    SessionSettings->bAllowJoinInProgress = false; //Once the session is started, no one can join.
    SessionSettings->bIsDedicated = false; //Session created on dedicated server.
    SessionSettings->bUseLobbiesIfAvailable = true; //For P2P we will use a lobby instead of a session
    SessionSettings->bUseLobbiesVoiceChatIfAvailable = true; //We will also enable voice
    SessionSettings->bUsesStats = true; //Needed to keep track of player stats.
    SessionSettings->Settings.Add(FName("ZeroLockLobbies"),FOnlineSessionSetting(true, EOnlineDataAdvertisementType::ViaOnlineService));
    SessionSettings->Settings.Add(KeyName, FOnlineSessionSetting((KeyValue), EOnlineDataAdvertisementType::ViaOnlineService));

    UE_LOG(LogTemp, Log, TEXT("Creating Lobby..."));

    if (!Session->CreateSession(0, LobbyName, *SessionSettings))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to create Lobby!"));
    }
}

void AZL_EOS_PlayerController::HandleCreateLobbyCompleted(FName EOSLobbyName, bool bWasSuccessful)
{
    // Tutorial 7: Callback function: This is called once our lobby is created

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
    if (bWasSuccessful)
    {
        UE_LOG(LogTemp, Log, TEXT("Lobby: %s Created!"), *EOSLobbyName.ToString());
        FString Map = "Game/Content/ThirdPerson/Maps/ThirdPersonMap?listen"; //Hardcoding map name here, should be passed by parameter
        FURL TravelURL;
        TravelURL.Map = Map;
        GetWorld()->Listen(TravelURL);
        SetupNotifications(); // Setup our listeners for lobby notification events 
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to create lobby!"));
    }

    // Clear our handle and reset the delegate. 
    Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateLobbyDelegateHandle);
    CreateLobbyDelegateHandle.Reset();
}

void AZL_EOS_PlayerController::SetupNotifications()
{
    // Tutorial 7: EOS Lobbies are great as there are notifications sent for our backend when there are changes to lobbies (ex: Participant Joins/Leaves, lobby or lobby member data is updated, etc...) 
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
    
    // In this tutorial we're only giving an example of a notification for when a participant joins/leaves the lobby. The approach is similar for other notifications. 
   // Session->AddOnSessionParticipantsChangeDelegate_Handle(FOnSessionParticipantsChangeDelegate::CreateUObject(
     //   this,
     //   &ThisClass::HandleParticipantChanged)); 
}

void AZL_EOS_PlayerController::HandleParticipantChanged(FName EOSLobbyName, const FUniqueNetId& NetId, bool bJoined)
{
    // Tutorial 7: Callback function called when participants join/leave. 
    if (bJoined)
    {
        UE_LOG(LogTemp, Log, TEXT("A player has joined Lobby: %s"), *LobbyName.ToString()); 
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("A player has left Lobby: %s"), *LobbyName.ToString());
    }
}
