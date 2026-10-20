// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
// Helper Macros
#if 1
#define ZLOG(x) GEngine->AddOnScreenDebugMessage(-1, 2.f ? 2.f : -1.f, FColor::Yellow, x);
#define ZLOG_COLOR_TIME(x,c,t) GEngine->AddOnScreenDebugMessage(-1,t,c,x);
#define ZPOINT(x, c) DrawDebugPoint(GetWorld(), x, 10, c, !2.f, 2.f);
#define ZLINE(x1, x2, c) DrawDebugLine(GetWorld(), x1, x2, c, !2.f, 2.f);
#define ZCAPSULE(x, c) DrawDebugCapsule(GetWorld(), x, CapHH(), CapR(), FQuat::Identity, c, !MacroDuration, MacroDuration);
#else
#define ZLOG(x)
#define ZPOINT(x, c)
#define ZLINE(x1, x2, c)
#define ZCAPSULE(x, c)
#define ZLOG_COLOR_TIME(x,c,t)
#endif

UENUM(BlueprintType)
enum class EGASAbilityInputID : uint8
{
	None  UMETA(DisplayName = "None"),
	Confirm UMETA(DisplayName = "Confirm"),
	Cancel UMETA(DisplayName = "Cancel"),
	Primary_Attack UMETA(DisplayName = "PrimaryAttack"),
	Secondry_Attack UMETA(DisplayName = "SecondryAttack"),
	Ability_1 UMETA(DisplayName = "Ability_1"),
	Ability_2 UMETA(DisplayName = "Ability_2"),
	Ultimate UMETA(DisplayName = "Ultimate"),
	Melee UMETA(DisplayName = "Melee"),
	Parry UMETA(DisplayName = "Parry"),
	Reload UMETA(DisplayName = "Reload"),
	LightMelee UMETA(DisplayName = "LightMelee")
};
UENUM(BlueprintType)
enum class EGASTargetConfirmationStyle : uint8
{
	Instant,
	Quick,
	Confirm,
	Passive
};

UENUM(BlueprintType)
enum class ETeamID : uint8
{
	TeamNull = 0,
	TeamBlue,
	TeamRed
};