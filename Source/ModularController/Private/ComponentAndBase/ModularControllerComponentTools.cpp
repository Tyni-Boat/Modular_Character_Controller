// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#include "ComponentAndBase/ModularControllerComponent.h"

#include <functional>
#include "Kismet/KismetMathLibrary.h"
#include "Engine.h"
#include "FunctionLibrary.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "ToolsLibrary.h"


#pragma region Tools & Utils

bool UModularControllerComponent::ComponentTraceSingleUntil(FHitResult& outHit, FVector direction, FVector position,
                                                            FQuat rotation, std::function<bool(FHitResult)> condition, int iterations, double inflation, bool traceComplex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ComponentTraceCastSingleUntil");

	FCollisionQueryParams queryParams = FCollisionQueryParams::DefaultQueryParam;
	//TArray<FHitResult> hits;
	// if (ComponentTraceCastMulti_internal(hits, position, direction, rotation, inflation, traceComplex, queryParams))
	// {
	// 	for (int i = 0; i < hits.Num(); i++)
	// 	{
	// 		if (condition(hits[i]))
	// 		{
	// 			outHit = hits[i];
	// 			return true;
	// 		}
	// 	}
	// }
	for (int i = 0; i < iterations; i++)
	{
		FHitResult iterationHit;
		if (ComponentTraceSingle_Internal(iterationHit, position, direction, rotation, inflation, traceComplex, queryParams))
		{
			if (condition(iterationHit))
			{
				outHit = iterationHit;
				return true;
			}

			queryParams.AddIgnoredComponent(iterationHit.GetComponent());
			continue;
		}

		break;
	}
	return false;
}


bool UModularControllerComponent::ComponentTraceMulti_internal(TArray<FHitResultExpanded>& outHits, FVector position, FVector direction, FQuat rotation, double inflation,
                                                               bool traceComplex,
                                                               FCollisionQueryParams& queryParams, double counterDirectionMaxOffset) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ComponentTraceCastMulti");

	if (!UpdatedPrimitive)
		return false;

	queryParams.AddIgnoredActor(GetOwner());
	queryParams.bTraceComplex = traceComplex;
	queryParams.bReturnPhysicalMaterial = true;
	const float OverlapInflation = inflation;
	const auto shape = UpdatedPrimitive->GetCollisionShape(OverlapInflation);
	const auto channel = UpdatedPrimitive->GetCollisionObjectType();
	FCollisionResponseParams response = FCollisionResponseParams::DefaultResponseParam;
	response.CollisionResponse.SetAllChannels(ECollisionResponse::ECR_Block);
	const float shapeHalfLenght = (GetWorldSpaceCardinalPoint(direction) - GetLocation()).Length();

	constexpr int maxIterations = 64;
	TArray<FHitResult> loopHits;
	FCollisionQueryParams loopQueryParams = queryParams;
	for (int i = 0; i < maxIterations; i++)
	{
		const bool result = GetWorld()->SweepMultiByChannel(loopHits, position, position + direction, rotation, channel, shape, loopQueryParams);
		for (int j = 0; j < loopHits.Num(); j++)
		{
			auto queryType = loopHits[j].Component->GetCollisionResponseToChannel(UpdatedPrimitive->GetCollisionObjectType());
			outHits.Add(FHitResultExpanded(loopHits[j], 0, queryType));
			if (queryType == ECR_Block && direction.SquaredLength() > 0)
			{
				int hitIndex = 1;
				int maxSubSurfaceHit = 5;
				const auto respParam = FCollisionResponseParams::DefaultResponseParam;
				const auto objParam = FCollisionObjectQueryParams::DefaultObjectQueryParam;

				FHitResult zeroHitResult;
				const FVector chkDir = direction + direction.GetSafeNormal() * (shapeHalfLenght * 3);
				const FVector penetrationVector = direction.GetSafeNormal() * 3;

				FVector awayOffset = FVector::VectorPlaneProject(loopHits[j].ImpactPoint - position, direction.GetSafeNormal());
				FVector pt = loopHits[j].ImpactPoint + penetrationVector + awayOffset.GetSafeNormal() * 0.125;
				if (DebugType == EControllerDebugType::PhysicDebug)
					UKismetSystemLibrary::DrawDebugArrow(this, pt, pt + chkDir, 50, FColor::Silver, 0.01);
				while (loopHits[j].GetComponent()->LineTraceComponent(zeroHitResult, pt, pt + chkDir, channel, loopQueryParams, respParam, objParam))
				{
					if (zeroHitResult.Distance < 3)
						break;
					FHitResult innerHit;
					FVector innerPt = zeroHitResult.ImpactPoint - chkDir.GetSafeNormal() * (shapeHalfLenght + OverlapInflation + 0.125);
					if (loopHits[j].GetComponent()->SweepComponent(innerHit, innerPt, innerPt + chkDir, rotation, shape, traceComplex))
					{
						if (innerHit.bStartPenetrating)
						{
							innerHit.ImpactPoint = zeroHitResult.ImpactPoint;
							innerHit.Normal = zeroHitResult.Normal;
							innerHit.ImpactNormal = zeroHitResult.ImpactNormal;
						}
						outHits.Add(FHitResultExpanded(innerHit, hitIndex, queryType));
						hitIndex++;
					}
					pt = zeroHitResult.ImpactPoint + penetrationVector;
					maxSubSurfaceHit--;
					if (maxSubSurfaceHit <= 0)
						break;
				}
			}
			loopQueryParams.AddIgnoredComponent(loopHits[j].GetComponent());
		}

		if (!result)
			break;
	}
	queryParams.ClearIgnoredActors();

	return outHits.Num() > 0;
}


bool UModularControllerComponent::ComponentTraceSingle_Internal(FHitResult& outHit, FVector position, FVector direction, FQuat rotation, double inflation, bool traceComplex,
                                                                FCollisionQueryParams& queryParams) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ComponentTraceCastSingle");

	if (!UpdatedPrimitive)
		return false;

	queryParams.AddIgnoredActor(GetOwner());
	queryParams.bTraceComplex = traceComplex;
	queryParams.bReturnPhysicalMaterial = true;
	const float OverlapInflation = inflation;
	const auto shape = UpdatedPrimitive->GetCollisionShape(OverlapInflation);
	const auto channel = UpdatedPrimitive->GetCollisionObjectType();

	const bool result = GetWorld()->SweepSingleByChannel(outHit, position, position + direction, rotation, channel, shape, queryParams);
	queryParams.ClearIgnoredActors();

	return result;
}


bool UModularControllerComponent::ComponentPathTraceSingle_Internal(FHitResult& result, int& pathPtIndex, TArray<FTransform> pathPoints, float inflation, bool traceComplex,
                                                                    FCollisionQueryParams& queryParams)
{
	if (pathPoints.Num() <= 1)
		return false;
	FHitResult hit;
	float cumulatedDistance = 0;
	for (int i = 1; i < pathPoints.Num(); i++)
	{
		if (ComponentTraceSingle_Internal(hit, pathPoints[i - 1].GetLocation(), pathPoints[i].GetLocation() - pathPoints[i - 1].GetLocation()
		                                  , pathPoints[i - 1].GetRotation(), inflation, traceComplex, queryParams))
		{
			hit.Distance += cumulatedDistance;
			result = hit;
			pathPtIndex = i - 1;
			return true;
		}
		cumulatedDistance += (pathPoints[i].GetLocation() - pathPoints[i - 1].GetLocation()).Length();
	}

	return false;
}


bool UModularControllerComponent::ComponentPathTraceMulti_Internal(TArray<FHitResultExpanded>& results, TArray<int>& pathPtIndexes, TArray<FTransform> pathPoints, float inflation,
                                                                   bool traceComplex,
                                                                   FCollisionQueryParams& queryParams)
{
	if (pathPoints.Num() <= 1)
		return false;
	TArray<FHitResultExpanded> innerHits;
	float cumulatedDistance = 0;
	for (int i = 1; i < pathPoints.Num(); i++)
	{
		if (ComponentTraceMulti_internal(innerHits, pathPoints[i - 1].GetLocation(), pathPoints[i].GetLocation() - pathPoints[i - 1].GetLocation()
		                                 , pathPoints[i - 1].GetRotation(), inflation, traceComplex, queryParams))
		{
			for (int j = 0; j < innerHits.Num(); j++)
			{
				innerHits[j].HitResult.Distance += cumulatedDistance;
				results.Add(innerHits[j]);
			}
			pathPtIndexes.Add(i - 1);
		}
		cumulatedDistance += (pathPoints[i].GetLocation() - pathPoints[i - 1].GetLocation()).Length();
	}

	return results.Num() > 0;
}


bool UModularControllerComponent::CheckPenetrationAt(FVector& separationForce, FVector& contactForce, FVector atPosition, FQuat withOrientation, UPrimitiveComponent* onlyThisComponent,
                                                     double hullInflation, bool getVelocity)
{
	{
		FVector moveVec = FVector(0);
		FVector velVec = FVector(0);
		auto owner = GetOwner();
		if (owner == nullptr)
			return false;

		UPrimitiveComponent* primitive = UpdatedPrimitive;
		if (!primitive)
			return false;
		bool overlapFound = false;
		TArray<FOverlapResult> _overlaps;
		FComponentQueryParams comQueryParams;
		comQueryParams.AddIgnoredActor(owner);
		if (GetWorld()->OverlapMultiByChannel(_overlaps, atPosition, withOrientation, primitive->GetCollisionObjectType(), primitive->GetCollisionShape(hullInflation), comQueryParams))
		{
			FMTDResult depenetrationInfos;
			for (int i = 0; i < _overlaps.Num(); i++)
			{
				auto& overlap = _overlaps[i];

				if (!overlapFound)
					overlapFound = true;

				if (overlap.Component->GetOwner() == this->GetOwner())
					continue;

				if (DebugType == EControllerDebugType::MovementDebug)
				{
					FVector thisClosestPt;
					FVector compClosestPt;
					overlap.Component->GetClosestPointOnCollision(atPosition, compClosestPt);
					primitive->GetClosestPointOnCollision(compClosestPt, thisClosestPt);
					const FVector separationVector = compClosestPt - thisClosestPt;
					UKismetSystemLibrary::DrawDebugArrow(this, compClosestPt, compClosestPt + separationVector * 10, 1, FColor::Silver, 0, 0.1);
					if (overlap.GetActor())
					{
						UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Overlap Actor: (%s)"), *overlap.GetActor()->GetActorNameOrLabel()), true, true, FColor::White, 0,
						                                  FName(FString::Printf(TEXT("Overlap_%s"), *overlap.GetActor()->GetActorNameOrLabel())));
					}
				}

				if (overlap.Component->ComputePenetration(depenetrationInfos, primitive->GetCollisionShape(hullInflation), atPosition, withOrientation))
				{
					if (DebugType == EControllerDebugType::MovementDebug)
					{
						if (overlap.GetActor())
						{
							UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Depentrate Actor: (%s)"), *overlap.GetActor()->GetActorNameOrLabel()), true, true, FColor::Silver,
							                                  0, FName(FString::Printf(TEXT("OverlapPenetration_%s"), *overlap.GetActor()->GetActorNameOrLabel())));
						}
					}

					const FVector depForce = depenetrationInfos.Direction * depenetrationInfos.Distance;
					FVector hullPt = PointOnShape(-depenetrationInfos.Direction, atPosition);

					if (DebugType == EControllerDebugType::MovementDebug)
					{
						UKismetSystemLibrary::DrawDebugArrow(this, hullPt, hullPt + depForce * 10, 100, FColor::White, 0.018, 0.5);
					}

					FVector overlapObjectForce = FVector(0);
					if (getVelocity)
					{
						UModularControllerComponent* otherModularComponent = nullptr;
						if (overlap.GetActor())
						{
							auto component = overlap.GetActor()->GetComponentByClass(UModularControllerComponent::StaticClass());
							if (component != nullptr)
							{
								otherModularComponent = Cast<UModularControllerComponent>(component);
							}
						}

						bool showDebug = false;

						if (overlap.GetComponent()->IsSimulatingPhysics())
						{
							const double compMass = overlap.GetComponent()->GetMass();
							overlapObjectForce = (overlap.GetComponent()->GetPhysicsLinearVelocityAtPoint(hullPt) * compMass).ProjectOnToNormal(depenetrationInfos.Direction);
							showDebug = true;
						}
						else if (otherModularComponent != nullptr)
						{
							overlapObjectForce = otherModularComponent->ComputedControllerStatus.Kinematics.LinearKinematic.Acceleration.ProjectOnToNormal(depenetrationInfos.Direction);
							showDebug = true;
						}

						if (showDebug && DebugType == EControllerDebugType::MovementDebug)
						{
							UKismetSystemLibrary::DrawDebugArrow(this, hullPt, hullPt + overlapObjectForce, 100, FColor::Silver, 0.018, 1);
						}
					}

					if (onlyThisComponent == overlap.Component)
					{
						separationForce = depForce;
						contactForce = overlapObjectForce;
						return true;
					}
					moveVec += depForce;
					velVec += overlapObjectForce;
				}
			}
		}

		if (onlyThisComponent != nullptr)
		{
			separationForce = FVector(0);
			contactForce = FVector(0);
			return false;
		}

		separationForce = moveVec;
		contactForce = velVec;
		return overlapFound;
	}
}


FVector UModularControllerComponent::PointOnShape(FVector direction, const FVector inLocation, const float hullInflation)
{
	if (!UpdatedPrimitive)
		return inLocation;

	const auto bounds = UpdatedPrimitive->Bounds;
	const FVector bCenter = bounds.Origin;
	const float bRadius = bounds.SphereRadius;
	direction.Normalize();
	const FVector outterBoundPt = GetLocation() + direction * bRadius;
	const FVector offset = inLocation - GetLocation();
	FVector onColliderPt;
	UpdatedPrimitive->GetClosestPointOnCollision(outterBoundPt, onColliderPt);

	return onColliderPt + offset + direction * hullInflation;
}


bool UModularControllerComponent::EvaluateSurfaceConditions(FSurfaceCheckParams conditions, FSurfaceCheckResponse& response, FControllerStatus inStatus,
                                                            FVector locationOffset, FVector orientationOffset, FVector solverChkParam,
                                                            FVector customDirection)
{
	if (!UpdatedPrimitive)
		return false;

	// Prediction
	bool asPrediction = conditions.PredictionDistanceRange.Y > conditions.PredictionDistanceRange.X && conditions.PredictionDistanceRange.X >= 0;
	bool foundTrue = false;
	FControllerStatus status = inStatus;
	TArray<FSurface> surfaces;
	if (asPrediction)
	{
		FHitResult trajHit;
		TArray<FTransform> trajPts;
		int trajHitIndex = -1;
		status.Kinematics.SurfacesInContact.Empty();
		//Prediction trajectory
		auto trajectory = UFunctionLibrary::MakeKinematicsTrajectory(status.Kinematics, MaxHistorySamples, HistoryTimeStep);
		for (int i = 0; i < trajectory.Num(); i++)
		{
			trajPts.Add(FTransform(trajectory[i].AngularKinematic.Orientation, trajectory[i].LinearKinematic.Position));
		}
		if (!ComponentPathTraceSingle(trajHit, trajHitIndex, trajPts, 0, bUseComplexCollision))
			return false;
		if (!UToolsLibrary::CheckInRange(conditions.PredictionDistanceRange, trajHit.Distance, true))
			return false;
		if (!trajectory.IsValidIndex(trajHitIndex))
			return false;
		status.Kinematics.LinearKinematic = trajectory[trajHitIndex].LinearKinematic;
		status.Kinematics.LinearKinematic.Position = trajHit.Location;
		status.Kinematics.AngularKinematic = trajectory[trajHitIndex].AngularKinematic;
		//Get surfaces
		int maxDepth = 0;
		FTransform customTr = FTransform(status.Kinematics.AngularKinematic.Orientation, status.Kinematics.LinearKinematic.Position);
		TArray<FHitResultExpanded> hits;
		OverlapSolver(maxDepth, HistoryTimeStep, &hits, solverChkParam.SquaredLength() > 0 ? FVector4(solverChkParam) : inStatus.CustomSolverCheckParameters,
		              &customTr);
		HandleTrackedSurface(status, hits, HistoryTimeStep);
		surfaces = status.Kinematics.SurfacesInContact;
	}
	else
	{
		//Use status surfaces
		surfaces = status.Kinematics.SurfacesInContact;
	}

	if (surfaces.Num() <= 0)
		return false;

	for (auto surface : surfaces)
	{
		// Conditions are never valid on invalid surface
		if (!surface.TrackedComponent.IsValid())
			continue;
		const FVector chkSurfaceDirection = (customDirection.SquaredLength() <= 0 ? -status.Kinematics.GetGravityDirection() : customDirection).GetSafeNormal();
		const FVector orientation = (status.Kinematics.AngularKinematic.Orientation * FQuat(orientationOffset.GetSafeNormal(), orientationOffset.Length())).Vector().GetSafeNormal();
		const FVector location = status.Kinematics.LinearKinematic.Position;
		const FVector offsetLocation = location + locationOffset;

		//Collision response test
		if (conditions.CollisionResponse != ECR_MAX && static_cast<ECollisionResponse>(surface.SurfacePhysicProperties.Z) != conditions.CollisionResponse)
			continue;

		//Stepability test
		if (conditions.bMustBeStepable != static_cast<bool>(surface.SurfacePhysicProperties.W))
			continue;

		//Heigth Test
		const FVector heightVector = (surface.SurfacePoint - offsetLocation).ProjectOnToNormal(chkSurfaceDirection);
		const float directionalHeightScale = heightVector.Length() * ((heightVector | chkSurfaceDirection) > 0 ? 1 : -1);
		if (!UToolsLibrary::CheckInRange(conditions.HeightRange, directionalHeightScale, true))
			continue;

		//Angle Test (N)
		const float normalAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(surface.SurfaceNormal, chkSurfaceDirection)));
		if (!UToolsLibrary::CheckInRange(conditions.NormalAngleRange, normalAngle, true))
			continue;

		//Angle Test (I)
		const float impactAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(surface.SurfaceImpactNormal, chkSurfaceDirection)));
		if (!UToolsLibrary::CheckInRange(conditions.ImpactAngleRange, impactAngle, true))
			continue;

		//Offset test
		const FVector farAwayVector = FVector::VectorPlaneProject(surface.SurfacePoint - location, chkSurfaceDirection);
		const FVector shapePtInDir = GetWorldSpaceCardinalPoint(farAwayVector);
		const FVector inShapeDir = shapePtInDir - location;
		if (inShapeDir.SquaredLength() > 0 && !UToolsLibrary::CheckInRange(conditions.OffsetRange, farAwayVector.SquaredLength() / inShapeDir.SquaredLength(), true))
			continue;

		//Angle Test (orientation)
		const float orientationAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(farAwayVector.GetSafeNormal(), orientation)));
		if (!UToolsLibrary::CheckInRange(conditions.OrientationAngleRange, orientationAngle, true))
			continue;

		//Depth test
		if (conditions.DepthRange.Z > 0 && conditions.DepthRange.X > 0)
		{
			const FVector virtualSnap = UFunctionLibrary::GetSnapOnSurfaceVector(offsetLocation, surface, chkSurfaceDirection);
			FVector offset = farAwayVector.GetSafeNormal() * conditions.DepthRange.Z;
			FHitResult hitMantle;
			FVector pt = location + virtualSnap + virtualSnap.GetSafeNormal() * 0.1;
			if (!ComponentTraceSingle(hitMantle, pt, offset, status.Kinematics.AngularKinematic.Orientation, 0))
				hitMantle.Distance = offset.Length();
			if (!UToolsLibrary::CheckInRange(FVector2D(conditions.DepthRange.X, conditions.DepthRange.Z + 0.125), hitMantle.Distance, true))
				continue;
			if (conditions.DepthRange.Y >= 0)
			{
				offset = offset.GetSafeNormal() * hitMantle.Distance;
				FHitResult hitBackWall;
				if (!surface.TrackedComponent->LineTraceComponent(hitBackWall, location + offset, location, FCollisionQueryParams::DefaultQueryParam))
					continue;
				if (hitBackWall.bStartPenetrating)
					continue;
				FHitResult hitVault;
				pt = hitBackWall.ImpactPoint + offset.GetSafeNormal() * inShapeDir.Length() + virtualSnap + virtualSnap.GetSafeNormal() * 0.1;
				if (ComponentTraceSingle(hitVault, pt, chkSurfaceDirection.GetSafeNormal() * conditions.DepthRange.Y, status.Kinematics.AngularKinematic.Orientation, 0))
					continue;
				response.VaultDepthVector = FVector::VectorPlaneProject(hitBackWall.ImpactPoint - surface.SurfacePoint, chkSurfaceDirection);
			}
		}

		//Speed test
		const FVector speedVector = status.Kinematics.LinearKinematic.Velocity.ProjectOnToNormal(chkSurfaceDirection);
		const float directionalSpeedScale = speedVector.Length() * ((speedVector | chkSurfaceDirection) > 0 ? 1 : -1);
		if (!UToolsLibrary::CheckInRange(conditions.SpeedRange, directionalSpeedScale, true))
			continue;

		//Orientation Speed test
		const FVector orientationSpeedVector = status.Kinematics.LinearKinematic.Velocity.ProjectOnToNormal(farAwayVector.GetSafeNormal());
		const float orientationDirectionalSpeedScale = orientationSpeedVector.Length() * ((orientationSpeedVector | farAwayVector.GetSafeNormal()) > 0 ? 1 : -1);
		if (!UToolsLibrary::CheckInRange(conditions.OrientationSpeedRange, orientationDirectionalSpeedScale, true))
			continue;

		//Surface speed Test
		const FVector surfaceSpeedVector = surface.GetVelocityAt(surface.SurfacePoint).ProjectOnToNormal(chkSurfaceDirection);
		const float dirSurfaceSpeedScale = surfaceSpeedVector.Length() * ((surfaceSpeedVector | chkSurfaceDirection) > 0 ? 1 : -1);
		if (!UToolsLibrary::CheckInRange(conditions.SurfaceSpeedRange, dirSurfaceSpeedScale, true))
			continue;

		//Cosmetic vars test
		bool breakFalse = false;
		for (auto entry : conditions.CosmeticVarRanges)
		{
			if (!status.StatusParams.StatusCosmeticVariables.Contains(entry.Key))
				continue;
			if (!UToolsLibrary::CheckInRange(entry.Value, status.StatusParams.StatusCosmeticVariables[entry.Key], true))
			{
				breakFalse = true;
				break;
			}
		}

		if (breakFalse)
			continue;

		response.Surface = surface;
		foundTrue = true;
	}

	return foundTrue;
}


#pragma endregion

#pragma region Debug

#pragma endregion
