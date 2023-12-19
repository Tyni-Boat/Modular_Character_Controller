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
enum EGroundStateMode
{
	GroundStateMode_No_Ground,
	GroundStateMode_StableGround,
	GroundStateMode_SlidingSurface,
	GroundStateMode_StairCases,
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

