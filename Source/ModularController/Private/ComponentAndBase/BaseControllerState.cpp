// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/BaseControllerState.h"



void UBaseControllerState::OnEnterState_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
}

void UBaseControllerState::OnExitState_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
}

FControllerCheckResult UBaseControllerState::CheckState_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, const float inDelta, bool asLastActiveState) const
{
	auto result = startingConditions;
	result.ControllerSurface.Reset();
	return FControllerCheckResult(false, result);
}

FControllerStatus UBaseControllerState::ProcessState_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, const float delta) const
{
	return startingConditions;
}



FString UBaseControllerState::DebugString() const
{
	return GetDescriptionName().ToString();
}



