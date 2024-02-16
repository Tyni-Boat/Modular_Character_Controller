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

bool UFreeFallState::CheckState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UInputEntryPool* inputs, UModularControllerComponent* controller, const float inDelta, int overrideWasLastStateStatus)
{
	return true;
}

void UFreeFallState::OnEnterState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	_gravity = Gravity;
	if (controller)
		controller->SetGravity(_gravity, this);
	_airTime = 0;
}

FVelocity UFreeFallState::ProcessState_Implementation(FStatusParameters& controllerStatus,
	const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller,
	const float inDelta)
{
	auto inputAxis = moveInput;
	if (inputAxis.Normalize())
	{
		const FVector planarInput = FVector::VectorPlaneProject(inputAxis, Gravity.GetSafeNormal());
		const FVector resultingVector = planarInput * AirControlSpeed;
		inputAxis = resultingVector;
	}
	if (controllerStatus.StateModifiers.IsValidIndex(0))
		_airTime = controllerStatus.StateModifiers[0];

	FVector HorizontalVelocity = FVector(0);
	FVector VerticalVelocity = FVector(0);
	FVelocity move = FVelocity();
	move.Rotation = inDatas.InitialTransform.GetRotation();

	if (inDatas.GetInitialMomentum().Length() > 0)
	{
		HorizontalVelocity = FVector::VectorPlaneProject(inDatas.GetInitialMomentum(), inDatas.Gravity.GetSafeNormal());
		VerticalVelocity = inDatas.GetInitialMomentum().ProjectOnToNormal(inDatas.Gravity.GetSafeNormal());
	}

	FVector velocity = FVector(0);
	if (GetWasTheLastFrameControllerState())
	{
		velocity = AirControl(inputAxis, HorizontalVelocity, inDelta);
	}
	if (inputAxis.SquaredLength() > 0)
	{
		const auto lookDir = inputAxis.GetSafeNormal();
		move.Rotation = UStructExtensions::GetProgressiveRotation(inDatas.InitialTransform.GetRotation(), -inDatas.Gravity.GetSafeNormal(), lookDir, AirControlRotationSpeed, inDelta);
	}

	velocity += AddGravity(VerticalVelocity, inDelta);
	_airTime += inDelta;
	move.ConstantLinearVelocity = velocity;

	controllerStatus.StateModifiers = { _airTime };

	return move;
}

void UFreeFallState::OnExitState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	_airTime = 0;
}


void UFreeFallState::OnControllerStateChanged_Implementation(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller)
{
}

void UFreeFallState::SaveStateSnapShot_Internal()
{
	_airTime_saved = _airTime;
	_gravity_saved = _gravity;
}

void UFreeFallState::RestoreStateFromSnapShot_Internal()
{
	_airTime = _airTime_saved;
	_gravity = _gravity_saved;
}


#pragma endregion

