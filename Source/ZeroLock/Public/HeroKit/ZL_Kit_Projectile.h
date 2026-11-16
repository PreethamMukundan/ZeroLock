// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Weapon/Projectile.h"
#include "HeroKit/ZL_KitTypes.h"
#include "HeroKit/ZL_Kit_Zone.h"
#include "ZL_Kit_Projectile.generated.h"

class AZeroLockCharacter;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/** What a kit projectile does when it lands. The ability fills it in on the server copy (Arm). */
USTRUCT(BlueprintType)
struct ZEROLOCK_API FZL_KitProjectileHit
{
	GENERATED_BODY()

	/** On the hero it hits (HitTeam). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FZL_KitPayload DirectPayload;

	/** On every enemy within AreaRadius where it detonates. 0 = no explosion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AreaRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FZL_KitPayload AreaPayload;

	/** On every ally within AreaRadius where it detonates (heal bombs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FZL_KitPayload AreaAllyPayload;

	/** Left on the floor where it detonates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FZL_KitZoneSpec Zone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AZL_Kit_Zone> ZoneClass;
};

/**
 * Hero kit projectile: client-predicted (AProjectile), configured in a BP child: speed, gravity, bounces
 * (ProjectileMovement + bLimitBounces/MaximumBounces), life, hitbox size, placeholder mesh and colour, and the flags below.
 * Damage and effects are server-only, from the FZL_KitProjectileHit the ability arms it with.
 */
UCLASS()
class ZEROLOCK_API AZL_Kit_Projectile : public AProjectile
{
	GENERATED_BODY()

public:
	AZL_Kit_Projectile(const FObjectInitializer& ObjectInitializer);

	/** Server copy: what it does. Power and RangeScale come from the ability. Key groups its zones (MaxPerCaster). */
	void Arm(const FZL_KitProjectileHit& InHit, float InPower, float InRangeScale, FName InZoneKey);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
	virtual void OnStop(const FHitResult& Hit) override;
	virtual void LifeSpanExpired() override;

	/** Who it hits. Others fly through. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	EZL_KitTeam HitTeam = EZL_KitTeam::Enemies;

	/** Flies through heroes, hitting each once (per leg when it returns). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	bool bPierce = false;

	/** Flies back to the caster after this many seconds, or when it hits a wall first. 0 = never. Set InitialLifeSpan long
	 *  enough for the way back. Returning, it passes through walls. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	float ReturnAfter = 0.f;

	/** Flies through walls (the root ignores WorldStatic); heroes still hit it. Pair with bPierce for a wall-piercing beam. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	bool bPassThroughWalls = false;

	/** Guided: turns toward the caster's crosshair point every frame, so the caster steers it by aiming. Each copy works
	 *  the aim out itself (owner: camera; server: the replicated look direction; other players: the pawn's aim), so
	 *  other players' copies may drift a little; damage is the server's. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Steer")
	bool bSteerToCrosshair = false;

	/** Degrees per second it can turn. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Steer", meta = (EditCondition = "bSteerToCrosshair"))
	float SteerTurnRate = 240.f;

	/** Seconds of straight flight before it starts steering. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Steer", meta = (EditCondition = "bSteerToCrosshair"))
	float SteerDelay = 0.1f;

	/** How far the crosshair trace looks for the point to steer to (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Steer", meta = (EditCondition = "bSteerToCrosshair"))
	float SteerRange = 8000.f;

	/** Leaves its zone / explosion at the floor under the impact instead of the impact point. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	bool bZoneOnFloor = true;

	/** With bPierce: also bursts (AreaPayload + Zone, like the final detonation) on each hero it passes through,
	 *  centred on that hero. The ball keeps flying and still bursts where it stops. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit", meta = (EditCondition = "bPierce"))
	bool bBurstOnPierceHit = false;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Kit")
	TObjectPtr<UStaticMeshComponent> PlaceholderMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FLinearColor PlaceholderColor = FLinearColor(1.f, 0.5f, 0.1f, 1.f);

	/** Inner sphere inside the placeholder (scaled by it): shown only when CoreMaterial is set. Gives a see-through
	 *  shell material (Tempest's wind ball) a solid-reading core with a rim. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Kit|Core")
	TObjectPtr<UStaticMeshComponent> CoreMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|Core")
	TObjectPtr<UMaterialInterface> CoreMaterial;

	/** Relative to the placeholder sphere. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Core", meta = (EditCondition = "CoreMaterial != nullptr"))
	float CoreScale = 0.8f;

	/** The core material's "Color" parameter. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Core", meta = (EditCondition = "CoreMaterial != nullptr"))
	FLinearColor CoreColor = FLinearColor(1.f, 1.f, 1.f, 1.f);

	/** Hides the placeholder sphere (when the trail cue, AProjectile::TrailCue, is the look). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	bool bHidePlaceholder = false;

	/** Cue where it detonates (server, through the caster's ASC); radius = its explosion radius. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|FX")
	FZL_KitCue ImpactCue;

	/** Draws the explosion on the server. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	bool bDrawDebug = true;

private:
	bool ShouldHit(AActor* OtherActor) const;
	void Explode(const FVector& Location);
	/** Area hit + zone at Location (Explode's work, without the once-only guard or the impact cue). */
	void Burst(AZeroLockCharacter* Hero, const FVector& Location);
	void StartReturn();
	void Steer(float DeltaSeconds);

	FZL_KitProjectileHit HitData;
	float Power = 1.f;
	float RangeScale = 1.f;
	FName ZoneKey;
	bool bExploded = false;
	bool bReturning = false;
	float Age = 0.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> AlreadyHit;
};
