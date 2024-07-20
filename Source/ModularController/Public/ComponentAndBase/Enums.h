// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "UObject/Class.h"
#include "CoreMinimal.h"



/*
* The type of an Input
*/
UENUM(BlueprintType)
enum class EInputEntryType: uint8
{
	Simple,
	Buffered,
};

/*
* The type of an Input Phase
*/
UENUM(BlueprintType)
enum class EInputEntryPhase: uint8
{
	None,
	Pressed,
	Held,
	Released,
};

/*
* The nature of an Input
*/
UENUM(BlueprintType)
enum class EInputEntryNature: uint8
{
	Axis,
	Value,
	Button,
};



/*
* The type of root motion to apply
*/
UENUM(BlueprintType)
enum class ERootMotionType: uint8
{
	NoRootMotion,
	Override,
	Additive,
};


/// <summary>
/// The type of debug to use on the controller.
/// </summary>
UENUM(BlueprintType)
enum class EControllerDebugType: uint8
{
	None,
	MovementDebug,
	PhysicDebug,
	NetworkDebug,
	StatusDebug,
	AnimationDebug,
	InputDebug,
};


/// <summary>
/// The type of compatibility mode an controller action.
/// </summary>
UENUM(BlueprintType)
enum class EActionCompatibilityMode: uint8
{
	AlwaysCompatible,
	OnCompatibleStateOnly,
	WhileCompatibleActionOnly,
	OnBothCompatiblesStateAndAction,
};


/// <summary>
/// The type of phases an action can be in
/// </summary>
UENUM(BlueprintType)
enum class EActionPhase: uint8
{
	Undetermined,
	Anticipation,
	Active,
	Recovery,
};

/// <summary>
/// The sixaxis directionnal enum 
/// </summary>
UENUM(BlueprintType)
enum class ESixAxisDirectionType: uint8
{
	NoDirection,
	Forward,
	Backward,
	Left,
	Right,
	UpSide,
	Downside,
};
