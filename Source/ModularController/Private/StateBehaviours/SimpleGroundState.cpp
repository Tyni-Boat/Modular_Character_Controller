// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "StateBehaviours/SimpleGroundState.h"
#include <Kismet/KismetMathLibrary.h>

#include "FunctionLibrary.h"
#include "ToolsLibrary.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"


#define FLOATING_HEIGHT 5


//Check if we are on the ground
#pragma region Check XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


int USimpleGroundState::CheckSurfaceIndex(UModularControllerComponent* controller, const FControllerStatus status, FStatusParameters& statusParams, const float inDelta, bool asActive) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CheckSurfaceIndex");
	if (!controller)
		return 0;

	FVector gravityDirection = controller->GetGravityDirection();
	FVector location = status.Kinematics.LinearKinematic.Position;
	if (!gravityDirection.Normalize())
		gravityDirection = FVector::DownVector;
	const FVector lowestPt = controller->GetWorldSpaceCardinalPoint(gravityDirection);
	const FVector velocity = status.Kinematics.LinearKinematic.Velocity;

	//Find the best surface
	int surfaceIndex = -1;
	float closestSurface = TNumericLimits<float>::Max();
	float closestSurface_low = TNumericLimits<float>::Max();
	float closestCheckSurface = TNumericLimits<float>::Max();

	// Default on bad angles
	int badAngleIndex = -1;
	float closestBadAngle = TNumericLimits<float>::Max();

	for (int i = 0; i < status.Kinematics.SurfacesInContact.Num(); i++)
	{
		const auto surface = status.Kinematics.SurfacesInContact[i];
		//Valid surface verification
		if (!surface.TrackedComponent.IsValid())
			continue;

		if (surface.TrackedComponent->GetCollisionResponseToChannel(ChannelGround) != SurfaceParams.CollisionResponse)
			continue;

		//Only surfaces we can step on
		if (static_cast<bool>(surface.SurfacePhysicProperties.W) != SurfaceParams.bMustBeStepable)
			continue;

		//Above surface verification
		const FVector fromCenter = (surface.SurfacePoint - location).GetSafeNormal();
		if ((fromCenter | gravityDirection) <= 0)
			continue;

		const FVector centerHeightVector = (surface.SurfacePoint - location).ProjectOnToNormal(-gravityDirection);
		const FVector heightVector = (surface.SurfacePoint - lowestPt).ProjectOnToNormal(-gravityDirection);
		const float angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(asActive ? surface.SurfaceImpactNormal : surface.SurfaceNormal, -gravityDirection)));
		const FVector farAwayVector = FVector::VectorPlaneProject(surface.SurfacePoint - location, gravityDirection);
		const FVector shapePtInDir = controller->GetWorldSpaceCardinalPoint(farAwayVector);
		const FVector inShapeDir = shapePtInDir - location;
		if (UToolsLibrary::CheckInRange(SurfaceParams.ImpactAngleRange, angle) && closestCheckSurface > heightVector.Length()
			&& (inShapeDir.SquaredLength() <= 0 || (inShapeDir.SquaredLength() > 0 && farAwayVector.Length() < inShapeDir.Length() * 0.75)))
		{
			closestCheckSurface = heightVector.Length();
		}

		//Step height verification
		if (heightVector.Length() > (SurfaceParams.HeightRange.Y + ((heightVector | gravityDirection) > 0 ? 10 : 0)))
			continue;

		// Avoid too far down surfaces on first detection
		if (!asActive && heightVector.Length() > FLOATING_HEIGHT && (heightVector | gravityDirection) > 0)
			continue;

		//Angle verification
		if (!UToolsLibrary::CheckInRange(SurfaceParams.ImpactAngleRange, angle))
		{
			const float badDistance = heightVector.Length() * ((surface.SurfacePoint - lowestPt).GetSafeNormal() | gravityDirection);
			if (badDistance >= closestBadAngle)
			{
				continue;
			}
			if(!asActive && (surface.SurfaceNormal | surface.SurfaceImpactNormal) < 0.9)
				continue;

			badAngleIndex = i;
			closestBadAngle = badDistance;
			continue;
		}

		//Avoid far distances when not active
		if (!asActive && inShapeDir.SquaredLength() > 0 && farAwayVector.Length() >= inShapeDir.Length() * 0.75)
			continue;

		const float distance_low = heightVector.Length() * ((surface.SurfacePoint - lowestPt).GetSafeNormal() | gravityDirection);
		const float distance = centerHeightVector.Length();
		if (distance >= closestSurface)
		{
			if (bDebugState)
				UFunctionLibrary::DrawDebugCircleOnSurface(surface, 25, FColor::Silver, inDelta * 1.5, 1, false);
			continue;
		}

		//How far from center is the surface point
		if (inShapeDir.SquaredLength() > 0 && inShapeDir.Length() <= farAwayVector.Length() && (heightVector | gravityDirection) < 0)
		{
			// Check if the step is safe
			if ((closestSurface - distance) >= FLOATING_HEIGHT && controller->UpdatedPrimitive)
			{
				const FVector virtualSnap = UFunctionLibrary::GetSnapOnSurfaceVector(lowestPt, surface, gravityDirection);
				const FVector offset = farAwayVector.GetSafeNormal() * SurfaceParams.DepthRange.X + virtualSnap + virtualSnap.GetSafeNormal() * FLOATING_HEIGHT;
				const auto shape = controller->UpdatedPrimitive->GetCollisionShape(0);
				const auto channel = controller->UpdatedPrimitive->GetCollisionObjectType();
				if (controller->OverlapTest(location + offset, status.Kinematics.AngularKinematic.Orientation, channel, shape, controller->GetOwner()))
				{
					if (bDebugState)
						UFunctionLibrary::DrawDebugCircleOnSurface(surface, 25, FColor::Black, inDelta * 1.5, 1, false);
					continue;
				}
			}
		}

		closestSurface = distance;
		closestSurface_low = distance_low;
		surfaceIndex = i;
	}

	UFunctionLibrary::AddOrReplaceCosmeticVariable(statusParams, GroundDistanceVarName, closestSurface_low < closestCheckSurface ? closestSurface_low : closestCheckSurface);

	//If we are ascending. do this here because ground distance evaluation
	if ((velocity | gravityDirection) < -FLOATING_HEIGHT && !asActive)
		return 0;

	//Compute the flag
	int flag = UToolsLibrary::BoolArrayToFlag(UToolsLibrary::IndexesToBoolArray(TArray<int>{surfaceIndex, badAngleIndex}));

	//Debug
	if (bDebugState)
	{
		if (status.Kinematics.SurfacesInContact.IsValidIndex(surfaceIndex))
			UFunctionLibrary::DrawDebugCircleOnSurface(status.Kinematics.SurfacesInContact[surfaceIndex], 25, asActive ? FColor::Blue : FColor::Yellow
			                                           , inDelta * 1.5, 2, true);
		if (status.Kinematics.SurfacesInContact.IsValidIndex(badAngleIndex))
			UFunctionLibrary::DrawDebugCircleOnSurface(status.Kinematics.SurfacesInContact[badAngleIndex], 25, asActive ? FColor::Purple : FColor::Magenta
			                                           , inDelta * 1.5, 2, true);
	}

	return flag;
}


#pragma endregion


#pragma region General Movement XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


FVector USimpleGroundState::GetMoveVector(const FVector inputVector, const float moveScale, const FSurface Surface, const UModularControllerComponent* controller) const
{
	FVector desiredMove = inputVector;
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
	if (!asLastActiveState)
		UFunctionLibrary::AddOrReplaceCosmeticVariable(result.ProcessResult.StatusParams, GroundDistanceVarName, TNumericLimits<float>::Max());
	if (!controller)
	{
		return result;
	}

	//Check
	const int surfaceFlag = CheckSurfaceIndex(controller, startingConditions, result.ProcessResult.StatusParams, inDelta, asLastActiveState);
	result.CheckedCondition = surfaceFlag > 0;
	if (result.CheckedCondition)
	{
		result.ProcessResult.Kinematics.SurfaceBinaryFlag = surfaceFlag;
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
	const FVector gravityDir = controller->GetGravityDirection();
	const FVector lowestPt = controller->GetWorldSpaceCardinalPoint(gravityDir);
	const TArray<int> indexes = UToolsLibrary::BoolToIndexesArray(UToolsLibrary::FlagToBoolArray(result.Kinematics.SurfaceBinaryFlag));
	int primarySurfaceIndex = UFunctionLibrary::GetSurfaceIndexUnderCondition(result.Kinematics, [gravityDir, this](const FSurface& surface) -> bool
	{
		const float angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(surface.SurfaceImpactNormal, -gravityDir)));
		return angle <= SurfaceParams.ImpactAngleRange.Y;
	});
	int secondarySurfaceIndex = UFunctionLibrary::GetSurfaceIndexUnderCondition(result.Kinematics, [gravityDir, this](const FSurface& surface) -> bool
	{
		const float angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(surface.SurfaceImpactNormal, -gravityDir)));
		return angle > SurfaceParams.ImpactAngleRange.Y;
	});
	if (!result.Kinematics.SurfacesInContact.IsValidIndex(primarySurfaceIndex))
	{
		if (!result.Kinematics.SurfacesInContact.IsValidIndex(secondarySurfaceIndex))
		{
			return result;
		}
		else
		{
			primarySurfaceIndex = secondarySurfaceIndex;
		}
	}
	const auto primarySurface = result.Kinematics.SurfacesInContact[primarySurfaceIndex];
	const auto secondarySurface = result.Kinematics.SurfacesInContact.IsValidIndex(secondarySurfaceIndex) ? result.Kinematics.SurfacesInContact[secondarySurfaceIndex] : FSurface();
	const FVector primaryHeightVector = (primarySurface.SurfacePoint - lowestPt).ProjectOnToNormal(-gravityDir);
	const float primaryAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(primarySurface.SurfaceImpactNormal, -gravityDir)));
	float secondaryAngle = 0;
	FVector secondaryHeightVector = FVector(0);
	if (secondarySurface.TrackedComponent.IsValid())
	{
		secondaryHeightVector = (secondarySurface.SurfacePoint - lowestPt).ProjectOnToNormal(-gravityDir);
		secondaryAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(secondarySurface.SurfaceImpactNormal, -gravityDir)));
	}

	//Collect inputs
	const FVector inputMove = result.MoveInput;
	const FVector lockOnDirection = controller->ReadAxisInput(LockOnDirection);
	const bool lockedOn = lockOnDirection.SquaredLength() > 0;

	//Parameters from inputs
	float moveScale = 1;

	//Snapping
	const FVector snapVector = UFunctionLibrary::GetSnapOnSurfaceVector(
		controller->GetWorldSpaceCardinalPoint(gravityDir) + gravityDir * FLOATING_HEIGHT * (primaryAngle > SurfaceParams.ImpactAngleRange.Y ? 0 : 1), primarySurface, gravityDir);
	result.Kinematics.LinearKinematic.SnapDisplacement = snapVector * SnapSpeed;

	//Lerp velocity
	FVector lastMoveVec = controller->TimeOnCurrentState <= delta
		                      ? FVector::VectorPlaneProject(result.Kinematics.LinearKinematic.Velocity, gravityDir).GetClampedToMaxSize(MaxSpeed)
		                      : result.StatusParams.StateModifiers;
	FSurface cloneSurface = primarySurface;
	if (result.Kinematics.LastMoveHit.HitResult.ImpactNormal.IsNormalized())
	{
		cloneSurface.SurfaceImpactNormal = result.Kinematics.LastMoveHit.HitResult.ImpactNormal;
		cloneSurface.SurfaceNormal = result.Kinematics.LastMoveHit.HitResult.Normal;
	}
	const FVector downSnap = snapVector.ProjectOnToNormal(gravityDir);
	const FVector userMove = inputMove * MaxSpeed * moveScale;
	const FVector originalMoveVec = FMath::Lerp(lastMoveVec, userMove, Acceleration * delta);
	const FVector postRMMove = controller->GetRootMotionTranslation(RootMotionMode, userMove);
	FVector moveVec = GetMoveVector(postRMMove, moveScale, cloneSurface, controller) * FMath::Clamp(1 - (downSnap.Length() / (SurfaceParams.HeightRange.Y * 0.5)), 0, 1);

	//Angle verification
	FVector slideVector = FVector(0);
	if (secondaryAngle > SurfaceParams.ImpactAngleRange.Y && secondaryHeightVector.SquaredLength() > 0 && (primaryHeightVector.Length() - secondaryHeightVector.Length()) > FLOATING_HEIGHT)
	{
		const FVector planedNormal = FVector::VectorPlaneProject(secondarySurface.SurfaceImpactNormal, gravityDir).GetSafeNormal();
		const FVector planarMoveVec = FVector::VectorPlaneProject(moveVec, planedNormal);
		const FVector orthogonalMoveVec = moveVec.ProjectOnToNormal(planedNormal) * ((planedNormal | moveVec) >= 0 ? 1 : 0);
		moveVec = planarMoveVec + orthogonalMoveVec;
	}
	if (primaryAngle > SurfaceParams.ImpactAngleRange.Y)
	{
		slideVector = FVector::VectorPlaneProject(gravityDir, primarySurface.SurfaceImpactNormal).GetSafeNormal() * controller->GetGravityScale();
	}

	//Rotate
	const float turnSpd = primaryAngle > SurfaceParams.ImpactAngleRange.Y && SlideTurnSpeed > 0 ? SlideTurnSpeed : TurnSpeed;
	FVector lookDir = lockedOn ? lockOnDirection : (slideVector.SquaredLength() > 0 && SlideTurnSpeed > 0 ? slideVector : inputMove);
	result.Kinematics.AngularKinematic = UFunctionLibrary::LookAt(result.Kinematics.AngularKinematic, lookDir, turnSpd, delta);

	//
	if (!controller->ActionInstances.IsValidIndex(result.StatusParams.ActionIndex))
	{
		result.StatusParams.StateModifiers = originalMoveVec;
		UFunctionLibrary::AddOrReplaceCosmeticVector(result.StatusParams, GroundMoveVarName, originalMoveVec);
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
		if (primaryAngle <= SurfaceParams.ImpactAngleRange.Y)
		{
			UFunctionLibrary::AddCompositeMovement(result.Kinematics.LinearKinematic, moveVec, primarySurface.SurfacePhysicProperties.X * (1 / (delta * delta)), 0);
		}
		else
		{
			const FVector relVel = FVector::VectorPlaneProject(result.Kinematics.LinearKinematic.Velocity - result.Kinematics.LinearKinematic.refVelocity, controller->GetGravityDirection());
			const FVector planedNormal = FVector::VectorPlaneProject(primarySurface.SurfaceImpactNormal, gravityDir).GetSafeNormal();
			const FVector orthogonalRelVel = relVel.ProjectOnToNormal(planedNormal) * ((planedNormal | relVel) < 0 ? 1 : 0);
			result.Kinematics.LinearKinematic.Acceleration = slideVector + moveVec - ((orthogonalRelVel / delta) * 0.25);
		}
	}
	const FVector scanDir = controller->GetGravityDirection() * (SurfaceParams.HeightRange.Y + FLOATING_HEIGHT + 1);
	result.CustomSolverCheckParameters = FVector4(scanDir.X, scanDir.Y, scanDir.Z, 0.125);
	UFunctionLibrary::ApplyForceOnSurfaces(result.Kinematics, primarySurface.SurfacePoint, controller->GetGravity() * controller->GetMass(), true, ECR_Block);

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
