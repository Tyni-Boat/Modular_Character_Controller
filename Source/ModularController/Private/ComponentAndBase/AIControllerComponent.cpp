// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/AIControllerComponent.h"



// Sets default values for this component's properties
UAIControllerComponent::UAIControllerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UAIControllerComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
}


// Called every frame
void UAIControllerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	_delta = DeltaTime;
	PathVelocity = FMath::Lerp(PathVelocity, FVector::Zero(), DeltaTime);
}



void UAIControllerComponent::RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed)
{
	PathVelocity = MoveVelocity;
	TargetDistance = PathVelocity.Length() * _delta;
}


void UAIControllerComponent::RequestPathMove(const FVector& MoveInput)
{
	PathInput = MoveInput;
	PathInputDistance = PathInput.Length();
}






int32 UAIControllerComponent::GetAvoidanceGroupMask()
{
	return 0;
}

int32 UAIControllerComponent::GetGroupsToAvoidMask()
{
	return 0;
}

int32 UAIControllerComponent::GetGroupsToIgnoreMask()
{
	return 0;
}

float UAIControllerComponent::GetRVOAvoidanceConsiderationRadius()
{
	return 0;
}

float UAIControllerComponent::GetRVOAvoidanceHeight()
{
	return 0;
}

FVector UAIControllerComponent::GetRVOAvoidanceOrigin()
{
	return GetOwner()->GetActorLocation();
}

float UAIControllerComponent::GetRVOAvoidanceRadius()
{
	return 0;
}

int32 UAIControllerComponent::GetRVOAvoidanceUID()
{
	return 0;
}

float UAIControllerComponent::GetRVOAvoidanceWeight()
{
	return 0;
}

FVector UAIControllerComponent::GetVelocityForRVOConsideration()
{
	return {};
}

void UAIControllerComponent::SetAvoidanceGroupMask(int32 GroupFlags)
{
}

void UAIControllerComponent::SetGroupsToAvoidMask(int32 GroupFlags)
{
}

void UAIControllerComponent::SetGroupsToIgnoreMask(int32 GroupFlags)
{
}

void UAIControllerComponent::SetRVOAvoidanceUID(int32 UID)
{
}

void UAIControllerComponent::SetRVOAvoidanceWeight(float Weight)
{
}