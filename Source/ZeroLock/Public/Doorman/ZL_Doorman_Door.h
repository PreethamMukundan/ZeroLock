// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapon/ZL_ProjectilePortal.h"
#include "ZL_Doorman_Door.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneCaptureComponent2D;
class UStaticMesh;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

/**
 * One of Doorman's two Doorway doors. Server-spawned and replicated; the ability links the pair (LinkedDoor).
 * Actor X = out of the wall, Z = up. Once linked:
 * - Players: each machine checks its locally controlled pawns; one walking into the door is teleported out of the
 *   linked door through the Mover input stream (RequestSafeTeleportWithVelocity), velocity and view yaw turned.
 * - Projectiles: PortalSurface (thin WorldStatic box, blocks WorldDynamic only) is an IZL_ProjectilePortal, so
 *   UZL_ProjectileMovementComponent moves projectiles that hit it to the linked door.
 * - View: on clients a scene capture renders the view out of the linked door into a render target shown on PortalMesh
 *   (PortalMaterial with a "PortalTexture" texture parameter sampled in screen space, "Open" 0/1).
 */
UCLASS()
class ZEROLOCK_API AZL_Doorman_Door : public AActor, public IZL_ProjectilePortal
{
	GENERATED_BODY()

public:
	AZL_Doorman_Door();

	/** Server: link two doors to each other. */
	static void LinkDoors(AZL_Doorman_Door* A, AZL_Doorman_Door* B);

	AZL_Doorman_Door* GetLinkedDoor() const { return LinkedDoor; }

	/** Mirror a world location / direction through this door to the linked door (rotate 180 about up). */
	FVector MirrorLocation(const FVector& WorldLocation) const;
	FVector MirrorVector(const FVector& WorldVector) const;
	FQuat MirrorRotation(const FQuat& WorldRotation) const;

	virtual bool GetPortalExit(const FHitResult& Hit, const FVector& Velocity, float ProjectileRadius, FVector& OutLocation, FVector& OutVelocity) const override;

	/** The see-through part of the door (not the frame). */
	bool IsPortalSurface(const UPrimitiveComponent* Component) const;

	float GetDoorWidth() const { return DoorWidth; }
	float GetDoorHeight() const { return DoorHeight; }

	/** Space the door and its frame take up, in door space (X out of the wall from 0, Z = 0 at the opening's center). */
	FBox GetLocalBounds() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Shows the view out of the linked door. A plane facing +X (set its mesh in the BP, default engine Plane). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<UStaticMeshComponent> PortalMesh;

	/** Projectiles hitting this go through (IZL_ProjectilePortal). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<UBoxComponent> PortalSurface;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<USceneCaptureComponent2D> PortalCapture;

	/** Door frame: two jambs and a lintel around the opening (engine Cube, sized in OnConstruction). Solid. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Frame")
	TObjectPtr<UStaticMeshComponent> FrameLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Frame")
	TObjectPtr<UStaticMeshComponent> FrameRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Frame")
	TObjectPtr<UStaticMeshComponent> FrameTop;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Frame")
	TObjectPtr<UStaticMeshComponent> FrameButtom;

	UPROPERTY(ReplicatedUsing = OnRep_LinkedDoor, BlueprintReadOnly, Category = "Door")
	TObjectPtr<AZL_Doorman_Door> LinkedDoor;

	UFUNCTION()
	void OnRep_LinkedDoor();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door", meta = (ClampMin = "10.0"))
	float DoorWidth = 140.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door", meta = (ClampMin = "10.0"))
	float DoorHeight = 230.0f;

	/** Width of the jambs and height of the lintel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|Frame", meta = (ClampMin = "1.0"))
	float FrameThickness = 15.0f;

	/** How far the frame sticks out of the wall. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|Frame", meta = (ClampMin = "1.0"))
	float FrameDepth = 20.0f;

	/** Applied to all frame pieces (none = the cube's default material). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|Frame")
	TObjectPtr<UMaterialInterface> FrameMaterial;

	/** Local pawns whose center gets this close to the door plane (and inside its outline) go through. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|Travel")
	float PawnTriggerDepth = 55.0f;

	/** Distance in front of the exit door a pawn arrives at. Must be more than PawnTriggerDepth. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|Travel")
	float PawnExitOffset = 90.0f;

	/** Minimum speed out of the exit door, so players don't stall in the doorway. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|Travel")
	float MinPawnExitSpeed = 300.0f;

	/** Seconds before the same pawn can use this door again. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|Travel")
	float PawnTravelCooldown = 0.3f;

	/** Distance in front of the exit door a projectile arrives at, plus its collision radius. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|Travel")
	float ProjectileExitOffset = 10.0f;

	/** Unlit material: emissive = "PortalTexture" sampled with screen UVs, lerped by "Open". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|View")
	TObjectPtr<UMaterialInterface> PortalMaterial;

	/** Render target size relative to the viewport. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|View", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float RenderTargetScale = 0.5f;

	/** Beyond this camera distance the door shows its closed color instead of rendering the view (0 = no limit). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|View", meta = (ClampMin = "0.0"))
	float MaxCaptureDistance = 5000.0f;

	/** Render dynamic shadows in the portal view. Costly with virtual shadow maps (no caching for captures). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|View")
	bool bCaptureShadows = false;

	/** Whether the portal view is shown (camera close enough). Drives "Open" with IsLinked. */
	bool bViewInRange = true;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PortalRenderTarget;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PortalMID;

	TMap<TWeakObjectPtr<APawn>, double> LastTravelTime;

	bool IsLinked() const { return IsValid(LinkedDoor); }
	void UpdateOpenState();
	void SetupPortalView();
	UStaticMeshComponent* CreateFramePiece(FName Name, UStaticMesh* Mesh);
	void UpdatePortalView();
	void CheckLocalPawnTravel();
	void TravelPawn(APawn* Pawn, const FVector& LocalLocation);
};
