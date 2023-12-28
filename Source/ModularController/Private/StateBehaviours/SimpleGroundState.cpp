// Copyright � 2023 by Tyni Boat. All Rights Reserved.


#include "StateBehaviours/SimpleGroundState.h"
#include <Kismet/KismetMathLibrary.h>


//Check if we are on the ground
#pragma region Check XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

bool USimpleGroundState::CheckSurface(const FKinematicInfos& inDatas, const FInputEntryPool& inputs,
	UModularControllerComponent* controller, const float inDelta)
{
	if (!controller) 
	{
		t_currentSurfaceInfos = FHitResult();
		SurfaceInfos.Reset();
		return false;
	}

	FHitResult surfaceInfos;
	FVector gravityDirection = inDatas.Gravity.GetSafeNormal();
	if (!gravityDirection.Normalize())
		gravityDirection = FVector::DownVector;
	const float checkDistance = FloatingGroundDistance + (GetWasTheLastFrameBehaviour()? MaxCheckDistance : 0);

	const bool haveHit = controller->ComponentTraceCastSingleByInflation(surfaceInfos, inDatas.InitialTransform.GetLocation(), gravityDirection * checkDistance
		, inDatas.InitialTransform.GetRotation(), HullInflation, ChannelGround, true);

	//Debug
	if (inDatas.IsDebugMode)
	{
		UStructExtensions::DrawDebugCircleOnSurface(surfaceInfos, false, 40, FColor::Green, 0, 2, true);
	}

	t_currentSurfaceInfos = surfaceInfos;
	SurfaceInfos.UpdateSurfaceInfos(inDatas.InitialTransform, surfaceInfos, inDelta);
	return haveHit;
}

void USimpleGroundState::OnLanding_Implementation(FSurfaceInfos landingSurface, const FKinematicInfos& inDatas,
	const float delta)
{
}

void USimpleGroundState::OnTakeOff_Implementation(FSurfaceInfos landingSurface, const FKinematicInfos& inDatas)
{
}

#pragma endregion



#pragma region Surface and Snapping XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


FVector USimpleGroundState::ComputeSnappingForce(const FKinematicInfos& inDatas) const
{
	if (!t_currentSurfaceInfos.IsValidBlockingHit())
		return FVector();
	const FVector offsetEndLocation = t_currentSurfaceInfos.Location + (t_currentSurfaceInfos.TraceStart - t_currentSurfaceInfos.TraceEnd).GetSafeNormal() * (FloatingGroundDistance - HullInflation);
	const FVector rawSnapForce = offsetEndLocation - inDatas.InitialTransform.GetLocation();
	FVector snappingForce = rawSnapForce;
	UKismetSystemLibrary::DrawDebugArrow(inDatas.GetActor(), inDatas.InitialTransform.GetLocation(), offsetEndLocation, 50, FColor::Yellow, 0, 3);
	return snappingForce;
}

#pragma endregion


#pragma region Move XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX



FVector USimpleGroundState::MoveOnTheGround(const FKinematicInfos& inDatas, const FInputEntryPool& inputs,
	const float inDelta, FQuat& modRotation)
{
	const FVector horizontalVelocity = FVector::VectorPlaneProject(inDatas.GetInitialMomentum(), inDatas.Gravity.GetSafeNormal());
	FVector inputMove = inputs.ReadInput(MovementInputName).Axis;
	modRotation = inDatas.InitialTransform.GetRotation();
	if (inputMove.SquaredLength() > 0)
	{
		FVector scaledInputs = FMath::Lerp(horizontalVelocity, inputMove * MaxMoveSpeed, inDelta * Acceleration);

		if (inDatas.bUsePhysic && inDatas.FinalSurface.GetHitResult().IsValidBlockingHit() && inDatas.FinalSurface.GetSurfacePrimitive() != nullptr && inDatas.FinalSurface.GetSurfacePrimitive()->IsSimulatingPhysics() && horizontalVelocity.Length() > 0)
		{
			inDatas.FinalSurface.GetSurfacePrimitive()->AddForceAtLocation(FVector::VectorPlaneProject(-horizontalVelocity, inDatas.FinalSurface.GetHitResult().Normal) * inDatas.GetMass(), inDatas.FinalSurface.GetHitResult().ImpactPoint, inDatas.FinalSurface.GetHitResult().BoneName);
		}

		//Rotate
		if (scaledInputs.Length() > 0 && inDatas.Gravity.Length() > 0)
		{
			FVector inputAxis = scaledInputs;

			inputAxis.Normalize();
			FVector fwd = FVector::VectorPlaneProject(inputAxis, inDatas.Gravity.GetSafeNormal());
			fwd.Normalize();
			FQuat fwdRot = UKismetMathLibrary::MakeRotationFromAxes(fwd, FVector::CrossProduct(inDatas.Gravity.GetSafeNormal(), fwd), inDatas.Gravity.GetSafeNormal()).Quaternion();
			FQuat rotation = FQuat::Slerp(inDatas.InitialTransform.GetRotation(), fwdRot, FMath::Clamp(inDelta * TurnSpeed, 0, 1));
			modRotation = rotation;
		}


		return scaledInputs;
	}
	else
	{
		float decc = FMath::Clamp(Deceleration, 1, TNumericLimits<float>().Max());
		FVector scaledInputs = FMath::Lerp(horizontalVelocity, FVector::ZeroVector, inDelta * decc);

		return scaledInputs;
	}
}


#pragma endregion



//Inherited functions
#pragma region Functions XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


int USimpleGroundState::GetPriority_Implementation()
{
	return BehaviourPriority;
}

FName USimpleGroundState::GetDescriptionName_Implementation()
{
	return BehaviourName;
}

void USimpleGroundState::StateIdle_Implementation(UModularControllerComponent* controller, const float inDelta)
{

}

bool USimpleGroundState::CheckState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs,
	UModularControllerComponent* controller, const float inDelta)
{
	return CheckSurface(inDatas, inputs, controller, inDelta);
}

void USimpleGroundState::OnEnterState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs,
	UModularControllerComponent* controller, const float inDelta)
{

}

FVelocity USimpleGroundState::ProcessState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs,
	UModularControllerComponent* controller, const float inDelta)
{
	FVelocity result = FVelocity();
	result.Rotation = inDatas.InitialTransform.GetRotation();

	//Move
	FVector moveVec = MoveOnTheGround(inDatas, inputs, inDelta, result.Rotation);
	result.ConstantLinearVelocity = moveVec;
	result.ConstantLinearVelocity *= result._rooMotionScale;

	//Snapping
	FVector snapForce = ComputeSnappingForce(inDatas);
	result.InstantLinearVelocity = snapForce + SurfaceInfos.GetSurfaceLinearVelocity();

	return result;
}

void USimpleGroundState::OnExitState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs,
	UModularControllerComponent* controller, const float inDelta)
{

}

void USimpleGroundState::OnBehaviourChanged_Implementation(FName newBehaviourDescName, int newPriority,
	UModularControllerComponent* controller)
{

}

FString USimpleGroundState::DebugString()
{
	return Super::DebugString();
}

void USimpleGroundState::ComputeFromFlag_Implementation(int flag)
{
	Super::ComputeFromFlag_Implementation(flag);
}

#pragma endregion
