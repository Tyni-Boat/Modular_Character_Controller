// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#include "ComponentAndBase/ModularControllerComponent.h"

#include <functional>
#include "Kismet/KismetMathLibrary.h"
#include "Engine.h"
#include "FunctionLibrary.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "ToolsLibrary.h"
#include "Sampling/SphericalFibonacci.h"
#include "VectorTypes.h"


#pragma region Physic


void UModularControllerComponent::BeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                               const FHitResult& SweepResult)
{
	////overlap objects
	if (OverlappedComponent != nullptr && OtherComp != nullptr && OtherActor != nullptr)
	{
		if (DebugType == EControllerDebugType::PhysicDebug)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Overlap with: (%s)"), *OtherActor->GetActorNameOrLabel()), true, false, FColor::Green, 0, "OverlapEvent");
		}
	}
}


void UModularControllerComponent::TrackShapeChanges()
{
	if (!UpdatedPrimitive)
		return;
	const auto currentShape = UpdatedPrimitive->GetCollisionShape();
	if (!UFunctionLibrary::CollisionShapeEquals(_shapeDatas, currentShape))
	{
		_shapeDatas = currentShape;
		EvaluateCardinalPoints();
		if (DebugType == EControllerDebugType::PhysicDebug)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Updated Shape On %s"), *GetName()), true, true, FColor::Red, 2, "ShapeChanged");
		}
	}
}


void UModularControllerComponent::OverlapSolver(int& maxDepth, float DeltaTime, TArray<FHitResultExpanded>* touchedHits, const FVector4 scanParameters)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("OverlapSolver");
	_tempOverlapSolverHits.Empty();
	if (touchedHits)
		touchedHits->Empty();
	if (!UpdatedPrimitive || !GetWorld())
		return;
	const auto world = GetWorld();
	constexpr float detectionInflation = OVERLAP_INFLATION;
	const FVector location = GetLocation();
	const FQuat rotation = UpdatedPrimitive->GetComponentQuat();
	FComponentQueryParams comQueryParams;
	if (UpdatedPrimitive->GetOwner())
		comQueryParams.AddIgnoredActor(UpdatedPrimitive->GetOwner());
	const auto shape = UpdatedPrimitive->GetCollisionShape(0);
	const auto channel = UpdatedPrimitive->GetCollisionObjectType();
	const FVector scanDirection = FVector(scanParameters.X, scanParameters.Y, scanParameters.Z);
	const float scanMaxOffset = scanParameters.W;
	const FVector cardinalPoint = GetWorldSpaceCardinalPoint(-scanDirection);
	FVector offset = scanDirection.GetClampedToMaxSize((cardinalPoint - location).Length() * 2) + scanDirection.GetSafeNormal() * detectionInflation;
	if (offset.SquaredLength() > 0)
	{
		FHitResult hit;
		// FCollisionQueryParams queryParams = FCollisionQueryParams::DefaultQueryParam;
		// FCollisionResponseParams response = FCollisionResponseParams::DefaultResponseParam;
		// response.CollisionResponse.SetAllChannels(ECR_Block);
		// queryParams.AddIgnoredActor(GetOwner());
		// queryParams.bTraceComplex = bUseComplexCollision;
		// queryParams.bReturnPhysicalMaterial = true;
		// const auto newShape = UpdatedPrimitive->GetCollisionShape(-2);
		//
		// if (GetWorld()->SweepSingleByChannel(hit, location, location - offset, rotation, channel, newShape, queryParams, response))
		// {
		// 	if (DebugType != EControllerDebugType::None)
		// 	{
		// 		UFunctionLibrary::DrawDebugCircleOnHit(hit, false, 45, hit.bStartPenetrating ? FColor::Yellow : FColor::Magenta, DeltaTime * 1.2, 0.5);
		// 	}
		// 	FVector vector = UFunctionLibrary::GetSnapOnSurfaceVector(cardinalPoint, FSurface(hit), scanDirection.GetSafeNormal());
		// 	const FVector offsetDir = -vector;
		// 	const FVector counterDir = vector.GetSafeNormal() * (5 + detectionInflation + 0.125);
		// 	offset = offsetDir + counterDir.GetClampedToMaxSize(offsetDir.Length());
		// }
		// else
		// {
		if (DebugType != EControllerDebugType::None)
		{
			hit.Normal = offset.GetSafeNormal();
			hit.ImpactNormal = offset.GetSafeNormal();
			hit.ImpactPoint = location - offset;
			hit.Component = UpdatedPrimitive;
			UFunctionLibrary::DrawDebugCircleOnHit(hit, false, FColor::White, DeltaTime * 1.2, 0.5);
		}
		// }
	}
	if (ComponentTraceCastMulti_internal(_tempOverlapSolverHits, location - offset, scanDirection + offset, rotation, detectionInflation, bUseComplexCollision,
	                                     FCollisionQueryParams::DefaultQueryParam, scanMaxOffset))
	{
		FMTDResult penetrationInfos;
		FVector displacement = FVector(0);
		for (int i = 0; i < _tempOverlapSolverHits.Num(); i++)
		{
			auto& overlapHit = _tempOverlapSolverHits[i];

			const bool isBlocking = overlapHit.QueryResponse == ECollisionResponse::ECR_Block;
			if (touchedHits)
				touchedHits->Add(overlapHit);

			if (!isBlocking || IsIgnoringCollision())
				continue;
			if (overlapHit.HitResult.Component.IsValid() && overlapHit.HitResult.Component->ComputePenetration(penetrationInfos, shape, location, rotation))
			{
				comQueryParams.AddIgnoredComponent(overlapHit.HitResult.GetComponent());
				const FVector depForce = penetrationInfos.Direction * penetrationInfos.Distance;

				//Handle physic objects collision
				if (overlapHit.HitResult.Component->IsSimulatingPhysics())
				{
					overlapHit.HitResult.Component->AddForce(-depForce * GetMass() / DeltaTime);
				}

				displacement += depForce;
			}
		}

		if (displacement.IsZero())
			return;

		//Try to go to that location
		FHitResult hit = FHitResult(NoInit);
		if (world->SweepSingleByChannel(hit, location, location + displacement, rotation, channel, shape, comQueryParams))
		{
			UModularControllerComponent* modularComp = hit.GetActor()->GetComponentByClass<UModularControllerComponent>();
			if (modularComp && modularComp->UpdatedPrimitive == hit.Component)
			{
				UpdatedPrimitive->SetWorldLocation(location + displacement);
				maxDepth--;
				if (maxDepth >= 0)
					modularComp->OverlapSolver(maxDepth, DeltaTime);
			}
			else
			{
				if (hit.Component->IsSimulatingPhysics())
					UpdatedPrimitive->SetWorldLocation(hit.Location + displacement.GetClampedToMaxSize(0.125));
				else
					UpdatedPrimitive->SetWorldLocation(hit.Location);
			}
		}
		else
		{
			UpdatedPrimitive->SetWorldLocation(location + displacement);
		}
	}
}


void UModularControllerComponent::HandleTrackedSurface(FControllerStatus& fromStatus, float delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("HandleTrackedSurface");
	if (fromStatus.Kinematics.SurfacesInContact.Num() > 0)
	{
		TArray<bool> debugFlagArray;
		if (DebugType != EControllerDebugType::None)
			debugFlagArray = UToolsLibrary::FlagToBoolArray(fromStatus.Kinematics.SurfaceBinaryFlag);

		//Remove obsolete
		for (int i = fromStatus.Kinematics.SurfacesInContact.Num() - 1; i >= 0; i--)
		{
			if (DebugType != EControllerDebugType::None)
			{
				const ECollisionResponse response = static_cast<ECollisionResponse>(fromStatus.Kinematics.SurfacesInContact[i].SurfacePhysicProperties.Z);
				FColor debugCol = FColor::Silver;
				switch (response)
				{
					case ECR_Block: debugCol = fromStatus.Kinematics.SurfacesInContact[i].SurfacePhysicProperties.W > 0 ? FColor::Orange : FColor::Red;
						break;
					case ECR_Overlap: debugCol = fromStatus.Kinematics.SurfacesInContact[i].SurfacePhysicProperties.W > 0 ? FColor::Emerald : FColor::Green;
						break;
					default: break;
				}
				if (debugFlagArray.IsValidIndex(i) && debugFlagArray[i])
					debugCol = FColor::Cyan;
				UFunctionLibrary::DrawDebugCircleOnSurface(fromStatus.Kinematics.SurfacesInContact[i], 15, debugCol, delta * 1.1, 1, true);
			}

			if (!fromStatus.Kinematics.SurfacesInContact[i].UpdateTracking(delta))
			{
				fromStatus.Kinematics.SurfacesInContact.RemoveAt(i);
				continue;
			}
			const auto surface = fromStatus.Kinematics.SurfacesInContact[i];
			const int indexOf = _contactHits.IndexOfByPredicate([surface](FHitResultExpanded innerHit)-> bool
			{
				return innerHit.HitResult.Component == surface.TrackedComponent && innerHit.HitIndex == surface.TrackedComponentIndex;
			});
			if (indexOf == INDEX_NONE)
				fromStatus.Kinematics.SurfacesInContact.RemoveAt(i);
		}

		//Add news and update currents
		for (int j = 0; j < _contactHits.Num(); j++)
		{
			const auto hit = _contactHits[j];
			bool canStepOn = true;
			const int indexOf = fromStatus.Kinematics.SurfacesInContact.IndexOfByPredicate([hit](const FSurface& item) -> bool
			{
				return item.TrackedComponent == hit.HitResult.Component && item.TrackedComponentIndex == hit.HitIndex;
			});
			if (indexOf != INDEX_NONE)
			{
				canStepOn = fromStatus.Kinematics.SurfacesInContact[indexOf].SurfacePhysicProperties.W > 0;
				fromStatus.Kinematics.SurfacesInContact[indexOf].UpdateHit(hit, canStepOn);
				continue;
			}

			const bool validPawn = hit.HitResult.Component.IsValid();
			canStepOn = validPawn ? hit.HitResult.Component->CanCharacterStepUpOn == ECB_Owner || hit.HitResult.Component->CanCharacterStepUpOn == ECB_Yes : true;
			fromStatus.Kinematics.SurfacesInContact.Add(FSurface(hit, canStepOn));
		}
	}
	else
	{
		for (int i = 0; i < _contactHits.Num(); i++)
		{
			const auto hit = _contactHits[i];
			const bool validPawn = hit.HitResult.Component.IsValid();
			const bool canStepOn = validPawn ? hit.HitResult.Component->CanCharacterStepUpOn == ECB_Owner || hit.HitResult.Component->CanCharacterStepUpOn == ECB_Yes : true;
			fromStatus.Kinematics.SurfacesInContact.Add(FSurface(hit, canStepOn));
		}
	}
}


bool UModularControllerComponent::IsIgnoringCollision() const
{
	bool ignore = bDisableCollision;
	if (!ignore)
	{
		if (const auto curAction = GetCurrentControllerAction())
		{
			const auto curActionInfos = GetCurrentControllerActionInfos();
			if (curAction->NoCollisionPhases.Num() > 0)
				ignore = curAction->NoCollisionPhases.Contains(curActionInfos.CurrentPhase);
		}

		if (!ignore && !_noCollisionOverrideRootMotionCommands.IsEmpty())
		{
			ignore = true;
		}
	}
	return ignore;
}

void UModularControllerComponent::AddForce(const FVector force)
{
	_externalForces += force;
}


void UModularControllerComponent::EvaluateCardinalPoints()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("EvaluateCardinalPoints");
	_localSpaceCardinalPoints.Empty();
	if (!GetOwner() || !UpdatedPrimitive)
		return;
	const FTransform Transform = GetOwner()->GetActorTransform();
	const auto bounds = UpdatedPrimitive->Bounds;
	const FVector bCenter = Transform.InverseTransformPosition(bounds.Origin);
	const float bRadius = bounds.SphereRadius;

	//Add first 6 cardinal points
	{
		const FVector fwd = bCenter + (FVector(1, 0, 0) - bCenter).GetSafeNormal() * bRadius * 1.33;
		const FVector bck = bCenter + (FVector(-1, 0, 0) - bCenter).GetSafeNormal() * bRadius * 1.33;
		const FVector dwn = bCenter + (FVector(0, 0, -1) - bCenter).GetSafeNormal() * bRadius * 1.33;
		const FVector up = bCenter + (FVector(0, 0, 1) - bCenter).GetSafeNormal() * bRadius * 1.33;
		const FVector rht = bCenter + (FVector(0, 1, 0) - bCenter).GetSafeNormal() * bRadius * 1.33;
		const FVector lft = bCenter + (FVector(0, -1, 0) - bCenter).GetSafeNormal() * bRadius * 1.33;

		_localSpaceCardinalPoints.Add(fwd);
		_localSpaceCardinalPoints.Add(bck);
		_localSpaceCardinalPoints.Add(dwn);
		_localSpaceCardinalPoints.Add(up);
		_localSpaceCardinalPoints.Add(rht);
		_localSpaceCardinalPoints.Add(lft);
	}

	const auto fibonacciSphere = UE::Geometry::TSphericalFibonacci<float>(CardinalPointsNumber);
	for (int i = 0; i < fibonacciSphere.Num(); i++)
	{
		const auto p = fibonacciSphere.Point(i);
		const FVector point = bCenter + (FVector(p.X, p.Y, p.Z) - bCenter).GetSafeNormal() * bRadius * 1.33;
		_localSpaceCardinalPoints.Add(point);
	}

	// Trace on shape
	for (int i = _localSpaceCardinalPoints.Num() - 1; i >= 0; i--)
	{
		FVector worldPoint = Transform.TransformPosition(_localSpaceCardinalPoints[i]);
		//Trace
		FHitResult hit;
		const auto channel = UpdatedPrimitive->GetCollisionObjectType();
		FCollisionResponseParams response = FCollisionResponseParams::DefaultResponseParam;
		response.CollisionResponse.SetAllChannels(ECollisionResponse::ECR_Block);
		bool touched = UpdatedPrimitive->LineTraceComponent(hit, worldPoint, Transform.TransformPosition(bCenter), channel, FCollisionQueryParams::DefaultQueryParam, response,
		                                                    FCollisionObjectQueryParams::DefaultObjectQueryParam);
		if (!touched)
		{
			_localSpaceCardinalPoints.RemoveAt(i);
			continue;
		}
		worldPoint = hit.ImpactPoint;
		_localSpaceCardinalPoints[i] = Transform.InverseTransformPosition(worldPoint);
	}
}


FVector UModularControllerComponent::GetWorldSpaceCardinalPoint(const FVector worldSpaceDirection) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("GetWorldSpaceCardinalPoint");
	if (!GetOwner() || _localSpaceCardinalPoints.Num() <= 0)
		return FVector(NAN);
	const FTransform Transform = GetOwner()->GetActorTransform();
	FVector wDir = worldSpaceDirection;
	if (!wDir.Normalize())
		return GetLocation();
	const FVector localDir = Transform.InverseTransformVector(wDir);
	int selectedIndex = -1;
	float maxDotProduct = TNumericLimits<float>::Lowest();
	for (int i = 0; i < _localSpaceCardinalPoints.Num(); i++)
	{
		const FVector ptDir = (_localSpaceCardinalPoints[i] - FVector(0)).GetSafeNormal();
		const float dot = ptDir | localDir;
		if (dot <= maxDotProduct)
			continue;
		maxDotProduct = dot;
		selectedIndex = i;
	}
	if (!_localSpaceCardinalPoints.IsValidIndex(selectedIndex))
		return GetLocation();
	return Transform.TransformPosition(_localSpaceCardinalPoints[selectedIndex]);
}


#pragma endregion
