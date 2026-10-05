// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZL_Pocket_SuitcaseActor.generated.h"

class UStaticMeshComponent;

/**
 * Placeholder suitcase shown while Pocket is inside the Enchanter's Satchel.
 * Attaches itself to its owner (the hero) and hides the hero's mesh for as long as it exists.
 *
 * The server's copy replicates to everyone except the owning client, who spawns a local copy
 * so the swap is instant for them.
 */
UCLASS()
class ZEROLOCK_API AZL_Pocket_SuitcaseActor : public AActor
{
	GENERATED_BODY()

public:
	AZL_Pocket_SuitcaseActor();

	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
	virtual void OnRep_Owner() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Suitcase")
	TObjectPtr<UStaticMeshComponent> SuitcaseMesh;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** The hero we're attached to and whose mesh we hid. */
	TWeakObjectPtr<APawn> DisguisedHero;

	void DisguiseOwner();
	void RevealOwner();
};
