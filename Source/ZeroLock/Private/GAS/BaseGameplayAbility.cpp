//Copyright Preetham Mukundan (C) 2026


#include "GAS/BaseGameplayAbility.h"

#include "GameplayTagsManager.h"
#include "KismetTraceUtils.h"
#include "Engine/OverlapResult.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/BaseCharAttributeSet.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/ZL_GE_BaseCooldown.h"
#include "UI/MVVM/ZL_VM_Attributes.h"
#include "UI/MVVM/Abilities/ZL_AbilityUIManagerComponent.h"
#include "UI/MVVM/Abilities/ZL_VM_AbilityTimerProgressBar.h"
#include "UI/MVVM/Abilities/ZL_VM_ChargePercent.h"
#include "ZeroLock/ZeroLockCharacter.h"

UBaseGameplayAbility::UBaseGameplayAbility()
{
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities"),false));
	BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities"),false));
	FGameplayTagContainer TagContainer;
	TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("ZeroLLock.Abilities"),false));
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	CooldownGameplayEffectClass = UZL_GE_BaseCooldown::StaticClass();
	SetAssetTags(TagContainer);
}

void UBaseGameplayAbility::SetSlot(EGameplayAbilitySlot slot)
{
	Slot = slot;
}

void UBaseGameplayAbility::ApplyGameplayEffectToTarget(TSubclassOf<UGameplayEffect> GEToApply,
                                                       UAbilitySystemComponent* TargetASC, UAbilitySystemComponent* SourceASC)
{
	if (GEToApply == nullptr || TargetASC == nullptr || SourceASC == nullptr)
	{
		ZLOG("CancelledInBaseGameplayAbility::ApplyGameplyEffectToTarget");
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
	if (HasAuthority(&CurrentActivationInfo))
	{
		//ZLOG("Found Damage");
		FGameplayEffectContextHandle EffectContext =SourceASC->MakeEffectContext();
		EffectContext.AddSourceObject(this);
				
	
		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(GEToApply, 1, EffectContext);

		if (SpecHandle.IsValid())
		{
			ZLOG("ApplyingEffect::ApplyGameplyEffectToTarget");
			FActiveGameplayEffectHandle GEHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(),TargetASC);
		}
	}
}

void UBaseGameplayAbility::ApplyGameplyEffectToSelf(TSubclassOf<UGameplayEffect> GEToApply,
	UAbilitySystemComponent* SourseASC)
{
	
	if (GEToApply == nullptr || SourseASC == nullptr)
	{
		ZLOG("CancelledInBaseGameplayAbility::ApplyGameplyEffectToSelf");
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
	if (HasAuthority(&CurrentActivationInfo))
	{
		FGameplayEffectContextHandle EffectContext =SourseASC->MakeEffectContext();
		EffectContext.AddSourceObject(this);


		FGameplayEffectSpecHandle SpecHandle = SourseASC->MakeOutgoingSpec(GEToApply, 1, EffectContext);

		if (SpecHandle.IsValid())
		{
			ZLOG("BuffDone");
			FActiveGameplayEffectHandle GEHandle = SourseASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
}

UAbilitySystemComponent* UBaseGameplayAbility::GetOwnerASC()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero )
	{
		return Hero->GetAbilitySystemComponent();
	}
	return nullptr;
}



void UBaseGameplayAbility::SetInputID(EGASAbilityInputID in)
{
	AbilityInputID=in;
}

void UBaseGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (CooldownGE)
	{
		
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CooldownGE->GetClass(), GetAbilityLevel());
		SpecHandle.Data.Get()->DynamicGrantedTags.AppendTags(CooldownTags);
		float CooldownTimeToUse =GetCoolDownTime();
		if (bIsChargedAbility)
		{
			CooldownTimeToUse = ChargeRechargeDuration;
		}
		SpecHandle.Data.Get()->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName(  "Ability.Cooldown.Duration" )), CooldownTimeToUse);
		FActiveGameplayEffectHandle GEHandle=ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		if (GEHandle.IsValid())
		{
			ZLOG("CustomSetByCaller");
		}
	}
}

float UBaseGameplayAbility::GetCoolDownTime() const
{
	if (!GetCurrentActorInfo()) 
	{
		return 0.0f; 
	}
	float cooldownTime = CooldownDuration.GetValueAtLevel(GetAbilityLevel());
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero)
	{ 
		if (!Hero->GetMyAttributeSet())
		{
			return cooldownTime;
		}
		float CDR = Hero->GetMyAttributeSet()->GetCooldownReduction();
		return (cooldownTime*(1-CDR/100));
	}
	return cooldownTime;
}

const FGameplayTagContainer* UBaseGameplayAbility::GetCooldownTags() const
{
	FGameplayTagContainer* MutableTags = const_cast<FGameplayTagContainer*>(&TempCooldownTags);
	MutableTags->Reset(); // MutableTags writes to the TempCooldownTags on the CDO so clear it in case the ability cooldown tags change (moved to a different slot)
	const FGameplayTagContainer* ParentTags = Super::GetCooldownTags();
	if (ParentTags)
	{
		MutableTags->AppendTags(*ParentTags);
	}
	MutableTags->AppendTags(CooldownTags);
	return MutableTags;
}

bool UBaseGameplayAbility::ReverseConeTraceMulti(const UObject* WorldContextObject, const FVector Start,
	const FRotator Direction, float ConeHeight, float ConeHalfAngle, ETraceTypeQuery TraceChannel, bool bTraceComplex,
	const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits,
	TArray<AZeroLockCharacter*>& OutVillans, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor,
	float DrawTime)
{OutHits.Reset();
    OutVillans.Reset();

    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World) return false;

    ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ReverseConeTraceMulti), bTraceComplex);
    Params.AddIgnoredActors(ActorsToIgnore);

    const FVector ForwardDir = Direction.Vector();
    const float ConeHalfAngleRad = FMath::DegreesToRadians(ConeHalfAngle);
    const float MaxRadius = ConeHeight * FMath::Tan(ConeHalfAngleRad);

    // 1. Use OverlapMulti instead of SweepMulti to ignore occlusion (blocks)
    // We use a sphere that encompasses the entire cone area.
    TArray<FOverlapResult> Overlaps;
    FCollisionShape BoundingSphere = FCollisionShape::MakeSphere(ConeHeight); 
    
    // We center the sphere at the start of the cone
    World->OverlapMultiByChannel(Overlaps, Start, FQuat::Identity, CollisionChannel, BoundingSphere, Params);

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* HitActor = Overlap.GetActor();
        if (!HitActor) continue;

        // Since Overlap doesn't give an ImpactPoint, we use the Actor's location
        FVector HitLocation = HitActor->GetActorLocation();
        FVector ToHit = HitLocation - Start;
        
        // Project the vector to the actor onto the cone's center axis
        float DistAlongAxis = FVector::DotProduct(ToHit, ForwardDir);

        // Filter: Is it within the height of the cone?
        if (DistAlongAxis < 0.f || DistAlongAxis > ConeHeight) continue;

        // Calculate the cone radius at this specific distance (Reverse Cone Logic)
        // Wide at Start (Dist=0), Tip at End (Dist=ConeHeight)
        float AllowedRadiusAtDist = MaxRadius * (1.0f - (DistAlongAxis / ConeHeight));

        // Find how far the actor is from the center axis
        FVector PointOnAxis = Start + (ForwardDir * DistAlongAxis);
        float ActualDistFromAxis = FVector::Dist(HitLocation, PointOnAxis);

        if (ActualDistFromAxis <= AllowedRadiusAtDist)
        {
            // Create a dummy FHitResult since the function signature requires it
            FHitResult Hit(HitActor, Overlap.GetComponent(), HitLocation, -ForwardDir);
            OutHits.Add(Hit);

            if (AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(HitActor))
            {
                OutVillans.AddUnique(Villan);
            }
        }
    }

#if ENABLE_DRAW_DEBUG
    if (DrawDebugType != EDrawDebugTrace::None)
    {
        FColor Color = TraceColor.ToFColor(true);
        const FVector End = Start + (ForwardDir * ConeHeight);
        // Draw the reverse cone (pointing back towards Start)
        DrawDebugCone(World, End, -ForwardDir, ConeHeight, ConeHalfAngleRad, ConeHalfAngleRad, 24, Color, (DrawDebugType == EDrawDebugTrace::Persistent), DrawTime);
        
        for (const FHitResult& Hit : OutHits)
        {
            DrawDebugPoint(World, Hit.ImpactPoint, 10.f, TraceHitColor.ToFColor(true), (DrawDebugType == EDrawDebugTrace::Persistent), DrawTime);
        }
    }
#endif

    return (OutVillans.Num() > 0);
}

bool UBaseGameplayAbility::ConeTraceMulti(const UObject* WorldContextObject, const FVector Start,
                                          const FRotator Direction, float ConeHeight, float ConeHalfAngle, ETraceTypeQuery TraceChannel, bool bTraceComplex,
                                          const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits,TArray<AZeroLockCharacter*>& OutVillans,
                                          bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	OutHits.Reset();
	OutVillans.Reset();
	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ConeTraceMulti), bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.AddIgnoredActors(ActorsToIgnore);
 
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return false;
	}
 
	TArray<FHitResult> TempHitResults;
	const FVector End = Start + (Direction.Vector() * ConeHeight);
	const double ConeHalfAngleRad = FMath::DegreesToRadians(ConeHalfAngle);
	// r = h * tan(theta / 2)
	const double ConeBaseRadius = ConeHeight * tan(ConeHalfAngleRad);
	const FCollisionShape SphereSweep = FCollisionShape::MakeSphere(ConeBaseRadius);
 
	// Perform a sweep encompassing an imaginary cone.
	World->SweepMultiByChannel(TempHitResults, Start, End, Direction.Quaternion(), CollisionChannel, SphereSweep, Params);
 
	// Filter for hits that would be inside the cone.
	for (FHitResult& HitResult : TempHitResults)
	{
		const FVector HitDirection = (HitResult.ImpactPoint - Start).GetSafeNormal();
		const double Dot = FVector::DotProduct(Direction.Vector(), HitDirection);
		// theta = arccos((A • B) / (|A|*|B|)). |A|*|B| = 1 because A and B are unit vectors.
		const double DeltaAngle = FMath::Acos(Dot);
 
		// Hit is outside the angle of the cone.
		if (DeltaAngle > ConeHalfAngleRad)
		{
			continue;
		}
 
		const double Distance = (HitResult.ImpactPoint - Start).Length();
		// Hypotenuse = adjacent / cos(theta)
		const double LengthAtAngle = ConeHeight / cos(DeltaAngle);
 
		// Hit is beyond the cone. This can happen because we sweep with spheres, which results in a cap at the end of the sweep.
		if (Distance > LengthAtAngle)
		{
			continue;
		}
		if (AZeroLockCharacter* villan = Cast<AZeroLockCharacter>(HitResult.GetActor()))
		{
			OutVillans.AddUnique(villan);
		}
		OutHits.Add(HitResult);
	}
 
#if ENABLE_DRAW_DEBUG
	if (DrawDebugType != EDrawDebugTrace::None)
	{
		// Cone trace.
		const double ConeSlantHeight = FMath::Sqrt((ConeBaseRadius * ConeBaseRadius) + (ConeHeight * ConeHeight)); // s = sqrt(r^2 + h^2)
		DrawDebugCone(World, Start, Direction.Vector(), ConeSlantHeight, ConeHalfAngleRad, ConeHalfAngleRad, 32, TraceColor.ToFColor(true), (DrawDebugType == EDrawDebugTrace::Persistent), DrawTime);
 
		// Uncomment to see the trace we're actually performing.
		// DrawDebugSweptSphere(World, Start, End, ConeBaseRadius, TraceColor.ToFColor(true), (DrawDebugType == EDrawDebugTrace::Persistent), DrawTime);
 
		// Successful hits.
		for (const FHitResult& Hit : OutHits)
		{
			DrawDebugLineTraceSingle(World, Hit.TraceStart, Hit.ImpactPoint, DrawDebugType, true, Hit, TraceHitColor, TraceHitColor, DrawTime);
		}
 
		// Uncomment to see hits from the sphere sweep that were filtered out.
		// for (const FHitResult& Hit : TempHitResults)
		// {
		//     if (!OutHits.ContainsByPredicate([Hit](const FHitResult& Other)
		//     {
		//         return (Hit.GetActor() == Other.GetActor()) &&
		//                (Hit.ImpactPoint == Other.ImpactPoint) &&
		//                (Hit.ImpactNormal == Other.ImpactNormal);
		//     }))
		//     {
		//         DrawDebugLineTraceSingle(World, Hit.TraceStart, Hit.ImpactPoint, DrawDebugType, false, Hit, FColor::Red, FColor::Red, DrawTime);
		//     }
		// }
	}
#endif // ENABLE_DRAW_DEBUG
 
	return (OutHits.Num() > 0);
}

bool UBaseGameplayAbility::GetConeOverlap(UWorld* World, TArray<FOverlapResult>& OutResults, const FVector& Origin,
                                          const FVector& Direction, float Radius, float AngleDegrees, ECollisionChannel Channel)
{
	if (!World) return false;

	DrawDebugSphere(GetWorld(), Origin, Radius, 20, FColor::Yellow, false,10);
	FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
	FCollisionQueryParams Params;
	Params.bTraceComplex = false;
	Params.AddIgnoredActor(GetOwningActorFromActorInfo());
	
	TArray<FOverlapResult> SphereResults;
	bool bHit = World->OverlapMultiByChannel(SphereResults, Origin, FQuat::Identity, Channel, Sphere, Params);

	if (!bHit) return false;


	float HalfAngleRadians = FMath::DegreesToRadians(AngleDegrees * 0.5f);
	float CosThreshold = FMath::Cos(HalfAngleRadians);
	FVector NormalizedDir = Direction.GetSafeNormal();

	for (const FOverlapResult& Result : SphereResults)
	{
		AActor* OverlappedActor = Result.GetActor();
		if (!OverlappedActor) continue;

		FVector TargetLocation = OverlappedActor->GetActorLocation();
		FVector DirToTarget = (TargetLocation - Origin).GetSafeNormal();

		
		float Dot = FVector::DotProduct(NormalizedDir, DirToTarget);

		if (Dot >= CosThreshold)
		{
			OutResults.Add(Result);
		}
	}

	return OutResults.Num() > 0;
}


void UBaseGameplayAbility::StartProgressionTimer()
{
	MyProgressBarVM= GetAbilityUiComp()->AddProgressBarVM(FName(AbilityName));
}

UZL_AbilityUIManagerComponent* UBaseGameplayAbility::GetAbilityUiComp()
{
	if (!MyAbilityManagerComp)
	{
		AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetCurrentActorInfo()->AvatarActor);
		if (Hero)
		{
			MyAbilityManagerComp = Hero->GetAbilityUIManager();
		}
	}
	return MyAbilityManagerComp;
}

void UBaseGameplayAbility::StopProgressionTimer()
{
	GetAbilityUiComp()->RemoveProgressBarVM(MyProgressBarVM);
	MyProgressBarVM = nullptr;
}

void UBaseGameplayAbility::UpdateProgressionTimer(float progress)
{

	if (MyProgressBarVM)
	{
		MyProgressBarVM->SetProgressionLevel(progress);
	}
}

void UBaseGameplayAbility::StartChargePhaseUI(float MaxTIme, float perfectmin, float perfectmax)
{
	
	MyChargePhaseVM = NewObject<UZL_VM_ChargePercent>(GetCurrentActorInfo()->AvatarActor.Get());
	if (MyChargePhaseVM)
	{
		
		ZLOG("StartChargePhaseUI");
		GetHeroAttributeVM()->SetVM_ChargePhase(MyChargePhaseVM);
		MyChargePhaseVM->SetMaxChargeTime(MaxTIme);
		MyChargePhaseVM->SetPerfectMax(perfectmax);
		MyChargePhaseVM->SetPerfectMin(perfectmin);
	}
}

void UBaseGameplayAbility::UpdateChargePhaseUI(float Progress, bool bIsPerfect, float ElapsedTime)
{
	if (MyChargePhaseVM)
	{
		MyChargePhaseVM->SetbPerfect(bIsPerfect);
		MyChargePhaseVM->SetPerfectCharge(Progress);
	}
}

void UBaseGameplayAbility::RemoveChargePhaseUI(bool wasPerfect)
{
	GetHeroAttributeVM()->SetVM_ChargePhase(nullptr);
	MyChargePhaseVM = nullptr;
}

UZL_VM_Attributes* UBaseGameplayAbility::GetHeroAttributeVM()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetCurrentActorInfo()->AvatarActor);
	if (Hero)
	{
		return Hero->GetVM_Attributes();
	}
	return nullptr;
}

void UBaseGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	
	if (bIsChargedAbility && GetCurrentActivationInfo().ActivationMode== EGameplayAbilityActivationMode::Authority)
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		FGameplayAttribute ChargeAttr = GetChargeAttribute();
		FGameplayAttribute MaxAttr = GetMaxChargeAttribute();

		if (ASC && ChargeAttr.IsValid() && MaxAttr.IsValid())
		{
			ASC->SetNumericAttributeBase(MaxAttr, MaxChargesConfig);
			ASC->SetNumericAttributeBase(ChargeAttr, MaxChargesConfig);
		}
	}
}

bool UBaseGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!bIsChargedAbility)
	{
		return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
	}
	
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC)
	{
		float CurrentCharges = ASC->GetNumericAttribute(GetChargeAttribute());
		return CurrentCharges >= 1.0f;
	}

	return false;
}

void UBaseGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!bIsChargedAbility)
	{
		Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
		return;
	}

	if (HasAuthority(&ActivationInfo))
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		if (ASC)
		{
			ASC->ApplyModToAttribute(GetChargeAttribute(), EGameplayModOp::Additive, -1.0f);

			if (ChargeRechargeGEClass)
			{
				FGameplayEffectSpecHandle RechargeSpec = MakeOutgoingGameplayEffectSpec(ChargeRechargeGEClass, GetAbilityLevel());
				if (RechargeSpec.IsValid())
				{
					RechargeSpec.Data.Get()->SetSetByCallerMagnitude(RechargeDurationTag, GetCoolDownTime());
					ASC->ApplyGameplayEffectSpecToSelf(*RechargeSpec.Data.Get());
				}
			}
		}
	}
}

void UBaseGameplayAbility::SendVictimMoveEvent(AZeroLockCharacter* Hero, AZeroLockCharacter* Victim,
	const TCHAR* TagName, float Magnitude, const FVector* Location)
{
	if (!Hero || !Victim) return;
	UAbilitySystemComponent* TargetASC = Victim->GetAbilitySystemComponent();
	if (!TargetASC) return;

	FGameplayEventData Payload;
	Payload.Instigator = Hero;
	Payload.Target = Victim;
	Payload.EventMagnitude = Magnitude;

	if (Location)
	{
		FGameplayAbilityTargetData_LocationInfo* LocData = new FGameplayAbilityTargetData_LocationInfo();
		LocData->TargetLocation.LiteralTransform = FTransform(*Location);
		LocData->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
		Payload.TargetData.Add(LocData);
	}

	TargetASC->HandleGameplayEvent(FGameplayTag::RequestGameplayTag(FName(TagName)), &Payload);
}

FGameplayAttribute UBaseGameplayAbility::GetChargeAttribute() const
{
	switch (Slot)
	{
	case EGameplayAbilitySlot::AbilitySlot1: return UBaseCharAttributeSet::GetAbilityCharges_1Attribute();
	case EGameplayAbilitySlot::AbilitySlot2: return UBaseCharAttributeSet::GetAbilityCharges_2Attribute();
	case EGameplayAbilitySlot::AbilitySlot3: return UBaseCharAttributeSet::GetAbilityCharges_3Attribute();
	case EGameplayAbilitySlot::UltimateSlot: return UBaseCharAttributeSet::GetAbilityCharges_4Attribute();
	default: return FGameplayAttribute();
	}
}

FGameplayAttribute UBaseGameplayAbility::GetMaxChargeAttribute() const
{
	switch (Slot)
	{
	case EGameplayAbilitySlot::AbilitySlot1: return UBaseCharAttributeSet::GetMaxCharges_1Attribute();
	case EGameplayAbilitySlot::AbilitySlot2: return UBaseCharAttributeSet::GetMaxCharges_2Attribute();
	case EGameplayAbilitySlot::AbilitySlot3: return UBaseCharAttributeSet::GetMaxCharges_3Attribute();
	case EGameplayAbilitySlot::UltimateSlot: return UBaseCharAttributeSet::GetMaxCharges_4Attribute();
	default: return FGameplayAttribute();
	}
}




