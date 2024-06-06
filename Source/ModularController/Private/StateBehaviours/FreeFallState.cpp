// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "StateBehaviours/FreeFallState.h"
#include "Kismet/KismetMathLibrary.h"


#pragma region Air Velocity and Checks


FVector UFreeFallState::AirControl(FVector desiredMove, FVector horizontalVelocity, float delta)
{
	if (desiredMove.Length() > 0)
	{
		FVector resultingVector = horizontalVelocity + desiredMove * delta;
		if (resultingVector.Length() > AirControlSpeed)
		{
			const float maxAllowedAdd = AirControlSpeed - horizontalVelocity.Length();
			resultingVector = horizontalVelocity + (maxAllowedAdd > 0 ? desiredMove.GetSafeNormal() * maxAllowedAdd : FVector(0));
		}
		return resultingVector;
	}
	return horizontalVelocity;
}


FVector UFreeFallState::AddGravity(FVector currentAcceleration)
{
	return currentAcceleration + Gravity;
}


void UFreeFallState::SetGravityForce(FVector newGravity, UModularControllerComponent* controller)
{
	Gravity = newGravity;
	if(controller)
	{
		if(controller->_currentActiveGravityState == this)
		{
			controller->SetGravity(newGravity, this);
		}
	}
}



#pragma endregion


#pragma region Functions


FControllerCheckResult UFreeFallState::CheckState_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, const float inDelta, bool asLastActiveState)
{
	return FControllerCheckResult(true, startingConditions);
}

FKinematicComponents UFreeFallState::OnEnterState_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta)
{
	if (controller)
		controller->SetGravity(Gravity, this);
	_airTime = 0;
	return Super::OnEnterState_Implementation(controller, startingConditions, moveInput, delta);
}

FControllerStatus UFreeFallState::ProcessState_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, const float delta)
{
	FControllerStatus processResult = startingConditions;

	//Input handling
	auto inputAxis = processResult.MoveInput;
	if (inputAxis.Normalize())
	{
		const FVector planarInput = FVector::VectorPlaneProject(inputAxis, Gravity.GetSafeNormal());
		const FVector resultingVector = planarInput * AirControlSpeed;
		inputAxis = resultingVector;
	}

	//Status loading
	//if (startingConditions.ControllerStatus.StateModifiers1.X > _airTime)
	//	_airTime = startingConditions.ControllerStatus.StateModifiers1.X;

	//Components separation
	const FVector HorizontalVelocity = FVector::VectorPlaneProject(startingConditions.Kinematics.LinearKinematic.Velocity, Gravity.GetSafeNormal());
	const FVector verticalVelocity = startingConditions.Kinematics.LinearKinematic.Velocity.ProjectOnToNormal(Gravity.GetSafeNormal());

	//Gravity acceleration and air time
	processResult.Kinematics.LinearKinematic.Acceleration = AddGravity(processResult.Kinematics.LinearKinematic.Acceleration);
	_airTime += delta;

	//Air control
	processResult.Kinematics.LinearKinematic.Velocity = AirControl(inputAxis, HorizontalVelocity, delta) + verticalVelocity;

	//Rotation
	processResult.Kinematics.AngularKinematic = UStructExtensions::LookAt(startingConditions.Kinematics.AngularKinematic, inputAxis, AirControlRotationSpeed, delta);

	//Save state
	processResult.ControllerStatus.StateModifiers1.X = _airTime;

	return processResult;
	
}





FString UFreeFallState::DebugString()
{
	return Super::DebugString() + " : " + FString::Printf(TEXT("Air Time (%f)"), _airTime);
}

#pragma endregion

