// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "StateBehaviours/FreeFallState.h"
#include "Kismet/KismetMathLibrary.h"


#pragma region Air Velocity and Checks


FVector UFreeFallState::AirControl(FVector inputAxis, FVector horizontalVelocity, float delta)
{
	FVector inputMove = inputAxis.GetSafeNormal();
	if (inputMove.Length() > 0)
	{
		FVector planarInput = FVector::VectorPlaneProject(inputMove, horizontalVelocity.GetSafeNormal());
		float dotProduct = FVector::DotProduct(inputMove.GetSafeNormal(), horizontalVelocity.GetSafeNormal());
		if (dotProduct >= 0)
		{
			return horizontalVelocity + planarInput * AirControlSpeed * delta * AirControlAcceleration;
		}
		else
		{
			return FMath::Lerp(horizontalVelocity, inputMove * AirControlSpeed, delta * AirControlAcceleration);
		}
	}
	return horizontalVelocity;
}


FVector UFreeFallState::AddGravity(FVector verticalVelocity, float delta)
{
	FVector gravity = FVector(0);
	gravity = _gravity * delta + verticalVelocity;
	_airTime += delta;
	return gravity;
}


void UFreeFallState::CheckGroundDistance(UModularControllerComponent* controller, const FVector inLocation, const FQuat inQuat)
{
	_groundDistance = MaxGroundDistDetection;

	if (controller == nullptr)
		return;

	FVector direction = controller->GetGravityDirection();
	FVector startPt = inLocation;
	FVector foots = controller->PointOnShape(direction, inLocation);
	float offset = (startPt - foots).Length();
	FHitResult selectedSurface;
	if (controller->ComponentTraceCastSingle(selectedSurface, startPt, direction * MaxGroundDistDetection, inQuat))
	{
		_groundDistance = selectedSurface.Distance - offset;
	}
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


void UFreeFallState::StateIdle_Implementation(UModularControllerComponent* controller, const float inDelta)
{
	if (controller == nullptr)
		return;
	_gravity = Gravity;
	controller->SetGravity(_gravity);
	CheckGroundDistance(controller, controller->GetLocation(), controller->GetRotation());
}


bool UFreeFallState::CheckState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	return true;
}


void UFreeFallState::OnEnterState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	_airTime = 0;
}


FVelocity UFreeFallState::ProcessState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	FVector hori = FVector(0);
	FVector vert = FVector(0);
	FVector inputAxis = inputs.ReadInput(MovementInputName).Axis.GetSafeNormal();
	FVelocity move = FVelocity();
	move.Rotation = inDatas.InitialTransform.GetRotation();

	if (inDatas.GetInitialMomentum().Length() > 0)
	{
		hori = FVector::VectorPlaneProject(inDatas.GetInitialMomentum(), inDatas.Gravity.GetSafeNormal());
		vert = inDatas.GetInitialMomentum().ProjectOnToNormal(inDatas.Gravity.GetSafeNormal());
	}
	FVector vector = FVector(0);
	vector = AirControl(inputAxis, hori, inDelta);
	if (inputAxis.SquaredLength() > 0)
	{
		inputAxis.Normalize();
		FVector up = -inDatas.Gravity.GetSafeNormal();
		FVector fwd = FVector::VectorPlaneProject(inputAxis, up);
		fwd.Normalize();
		FQuat fwdRot = UKismetMathLibrary::MakeRotationFromAxes(fwd, FVector::CrossProduct(up, fwd), up).Quaternion();
		FQuat rotation = FQuat::Slerp(inDatas.InitialTransform.GetRotation(), fwdRot, FMath::Clamp(inDelta * AirControlRotationSpeed, 0, 1));
		move.Rotation = rotation;
	}
	vector += AddGravity(vert, inDelta);
	move.ConstantLinearVelocity = vector;

	return move;
}


void UFreeFallState::OnExitState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	_airTime = 0;
}


void UFreeFallState::OnBehaviourChanged_Implementation(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller)
{
}


#pragma endregion

