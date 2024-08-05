// Copyright � 2023 by Tyni Boat. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentAndBase/BaseControllerState.h"
#include "SimpleGroundState.generated.h"

/**
 * The SImple ground base movement state using component shape.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular State Behaviours", abstract)
class MODULARCONTROLLER_API USimpleGroundState : public UBaseControllerState
{
	GENERATED_BODY()


#pragma region Check XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:
	
	// The surface conditions
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	FSurfaceCheckParams SurfaceParams;

	// The speed used to "snap" the controller ton the surface.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main", meta=(ClampMin = 0, ClampMax = 1))
	float SnapSpeed = 1;

	// The ground collision Channel.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	TEnumAsByte<ECollisionChannel> GroundObjectType;

	// The State's Root motion Mode
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Main")
	ERootMotionType RootMotionMode;

	// The name of the ground distance additional variable
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	FName GroundDistanceVarName = "GroundDistance";

	// The base name of the ground move additional variable. X, Y and Z is added to this name for each component
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	FName GroundMoveVarName = "GroundMoveAmount";


	//------------------------------------------------------------------------------------------


public:
	//Check for a valid surfaces and return the flag corresponding to their indexes. the first surface is the main one, the second is the bad angle one.
	virtual int CheckSurfaceIndex(UModularControllerComponent* controller, const FControllerStatus status, FStatusParameters& statusParams, const float inDelta, bool asActive = false) const;

#pragma endregion


#pragma region General Movement XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:
	//[Axis] The name of the lock-on input direction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement")
	FName LockOnDirection = "LockOnDirection";


public:
	
	//Get the move acceleration vector.
	virtual FVector GetMoveVector(const FVector inputVector, const float moveScale, const FSurface Surface, const FVector gravity) const;


#pragma endregion


#pragma region Slope And Sliding XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:

	// Should the slope increase or decrease speed when we are ascending and descending?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Slope and Slide")
	bool bSlopeAffectSpeed = true;

#pragma endregion


#pragma region Move XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:
	// The maximum speed of the controller on the surface (cm/s)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Move Parameters")
	float MaxSpeed = 450;

	// The movement acceleration
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Move Parameters")
	float Acceleration = 8;

	// The speed used to rotate toward movement direction
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Move Parameters")
	float TurnSpeed = 15;

	// The Turn curve
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Move Parameters")
	EAlphaBlendOption TurnCurve = EAlphaBlendOption::ExpOut;

	// The speed used to rotate toward sliding direction
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Move Parameters")
	float SlideTurnSpeed = 5;

	// The turn first before moving?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Move Parameters")
	bool bMoveOnlyForward = false;

#pragma endregion


#pragma region Functions XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
public:
	virtual FControllerCheckResult CheckState_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float inDelta,
	                                                         bool asLastActiveState) const override;

	virtual void OnEnterState_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput,
	                                         const float delta) const override;

	virtual FControllerStatus ProcessState_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta) const override;

	virtual void OnExitState_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput,
	                                        const float delta) const override;


	virtual FString DebugString() const override;


#pragma endregion
};
