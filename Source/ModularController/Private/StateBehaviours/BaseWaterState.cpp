// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "StateBehaviours/BaseWaterState.h"

#include "FunctionLibrary.h"
#include "ToolsLibrary.h"


int UBaseWaterState::CheckSurfaceIndex(UModularControllerComponent* controller, const FControllerStatus status, FStatusParameters& statusParams, const float inDelta,
                                       float previousWaterDistance, bool asActive) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CheckSurfaceIndex");
	if (!controller)
		return -1;

	FVector gravityDirection = controller->GetGravityDirection();
	FVector location = status.Kinematics.LinearKinematic.Position;
	if (!gravityDirection.Normalize())
		gravityDirection = FVector::DownVector;
	const FVector lowestPt = controller->GetWorldSpaceCardinalPoint(gravityDirection);
	const FVector highestPt = controller->GetWorldSpaceCardinalPoint(-gravityDirection);
	const FVector velocity = status.Kinematics.LinearKinematic.Velocity;

	//Find the best surface
	int surfaceIndex = -1;
	float closestSurface = TNumericLimits<float>::Max();
	float testingClosestSurface = TNumericLimits<float>::Max();
	for (int i = 0; i < status.Kinematics.SurfacesInContact.Num(); i++)
	{
		const auto surface = status.Kinematics.SurfacesInContact[i];
		//Valid surface verification
		if (!surface.TrackedComponent.IsValid())
			continue;

		if (surface.TrackedComponent->GetCollisionResponseToChannel(ChannelWater) != ECR_Overlap)
			continue;

		const FVector heightVector = (surface.SurfacePoint - lowestPt).ProjectOnToNormal(-gravityDirection);
		const float surfaceDistance = heightVector.Length() * (heightVector.GetSafeNormal() | controller->GetGravityDirection());
		bool asTheClosest = false;
		if (heightVector.SquaredLength() < testingClosestSurface)
		{
			UFunctionLibrary::AddOrReplaceCosmeticVariable(statusParams, WaterSurfaceDistanceVarName, surfaceDistance);
			testingClosestSurface = heightVector.SquaredLength();
			asTheClosest = true;
		}

		//below surface verification
		const FVector frombelowPt = (surface.SurfacePoint - lowestPt).GetSafeNormal();
		if ((frombelowPt | gravityDirection) >= 0)
			continue;


		//Immersion verification
		if (FMath::Abs(surfaceDistance) < MinimumEntryImmersion)
		{
			if (!asActive)
			{
				// const float distDiff = surfaceDistance - previousWaterDistance;
				// if (previousWaterDistance < 0 && asTheClosest && FMath::Abs(distDiff * 2) >= ((lowestPt + (highestPt - lowestPt).GetSafeNormal() * MinimumEntryImmersion) - highestPt).Length()
				// 	&& (velocity | surface.SurfaceNormal) < 0 && FMath::Abs(surfaceDistance) < (MinimumEntryImmersion + FMath::Abs(distDiff * distDiff)))
				// {
				// }
				// else
				continue;
			}
			if (FMath::Abs(surfaceDistance) < MaximumOutroImmersion)
				continue;
		}

		if (FMath::Abs(surfaceDistance) >= closestSurface)
		{
			if (bDebugState)
				UFunctionLibrary::DrawDebugCircleOnSurface(surface, 25, FColor::Silver, inDelta * 1.5, 1, false);
			continue;
		}

		closestSurface = FMath::Abs(surfaceDistance);
		surfaceIndex = i;
	}

	//Debug
	if (bDebugState && status.Kinematics.SurfacesInContact.IsValidIndex(surfaceIndex))
	{
		UFunctionLibrary::DrawDebugCircleOnSurface(status.Kinematics.SurfacesInContact[surfaceIndex], 25, asActive ? FColor::Cyan : FColor::Orange
		                                           , inDelta * 1.5, 2, true);
	}

	return surfaceIndex;
}


FVector UBaseWaterState::WaterControl(FVector desiredMove, FVector horizontalVelocity, float delta) const
{
	if (desiredMove.Length() > 0)
	{
		const FVector resultingVector = horizontalVelocity + desiredMove * delta;
		const FVector projection = resultingVector.ProjectOnToNormal(horizontalVelocity.GetSafeNormal()).GetClampedToMaxSize(MaxSpeed);
		const FVector planed = FVector::VectorPlaneProject(resultingVector, horizontalVelocity.GetSafeNormal());
		return projection + planed;
	}
	return horizontalVelocity;
}


FControllerCheckResult UBaseWaterState::CheckState_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float inDelta,
                                                                  bool asLastActiveState) const
{
	FControllerCheckResult result = FControllerCheckResult(false, startingConditions);
	const float lastWaterDist = UFunctionLibrary::GetCosmeticVariable(result.ProcessResult.StatusParams, WaterSurfaceDistanceVarName);
	UFunctionLibrary::AddOrReplaceCosmeticVariable(result.ProcessResult.StatusParams, WaterSurfaceDistanceVarName, TNumericLimits<float>::Max());
	if (!controller)
	{
		return result;
	}

	//Check
	const int surfaceIndex = CheckSurfaceIndex(controller, startingConditions, result.ProcessResult.StatusParams, inDelta, lastWaterDist, asLastActiveState);
	result.CheckedCondition = surfaceIndex >= 0;
	if (result.CheckedCondition)
		result.ProcessResult.Kinematics.SurfaceBinaryFlag = UToolsLibrary::IndexToFlag(surfaceIndex);

	return result;
}

void UBaseWaterState::OnEnterState_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
	Super::OnEnterState_Implementation(controller, startingConditions, moveInput, delta);
}

FControllerStatus UBaseWaterState::ProcessState_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta) const
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
	const FVector lowestPt = controller->GetWorldSpaceCardinalPoint(gravityDir);
	const FVector highestPt = controller->GetWorldSpaceCardinalPoint(-gravityDir);
	const FVector heightVector = (surface.SurfacePoint - lowestPt).ProjectOnToNormal(-gravityDir);
	const float surfaceDistance = heightVector.Length();

	//Collect inputs
	const FVector inputMove = result.MoveInput;

	//Parameters from inputs
	const float turnSpd = TurnSpeed;

	//Rotate
	result.Kinematics.AngularKinematic = UFunctionLibrary::LookAt(result.Kinematics.AngularKinematic, inputMove, turnSpd, delta);

	//Components separation
	const FVector HorizontalVelocity = FVector::VectorPlaneProject(startingConditions.Kinematics.LinearKinematic.Velocity, gravityDir);
	const FVector verticalVelocity = startingConditions.Kinematics.LinearKinematic.Velocity.ProjectOnToNormal(gravityDir);

	//Buoyancy
	FVector buoyancyVector = UFunctionLibrary::GetSnapOnSurfaceVector(
		controller->GetWorldSpaceCardinalPoint(gravityDir) - gravityDir * (MinimumEntryImmersion + FMath::Abs(MinimumEntryImmersion - MaximumOutroImmersion)), surface, gravityDir);
	buoyancyVector *= ArchimedForceScale;
	if((verticalVelocity | gravityDir) < 0)
	{
		const FVector kinetic = -UFunctionLibrary::GetKineticEnergy(verticalVelocity, controller->GetMass(), FMath::Abs(surfaceDistance - MinimumEntryImmersion));
		const float controllerLenght = (lowestPt - highestPt).Length();
		buoyancyVector += (kinetic / controller->GetMass()) * FMath::Clamp((surfaceDistance - MaximumOutroImmersion) / (controllerLenght - MinimumEntryImmersion), 0, 1);
	}
	if(controller->_externalForces.SquaredLength() > buoyancyVector.SquaredLength() && (controller->_externalForces | gravityDir) < 0)
		buoyancyVector = FVector(0);
	result.Kinematics.LinearKinematic.Acceleration = buoyancyVector;

	//Water control
	result.Kinematics.LinearKinematic.Velocity = WaterControl(inputMove * MaxSpeed, HorizontalVelocity, delta) + verticalVelocity;

	//Write values
	result.CustomPhysicDrag = WaterDrag;
	result.CustomSolverCheckParameters = controller->GetGravityDirection() * -MaxWaterCheckDeep;

	return result;
}

void UBaseWaterState::OnExitState_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
	Super::OnExitState_Implementation(controller, startingConditions, moveInput, delta);
}

FString UBaseWaterState::DebugString() const
{
	return Super::DebugString();
}
