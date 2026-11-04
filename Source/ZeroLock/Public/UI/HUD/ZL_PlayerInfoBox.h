//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "ZL_PlayerInfoBox.generated.h"

class UZL_VM_Attributes;
class UProgressBar;
class UZL_VM_PlayerInfo;
class AZero_BasePlayerState;
class UZL_Character_Data_Asset;
class UImage;
class UBorder;
class UCommonTextBlock;
class UTexture2D;

/**
 * One player card in the top team bar (UZL_GameState_Bar).
 * Used as a plain C++ class (no designer tree) it builds its own card: hero portrait in a team-coloured frame,
 * a health bar exactly as wide as the portrait, and the player name. A widget BP subclass may supply its own
 * layout instead; all widgets are optional and found by name.
 * Every tick it reads the player's live pawn: hero icon (DT_HeroList data asset matching the pawn class,
 * re-resolved on hero swap), health from the pawn's ASC (replicated, so it works for every player),
 * dead state, name and K/D/A from the PlayerState.
 */
UCLASS()
class ZEROLOCK_API UZL_PlayerInfoBox : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "ICON", meta = (BindWidgetOptional))
	TObjectPtr<UImage> PlayerIcon;

	UPROPERTY(BlueprintReadOnly, Category = "ICON", meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> PlayerName;

	/** Team-coloured frame around the portrait. */
	UPROPERTY(BlueprintReadOnly, Category = "ICON", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PortraitFrame;

	/** Fill behind the hero icon, tinted with the hero's IconColor. */
	UPROPERTY(BlueprintReadOnly, Category = "ICON", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PortraitBackground;

	UPROPERTY(BlueprintReadOnly, Category = "Stat", meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Kill;
	UPROPERTY(BlueprintReadOnly, Category = "Stat", meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Death;
	UPROPERTY(BlueprintReadOnly, Category = "Stat", meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Assist;

	UPROPERTY(BlueprintReadOnly, Category = "Stat", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	void SetViewModel(UZL_VM_PlayerInfo* ViewModel);

	UFUNCTION(BlueprintCallable)
	void SetAttributeVM(UZL_VM_Attributes* AtVM);

	/** Ally or enemy colours. Set by the team bar. */
	UFUNCTION(BlueprintCallable, Category = "Team")
	void SetIsAlly(bool bInIsAlly);

	UPROPERTY()
	UZL_VM_PlayerInfo* myVM;

	UPROPERTY()
	UZL_VM_Attributes* myAttributes;

	// --- Built card layout (only used without a designer tree) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	float PortraitSize = 52.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	float FrameThickness = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	float HealthBarHeight = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	float CornerRadius = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	int32 NameFontSize = 9;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	bool bShowName = true;

	// --- Colours ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor AllyColor = FLinearColor(0.20f, 0.62f, 1.00f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor EnemyColor = FLinearColor(0.95f, 0.22f, 0.20f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor AllyHealthColor = FLinearColor(0.30f, 0.85f, 0.35f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor EnemyHealthColor = FLinearColor(0.92f, 0.25f, 0.22f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor HealthBackgroundColor = FLinearColor(0.f, 0.f, 0.f, 0.65f);

	/** Frame and icon tint while the player is dead / has no pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor DeadColor = FLinearColor(0.28f, 0.28f, 0.28f, 1.f);

	/** Portrait fill when the hero has no data asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor DefaultPortraitColor = FLinearColor(0.06f, 0.06f, 0.07f, 0.9f);

	/** Shown when the pawn's class isn't in DT_HeroList (or its Icon is empty). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	TObjectPtr<UTexture2D> DefaultHeroIcon;

	/** The DT_HeroList entry whose CharacterClass is the closest parent of PawnClass. */
	static UZL_Character_Data_Asset* FindHeroData(const UObject* WorldContext, const UClass* PawnClass);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildCard();
	void Refresh();
	void ApplyHero(const UZL_Character_Data_Asset* HeroData);
	void ApplyColors();

	FLinearColor HeroPortraitColor = FLinearColor::Black;
	bool bIsAlly = true;
	bool bShownDead = false;
	bool bHasShownHero = false;
	TWeakObjectPtr<const UClass> ShownPawnClass;
	float ShownHealthPercent = -1.f;
	FString ShownName;
	FIntVector ShownKDA = FIntVector(-1);
};
