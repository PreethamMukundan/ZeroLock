// Copyright Preetham Mukundan (C) 2026


#include "Solomon/ZL_Solomon_PieceSelector.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Solomon/ZL_Solomon_ChessPiece.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Solomon_PieceSelector::UZL_Solomon_PieceSelector()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(false);
}

UZL_Solomon_PieceSelector* UZL_Solomon_PieceSelector::Get(AActor* Hero)
{
	if (!Hero)
	{
		return nullptr;
	}
	UZL_Solomon_PieceSelector* Selector = Hero->FindComponentByClass<UZL_Solomon_PieceSelector>();
	if (!Selector)
	{
		Selector = NewObject<UZL_Solomon_PieceSelector>(Hero, TEXT("PieceSelector"));
		Selector->RegisterComponent();
	}
	return Selector;
}

AZL_Solomon_ChessPiece* UZL_Solomon_PieceSelector::FindPieceToMove() const
{
	const APawn* Hero = Cast<APawn>(GetOwner());
	const APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	if (!PC || !PC->PlayerCameraManager)
	{
		return nullptr;
	}
	const TArray<AZL_Solomon_ChessPiece*> Pieces = AZL_Solomon_ChessPiece::GetPiecesOf(Hero);
	if (bOneMoveAtATime && Pieces.ContainsByPredicate([](const AZL_Solomon_ChessPiece* Piece) { return Piece->IsMoving(); }))
	{
		return nullptr;
	}

	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	const FVector AimDir = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(SelectMaxAngle));

	AZL_Solomon_ChessPiece* Best = nullptr;
	float BestDot = -1.0f;
	// His king rides on him: the fallback when nothing else is aimed at.
	AZL_Solomon_ChessPiece* OnSelf = nullptr;
	for (AZL_Solomon_ChessPiece* Piece : Pieces)
	{
		if (!Piece->CanMove() || FVector::DistSquared(Hero->GetActorLocation(), Piece->GetActorLocation()) > FMath::Square(SelectRange))
		{
			continue;
		}
		if (Piece->GetCarriedHero() == Hero)
		{
			OnSelf = Piece;
			continue;
		}
		// Aim at the middle of the piece, not its floor pivot.
		const FVector ToPiece = Piece->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f) - CameraLocation;
		const float Along = FVector::DotProduct(ToPiece, AimDir);
		if (Along <= 0.0f)
		{
			continue;
		}
		const float Dot = Along / ToPiece.Size();
		const float LineDistance = (ToPiece - AimDir * Along).Size();
		if ((Dot >= MinDot || LineDistance <= SelectRadius) && Dot > BestDot)
		{
			Best = Piece;
			BestDot = Dot;
		}
	}
	return Best ? Best : OnSelf;
}

void UZL_Solomon_PieceSelector::LockSelection(AZL_Solomon_ChessPiece* Piece)
{
	if (AZL_Solomon_ChessPiece* Previous = Locked.Get())
	{
		Previous->SetHighlight(EZL_PieceHighlight::None);
	}
	Locked = Piece;
	if (Piece)
	{
		SetHovered(nullptr);
		Piece->SetHighlight(EZL_PieceHighlight::Selected);
	}
}

void UZL_Solomon_PieceSelector::SetHovered(AZL_Solomon_ChessPiece* Piece)
{
	if (Hovered.Get() == Piece)
	{
		return;
	}
	if (AZL_Solomon_ChessPiece* Previous = Hovered.Get())
	{
		if (Previous != Locked.Get())
		{
			Previous->SetHighlight(EZL_PieceHighlight::None);
		}
	}
	Hovered = Piece;
	if (Piece && Piece != Locked.Get())
	{
		Piece->SetHighlight(EZL_PieceHighlight::Hover);
	}
}

void UZL_Solomon_PieceSelector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Hero = Cast<APawn>(GetOwner());
	if (!Hero || !Hero->IsLocallyControlled())
	{
		return;
	}
	SetHovered(Locked.IsValid() ? nullptr : FindPieceToMove());
}

void UZL_Solomon_PieceSelector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetHovered(nullptr);
	LockSelection(nullptr);
	Super::EndPlay(EndPlayReason);
}
