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
			return resultingVector;
		}
		return resultingVector;
	}
	return horizontalVelocity;
}


FVector UFreeFallState::AddGravity(FVector verticalVelocity, float delta)
{
	FVector gravity = FVector(0);
	if (const bool isCurrentlyFalling = FVector::DotProduct(verticalVelocity, _gravity) >= 0)
	{
		const float terminalDiff = TerminalVelocity - verticalVelocity.Length();
		gravity = verticalVelocity + _gravity * delta * FMath::Sign(terminalDiff);
	}
	else
	{
		gravity = _gravity * delta + verticalVelocity;
	}
	_airTime += delta;
	return gravity;
}


#pragma endregion


#pragma region Functions


int UFreeFallState::GetPriority_Implementation()
{
	return BehaviourPriority;
}


FName UFreeFallState::GetDescriptionName_Implementation()
{
	return BehaviourName;
}




bool UFreeFallState::CheckState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	return true;
}


void UFreeFallState::OnEnterState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	_gravity = Gravity;
	if (controller)
		controller->SetGravity(_gravity, this);
	_airTime = 0;
}

FMovePreprocessParams UFreeFallState::PreProcessState_Implementation(const FKinematicInfos& inDatas,
	const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	FMovePreprocessParams params;
	FVector inputAxis = inputs.ReadInput(MovementInputName).Axis.GetSafeNormal();
	if (inputAxis.Normalize())
	{
		const FVector planarInput = FVector::VectorPlaneProject(inputAxis, Gravity.GetSafeNormal());
		const FVector resultingVector = planarInput * AirControlSpeed;
		params.MoveVector = resultingVector;
	}
	params.StateFlag = 0;
	return params;
}

FVelocity UFreeFallState::ProcessState_Implementation(const FKinematicInfos& inDatas,
	const FMovePreprocessParams params, UModularControllerComponent* controller, const float inDelta)
{
	FVector hori = FVector(0);
	FVector vert = FVector(0);
	FVelocity move = FVelocity();
	move.Rotation = inDatas.InitialTransform.GetRotation();

	if (inDatas.GetInitialMomentum().Length() > 0)
	{
		hori = FVector::VectorPlaneProject(inDatas.GetInitialMomentum(), inDatas.Gravity.GetSafeNormal());
		vert = inDatas.GetInitialMomentum().ProjectOnToNormal(inDatas.Gravity.GetSafeNormal());
	}
	FVector velocity = FVector(0);
	if (GetWasTheLastFrameBehaviour()) 
	{
		velocity = AirControl(params.MoveVector, hori, inDelta);
	}
	if (params.MoveVector.SquaredLength() > 0)
	{
		auto lookDir = params.MoveVector.GetSafeNormal();
		move.Rotation = UStructExtensions::GetProgressiveRotation(inDatas.InitialTransform.GetRotation(), -inDatas.Gravity.GetSafeNormal(), lookDir, AirControlRotationSpeed, inDelta);
	}
	velocity += AddGravity(vert, inDelta);
	move.ConstantLinearVelocity = velocity;

	return move;
}


void UFreeFallState::OnExitState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	_airTime = 0;
}


void UFreeFallState::OnControllerStateChanged_Implementation(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller)
{
}


#pragma endregion

