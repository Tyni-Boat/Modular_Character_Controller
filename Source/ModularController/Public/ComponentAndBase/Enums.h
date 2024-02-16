// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "UObject/Class.h"
#include "CoreMinimal.h"



/*
* The type of an Input
*/
UENUM(BlueprintType)
enum EInputEntryType
{
	InputEntryType_Simple,
	InputEntryType_Buffered,
};

/*
* The type of an Input Phase
*/
UENUM(BlueprintType)
enum EInputEntryPhase
{
	InputEntryPhase_None,
	InputEntryPhase_Pressed,
	InputEntryPhase_Held,
	InputEntryPhase_Released,
};

/*
* The nature of an Input
*/
UENUM(BlueprintType)
enum EInputEntryNature
{
	InputEntryNature_Axis,
	InputEntryNature_Value,
	InputEntryNature_Button,
};



/*
* The type of root motion to apply
*/
UENUM(BlueprintType)
enum ERootMotionType
{
	RootMotionType_No_RootMotion,
	RootMotionType_OverridePrimary,
	RootMotionType_OverrideSecondary,
	RootMotionType_OverrideAll,
	RootMotionType_AdditivePrimary,
	RootMotionType_AdditiveSecondary,
};



/// <summary>
/// The ground state
/// </summary>
UENUM(BlueprintType)
enum EGroundLocomotionMode
{
	Jogging,
	Sprinting,
	Crouching,
	Crawling,
};


/// <summary>
/// The check shape enum
/// </summary>
UENUM(BlueprintType)
enum EShapeMode
{
	ShapeMode_Sphere,
	ShapeMode_Cube,
};



/// <summary>
/// The type of debug to use on the controller.
/// </summary>
UENUM(BlueprintType)
enum EControllerDebugType
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
enum EActionCompatibilityMode
{
	AlwaysCompatible,
	OnCompatibleStateOnly,
	WhileCompatibleActionOnly,
	OnBothCompatiblesStateAndAction,
};