// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/BaseTraversalWatcher.h"

#include <Async/Async.h>

#include "FunctionLibrary.h"


bool UBaseTraversalWatcher::CheckWatcher(TQueue<FTraversalCommandParams>& eventsCommands, const UModularControllerComponent* controller, const FControllerStatus startingConditions,
                                         const float delta, TMap<FName, TArray<bool>>* TraversalDebugMap) const
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
		TArray<FHitResultExpanded> tmpSolverHits;
		if (controller->EvaluateSurfaceConditionsInternal(tmpSolverHits, item.Value, response, startingConditions, locationOffset, FVector(0), FVector(0), FVector(0),
		                                                  TraversalDebugMap ? &(*TraversalDebugMap)[item.Key] : nullptr))
		{
			atLeastOneValid = true;
			FTraversalCommandParams eventParams = ComputeTraversalPath(startingConditions, FName(FString::Printf(TEXT("%s_%s"), *GetDescriptionName().ToString(), *item.Key.ToString())),
			                                                           item.Value, response);
			if (!eventParams.ParamKey.IsNone())
				eventsCommands.Enqueue(eventParams);
			if (!bMultiTraversalTrigger)
				break;
		}
	}
	return atLeastOneValid;
}


FTraversalCommandParams UBaseTraversalWatcher::ComputeTraversalPath_Implementation(const FControllerStatus startingConditions, FName combinedKey,
                                                                                   FSurfaceCheckParams TraversalParam, FSurfaceCheckResponse& response) const
{
	FTraversalCommandParams ret;
	ret.PathPoints.Empty();
	if (!response.HitPlanedNormal.ContainsNaN())
	{
		const FQuat lookDir = (-response.HitPlanedNormal).ToOrientationQuat();
		if (!response.HurdleStartLocation.ContainsNaN())
			ret.PathPoints.Add(FTransform(lookDir, response.HurdleStartLocation));
		if (!response.HurdleApexLocation.ContainsNaN())
			ret.PathPoints.Add(FTransform(lookDir, response.HurdleApexLocation));
		if (!response.HurdleApexDepthLocation.ContainsNaN())
			ret.PathPoints.Add(FTransform(lookDir, response.HurdleApexDepthLocation));
		if (!response.HurdleLandLocation.ContainsNaN())
			ret.PathPoints.Add(FTransform(lookDir, response.HurdleLandLocation));
	}

	FString leftTerm, rightTerm;
	if (combinedKey.ToString().Split("_", &leftTerm, &rightTerm))
	{
		ret.ParamKey = FName(rightTerm);
	}
	return ret;
}

FString UBaseTraversalWatcher::DebugString()
{
	return GetDescriptionName().ToString();
}
