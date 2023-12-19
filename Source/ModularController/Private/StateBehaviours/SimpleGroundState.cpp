// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "StateBehaviours/SimpleGroundState.h"


//Check if we are on the ground
#pragma region Check XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

bool USimpleGroundState::CheckSurface(const FKinematicInfos& inDatas, const FInputEntryPool& inputs,
	UModularControllerComponent* controller, const float inDelta)
{
	if (!controller)
		return false;

	FHitResult surfaceInfos;
	FVector gravityDirection = inDatas.Gravity.GetSafeNormal();
	if (!gravityDirection.Normalize())
		gravityDirection = FVector::DownVector;
	const float checkDistance = FloatingGroundDistance;

	const bool haveHit = controller->ComponentTraceCastSingle(surfaceInfos, inDatas.InitialTransform.GetLocation(), gravityDirection * checkDistance
		, inDatas.InitialTransform.GetRotation(), ChannelGround, CanTraceComplex);

	//Debug
	//if (inDatas.IsDebugMode)
	{
		UStructExtensions::DrawDebugCircleOnSurface(surfaceInfos, false, 40, FColor::Green, 0, 2, true);
	}
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


#pragma region Move XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX



FVector USimpleGroundState::MoveOnTheGround(const FKinematicInfos& inDatas, const FInputEntryPool& inputs,
	const float inDelta)
{
	const FVector horizontalVelocity = FVector::VectorPlaneProject(inDatas.GetInitialMomentum(), inDatas.Gravity.GetSafeNormal());
	FVector inputMove = inputs.ReadInput(MovementInputName).Axis;
	if (inputMove.SquaredLength() > 0)
	{
		FVector scaledInputs = FMath::Lerp(horizontalVelocity, inputMove * MaxMoveSpeed, inDelta * Acceleration);

		if (inDatas.bUsePhysic && inDatas.FinalSurface.GetHitResult().IsValidBlockingHit() && inDatas.FinalSurface.GetSurfacePrimitive() != nullptr && inDatas.FinalSurface.GetSurfacePrimitive()->IsSimulatingPhysics() && horizontalVelocity.Length() > 0)
		{
			inDatas.FinalSurface.GetSurfacePrimitive()->AddForceAtLocation(FVector::VectorPlaneProject(-horizontalVelocity, inDatas.FinalSurface.GetHitResult().Normal) * inDatas.GetMass(), inDatas.FinalSurface.GetHitResult().ImpactPoint, inDatas.FinalSurface.GetHitResult().BoneName);
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
	FVector moveVec = MoveOnTheGround(inDatas, inputs, inDelta);
	result.ConstantLinearVelocity = moveVec;
	result.ConstantLinearVelocity *= result._rooMotionScale;

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
