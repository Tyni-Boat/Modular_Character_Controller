// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/NavigationControlerComponent.h"

#include <Kismet/KismetSystemLibrary.h>
#include "NavigationPath.h"
#include "Kismet/KismetMathLibrary.h"
#include "AIModule/Classes/AIController.h"
#include "AIModule/Classes/Blueprint/AIBlueprintHelperLibrary.h"
#include "NavigationSystem.h"
#include "ComponentAndBase/ModularControllerComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"	
#include "NavMesh/NavMeshPath.h"
#include "Net/UnrealNetwork.h"
#include "NavLinkCustomInterface.h"
#include "Engine/ActorChannel.h"


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

	SetIsReplicatedByDefault(true);
	// ...
}


// Called when the game starts
void UNavigationControlerComponent::BeginPlay()
{
	Super::BeginPlay();
	t_ownerPawn = Cast<APawn>(GetOwner());
	if (!t_ownerPawn)
		return;

	SetBlockDetectionState(true);
	if (const auto modController = t_ownerPawn->GetComponentByClass<UModularControllerComponent>())
	{
		SetMovementComponent(modController);
	}

	if (IsDebug)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<int>(this->GetUniqueID() + 0), 5, FColor::Blue, FString::Printf(TEXT("Owner Pawn is Valid: %s"), *t_ownerPawn->GetActorNameOrLabel()));
	}
}


// Called every frame
void UNavigationControlerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetNetRole() == ROLE_Authority)
	{
		if (IsDebug && t_ownerPawn)
		{
			const FVector location = GetCurrentNavLocation();
			const FVector actorUp = t_ownerPawn->GetActorUpVector();
			UKismetSystemLibrary::DrawDebugCylinder(GetWorld(), location, location + actorUp * AgentHeight, AgentRadius, 6, FColor::White);
		}
	}

	UpdatePath(DeltaTime);
	FollowPath(DeltaTime);
}


bool UNavigationControlerComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch,
	FReplicationFlags* RepFlags)
{
	bool wroten = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	for (auto point : CustomPathPoints)
	{
		if (point)
		{
			wroten |= Channel->ReplicateSubobject(point, *Bunch, *RepFlags);
		}
	}
	return wroten;
}


#pragma endregion

#pragma region Network XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


void UNavigationControlerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNavigationControlerComponent, IsFollowingAPath);
	DOREPLIFETIME(UNavigationControlerComponent, CustomPathPoints);
}


void UNavigationControlerComponent::ServerFindPathToLocation_Implementation(FVector location,
	TSubclassOf<UNavigationQueryFilter> filter)
{
	FString debugString;
	//
	if (UAIBlueprintHelperLibrary::IsValidAILocation(GetOwner()->GetActorLocation()))
	{
		if (UAIBlueprintHelperLibrary::IsValidAILocation(location)) {
			if (const auto pathRequest = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), t_ownerPawn->GetActorLocation(), location, t_ownerPawn, filter))
			{
				if (pathRequest->GetPath())
				{
					_moveRequest = FAIMoveRequest(location);
					_moveRequest.SetAllowPartialPath(true);
					_moveRequest.SetAcceptanceRadius(AgentRadius);
					_moveRequest.SetNavigationFilter(filter);
					_moveRequest.SetProjectGoalLocation(true);
					_moveRequest.SetCanStrafe(true);
					_moveRequest.SetReachTestIncludesAgentRadius(true);
					_moveRequest.SetUsePathfinding(true);
					_moveRequest.SetRequireNavigableEndLocation(false);
					_currentMoveRequestID = RequestMove(_moveRequest, pathRequest->GetPath());
					debugString = FString::Printf(TEXT("Starting path follow with %d path points"), pathRequest->GetPath()->GetPathPoints().Num());
				}
				else
				{
					MulticastOnPathFinnished(EPathFollowingResult::Invalid);
					debugString = FString::Printf(TEXT("Invalid Nav Shared Pointer Path"));
				}
			}
			else
			{
				MulticastOnPathFinnished(EPathFollowingResult::Invalid);
				debugString = FString::Printf(TEXT("Invalid Navigation Path"));
			}
		}
		else
		{
			MulticastOnPathFinnished(EPathFollowingResult::Invalid);
			debugString = FString::Printf(TEXT("Invalid AI location"));
		}
	}
	else
	{
		MulticastOnPathFinnished(EPathFollowingResult::Invalid);
		debugString = FString::Printf(TEXT("Invalid Current location"));
	}
	//
	if (IsDebug)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<int>(this->GetUniqueID() + 1), 3, FColor::Blue, FString::Printf(TEXT("Request Path To: %s. Result: %s"), *location.ToCompactString(), *debugString));
	}
}


void UNavigationControlerComponent::ServerFindPathToActor_Implementation(AActor* target,
	TSubclassOf<UNavigationQueryFilter> filter)
{
	FString actorName;
	FString debugString;
	//
	if (UAIBlueprintHelperLibrary::IsValidAILocation(GetOwner()->GetActorLocation()))
	{
		if (target)
		{
			actorName = target->GetActorNameOrLabel();
			if (UAIBlueprintHelperLibrary::IsValidAILocation(target->GetActorLocation()))
			{
				if (const auto pathRequest = UNavigationSystemV1::FindPathToActorSynchronously(GetWorld(), t_ownerPawn->GetActorLocation(), target, AgentRadius * 0.5f, t_ownerPawn, filter))
				{
					if (pathRequest->GetPath())
					{
						_moveRequest = FAIMoveRequest(target);
						_moveRequest.SetAllowPartialPath(true);
						_moveRequest.SetAcceptanceRadius(AgentRadius);
						_moveRequest.SetNavigationFilter(filter);
						_moveRequest.SetProjectGoalLocation(true);
						_moveRequest.SetCanStrafe(true);
						_moveRequest.SetReachTestIncludesAgentRadius(true);
						_moveRequest.SetUsePathfinding(true);
						_moveRequest.SetRequireNavigableEndLocation(false);
						_currentMoveRequestID = RequestMove(_moveRequest, pathRequest->GetPath());
						debugString = FString::Printf(TEXT("Starting path follow with %d path points"), pathRequest->GetPath()->GetPathPoints().Num());
					}
					else
					{
						MulticastOnPathFinnished(EPathFollowingResult::Invalid);
						debugString = FString::Printf(TEXT("Invalid Nav Shared Pointer Path"));
					}
				}
				else
				{
					MulticastOnPathFinnished(EPathFollowingResult::Invalid);
					debugString = FString::Printf(TEXT("Invalid Navigation Path"));
				}

			}
			else
			{
				MulticastOnPathFinnished(EPathFollowingResult::Invalid);
				debugString = FString::Printf(TEXT("Invalid AI location"));
			}
		}
		else
		{
			MulticastOnPathFinnished(EPathFollowingResult::Invalid);
			debugString = FString::Printf(TEXT("Invalid Target Actor"));
		}
	}
	else
	{
		MulticastOnPathFinnished(EPathFollowingResult::Invalid);
		debugString = FString::Printf(TEXT("Invalid Current location"));
	}
	if (IsDebug)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<int>(this->GetUniqueID() + 1), 3, FColor::Blue, FString::Printf(TEXT("Request to Follow: %s. Result: %s"), *actorName, *debugString));
	}
}


void UNavigationControlerComponent::MulticastUpdatePath_Implementation()
{
	OnPathUpdatedEvent.Broadcast();

	if (GetNetRole() == ROLE_Authority)
		return;

	if (IsDebug)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<int>(this->GetUniqueID() + 4), 3, FColor::Yellow, FString::Printf(TEXT("Updated Path Broadcast")));
	}

	//Follow index
	_clientNextPathIndex = 1;
}

void UNavigationControlerComponent::MulticastOnPathFinnished_Implementation(uint32 result)
{
	EPathFollowingResult::Type code = static_cast<EPathFollowingResult::Type>(result);
	switch (code)
	{
	default:
		OnPathFailedEvent.Broadcast();
		break;
	case EPathFollowingResult::Success:
		OnPathReachedEvent.Broadcast();
		break;
	}
}

#pragma endregion

#pragma region Path Requests and Follow XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


bool UNavigationControlerComponent::AIRequestPathTo(FVector location, TSubclassOf<UNavigationQueryFilter> filter)
{
	if (!t_ownerPawn)
		return false;
	ServerFindPathToLocation(location, filter);
	return true;
}

bool UNavigationControlerComponent::AIRequestPathToActor(AActor* target, TSubclassOf<UNavigationQueryFilter> filter)
{
	if (!t_ownerPawn)
		return false;
	if (!target)
		return false;
	ServerFindPathToActor(target, filter);
	return true;
}

void UNavigationControlerComponent::OnPathUpdated()
{
	Super::OnPathUpdated();
	if (GetNetRole() != ROLE_Authority)
		return;

	auto updatedPathArray = HasValidPath() ? GetPath()->GetPathPoints() : TArray<FNavPathPoint>();

	//Set new value and synchronize
	CustomPathPoints.Empty();
	for (int i = 0; i < updatedPathArray.Num(); i++)
	{
		INavLinkCustomInterface* customNavLink = nullptr;
		if (const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			customNavLink = NavSys->GetCustomLink(updatedPathArray[i].CustomNavLinkId);
		}

		auto pt = NewObject<UNetPathPoint>(UNetPathPoint::StaticClass(), UNetPathPoint::StaticClass());
		pt->Location = updatedPathArray[i].Location;
		pt->PointIndex = i;
		pt->NavLinkInterface = customNavLink ? Cast<UObject>(customNavLink) : nullptr;
		CustomPathPoints.Add(pt);
	}

	MulticastUpdatePath();

	if (IsDebug)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<int>(this->GetUniqueID() + 1), 2, FColor::Blue, FString::Printf(TEXT("Path Updated")));
	}
}

void UNavigationControlerComponent::OnPathFinished(const FPathFollowingResult& Result)
{
	Super::OnPathFinished(Result);
	MulticastOnPathFinnished(Result.Code);
	if (IsDebug)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<int>(this->GetUniqueID() + 2), 3, FColor::Orange, FString::Printf(TEXT("Path Finnished with result: %s. Is success? %d"), *UEnum::GetValueAsName(Result.Code).ToString(), Result.IsSuccess()));
	}
}

bool UNavigationControlerComponent::AgentCapsuleContainsPoint(const FVector point)
{
	if (GetNetRole() == ROLE_Authority)
	{
		if (t_ownerPawn)
		{
			const FVector location = GetCurrentNavLocation();
			const FVector actorUp = t_ownerPawn->GetActorUpVector();

			const FVector VVec = (point - location).ProjectOnToNormal(actorUp);
			const FVector Hvec = FVector::VectorPlaneProject(point - location, actorUp);

			return VVec.Length() <= AgentHeight && Hvec.Length() <= AgentRadius;
		}
	}
	return false;
}


void UNavigationControlerComponent::UpdatePath(float delta)
{
	if (!t_ownerPawn)
		return;

	if (GetNetRole() == ROLE_Authority)
	{
		if (IsDebug && t_ownerPawn->IsLocallyControlled() && HasValidPath() && Path->GetPathPoints().Num() > 0)
		{
			for (int i = 1; i < Path->GetPathPoints().Num(); i++)
			{
				FColor debugColor = i == (GetCurrentPathIndex() + 1) ? FColor::Orange : (FNavMeshNodeFlags(Path->GetPathPoints()[i - 1].Flags).IsNavLink() ? FColor::Magenta : FColor::White);
				FVector offset = FVector::UpVector * 10;
				UKismetSystemLibrary::DrawDebugArrow(GetWorld(), Path->GetPathPoints()[i - 1].Location + offset
					, Path->GetPathPoints()[i].Location + offset, 100, debugColor, 0, 3);
			}
		}
	}
	else
	{
		if (IsDebug && IsFollowingAPath && t_ownerPawn->IsLocallyControlled())
		{
			for (int i = 1; i < CustomPathPoints.Num(); i++)
			{
				FColor debugColor = i == _clientNextPathIndex ? FColor::Red : (CustomPathPoints[i - 1]->GetNavLinkInterface() ? FColor::Magenta : FColor::Yellow);
				FVector offset = FVector::UpVector * 10;
				UKismetSystemLibrary::DrawDebugArrow(GetWorld(), CustomPathPoints[i - 1]->Location + offset
					, CustomPathPoints[i]->Location + offset, 100, debugColor, 0, 3);
			}
		}
	}
}

void UNavigationControlerComponent::FollowPath(float delta)
{
	if (!t_ownerPawn)
		return;

	bool followingPath = false;

	//Locations and directions
	const FVector currentLocation = t_ownerPawn->GetActorLocation();
	FVector nextLocation = currentLocation;
	float distance = 0;

	if (GetNetRole() == ROLE_Authority)
	{
		followingPath = HasValidPath()
			&& GetPath()->GetPathPoints().IsValidIndex(GetNextPathIndex());
		IsFollowingAPath = followingPath;
		nextLocation = HasValidPath() && GetPath()->GetPathPoints().IsValidIndex(GetNextPathIndex()) ? GetPath()->GetPathPoints()[GetNextPathIndex()].Location : currentLocation;
		distance = followingPath ? GetPath()->GetLengthFromPosition(currentLocation, GetNextPathIndex()) : 0;


		//Handle path recalculation
		if (t_lastDistance >= 0 && followingPath)
		{
			if (distance > t_lastDistance)
			{
				if (FMath::Abs(t_lastDistance - distance) >= MaxMoveAwayDistanceThreshold && AgentCapsuleContainsPoint(currentLocation))
				{
					GetPath()->ResetForRepath();
				}
			}
			else
			{
				t_lastDistance = distance;
			}
		}
		else if (followingPath)
		{
			t_lastDistance = distance;
		}
		else
		{
			t_lastDistance = -1;
		}
	}
	else
	{
		followingPath = CustomPathPoints.IsValidIndex(_clientNextPathIndex) ? IsFollowingAPath : false;
		nextLocation = CustomPathPoints.IsValidIndex(_clientNextPathIndex) && followingPath ? CustomPathPoints[_clientNextPathIndex]->Location : nextLocation;

		//Distance
		for (int i = CustomPathPoints.Num() - 1; i > static_cast<int>(_clientNextPathIndex); i--)
		{
			if ((i - 1) < 0)
				break;
			distance += (CustomPathPoints[i - 1]->Location - CustomPathPoints[i]->Location).Length();
		}
		distance += (nextLocation - currentLocation).Length();

		//On reach
		if (followingPath && CustomPathPoints.IsValidIndex(_clientNextPathIndex) && HasReached(nextLocation, EPathFollowingReachMode::OverlapAgent, AgentRadius))
		{
			//Check navlink
			if (CustomPathPoints[_clientNextPathIndex]->GetNavLinkInterface() && CustomPathPoints.IsValidIndex(_clientNextPathIndex + 1))
			{
				CustomPathPoints[_clientNextPathIndex]->GetNavLinkInterface()->OnLinkMoveStarted(this, CustomPathPoints[_clientNextPathIndex + 1]->Location);
				if (IsDebug)
				{
					GEngine->AddOnScreenDebugMessage(static_cast<int>(this->GetUniqueID() + 5), 3, FColor::Purple, FString::Printf(TEXT("nav link reached: %s"), *CustomPathPoints[_clientNextPathIndex]->GetNavLinkInterface()->GetLinkOwner()->GetName()));
				}
			}

			_clientNextPathIndex++;
			if (IsDebug)
			{
				GEngine->AddOnScreenDebugMessage(static_cast<int>(this->GetUniqueID() + 4), 1, FColor::Red, FString::Printf(TEXT("Client Reached Next Path Point. Now Next Target is: %d"), _clientNextPathIndex));
			}
		}
	}

	//Velocity required
	PathVelocity = followingPath ? (nextLocation - currentLocation) : FVector(0);

	//Compute distance
	PathRemainingDistance = distance;

	if (IsDebug)
	{
		if (GetNetRole() == ROLE_Authority)
			UKismetSystemLibrary::DrawDebugSphere(GetWorld(), currentLocation - FVector::UpVector * (AgentHeight * 0.5f), GetAcceptanceRadius(), 12, FColor::Yellow);
		if (followingPath && t_ownerPawn->IsLocallyControlled())
		{
			GEngine->AddOnScreenDebugMessage(static_cast<int>(this->GetUniqueID() + 3), 0, FColor::Green, FString::Printf(TEXT("Remaining Distance: %d"), static_cast<int>(PathRemainingDistance)));
		}
		UKismetSystemLibrary::DrawDebugArrow(GetWorld(), currentLocation, currentLocation + PathVelocity.GetSafeNormal() * 100, 100, FColor::Magenta, 0, 2);
	}
}


#pragma endregion


#pragma region Nav Events XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


void UPathFollowEvent::Activate()
{
	if (_controller == nullptr)
	{
		FFrame::KismetExecutionMessage(TEXT("Invalid AI Modular Controller. Cannot execute PathFollow."), ELogVerbosity::Error);
		_OnPathFailed();
		return;
	}

	if (_targetMode)
	{
		if (!_controller->AIRequestPathToActor(_target))
		{
			_controller->OnPathFailedEvent.AddDynamic(this, &UPathFollowEvent::_OnPathFailed);
			_controller->GetWorld()->GetTimerManager().SetTimer(TimerHandle_OnInstantFinish, this, &UPathFollowEvent::_OnPathFailed, 0.1f, false);
			//_OnPathFailed();
			return;
		}
	}
	else
	{
		if (!_controller->AIRequestPathTo(_destination))
		{
			_controller->OnPathFailedEvent.AddDynamic(this, &UPathFollowEvent::_OnPathFailed);
			_controller->GetWorld()->GetTimerManager().SetTimer(TimerHandle_OnInstantFinish, this, &UPathFollowEvent::_OnPathFailed, 0.1f, false);
			//_OnPathFailed();
			return;
		}
	}

	_controller->OnPathReachedEvent.AddDynamic(this, &UPathFollowEvent::_OnPathReached);
	_controller->OnPathFailedEvent.AddDynamic(this, &UPathFollowEvent::_OnPathFailed);
	_controller->OnPathUpdatedEvent.AddDynamic(this, &UPathFollowEvent::_OnPathUpdated);
}


void UPathFollowEvent::CleanUp()
{
	if (_controller == nullptr)
	{
		return;
	}
	_controller->OnPathReachedEvent.RemoveDynamic(this, &UPathFollowEvent::_OnPathReached);
	_controller->OnPathFailedEvent.RemoveDynamic(this, &UPathFollowEvent::_OnPathFailed);
	_controller->OnPathUpdatedEvent.RemoveDynamic(this, &UPathFollowEvent::_OnPathUpdated);
}


UPathFollowEvent* UPathFollowEvent::ModularAIMoveTo(const UObject* WorldContextObject, UNavigationControlerComponent* controller, FVector location, TSubclassOf<UNavigationQueryFilter> filter)
{
	UPathFollowEvent* Node = NewObject<UPathFollowEvent>();
	Node->_controller = controller;
	Node->_destination = location;
	Node->_targetMode = false;
	Node->RegisterWithGameInstance(WorldContextObject);
	return Node;
}

UPathFollowEvent* UPathFollowEvent::ModularAIFollow(const UObject* WorldContextObject,
	UNavigationControlerComponent* controller, AActor* target, TSubclassOf<UNavigationQueryFilter> filter)
{
	UPathFollowEvent* Node = NewObject<UPathFollowEvent>();
	Node->_controller = controller;
	Node->_target = target;
	Node->_targetMode = true;
	Node->RegisterWithGameInstance(WorldContextObject);
	return Node;
}

void UPathFollowEvent::_OnPathReached()
{
	OnPathReached.Broadcast();
	CleanUp();
	SetReadyToDestroy();
}

void UPathFollowEvent::_OnPathFailed()
{
	OnPathFailed.Broadcast();
	CleanUp();
	SetReadyToDestroy();
}

void UPathFollowEvent::_OnPathUpdated()
{
	OnPathUpdated.Broadcast();
}

#pragma endregion