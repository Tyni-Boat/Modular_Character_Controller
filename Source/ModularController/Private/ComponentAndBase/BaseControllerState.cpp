// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/BaseControllerState.h"



void UBaseControllerState::SaveStateSnapShot()
{
	if (_snapShotSaved)
		return;
	SaveStateSnapShot_Internal();
	_wasTheLastFrameBehaviour_saved = _wasTheLastFrameBehaviour;
	_snapShotSaved = true;
}

void UBaseControllerState::RestoreStateFromSnapShot()
{
	if(!_snapShotSaved)
		return;
	_wasTheLastFrameBehaviour = _wasTheLastFrameBehaviour_saved;
	RestoreStateFromSnapShot_Internal();
	_snapShotSaved = false;
}




bool UBaseControllerState::CheckState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs, UModularControllerComponent* controller
	, FStatusParameters controllerStatusParam, FStatusParameters& currentStatus, const float inDelta, int overrideWasLastStateStatus)
{
	return false;
}



FVelocity UBaseControllerState::ProcessState_Implementation(FStatusParameters controllerStatusParam, FStatusParameters& controllerStatus, const FKinematicInfos& inDatas,
	const FVector moveInput, UModularControllerComponent* controller, const float inDelta)
{
	return FVelocity();
}

void UBaseControllerState::OnEnterState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta)
{
}


void UBaseControllerState::OnExitState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta)
{
}

void UBaseControllerState::OnControllerStateChanged_Implementation(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller)
{
}

void UBaseControllerState::SaveStateSnapShot_Internal()
{
}

void UBaseControllerState::RestoreStateFromSnapShot_Internal()
{
}


FString UBaseControllerState::DebugString()
{
	return GetDescriptionName().ToString();
}


void UBaseControllerState::OnActionChanged_Implementation(UBaseControllerAction* newAction,
	UBaseControllerAction* lastAction)
{
}

bool UBaseControllerState::GetWasTheLastFrameControllerState()
{
	return _wasTheLastFrameBehaviour;
}

void UBaseControllerState::SetWasTheLastFrameControllerState(bool value)
{
	_wasTheLastFrameBehaviour = value;
}

