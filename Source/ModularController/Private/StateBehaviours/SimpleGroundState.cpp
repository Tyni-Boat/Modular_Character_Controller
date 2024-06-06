// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "StateBehaviours/SimpleGroundState.h"
#include <Kismet/KismetMathLibrary.h>


//Check if we are on the ground
#pragma region Check XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

bool USimpleGroundState::CheckSurface(const FTransform spacialInfos, const FVector gravity, UModularControllerComponent* controller, const float inDelta, bool useMaxDistance)
{
	if (!controller)
	{
		SurfaceInfos.Reset();
		return false;
	}

	FVector gravityDirection = gravity.GetSafeNormal();
	if (!gravityDirection.Normalize())
		gravityDirection = FVector::DownVector;
	const FVector lowestPt = controller->PointOnShape(gravityDirection, spacialInfos.GetLocation());
	const float checkDistance = (useMaxDistance ? MaxStepHeight : 5) + MaxStepHeight;
	const float inflation = useMaxDistance ? 3 : -0.125;
	const bool debugInner = bDebugState;
	const float innerStepHeight = MaxStepHeight;
	FHitResult surfaceInfos;

	const bool haveHit = controller->ComponentTraceCastSingleUntil(surfaceInfos, gravityDirection * checkDistance, spacialInfos.GetLocation() - gravityDirection * MaxStepHeight
		, spacialInfos.GetRotation(), [spacialInfos, gravityDirection, lowestPt, innerStepHeight, debugInner](FHitResult hit)->bool
		{
			if (debugInner)
			{
				UStructExtensions::DrawDebugCircleOnSurface(hit, true, 45, FColor::Silver, 0.01, 1, false);
			}

			//Above surface verification
			const FVector fromCenter = (hit.ImpactPoint - spacialInfos.GetLocation()).GetSafeNormal();
			if ((fromCenter | gravityDirection) <= 0)
				return false;

			//Angle verification
			const float angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(hit.ImpactNormal, -gravityDirection)));
			if (angle > 60)
				return false;

			//Step height verification
			const FVector heightVector = (hit.ImpactPoint - lowestPt).ProjectOnToNormal(-gravityDirection);
			if (heightVector.Length() > innerStepHeight)
				return false;

			return true;
		}, 5, inflation, controller->bUseComplexCollision);

	if (!haveHit) 
	{
		SurfaceInfos.Reset();
		return false;
	}

	SurfaceInfos.UpdateSurfaceInfos(spacialInfos, surfaceInfos, inDelta);

	//Debug
	if (bDebugState)
	{
		if (!haveHit)
		{
			surfaceInfos.ImpactPoint = lowestPt + gravityDirection * checkDistance;
			surfaceInfos.ImpactNormal = -gravityDirection;
			surfaceInfos.Normal = -gravityDirection;
			surfaceInfos.HitObjectHandle = controller->GetOwner();
		}
		UStructExtensions::DrawDebugCircleOnSurface(surfaceInfos, true, 45, useMaxDistance ? FColor::Blue : FColor::Yellow, inDelta * 1.7, 2, true);
	}

	return haveHit && surfaceInfos.Component.IsValid() && surfaceInfos.Component->CanCharacterStepUpOn;
}


#pragma endregion



#pragma region Surface and Snapping XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


FVector USimpleGroundState::GetSnappedVector(const FVector onShapeLowestPoint) const
{
	auto t_currentSurfaceInfos = SurfaceInfos.GetHitResult();
	if (!t_currentSurfaceInfos.GetComponent())
		return FVector(0);

	const FVector snapDirection = (t_currentSurfaceInfos.TraceStart - t_currentSurfaceInfos.TraceEnd).GetSafeNormal();
	const FVector hitPoint = t_currentSurfaceInfos.ImpactPoint;
	const FVector farAwayVector = FVector::VectorPlaneProject(hitPoint - onShapeLowestPoint, snapDirection);
	const FVector elevationDiff = (hitPoint - onShapeLowestPoint).ProjectOnToNormal(snapDirection);
	FVector snapVector = elevationDiff * (snapDirection | t_currentSurfaceInfos.ImpactNormal);
	FHitResult middleHit;
	bool hitDown = t_currentSurfaceInfos.GetComponent()->GetWorld()->LineTraceSingleByChannel(middleHit, onShapeLowestPoint - snapDirection * MaxStepHeight, onShapeLowestPoint + snapDirection * 2 * MaxStepHeight, ChannelGround);
	if (bDebugState)
	{
		UKismetSystemLibrary::DrawDebugArrow(t_currentSurfaceInfos.GetComponent(), middleHit.TraceStart, hitDown ? middleHit.Location : middleHit.TraceEnd, 15, hitDown ? FColor::White : FColor::Silver, 0.03, 1);
		UKismetSystemLibrary::DrawDebugArrow(t_currentSurfaceInfos.GetComponent(), hitPoint, hitPoint + snapVector, 15, hitDown ? FColor::Yellow : FColor::Black, 1, 1);
	}
	//return hitDown ? snapVector : FVector(0);
	return snapVector;
}


FVector USimpleGroundState::GetSlidingVector() const
{
	auto t_currentSurfaceInfos = SurfaceInfos.GetHitResult();
	if (!t_currentSurfaceInfos.GetComponent())
		return FVector(0);

	const float surfaceFriction = t_currentSurfaceInfos.PhysMaterial != nullptr ? t_currentSurfaceInfos.PhysMaterial->Friction : 1;
	FVector normal = (t_currentSurfaceInfos.TraceStart - t_currentSurfaceInfos.TraceEnd).GetSafeNormal();
	const float angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(t_currentSurfaceInfos.ImpactNormal, normal)));
	if (angle > MaxSlopeAngle)
	{
		const FVector slopeDirection = FVector::VectorPlaneProject(t_currentSurfaceInfos.ImpactNormal, normal).GetSafeNormal();
		const double alpha = (angle - MaxSlopeAngle) / 5;
		const FVector slidingVelocity = slopeDirection * FMath::Lerp(0, 1, alpha) * (1 - FMath::Clamp(surfaceFriction, 0, 1));
		return slidingVelocity;
	}

	return FVector(0);
}


#pragma endregion


#pragma region General Movement XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX



FVector USimpleGroundState::GetMoveVector(UModularControllerComponent* controller, const FVector inputVector, const float moveScale, const double deltaTime)
{
	auto t_currentSurfaceInfos = SurfaceInfos.GetHitResult();
	if (!t_currentSurfaceInfos.GetComponent() || !controller)
		return inputVector;

	FVector desiredMove = inputVector * MaxSpeed * moveScale;
	desiredMove = controller->GetRootMotionTranslation(RootMotionMode, desiredMove, deltaTime);
	const FVector normal = (t_currentSurfaceInfos.TraceStart - t_currentSurfaceInfos.TraceEnd).GetSafeNormal();

	//Slope handling
	{
		if (bSlopeAffectSpeed && desiredMove.Length() > 0 && FMath::Abs(t_currentSurfaceInfos.Normal | normal) < 1)
		{
			const FVector slopeDirection = FVector::VectorPlaneProject(t_currentSurfaceInfos.ImpactNormal, normal);
			double slopeScale = slopeDirection | desiredMove.GetSafeNormal();
			desiredMove *= FMath::GetMappedRangeValueClamped(TRange<double>(-1, 1), TRange<double>(0.5, 1.25), slopeScale);
		}
	}

	return desiredMove;
}



#pragma endregion



//Inherited functions
#pragma region Functions XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


FControllerCheckResult USimpleGroundState::CheckState_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, const float inDelta, bool asLastActiveState)
{
	FControllerCheckResult result = FControllerCheckResult(false, startingConditions);
	if (!controller)
	{
		return result;
	}

	bool willUseMaxDistance = asLastActiveState;

	const FVector_NetQuantize currentPos = startingConditions.Kinematics.LinearKinematic.Position;
	if (currentPos.Equals(_lastControlledPosition, 1) && willUseMaxDistance && t_savePosDelay <= 0)
	{
		//result.CheckedCondition = true;
		//return result;
	}

	if (t_savePosDelay > 0)
		t_savePosDelay -= inDelta;
	_lastControlledPosition = currentPos;

	result.CheckedCondition = CheckSurface(FTransform(startingConditions.Kinematics.AngularKinematic.Orientation, startingConditions.Kinematics.LinearKinematic.Position), controller->GetGravity(), controller, inDelta, willUseMaxDistance);

	return  result;
}


FKinematicComponents USimpleGroundState::OnEnterState_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta)
{
	auto result = startingConditions;
	t_savePosDelay = 1;

	return result;
}


FControllerStatus USimpleGroundState::ProcessState_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, const float delta)
{
	FControllerStatus result = startingConditions;

	if (!controller)
	{
		return result;
	}
	const auto t_currentSurfaceInfos = SurfaceInfos.GetHitResult();

	//Collect inputs
	const FVector inputMove = result.MoveInput;
	const FVector lockOnDirection = controller->ReadAxisInput(LockOnDirection);
	const bool lockedOn = lockOnDirection.SquaredLength() > 0;

	//Parameters from inputs
	const float turnSpd = TurnSpeed;
	float moveScale = 1;

	//Rotate
	FVector lookDir = lockedOn ? lockOnDirection : inputMove;
	result.Kinematics.AngularKinematic = UStructExtensions::LookAt(result.Kinematics.AngularKinematic, lookDir, turnSpd, delta);

	//scale the move with direction
	if (!lockedOn)
	{
		moveScale = FMath::Clamp(FVector::DotProduct(result.Kinematics.AngularKinematic.Orientation.Vector(), lookDir.GetSafeNormal()), 0.001f, 1);
		moveScale = moveScale * moveScale * moveScale * moveScale;
	}
	
	FVector moveVec = GetMoveVector(controller, inputMove, moveScale, delta);
	moveVec = FVector::VectorPlaneProject(moveVec, t_currentSurfaceInfos.ImpactNormal).GetSafeNormal() * moveVec.Length();

	//Snapping
	const FVector snapVector = GetSnappedVector(controller->PointOnShape(controller->GetGravityDirection(), result.Kinematics.LinearKinematic.Position, 2));
	result.Kinematics.LinearKinematic.SnapDisplacement = snapVector * SnapSpeed;

	//Write values
	const FVector surfaceValues = UStructExtensions::GetSurfacePhysicProperties(t_currentSurfaceInfos);
	result.Kinematics.LinearKinematic.AddCompositeMovement(moveVec, Acceleration * surfaceValues.X, 0);
	result.CustomPhysicProperties = FVector(-1, 0, 0);

	return result;
}


FKinematicComponents USimpleGroundState::OnExitState_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta)
{
	t_savePosDelay = 1;
	_lastControlledPosition = FVector(0);
	return Super::OnExitState_Implementation(controller, startingConditions, moveInput, delta);
}



FString USimpleGroundState::DebugString()
{
	return Super::DebugString() + " : " + (SurfaceInfos.GetHitResult().PhysMaterial.Get() != nullptr ? FString::Printf(TEXT(" On %s"), *SurfaceInfos.GetHitResult().PhysMaterial.Get()->GetName()) : " On NULL");
}

void USimpleGroundState::SaveStateSnapShot_Internal()
{
	_lastControlledPosition_saved = _lastControlledPosition;
}

void USimpleGroundState::RestoreStateFromSnapShot_Internal()
{
	_lastControlledPosition = _lastControlledPosition_saved;
}


#pragma endregion
