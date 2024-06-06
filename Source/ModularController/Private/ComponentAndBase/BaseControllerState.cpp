// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/BaseControllerState.h"



void UBaseControllerState::SaveStateSnapShot()
{
	if (_snapShotSaved)
		return;
	SaveStateSnapShot_Internal();
	_snapShotSaved = true;
}

void UBaseControllerState::RestoreStateFromSnapShot()
{
	if(!_snapShotSaved)
		return;
	RestoreStateFromSnapShot_Internal();
	_snapShotSaved = false;
}



FKinematicComponents UBaseControllerState::OnEnterState_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta)
{
	return startingConditions;
}

FKinematicComponents UBaseControllerState::OnExitState_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta)
{
	return startingConditions;
}

FControllerCheckResult UBaseControllerState::CheckState_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, const float inDelta, bool asLastActiveState)
{
	return FControllerCheckResult(false, startingConditions);
}

FControllerStatus UBaseControllerState::ProcessState_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, const float delta)
{
	return startingConditions;
}

void UBaseControllerState::OnControllerStateChanged_Implementation(UModularControllerComponent* onController,
	FName newBehaviourDescName, int newPriority)
{
}

void UBaseControllerState::OnControllerActionChanged_Implementation(UModularControllerComponent* onController,
	UBaseControllerAction* newAction, UBaseControllerAction* lastAction)
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



