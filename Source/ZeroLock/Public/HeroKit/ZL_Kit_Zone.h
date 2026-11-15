// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeroKit/ZL_KitTypes.h"
#include "ZL_Kit_Zone.generated.h"

class AZeroLockCharacter;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/** What a kit zone does. Set on the ability; the zone copies it at spawn. */
USTRUCT(BlueprintType)
struct ZEROLOCK_API FZL_KitZoneSpec
{
	GENERATED_BODY()

	/** 0 = no zone. Seconds it lasts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Duration = 0.f;

	/** cm. Scaled by the caster's ability range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Radius = 300.f;

	/** Seconds between pulses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Interval = 0.5f;

	/** Each pulse, on every enemy inside (or the strike area). DoTs: keep DotDuration about Interval. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FZL_KitPayload EnemyPayload;

	/** Each pulse, on every ally inside. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FZL_KitPayload AllyPayload;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAllyPayloadOnCaster = true;

	/** Mine: waits ArmDelay, then the first enemy within TriggerRadius sets it off once (EnemyPayload to every enemy in Radius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bTriggerOnce = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bTriggerOnce"))
	float TriggerRadius = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ArmDelay = 0.f;

	/** Each pulse hits one random spot in Radius, StrikeRadius wide, instead of the whole zone (lightning, arrows, spikes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StrikeRadius = 0.f;

	/** Follows the caster (auras, force fields). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bFollowCaster = false;

	/** Oldest of this ability's zones goes when there are more. 0 = no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxPerCaster = 0;

	/** Placeholder disc tint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor Color = FLinearColor(0.2f, 0.6f, 1.0f, 1.0f);

	/** Hides the placeholder disc (when the loop cue is the look). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHideDisc = false;

	/** Looping cue on the zone for its whole life, added locally on every machine; radius = the zone's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FZL_KitCue LoopCue;

	/** Cue at each pulse (strike spot or the whole zone) and when a mine goes off; through the caster's ASC. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FZL_KitCue PulseCue;

	bool IsValid() const { return Duration > 0.f; }
};

/**
 * Hero kit area on the ground: puddles, smoke, mines, rain of arrows, auras. Server-spawned (Spawn), replicated for the
 * placeholder disc; all gameplay is on the server. Instigator = the caster. Replace the look in a BP child (ZoneClass).
 */
UCLASS()
class ZEROLOCK_API AZL_Kit_Zone : public AActor
{
	GENERATED_BODY()

public:
	AZL_Kit_Zone();

	/** Server. Spawns ZoneClass (or this class) at Location, set up from Spec. Key groups zones for MaxPerCaster. */
	static AZL_Kit_Zone* Spawn(TSubclassOf<AZL_Kit_Zone> ZoneClass, AZeroLockCharacter* Caster, const FVector& Location, const FZL_KitZoneSpec& Spec,
		float PowerScale, float RangeScale, FName Key);

	/** Drops Location onto the floor below it (WorldStatic, 2000 cm), or returns it unchanged. */
	static FVector SnapToFloor(const UWorld* World, const FVector& Location);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone")
	TObjectPtr<UStaticMeshComponent> DiscMesh;

	/** Disc thickness, cm. */
	UPROPERTY(EditDefaultsOnly, Category = "Zone")
	float DiscThickness = 4.f;

	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	float VisualRadius = 300.f;

	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	FLinearColor VisualColor = FLinearColor::White;

	UPROPERTY(Replicated)
	bool bFollow = false;

	UPROPERTY(Replicated)
	FGameplayTag VisualLoopCue;

	UPROPERTY(Replicated)
	bool bVisualHideDisc = false;

	UFUNCTION()
	void OnRep_Visual();

	/** Draws each pulse on the server. */
	UPROPERTY(EditDefaultsOnly, Category = "Zone")
	bool bDrawDebug = true;

private:
	void Pulse();
	void CheckTrigger();
	void HitArea(const FVector& Center, float Radius);
	AZeroLockCharacter* GetCaster() const;

	FZL_KitZoneSpec Spec;
	float Power = 1.f;
	float Radius = 300.f;
	FName ZoneKey;
	double SpawnTime = 0.0;
	FTimerHandle PulseTimer;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DiscMaterial;
};
