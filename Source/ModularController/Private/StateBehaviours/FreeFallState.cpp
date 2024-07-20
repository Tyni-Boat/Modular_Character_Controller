// Copyright � 2023 by Tyni Boat. All Rights Reserved.


#include "StateBehaviours/FreeFallState.h"

#include "FunctionLibrary.h"
#include "Kismet/KismetMathLibrary.h"


#pragma region Air Velocity and Checks


FVector UFreeFallState::AirControl(FVector desiredMove, FVector horizontalVelocity, float delta) const
{
	if (desiredMove.Length() > 0)
	{
		const FVector resultingVector = horizontalVelocity + desiredMove * delta;
		const FVector projection = resultingVector.ProjectOnToNormal(horizontalVelocity.GetSafeNormal()).GetClampedToMaxSize(AirControlSpeed);
		const FVector planed = FVector::VectorPlaneProject(resultingVector, horizontalVelocity.GetSafeNormal());
		return projection + planed;
	}
	return horizontalVelocity;
}


FVector UFreeFallState::AddGravity(FVector currentAcceleration) const
{
	return currentAcceleration + Gravity;
}


#pragma endregion


#pragma region Functions


FControllerCheckResult UFreeFallState::CheckState_Implementation(UModularControllerComponent* controller,
                                                                 const FControllerStatus startingConditions, const float inDelta, bool asLastActiveState) const
{
	auto result = startingConditions;
	UFunctionLibrary::AddOrReplaceCosmeticVariable(result.StatusParams, AirTimeVarName, result.StatusParams.StateModifiers.X);
	return FControllerCheckResult(true, result);
}

void UFreeFallState::OnEnterState_Implementation(UModularControllerComponent* controller,
                                                 const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
	if (controller)
		controller->SetGravity(Gravity);
}


FControllerStatus UFreeFallState::ProcessState_Implementation(UModularControllerComponent* controller,
                                                              const FControllerStatus startingConditions, const float delta) const
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

	//Components separation
	const FVector HorizontalVelocity = FVector::VectorPlaneProject(startingConditions.Kinematics.LinearKinematic.Velocity, Gravity.GetSafeNormal());
	const FVector verticalVelocity = startingConditions.Kinematics.LinearKinematic.Velocity.ProjectOnToNormal(Gravity.GetSafeNormal());

	//Gravity acceleration and air time
	processResult.Kinematics.LinearKinematic.Acceleration = AddGravity(processResult.Kinematics.LinearKinematic.Acceleration);

	//Air control
	processResult.Kinematics.LinearKinematic.Velocity = AirControl(inputAxis, HorizontalVelocity, delta) + verticalVelocity;

	//Rotation
	processResult.Kinematics.AngularKinematic = UFunctionLibrary::LookAt(startingConditions.Kinematics.AngularKinematic, inputAxis, AirControlRotationSpeed, delta);

	processResult.StatusParams.StateModifiers.X += delta;
	processResult.CustomSolverCheckParameters = Gravity.GetSafeNormal() * MaxCheckSurfaceDistance;
	processResult.Kinematics.SurfaceBinaryFlag = 0;
	return processResult;
}


FString UFreeFallState::DebugString() const
{
	return Super::DebugString() + " : " + FString::Printf(TEXT("Gravity Acceleration (%s)"), *Gravity.ToCompactString());
}

#pragma endregion
