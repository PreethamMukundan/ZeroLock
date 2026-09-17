// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZeroLockCharacter.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "TimerManager.h"
#include "Zero_BasePlayerController.h"
#include "Zero_BasePlayerState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Gamemode/Zero_BaseGameModeBase.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/BaseCharAttributeSet.h"
#include "GAS/BaseGameplayAbility.h"
#include "GAS/ZL_GameplayTags.h"
#include "Input/KeybindManagerSubsystem.h"
#include "Items/Zero_Item_Inventory_Component.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UI/Damage/ZL_BaseDamageWidgetComponent.h"
#include "UI/MVVM/ZL_VM_Attributes.h"
#include "UI/MVVM/Abilities/ZL_AbilityUIManagerComponent.h"
#include "UI/OverHead/ZL_OverHeadDisplay.h"
#include "UI/OverHead/ZL_OverHeadWidgetComponent.h"
#include "ZeroLock/Public/Movement/Zero_ZiplineActor.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AZeroLockCharacter

AZeroLockCharacter::AZeroLockCharacter()
{
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	//GAS Components
	AbilitySystemComp = CreateDefaultSubobject<UBaseCharAbilitySystemComponent>(TEXT("AbilitySystemComp"));
	AbilitySystemComp->SetIsReplicated(true);
	AbilitySystemComp->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	AttributeSet = CreateDefaultSubobject<UBaseCharAttributeSet>(TEXT("AttributeSet"));

	ItemInventoryComp = CreateDefaultSubobject<UZero_Item_Inventory_Component>(TEXT("ItemInventory"));
	ItemInventoryComp->SetIsReplicated(true);

	ParryComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ParryComponent"));
	ParryComp->SetupAttachment(RootComponent);
	ParryComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (GetMesh()) 
	{
		GetMesh()->CustomDepthStencilValue = 1;
		GetMesh()->SetRenderInDepthPass(true);
	}

	AbilityUIManager = CreateDefaultSubobject<UZL_AbilityUIManagerComponent>(TEXT("AbilityUIManager"));

	DamageWidgetComp = CreateDefaultSubobject<UZL_BaseDamageWidgetComponent>(TEXT("DamageNumberComp"));
	DamageWidgetComp->SetupAttachment(RootComponent);

	OverHeadDisplay = CreateDefaultSubobject<UZL_OverHeadWidgetComponent>(TEXT("OverHeadDisplay"));
	OverHeadDisplay->SetupAttachment(RootComponent);
}

void AZeroLockCharacter::InitializeFloatingStatusBar()
{
	if (IsNetMode(NM_DedicatedServer)) return;
	if (IsLocallyControlled()) return;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !OverHeadDisplay) return;
	
	UZL_OverHeadDisplay* WidgetInstance = Cast<UZL_OverHeadDisplay>(OverHeadDisplay->GetWidget());
	
	if (!WidgetInstance && OverHeadClass)
	{
		WidgetInstance = CreateWidget<UZL_OverHeadDisplay>(PC, OverHeadClass);
		OverHeadDisplay->SetWidget(WidgetInstance);
	}
	
	if (WidgetInstance)
	{
		OverHeadDisplayRef = WidgetInstance;

		if (UZL_VM_Attributes* VM = GetVM_Attributes())
		{
			WidgetInstance->SetViewModel(VM);

			if (AttributeSet)
			{
				VM->SetHealth(AttributeSet->GetCurrentHealth());
				VM->SetMaxHealth(AttributeSet->GetMaximumHealth());
			}
			//ZLOG("Overhead Widget Initialized and Linked");
		}
	}
}

void AZeroLockCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	ParryComp->SetVisibility(false);
	
	if (GetMesh())
	{
		GetMesh()->CustomDepthStencilValue = 1;
		GetMesh()->SetRenderInDepthPass(true);
	}
}

FCollisionQueryParams AZeroLockCharacter::GetIgnoreCharacterParams() const
{
	FCollisionQueryParams params;
	TArray<AActor*> CharacterChilder;
	GetAllChildActors(CharacterChilder);
	params.AddIgnoredActors(CharacterChilder);
	params.AddIgnoredActor(this);
	return params;
}

void AZeroLockCharacter::Death()
{
	// Optional death implementation
}

class UAbilitySystemComponent* AZeroLockCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComp;
}

UBaseCharAbilitySystemComponent* AZeroLockCharacter::GetMyAbilitySystemComp() const
{
	return AbilitySystemComp;
}

UBaseCharAttributeSet* AZeroLockCharacter::GetMyAttributeSet() const
{
	return AttributeSet;
}

void AZeroLockCharacter::MovementLocked(FGameplayTag GameplayTag, int NewCount)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (NewCount > 0)
	{
		// Tag added logic here if needed
	}
	else
	{
		// Tag removed logic here if needed
	}
}

void AZeroLockCharacter::InitializeAttributes()
{
	if (AbilitySystemComp && DefaultGameplayEffect)
	{
		FGameplayEffectContextHandle EffectContext = AbilitySystemComp->MakeEffectContext();
		EffectContext.AddSourceObject(this);

		FGameplayEffectSpecHandle SpecHandle = AbilitySystemComp->MakeOutgoingSpec(DefaultGameplayEffect, 1, EffectContext);

		if (SpecHandle.IsValid())
		{
			FActiveGameplayEffectHandle GEHandle = AbilitySystemComp->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
		
		FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(FName("ZeroLock.Stun"), false);
		FGameplayTag ParryTag = FGameplayTag::RequestGameplayTag(FName("ZeroLock.Melee.Parry"), false);
		
		AbilitySystemComp->RegisterGameplayTagEvent(StunTag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AZeroLockCharacter::Stunned);
		AbilitySystemComp->RegisterGameplayTagEvent(FGameplayTag::RequestGameplayTag("ZeroLock.Abilities.MovementLock"), EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AZeroLockCharacter::MovementLocked);
		AbilitySystemComp->RegisterGameplayTagEvent(ParryTag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AZeroLockCharacter::Parry);
		
		AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetCurrentHealthAttribute()).AddUObject(this, &AZeroLockCharacter::HealthAttributeChanged);
		AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetMaximumHealthAttribute()).AddUObject(this, &AZeroLockCharacter::HealthAttributeChanged);
		AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetCurrentSpeedAttribute()).AddUObject(this, &AZeroLockCharacter::SpeedAttributeChanged);
		AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetMaxAmmoAttribute()).AddUObject(this, &AZeroLockCharacter::AmmoAttributeChange);
		AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetCurrentAmmoAttribute()).AddUObject(this, &AZeroLockCharacter::AmmoAttributeChange);
		
		AbilitySystemComp->OnNewAbilityAdded.AddUniqueDynamic(this, &ThisClass::NewAbilityAddedLocal);
	}
}

void AZeroLockCharacter::NewAbilityAddedLocal(FGameplayAbilitySpec& AbilitySpec)
{
	if (!IsLocallyControlled()) return;
	
	UBaseGameplayAbility* Ability = Cast<UBaseGameplayAbility>(AbilitySpec.GetPrimaryInstance());
	EGASAbilityInputID InputID = static_cast<EGASAbilityInputID>(AbilitySpec.InputID);
	if (Ability)
	{
		AbilitiesArray.Add(FMyAbilityMap(Ability, InputID));
		BroadcastAbilitiesToUI(Ability, InputID);
	}
}

void AZeroLockCharacter::GiveAbilities()
{
	InitInputTagsMap();
	
	if (HasAuthority() && AbilitySystemComp)
	{
		for (TSubclassOf<UBaseGameplayAbility>& StartupAbility : DefaultAbilities)
		{
			DefaultAbilitiesHandles.Add(
				AbilitySystemComp->GiveAbility(FGameplayAbilitySpec(StartupAbility, 1, static_cast<int32>(StartupAbility.GetDefaultObject()->AbilityInputID), this)));
		}
		if (PrimaryFireAbility)
		{
			DefaultAbilitiesHandles.Add(
				AbilitySystemComp->GiveAbility(FGameplayAbilitySpec(PrimaryFireAbility, 1, static_cast<int32>(PrimaryFireAbility.GetDefaultObject()->AbilityInputID), this)));
		}
		
		GrantAbilityOfClassX(SecondryFireAbility, EGASAbilityInputID::Secondry_Attack, true);
		GrantAbilityOfClassX(Ability_1, EGASAbilityInputID::Ability_1, true);
		GrantAbilityOfClassX(Ability_2, EGASAbilityInputID::Ability_2, true);
		GrantAbilityOfClassX(UltimateAbility, EGASAbilityInputID::Ultimate, true);
		GrantAbilityOfClassX(ReloadAbility, EGASAbilityInputID::Reload);
		GrantAbilityOfClassX(HeavyMeleeAbility, EGASAbilityInputID::Melee);
		GrantAbilityOfClassX(LightMeleeAbility, EGASAbilityInputID::None);
		GrantAbilityOfClassX(ParryAbility, EGASAbilityInputID::Parry);
	}
}

void AZeroLockCharacter::BroadcastAbilitiesToUI(UBaseGameplayAbility* Ability , EGASAbilityInputID InputID)
{	
	AbilitiesArray.Add(FMyAbilityMap(Ability, InputID));
}

void AZeroLockCharacter::GrantAbilityOfClassX(TSubclassOf<class UBaseGameplayAbility> AbilityToGrant, EGASAbilityInputID InputToBindTo, bool brodcast)
{
	if (AbilityToGrant)
	{
		EGASAbilityInputID AbiltyInputID = InputToBindTo;
		FGameplayAbilitySpecHandle GrantedHandle;
		FGameplayAbilitySpec GrantedSpec = FGameplayAbilitySpec(AbilityToGrant, 0, static_cast<int32>(AbiltyInputID), this);
		
		if (inputTags.Contains(AbiltyInputID))
		{
			GrantedSpec.GetDynamicSpecSourceTags().AddTag(inputTags.FindRef(InputToBindTo));
		}
		
		if (AbilityToGrant.GetDefaultObject()->TargetStyle == EGASTargetConfirmationStyle::Passive)
		{
			EGASAbilityInputID AbiltyInputIDX = EGASAbilityInputID::None;
			GrantedSpec.InputID = static_cast<int32>(AbiltyInputIDX);
			GrantedHandle = AbilitySystemComp->GiveAbility(GrantedSpec);
			DefaultAbilitiesHandles.Add(GrantedHandle);
		}
		else
		{
			GrantedHandle = AbilitySystemComp->GiveAbility(GrantedSpec);
			DefaultAbilitiesHandles.Add(GrantedHandle);
		}
		FGameplayAbilitySpec* Spec = GetAbilitySystemComponent()->FindAbilitySpecFromHandle(GrantedHandle);
		UBaseGameplayAbility* Ability = Cast<UBaseGameplayAbility>(Spec->Ability);
		
		if (Ability)
		{
			Ability->SetInputID(InputToBindTo);
		}
	}
}

UZero_Item_Inventory_Component* AZeroLockCharacter::GetInventoryComponent() const
{
	return ItemInventoryComp;
}

void AZeroLockCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	AbilitySystemComp->InitAbilityActorInfo(this, this);

	CreateVM_Att();
	InitializeAttributes();
	GiveAbilities();
	InitializeFloatingStatusBar();
}

void AZeroLockCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	AbilitySystemComp->InitAbilityActorInfo(this, this);

	InitializeAttributes();
	
	CreateVM_Att();
	InitializeFloatingStatusBar();
}

void AZeroLockCharacter::PrimaryFirePressed()
{
	if (!PrimaryFireAbility || !AttributeSet) return;

	bIsPrimaryPressed = true;
	float FirstDelay = FMath::Max(TimeOfLastShot + AttributeSet->FireRate.GetCurrentValue() - GetWorld()->TimeSeconds, 0.0f);
	float FireRate = AttributeSet->FireRate.GetCurrentValue();
	
	GetWorldTimerManager().SetTimer(PrimaryFireTickHandle, this, &AZeroLockCharacter::PrimaryFireTickFunction, FireRate, true, FirstDelay);
}

void AZeroLockCharacter::PrimaryFireReleased()
{
	bIsPrimaryPressed = false;
	GetWorldTimerManager().ClearTimer(PrimaryFireTickHandle);
}

bool AZeroLockCharacter::IsAlive()
{
	if (AttributeSet)
	{
		return AttributeSet->GetCurrentHealth() > 0;
	}
	return false;
}

void AZeroLockCharacter::ChangeFireRate()
{
	if (!bIsPrimaryPressed) return;
	float FirstDelay = FMath::Max(TimeOfLastShot + AttributeSet->FireRate.GetCurrentValue() - GetWorld()->TimeSeconds, 0.0f);
	
	float FireRate = AttributeSet->FireRate.GetCurrentValue();
	GetWorldTimerManager().SetTimer(PrimaryFireTickHandle, this, &AZeroLockCharacter::PrimaryFireTickFunction, FireRate, true, FirstDelay);
}

void AZeroLockCharacter::PrimaryFireTickFunction()
{
	if (AttributeSet->GetCurrentAmmo() > 0)
	{
		AbilitySystemComp->TryActivateAbilityByClass(PrimaryFireAbility, true);
		TimeOfLastShot = GetWorld()->TimeSeconds;
	}
	else
	{
		PrimaryFireReleased();
	}
}

void AZeroLockCharacter::SecondryFirePressed()
{
	GetAbilitySystemComponent()->AbilityLocalInputPressed(static_cast<int32>(EGASAbilityInputID::Secondry_Attack));
}

void AZeroLockCharacter::SecondryFireReleased()
{
	GetAbilitySystemComponent()->AbilityLocalInputReleased(static_cast<int32>(EGASAbilityInputID::Secondry_Attack));
}

void AZeroLockCharacter::Ability_1Pressed()
{
	GetAbilitySystemComponent()->AbilityLocalInputPressed(static_cast<int32>(EGASAbilityInputID::Ability_1));
}

void AZeroLockCharacter::Ability_1Released()
{
	GetAbilitySystemComponent()->AbilityLocalInputReleased(static_cast<int32>(EGASAbilityInputID::Ability_1));
}

void AZeroLockCharacter::Ability_2Pressed()
{
	GetAbilitySystemComponent()->AbilityLocalInputPressed(static_cast<int32>(EGASAbilityInputID::Ability_2));
}

void AZeroLockCharacter::Ability_2Released()
{
	GetAbilitySystemComponent()->AbilityLocalInputReleased(static_cast<int32>(EGASAbilityInputID::Ability_2));
}

void AZeroLockCharacter::UltimateAbilityPressed()
{
	GetAbilitySystemComponent()->AbilityLocalInputPressed(static_cast<int32>(EGASAbilityInputID::Ultimate));
}

void AZeroLockCharacter::UltimateAbilityReleased()
{
	GetAbilitySystemComponent()->AbilityLocalInputReleased(static_cast<int32>(EGASAbilityInputID::Ultimate));
}

void AZeroLockCharacter::Reload()
{
	GetAbilitySystemComponent()->AbilityLocalInputPressed(static_cast<int32>(EGASAbilityInputID::Reload));
}

void AZeroLockCharacter::HealthChanged(float currentH , float MaxH)
{
	if (HealthChangeDelegate.IsBound())
	{
		HealthChangeDelegate.Broadcast(currentH, MaxH);
	}
	if (UZL_VM_Attributes* VM = GetVM_Attributes())
	{
		VM->SetHealth(currentH);
		VM->SetMaxHealth(MaxH);
	}
}

void AZeroLockCharacter::OnTakeDamage(float currentH)
{
	if (DamageRecievedDelegate.IsBound())
	{
		DamageRecievedDelegate.Broadcast(currentH);
	}
}

void AZeroLockCharacter::AddLastHit(AZeroLockCharacter* Character)
{
	if (!IsAlive()) return;
	if (!Character) return;
	
	float CurrentTime = GetWorld()->GetTimeSeconds();
	LastHitCharacter = Character;

	if (AssistTimeMap.Contains(Character))
	{
		AssistTimeMap.Remove(Character);
		AssistTimeMap.Add(Character, CurrentTime);
	}
	if (!AssistListCharacters.Contains(Character))
	{
		AssistListCharacters.Add(Character);
	}
	ClearAssistList(CurrentTime);
}

void AZeroLockCharacter::ClearAssistList(float currentTime)
{
	TArray<AZeroLockCharacter*> ToRemove;

	for (auto& Pair : AssistTimeMap)
	{
		if (currentTime - Pair.Value > AssistWindow)
		{
			ToRemove.Add(Pair.Key);
		}
	}

	for (AZeroLockCharacter* Entry : ToRemove)
	{
		AssistListCharacters.Remove(Entry);
		AssistTimeMap.Remove(Entry);
	}
}

void AZeroLockCharacter::OnDied(AController* Killer, AController* Victim)
{
	if (!AbilitySystemComp->GetOwner()->HasAuthority()) return;

	AbilitySystemComp->CancelAllAbilities();
	AbilitySystemComp->RemoveActiveEffectsWithTags(FGameplayTagContainer());

	if (AZero_BaseGameModeBase* GM = GetWorld()->GetAuthGameMode<AZero_BaseGameModeBase>())
	{
		GM->Killed(Killer, Victim);
	}
}

void AZeroLockCharacter::ResetCharacter()
{
	AZero_BasePlayerController* PC = Cast<AZero_BasePlayerController>(GetController());
	if (PC) SetActorLocation(PC->SelectedStartLocation);

	if (!AbilitySystemComp->GetOwner()->HasAuthority()) return;

	AbilitySystemComp->CancelAllAbilities();
	AbilitySystemComp->RemoveActiveEffectsWithTags(FGameplayTagContainer());
	AbilitySystemComp->RemoveActiveEffects(FGameplayEffectQuery()); 

	AbilitySystemComp->InitAbilityActorInfo(this, this);

	ResetAllAbilities();

	float Max = AttributeSet->GetMaximumHealth();
	AttributeSet->SetCurrentHealth(Max);
	if (PC) EnableInput(PC);
	bIsDead = false;
}

void AZeroLockCharacter::PassiveAbilityRestart(TSubclassOf<class UBaseGameplayAbility> AbilityToGrant)
{
	if (!AbilityToGrant) return;
	if (AbilityToGrant.GetDefaultObject()->TargetStyle == EGASTargetConfirmationStyle::Passive)
	{
		AbilitySystemComp->TryActivateAbilityByClass(AbilityToGrant);
	}
}

void AZeroLockCharacter::ResetAllAbilities()
{
	PassiveAbilityRestart(SecondryFireAbility);
	PassiveAbilityRestart(Ability_1);
	PassiveAbilityRestart(Ability_2);
	PassiveAbilityRestart(UltimateAbility);
	
	for (TSubclassOf<UBaseGameplayAbility> ability : DefaultAbilities)
	{
		PassiveAbilityRestart(ability);
	}
}

void AZeroLockCharacter::HandleDeath()
{
	if (bIsDead) return;
	
	APlayerController* PC = Cast<APlayerController>(GetController());
	AbilitySystemComp->CancelAllAbilities();
	AbilitySystemComp->RemoveActiveEffectsWithTags(FGameplayTagContainer());
	bIsDead = true;
		
	if (PC)
	{
		DisableInput(PC);
		ServerHandleDeath(PC);
	}
}

void AZeroLockCharacter::ServerHandleDeath_Implementation(APlayerController* PC)
{
	if (AZero_BaseGameModeBase* GM = GetWorld()->GetAuthGameMode<AZero_BaseGameModeBase>())
	{
		GM->HandlePlayerDeath(this, PC);
	}
}

void AZeroLockCharacter::OnRep_AbilityUIData()
{
	for (FMyAbilityMap unit : AbilitiesArray)
	{
		AddAbilityIconDelegate.Broadcast(unit.Ability, unit.InputID);
	}
}

void AZeroLockCharacter::InitInputTagsMap()
{
	inputTags.Add(EGASAbilityInputID::Secondry_Attack, ZerolockGameplayTagsForBinding::TAG_INPUT_SECONDRY);
	inputTags.Add(EGASAbilityInputID::Ability_1, ZerolockGameplayTagsForBinding::TAG_INPUT_ABILITY_1);
	inputTags.Add(EGASAbilityInputID::Ability_2, ZerolockGameplayTagsForBinding::TAG_INPUT_ABILITY_2);
	inputTags.Add(EGASAbilityInputID::Ultimate, ZerolockGameplayTagsForBinding::TAG_INPUT_ULTIMATE);
}

void AZeroLockCharacter::HandleWeaponHitEvent(const FGameplayEventData& EventData)
{
	OnWeaponHitEventReceived.Broadcast(EventData);
}

UZL_VM_Attributes* AZeroLockCharacter::GetVM_Attributes()
{
	if (!VM_Attributes)
	{
		CreateVM_Att();
	}
	if (AZero_BasePlayerState* PS = GetPlayerState<AZero_BasePlayerState>())
	{
		if (!PS->CurrentVM)
		{
			PS->SetCurrentVM(VM_Attributes);
		}
	}
	return VM_Attributes;
}

UObject* AZeroLockCharacter::GetVMObject_Attributes()
{
	return GetVM_Attributes();
}

void AZeroLockCharacter::CreateVM_Att()
{
	if (VM_Attributes) return;
	VM_Attributes = NewObject<UZL_VM_Attributes>(this);

	if (AttributeSet)
	{
		VM_Attributes->SetHealth(AttributeSet->GetCurrentHealth());
		VM_Attributes->SetMaxHealth(AttributeSet->GetMaximumHealth());
		VM_Attributes->SetAmmo(static_cast<int32>(AttributeSet->GetCurrentAmmo()));
		VM_Attributes->SetMaxAmmo(static_cast<int32>(AttributeSet->GetMaxAmmo()));
		VM_Attributes->SetIsInfiniteAmmo(false);
	}
	if (AZero_BasePlayerState* PS = GetPlayerState<AZero_BasePlayerState>())
	{
		PS->SetCurrentVM(VM_Attributes);
	}
}

bool AZeroLockCharacter::IsOnSameTeam(AZeroLockCharacter* CharacterToCheck)
{
	AZero_BasePlayerState* MyPS = Cast<AZero_BasePlayerState>(GetPlayerState());
	if (!MyPS) return false;
	
	AZero_BasePlayerState* OtherPS = Cast<AZero_BasePlayerState>(CharacterToCheck->GetPlayerState());
	if (!OtherPS) return false;
	
	return MyPS->TeamID == OtherPS->TeamID;
}

void AZeroLockCharacter::AddDamageNumber(float Damage, FGameplayTagContainer DamageNumberTags)
{
	DamageWidgetComp->ShowDamageNumber(Damage, DamageNumberTags);
}

void AZeroLockCharacter::ShowDamageNumber()
{
}

void AZeroLockCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Bind MoverPawn generic movement logic
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) 
	{
		EnhancedInputComponent->BindAction(EI_PrimaryFire, ETriggerEvent::Started, this, &AZeroLockCharacter::PrimaryFirePressed);
		EnhancedInputComponent->BindAction(EI_PrimaryFire, ETriggerEvent::Completed, this, &AZeroLockCharacter::PrimaryFireReleased);

		EnhancedInputComponent->BindAction(EI_SecondryFire, ETriggerEvent::Started, this, &AZeroLockCharacter::SecondryFirePressed);
		EnhancedInputComponent->BindAction(EI_SecondryFire, ETriggerEvent::Completed, this, &AZeroLockCharacter::SecondryFireReleased);

		EnhancedInputComponent->BindAction(EI_Ability1, ETriggerEvent::Started, this, &AZeroLockCharacter::Ability_1Pressed);
		EnhancedInputComponent->BindAction(EI_Ability1, ETriggerEvent::Completed, this, &AZeroLockCharacter::Ability_1Released);

		EnhancedInputComponent->BindAction(EI_Ability2, ETriggerEvent::Started, this, &AZeroLockCharacter::Ability_2Pressed);
		EnhancedInputComponent->BindAction(EI_Ability2, ETriggerEvent::Completed, this, &AZeroLockCharacter::Ability_2Released);

		EnhancedInputComponent->BindAction(EI_Ultimate, ETriggerEvent::Started, this, &AZeroLockCharacter::UltimateAbilityPressed);
		EnhancedInputComponent->BindAction(EI_Ultimate, ETriggerEvent::Completed, this, &AZeroLockCharacter::UltimateAbilityReleased);
		
		EnhancedInputComponent->BindAction(EI_Reload, ETriggerEvent::Started, this, &AZeroLockCharacter::Reload);

		EnhancedInputComponent->BindAction(EI_Parry, ETriggerEvent::Started, this, &AZeroLockCharacter::ParryPressed);

		EnhancedInputComponent->BindAction(EI_Confirm, ETriggerEvent::Completed, AbilitySystemComp, &UAbilitySystemComponent::LocalInputConfirm);
		EnhancedInputComponent->BindAction(EI_Cancel, ETriggerEvent::Completed, AbilitySystemComp, &UAbilitySystemComponent::LocalInputCancel);

		EnhancedInputComponent->BindAction(EI_UIInfo, ETriggerEvent::Started, this, &AZeroLockCharacter::UIInfoPressed);
		EnhancedInputComponent->BindAction(EI_UIInfo, ETriggerEvent::Completed, this, &AZeroLockCharacter::UIInfoReleased);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component!"), *GetNameSafe(this));
	}
	
	if (AbilitySystemComp && InputComponent)
	{
		const FGameplayAbilityInputBinds Binds("Confirm", "Cancel", FTopLevelAssetPath(GetPathNameSafe(UClass::TryFindTypeSlow<UEnum>(TEXT("EGASAbilityInputID")))), static_cast<int32>(EGASAbilityInputID::Confirm), static_cast<int32>(EGASAbilityInputID::Cancel));
		AbilitySystemComp->BindAbilityActivationToInputComponent(InputComponent, Binds);
	}
}

void AZeroLockCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZeroLockCharacter, bIsDead);
	DOREPLIFETIME(AZeroLockCharacter, StartLocation);
}

void AZeroLockCharacter::Stunned(FGameplayTag GameplayTag, int NewCount)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (NewCount > 0)
	{
		AbilitySystemComp->CancelAllAbilities();
		if (PC) DisableInput(PC);
		
		if (StunChangedDelegate.IsBound())
		{
			StunChangedDelegate.Broadcast(true);
		}
	}
	else
	{
		if (PC) EnableInput(PC);
		
		if (StunChangedDelegate.IsBound())
		{
			StunChangedDelegate.Broadcast(false);
		}
	}
}

void AZeroLockCharacter::Parry(FGameplayTag GameplayTag, int NewCount)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (NewCount > 0)
	{
		ParryComp->SetVisibility(true);
		if (PC) DisableInput(PC);
	}
	else
	{
		ParryComp->SetVisibility(false);
		if (PC) EnableInput(PC);
	}
}

void AZeroLockCharacter::HealthAttributeChanged(const FOnAttributeChangeData& OnAttributeChangeData)
{
	if (!AttributeSet) return;
	
	float currentH= AttributeSet->GetCurrentHealth();
	float MaxH = AttributeSet->GetMaximumHealth();

	if (OverHeadDisplay && !OverHeadDisplayRef)
	{
		InitializeFloatingStatusBar();
	}
	
	if (HealthChangeDelegate.IsBound())
	{
		HealthChangeDelegate.Broadcast(currentH, MaxH);
	}
	if (currentH <= 0.0f && currentH < MaxH)
	{
		AddEventForDeath();
	}
	OnTakeDamage(currentH);
	
	if (UZL_VM_Attributes* VM = GetVM_Attributes())
	{
		VM->SetHealth(currentH);
		VM->SetMaxHealth(MaxH);
	}
	
	if (OverHeadDisplay && !OverHeadDisplayRef)
	{
		InitializeFloatingStatusBar();
	}
}

void AZeroLockCharacter::AmmoAttributeChange(const FOnAttributeChangeData& OnAttributeChangeData)
{
	if (!AttributeSet) return;
	float currentA= AttributeSet->GetCurrentAmmo();
	float MaxA = AttributeSet->GetMaxAmmo();
	
	GetVM_Attributes()->SetMaxAmmo(static_cast<int32>(MaxA));
	GetVM_Attributes()->SetAmmo(static_cast<int32>(currentA));
}

void AZeroLockCharacter::SpeedAttributeChanged(const FOnAttributeChangeData& OnAttributeChangeData)
{
	if (!AttributeSet) return;
	// TODO: Forward current speed to UZeroMoverComponent modifier
	// float currentS = OnAttributeChangeData.NewValue;
}

void AZeroLockCharacter::AddEventForDeath()
{
	if (LastHitCharacter)
	{
		FGameplayTag DeathTag = FGameplayTag::RequestGameplayTag("Event.Death");
		LastHitCharacter->GetMyAbilitySystemComp()->SendGameplayEventToTarget(DeathTag, GetAbilitySystemComponent());
		FGameplayTag KillTag = FGameplayTag::RequestGameplayTag("Event.Kill");
		LastHitCharacter->GetMyAbilitySystemComp()->SendGameplayEventToSelf(KillTag, GetAbilitySystemComponent());
	}
	if (AssistListCharacters.Num() > 0)
	{
		for (AZeroLockCharacter* AssitChar : AssistListCharacters)
		{
			if (!AssitChar) continue;
			if (AssitChar == LastHitCharacter) continue;
			
			FGameplayTag AssistTag = FGameplayTag::RequestGameplayTag("Event.Assist");
			AssitChar->GetMyAbilitySystemComp()->SendGameplayEventToTarget(AssistTag, GetAbilitySystemComponent());
		}
	}
}

void AZeroLockCharacter::MeleePressed()
{
	// Implementation for legacy bindings if you plan to move it over entirely
}

void AZeroLockCharacter::MeleeReleased()
{
	// Legacy
}

void AZeroLockCharacter::ParryPressed()
{
	GetAbilitySystemComponent()->AbilityLocalInputPressed(static_cast<int32>(EGASAbilityInputID::Parry));
}

void AZeroLockCharacter::UIInfoPressed()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
		PC->SetInputMode(FInputModeGameAndUI());
	}
}

void AZeroLockCharacter::UIInfoReleased()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
}