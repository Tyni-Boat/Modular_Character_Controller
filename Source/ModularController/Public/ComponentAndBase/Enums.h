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
	RootMotionType_Override,
	RootMotionType_Additive,
};



/// <summary>
/// The ground state
/// </summary>
UENUM(BlueprintType)
enum EGroundLocomotionMode
{
	GroundLocomotionMode_Jogging,
	GroundLocomotionMode_Sprinting,
	GroundLocomotionMode_Crouching,
	GroundLocomotionMode_Crawling,
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
	ControllerDebugType_None,
	ControllerDebugType_MovementDebug,
	ControllerDebugType_PhysicDebug,
	ControllerDebugType_NetworkDebug,
	ControllerDebugType_StatusDebug,
	ControllerDebugType_AnimationDebug,
	ControllerDebugType_InputDebug,
};


/// <summary>
/// The type of compatibility mode an controller action.
/// </summary>
UENUM(BlueprintType)
enum EActionCompatibilityMode
{
	ActionCompatibilityMode_AlwaysCompatible,
	ActionCompatibilityMode_OnCompatibleStateOnly,
	ActionCompatibilityMode_WhileCompatibleActionOnly,
	ActionCompatibilityMode_OnBothCompatiblesStateAndAction,
};


/// <summary>
/// The type of phases an action can be in
/// </summary>
UENUM(BlueprintType)
enum EActionPhase
{
	ActionPhase_Undetermined,
	ActionPhase_Anticipation,
	ActionPhase_Active,
	ActionPhase_Recovery,
};