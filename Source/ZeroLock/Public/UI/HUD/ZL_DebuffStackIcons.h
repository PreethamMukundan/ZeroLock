// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Layout/Margin.h"
#include "Types/SlateEnums.h"
#include "ZL_DebuffStackIcons.generated.h"

class AZeroLockCharacter;
class UHorizontalBox;
class UImage;
class UOverlay;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UZL_VM_DebuffStack;

/** How one debuff stack tag looks: a circle in Color with Letter in it. */
USTRUCT(BlueprintType)
struct ZEROLOCK_API FZL_DebuffStackStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debuff")
	FText Letter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debuff")
	FLinearColor Color = FLinearColor(0.32f, 0.32f, 0.34f, 0.95f);
};

/**
 * A row of debuff stack icons (GAS/ZL_DebuffStacks.h), one per tag, built from widgets in C++ (no designer tree): a round
 * image with the tag's letter, the stack count in a small badge at its bottom right, and a thin progress bar under it for
 * the time left before the stacks fade. Reads the hero's UZL_VM_DebuffStacks (its ASC) every tick and reuses the icons.
 * Hero = SetHero, else the owning player's pawn. bOnlyLocalInstigator: just the stacks the local player applied (overhead
 * bars). Icons shrink to fit the width they get; height comes from the parent (put it in a SizeBox).
 * At design time it draws two sample icons.
 */
UCLASS()
class ZEROLOCK_API UZL_DebuffStackIcons : public UUserWidget
{
	GENERATED_BODY()

public:
	UZL_DebuffStackIcons(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Debuff")
	void SetHero(AZeroLockCharacter* InHero);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debuff")
	bool bOnlyLocalInstigator = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debuff")
	TEnumAsByte<EHorizontalAlignment> IconAlignment = HAlign_Left;

	/** Circle diameter; shrinks when the row doesn't fit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debuff")
	float IconSize = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debuff")
	float IconSpacing = 4.f;

	/** Time-left bar under each circle; 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debuff")
	float TimeBarHeight = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debuff")
	bool bShowStackCount = true;

	/** Per tag; a missing tag gets DefaultStyle with the first letter of its last name segment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debuff", meta = (Categories = "Zerolock.DebuffStack"))
	TMap<FGameplayTag, FZL_DebuffStackStyle> Styles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debuff")
	FZL_DebuffStackStyle DefaultStyle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debuff")
	FLinearColor OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.7f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debuff")
	FLinearColor TextColor = FLinearColor(0.95f, 0.95f, 0.95f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debuff")
	FLinearColor BadgeColor = FLinearColor(0.05f, 0.05f, 0.06f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debuff")
	FLinearColor TimeBarColor = FLinearColor(0.85f, 0.9f, 0.95f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debuff")
	FLinearColor TimeBarBackColor = FLinearColor(0.f, 0.f, 0.f, 0.55f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FIcon
	{
		FGameplayTag Tag;
		int32 Stacks = 0;
		float Remaining = 1.f;
	};
	struct FIconWidgets
	{
		TObjectPtr<USizeBox> Root;
		TObjectPtr<USizeBox> CircleBox;
		TObjectPtr<UImage> Circle;
		TObjectPtr<UTextBlock> Letter;
		TObjectPtr<USizeBox> BadgeBox;
		TObjectPtr<UImage> Badge;
		TObjectPtr<UTextBlock> Count;
		TObjectPtr<USizeBox> BarBox;
		TObjectPtr<UProgressBar> Bar;
	};
	void GatherIcons(TArray<FIcon>& Out) const;
	const FZL_DebuffStackStyle& GetStyle(const FGameplayTag& Tag, FText& OutLetter) const;
	void BuildRoot();
	FIconWidgets& GetIconWidgets(int32 Index);
	void Refresh(const FVector2D& Area);

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> Row;

	/** Keeps the pooled icon widgets alive (FIconWidgets isn't reflected). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> PoolRoots;

	TArray<FIconWidgets> Pool;
	float AppliedSize = -1.f;

	TWeakObjectPtr<AZeroLockCharacter> Hero;
};
