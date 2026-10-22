// Copyright Preetham Mukundan (C) 2026


#include "Doorman/ZL_Doorman_Hotel.h"

#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Doorman/ZL_Doorman_HotelGuestEffect.h"
#include "Doorman/ZL_Doorman_HotelPlaceholder.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/ZL_StatusTimerAbility.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "ZeroLock/ZeroLockCharacter.h"

namespace
{
	// Per lane: floor + ceiling, left + right + start + end walls, one elevator pad, two lights.
	constexpr int32 FloorsPerLane = 2;
	constexpr int32 WallsPerLane = 4;
	constexpr int32 LightsPerLane = 2;

	UStaticMeshComponent* CreateHotelMesh(AActor* Owner, const FString& Name, USceneComponent* Parent, UStaticMesh* Mesh, bool bCollides)
	{
		UStaticMeshComponent* Comp = Owner->CreateDefaultSubobject<UStaticMeshComponent>(*Name);
		Comp->SetupAttachment(Parent);
		Comp->SetStaticMesh(Mesh);
		Comp->SetMobility(EComponentMobility::Movable); // the hotel can be spawned at runtime
		Comp->SetCollisionProfileName(bCollides ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		Comp->SetGenerateOverlapEvents(false);
		return Comp;
	}

	/** Engine Cube is 100 cm and centered on its origin. */
	void PlaceBox(UStaticMeshComponent* Comp, const FVector& Center, const FVector& Size)
	{
		Comp->SetRelativeLocation(Center);
		Comp->SetRelativeScale3D(Size / 100.0f);
	}
}

AZL_Doorman_Hotel::AZL_Doorman_Hotel()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	// Far from everyone, but the guest's machine needs its collision to move predictively in it.
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		Material = ShapeMaterial.Object;
	}

	for (int32 Lane = 0; Lane < NumLanes; ++Lane)
	{
		for (int32 i = 0; i < FloorsPerLane; ++i)
		{
			FloorMeshes.Add(CreateHotelMesh(this, FString::Printf(TEXT("Lane%d_Floor%d"), Lane, i), Root, CubeMesh.Object, true));
		}
		for (int32 i = 0; i < WallsPerLane; ++i)
		{
			WallMeshes.Add(CreateHotelMesh(this, FString::Printf(TEXT("Lane%d_Wall%d"), Lane, i), Root, CubeMesh.Object, true));
		}
		ElevatorMeshes.Add(CreateHotelMesh(this, FString::Printf(TEXT("Lane%d_Elevator"), Lane), Root, CubeMesh.Object, false));
		for (int32 i = 0; i < LightsPerLane; ++i)
		{
			UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("Lane%d_Light%d"), Lane, i));
			Light->SetupAttachment(Root);
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetCastShadows(false);
			Lights.Add(Light);
		}
	}
}

void AZL_Doorman_Hotel::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	const float L = CorridorLength;
	const float W = CorridorWidth;
	const float H = CorridorHeight;
	const float T = WallThickness;

	for (int32 Lane = 0; Lane < NumLanes; ++Lane)
	{
		const FVector O = GetLaneOrigin(Lane);

		PlaceBox(FloorMeshes[Lane * FloorsPerLane + 0], O + FVector(L * 0.5f, 0.0f, -T * 0.5f), FVector(L + 2.0f * T, W + 2.0f * T, T));
		PlaceBox(FloorMeshes[Lane * FloorsPerLane + 1], O + FVector(L * 0.5f, 0.0f, H + T * 0.5f), FVector(L + 2.0f * T, W + 2.0f * T, T));

		PlaceBox(WallMeshes[Lane * WallsPerLane + 0], O + FVector(L * 0.5f, -(W + T) * 0.5f, H * 0.5f), FVector(L + 2.0f * T, T, H));
		PlaceBox(WallMeshes[Lane * WallsPerLane + 1], O + FVector(L * 0.5f, (W + T) * 0.5f, H * 0.5f), FVector(L + 2.0f * T, T, H));
		PlaceBox(WallMeshes[Lane * WallsPerLane + 2], O + FVector(-T * 0.5f, 0.0f, H * 0.5f), FVector(T, W, H));
		PlaceBox(WallMeshes[Lane * WallsPerLane + 3], O + FVector(L + T * 0.5f, 0.0f, H * 0.5f), FVector(T, W, H));

		// Elevator: a glowing-colored pad on the floor where the checkout box is.
		PlaceBox(ElevatorMeshes[Lane], O + FVector(L - ElevatorDepth * 0.5f, 0.0f, 2.0f), FVector(ElevatorDepth, W, 4.0f));

		for (int32 i = 0; i < LightsPerLane; ++i)
		{
			UPointLightComponent* Light = Lights[Lane * LightsPerLane + i];
			Light->SetRelativeLocation(O + FVector(L * (0.25f + 0.5f * i), 0.0f, H - 50.0f));
			Light->SetIntensity(LightIntensity);
			Light->SetAttenuationRadius(FMath::Max(L * 0.5f, W));
		}
	}
}

void AZL_Doorman_Hotel::BeginPlay()
{
	Super::BeginPlay();
	ApplyColors();
}

void AZL_Doorman_Hotel::ApplyColors()
{
	if (!Material || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	auto Tint = [this](const TArray<TObjectPtr<UStaticMeshComponent>>& Meshes, const FLinearColor& Color)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			Mesh->SetMaterial(0, MID);
		}
	};
	Tint(WallMeshes, WallColor);
	Tint(FloorMeshes, FloorColor);
	Tint(ElevatorMeshes, ElevatorColor);
}

void AZL_Doorman_Hotel::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		while (Stays.Num() > 0)
		{
			CheckOut(Stays.Last().Guest.Get(), EZL_HotelCheckout::Cancelled);
		}
	}
	Super::EndPlay(EndPlayReason);
}

AZL_Doorman_Hotel* AZL_Doorman_Hotel::GetOrSpawnHotel(UWorld* World, TSubclassOf<AZL_Doorman_Hotel> HotelClass, const FVector& SpawnLocation)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AZL_Doorman_Hotel> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	if (World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* Class = HotelClass ? HotelClass.Get() : AZL_Doorman_Hotel::StaticClass();
	return World->SpawnActor<AZL_Doorman_Hotel>(Class, FTransform(SpawnLocation), SpawnParams);
}

FVector AZL_Doorman_Hotel::GetLaneOrigin(int32 Lane) const
{
	return FVector(0.0f, (Lane - (NumLanes - 1) * 0.5f) * LaneSpacing, 0.0f);
}

FVector AZL_Doorman_Hotel::GetArrivalLocation(int32 Lane, float GuestHalfHeight) const
{
	return GetActorTransform().TransformPosition(GetLaneOrigin(Lane) + FVector(ArrivalOffset, 0.0f, GuestHalfHeight + 5.0f));
}

FBox AZL_Doorman_Hotel::GetElevatorBox(int32 Lane) const
{
	const FVector O = GetLaneOrigin(Lane);
	const FVector Min = O + FVector(CorridorLength - ElevatorDepth, -CorridorWidth * 0.5f, 0.0f);
	const FVector Max = O + FVector(CorridorLength, CorridorWidth * 0.5f, CorridorHeight);
	return FBox(Min, Max).ExpandBy(ElevatorCheckTolerance);
}

int32 AZL_Doorman_Hotel::FindFreeLane() const
{
	for (int32 Lane = 0; Lane < NumLanes; ++Lane)
	{
		if (!Stays.ContainsByPredicate([Lane](const FZL_HotelStay& Stay) { return Stay.Lane == Lane; }))
		{
			return Lane;
		}
	}
	return INDEX_NONE;
}

bool AZL_Doorman_Hotel::IsGuest(const AZeroLockCharacter* Guest) const
{
	return Guest && Stays.ContainsByPredicate([Guest](const FZL_HotelStay& Stay) { return Stay.Guest.Get() == Guest; });
}

bool AZL_Doorman_Hotel::CheckIn(AZeroLockCharacter* Doorman, AZeroLockCharacter* Guest, const FZL_HotelStayParams& Params)
{
	if (!HasAuthority() || !Doorman || !Guest || IsGuest(Guest))
	{
		return false;
	}
	UBaseCharAbilitySystemComponent* DoormanASC = Doorman->GetMyAbilitySystemComp();
	UBaseCharAbilitySystemComponent* GuestASC = Guest->GetMyAbilitySystemComp();
	const int32 Lane = FindFreeLane();
	if (!DoormanASC || !GuestASC || Lane == INDEX_NONE)
	{
		return false;
	}

	FZL_HotelStay& Stay = Stays.AddDefaulted_GetRef();
	Stay.Guest = Guest;
	Stay.Doorman = Doorman;
	Stay.DoormanASC = DoormanASC;
	Stay.Lane = Lane;
	Stay.ReturnLocation = Guest->GetActorLocation();
	Stay.ReturnControlRotation = FRotator(0.0f, Guest->GetActorRotation().Yaw, 0.0f);
	Stay.Deadline = GetWorld()->GetTimeSeconds() + Params.TimeLimit;
	Stay.LateCheckoutDamage = Params.LateCheckoutDamage;

	// The body leaves a placeholder behind.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* PlaceholderClass = Params.PlaceholderClass ? Params.PlaceholderClass.Get() : AZL_Doorman_HotelPlaceholder::StaticClass();
	Stay.Placeholder = GetWorld()->SpawnActor<AZL_Doorman_HotelPlaceholder>(PlaceholderClass, FTransform(Stay.ReturnControlRotation, Stay.ReturnLocation), SpawnParams);

	if (Params.GuestEffect)
	{
		FGameplayEffectContextHandle Context = DoormanASC->MakeEffectContext();
		Context.AddSourceObject(Doorman);
		FGameplayEffectSpecHandle SpecHandle = DoormanASC->MakeOutgoingSpec(Params.GuestEffect, Params.Level, Context);
		if (SpecHandle.IsValid())
		{
			FGameplayEffectSpec& Spec = *SpecHandle.Data.Get();
			Spec.SetDuration(Params.TimeLimit, true);
			Spec.DynamicGrantedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_STATUS_HOTEL_GUEST);
			Spec.DynamicGrantedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_STATUS_MOVEMENT_ABILITIES_BLOCKED);
			Stay.EffectHandle = DoormanASC->ApplyGameplayEffectSpecToTarget(Spec, GuestASC);
		}
	}

	Stay.PreDeathHandle = Guest->OnServerPreDeath.AddUObject(this, &AZL_Doorman_Hotel::HandleGuestPreDeath);

	const FVector Arrival = GetArrivalLocation(Lane, Guest->GetSimpleCollisionHalfHeight());
	Guest->ServerForceTeleport(Arrival, FRotator(0.0f, GetActorRotation().Yaw, 0.0f));
	UZL_StatusTimerAbility::SendStatusTimer(Doorman, GuestASC, ZerolockGameplayTagsForBinding::TAG_STATUS_HOTEL_GUEST, Params.TimeLimit);

	SetActorTickEnabled(true);

	// Last: if this kills the guest, the pre-death handler checks them out right away.
	if (Params.EntryDamage > 0.0f)
	{
		DoormanASC->ApplySpiritDamage(GuestASC, Params.EntryDamage, this);
	}

	UE_LOG(LogTemp, Log, TEXT("[Hotel] %s checked in %s (lane %d, %.1f s)"), *Doorman->GetName(), *Guest->GetName(), Lane, Params.TimeLimit);
	return true;
}

void AZL_Doorman_Hotel::HandleGuestPreDeath(AZeroLockCharacter* Guest)
{
	CheckOut(Guest, EZL_HotelCheckout::Died);
}

void AZL_Doorman_Hotel::CheckOut(AZeroLockCharacter* Guest, EZL_HotelCheckout Reason)
{
	// A null guest checks out the first stay whose pawn is gone.
	const int32 Index = Stays.IndexOfByPredicate([Guest](const FZL_HotelStay& Stay)
	{
		return Guest ? Stay.Guest.Get() == Guest : !Stay.Guest.IsValid();
	});
	if (Index == INDEX_NONE)
	{
		return;
	}

	// Removed first: the damage and broadcast below can re-enter (a lethal late fee fires the pre-death hook).
	const FZL_HotelStay Stay = Stays[Index];
	Stays.RemoveAt(Index);

	if (Guest)
	{
		Guest->OnServerPreDeath.Remove(Stay.PreDeathHandle);
		if (UBaseCharAbilitySystemComponent* GuestASC = Guest->GetMyAbilitySystemComp())
		{
			GuestASC->RemoveActiveGameplayEffect(Stay.EffectHandle);
			UZL_StatusTimerAbility::ClearStatusTimer(Stay.Doorman.Get(), GuestASC, ZerolockGameplayTagsForBinding::TAG_STATUS_HOTEL_GUEST);

			if (Reason == EZL_HotelCheckout::Late && Stay.LateCheckoutDamage > 0.0f)
			{
				if (UBaseCharAbilitySystemComponent* DoormanASC = Stay.DoormanASC.Get())
				{
					DoormanASC->ApplySpiritDamage(GuestASC, Stay.LateCheckoutDamage, this);
				}
			}
		}
		// Back where the body was taken from, even when the late fee just killed them (the hook found no stay).
		Guest->ServerForceTeleport(Stay.ReturnLocation, Stay.ReturnControlRotation);
	}

	if (AZL_Doorman_HotelPlaceholder* Placeholder = Stay.Placeholder.Get())
	{
		Placeholder->Destroy();
	}

	if (Stays.Num() == 0)
	{
		SetActorTickEnabled(false);
	}

	UE_LOG(LogTemp, Log, TEXT("[Hotel] %s checked out (%s)"), Guest ? *Guest->GetName() : TEXT("<gone>"), *UEnum::GetValueAsString(Reason));
	OnStayEnded.Broadcast(Guest, Reason);
}

void AZL_Doorman_Hotel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		SetActorTickEnabled(false);
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();

	// Decide first, check out after: CheckOut changes Stays.
	TArray<TPair<TWeakObjectPtr<AZeroLockCharacter>, EZL_HotelCheckout>> Checkouts;
	bool bHasStaleStay = false;
	for (const FZL_HotelStay& Stay : Stays)
	{
		const AZeroLockCharacter* Guest = Stay.Guest.Get();
		if (!Guest)
		{
			bHasStaleStay = true;
			continue;
		}

		const FBox Elevator = GetElevatorBox(Stay.Lane);
		if (Elevator.IsInside(GetActorTransform().InverseTransformPosition(Guest->GetActorLocation())))
		{
			Checkouts.Emplace(Stay.Guest, EZL_HotelCheckout::OnTime);
		}
		else if (Now >= Stay.Deadline)
		{
			Checkouts.Emplace(Stay.Guest, EZL_HotelCheckout::Late);
		}

#if ENABLE_DRAW_DEBUG
		if (bDrawDebug)
		{
			DrawDebugBox(GetWorld(), GetActorTransform().TransformPosition(Elevator.GetCenter()), Elevator.GetExtent(), GetActorQuat(), FColor::Yellow, false, -1.0f, 0, 3.0f);
		}
#endif
	}

	for (const TPair<TWeakObjectPtr<AZeroLockCharacter>, EZL_HotelCheckout>& Checkout : Checkouts)
	{
		if (AZeroLockCharacter* Guest = Checkout.Key.Get())
		{
			CheckOut(Guest, Checkout.Value);
		}
	}
	if (bHasStaleStay)
	{
		CheckOut(nullptr, EZL_HotelCheckout::Cancelled);
	}
}
