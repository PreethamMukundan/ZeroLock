// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "HeroKit/ZL_KitCue.h"
#include "Weapon/Projectile.h"
#include "ZL_Tempest_WindBall.generated.h"

class AZeroLockCharacter;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * Tempest's wind ball (Alley-oop). An engine sphere shell (ShellMesh, BP material e.g. MI_HeroKit_WindBall, "Color"
 * parameter = ShellColor) around a core sphere (CoreMesh, e.g. MI_HeroKit_EnergyCore_Wind, "Color" = CoreColor).
 * With bSteerToCrosshair it turns toward the caster's crosshair point at SteerTurnRate after SteerDelay. Each copy works
 * the point out itself (owner: camera trace; server: the Mover input's LookDir; other clients: base aim rotation), so
 * proxies can drift a little; hits are the server's.
 * The first enemy, a wall or the end of its life detonates it: the server copy bursts (BurstDamage spirit to enemies in
 * BurstRadius, ImpactCue).
 */
UCLASS()
class ZEROLOCK_API AZL_Tempest_WindBall : public AProjectile
{
	GENERATED_BODY()

public:
	AZL_Tempest_WindBall(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server: the ability arms its copy after the spawn. */
	void InitWindBall(float InBurstDamage, float InBurstRadius, const UObject* InDamageSource);

	/** Where Hero's crosshair points, as well as this machine can tell. */
	static FVector GetCrosshairPoint(const AZeroLockCharacter* Hero, float Range);

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Wind Ball")
	TObjectPtr<UStaticMeshComponent> ShellMesh;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Wind Ball")
	TObjectPtr<UStaticMeshComponent> CoreMesh;

	/** Shell size as a fraction of the 100 cm engine sphere. */
	UPROPERTY(EditDefaultsOnly, Category = "Wind Ball")
	float ShellScale = 0.7f;

	UPROPERTY(EditDefaultsOnly, Category = "Wind Ball")
	FLinearColor ShellColor = FLinearColor(2.2f, 5.0f, 3.5f, 1.0f);

	/** Core size relative to the shell. */
	UPROPERTY(EditDefaultsOnly, Category = "Wind Ball")
	float CoreScale = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "Wind Ball")
	FLinearColor CoreColor = FLinearColor(0.5f, 1.6f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Wind Ball|Steer")
	bool bSteerToCrosshair = true;

	/** Degrees per second. */
	UPROPERTY(EditDefaultsOnly, Category = "Wind Ball|Steer", meta = (EditCondition = "bSteerToCrosshair"))
	float SteerTurnRate = 240.0f;

	/** Seconds of straight flight before it starts turning. */
	UPROPERTY(EditDefaultsOnly, Category = "Wind Ball|Steer", meta = (EditCondition = "bSteerToCrosshair"))
	float SteerDelay = 0.1f;

	/** How far the crosshair trace reaches. */
	UPROPERTY(EditDefaultsOnly, Category = "Wind Ball|Steer", meta = (EditCondition = "bSteerToCrosshair"))
	float SteerRange = 8000.0f;

	/** Fallbacks until the ability arms the projectile. */
	UPROPERTY(EditDefaultsOnly, Category = "Wind Ball|Burst")
	float BurstDamage = 75.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Wind Ball|Burst")
	float BurstRadius = 300.0f;

	/** At the burst (server, on the caster's ASC). Radius = BurstRadius. */
	UPROPERTY(EditDefaultsOnly, Category = "Wind Ball|Burst")
	FZL_KitCue ImpactCue;

	/** Draws the burst sphere on the server. */
	UPROPERTY(EditDefaultsOnly, Category = "Wind Ball|Burst")
	bool bDrawDebug = false;

	virtual void OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
	virtual void OnStop(const FHitResult& Hit) override;
	virtual void LifeSpanExpired() override;

	/** Server copy only: the ball's area effect at Location. Once per ball unless a subclass says otherwise. */
	virtual void Burst(const FVector& Location);

	/** Enemies within Radius of Center (Hero object type, alive, not untouchable). No line of sight. */
	void FindVillans(const FVector& Center, float Radius, TArray<AZeroLockCharacter*>& OutVillans) const;

	bool IsServerCopy() const { return HasAuthority() && !bIsFakeProjectile; }

	const UObject* GetDamageSource() const { return DamageSource.IsValid() ? DamageSource.Get() : this; }

	TWeakObjectPtr<const UObject> DamageSource;
	bool bBurst = false;

private:
	void Steer(float DeltaSeconds);

	float Age = 0.0f;
};
