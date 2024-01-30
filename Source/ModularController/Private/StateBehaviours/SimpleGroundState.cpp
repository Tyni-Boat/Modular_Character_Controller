// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "StateBehaviours/SimpleGroundState.h"
#include <Kismet/KismetMathLibrary.h>


//Check if we are on the ground
#pragma region Check XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

bool USimpleGroundState::CheckSurface(const FKinematicInfos& inDatas, UModularControllerComponent* controller, const float inDelta, bool forceMaxDistance)
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
	const float hulloffset = -HullInflation;
	const float checkDistance = FloatingGroundDistance + (forceMaxDistance ? MaxCheckDistance : (GetWasTheLastFrameBehaviour() ? MaxCheckDistance : 0));

	const bool haveHit = controller->ComponentTraceCastSingle(surfaceInfos, inDatas.InitialTransform.GetLocation(), gravityDirection * (checkDistance + hulloffset)
		, inDatas.InitialTransform.GetRotation(), HullInflation, controller->bUseComplexCollision);

	//Debug
	if (inDatas.IsDebugMode)
	{
		UStructExtensions::DrawDebugCircleOnSurface(surfaceInfos, false, 40, FColor::Green, 0, 2, true);
	}

	t_currentSurfaceInfos = surfaceInfos;
	SurfaceInfos.UpdateSurfaceInfos(inDatas.InitialTransform, surfaceInfos, inDelta);
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


FVector USimpleGroundState::ComputeSnappingForce(const FKinematicInfos& inDatas) const
{
	if (!t_currentSurfaceInfos.IsValidBlockingHit())
		return FVector();
	const FVector offsetEndLocation = t_currentSurfaceInfos.Location + (t_currentSurfaceInfos.TraceStart - t_currentSurfaceInfos.TraceEnd).GetSafeNormal() * (FloatingGroundDistance - HullInflation);
	const FVector rawSnapForce = offsetEndLocation - inDatas.InitialTransform.GetLocation();
	FVector snappingForce = rawSnapForce.ProjectOnToNormal(inDatas.Gravity.GetSafeNormal());
	if (inDatas.IsDebugMode)
		UKismetSystemLibrary::DrawDebugArrow(inDatas.GetActor(), inDatas.InitialTransform.GetLocation(), offsetEndLocation, 50, FColor::Yellow, 0, 3);
	return snappingForce;
}

#pragma endregion


#pragma region General Movement XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX



FVector USimpleGroundState::MoveOnTheGround(const FKinematicInfos& inDatas, FVector desiredMovement, const float acceleration, const float decceleration, const float inDelta)
{
	const FVector hVel = FVector::VectorPlaneProject(inDatas.GetInitialMomentum(), inDatas.Gravity.GetSafeNormal());
	const FVector vVel = inDatas.Gravity;
	const float surfaceDrag = t_currentSurfaceInfos.PhysMaterial != nullptr ? t_currentSurfaceInfos.PhysMaterial->Friction : 1;

	//Slope handling
	if (vVel.SquaredLength() > 0.05f && FMath::Abs(FVector::DotProduct(t_currentSurfaceInfos.ImpactNormal, vVel.GetSafeNormal())) < 1)
	{
		FVector downHillDirection = FVector::VectorPlaneProject(t_currentSurfaceInfos.ImpactNormal, vVel.GetSafeNormal()).GetSafeNormal();
		if (bSlopeAffectSpeed && desiredMovement.Length() > 0)
		{
			FVector slopeDesiredMovement = FVector::VectorPlaneProject(desiredMovement, t_currentSurfaceInfos.ImpactNormal);
			float diff = FMath::Abs(desiredMovement.Length() - slopeDesiredMovement.Length());
			float dirScale = FVector::DotProduct(desiredMovement.GetSafeNormal(), downHillDirection);
			desiredMovement += desiredMovement.GetSafeNormal() * diff * dirScale;
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

	if (desiredMovement.Length() > 0.05f)
	{
		const bool isDeccelerating = hVel.SquaredLength() > desiredMovement.SquaredLength();
		const FVector scaledInputs = UStructExtensions::AccelerateTo(hVel, desiredMovement, (isDeccelerating ? decceleration : acceleration) * surfaceDrag, inDelta);

		if (inDatas.bUsePhysic && t_currentSurfaceInfos.IsValidBlockingHit() && t_currentSurfaceInfos.Component.IsValid() && t_currentSurfaceInfos.Component->IsSimulatingPhysics()
			&& scaledInputs.Length() > 0)
		{
			t_currentSurfaceInfos.Component->AddForceAtLocation(FVector::VectorPlaneProject(-scaledInputs, t_currentSurfaceInfos.ImpactNormal) * inDatas.GetMass(), t_currentSurfaceInfos.ImpactPoint, t_currentSurfaceInfos.BoneName);
		}

		return scaledInputs;
	}
	else
	{
		const float decc = FMath::Clamp(decceleration, 0.001f, TNumericLimits<float>().Max());
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
	const float hulloffset = -HullInflation;
	const float relativeCheckDistance = 0;// FMath::Clamp(attemptedMove.Length(), 1, TNumericLimits<float>().Max());
	const float checkDistance = FloatingGroundDistance + MaxCheckDistance;

	bool haveHit = controller->ComponentTraceCastSingle(surfaceInfos, newPos + checkDir * (HullInflation + relativeCheckDistance), gravityDirection * (checkDistance + hulloffset)
		, inDatas.InitialTransform.GetRotation(), HullInflation, controller->bUseComplexCollision);

	if (inDatas.IsDebugMode)
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
			haveHit = controller->ComponentTraceCastSingle(surfaceInfos, newPos + checkDir * (HullInflation + relativeCheckDistance), gravityDirection * (checkDistance + hulloffset)
				, inDatas.InitialTransform.GetRotation(), HullInflation, controller->bUseComplexCollision);

			if (inDatas.IsDebugMode)
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


int USimpleGroundState::GetPriority_Implementation()
{
	return BehaviourPriority;
}

FName USimpleGroundState::GetDescriptionName_Implementation()
{
	return BehaviourName;
}




bool USimpleGroundState::CheckState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs,
	UModularControllerComponent* controller, const float inDelta)
{
	return CheckSurface(inDatas, controller, inDelta);
}

void USimpleGroundState::OnEnterState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs,
	UModularControllerComponent* controller, const float inDelta)
{
	if (inDatas.GetInitialMomentum().Length() > 0)
	{
		FVector vert = inDatas.GetInitialMomentum().ProjectOnToNormal(inDatas.Gravity.GetSafeNormal());
		const float scale = FMath::Clamp(FVector::DotProduct(vert.GetSafeNormal(), inDatas.Gravity.GetSafeNormal()), 0, 1);
		LandingImpactRemainingForce = vert.Length() * scale;
	}
}

FMovePreprocessParams USimpleGroundState::PreProcessState_Implementation(const FKinematicInfos& inDatas,
	const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	FMovePreprocessParams params;

	FVector speedAcc = FVector(MaxJogSpeed, JogAcceleration, JogDeceleration);
	float turnSpd = JoggingTurnSpeed;
	TEnumAsByte<EGroundLocomotionMode> locomotionMode = Jogging;
	const FVector horizontalVelocity = FVector::VectorPlaneProject(inDatas.GetInitialMomentum(), inDatas.Gravity.GetSafeNormal());

	const FVector inputMove = inputs.ReadInput(MovementInputName).Axis;
	FVector lockOnDirection = inputs.ReadInput(LockOnDirection).Axis;

	//crouch
	if (inputs.ReadInput(CrouchInputName).Phase == InputEntryPhase_Held)
	{
		speedAcc = FVector(MaxCrouchSpeed, CrouchAcceleration, CrouchDeceleration);
		if (horizontalVelocity.Length() <= MaxCrouchSpeed)
		{
			turnSpd = CrouchTurnSpeed;
			locomotionMode = Crouching;
		}
	}
	//crawl
	if (inputs.ReadInput(CrawlInputName).Phase == InputEntryPhase_Held)
	{
		speedAcc = FVector(MaxCrawlSpeed, CrawlAcceleration, CrawlDeceleration);
		if (horizontalVelocity.Length() <= MaxCrawlSpeed)
		{
			turnSpd = CrawlTurnSpeed;
			locomotionMode = Crawling;
		}
	}
	//sprint
	if (inputs.ReadInput(SprintInputName).Phase == InputEntryPhase_Held && horizontalVelocity.Length() >= MinSprintSpeed)
	{
		speedAcc = FVector(MaxSprintSpeed, SprintAcceleration, SprintDeceleration);
		turnSpd = SprintingTurnSpeed;
		locomotionMode = Sprinting;
	}

	float moveScale = 1;

	//Rotate
	const FVector lookDir = lockOnDirection.SquaredLength() > 0 ? lockOnDirection : inputMove;
	if (inDatas.Gravity.Length() > 0 && lookDir.Length() > 0 && turnSpd > 0)
	{
		const FQuat rotation = UStructExtensions::GetProgressiveRotation(inDatas.InitialTransform.GetRotation()
			, inDatas.Gravity.GetSafeNormal(), lookDir, turnSpd, inDelta);
		params.SetLookDiff(inDatas.InitialTransform.GetRotation(), rotation);

		//scale the move with direction
		moveScale = FMath::Clamp(FVector::DotProduct(rotation.Vector(), lookDir.GetSafeNormal()), 0.001f, 1);
		moveScale = moveScale * moveScale * moveScale * moveScale;
	}

	const FVector desiredMove = inputMove * speedAcc.X * moveScale;
	params.MoveVector = desiredMove;
	params.StateFlag = static_cast<int>(locomotionMode);
	return params;
}

FVelocity USimpleGroundState::ProcessState_Implementation(const FKinematicInfos& inDatas,
	const FMovePreprocessParams params, UModularControllerComponent* controller, const float inDelta)
{
	FVelocity result = FVelocity();
	CheckSurface(inDatas, controller, inDelta, true);

	float acceleration = 0;
	float decceleration = 0;

	switch (static_cast<EGroundLocomotionMode>(params.StateFlag))
	{
	case Sprinting:
		acceleration = SprintAcceleration;
		decceleration = SprintDeceleration;
		break;
	case Crawling:
		acceleration = CrawlAcceleration;
		decceleration = CrawlDeceleration;
		break;
	case Crouching:
		acceleration = CrouchAcceleration;
		decceleration = CrouchDeceleration;
		break;
	default:
		acceleration = JogAcceleration;
		decceleration = JogDeceleration;
		break;
	}

	FVector moveVec = MoveOnTheGround(inDatas, params.MoveVector, acceleration, decceleration, inDelta);
	//Change locomotion mode
	CurrentLocomotionMode = static_cast<TEnumAsByte<EGroundLocomotionMode>>(params.StateFlag);

	//Fall prevention
	FVector preventionForce = FVector(0);
	if (IsPreventingFalling)
	{
		moveVec = MoveToPreventFalling(controller, inDatas, moveVec, inDelta, preventionForce);
	}

	result.Rotation = inDatas.InitialTransform.GetRotation() * params.RotationDiff;
	result.ConstantLinearVelocity = moveVec;
	result.ConstantLinearVelocity *= result._rooMotionScale;

	//Snapping
	const FVector snapForce = ComputeSnappingForce(inDatas) * 50 * inDelta;
	result.InstantLinearVelocity = snapForce + SurfaceInfos.GetSurfaceLinearVelocity() + preventionForce;

	return result;
}


void USimpleGroundState::OnExitState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs,
	UModularControllerComponent* controller, const float inDelta)
{
	LandingImpactRemainingForce = 0;
	CurrentLocomotionMode = Jogging;
}

void USimpleGroundState::OnControllerStateChanged_Implementation(FName newBehaviourDescName, int newPriority,
	UModularControllerComponent* controller)
{

}

FString USimpleGroundState::DebugString()
{
	return Super::DebugString() + " : " + (LandingImpactRemainingForce > LandingImpactMoveThreshold ? FString::Printf(TEXT("Land (-%d)"), static_cast<int>(LandingImpactRemainingForce - LandingImpactMoveThreshold)) : (UEnum::GetValueAsName<EGroundLocomotionMode>(CurrentLocomotionMode).ToString() + (t_currentSurfaceInfos.PhysMaterial.Get() != nullptr ? FString::Printf(TEXT(" On %s"), *t_currentSurfaceInfos.PhysMaterial.Get()->GetName()) : " On NULL")));
}


#pragma endregion
