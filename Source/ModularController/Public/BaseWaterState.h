// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentAndBase/BaseControllerState.h"
#include "BaseWaterState.generated.h"


/**
 * The Water base movement state using component shape.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular State Behaviours", abstract)
class MODULARCONTROLLER_API UBaseWaterState : public UBaseControllerState
{
	GENERATED_BODY()
	
#pragma region Check XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:

	// The minimum immersion distance to be considered in water
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	float MinimumEntryImmersion = 150;

	// The maximum immersion distance (Must be less than MinimumEntryImmersion). past it and it will be considered out of water
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	float MaximumOutroImmersion = 140;

	// The maximum distance to check the water
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	float MaxWaterCheckDeep = 5000;

	// The scale of the archimed force pushing the controller to the surface.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	float ArchimedForceScale = 0.5;

	// The drag of the water.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	float WaterDrag = 100;
	
	// The ground collision Channel.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	TEnumAsByte<ECollisionChannel> ChannelWater;

	// The State's Root motion Mode
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Main")
	TEnumAsByte<ERootMotionType> RootMotionMode;

	// The name of the water surface distance additional variable
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	FName WaterSurfaceDistanceVarName = "WaterSurfaceDistance";


	//------------------------------------------------------------------------------------------


public:
	//Check for a valid water surface and return it's index
	virtual int CheckSurfaceIndex(UModularControllerComponent* controller, const FControllerStatus status, FStatusParameters& statusParams, const float inDelta, float previousWaterDistance, bool asActive = false) const;

#pragma endregion


#pragma region Move XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:
	// The maximum speed of the controller on the surface (cm/s)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Move Parameters")
	float MaxSpeed = 250;

	// The speed used to rotate toward movement direction
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Move Parameters")
	float TurnSpeed = 2;


	// To control the controller in the water
	virtual FVector WaterControl(FVector desiredMove, FVector horizontalVelocity, float delta) const;

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
