// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/BaseTraversalWatcher.h"
#include "FunctionLibrary.h"


bool UBaseTraversalWatcher::CheckWatcher(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta,
                                         TMap<FName, TArray<bool>>* TraversalDebugMap) const
{
	if (TraversalDebugMap)
		TraversalDebugMap->Empty();
	bool atLeastOneValid = false;
	const FVector feetPos = controller->GetWorldSpaceCardinalPoint(startingConditions.Kinematics.GetGravityDirection());
	const FVector locationOffset = (feetPos - startingConditions.Kinematics.LinearKinematic.Position);
	for (auto item : TraversalMap)
	{
		FSurfaceCheckResponse response;
		if (TraversalDebugMap)
			TraversalDebugMap->Add(item.Key, TArray<bool>());

		if (controller->EvaluateSurfaceConditionsInternal(item.Value, response, startingConditions, locationOffset, FVector(0), FVector(0), FVector(0),
		                                                  TraversalDebugMap ? &(*TraversalDebugMap)[item.Key] : nullptr))
		{
			atLeastOneValid = true;
			TriggerTraversalEvent(controller, startingConditions, FName(FString::Printf(TEXT("%s_%s"), *GetDescriptionName().ToString(), *item.Key.ToString())), item.Value, response);
			if (!bMultiTraversalTrigger)
				break;
		}
	}
	return atLeastOneValid;
}


void UBaseTraversalWatcher::TriggerTraversalEvent_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FName combinedKey,
                                                                 FSurfaceCheckParams TraversalParam, FSurfaceCheckResponse response) const
{
	if (controller)
	{
		TArray<FTransform> ptsList;
		ptsList.Empty();
		if (TraversalParam.DepthRange.Z > 0)
		{
			FVector pos = startingConditions.Kinematics.LinearKinematic.Position;
			FVector normal = startingConditions.Kinematics.GetGravityDirection();
			const FVector snapVector = UFunctionLibrary::GetSnapOnSurfaceVector(pos + response.LocationOffset, response.Surface, normal);
			const FVector ledgeLocation = response.Surface.SurfacePoint + snapVector - snapVector.GetSafeNormal() * 2 * OVERLAP_INFLATION;
			const FQuat lookDir = FVector::VectorPlaneProject(-response.Surface.SurfaceNormal, normal).ToOrientationQuat();
			ptsList.Add(FTransform(lookDir, ledgeLocation));
			if (!response.VaultDepthVector.ContainsNaN())
				ptsList.Add(FTransform(lookDir, ledgeLocation + response.VaultDepthVector));
		}
		FString leftTerm, rightTerm;
		if(combinedKey.ToString().Split("_", &leftTerm, &rightTerm))
		{
			controller->OnControllerTriggerPathEvent.Broadcast(FName(rightTerm), ptsList);
		}
	}
}

FString UBaseTraversalWatcher::DebugString()
{
	return GetDescriptionName().ToString();
}
