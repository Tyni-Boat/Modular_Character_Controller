// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "StateBehaviours/SimpleGroundState.h"
#include <Kismet/KismetMathLibrary.h>


//Check if we are on the ground
#pragma region Check XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

bool USimpleGroundState::CheckSurface(const FTransform spacialInfos, const FVector gravity, UModularControllerComponent* controller, const FVector momentum, const float inDelta, bool useMaxDistance)
{
	if (!controller)
	{
		t_currentSurfaceInfos = FHitResult();
		SurfaceInfos.Reset();
		return false;
	}

	FHitResult surfaceInfos;
	FVector gravityDirection = gravity.GetSafeNormal();
	if (!gravityDirection.Normalize())
		gravityDirection = FVector::DownVector;
	const float hulloffset = -HullInflation;
	const float checkDistance = (FloatingGroundDistance + 1) + (useMaxDistance ? MaxCheckDistance : 0);

	const bool haveHit = controller->ComponentTraceCastSingle(surfaceInfos, spacialInfos.GetLocation(), gravityDirection * (checkDistance + hulloffset)
		, spacialInfos.GetRotation(), HullInflation, controller->bUseComplexCollision);

	//Debug
	if (bDebugState)
	{
		UStructExtensions::DrawDebugCircleOnSurface(surfaceInfos, false, 40, useMaxDistance ? FColor::Green : FColor::Yellow, 0, 2, true);
	}

	t_currentSurfaceInfos = surfaceInfos;
	SurfaceInfos.UpdateSurfaceInfos(spacialInfos, surfaceInfos, inDelta);

	//Check if surface is falling faster tha gravity
	auto surfaceVelocity = SurfaceInfos.GetSurfaceLinearVelocity();
	if (surfaceVelocity.Length() > 0)
	{
		FVector momentumOnGrav = momentum.ProjectOnToNormal(gravityDirection);
		FVector surfaceFallingVel = surfaceVelocity.ProjectOnToNormal(gravityDirection);
		auto velocityOnGravityDir = FVector::DotProduct(surfaceVelocity, gravityDirection);
		auto momentumDir = FVector::DotProduct(momentumOnGrav, gravityDirection);
		if (velocityOnGravityDir > 0)
		{
			if (surfaceFallingVel.Length() > gravity.Length() * inDelta && surfaceInfos.Distance > (FloatingGroundDistance * 2) + 1)
				return false;
		}
	}

	return haveHit && surfaceInfos.Component.IsValid() && surfaceInfos.Component->CanCharacterStepUpOn;
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


FVector USimpleGroundState::ComputeSnappingForce(const FKinematicInfos& inDatas, UObject* debugObject) const
{
	if (!t_currentSurfaceInfos.IsValidBlockingHit())
		return FVector();
	const FVector offsetEndLocation = t_currentSurfaceInfos.Location + (t_currentSurfaceInfos.TraceStart - t_currentSurfaceInfos.TraceEnd).GetSafeNormal()
		* (FloatingGroundDistance - HullInflation);
	const FVector rawSnapForce = offsetEndLocation - inDatas.InitialTransform.GetLocation();
	FVector snappingForce = rawSnapForce.ProjectOnToNormal(inDatas.Gravity.GetSafeNormal());
	if (bDebugState && debugObject)
		UKismetSystemLibrary::DrawDebugArrow(debugObject, inDatas.InitialTransform.GetLocation(), offsetEndLocation, 50, FColor::Yellow, 0, 3);
	return snappingForce;
}

#pragma endregion


#pragma region General Movement XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX



FVector USimpleGroundState::MoveOnTheGround(const FKinematicInfos& inDatas, FVector desiredMovement, const float acceleration, const float deceleration, const float inDelta)
{
	FVector hVel = FVector::VectorPlaneProject(inDatas.GetInitialMomentum(), inDatas.Gravity.GetSafeNormal());
	const FVector vVel = inDatas.Gravity;
	const float surfaceDrag = t_currentSurfaceInfos.PhysMaterial != nullptr ? t_currentSurfaceInfos.PhysMaterial->Friction : 1;
	FVector inputMove = desiredMovement;

	//Slope handling
	if (vVel.SquaredLength() > 0.05f && FMath::Abs(FVector::DotProduct(t_currentSurfaceInfos.ImpactNormal, vVel.GetSafeNormal())) < 1)
	{
		const FVector downHillDirection = FVector::VectorPlaneProject(t_currentSurfaceInfos.ImpactNormal, vVel.GetSafeNormal()).GetSafeNormal();
		if (bSlopeAffectSpeed && desiredMovement.Length() > 0)
		{
			const FVector slopeDesiredMovement = FVector::VectorPlaneProject(desiredMovement, t_currentSurfaceInfos.ImpactNormal);
			const float diff = FMath::Abs(desiredMovement.Length() - slopeDesiredMovement.Length());
			const float dirScale = FVector::DotProduct(desiredMovement.GetSafeNormal(), downHillDirection);
			desiredMovement += desiredMovement.GetSafeNormal() * diff * dirScale;
			inputMove = slopeDesiredMovement.GetSafeNormal() * desiredMovement.Length();
		}

		const float angle = (1 - FVector::DotProduct(-vVel.GetSafeNormal(), t_currentSurfaceInfos.ImpactNormal)) * 90;
		if (angle > MaxSlopeAngle)
		{
			const FVector downHillGravity = FVector::VectorPlaneProject(vVel, t_currentSurfaceInfos.ImpactNormal).GetSafeNormal();
			FVector onHillDesiredMove = FVector::VectorPlaneProject(desiredMovement, downHillGravity.GetSafeNormal());
			onHillDesiredMove = onHillDesiredMove.GetClampedToMaxSize(MaxSlidingSpeed * 0.5);
			const FVector scaledInputs = UStructExtensions::AccelerateTo(hVel, downHillGravity * MaxSlidingSpeed + onHillDesiredMove, SlidingAcceleration * FMath::Clamp(1 - surfaceDrag, 0.01, 1), inDelta);
			return scaledInputs;
		}
	}

	//void any movement if we are absorbing landing impact
	if (LandingImpactRemainingForce > 0)
	{
		LandingImpactRemainingForce -= LandingImpactAbsorbtionSpeed * inDelta;
		if (LandingImpactRemainingForce > LandingImpactMoveThreshold)
		{
			const FVector scaledInputs = UStructExtensions::AccelerateTo(hVel, FVector::ZeroVector, (LandingImpactMoveThreshold / LandingImpactRemainingForce) * 5 * surfaceDrag, inDelta);
			return scaledInputs;
		}
	}

	hVel = FVector::VectorPlaneProject(inDatas.GetInitialMomentum(), t_currentSurfaceInfos.Normal);

	if (inputMove.Length() > 0.05f)
	{
		const bool isDecelerating = hVel.SquaredLength() > inputMove.SquaredLength();
		FVector scaledInputs = UStructExtensions::AccelerateTo(hVel, inputMove, (isDecelerating ? deceleration : acceleration) * surfaceDrag, inDelta);

		if (inDatas.bUsePhysic && t_currentSurfaceInfos.IsValidBlockingHit() && t_currentSurfaceInfos.Component.IsValid() && t_currentSurfaceInfos.Component->IsSimulatingPhysics()
			&& scaledInputs.Length() > 0)
		{
			t_currentSurfaceInfos.Component->AddForceAtLocation(FVector::VectorPlaneProject(-scaledInputs, t_currentSurfaceInfos.Normal) * inDatas.GetMass() * inDelta, t_currentSurfaceInfos.ImpactPoint, t_currentSurfaceInfos.BoneName);
		}

		return scaledInputs;
	}
	else
	{
		const float decc = FMath::Clamp(deceleration, 0.001f, TNumericLimits<float>().Max());
		const FVector scaledInputs = UStructExtensions::AccelerateTo(hVel, FVector::ZeroVector, decc * surfaceDrag, inDelta);

		return scaledInputs;
	}
}

FVector USimpleGroundState::MoveToPreventFalling(UModularControllerComponent* controller, const FKinematicInfos& inDatas, const FVector attemptedMove,
	const float inDelta, FVector& adjusmentMove)
{
	if (!controller)
		return attemptedMove;
	if (!t_currentSurfaceInfos.GetActor())
		return attemptedMove;

	const FVector normalPt = t_currentSurfaceInfos.ImpactPoint + t_currentSurfaceInfos.Normal;
	const FVector imp_normalPt = t_currentSurfaceInfos.ImpactPoint + t_currentSurfaceInfos.ImpactNormal;
	FVector upVector = inDatas.InitialTransform.GetRotation().GetUpVector();
	FVector checkDir = FVector::VectorPlaneProject((normalPt - imp_normalPt), upVector);
	if (!checkDir.Normalize())
		return attemptedMove;

	FVector planedImpactVec = FVector::VectorPlaneProject(t_currentSurfaceInfos.ImpactNormal, upVector);
	if (planedImpactVec.Normalize())
	{
		float bothNormalsLookingSameDir = FVector::DotProduct(planedImpactVec, t_currentSurfaceInfos.Normal);
		if (bothNormalsLookingSameDir > 0)
		{
			checkDir = FVector::VectorPlaneProject(checkDir, planedImpactVec);
			checkDir.Normalize();
		}
	}
	FVector newPos = controller->PointOnShape(checkDir, inDatas.InitialTransform.GetLocation());

	FHitResult surfaceInfos;
	FVector gravityDirection = inDatas.Gravity.GetSafeNormal();
	if (!gravityDirection.Normalize())
		gravityDirection = FVector::DownVector;
	const float hullOffset = -HullInflation;
	const float relativeCheckDistance = 0;// FMath::Clamp(attemptedMove.Length(), 1, TNumericLimits<float>().Max());
	const float checkDistance = FloatingGroundDistance + MaxCheckDistance;

	bool haveHit = controller->ComponentTraceCastSingle(surfaceInfos, newPos + checkDir * (HullInflation + relativeCheckDistance), gravityDirection * (checkDistance + hullOffset)
		, inDatas.InitialTransform.GetRotation(), HullInflation, controller->bUseComplexCollision);

	if (bDebugState)
	{
		if (haveHit)
			UStructExtensions::DrawDebugCircleOnSurface(surfaceInfos, false, 30, FColor::Orange, 0, 1, true);
		else
			UKismetSystemLibrary::DrawDebugBox(controller, newPos, FVector(1, 40, 40), FColor::Orange, checkDir.Rotation(), 0, 1);
	}

	if (haveHit)
	{
		//Check stair cases mode
		FVector impactsLinker = t_currentSurfaceInfos.ImpactPoint - surfaceInfos.ImpactPoint;
		if (surfaceInfos.ImpactNormal == t_currentSurfaceInfos.ImpactNormal && surfaceInfos.ImpactNormal == upVector && impactsLinker.Length() > 0)
		{
			impactsLinker = FVector::VectorPlaneProject(impactsLinker, upVector);
			impactsLinker.Normalize();
			checkDir = FVector::VectorPlaneProject(surfaceInfos.Normal, impactsLinker);
			checkDir = FVector::VectorPlaneProject(checkDir, upVector);
			checkDir.Normalize();

			newPos = controller->PointOnShape(checkDir, surfaceInfos.Location);
			haveHit = controller->ComponentTraceCastSingle(surfaceInfos, newPos + checkDir * (HullInflation + relativeCheckDistance), gravityDirection * (checkDistance + hullOffset)
				, inDatas.InitialTransform.GetRotation(), HullInflation, controller->bUseComplexCollision);

			if (bDebugState)
			{
				if (haveHit)
					UStructExtensions::DrawDebugCircleOnSurface(surfaceInfos, false, 20, FColor::Purple, 0, 1, true);
				else
					UKismetSystemLibrary::DrawDebugBox(controller, newPos, FVector(1, 40, 40), FColor::Purple, checkDir.Rotation(), 0, 1);

				UKismetSystemLibrary::DrawDebugArrow(controller, inDatas.InitialTransform.GetLocation(), inDatas.InitialTransform.GetLocation() + checkDir * 50, 200, FColor::White, 0, 3);
			}

			if (!haveHit)
			{
				FVector correctionVec = (t_currentSurfaceInfos.ImpactPoint - newPos).ProjectOnToNormal(checkDir);
				//adjusmentMove = correctionVec * 0.45f;
				const FVector newMove = FVector::VectorPlaneProject(attemptedMove, checkDir.GetSafeNormal());
				return FVector::DotProduct(attemptedMove, checkDir) >= 0 ? newMove + correctionVec * inDelta * 25 : attemptedMove;
			}
		}
		return attemptedMove;
	}
	else
	{
		FVector correctionVec = (t_currentSurfaceInfos.ImpactPoint - newPos).ProjectOnToNormal(checkDir);
		//adjusmentMove = correctionVec * 0.45f;
		const FVector newMove = FVector::VectorPlaneProject(attemptedMove, checkDir.GetSafeNormal());
		return FVector::DotProduct(attemptedMove, checkDir) >= 0 ? newMove + correctionVec * inDelta * 5 : attemptedMove;
	}
}


#pragma endregion



//Inherited functions
#pragma region Functions XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


bool USimpleGroundState::CheckState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UInputEntryPool* inputs, UModularControllerComponent* controller, FStatusParameters controllerStatusParam, FStatusParameters& currentStatus, const float inDelta, int overrideWasLastStateStatus)
{
	bool willUseMaxDistance = GetWasTheLastFrameControllerState();
	if (overrideWasLastStateStatus >= 0)
	{
		willUseMaxDistance = overrideWasLastStateStatus > 0;
	}

	FVector lockOnDirection = FVector(0);
	auto input = inputs->ReadInput(LockOnDirection, bDebugState, this);
	lockOnDirection = input.Axis;

	controllerStatusParam.StateModifiers2 = lockOnDirection;

	currentStatus = controllerStatusParam;
	const FVector_NetQuantize currentPos = inDatas.InitialTransform.GetLocation();
	if (currentPos == _lastControlledPosition && willUseMaxDistance && t_savePosDelay <= 0)
	{
		return true;
	}

	if (t_savePosDelay > 0)
		t_savePosDelay -= inDelta;
	_lastControlledPosition = currentPos;

	return CheckSurface(inDatas.InitialTransform, inDatas.Gravity, controller, inDatas.GetInitialMomentum(), inDelta, willUseMaxDistance);
}


void USimpleGroundState::OnEnterState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	if (inDatas.bUsePhysic && t_currentSurfaceInfos.IsValidBlockingHit() && t_currentSurfaceInfos.Component.IsValid() && t_currentSurfaceInfos.Component->IsSimulatingPhysics()
		&& inDatas.GetInitialMomentum().Length() > 0)
	{
		t_currentSurfaceInfos.Component->AddForceAtLocation(inDatas.GetInitialMomentum() * inDatas.GetMass(), t_currentSurfaceInfos.ImpactPoint, t_currentSurfaceInfos.BoneName);
	}

	if (inDatas.GetInitialMomentum().Length() > 0)
	{
		FVector vert = inDatas.GetInitialMomentum().ProjectOnToNormal(inDatas.Gravity.GetSafeNormal());
		const float scale = FMath::Clamp(FVector::DotProduct(vert.GetSafeNormal(), inDatas.Gravity.GetSafeNormal()), 0, 1);
		LandingImpactRemainingForce = vert.Length() * scale;
	}

	t_savePosDelay = 1;
}


FVelocity USimpleGroundState::ProcessState_Implementation(FStatusParameters controllerStatusParam, FStatusParameters& controllerStatus,
	const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller,
	const float inDelta)
{
	FVelocity result = FVelocity();
	result.Rotation = inDatas.InitialVelocities.Rotation;

	if (!controller)
	{
		controllerStatus = controllerStatusParam;
		return result;
	}

	if (controllerStatusParam.StateModifiers1.X > 0)
		LandingImpactRemainingForce = controllerStatusParam.StateModifiers1.X;

	const FVector horizontalVelocity = FVector::VectorPlaneProject(inDatas.GetInitialMomentum(), inDatas.Gravity.GetSafeNormal());
	const FVector verticalVelocity = inDatas.GetInitialMomentum().ProjectOnToNormal(inDatas.Gravity.GetSafeNormal());

	//Collect inputs
	const FVector inputMove = moveInput;
	FVector lockOnDirection = FVector(0);
	bool lockedOn = false;

	if (controllerStatusParam.StateModifiers2.SquaredLength() > 0)
		lockOnDirection = controllerStatusParam.StateModifiers2;

	lockedOn = lockOnDirection.SquaredLength() > 0;

	//Parameters from inputs
	FVector speedAcc = FVector(MaxSpeed, Acceleration, Deceleration);
	float turnSpd = TurnSpeed;
	float moveScale = 1;

	//Rotate
	const FVector lookDir = lockedOn ? lockOnDirection : inputMove;
	if (inDatas.Gravity.Length() > 0 && lookDir.Length() > 0 && turnSpd > 0)
	{
		const FQuat rotation = UStructExtensions::GetProgressiveRotation(inDatas.InitialTransform.GetRotation()
			, inDatas.Gravity.GetSafeNormal(), lookDir, turnSpd, inDelta);

		//scale the move with direction
		if (!lockedOn)
		{
			moveScale = FMath::Clamp(FVector::DotProduct(rotation.Vector(), lookDir.GetSafeNormal()), 0.001f, 1);
			moveScale = moveScale * moveScale * moveScale * moveScale;
		}
		result.Rotation = rotation;
	}

	const FVector desiredMove = inputMove * speedAcc.X * moveScale;

	FVector moveVec = MoveOnTheGround(inDatas, desiredMove, speedAcc.Y, speedAcc.Z, inDelta);

	//Fall prevention
	FVector preventionForce = FVector(0);
	if (IsPreventingFalling)
	{
		moveVec = MoveToPreventFalling(controller, inDatas, moveVec, inDelta, preventionForce);
	}

	result.ConstantLinearVelocity = moveVec;
	result.ConstantLinearVelocity *= result._rooMotionScale;

	//Snapping
	FVector snapForce = ComputeSnappingForce(inDatas, controller);// *50 * inDelta;
	if(controller->ActionInstances.IsValidIndex(controllerStatusParam.ActionIndex) 
		&& controller->ActionInstances[controllerStatusParam.ActionIndex]->bShouldControllerStateCheckOverride)
	{
		snapForce = FVector(0);
	}
	result.InstantLinearVelocity = snapForce + preventionForce;

	controllerStatusParam.StateModifiers1.X = LandingImpactRemainingForce;
	if (lockedOn)
		controllerStatusParam.StateModifiers2 = lockOnDirection;

	controllerStatus = controllerStatusParam;
	return result;

}

void USimpleGroundState::OnExitState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	LandingImpactRemainingForce = 0;
	t_savePosDelay = 1;
	_lastControlledPosition = FVector(0);
}


void USimpleGroundState::OnControllerStateChanged_Implementation(FName newBehaviourDescName, int newPriority,
	UModularControllerComponent* controller)
{

}

FString USimpleGroundState::DebugString()
{
	return Super::DebugString() + " : " + (LandingImpactRemainingForce > LandingImpactMoveThreshold ? FString::Printf(TEXT("Land (-%d)"), static_cast<int>(LandingImpactRemainingForce - LandingImpactMoveThreshold)) : (t_currentSurfaceInfos.PhysMaterial.Get() != nullptr ? FString::Printf(TEXT(" On %s"), *t_currentSurfaceInfos.PhysMaterial.Get()->GetName()) : " On NULL"));
}

void USimpleGroundState::SaveStateSnapShot_Internal()
{
	_landingImpactRemainingForce_saved = LandingImpactRemainingForce;
	_lastControlledPosition_saved = _lastControlledPosition;
}

void USimpleGroundState::RestoreStateFromSnapShot_Internal()
{
	LandingImpactRemainingForce = _landingImpactRemainingForce_saved;
	_lastControlledPosition = _lastControlledPosition_saved;
}


#pragma endregion
