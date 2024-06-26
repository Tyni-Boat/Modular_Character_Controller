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




#pragma region Physic


void UModularControllerComponent::BeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	////overlap objects
	if (OverlappedComponent != nullptr && OtherComp != nullptr && OtherActor != nullptr)
	{
		if (DebugType == ControllerDebugType_PhysicDebug)
		{
			GEngine->AddOnScreenDebugMessage((int32) GetOwner()->GetUniqueID() + 9, 1, FColor::Green, FString::Printf(TEXT("Overlaped With: %s"), *OtherActor->GetActorNameOrLabel()));
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Overlap with: (%s)"), *OtherActor->GetActorNameOrLabel()), true, true, FColor::Green, 0, "OverlapEvent");
		}
	}
}


void UModularControllerComponent::OverlapSolver(int& maxDepth, float DeltaTime, TArray<TWeakObjectPtr<UPrimitiveComponent>>* touchedComponents) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("OverlapSolver");
	if (touchedComponents)
		touchedComponents->Empty();
	if (!UpdatedPrimitive || !GetWorld())
		return;
	const auto world = GetWorld();
	const FVector location = GetLocation();
	const FQuat rotation = UpdatedPrimitive->GetComponentQuat();
	TArray<FOverlapResult> _overlaps;
	FComponentQueryParams comQueryParams;
	if (UpdatedPrimitive->GetOwner())
		comQueryParams.AddIgnoredActor(UpdatedPrimitive->GetOwner());
	const auto shapeExtended = UpdatedPrimitive->GetCollisionShape(1); //1 cm inflated hull
	const auto shape = UpdatedPrimitive->GetCollisionShape(0);
	const auto channel = UpdatedPrimitive->GetCollisionObjectType();
	if (world->OverlapMultiByChannel(_overlaps, location, rotation, channel, shapeExtended, comQueryParams))
	{
		FMTDResult penetrationInfos;
		FVector displacement = FVector(0);
		for (int i = 0; i < _overlaps.Num(); i++)
		{
			const auto& overlap = _overlaps[i];
			if (touchedComponents)
				touchedComponents->Add(overlap.Component);
			if (!overlap.bBlockingHit)
				continue;
			if (overlap.Component.IsValid() && overlap.Component->ComputePenetration(penetrationInfos, shape, location, rotation))
			{
				comQueryParams.AddIgnoredComponent(overlap.GetComponent());
				const FVector depForce = penetrationInfos.Direction * penetrationInfos.Distance;

				//Handle physic objects collision
				if (overlap.Component->IsSimulatingPhysics())
				{
					overlap.Component->AddForce(-depForce * GetMass() / DeltaTime);
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


void UModularControllerComponent::HandleTrackedSurface(float delta)
{
	if (ApplyedControllerStatus.SurfacesInContact.Num() > 0)
	{
		//Remove obsolete
		for (int i = ApplyedControllerStatus.SurfacesInContact.Num() - 1; i >= 0; i--)
		{
			if (!ApplyedControllerStatus.SurfacesInContact[i].UpdateTracking(delta))
				ApplyedControllerStatus.SurfacesInContact.RemoveAt(i);
		}
		//Add news
		for (int j = 0; j < _contactComponents.Num(); j++)
		{
			const auto contact = _contactComponents[j];
			const int indexOf = ApplyedControllerStatus.SurfacesInContact.IndexOfByPredicate([contact](const FSurfaceTrackData& item) ->bool { return item.TrackedComponent == contact; });
			if (indexOf != INDEX_NONE)
				continue;
			FSurfaceTrackData surfaceTrack;
			surfaceTrack.TrackedComponent = contact;
			ApplyedControllerStatus.SurfacesInContact.Add(surfaceTrack);
		}
	}
	else
	{
		for (const auto component : _contactComponents)
		{
			FSurfaceTrackData surfaceTrack;
			surfaceTrack.TrackedComponent = component;
			ApplyedControllerStatus.SurfacesInContact.Add(surfaceTrack);
		}
	}
}


FSurfaceInfos UModularControllerComponent::GetCurrentSurface() const
{
	return  ComputedControllerStatus.ControllerSurface;
}


void UModularControllerComponent::AddForce(const FHitResult hit, FVector momentum)
{
}


#pragma endregion

