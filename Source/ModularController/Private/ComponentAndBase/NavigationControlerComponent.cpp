// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/NavigationControlerComponent.h"

#include <string>
#include <Kismet/KismetSystemLibrary.h>
#include "NavigationPath.h"
#include "Kismet/KismetMathLibrary.h"
#include "AIController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NavigationSystem.h"
#include "ComponentAndBase/ModularControllerComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavMesh/NavMeshPath.h"
#include "Engine.h"
#include "Net/UnrealNetwork.h"
#include "NavLinkCustomInterface.h"
#include "ToolsLibrary.h"
#include "Engine/ActorChannel.h"
#include "NavFilters/NavigationQueryFilter.h"


#pragma region Network path type XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

UNetPathPoint::UNetPathPoint()
{
}

UNetPathPoint::UNetPathPoint(FVector location, int index, INavLinkCustomInterface* navLinkInterface)
{
	Location = location;
	PointIndex = index;
	if (navLinkInterface)
	{
		UObject* NewNavLinkOb = Cast<UObject>(navLinkInterface);
		NavLinkInterface = NewNavLinkOb;
	}
}

bool UNetPathPoint::IsSupportedForNetworking() const
{
	return true;
}

void UNetPathPoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNetPathPoint, NavLinkInterface);
	DOREPLIFETIME(UNetPathPoint, PointIndex);
	DOREPLIFETIME(UNetPathPoint, Location);
}

INavLinkCustomInterface* UNetPathPoint::GetNavLinkInterface() const
{
	if (NavLinkInterface)
		return Cast<INavLinkCustomInterface>(NavLinkInterface);
	return nullptr;
}

#pragma endregion


#pragma region Core XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


// Sets default values for this component's properties
UNavigationControlerComponent::UNavigationControlerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	// ...
}


// Called when the game starts
void UNavigationControlerComponent::BeginPlay()
{
	Super::BeginPlay();

	SetBlockDetectionState(true);
	if (const auto modController = GetOwner()->GetComponentByClass<UModularControllerComponent>())
	{
		SetMovementComponent(modController);
	}
}


// Called every frame
void UNavigationControlerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateStartPath();
	FollowPath(DeltaTime);
}


#pragma endregion

#pragma region Search XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


int UNavigationControlerComponent::SearchPath(AActor* target, FVector location, float maxOffNavDistance, TSubclassOf<UNavigationQueryFilter> filter)
{
	if (!GetOwner())
	{
		if (IsDebug)
		{
			FName compName = FName(this->GetReadableName());
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[PathFinding] - Imposible to search for a path: NULL ACTOR")), true, true, FColor::Red, 2, compName);
		}
		return - 1;
	}

	FNavLocation navStartPt;
	FVector initialLocation = GetCurrentNavLocation().Location;
	FVector moveLocation = target ? target->GetActorLocation() : location;
	const auto navSys = UNavigationSystemV1::GetCurrent(GetWorld());
	const auto navData = navSys->GetDefaultNavDataInstance();
	bool validStartLocation = false;

	//Find the start navigation point
	if (navData->ProjectPoint(initialLocation, navStartPt, FVector(1, 1, maxOffNavDistance)))
	{
		initialLocation = navStartPt.Location;
		validStartLocation = true;
	}

	//Check Start
	if (!validStartLocation)
	{
		if (IsDebug)
		{
			FName compName = FName(this->GetReadableName());
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[PathFinding] - Unable to start searching for a path: INVALID START LOCATION")), true, true, FColor::Red, 2,
			                                  compName);
		}
		return - 1;
	}

	FNavLocation navEndPt;
	bool validEndLocation = false;

	//Find the closest point on navigation
	for (float i = 0; i < maxOffNavDistance; i += maxOffNavDistance * 0.1)
	{
		if (navData->ProjectPoint(moveLocation, navEndPt, FVector::OneVector * i))
		{
			moveLocation = navEndPt.Location;
			validEndLocation = true;
			break;
		}
	}

	//Check destination
	if (!validEndLocation)
	{
		if (IsDebug)
		{
			FName compName = FName(this->GetReadableName());
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[PathFinding] - Unable to start searching for a path: INVALID END LOCATION")), true, true, FColor::Red, 2,
			                                  compName);
		}
		return -1;
	}

	//Search path
	MaxPointProjection = maxOffNavDistance;
	FNavAgentProperties agentProps = FNavAgentProperties(AgentRadius, AgentHeight);
	FPathFindingQuery pathFindingQuery;
	pathFindingQuery.SetAllowPartialPaths(true);
	pathFindingQuery.SetNavAgentProperties(agentProps);
	pathFindingQuery.SetPathInstanceToUpdate(Path);
	pathFindingQuery.SetRequireNavigableEndLocation(false);
	pathFindingQuery.EndLocation = moveLocation;
	pathFindingQuery.StartLocation = initialLocation;
	pathFindingQuery.Owner = GetOwner();
	auto navFilter = UNavigationQueryFilter::StaticClass();
	if (filter) navFilter = filter;
	pathFindingQuery.QueryFilter = UNavigationQueryFilter::GetQueryFilter<UNavigationQueryFilter>(*navData, navFilter);
	pathFindingQuery.NavData = navData;
	FNavPathQueryDelegate PathQueryDelegate;
	PathQueryDelegate.BindUObject(this, &UNavigationControlerComponent::OnAsyncPathEvaluated);

	auto asyncReqID = navSys->FindPathAsync(agentProps, pathFindingQuery, PathQueryDelegate, EPathFindingMode::Regular);
	_asyncPathRequestQueue.Enqueue(TTuple<uint32, TSoftObjectPtr<AActor>>(asyncReqID, target));
	return asyncReqID;
}


void UNavigationControlerComponent::CancelPath()
{
	if (GetOwner())
	{
		AbortMove(*GetOwner(), FPathFollowingResultFlags::MovementStop);
		_asyncPathRequestQueue.Empty();
	}
}


void UNavigationControlerComponent::OnPathEnds(FAIRequestID requestID, uint32 result)
{
	EPathFollowingResult::Type code = static_cast<EPathFollowingResult::Type>(result);
	switch (code)
	{
		default:
			OnPathFailedEvent.Broadcast(requestID);
			break;
		case EPathFollowingResult::Success:
			OnPathReachedEvent.Broadcast(requestID);
			break;
	}
}

#pragma endregion

#pragma region Path Requests and Follow XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


int UNavigationControlerComponent::AIRequestPathTo(FVector location, float maxOffNavDistance, TSubclassOf<UNavigationQueryFilter> filter)
{
	if (!GetOwner())
		return -1;
	return SearchPath(nullptr, location, maxOffNavDistance, filter);
}

int UNavigationControlerComponent::AIRequestPathToActor(AActor* target, float maxOffNavDistance, TSubclassOf<UNavigationQueryFilter> filter)
{
	if (!GetOwner())
		return -1;
	if (!target)
		return -1;
	return SearchPath(target, target->GetActorLocation(), maxOffNavDistance, filter);
}

void UNavigationControlerComponent::OnPathFinished(const FPathFollowingResult& Result)
{
	Super::OnPathFinished(Result);
	FAIRequestID id;
	FName compName = FName(this->GetReadableName());
	if (!_activePathQueue.Dequeue(id))
	{
		if (IsDebug)
		{
			UKismetSystemLibrary::PrintString(
				this, FString::Printf(TEXT("[PathFinding] - Path Finnished but wasn't in queue")), true, true,
				FColor::Red, 2, compName);
		}
		return;
	}
	if (!id.IsValid())
	{
		if (IsDebug)
		{
			UKismetSystemLibrary::PrintString(
				this, FString::Printf(TEXT("[PathFinding] - Path Finnished with an invalid ID")), true, true,
				FColor::Red, 2, compName);
		}
		return;
	}
	OnPathEnds(id, Result.Code);
	if (IsDebug)
	{
		UKismetSystemLibrary::PrintString(
			this, FString::Printf(TEXT("[PathFinding] - Path ID(%d) Finnished with result: %s"), id.GetID(), *UEnum::GetValueAsName(Result.Code).ToString()), true, true,
			Result.IsSuccess() ? FColor::Green : FColor::Cyan, 2, compName);
	}
}


FAIRequestID UNavigationControlerComponent::RequestMove(const FAIMoveRequest& RequestData, FNavPathSharedPtr InPath)
{
	_curvesMap.Empty();
	_explicitPathPause = false;
	if (SmoothDirectionThreshold > 0 && InPath.IsValid() && InPath->GetPathPoints().Num() > 3)
	{
		for (int i = InPath->GetPathPoints().Num() - 2; i >= 1; i--)
		{
			auto middlePoint = InPath->GetPathPoints()[i];
			if (middlePoint.CustomNavLinkId != FNavLinkId::Invalid)
				continue;
			auto startPoint = middlePoint.Location + (InPath->GetPathPoints()[i - 1].Location - middlePoint.Location).GetClampedToMaxSize(SmoothDirectionThreshold);
			auto endPoint = middlePoint.Location + (InPath->GetPathPoints()[i + 1].Location - middlePoint.Location).GetClampedToMaxSize(SmoothDirectionThreshold);
			if (UToolsLibrary::IsVectorCone((startPoint - middlePoint.Location).GetSafeNormal(), (middlePoint.Location - endPoint).GetSafeNormal(), SmoothAngleThreshold))
				continue;
			const float dotProduct = ((startPoint - middlePoint.Location).GetSafeNormal() | (middlePoint.Location - endPoint).GetSafeNormal());
			InPath->GetPathPoints().RemoveAt(i);
			FVector headingDirection = FVector(0);
			int count = 0;
			for (float f = 1; f >= 0; f -= SmoothStep)
			{
				float alpha = FAlphaBlend::AlphaToBlendOption(f, SmoothCurve);
				const FVector lerp1 = FMath::Lerp(startPoint, middlePoint.Location, f);
				const FVector lerp2 = FMath::Lerp(middlePoint.Location, endPoint, f);
				const FVector pt = FMath::Lerp(lerp1, lerp2, alpha);
				if (f != 1)
				{
					FVector direction = (InPath->GetPathPoints()[i].Location - pt).GetSafeNormal();
					if (headingDirection.SquaredLength() > 0 && (headingDirection | direction) <= 0)
						continue;;
					headingDirection = direction;
					count++;
				}
				FNavPathPoint navPt = middlePoint;
				navPt.Location = pt;
				InPath->GetPathPoints().Insert(navPt, i);
			}
			for (int k = 0; k < _curvesMap.Num(); k++)
				_curvesMap[k] = FVector(_curvesMap[k].X + count, _curvesMap[k].Y + count, _curvesMap[k].Z);
			_curvesMap.Insert(FVector(i, i + count - 1, FMath::RadiansToDegrees(FMath::Acos(dotProduct))), 0);
		}
	}
	return Super::RequestMove(RequestData, InPath);
}


void UNavigationControlerComponent::OnAsyncPathEvaluated(uint32 aPathId, ENavigationQueryResult::Type aResultType, FNavPathSharedPtr aNavPointer)
{
	FName compName = FName(this->GetReadableName());

	TTuple<uint32, TSoftObjectPtr<AActor>> requestItem;
	if (!_asyncPathRequestQueue.Dequeue(requestItem))
	{
		if (IsDebug)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[PathFinding] - Async Path Evaluation ID (%d) was not been Queued. Aborting"), aPathId),
			                                  true, true, FColor::Red, 2, compName);
		}
		OnPathEnds(FAIRequestID(aPathId), EPathFollowingResult::Invalid);
		return;
	}
	if (requestItem.Key != aPathId)
	{
		if (IsDebug)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[PathFinding] - Async Path Evaluation ID (%d) have ID mismatch (%d). Aborting"), aPathId, requestItem.Key),
			                                  true, true, FColor::Red, 2, compName);
		}
		OnPathEnds(FAIRequestID(aPathId), EPathFollowingResult::Invalid);
		return;
	}

	if (aResultType != ENavigationQueryResult::Success)
	{
		if (IsDebug)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[PathFinding] - Async Path Evaluation ID (%d) Was not successfull. Aborting"), aPathId),
			                                  true, true, FColor::Red, 2, compName);
		}
		OnPathEnds(FAIRequestID(aPathId), EPathFollowingResult::Invalid);
		return;
	}

	if (IsDebug)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[PathFinding] - Async Path Evaluation ID (%d) ended with result (%s)"), aPathId, *UEnum::GetValueAsString(aResultType)),
		                                  true, true, FColor::Turquoise, 2, compName);
	}

	auto moveRequest = FAIMoveRequest(aNavPointer->GetGoalLocation());
	moveRequest.SetAllowPartialPath(aNavPointer->IsPartial());
	moveRequest.SetAcceptanceRadius(AgentRadius);
	//_moveRequest.SetNavigationFilter(aNavPointer->GetFilter());
	moveRequest.SetProjectGoalLocation(true);
	moveRequest.SetCanStrafe(true);
	moveRequest.SetReachTestIncludesAgentRadius(true);
	moveRequest.SetUsePathfinding(true);
	moveRequest.SetRequireNavigableEndLocation(false);

	if (requestItem.Value.IsValid())
	{
		moveRequest.SetGoalActor(requestItem.Value.Get());
		aNavPointer->SetGoalActorObservation(*requestItem.Value.Get(), AgentRadius * 0.5);
	}

	_asyncPathResponseQueue.Enqueue(TTuple<uint32, TTuple<FAIMoveRequest, FNavPathSharedPtr>>(aPathId, TTuple<FAIMoveRequest, FNavPathSharedPtr>(moveRequest, aNavPointer)));
}

void UNavigationControlerComponent::UpdateStartPath()
{
	if (_asyncPathResponseQueue.IsEmpty())
		return;
	TTuple<uint32, TTuple<FAIMoveRequest, FNavPathSharedPtr>> request;
	if (_asyncPathResponseQueue.Dequeue(request))
	{
		auto thisRequestID = RequestMove(request.Value.Key, request.Value.Value);
		FName compName = FName(this->GetReadableName());
		if (thisRequestID.IsValid())
		{
			SetAcceptanceRadius(request.Value.Key.GetAcceptanceRadius());
			FAIRequestID id = FAIRequestID(request.Key);
			_activePathQueue.Enqueue(id);
			OnPathStartedEvent.Broadcast(id);
			if (IsDebug)
			{
				UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[PathFinding] - Path ID(%d) Started with request ID(%d)"), id.GetID(), thisRequestID.GetID()),
				                                  true, true, FColor::Turquoise, 2, compName);
			}
		}
		else
		{
			if (IsDebug)
			{
				UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[PathFinding] - Path ID(%d) aborted due to invalid request ID(%d)"), request.Key, thisRequestID.GetID()),
				                                  true, true, FColor::Red, 2, compName);
				OnPathEnds(FAIRequestID(request.Key), EPathFollowingResult::Invalid);
			}
		}
	}
}

void UNavigationControlerComponent::CalculatePathRemainingLenght()
{
	PathTotalLenght = 0;
	PathRemainingLenght = 0;
	PathCurrentSegmentLenght = 0;
	PathCurrentSegmentRemainingLenght = 0;
	float addSegmentLenght = 0;

	if (HasValidPath())
	{
		for (uint32 i = 1; static_cast<int>(i) < GetPath()->GetPathPoints().Num(); i++)
		{
			PathTotalLenght += (GetPath()->GetPathPoints()[i].Location - GetPath()->GetPathPoints()[i - 1].Location).Length();
			if (i > GetNextPathIndex())
			{
				PathRemainingLenght += (GetPath()->GetPathPoints()[i].Location - GetPath()->GetPathPoints()[i - 1].Location).Length();
			}
			if (i == GetNextPathIndex())
			{
				const int curveIndex = _curvesMap.Num() > 0 ? _curvesMap.IndexOfByPredicate([i](const FVector& item)-> bool { return i >= item.X && i <= item.Y; }) : INDEX_NONE;
				if (curveIndex != INDEX_NONE)
				{
					float firstSegment = 0;
					for (int j = _curvesMap[curveIndex].X; j <= _curvesMap[curveIndex].Y; j++)
					{
						PathCurrentSegmentLenght += (GetPath()->GetPathPoints()[j].Location - GetPath()->GetPathPoints()[j - 1].Location).Length();
						if (j == _curvesMap[curveIndex].X)
							firstSegment = PathCurrentSegmentLenght;
					}
					addSegmentLenght = PathCurrentSegmentLenght - firstSegment;
				}
				else
				{
					PathCurrentSegmentLenght = (GetPath()->GetPathPoints()[i].Location - GetPath()->GetPathPoints()[i - 1].Location).Length();
				}
			}
		}
		if (GetPath()->GetPathPoints().IsValidIndex(GetNextPathIndex()))
		{
			PathRemainingLenght += (GetPath()->GetPathPoints()[GetNextPathIndex()].Location - GetCurrentNavLocation()).Length();
			PathCurrentSegmentRemainingLenght = (GetPath()->GetPathPoints()[GetNextPathIndex()].Location - GetCurrentNavLocation()).Length() + addSegmentLenght;
		}
	}
}

void UNavigationControlerComponent::FollowPath(float delta)
{
	IsFollowingAPath = HasValidPath() && GetPathFollowingStatus() == EPathFollowingStatus::Moving;

	// Calculate path distances
	CalculatePathRemainingLenght();

	//Handle direction
	FVector newPathDir = FVector(0);
	float speedScale = 1;
	if (IsFollowingAPath)
	{
		newPathDir = GetCurrentDirection();
		int pathIndex = GetCurrentPathIndex();
		const int curveIndex = _curvesMap.Num() > 0
			                       ? _curvesMap.IndexOfByPredicate([pathIndex](const FVector& item)-> bool { return pathIndex >= item.X && pathIndex <= item.Y; })
			                       : INDEX_NONE;
		if (curveIndex != INDEX_NONE)
		{
			const float angleScale = FMath::GetMappedRangeValueClamped(TRange<double>(SmoothAngleThreshold, 90), TRange<double>(0, 1), _curvesMap[curveIndex].Z);
			speedScale = 1 - (CorneringSpeedReduction * angleScale);
		}
		else if (GetPath()->GetPathPoints().IsValidIndex(GetNextPathIndex()) && PathCurrentSegmentLenght >= MinimumBackToPathSegmentLeght)
		{
			const FVector pointDirVector = FVector::VectorPlaneProject(GetPath()->GetPathPoints()[GetNextPathIndex()].Location - GetCurrentNavLocation().Location, FVector::UpVector);
			if (pointDirVector.Length() > AgentRadius * 3)
				newPathDir = pointDirVector.GetSafeNormal();
		}
	}

	//Check if the navigation location is too far from the actual location
	{
		NavigationOffset = FVector::VectorPlaneProject((GetOwner()->GetActorLocation() - GetCurrentNavLocation().Location), FVector::UpVector);
		if (NavigationOffset.Length() > AgentRadius && IsFollowingAPath && !_explicitPathPause)
		{
			if (!UToolsLibrary::IsVectorCone(FVector::VectorPlaneProject(newPathDir, FVector::UpVector), -NavigationOffset, 30))
			{
				CancelPath();
			}
			else
			{
				newPathDir = FVector::VectorPlaneProject(GetCurrentNavLocation().Location - GetOwner()->GetActorLocation(), FVector::UpVector).GetSafeNormal();
			}
		}
	}

	if (IsDebug)
	{
		const FName compName = FName(this->GetReadableName());
		UKismetSystemLibrary::PrintString(this, FString::Printf(
			                                  TEXT("[PathFinding] - Path following state: %s. Paused? (%d). Total Path Lenght (%f), Remaining distance (%f), Segment Lenght (%f), Remaining Segment (%f)")
			                                  , *UEnum::GetValueAsString(Status), _explicitPathPause, PathTotalLenght, PathRemainingLenght, PathCurrentSegmentLenght, PathCurrentSegmentRemainingLenght),
		                                  true, false, FColor::Silver, delta, FName(FString::Printf(TEXT("%s_Status"), *compName.ToString())));
		UKismetSystemLibrary::DrawDebugCylinder(this, GetCurrentNavLocation().Location, GetCurrentNavLocation().Location + FVector::UpVector * AgentHeight, AgentRadius, 12, FColor::Silver,
		                                        delta);
	}

	// handle accelerations
	float alpha = 1;
	if (SmoothDirectionThreshold > 0 && (PathRemainingLenght < SmoothDirectionThreshold || (PathTotalLenght - PathRemainingLenght) < SmoothDirectionThreshold))
		alpha = 2 * delta;

	//Handle cornering
	newPathDir *= speedScale;
	if (speedScale < 1)
		alpha *= 1 - speedScale;

	//Lerp velocity
	PathVelocity = FMath::Lerp(PathVelocity, newPathDir, alpha);

	if (HasValidPath())
	{
		if (IsDebug)
		{
			for (uint32 i = 0; static_cast<int>(i) < GetPath()->GetPathPoints().Num(); i++)
			{
				if (GetPath()->GetPathPoints().IsValidIndex(i + 1))
				{
					const int curveIndex = _curvesMap.IndexOfByPredicate([i](const FVector& item)-> bool { return i >= item.X && i <= item.Y; });
					FColor futureColor = curveIndex != INDEX_NONE ? FColor::Yellow : FColor::White;
					FColor pastColor = curveIndex != INDEX_NONE ? FColor::Black : FColor::Silver;
					FColor debugColor = i == GetCurrentPathIndex() ? FColor::Orange : (i > GetCurrentPathIndex() ? futureColor : pastColor);
					UKismetSystemLibrary::DrawDebugArrow(this, GetPath()->GetPathPoints()[i].Location, GetPath()->GetPathPoints()[i + 1].Location, 50, debugColor, delta, 3);
				}
			}

			const FVector currentLocation = GetOwner()->GetActorLocation();
			UKismetSystemLibrary::DrawDebugArrow(this, currentLocation, currentLocation + PathVelocity * 50, 50, FColor::Magenta, delta);
		}
	}
}


#pragma endregion


#pragma region Nav Events XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


void UPathFollowEvent::Activate()
{
	if (_controller == nullptr)
	{
		FFrame::KismetExecutionMessage(TEXT("Invalid AI Modular Controller. Cannot execute PathFollow."), ELogVerbosity::Error);
		_OnPathFailed(FAIRequestID::InvalidRequest);
		return;
	}

	int requestID = -1;

	if (_targetMode)
	{
		requestID = _controller->AIRequestPathToActor(_target, _offNavDistance, _navFilter);
	}
	else
	{
		requestID = _controller->AIRequestPathTo(_destination, _offNavDistance, _navFilter);
	}

	if (requestID < 0)
		_OnPathFailed(FAIRequestID::InvalidRequest);

	pathID = FAIRequestID(requestID);
	_controller->OnPathFailedEvent.AddDynamic(this, &UPathFollowEvent::_OnPathFailed);
	_controller->OnPathReachedEvent.AddDynamic(this, &UPathFollowEvent::_OnPathReached);
}


void UPathFollowEvent::CleanUp()
{
	if (_controller == nullptr)
	{
		return;
	}

	_controller->OnPathFailedEvent.RemoveDynamic(this, &UPathFollowEvent::_OnPathFailed);
	_controller->OnPathReachedEvent.RemoveDynamic(this, &UPathFollowEvent::_OnPathReached);
}


UPathFollowEvent* UPathFollowEvent::ModularAIMoveTo(const UObject* WorldContextObject, UNavigationControlerComponent* controller, FVector location,
                                                    float maxOffNavDistance, TSubclassOf<UNavigationQueryFilter> filter)
{
	UPathFollowEvent* Node = NewObject<UPathFollowEvent>();
	Node->_controller = controller;
	Node->_destination = location;
	Node->_offNavDistance = maxOffNavDistance;
	Node->_targetMode = false;
	Node->_navFilter = filter;
	Node->RegisterWithGameInstance(WorldContextObject);
	return Node;
}

UPathFollowEvent* UPathFollowEvent::ModularAIFollow(const UObject* WorldContextObject,
                                                    UNavigationControlerComponent* controller, AActor* target, float maxOffNavDistance,
                                                    TSubclassOf<UNavigationQueryFilter> filter)
{
	UPathFollowEvent* Node = NewObject<UPathFollowEvent>();
	Node->_controller = controller;
	Node->_offNavDistance = maxOffNavDistance;
	Node->_target = target;
	Node->_targetMode = true;
	Node->_navFilter = filter;
	Node->RegisterWithGameInstance(WorldContextObject);
	return Node;
}

void UPathFollowEvent::_OnPathReached(FAIRequestID requestID)
{
	if (pathID.IsValid() || requestID.IsValid())
	{
		if (!requestID.IsEquivalent(pathID))
			return;
	}
	OnPathReached.Broadcast(requestID);
	CleanUp();
	SetReadyToDestroy();
}


void UPathFollowEvent::_OnPathFailed(FAIRequestID requestID)
{
	if (pathID.IsValid() || requestID.IsValid())
	{
		if (!requestID.IsEquivalent(pathID))
			return;
	}
	OnPathFailed.Broadcast(requestID);
	CleanUp();
	SetReadyToDestroy();
}

#pragma endregion
