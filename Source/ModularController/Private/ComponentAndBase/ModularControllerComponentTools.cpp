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


#pragma region Tools & Utils

bool UModularControllerComponent::ComponentTraceCastSingleUntil(FHitResult& outHit, FVector direction, FVector position,
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
		if (ComponentTraceCastSingle_Internal(iterationHit, position, direction, rotation, inflation, traceComplex, queryParams))
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


bool UModularControllerComponent::ComponentTraceCastMulti_internal(TArray<FHitResult>& outHits, FVector position, FVector direction, FQuat rotation, double inflation, bool traceComplex,
                                                                   FCollisionQueryParams& queryParams) const
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

	constexpr int maxIterations = 64;
	TArray<FHitResult> loopHits;
	FCollisionQueryParams loopQueryParams = queryParams;
	for (int i = 0; i < maxIterations; i++)
	{
		const bool result = GetWorld()->SweepMultiByChannel(loopHits, position, position + direction, rotation, channel, shape, loopQueryParams);
		if (!result)
			break;
		for (int j = 0; j < loopHits.Num(); j++)
		{
			outHits.Add(loopHits[j]);
			loopQueryParams.AddIgnoredComponent(loopHits[j].GetComponent());
		}
	}
	queryParams.ClearIgnoredActors();

	return outHits.Num() > 0;
}


bool UModularControllerComponent::ComponentTraceCastSingle_Internal(FHitResult& outHit, FVector position, FVector direction, FQuat rotation, double inflation, bool traceComplex,
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


void UModularControllerComponent::PathCastComponent_Internal(TArray<FHitResult>& results, FVector start, TArray<FVector> pathPoints, bool stopOnHit, float skinWeight, bool debugRay,
                                                             bool rotateAlongPath, bool bendOnCollision, bool traceComplex, FCollisionQueryParams& queryParams)
{
	if (pathPoints.Num() <= 0)
		return;


	auto owner = GetOwner();
	if (owner == nullptr)
		return;

	results.Empty();
	queryParams.AddIgnoredActor(owner);
	queryParams.bTraceComplex = traceComplex;
	queryParams.bReturnPhysicalMaterial = true;


	UPrimitiveComponent* primitive = UpdatedPrimitive;
	if (!primitive)
	{
		queryParams.ClearIgnoredActors();
		return;
	}
	auto shape = primitive->GetCollisionShape(skinWeight);

	for (int i = 0; i < pathPoints.Num(); i++)
	{
		FHitResult soloHit;
		FVector in = i <= 0 ? start : pathPoints[i - 1];
		FVector out = pathPoints[i];
		GetWorld()->SweepSingleByChannel(soloHit, in, out, rotateAlongPath ? (out - in).Rotation().Quaternion() : GetRotation()
		                                 , primitive->GetCollisionObjectType(), shape, queryParams, FCollisionResponseParams::DefaultResponseParam);
		if (debugRay)
		{
			UKismetSystemLibrary::DrawDebugArrow(this, in, out, 15, soloHit.Component != nullptr ? FColor::Green : FColor::Silver, 0, 15);
			if (soloHit.Component != nullptr)
			{
				UKismetSystemLibrary::DrawDebugPoint(this, soloHit.ImpactPoint, 30, FColor::Green, 0);
				UKismetSystemLibrary::DrawDebugArrow(this, soloHit.ImpactPoint, soloHit.ImpactPoint + soloHit.ImpactNormal, 15, FColor::Red, 0, 15);
				UKismetSystemLibrary::DrawDebugArrow(this, soloHit.ImpactPoint, soloHit.ImpactPoint + soloHit.Normal, 15, FColor::Orange, 0, 15);
			}
		}
		results.Add(soloHit);
		if (stopOnHit && soloHit.IsValidBlockingHit())
		{
			break;
		}

		if (bendOnCollision && soloHit.IsValidBlockingHit())
		{
			FVector offset = soloHit.Location - out;
			for (int j = i; j < pathPoints.Num(); j++)
			{
				pathPoints[j] += offset + offset.GetSafeNormal();
			}
		}
	}

	queryParams.ClearIgnoredActors();
}


void UModularControllerComponent::PathCastLine(TArray<FHitResult>& results, FVector start, TArray<FVector> pathPoints, ECollisionChannel channel, bool stopOnHit, bool debugRay,
                                               bool bendOnCollision, bool traceComplex)
{
	if (pathPoints.Num() <= 0)
		return;


	auto owner = GetOwner();
	if (owner == nullptr)
		return;

	results.Empty();
	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(owner);
	queryParams.bTraceComplex = traceComplex;
	queryParams.bReturnPhysicalMaterial = true;

	for (int i = 0; i < pathPoints.Num(); i++)
	{
		FHitResult soloHit;
		FVector in = i <= 0 ? start : pathPoints[i - 1];
		FVector out = pathPoints[i];
		GetWorld()->LineTraceSingleByChannel(soloHit, in, out, channel, queryParams, FCollisionResponseParams::DefaultResponseParam);
		if (debugRay)
		{
			UKismetSystemLibrary::DrawDebugArrow(this, in, out, 15, soloHit.Component != nullptr ? FColor::Green : FColor::Silver, 0, 15);
			if (soloHit.Component != nullptr)
			{
				UKismetSystemLibrary::DrawDebugPoint(this, soloHit.ImpactPoint, 30, FColor::Green, 0);
				UKismetSystemLibrary::DrawDebugArrow(this, soloHit.ImpactPoint, soloHit.ImpactPoint + soloHit.ImpactNormal, 15, FColor::Red, 0, 15);
			}
		}
		results.Add(soloHit);
		if (stopOnHit && soloHit.IsValidBlockingHit())
		{
			break;
		}

		if (bendOnCollision && soloHit.IsValidBlockingHit())
		{
			FVector offset = soloHit.Location - out;
			for (int j = i; j < pathPoints.Num(); j++)
			{
				pathPoints[j] += offset + offset.GetSafeNormal();
			}
		}
	}
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

				if (DebugType == ControllerDebugType_MovementDebug)
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
					if (DebugType == ControllerDebugType_MovementDebug)
					{
						if (overlap.GetActor())
						{
							UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Depentrate Actor: (%s)"), *overlap.GetActor()->GetActorNameOrLabel()), true, true, FColor::Silver,
							                                  0, FName(FString::Printf(TEXT("OverlapPenetration_%s"), *overlap.GetActor()->GetActorNameOrLabel())));
						}
					}

					const FVector depForce = depenetrationInfos.Direction * depenetrationInfos.Distance;
					FVector hullPt = PointOnShape(-depenetrationInfos.Direction, atPosition);

					if (DebugType == ControllerDebugType_MovementDebug)
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

						if (showDebug && DebugType == ControllerDebugType_MovementDebug)
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


#pragma endregion

#pragma region Debug

#pragma endregion
