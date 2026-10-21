// Copyright Preetham Mukundan (C) 2026


#include "Doorman/ZL_Doorman_Door.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Mover/ZeroMoverComponent.h"
#include "Mover/ZeroMoverPawn.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AZL_Doorman_Door::AZL_Doorman_Door()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Engine plane is 100x100 in XY facing +Z; pitch -90 turns its face to +X. Sized in OnConstruction.
	PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
	PortalMesh->SetupAttachment(SceneRoot);
	PortalMesh->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	PortalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PortalMesh->SetCastShadow(false);
	PortalMesh->SetGenerateOverlapEvents(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		PortalMesh->SetStaticMesh(PlaneMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultPortalMaterial(TEXT("/Game/ZERO/HeroAbilities/DoormanTest/Doorway/M_DoorwayPortal.M_DoorwayPortal"));
	if (DefaultPortalMaterial.Succeeded())
	{
		PortalMaterial = DefaultPortalMaterial.Object;
	}

	// Projectile portal: blocks only projectiles (WorldDynamic); players, traces on other channels and pawns pass.
	PortalSurface = CreateDefaultSubobject<UBoxComponent>(TEXT("PortalSurface"));
	PortalSurface->SetupAttachment(SceneRoot);
	PortalSurface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PortalSurface->SetCollisionObjectType(ECC_WorldStatic);
	PortalSurface->SetCollisionResponseToAllChannels(ECR_Ignore);
	PortalSurface->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	PortalSurface->SetGenerateOverlapEvents(false);
	PortalSurface->CanCharacterStepUpOn = ECB_No;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	FrameLeft = CreateFramePiece(TEXT("FrameLeft"), CubeMesh.Object);
	FrameRight = CreateFramePiece(TEXT("FrameRight"), CubeMesh.Object);
	FrameTop = CreateFramePiece(TEXT("FrameTop"), CubeMesh.Object);
	FrameButtom = CreateFramePiece(TEXT("FrameBottom"), CubeMesh.Object);

	PortalCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PortalCapture"));
	PortalCapture->SetupAttachment(SceneRoot);
	PortalCapture->SetUsingAbsoluteLocation(true);
	PortalCapture->SetUsingAbsoluteRotation(true);
	PortalCapture->bCaptureEveryFrame = false;
	PortalCapture->bCaptureOnMovement = false;
	PortalCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	PortalCapture->bOverride_CustomNearClippingPlane = true;

	// The capture is a second scene render per visible door: drop the expensive features. The portal material is
	// unlit and small on screen, so they barely show.
	PortalCapture->bUseRayTracingIfEnabled = false;
	PortalCapture->LODDistanceFactor = 2.0f;
	PortalCapture->ShowFlags.SetLumenGlobalIllumination(false);
	PortalCapture->ShowFlags.SetLumenReflections(false);
	PortalCapture->ShowFlags.SetGlobalIllumination(false);
	PortalCapture->ShowFlags.SetAmbientOcclusion(false);
	PortalCapture->ShowFlags.SetScreenSpaceAO(false);
	PortalCapture->ShowFlags.SetDistanceFieldAO(false);
	PortalCapture->ShowFlags.SetScreenSpaceReflections(false);
	PortalCapture->ShowFlags.SetContactShadows(false);
	PortalCapture->ShowFlags.SetVolumetricFog(false);
	PortalCapture->ShowFlags.SetMotionBlur(false);
	PortalCapture->ShowFlags.SetTemporalAA(false); // FXAA; a capture has no stable TAA history anyway.
	PortalCapture->ShowFlags.SetEyeAdaptation(false);
	PortalCapture->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
	PortalCapture->PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
	PortalCapture->PostProcessSettings.bOverride_ReflectionMethod = true;
	PortalCapture->PostProcessSettings.ReflectionMethod = EReflectionMethod::None;
}

UStaticMeshComponent* AZL_Doorman_Door::CreateFramePiece(FName Name, UStaticMesh* Mesh)
{
	// Solid like the wall it is set in, except to the camera (no spring arm pops when standing next to it).
	UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Piece->SetupAttachment(SceneRoot);
	Piece->SetStaticMesh(Mesh);
	Piece->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Piece->SetCollisionObjectType(ECC_WorldStatic);
	Piece->SetCollisionResponseToAllChannels(ECR_Block);
	Piece->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Piece->SetGenerateOverlapEvents(false);
	Piece->CanCharacterStepUpOn = ECB_No;
	return Piece;
}

void AZL_Doorman_Door::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Frame around the opening, from the wall (X = 0) out to FrameDepth; the opening's bottom (-DoorHeight/2) is the
	// floor. Engine Cube is 100 cm, centered.
	const float HalfW = DoorWidth * 0.5f;
	const float HalfH = DoorHeight * 0.5f;
	const float JambY = HalfW + FrameThickness * 0.5f;
	const FVector JambScale(FrameDepth, FrameThickness, DoorHeight + FrameThickness);
	FrameLeft->SetRelativeLocation(FVector(FrameDepth * 0.5f, -JambY, FrameThickness * 0.5f));
	FrameLeft->SetRelativeScale3D(JambScale / 100.0f);
	FrameRight->SetRelativeLocation(FVector(FrameDepth * 0.5f, JambY, FrameThickness * 0.5f));
	FrameRight->SetRelativeScale3D(JambScale / 100.0f);
	FrameTop->SetRelativeLocation(FVector(FrameDepth * 0.5f, 0.0f, HalfH + FrameThickness * 0.5f));
	FrameTop->SetRelativeScale3D(FVector(FrameDepth, DoorWidth, FrameThickness) / 100.0f);
	FrameButtom->SetRelativeLocation(FVector(FrameDepth * 0.5f, 0.0f, -(HalfH + FrameThickness * 0.5f)));
	FrameButtom->SetRelativeScale3D(FVector(FrameDepth, DoorWidth, FrameThickness) / 100.0f);
	if (FrameMaterial)
	{
		FrameLeft->SetMaterial(0, FrameMaterial);
		FrameRight->SetMaterial(0, FrameMaterial);
		FrameTop->SetMaterial(0, FrameMaterial);
		FrameButtom->SetMaterial(0, FrameMaterial);
	}

	// Plane local X (after the pitch) runs along the door height, local Y along its width.
	PortalMesh->SetRelativeLocation(FVector(1.0f, 0.0f, 0.0f));
	PortalMesh->SetRelativeScale3D(FVector(DoorHeight / 100.0f, DoorWidth / 100.0f, 1.0f));

	PortalSurface->SetRelativeLocation(FVector(2.0f, 0.0f, 0.0f));
	PortalSurface->SetBoxExtent(FVector(2.0f, DoorWidth * 0.5f, DoorHeight * 0.5f));
}

void AZL_Doorman_Door::BeginPlay()
{
	Super::BeginPlay();

	SetupPortalView();
	UpdateOpenState();
}

void AZL_Doorman_Door::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZL_Doorman_Door, LinkedDoor);
}

void AZL_Doorman_Door::LinkDoors(AZL_Doorman_Door* A, AZL_Doorman_Door* B)
{
	if (!IsValid(A) || !IsValid(B))
	{
		return;
	}
	A->LinkedDoor = B;
	B->LinkedDoor = A;
	A->UpdateOpenState();
	B->UpdateOpenState();
}

void AZL_Doorman_Door::OnRep_LinkedDoor()
{
	UpdateOpenState();
}

void AZL_Doorman_Door::UpdateOpenState()
{
	if (PortalMID)
	{
		PortalMID->SetScalarParameterValue(TEXT("Open"), IsLinked() && bViewInRange ? 1.0f : 0.0f);
	}

	// The exit door's portal plane sits right at the capture's near plane; keep it out of our view.
	if (IsLinked() && LinkedDoor->PortalMesh)
	{
		PortalCapture->HideComponent(LinkedDoor->PortalMesh);
	}
}

FVector AZL_Doorman_Door::MirrorLocation(const FVector& WorldLocation) const
{
	FVector Local = GetActorTransform().InverseTransformPositionNoScale(WorldLocation);
	Local.X = -Local.X;
	Local.Y = -Local.Y;
	return LinkedDoor->GetActorTransform().TransformPositionNoScale(Local);
}

FVector AZL_Doorman_Door::MirrorVector(const FVector& WorldVector) const
{
	FVector Local = GetActorTransform().InverseTransformVectorNoScale(WorldVector);
	Local.X = -Local.X;
	Local.Y = -Local.Y;
	return LinkedDoor->GetActorTransform().TransformVectorNoScale(Local);
}

FQuat AZL_Doorman_Door::MirrorRotation(const FQuat& WorldRotation) const
{
	const FQuat Flip(FVector::UpVector, PI);
	return LinkedDoor->GetActorQuat() * Flip * GetActorQuat().Inverse() * WorldRotation;
}

FBox AZL_Doorman_Door::GetLocalBounds() const
{
	const float HalfOuterWidth = DoorWidth * 0.5f + FrameThickness;
	return FBox(FVector(0.0f, -HalfOuterWidth, -DoorHeight * 0.5f), FVector(FrameDepth, HalfOuterWidth, DoorHeight * 0.5f + FrameThickness));
}

bool AZL_Doorman_Door::IsPortalSurface(const UPrimitiveComponent* Component) const
{
	return Component && Component == PortalSurface;
}

bool AZL_Doorman_Door::GetPortalExit(const FHitResult& Hit, const FVector& Velocity, float ProjectileRadius, FVector& OutLocation, FVector& OutVelocity) const
{
	// The frame is solid.
	if (!IsLinked() || !IsPortalSurface(Hit.GetComponent()))
	{
		return false;
	}
	const FVector HitLocation = Hit.Location;

	// Only projectiles flying into the front of the door go through.
	if (FVector::DotProduct(Velocity, GetActorForwardVector()) >= 0.0f)
	{
		return false;
	}

	FVector Local = GetActorTransform().InverseTransformPositionNoScale(HitLocation);
	/* Clear of the exit's portal surface (X 0..4) and inside the frame, so the projectile doesn't start the next move
	 * penetrating them (a non-bouncing projectile would detonate right at the exit). */
	const float MaxY = FMath::Max(DoorWidth * 0.5f - ProjectileRadius, 0.0f);
	const float MaxZ = FMath::Max(DoorHeight * 0.5f - ProjectileRadius, 0.0f);
	const FVector ExitLocal(ProjectileExitOffset + ProjectileRadius, FMath::Clamp(-Local.Y, -MaxY, MaxY), FMath::Clamp(Local.Z, -MaxZ, MaxZ));
	OutLocation = LinkedDoor->GetActorTransform().TransformPositionNoScale(ExitLocal);
	OutVelocity = MirrorVector(Velocity);
	UE_LOG(LogTemp, Log, TEXT("Doorway: projectile through %s -> %s at %s"), *GetName(), *LinkedDoor->GetName(), *OutLocation.ToString());
	return true;
}

void AZL_Doorman_Door::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetNetMode() == NM_DedicatedServer || !IsLinked())
	{
		return;
	}

	CheckLocalPawnTravel();
	UpdatePortalView();
}

void AZL_Doorman_Door::CheckLocalPawnTravel()
{
	const double Now = GetWorld()->GetTimeSeconds();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Pawn = PC && PC->IsLocalController() ? PC->GetPawn() : nullptr;
		if (!IsValid(Pawn))
		{
			continue;
		}

		const FVector Local = GetActorTransform().InverseTransformPositionNoScale(Pawn->GetActorLocation());
		if (Local.X > PawnTriggerDepth || Local.X < -PawnTriggerDepth
			|| FMath::Abs(Local.Y) > DoorWidth * 0.5f || FMath::Abs(Local.Z) > DoorHeight * 0.5f)
		{
			continue;
		}

		// Moving into the door (or pushing against it), not walking out of it.
		if (FVector::DotProduct(Pawn->GetVelocity(), GetActorForwardVector()) > 1.0f)
		{
			continue;
		}

		if (const double* Last = LastTravelTime.Find(Pawn); Last && Now - *Last < PawnTravelCooldown)
		{
			continue;
		}

		TravelPawn(Pawn, Local);
	}
}

void AZL_Doorman_Door::TravelPawn(APawn* Pawn, const FVector& LocalLocation)
{
	AZeroMoverPawn* MoverPawn = Cast<AZeroMoverPawn>(Pawn);
	UZeroMoverComponent* Mover = MoverPawn ? MoverPawn->GetZeroMoverComponent() : nullptr;
	if (!Mover)
	{
		return;
	}

	const FVector ExitLocal(PawnExitOffset, -LocalLocation.Y, LocalLocation.Z);
	const FVector ExitLocation = LinkedDoor->GetActorTransform().TransformPositionNoScale(ExitLocal);

	// Mirror the velocity, then make sure it carries the pawn out of the exit door.
	FVector VelocityLocal = GetActorTransform().InverseTransformVectorNoScale(Pawn->GetVelocity());
	VelocityLocal = FVector(FMath::Max(-VelocityLocal.X, MinPawnExitSpeed), -VelocityLocal.Y, VelocityLocal.Z);
	const FVector ExitVelocity = LinkedDoor->GetActorTransform().TransformVectorNoScale(VelocityLocal);

	// Latched into the Mover input stream: client and server teleport on the same frame.
	Mover->RequestSafeTeleportWithVelocity(ExitLocation, ExitVelocity);
	UE_LOG(LogTemp, Log, TEXT("Doorway: %s through %s -> %s at %s"), *Pawn->GetName(), *GetName(), *LinkedDoor->GetName(), *ExitLocation.ToString());

	// Turn the view with the pawn (the server follows through the input's LookDir).
	if (AController* Controller = Pawn->GetController())
	{
		FRotator ControlRotation = Controller->GetControlRotation();
		ControlRotation.Yaw += LinkedDoor->GetActorRotation().Yaw + 180.0f - GetActorRotation().Yaw;
		Controller->SetControlRotation(ControlRotation);
	}

	const double Now = GetWorld()->GetTimeSeconds();
	LastTravelTime.Add(Pawn, Now);
	LinkedDoor->LastTravelTime.Add(Pawn, Now);
}

void AZL_Doorman_Door::SetupPortalView()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		PortalCapture->Deactivate();
		return;
	}

	PortalCapture->ShowFlags.SetDynamicShadows(bCaptureShadows);

	FVector2D ViewportSize(1280.0f, 720.0f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	const int32 Width = FMath::Max(64, FMath::RoundToInt(ViewportSize.X * RenderTargetScale));
	const int32 Height = FMath::Max(64, FMath::RoundToInt(ViewportSize.Y * RenderTargetScale));

	PortalRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, Width, Height, RTF_RGBA8);
	PortalCapture->TextureTarget = PortalRenderTarget;

	if (PortalMaterial)
	{
		PortalMID = UMaterialInstanceDynamic::Create(PortalMaterial, this);
		PortalMID->SetTextureParameterValue(TEXT("PortalTexture"), PortalRenderTarget);
		PortalMesh->SetMaterial(0, PortalMID);
	}
}

void AZL_Doorman_Door::UpdatePortalView()
{
	if (!PortalRenderTarget || !PortalMesh->WasRecentlyRendered(0.2f))
	{
		return;
	}

	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APlayerCameraManager* Camera = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (!Camera)
	{
		return;
	}

	// Behind the door the portal face can't be seen.
	const FVector CameraLocation = Camera->GetCameraLocation();
	const float DistanceToDoor = GetActorTransform().InverseTransformPositionNoScale(CameraLocation).X;
	if (DistanceToDoor <= 0.0f)
	{
		return;
	}

	// Far away the door is small on screen: show it closed instead of paying for a scene render.
	const bool bInRange = MaxCaptureDistance <= 0.0f || FVector::DistSquared(CameraLocation, GetActorLocation()) <= FMath::Square(MaxCaptureDistance);
	if (bInRange != bViewInRange)
	{
		bViewInRange = bInRange;
		UpdateOpenState();
	}
	if (!bInRange)
	{
		return;
	}

	// The view through this door is the view out of the linked door, from the mirrored camera.
	const FVector VirtualLocation = MirrorLocation(CameraLocation);
	const FQuat VirtualRotation = MirrorRotation(Camera->GetCameraRotation().Quaternion());
	PortalCapture->SetWorldLocationAndRotation(VirtualLocation, VirtualRotation);
	PortalCapture->FOVAngle = Camera->GetFOVAngle();

	// Skip whatever is between the virtual camera and the exit door (the wall it is set in).
	PortalCapture->CustomNearClippingPlane = FMath::Max(DistanceToDoor - 5.0f, 1.0f);

	PortalCapture->CaptureScene();
}
