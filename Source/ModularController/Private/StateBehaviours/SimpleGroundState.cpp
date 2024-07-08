// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "StateBehaviours/SimpleGroundState.h"
#include <Kismet/KismetMathLibrary.h>

#include "FunctionLibrary.h"
#include "ToolsLibrary.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"


//Check if we are on the ground
#pragma region Check XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


int USimpleGroundState::CheckSurfaceIndex(UModularControllerComponent* controller, const FControllerStatus status, FStatusParameters& statusParams, const float inDelta, bool asActive) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CheckSurfaceIndex");
	if (!controller)
		return -1;

	FVector gravityDirection = controller->GetGravityDirection();
	FVector location = status.Kinematics.LinearKinematic.Position;
	if (!gravityDirection.Normalize())
		gravityDirection = FVector::DownVector;
	const FVector lowestPt = controller->GetWorldSpaceCardinalPoint(gravityDirection);
	const FVector velocity = status.Kinematics.LinearKinematic.Velocity;

	//Find the best surface
	int surfaceIndex = -1;
	float closestSurface = TNumericLimits<float>::Max();
	float testingClosestSurface = TNumericLimits<float>::Max();

	// Default on bad angles
	int badAngleIndex = -1;
	float closestBadAngle = TNumericLimits<float>::Max();

	for (int i = 0; i < status.Kinematics.SurfacesInContact.Num(); i++)
	{
		const auto surface = status.Kinematics.SurfacesInContact[i];
		//Valid surface verification
		if (!surface.TrackedComponent.IsValid())
			continue;

		if (surface.TrackedComponent->GetCollisionResponseToChannel(ChannelGround) != ECR_Block)
			continue;

		//Only surfaces we can step on
		if (surface.SurfacePhysicProperties.W <= 0)
			continue;

		//Above surface verification
		const FVector fromCenter = (surface.SurfacePoint - location).GetSafeNormal();
		if ((fromCenter | gravityDirection) <= 0)
			continue;


		//Avoid Surfaces too far away when inactive
		const FVector farAwayVector = FVector::VectorPlaneProject(surface.SurfacePoint - location, gravityDirection);
		const FVector shapePtInDir = controller->GetWorldSpaceCardinalPoint(farAwayVector);
		const FVector inShapeDir = shapePtInDir - location;
		if (!asActive && inShapeDir.SquaredLength() > 0 && inShapeDir.SquaredLength() * 0.5 < farAwayVector.SquaredLength())
		{
			continue;
		}

		const FVector heightVector = (surface.SurfacePoint - lowestPt).ProjectOnToNormal(-gravityDirection);
		if (heightVector.SquaredLength() < testingClosestSurface)
		{
			UFunctionLibrary::AddOrReplaceCosmeticVariable(statusParams, GroundDistanceVarName, heightVector.Length());
			testingClosestSurface = heightVector.SquaredLength();
		}

		//Avoid surface we are moving sharply away
		// if (!asActive && (surface.SurfaceImpactNormal | velocity.GetSafeNormal()) > 0.5 && velocity.ProjectOnToNormal(gravityDirection).Length() > controller->GetGravityScale() * inDelta)
		// 	continue;

		//Step height verification
		if (heightVector.Length() > (MaxStepHeight + ((heightVector | gravityDirection) > 0 ? 10 : 0)))
			continue;

		// Avoid too far down surfaces on first detection
		if (!asActive && heightVector.Length() > 5 && (heightVector | gravityDirection) > 0)
			continue;

		//Angle verification
		const float angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(surface.SurfaceImpactNormal, -gravityDirection)));
		if (angle > MaxSurfaceAngle)
		{
			const float badDistance = heightVector.Length() * ((surface.SurfacePoint - lowestPt).GetSafeNormal() | gravityDirection);
			if (badDistance >= closestBadAngle)
			{
				continue;
			}

			badAngleIndex = i;
			closestBadAngle = badDistance;
			continue;
		}

		const float distance = heightVector.Length() * ((surface.SurfacePoint - lowestPt).GetSafeNormal() | gravityDirection);
		if (distance >= closestSurface)
		{
			if (bDebugState)
				UFunctionLibrary::DrawDebugCircleOnSurface(surface, 25, FColor::Silver, inDelta * 1.5, 1, false, true);
			continue;
		}

		closestSurface = distance;
		surfaceIndex = i;
	}

	//If no surface found, default on best bad angle one
	if (surfaceIndex < 0 && badAngleIndex >= 0)
		surfaceIndex = badAngleIndex;

	//Debug
	if (bDebugState && status.Kinematics.SurfacesInContact.IsValidIndex(surfaceIndex))
	{
		UFunctionLibrary::DrawDebugCircleOnSurface(status.Kinematics.SurfacesInContact[surfaceIndex], 25, asActive ? FColor::Blue : FColor::Yellow
		                                           , inDelta * 1.5, 2, true, true);
	}

	return surfaceIndex;
}


#pragma endregion


#pragma region General Movement XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


FVector USimpleGroundState::GetMoveVector(const FVector inputVector, const float moveScale, const FSurface Surface, const UModularControllerComponent* controller) const
{
	FVector desiredMove = inputVector * MaxSpeed * moveScale;
	if (controller)
		desiredMove = controller->GetRootMotionTranslation(RootMotionMode, desiredMove);
	const FVector normal = controller ? -controller->GetGravityDirection() : FVector::UpVector;

	//Slope handling
	{
		if (bSlopeAffectSpeed && desiredMove.Length() > 0 && FMath::Abs(Surface.SurfaceNormal | normal) < 1)
		{
			const FVector slopeDirection = FVector::VectorPlaneProject(Surface.SurfaceImpactNormal, normal);
			const double slopeScale = slopeDirection | desiredMove.GetSafeNormal();
			desiredMove *= FMath::GetMappedRangeValueClamped(TRange<double>(-1, 1), TRange<double>(0.25, 1.25), slopeScale);
		}
	}

	return desiredMove;
}


#pragma endregion


//Inherited functions
#pragma region Functions XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


FControllerCheckResult USimpleGroundState::CheckState_Implementation(UModularControllerComponent* controller,
                                                                     const FControllerStatus startingConditions, const float inDelta, bool asLastActiveState) const
{
	FControllerCheckResult result = FControllerCheckResult(false, startingConditions);
	UFunctionLibrary::AddOrReplaceCosmeticVariable(result.ProcessResult.StatusParams, GroundDistanceVarName, TNumericLimits<float>::Max());
	if (!controller)
	{
		return result;
	}

	//Check
	const int surfaceIndex = CheckSurfaceIndex(controller, startingConditions, result.ProcessResult.StatusParams, inDelta, asLastActiveState);
	result.CheckedCondition = surfaceIndex >= 0;
	if (result.CheckedCondition)
	{
		result.ProcessResult.Kinematics.SurfaceBinaryFlag = UToolsLibrary::IndexToFlag(surfaceIndex);
	}
	else
	{
		const FVector relativeVel = FVector::VectorPlaneProject(result.ProcessResult.Kinematics.LinearKinematic.Velocity - result.ProcessResult.Kinematics.LinearKinematic.refVelocity,
		                                                        controller->GetGravityDirection());
		UFunctionLibrary::AddOrReplaceCosmeticVector(result.ProcessResult.StatusParams, GroundMoveVarName, relativeVel);
	}

	return result;
}


void USimpleGroundState::OnEnterState_Implementation(UModularControllerComponent* controller,
                                                     const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
	auto result = startingConditions;
	UFunctionLibrary::ApplyForceOnSurfaces(result, result.LinearKinematic.Position,
	                                       UFunctionLibrary::GetKineticEnergy(result.LinearKinematic.Velocity, controller->GetMass(), (result.LinearKinematic.Velocity * delta).Length()),
	                                       true, ECR_Block);
}


FControllerStatus USimpleGroundState::ProcessState_Implementation(UModularControllerComponent* controller,
                                                                  const FControllerStatus startingConditions, const float delta) const
{
	FControllerStatus result = startingConditions;
	if (!controller)
		return result;
	const TArray<int> indexes = UToolsLibrary::BoolToIndexesArray(UToolsLibrary::FlagToBoolArray(result.Kinematics.SurfaceBinaryFlag));
	const int surfaceIndex = indexes.Num() > 0 ? indexes[0] : -1;
	if (!result.Kinematics.SurfacesInContact.IsValidIndex(surfaceIndex))
		return result;
	const auto surface = result.Kinematics.SurfacesInContact[surfaceIndex];
	const FVector gravityDir = controller->GetGravityDirection();
	const float angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(surface.SurfaceImpactNormal, -gravityDir)));

	//Collect inputs
	const FVector inputMove = result.MoveInput;
	const FVector lockOnDirection = controller->ReadAxisInput(LockOnDirection);
	const bool lockedOn = lockOnDirection.SquaredLength() > 0;

	//Parameters from inputs
	const float turnSpd = TurnSpeed;
	float moveScale = 1;

	//Rotate
	FVector lookDir = lockedOn ? lockOnDirection : inputMove;
	result.Kinematics.AngularKinematic = UFunctionLibrary::LookAt(result.Kinematics.AngularKinematic, lookDir, turnSpd, delta);

	//Snapping
	const FVector snapVector = UFunctionLibrary::GetSnapOnSurfaceVector(controller->GetWorldSpaceCardinalPoint(gravityDir) + gravityDir * 5, surface, gravityDir);
	result.Kinematics.LinearKinematic.SnapDisplacement = snapVector * SnapSpeed;

	//Lerp velocity
	FVector lastMoveVec = result.StatusParams.StateModifiers;
	FSurface cloneSurface = surface;
	if (result.Kinematics.LastMoveHit.HitResult.ImpactNormal.IsNormalized())
	{
		cloneSurface.SurfaceImpactNormal = result.Kinematics.LastMoveHit.HitResult.ImpactNormal;
		cloneSurface.SurfaceNormal = result.Kinematics.LastMoveHit.HitResult.Normal;
	}
	const FVector downSnap = snapVector.ProjectOnToNormal(gravityDir);
	const FVector targetMoveVec = GetMoveVector(inputMove, moveScale, cloneSurface, controller) * FMath::Clamp(1 - (downSnap.Length() / (MaxStepHeight * 0.5)), 0, 1);
	FVector moveVec = FMath::Lerp(lastMoveVec, targetMoveVec, Acceleration * delta);

	//Angle verification
	FVector slideVector = FVector(0);
	if (angle > MaxSurfaceAngle)
	{
		slideVector = FVector::VectorPlaneProject(gravityDir, surface.SurfaceImpactNormal).GetSafeNormal() * controller->GetGravityScale();
		const FVector planedNormal = FVector::VectorPlaneProject(surface.SurfaceImpactNormal, gravityDir).GetSafeNormal();
		const FVector planarMoveVec = FVector::VectorPlaneProject(moveVec, planedNormal);
		const FVector orthogonalMoveVec = moveVec.ProjectOnToNormal(planedNormal) * ((planedNormal | moveVec) >= 0 ? 1 : 0);
		moveVec = planarMoveVec + orthogonalMoveVec;
	}

	//
	if (!controller->ActionInstances.IsValidIndex(result.StatusParams.ActionIndex))
	{
		result.StatusParams.StateModifiers = moveVec;
		UFunctionLibrary::AddOrReplaceCosmeticVector(result.StatusParams, GroundMoveVarName, moveVec);
	}
	else
	{
		const FVector relVel = FVector::VectorPlaneProject(result.Kinematics.LinearKinematic.Velocity - result.Kinematics.LinearKinematic.refVelocity, controller->GetGravityDirection());
		result.StatusParams.StateModifiers = relVel;
		UFunctionLibrary::AddOrReplaceCosmeticVector(result.StatusParams, GroundMoveVarName, relVel);
	}

	//Check if an action override state check
	bool writeMovement = true;
	if (controller->ActionInstances.IsValidIndex(result.StatusParams.ActionIndex) && controller->ActionInstances[result.StatusParams.ActionIndex].IsValid())
	{
		//Find last frame status canceller
		if (controller->ActionInstances[result.StatusParams.ActionIndex]->bShouldControllerStateCheckOverride)
			writeMovement = false;
	}

	//Write values
	result.CustomPhysicDrag = 0;
	if (writeMovement)
	{
		if (angle <= MaxSurfaceAngle)
		{
			UFunctionLibrary::AddCompositeMovement(result.Kinematics.LinearKinematic, moveVec, surface.SurfacePhysicProperties.X * (1 / (delta * delta)), 0);
		}
		else
		{
			const FVector relVel = FVector::VectorPlaneProject(result.Kinematics.LinearKinematic.Velocity - result.Kinematics.LinearKinematic.refVelocity, controller->GetGravityDirection());
			const FVector planedNormal = FVector::VectorPlaneProject(surface.SurfaceImpactNormal, gravityDir).GetSafeNormal();
			const FVector orthogonalRelVel = relVel.ProjectOnToNormal(planedNormal) * ((planedNormal | relVel) < 0 ? 1 : 0);
			result.Kinematics.LinearKinematic.Acceleration = slideVector + moveVec - ((orthogonalRelVel / delta) * 0.25);
		}
	}
	result.CustomSolverCheckDirection = controller->GetGravityDirection() * (MaxStepHeight + 6);
	UFunctionLibrary::ApplyForceOnSurfaces(result.Kinematics, surface.SurfacePoint, controller->GetGravity() * controller->GetMass(), true, ECR_Block);

	return result;
}


void USimpleGroundState::OnExitState_Implementation(UModularControllerComponent* controller,
                                                    const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
}


FString USimpleGroundState::DebugString() const
{
	return Super::DebugString();
}


#pragma endregion
