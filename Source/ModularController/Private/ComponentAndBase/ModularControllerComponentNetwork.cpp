// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#include "ComponentAndBase/ModularControllerComponent.h"

#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Pawn.h"
#include "Engine.h"
#include "FunctionLibrary.h"
#include "ToolsLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Engine/EngineTypes.h"




#pragma region Network Logic XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

#pragma region Common Logic

void UModularControllerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//DOREPLIFETIME(UModularControllerComponent, LastMoveMade);
	//DOREPLIFETIME(UModularControllerComponent, ActionInstances);
}


ENetRole UModularControllerComponent::GetNetRole(TSoftObjectPtr<APawn> pawn) const
{
	if (!pawn.IsValid())
		return ENetRole::ROLE_None;

	if (pawn->HasAuthority())
	{
		return ENetRole::ROLE_Authority;
	}
	else if (pawn->IsLocallyControlled())
	{
		return ENetRole::ROLE_AutonomousProxy;
	}

	return ENetRole::ROLE_SimulatedProxy;
}


FName UModularControllerComponent::GetNetRoleDebug(ENetRole role) const
{
	FName value = "";
	switch (role)
	{
		case ENetRole::ROLE_Authority:
			value = "Authority";
		case ENetRole::ROLE_AutonomousProxy:
			value = "AutonomousProxy";
		case ENetRole::ROLE_SimulatedProxy:
			value = "SimulatedProxy";
		default:
			value = "InputEntryPhaseNone";
	}
	return value;
}


double UModularControllerComponent::GetNetLatency() const
{
	return _timeNetLatency;
}


#pragma endregion



#pragma region Server Logic //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UModularControllerComponent::MultiCastTime_Implementation(double timeStamp)
{
	_timeNetLatency = FMath::Abs(timeStamp - _timeElapsed);
}

void UModularControllerComponent::MultiCastKinematics_Implementation(FNetKinematic netKinematic)
{
	const ENetRole role = GetNetRole(OwnerPawn);
	if (role != ROLE_SimulatedProxy)
		return;
	netKinematic.RestoreOnToStatus(lastUpdatedControllerStatus);
	lastUpdatedControllerStatus.Kinematics.LinearKinematic = lastUpdatedControllerStatus.Kinematics.LinearKinematic.GetFinalCondition(GetNetLatency() * LatencyCompensationScale);
	if (DebugType == EControllerDebugType::NetworkDebug)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[DOWN] - Simulated Client { Received Kinematics with %fs latency}"), GetNetLatency()), true, true, FColor::Cyan, 1, "SimClientReceiveCommand_kin");
	}
}

void UModularControllerComponent::MultiCastStatusParams_Implementation(FNetStatusParam netStatusParam)
{
	const ENetRole role = GetNetRole(OwnerPawn);
	if (role != ROLE_SimulatedProxy)
		return;
	netStatusParam.RestoreOnToStatus(lastUpdatedControllerStatus);
	if (DebugType == EControllerDebugType::NetworkDebug)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[DOWN] - Simulated Client { Received Status Param with %fs latency}"), GetNetLatency()), true, true, FColor::Cyan, 1, "SimClientReceiveCommand_statusP");
	}
}




void UModularControllerComponent::MultiCastStates_Implementation(const TArray<TSubclassOf<UBaseControllerState>>& states, UModularControllerComponent* caller)
{
	if (caller != this)
		return;
	StatesInstances.Empty();
	for (int i = 0; i < states.Num(); i++)
	{
		if (!states[i])
			continue;
		StatesInstances.Add(states[i]->GetDefaultObject());
	}

	SortStates();
}


void UModularControllerComponent::MultiCastActions_Implementation(const TArray<TSubclassOf<UBaseControllerAction>>& actions, UModularControllerComponent* caller)
{
	if (caller != this)
		return;
	ActionInstances.Empty();
	for (int i = 0; i < actions.Num(); i++)
	{
		if (!actions[i])
			continue;
		ActionInstances.Add(actions[i]->GetDefaultObject());
	}

	SortActions();
}


#pragma region Listened/Athority OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO


void UModularControllerComponent::AuthorityComputeComponent(float delta, bool asServer)
{
	//_timeNetLatency = 0;
	const FVector moveInp = ConsumeMovementInput();
	const FControllerStatus initialState = ConsumeLastKinematicMove(moveInp, delta);
	auto status = StandAloneEvaluateStatus(initialState, delta);
	ComputedControllerStatus = status;

	//multi-casting
	if(asServer)
	{
		MultiCastTime(_timeElapsed);
		FNetKinematic netKinematic;
		netKinematic.ExtractFromStatus(status);
		MultiCastKinematics(netKinematic);
		FNetStatusParam netStatusParams;
		netStatusParams.ExtractFromStatus(status);
		MultiCastStatusParams(netStatusParams);

		if (DebugType == EControllerDebugType::NetworkDebug)
		{
			int dataSize = sizeof(netKinematic) + sizeof(netStatusParams) + sizeof(_timeElapsed);
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[UP] - Listen Server { Send Command at TimeStamp: %f. sizeof = %d bytes}"), _timeElapsed, dataSize), true, true, FColor::White, 1, "ListenServerSendCommand");
		}
	}
}


void UModularControllerComponent::AuthorityMoveComponent(float delta)
{
	StandAloneApplyStatus(ComputedControllerStatus, delta);
}


#pragma endregion

#pragma region Dedicated OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO


void UModularControllerComponent::DedicatedServerUpdateComponent(float delta)
{
	lastUpdatedControllerStatus = _clientRequestReceptionQueue.Num() > 0? _clientRequestReceptionQueue[0].Value : ApplyedControllerStatus;
	if(_clientRequestReceptionQueue.Num() > 0) _clientRequestReceptionQueue.RemoveAt(0);
	FControllerStatus initialState = ConsumeLastKinematicMove(lastUpdatedControllerStatus.MoveInput, delta);	
	initialState.Kinematics = UFunctionLibrary::LerpKinematic(initialState.Kinematics, lastUpdatedControllerStatus.Kinematics, delta * (UToolsLibrary::GetFPS(delta) * 0.5));

	if(bServerSideCosmetic)
	{
		const auto cosmeticsVars = initialState.StatusParams.StatusCosmeticVariables;
		initialState.StatusParams = lastUpdatedControllerStatus.StatusParams;
		initialState.StatusParams.StatusCosmeticVariables = cosmeticsVars;
	
		// Cosmetic Evaluation
		initialState = StandAloneCosmeticStatus(initialState, delta);
	}
	else
	{
		initialState.StatusParams = lastUpdatedControllerStatus.StatusParams;		
	}

	//TODO: Check correction
	
	ComputedControllerStatus = initialState;
	
	//multi-casting
	{
		MultiCastTime(_timeElapsed);
		FNetKinematic netKinematic;
		netKinematic.ExtractFromStatus(initialState);
		MultiCastKinematics(netKinematic);
		FNetStatusParam netStatusParams;
		netStatusParams.ExtractFromStatus(initialState);
		MultiCastStatusParams(netStatusParams);
	
		if (DebugType == EControllerDebugType::NetworkDebug)
		{
			int dataSize = sizeof(netKinematic) + sizeof(netStatusParams) + sizeof(_timeElapsed);
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[UP] - Dedicated Server { Send Command at TimeStamp: %f. sizeof = %d bytes}"), _timeElapsed, dataSize), true, true, FColor::Silver, 1, "DedicatedServerSendCommand");
		}
	}
}



#pragma endregion

#pragma endregion



#pragma region Client Logic ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UModularControllerComponent::ServerControllerStatus_Implementation(double timeStamp, FNetKinematic netkinematic, FNetStatusParam netStatusParam)
{
	if(waitingClientCorrectionACK >= 0 && waitingClientCorrectionACK != timeStamp)
	{
		if (DebugType == EControllerDebugType::NetworkDebug)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[DOWN] - Dedicated Server { Ignoring Recived Status: Waiting client correction acknowledgement}"))
				, true, true, FColor::Red, 1, "DedicatedServerReceiveCommand");
		}
		return;
	}
	
	_timeNetLatency = FMath::Abs(timeStamp - _timeElapsed);
	FControllerStatus cloneLastStatus = _clientRequestReceptionQueue.Num() > 0? _clientRequestReceptionQueue[_clientRequestReceptionQueue.Num() - 1].Value : ApplyedControllerStatus;
	netkinematic.RestoreOnToStatus(cloneLastStatus);
	netStatusParam.RestoreOnToStatus(cloneLastStatus);
	cloneLastStatus.Kinematics.LinearKinematic = cloneLastStatus.Kinematics.LinearKinematic.GetFinalCondition(GetNetLatency() * LatencyCompensationScale);
	_clientRequestReceptionQueue.Add(TTuple<double, FControllerStatus>(timeStamp, cloneLastStatus));

	if (DebugType == EControllerDebugType::NetworkDebug)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[DOWN] - Dedicated Server { Received Status with %fs latency}"), GetNetLatency()), true, true, FColor::Silver, 1, "DedicatedServerReceiveCommand");
	}
}

void UModularControllerComponent::ServerRequestStates_Implementation(UModularControllerComponent* caller)
{
	MultiCastStates(StateClasses, caller);
}

void UModularControllerComponent::ServerRequestActions_Implementation(UModularControllerComponent* caller)
{
	MultiCastActions(ActionClasses, caller);
}

#pragma region Automonous Proxy OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO


void UModularControllerComponent::AutonomousProxyUpdateComponent(float delta)
{
	const FVector moveInp = ConsumeMovementInput();
	const FControllerStatus initialState = ConsumeLastKinematicMove(moveInp, delta);
	const auto status = StandAloneEvaluateStatus(initialState, delta);
	ComputedControllerStatus = status;
	
	//Server-casting
	{
		FNetKinematic netKinematic;
		netKinematic.ExtractFromStatus(status);
		FNetStatusParam netStatusParams;
		netStatusParams.ExtractFromStatus(status);
		ServerControllerStatus(_timeElapsed, netKinematic, netStatusParams);
	
		if (DebugType == EControllerDebugType::NetworkDebug)
		{
			int dataSize = sizeof(netKinematic) + sizeof(netStatusParams) + sizeof(_timeElapsed);
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("[UP] - Autonomous { Send Command at TimeStamp: %f. sizeof = %d bytes}"), _timeElapsed, dataSize), true, true, FColor::Orange, 1, "AutonomousSendCommand");
		}
	}
}

#pragma endregion

#pragma region Simulated Proxy OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO


void UModularControllerComponent::SimulatedProxyComputeComponent(float delta)
{
	FControllerStatus initialState = ConsumeLastKinematicMove(lastUpdatedControllerStatus.MoveInput, delta);
	initialState.Kinematics = UFunctionLibrary::LerpKinematic(initialState.Kinematics, lastUpdatedControllerStatus.Kinematics, delta * (UToolsLibrary::GetFPS(delta) * 0.5));
	const auto cosmeticsVars = initialState.StatusParams.StatusCosmeticVariables;
	initialState.StatusParams = lastUpdatedControllerStatus.StatusParams;
	initialState.StatusParams.StatusCosmeticVariables = cosmeticsVars;
	
	// Cosmetic Evaluation
	initialState = StandAloneCosmeticStatus(initialState, delta);
	
	ComputedControllerStatus = initialState;
}


#pragma endregion

#pragma endregion


#pragma endregion

