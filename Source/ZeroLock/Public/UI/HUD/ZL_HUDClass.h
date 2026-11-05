//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "InputCoreTypes.h"
#include "ZL_HUDClass.generated.h"

/**
 * 
 */
 class UZL_BaseUILayout;
class UCommonActivatableWidget;
class UInputAction;
class UInputMappingContext;
class AZL_CharacterSelector_Actor;
UCLASS()
class ZEROLOCK_API AZL_HUDClass : public AHUD
{
	GENERATED_BODY()
	
	public:
    	UPROPERTY(EditDefaultsOnly)
    	TSubclassOf<UZL_BaseUILayout> BaseUILayer;
    
    	UPROPERTY(BlueprintReadWrite, Category="RootUILayer")
    	UZL_BaseUILayout* RootLayer;
    
    	UFUNCTION(BlueprintCallable)
    	UZL_BaseUILayout* ReturnRootLayer(); 
    
    	virtual void BeginPlay() override;


	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UCommonActivatableWidget> PlayerInGameHUD;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UCommonActivatableWidget> SettingsMenu;

	/** In-game hero select (hero swap). Pushed onto UI.Layer.Menu. Should be a UZL_HeroSelectionScreen with bSwapHeroOnClick (WBP_HeroSwap). */
	UPROPERTY(EditDefaultsOnly, Category="HeroSwap")
	TSubclassOf<UCommonActivatableWidget> HeroSelectWidgetClass;

	/** 3D preview rendering into the hero select render target. Spawned locally while the screen is open. */
	UPROPERTY(EditDefaultsOnly, Category="HeroSwap")
	TSubclassOf<AZL_CharacterSelector_Actor> HeroSelectPreviewClass;

	/** World location for the preview actor. Keep it out of sight. */
	UPROPERTY(EditDefaultsOnly, Category="HeroSwap")
	FVector HeroSelectPreviewLocation = FVector(0.f, 0.f, -10000.f);

	/** Opens the hero select screen in game. Mapped at runtime into its own mapping context (priority 1); None disables it. */
	UPROPERTY(EditDefaultsOnly, Category="HeroSwap")
	FKey HeroSelectKey = EKeys::H;

	UFUNCTION(BlueprintCallable, Category="HeroSwap")
	void OpenHeroSelect();

	UFUNCTION(BlueprintCallable, Category="HeroSwap")
	void CloseHeroSelect();

	UFUNCTION(BlueprintCallable, Category="HeroSwap")
	void ToggleHeroSelect();

	UFUNCTION(BlueprintPure, Category="HeroSwap")
	bool IsHeroSelectOpen() const { return HeroSelectWidget.IsValid(); }

	/** Dev cheat menu (UZL_CheatComponent on the controller). Pushed onto UI.Layer.Menu; not set up in Shipping. */
	UPROPERTY(EditDefaultsOnly, Category="Cheats")
	TSubclassOf<UCommonActivatableWidget> CheatMenuClass;

	/** Opens / closes the cheat menu (default 0, the number row key). Own runtime mapping context like HeroSelectKey; None disables it. */
	UPROPERTY(EditDefaultsOnly, Category="Cheats")
	FKey CheatMenuKey = EKeys::Zero;

	UFUNCTION(BlueprintCallable, Category="Cheats")
	void ToggleCheatMenu();

	UFUNCTION(BlueprintPure, Category="Cheats")
	bool IsCheatMenuOpen() const { return CheatMenuWidget.IsValid(); }

	/** The team bar cards show K/D/A while this is on. Set by the pawn while IA_UIInfo (Tab) is held. */
	void SetShowingTeamStats(bool bShow) { bShowingTeamStats = bShow; }

	UFUNCTION(BlueprintPure, Category="TeamBar")
	bool IsShowingTeamStats() const { return bShowingTeamStats; }

protected:

	UFUNCTION(BlueprintCallable)
	virtual void PushHUD();

	void RebuildGameHUD(int32 Attempt);
	void RemoveOldHUD(TWeakObjectPtr<UCommonActivatableWidget> OldHUD, int32 Attempt);

	TWeakObjectPtr<APawn> HUDPawn;
	bool bHUDHadPawn = false;
	bool bShowingTeamStats = false;

	/** Polls until there's a pawn: with AZero_BaseGameState::bPickHeroOnJoin, keeps the hero select open (reopens it if closed). */
	void UpdateInitialHeroPick();
	FTimerHandle InitialHeroPickTimer;
	/** The join hero pick was shown, so the first pawn needs its Game-layer HUD built. */
	bool bWaitingForHeroPick = false;

	void SetupHeroSelectInput();
	void HandleHeroSelectDeactivated();

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> HeroSelectAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> HeroSelectMappingContext;

	TWeakObjectPtr<UCommonActivatableWidget> HeroSelectWidget;

	UPROPERTY(Transient)
	TObjectPtr<AZL_CharacterSelector_Actor> HeroSelectPreview;

	void SetupCheatMenuInput();
	void HandleCheatMenuDeactivated();

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CheatMenuAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> CheatMenuMappingContext;

	TWeakObjectPtr<UCommonActivatableWidget> CheatMenuWidget;

public:
	/** Called by AZeroLockCharacter once a locally controlled pawn has its PlayerState. HUD widgets bind to the pawn once at init,
	 *  so a new pawn after the first (hero swap) gets a freshly created Game-layer HUD. */
	void HandleHeroReady(APawn* Hero);

};
