// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "ComponentAndBase/BaseControllerState.h"
#include "FreeFallState.generated.h"



/**
 A Free fall behaviour for the Modular controller component
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular State Behaviours", abstract)
class MODULARCONTROLLER_API UFreeFallState : public UBaseControllerState
{
	GENERATED_BODY()

#pragma region Parameters
protected:
	
	// The maximun velocity the user can move planar to the gravity force
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Air Control")
	float AirControlSpeed = 300;

	// In air controlled user rotation speed
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Air Control")
	float AirControlRotationSpeed = 3;

	// The Gravity force
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Gravity")
	FVector Gravity = FVector(0, 0, -981);
	

#pragma endregion
		
#pragma region Air Velocity and Checks
public:

	
	// To control the controller in the air
	virtual FVector AirControl(FVector desiredMove, FVector horizontalVelocity, float delta) const;


	// Apply Gravity force
	virtual FVector AddGravity(FVector currentAcceleration) const;
			

#pragma endregion

#pragma region Functions
public:

	virtual FControllerCheckResult CheckState_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float inDelta, bool asLastActiveState) const override;

	virtual void OnEnterState_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const override;

	virtual FControllerStatus ProcessState_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta) const override;


	virtual FString DebugString() const override;
	
#pragma endregion

};
