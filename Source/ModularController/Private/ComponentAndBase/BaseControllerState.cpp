// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/BaseControllerState.h"




int UBaseControllerState::GetPriority_Implementation()
{
	return 0;
}

FName UBaseControllerState::GetDescriptionName_Implementation()
{
	return "";
}


bool UBaseControllerState::CheckState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	return false;
}

FMovePreprocessParams UBaseControllerState::PreProcessState_Implementation(const FKinematicInfos& inDatas,
	const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	return FMovePreprocessParams();
}

FVelocity UBaseControllerState::ProcessState_Implementation(const FKinematicInfos& inDatas, const FMovePreprocessParams params,
	UModularControllerComponent* controller, const float inDelta)
{
	return FVelocity();
}

void UBaseControllerState::OnEnterState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
}


void UBaseControllerState::OnExitState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
}

void UBaseControllerState::OnControllerStateChanged_Implementation(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller)
{
}



FString UBaseControllerState::DebugString()
{
	return GetDescriptionName().ToString();
}


bool UBaseControllerState::GetWasTheLastFrameBehaviour()
{
	return _wasTheLastFrameBehaviour;
}

void UBaseControllerState::SetWasTheLastFrameBehaviour(bool value)
{
	_wasTheLastFrameBehaviour = value;
}