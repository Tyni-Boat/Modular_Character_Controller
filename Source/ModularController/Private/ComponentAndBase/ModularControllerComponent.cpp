// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#include "ComponentAndBase/ModularControllerComponent.h"

#include <functional>
#include "CoreTypes.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Pawn.h"
#include "Engine.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"


#pragma region Core and Constructor XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


// Sets default values for this component's properties
UModularControllerComponent::UModularControllerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	SetIsReplicatedByDefault(true);
}


// Called when the game starts
void UModularControllerComponent::BeginPlay()
{
	GetOwner()->SetReplicateMovement(false);
	SetTickGroup(ETickingGroup::TG_PostPhysics);
	Super::BeginPlay();
	if (UpdatedPrimitive != nullptr)
	{
		UpdatedPrimitive->OnComponentHit.AddDynamic(this, &UModularControllerComponent::BeginCollision);
		UpdatedPrimitive->OnComponentBeginOverlap.AddDynamic(this, &UModularControllerComponent::BeginOverlap);
		OnCalculateCustomPhysics.BindUObject(this, &UModularControllerComponent::SubstepTick);
	}
	Initialize();
}


void UModularControllerComponent::Initialize()
{
	Velocity = FVector(0);
	_ownerPawn = Cast<APawn>(GetOwner());
	SetGravity(FVector::DownVector * FMath::Abs(GetGravityZ()));

	//Init collider
	if (UpdatedPrimitive)
	{
		UpdatedPrimitive->SetGenerateOverlapEvents(true);
		UpdatedPrimitive->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	//Inputs
	_inputPool = NewObject<UInputEntryPool>(UInputEntryPool::StaticClass(), UInputEntryPool::StaticClass());

	//State behaviors
	StatesInstances.Empty();
	{
		for (int i = StateClasses.Num() - 1; i >= 0; i--)
		{
			if (StateClasses[i] == nullptr)
				continue;
			UBaseControllerState* instance = NewObject<UBaseControllerState>(StateClasses[i], StateClasses[i]);
			StatesInstances.Add(instance);
		}
		StatesInstances.Sort([](UBaseControllerState& a, UBaseControllerState& b) { return a.GetPriority() > b.GetPriority(); });
	}

	//Action behaviors
	ActionInstances.Empty();
	{
		for (int i = ActionClasses.Num() - 1; i >= 0; i--)
		{
			if (ActionClasses[i] == nullptr)
				continue;
			UBaseControllerAction* instance = NewObject<UBaseControllerAction>(ActionClasses[i], ActionClasses[i]);
			instance->InitializeAction();
			ActionInstances.Add(instance);
		}
	}

	//Init last move
	ControllerStatus.Kinematics.LinearKinematic.Position = GetActorLocation();
	ControllerStatus.Kinematics.AngularKinematic.Orientation = GetRotation();

	//Set time elapsed
	auto timePassedSince = FDateTime::UtcNow() - FDateTime(2024, 01, 01, 0, 0, 0, 0);
	_timeElapsed = timePassedSince.GetTotalSeconds();
}


void UModularControllerComponent::MainUpdateComponent(float delta)
{
	APawn* pawn = _ownerPawn.Get();
	if (pawn == nullptr)
		return;

	if (GetNetMode() == ENetMode::NM_Standalone)
	{
		const FVector moveInp = ConsumeMovementInput();
		FControllerStatus initialState = ConsumeLastKinematicMove(moveInp);
		StandAloneUpdateComponent(initialState, _inputPool, delta);
	}
	else
	{
		ENetRole role = GetNetRole();
		switch (role)
		{
		case ENetRole::ROLE_Authority: {
			if (pawn->IsLocallyControlled())
			{
				ListenServerUpdateComponent(delta);
			}
			else
			{
				DedicatedServerUpdateComponent(delta);
			}
		}break;
		case ENetRole::ROLE_AutonomousProxy: {
			AutonomousProxyUpdateComponent(delta);
		}break;
		default: {
			SimulatedProxyUpdateComponent(delta);
		}break;
		}
	}
}


// Called every frame
void UModularControllerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (UpdatedPrimitive == nullptr)
		return;

	EvaluateRootMotions(DeltaTime);

	if (UpdatedPrimitive->IsSimulatingPhysics())
	{
		UpdatedPrimitive->GetBodyInstance()->AddCustomPhysics(OnCalculateCustomPhysics);
	}
	else
	{
		MainUpdateComponent(DeltaTime);
	}

	//Count time elapsed
	_timeElapsed += DeltaTime;
}



void UModularControllerComponent::StandAloneUpdateComponent(FControllerStatus initialState, UInputEntryPool* usedInputPool, float delta, bool noCollision)
{
	FControllerStatus processState = EvaluateControllerStatus(initialState, usedInputPool, delta);
	processState = ProcessStatus(processState, delta);

	processState = EvaluateRootMotionOverride(processState, delta);
	processState.Kinematics.AngularKinematic = HandleKinematicRotation(processState.Kinematics.AngularKinematic, delta);
	if (usedInputPool)
		usedInputPool->UpdateInputs(delta, DebugType == ControllerDebugType_InputDebug, this);

	//Evaluate
	FKinematicComponents finalKcomp = KinematicMoveEvaluation(processState, noCollision, delta);

	//Apply
	Move(finalKcomp, delta);

	//Conservation
	processState.Kinematics = finalKcomp;
	KinematicPostMove(processState, delta);
}



FControllerStatus UModularControllerComponent::EvaluateControllerStatus(const FControllerStatus initialStatus, UInputEntryPool* usedInputPool, const float delta, const FVector simulationDatas)
{
	//State
	FControllerStatus stateStatus = initialStatus;
	const int initialState = simulationDatas.Y >= 0 ? static_cast<int>(simulationDatas.Y) : stateStatus.ControllerStatus.StateIndex;
	const auto stateControllerStatus = CheckControllerStates(initialStatus, delta, simulationDatas);
	stateStatus.ControllerStatus.StateIndex = initialState;

	const auto swapCheckedStatus = TryChangeControllerState(stateControllerStatus, stateStatus, delta, simulationDatas.X > 0);
	stateStatus = swapCheckedStatus.ProcessResult;

	//Actions
	FControllerStatus actionStatus = stateStatus;
	const int initialActionIndex = simulationDatas.Z >= 0 ? static_cast<int>(simulationDatas.Z) : actionStatus.ControllerStatus.ActionIndex;
	const auto actionControllerStatus = CheckControllerActions(actionStatus, delta, simulationDatas);
	actionStatus.ControllerStatus.ActionIndex = initialActionIndex;

	const auto actionCheckedStatus = TryChangeControllerAction(actionControllerStatus, actionStatus, delta, simulationDatas.X > 0);
	actionStatus = actionCheckedStatus.ProcessResult;

	return actionStatus;
}



FControllerStatus UModularControllerComponent::ProcessStatus(const FControllerStatus initialState, const float inDelta)
{
	const FControllerStatus primaryMotion = ProcessControllerState(initialState, inDelta);
	const FControllerStatus alteredMotion = ProcessControllerAction(primaryMotion, inDelta);
	return alteredMotion;
}


#pragma endregion



#pragma region Input Handling XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


void UModularControllerComponent::MovementInput(FVector movement)
{
	FVector normalisationTester = movement;
	if (normalisationTester.Normalize())
		_userMoveDirectionHistory.Add(movement.GetClampedToMaxSize(1));
	else
		_userMoveDirectionHistory.Add(FVector(0));
}

void UModularControllerComponent::ListenInput(const FName key, const FInputEntry entry, const bool hold)
{
	if (_ownerPawn == nullptr)
		return;
	if (!_ownerPawn->IsLocallyControlled())
		return;
	if (_inputPool)
		_inputPool->AddOrReplace(key, entry, hold);
}

void UModularControllerComponent::ListenButtonInput(const FName key, const float buttonBufferTime, const bool hold)
{
	if (!key.IsValid())
		return;
	FInputEntry entry;
	entry.Nature = EInputEntryNature::InputEntryNature_Button;
	entry.Type = buttonBufferTime > 0 ? EInputEntryType::InputEntryType_Buffered : EInputEntryType::InputEntryType_Simple;
	entry.InputBuffer = buttonBufferTime;
	ListenInput(key, entry, hold);
}

void UModularControllerComponent::ListenValueInput(const FName key, const float value)
{
	if (!key.IsValid())
		return;
	FInputEntry entry;
	entry.Nature = EInputEntryNature::InputEntryNature_Value;
	entry.Axis.X = value;
	ListenInput(key, entry);
}

void UModularControllerComponent::ListenAxisInput(const FName key, const FVector axis)
{
	if (!key.IsValid())
		return;
	FInputEntry entry;
	entry.Nature = EInputEntryNature::InputEntryNature_Axis;
	entry.Axis = axis;
	ListenInput(key, entry);
}



FVector UModularControllerComponent::ConsumeMovementInput()
{
	if (_userMoveDirectionHistory.Num() < 2)
		return FVector(0);
	const FVector move = _userMoveDirectionHistory[0];
	_userMoveDirectionHistory.RemoveAt(0);
	if (DebugType == ControllerDebugType_MovementDebug)
	{
		FVector lookDir = move;
		if (lookDir.Normalize()) {
			UKismetSystemLibrary::DrawDebugArrow(this, GetLocation(), GetLocation() + lookDir * 100, 50, FColor::Silver, 0.017, 2);
		}
	}
	if (DebugType == ControllerDebugType_InputDebug)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Consumed Move Input: %s"), *move.ToCompactString()), true, true, FColor::Silver, 0, "MoveInput_");
	}
	return move;
}

FInputEntry UModularControllerComponent::ReadInput(const FName key)
{
	if (!_inputPool)
		return {};
	return _inputPool->ReadInput(key);
}

bool UModularControllerComponent::ReadButtonInput(const FName key)
{
	const FInputEntry entry = ReadInput(key);
	return entry.Phase == EInputEntryPhase::InputEntryPhase_Held || entry.Phase == EInputEntryPhase::InputEntryPhase_Pressed;
}

float UModularControllerComponent::ReadValueInput(const FName key)
{
	const FInputEntry entry = ReadInput(key);
	return entry.Axis.X;
}

FVector UModularControllerComponent::ReadAxisInput(const FName key)
{
	const FInputEntry entry = ReadInput(key);
	return entry.Axis;
}

#pragma endregion



#pragma region Network Logic XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

#pragma region Common Logic

void UModularControllerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);


	//DOREPLIFETIME(UModularControllerComponent, LastMoveMade);
	//DOREPLIFETIME(UModularControllerComponent, ActionInstances);
}


ENetRole UModularControllerComponent::GetNetRole()
{
	if (GetOwner()->HasAuthority())
	{
		return ENetRole::ROLE_Authority;
	}
	else if (Cast<APawn>(GetOwner()) && Cast<APawn>(GetOwner())->IsLocallyControlled())
	{
		return ENetRole::ROLE_AutonomousProxy;
	}

	return ENetRole::ROLE_SimulatedProxy;
}


FName UModularControllerComponent::GetNetRoleDebug(ENetRole role)
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

#pragma endregion



#pragma region Server Logic //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UModularControllerComponent::MultiCastMoveCommand_Implementation()
{
	const ENetRole role = GetNetRole();
	//switch (role)
	//{
	//case ENetRole::ROLE_Authority:
	//{
	//} break;
	//case ENetRole::ROLE_AutonomousProxy:
	//{
	//	if (asCorrection)
	//	{
	//		_lastCorrectionReceived = Correction;
	//		if (DebugType == ControllerDebugType_NetworkDebug)
	//		{
	//			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Autonomous Proxy Received Correction Stamped: %f"), Correction.TimeStamp), true, true, FColor::Orange, 5, TEXT("MultiCastMoveCommand_1"));
	//		}
	//	}

	//	_lastCmdReceived = command;
	//}
	//break;

	//default:
	//{
	//	_lastCmdReceived = command;
	//	if (DebugType == ControllerDebugType_NetworkDebug)
	//	{
	//		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Simulated Proxy Received Command Stamped: %f"), command.TimeStamp), true, true, FColor::Cyan, 1, TEXT("MultiCastMoveCommand_2"));
	//	}
	//}
	//break;
	//}
}


void UModularControllerComponent::MultiCastStates_Implementation(const TArray<TSubclassOf<UBaseControllerState>>& states, UModularControllerComponent* caller)
{
	if (caller != this)
		return;
	StatesInstances.Empty();
	for (int i = 0; i < states.Num(); i++)
	{
		if (states[i] == nullptr)
			continue;
		UBaseControllerState* instance = NewObject<UBaseControllerState>(states[i], states[i]);
		StatesInstances.Add(instance);
	}

	if (StatesInstances.Num() > 0)
		StatesInstances.Sort([](UBaseControllerState& a, UBaseControllerState& b) { return a.GetPriority() > b.GetPriority(); });
}

void UModularControllerComponent::MultiCastActions_Implementation(const TArray<TSubclassOf<UBaseControllerAction>>& actions, UModularControllerComponent* caller)
{
	if (caller != this)
		return;
	ActionInstances.Empty();
	for (int i = 0; i < actions.Num(); i++)
	{
		if (actions[i] == nullptr)
			continue;
		UBaseControllerAction* instance = NewObject<UBaseControllerAction>(actions[i], actions[i]);
		instance->InitializeAction();
	}

	if (ActionInstances.Num() > 0)
		ActionInstances.Sort([](UBaseControllerAction& a, UBaseControllerAction& b) { return a.GetPriority() > b.GetPriority(); });
}

#pragma region Listened OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO


void UModularControllerComponent::ListenServerUpdateComponent(float delta)
{
	//const FVector moveInp = ConsumeMovementInput();
	//FKinematicInfos movement = FKinematicInfos(moveInp, GetGravity(), LastMoveMade, GetMass());

	////Execute the move
	//movement.bUsePhysic = bUsePhysicAuthority;
	//auto statusInfos = EvaluateControllerStatus(movement, moveInp, _inputPool, delta);
	////
	//{
	//	FVelocity alteredMotion = ProcessStatus(statusInfos, movement, moveInp, _inputPool, delta);

	//	EvaluateRootMotionOverride(alteredMotion, movement, delta);
	//	alteredMotion.Rotation = HandleRotation(alteredMotion, movement, delta);
	//	if (_inputPool)
	//		_inputPool->UpdateInputs(delta);

	//	FVelocity resultingMove = EvaluateMove(movement, alteredMotion, delta);
	//	resultingMove._rooMotionScale = alteredMotion._rooMotionScale;
	//	PostMoveUpdate(movement, resultingMove, CurrentStateIndex, delta);
	//	//Move(movement.FinalTransform.GetLocation(), movement.FinalTransform.GetRotation(), delta);

	//	movement.FinalTransform.SetComponents(UpdatedPrimitive->GetComponentRotation().Quaternion(), UpdatedPrimitive->GetComponentLocation(), UpdatedPrimitive->GetComponentScale());
	//}
	//LastMoveMade = movement;
	//auto moveCmd = FClientNetMoveCommand(_timeElapsed, delta, moveInp, LastMoveMade, statusInfos);

	//if (_lastCmdReceived.HasChanged(moveCmd, 1, 5) || !_startPositionSet)
	//{
	//	_startPositionSet = true;
	//	_lastCmdReceived = moveCmd;
	//	MultiCastMoveCommand(moveCmd);
	//	if (DebugType == ControllerDebugType_NetworkDebug)
	//	{
	//		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Listen Send Command Stamped: %f"), moveCmd.TimeStamp), true, true, FColor::White, 1, TEXT("ListenServerUpdateComponent"));
	//	}
	//}
}


#pragma endregion

#pragma region Dedicated OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO


void UModularControllerComponent::DedicatedServerUpdateComponent(float delta)
{
	//FHitResult initialChk;
	//bool madeCorrection = false;
	//bool ackCorrection = false;

	////Verification
	//{
	//	if (_servercmdCheckPool.Num() > 0)
	//	{
	//		if (bUseClientAuthorative)
	//		{
	//			_lastCmdReceived = _servercmdCheckPool[0];
	//			_servercmdCheckPool.RemoveAt(0);
	//		}
	//		else
	//		{
	//			_lastCmdReceived = _servercmdCheckPool[0];
	//			if (ComponentTraceCastSingle_Internal(initialChk, UpdatedComponent->GetComponentLocation(), _lastCmdReceived.FromLocation - UpdatedComponent->GetComponentLocation(), _lastCmdReceived.FromRotation.Quaternion()))
	//			{
	//				madeCorrection = true;
	//			}
	//			else if (ComponentTraceCastSingle_Internal(initialChk, _lastCmdReceived.FromLocation, _lastCmdReceived.ToLocation - _lastCmdReceived.FromLocation, _lastCmdReceived.ToRotation.Quaternion()))
	//			{
	//				madeCorrection = true;
	//			}

	//			if (madeCorrection)
	//			{
	//				_lastCmdReceived.FromLocation = initialChk.TraceStart;
	//				_lastCmdReceived.ToLocation = initialChk.Location;
	//			}
	//			else
	//			{
	//				ackCorrection = _lastCmdReceived.CorrectionAckowledgement;
	//			}

	//			_servercmdCheckPool.RemoveAt(0);
	//		}
	//	}
	//}


	//FKinematicInfos movement = FKinematicInfos(_lastCmdReceived.userMoveInput, GetGravity(), LastMoveMade, GetMass());

	////Move
	//FVector currentLocation = UpdatedPrimitive->GetComponentLocation();
	//FVector targetLocation = _lastCmdReceived.ToLocation;
	//FVector lerpLocation = FMath::Lerp(currentLocation, targetLocation, delta * AdjustmentSpeed);
	//FHitResult sweepHit;
	//UpdatedPrimitive->SetWorldLocation(lerpLocation, true, &sweepHit);
	//movement.InitialTransform.SetLocation(UpdatedPrimitive->GetComponentLocation());

	////Rotate
	//FQuat currentRotation = UpdatedPrimitive->GetComponentRotation().Quaternion();
	//FQuat targetRotation = _lastCmdReceived.ToRotation.Quaternion();
	//FQuat slerpRot = FQuat::Slerp(currentRotation, targetRotation, delta * AdjustmentSpeed);
	//UpdatedPrimitive->SetWorldRotation(slerpRot.Rotator());
	//movement.InitialTransform.SetRotation(currentRotation);
	//movement.FinalVelocities.Rotation = targetRotation;

	////Velocity
	//movement.FinalVelocities.ConstantLinearVelocity = _lastCmdReceived.WithVelocity;

	////Status
	//EvaluateControllerStatus(movement, _lastCmdReceived.userMoveInput, _inputPool, delta, _lastCmdReceived.ControllerStatus);
	//auto copyOfStatus = _lastCmdReceived.ControllerStatus;
	//ProcessStatus(copyOfStatus, movement, _lastCmdReceived.userMoveInput, _inputPool, delta);

	//PostMoveUpdate(movement, movement.FinalVelocities, _lastCmdReceived.ControllerStatus.StateIndex, delta);
	//LastMoveMade = movement;

	////Network
	//if (_lastCmdExecuted.HasChanged(_lastCmdReceived, 1, 5) || !_startPositionSet)
	//{
	//	//Set a time stamp to be able to initialize client
	//	if (!_startPositionSet)
	//		_lastCmdReceived.TimeStamp = _timeElapsed;

	//	_lastCmdExecuted = _lastCmdReceived;
	//	if (madeCorrection || ackCorrection)
	//	{
	//		auto hitResult = initialChk.IsValidBlockingHit() ? initialChk : sweepHit;
	//		FServerNetCorrectionData correction = FServerNetCorrectionData(ackCorrection ? 0 : _lastCmdReceived.TimeStamp, LastMoveMade, &hitResult);
	//		MultiCastMoveCommand(_lastCmdReceived, correction, true);
	//	}
	//	else
	//	{
	//		MultiCastMoveCommand(_lastCmdReceived);
	//	}

	//	if (DebugType == ControllerDebugType_NetworkDebug)
	//	{
	//		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Dedicated Send Command Stamped: %f as correction? %d"), _lastCmdReceived.TimeStamp, madeCorrection), true, true, FColor::White, 1, TEXT("DedicatedServerUpdateComponent"));
	//	}
	//}
}


#pragma endregion

#pragma endregion



#pragma region Client Logic ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UModularControllerComponent::ServerRequestStates_Implementation(UModularControllerComponent* caller)
{
	MultiCastStates(StateClasses, caller);
}

void UModularControllerComponent::ServerRequestActions_Implementation(UModularControllerComponent* caller)
{
	MultiCastActions(ActionClasses, caller);
}

#pragma region Automonous Proxy OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO


void UModularControllerComponent::ServerCastMoveCommand_Implementation()
{
	if (bUseClientAuthorative)
	{
		MultiCastMoveCommand();
	}

	if (DebugType == ControllerDebugType_NetworkDebug)
	{
		//UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Dedicated received command Stamped: %f"), command.TimeStamp), true, true, FColor::Black, 1, TEXT("ServerCastMoveCommand"));
	}
}


void UModularControllerComponent::AutonomousProxyUpdateComponent(float delta)
{
	////Handle Starting Location
	//{
	//	if (_lastCmdReceived.TimeStamp == 0 && !_startPositionSet)
	//		return;
	//	if (!_startPositionSet)
	//	{
	//		UpdatedComponent->SetWorldLocationAndRotation(_lastCmdReceived.ToLocation, _lastCmdReceived.ToRotation);
	//		LastMoveMade.FinalTransform.SetComponents(_lastCmdReceived.ToRotation.Quaternion(), _lastCmdReceived.ToLocation, LastMoveMade.FinalTransform.GetScale3D());
	//		_startPositionSet = true;
	//	}
	//}


	//////Correction
	//bool corrected = false;
	//if (!bUseClientAuthorative)
	//{
	//	FClientNetMoveCommand cmdBefore;
	//	if (_clientcmdHistory.Num() > 0)
	//	{
	//		cmdBefore = _clientcmdHistory[_clientcmdHistory.Num() - 1];
	//	}
	//	FClientNetMoveCommand correctionCmd = cmdBefore;
	//	if (_lastCorrectionReceived.ApplyCorrectionRecursive(_clientcmdHistory, correctionCmd))
	//	{
	//		if (cmdBefore.HasChanged(correctionCmd))
	//		{
	//			corrected = true;

	//			LastMoveMade.FinalTransform.SetComponents(correctionCmd.ToRotation.Quaternion(), correctionCmd.ToLocation, FVector::OneVector);
	//			LastMoveMade.FinalVelocities.ConstantLinearVelocity = correctionCmd.ToVelocity;
	//			LastMoveMade.FinalVelocities.Rotation = LastMoveMade.InitialTransform.GetRotation();

	//			if (DebugType == ControllerDebugType_NetworkDebug)
	//			{
	//				UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Autonomous Set Correction to Stamped: %f"), _lastCorrectionReceived.TimeStamp), true, true, FColor::Orange, 1, TEXT("AutonomousProxyUpdateComponent_correction_1"));
	//			}
	//		}
	//	}

	//	if (DebugType == ControllerDebugType_NetworkDebug)
	//	{
	//		DrawDebugCapsule(GetWorld(), correctionCmd.FromLocation, 90, 40, correctionCmd.FromRotation.Quaternion(), FColor::Orange, false, 1);
	//		DrawDebugDirectionalArrow(GetWorld(), correctionCmd.FromLocation, correctionCmd.FromLocation + correctionCmd.FromRotation.Vector() * 40, 20, FColor::Red, false, -1);

	//		auto acceleration = correctionCmd.ToVelocity.Length() - correctionCmd.WithVelocity.Length();
	//		FLinearColor accColor = FLinearColor(-acceleration, acceleration, 0);
	//		DrawDebugLine(GetWorld(), correctionCmd.FromLocation + FVector::UpVector * 15, correctionCmd.ToLocation + FVector::UpVector * 15, accColor.ToFColor(true), false, -1);
	//	}
	//}


	//const FVector moveInp = ConsumeMovementInput();
	//FKinematicInfos movement = FKinematicInfos(moveInp, GetGravity(), LastMoveMade, GetMass());

	////Execute the move
	//movement.bUsePhysic = bUsePhysicAuthority;
	//auto statusInfos = EvaluateControllerStatus(movement, moveInp, _inputPool, delta);
	////
	//{
	//	FVelocity alteredMotion = ProcessStatus(statusInfos, movement, moveInp, _inputPool, delta);

	//	EvaluateRootMotionOverride(alteredMotion, movement, delta);
	//	alteredMotion.Rotation = HandleRotation(alteredMotion, movement, delta);
	//	if (_inputPool)
	//		_inputPool->UpdateInputs(delta);

	//	FVelocity resultingMove = EvaluateMove(movement, alteredMotion, delta);
	//	if (_lastCorrectionReceived.CollisionOccured)
	//	{
	//		const bool tryMoveThroughObstacle_Linear = FVector::DotProduct(resultingMove.ConstantLinearVelocity, _lastCorrectionReceived.CollisionNormal) <= 0;
	//		if (tryMoveThroughObstacle_Linear)
	//			resultingMove.ConstantLinearVelocity = FVector::VectorPlaneProject(resultingMove.ConstantLinearVelocity, _lastCorrectionReceived.CollisionNormal);

	//		const bool tryMoveThroughObstacle_Instant = FVector::DotProduct(resultingMove.InstantLinearVelocity, _lastCorrectionReceived.CollisionNormal) <= 0;
	//		if (tryMoveThroughObstacle_Instant)
	//			resultingMove.InstantLinearVelocity = FVector::VectorPlaneProject(resultingMove.InstantLinearVelocity, _lastCorrectionReceived.CollisionNormal);
	//	}
	//	resultingMove._rooMotionScale = alteredMotion._rooMotionScale;
	//	PostMoveUpdate(movement, resultingMove, CurrentStateIndex, delta);

	//	if (bUseClientAuthorative)
	//	{
	//		//Move(movement.FinalTransform.GetLocation(), movement.FinalTransform.GetRotation(), delta);
	//		movement.FinalTransform.SetComponents(UpdatedPrimitive->GetComponentRotation().Quaternion(), UpdatedPrimitive->GetComponentLocation()
	//			, UpdatedPrimitive->GetComponentScale());
	//	}
	//	else
	//	{
	//		const FVector lerpPos = FMath::Lerp(UpdatedComponent->GetComponentLocation(), movement.FinalTransform.GetLocation(), delta * AdjustmentSpeed);
	//		const FQuat slerpRot = FQuat::Slerp(UpdatedComponent->GetComponentQuat(), movement.FinalTransform.GetRotation(), delta * AdjustmentSpeed);
	//		UpdatedComponent->SetWorldLocationAndRotation(lerpPos, slerpRot);
	//	}
	//}
	//LastMoveMade = movement;
	//auto moveCmd = FClientNetMoveCommand(_timeElapsed, delta, moveInp, LastMoveMade, statusInfos);

	////Changes and Network
	//if (_lastCmdExecuted.HasChanged(moveCmd, 1, 5))
	//{
	//	_lastCmdExecuted = moveCmd;
	//	_clientcmdHistory.Add(moveCmd);
	//	if (!corrected)
	//	{
	//		moveCmd.CorrectionAckowledgement = _lastCorrectionReceived.TimeStamp != 0;
	//		ServerCastMoveCommand(moveCmd);
	//		if (DebugType == ControllerDebugType_NetworkDebug)
	//		{
	//			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Autonomous Send Command Stamped: %f"), moveCmd.TimeStamp), true, true, FColor::Orange, 1, TEXT("AutonomousProxyUpdateComponent"));
	//		}
	//	}
	//}
}

#pragma endregion

#pragma region Simulated Proxy OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO


void UModularControllerComponent::SimulatedProxyUpdateComponent(float delta)
{
	//FKinematicInfos movement = FKinematicInfos(_lastCmdReceived.userMoveInput, GetGravity(), LastMoveMade, GetMass());

	////Move
	//FVector currentLocation = UpdatedPrimitive->GetComponentLocation();
	//FVector targetLocation = _lastCmdReceived.ToLocation;
	//FVector lerpLocation = FMath::Lerp(currentLocation, targetLocation, delta * AdjustmentSpeed);
	//UpdatedPrimitive->SetWorldLocation(lerpLocation);
	//movement.InitialTransform.SetLocation(currentLocation);

	////Rotate
	//FQuat currentRotation = UpdatedPrimitive->GetComponentRotation().Quaternion();
	//FQuat targetRotation = _lastCmdReceived.ToRotation.Quaternion();
	//FQuat slerpRot = FQuat::Slerp(currentRotation, targetRotation, delta * AdjustmentSpeed);
	//UpdatedPrimitive->SetWorldRotation(slerpRot.Rotator());
	//movement.InitialTransform.SetRotation(currentRotation);
	//movement.FinalVelocities.Rotation = targetRotation;

	////Velocity
	//movement.FinalVelocities.ConstantLinearVelocity = _lastCmdReceived.WithVelocity;

	////Status
	//EvaluateControllerStatus(movement, _lastCmdReceived.userMoveInput, _inputPool, delta, _lastCmdReceived.ControllerStatus);
	//auto copyOfStatus = _lastCmdReceived.ControllerStatus;
	//ProcessStatus(copyOfStatus, movement, _lastCmdReceived.userMoveInput, _inputPool, delta);

	//PostMoveUpdate(movement, movement.FinalVelocities, _lastCmdReceived.ControllerStatus.StateIndex, delta);
	//LastMoveMade = movement;
}


#pragma endregion

#pragma endregion


#pragma endregion



#pragma region Physic


void UModularControllerComponent::SubstepTick(float DeltaTime, FBodyInstance* BodyInstance)
{
	if (UpdatedPrimitive != nullptr)
	{
		MainUpdateComponent(DeltaTime);
	}
}


void UModularControllerComponent::BeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	////overlap objects
	if (OverlappedComponent != nullptr && OtherComp != nullptr && OtherActor != nullptr)
	{
		if (DebugType == ControllerDebugType_PhysicDebug)
		{
			GEngine->AddOnScreenDebugMessage((int32)GetOwner()->GetUniqueID() + 9, 1, FColor::Green, FString::Printf(TEXT("Overlaped With: %s"), *OtherActor->GetActorNameOrLabel()));
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Overlap with: (%s)"), *OtherActor->GetActorNameOrLabel()), true, true, FColor::Green, 0, "OverlapEvent");
		}
	}
}


void UModularControllerComponent::BeginCollision(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor != nullptr && DebugType == ControllerDebugType_PhysicDebug)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Colliding with: (%s)"), *OtherActor->GetActorNameOrLabel()), true, true, FColor::Green, 0, "CollisionImpact");
	}

	if (OtherComp)
	{
		UModularControllerComponent* otherModularComponent = nullptr;
		if (OtherActor != nullptr)
		{
			auto component = OtherActor->GetComponentByClass(UModularControllerComponent::StaticClass());
			if (component != nullptr)
			{
				otherModularComponent = Cast<UModularControllerComponent>(component);
			}
		}

		if (OtherComp->IsSimulatingPhysics())
		{
			//AddForce(OtherComp->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint, Hit.BoneName) * OtherComp->GetMass());
		}
		else if (otherModularComponent != nullptr)
		{
			//AddForce(otherModularComponent->ControllerStatus.LinearKinematic.Velocity * otherModularComponent->GetMass());
		}
	}
}



FSurfaceInfos UModularControllerComponent::GetCurrentSurface() const
{
	return  ControllerStatus.ControllerSurface;
}



void UModularControllerComponent::AddForce(FVector force, bool massIndependant)
{
	FVector acceleration = massIndependant ? force : force / GetMass();
	_collisionAccelerations += acceleration;
}


void UModularControllerComponent::AddMomentumOnHit(const FHitResult hit, FVector momentum)
{
	if (!hit.GetActor())
		return;
	if (hit.Component == nullptr)
		return;

	//BeginCollision(UpdatedPrimitive, hit.GetActor(), hit.GetComponent(), hit.Normal, hit);

	UModularControllerComponent* otherModularComponent = nullptr;
	const auto component = hit.GetActor()->GetComponentByClass(UModularControllerComponent::StaticClass());
	if (component != nullptr)
	{
		otherModularComponent = Cast<UModularControllerComponent>(component);
	}

	if (hit.Component->IsSimulatingPhysics())
	{
		hit.Component->WakeRigidBody(hit.BoneName);
		hit.Component->AddForceAtLocation(momentum, hit.ImpactPoint, hit.BoneName);
	}
	else if (otherModularComponent != nullptr)
	{
		otherModularComponent->AddForce(momentum);
	}
}


#pragma endregion



#pragma region All Behaviours


void UModularControllerComponent::SetOverrideRootMotionMode(USkeletalMeshComponent* caller, const ERootMotionType translationMode, const ERootMotionType rotationMode)
{
	if (_overrideRootMotionCommands.Contains(caller))
	{
		_overrideRootMotionCommands[caller].OverrideTranslationRootMotionMode = translationMode;
		_overrideRootMotionCommands[caller].OverrideRotationRootMotionMode = rotationMode;
		_overrideRootMotionCommands[caller].OverrideRootMotionChrono = 0.15f;
	}
	else
	{
		_overrideRootMotionCommands.Add(caller, FOverrideRootMotionCommand(translationMode, rotationMode, 0.15f));
	}
}

#pragma endregion



#pragma region States


bool UModularControllerComponent::CheckControllerStateByType(TSubclassOf<UBaseControllerState> moduleType)
{
	if (StatesInstances.Num() <= 0)
		return false;
	auto index = StatesInstances.IndexOfByPredicate([moduleType](UBaseControllerState* state) -> bool { return state->GetClass() == moduleType; });
	return StatesInstances.IsValidIndex(index);
}

bool UModularControllerComponent::CheckControllerStateByName(FName moduleName)
{
	if (StatesInstances.Num() <= 0)
		return false;
	auto index = StatesInstances.IndexOfByPredicate([moduleName](UBaseControllerState* state) -> bool { return state->GetDescriptionName() == moduleName; });
	return StatesInstances.IsValidIndex(index);
}

bool UModularControllerComponent::CheckControllerStateByPriority(int modulePriority)
{
	if (StatesInstances.Num() <= 0)
		return false;
	auto index = StatesInstances.IndexOfByPredicate([modulePriority](UBaseControllerState* state) -> bool { return state->GetPriority() == modulePriority; });
	return StatesInstances.IsValidIndex(index);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UModularControllerComponent::AddControllerState_Implementation(TSubclassOf<UBaseControllerState> moduleType)
{
	if (moduleType == nullptr)
		return;
	if (CheckControllerStateByType(moduleType))
		return;
	UBaseControllerState* instance = NewObject<UBaseControllerState>(moduleType, moduleType);
	StatesInstances.Add(instance);
	StatesInstances.Sort([](UBaseControllerState& a, UBaseControllerState& b) { return a.GetPriority() > b.GetPriority(); });
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UBaseControllerState* UModularControllerComponent::GetControllerStateByType(TSubclassOf<UBaseControllerState> moduleType)
{
	if (StatesInstances.Num() <= 0)
		return nullptr;
	auto index = StatesInstances.IndexOfByPredicate([moduleType](UBaseControllerState* state) -> bool { return state->GetClass() == moduleType; });
	if (StatesInstances.IsValidIndex(index))
	{
		return StatesInstances[index];
	}
	return nullptr;
}

UBaseControllerState* UModularControllerComponent::GetControllerStateByName(FName moduleName)
{
	if (StatesInstances.Num() <= 0)
		return nullptr;
	auto index = StatesInstances.IndexOfByPredicate([moduleName](UBaseControllerState* state) -> bool { return state->GetDescriptionName() == moduleName; });
	if (StatesInstances.IsValidIndex(index))
		return StatesInstances[index];
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UModularControllerComponent::RemoveControllerStateByType_Implementation(TSubclassOf<UBaseControllerState> moduleType)
{
	if (CheckControllerStateByType(moduleType))
	{
		auto behaviour = StatesInstances.FindByPredicate([moduleType](UBaseControllerState* state) -> bool { return state->GetClass() == moduleType->GetClass(); });
		StatesInstances.Remove(*behaviour);
		if (StatesInstances.Num() > 0)
			StatesInstances.Sort([](UBaseControllerState& a, UBaseControllerState& b) { return a.GetPriority() > b.GetPriority(); });
		return;
	}
}

void UModularControllerComponent::RemoveControllerStateByName_Implementation(FName moduleName)
{
	if (CheckControllerStateByName(moduleName))
	{
		auto behaviour = StatesInstances.FindByPredicate([moduleName](UBaseControllerState* state) -> bool { return state->GetDescriptionName() == moduleName; });
		StatesInstances.Remove(*behaviour);
		if (StatesInstances.Num() > 0)
			StatesInstances.Sort([](UBaseControllerState& a, UBaseControllerState& b) { return a.GetPriority() > b.GetPriority(); });
		return;
	}
}

void UModularControllerComponent::RemoveControllerStateByPriority_Implementation(int modulePriority)
{
	if (CheckControllerStateByPriority(modulePriority))
	{
		auto behaviour = StatesInstances.FindByPredicate([modulePriority](UBaseControllerState* state) -> bool { return state->GetPriority() == modulePriority; });
		StatesInstances.Remove(*behaviour);
		if (StatesInstances.Num() > 0)
			StatesInstances.Sort([](UBaseControllerState& a, UBaseControllerState& b) { return a.GetPriority() > b.GetPriority(); });
		return;
	}
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


FControllerStatus UModularControllerComponent::CheckControllerStates(FControllerStatus currentControllerStatus, const float inDelta, FVector simulationDatas)
{
	FControllerStatus endStatus = currentControllerStatus;
	FControllerStatus selectedStatus = endStatus;
	int maxStatePriority = -1;
	int selectedStateIndex = -1;
	bool overrideNewState = false;

	//Check if a State's check have success state
	{
		//Check if a valid action freeze the current state
		if (selectedStateIndex < 0)
		{
			int activeActionIndex = endStatus.ControllerStatus.ActionIndex;
			if (simulationDatas.Z >= 0)
			{
				activeActionIndex = static_cast<int>(simulationDatas.Z);
			}

			if (ActionInstances.IsValidIndex(activeActionIndex) && ActionInstances[activeActionIndex])
			{
				//Find state freeze
				if (ActionInstances[activeActionIndex]->bFreezeCurrentState)
				{
					selectedStateIndex = simulationDatas.Y < 0 ? endStatus.ControllerStatus.StateIndex : static_cast<int>(simulationDatas.Y);
				}

				//Find last frame status voider
				if (ActionInstances[activeActionIndex]->bShouldControllerStateCheckOverride)
				{
					overrideNewState = true;
				}
			}
		}

		if (selectedStateIndex < 0)
		{
			for (int i = 0; i < StatesInstances.Num(); i++)
			{
				if (StatesInstances[i] == nullptr)
					continue;

				StatesInstances[i]->SurfaceInfos.ReleaseLock();

				//Don't event check lower priorities
				if (StatesInstances[i]->GetPriority() < maxStatePriority)
				{
					//Reset the surface if it's not even checked.
					StatesInstances[i]->SurfaceInfos.Reset();
					continue;
				}

				//Handle state snapshot
				if (simulationDatas.X > 0)
					StatesInstances[i]->SaveStateSnapShot();
				else
					StatesInstances[i]->RestoreStateFromSnapShot();

				auto checkResult = StatesInstances[i]->CheckState(this, endStatus, inDelta, overrideNewState ? false : endStatus.ControllerStatus.StateIndex == i);
				if (checkResult.CheckedCondition)
				{
					selectedStateIndex = i;
					selectedStatus = checkResult.ProcessResult;
					maxStatePriority = StatesInstances[i]->GetPriority();
				}
			}
		}

	}

	endStatus = selectedStatus;
	endStatus.ControllerStatus.StateIndex = selectedStateIndex;
	return endStatus;
}


FControllerCheckResult UModularControllerComponent::TryChangeControllerState(FControllerStatus ToStateStatus, FControllerStatus fromStateStatus, const float inDelta, bool simulate)
{
	FControllerCheckResult result = FControllerCheckResult(false, ToStateStatus);
	int fromIndex = fromStateStatus.ControllerStatus.StateIndex;
	int toIndex = ToStateStatus.ControllerStatus.StateIndex;

	if (toIndex == fromIndex)
	{
		result = FControllerCheckResult(false, fromStateStatus);
		return result;
	}

	if (!StatesInstances.IsValidIndex(toIndex))
	{
		result = FControllerCheckResult(false, fromStateStatus);
		return result;
	}

	result.CheckedCondition = true;

	if (StatesInstances.IsValidIndex(fromIndex))
	{
		//Leaving
		result.ProcessResult.Kinematics = StatesInstances[fromIndex]->OnExitState(this, result.ProcessResult.Kinematics, result.ProcessResult.MoveInput, inDelta);
		StatesInstances[fromIndex]->SurfaceInfos.Reset();
	}

	//Landing
	result.ProcessResult.Kinematics = StatesInstances[toIndex]->OnEnterState(this, result.ProcessResult.Kinematics, result.ProcessResult.MoveInput, inDelta);
	if (!simulate)
		LinkAnimBlueprint(GetSkeletalMesh(), "State", StatesInstances[toIndex]->StateBlueprintClass);

	if (!simulate)
	{
		//Reset the time spend on state
		TimeOnCurrentState = 0;

		//Notify other states
		for (int i = 0; i < StatesInstances.Num(); i++)
		{
			if (i == toIndex)
				continue;

			StatesInstances[i]->OnControllerStateChanged(this, StatesInstances.IsValidIndex(toIndex) ? StatesInstances[toIndex]->GetDescriptionName() : "", StatesInstances.IsValidIndex(toIndex) ? StatesInstances[toIndex]->GetPriority() : -1);
		}

		//Notify the controller
		OnControllerStateChanged(StatesInstances.IsValidIndex(toIndex) ? StatesInstances[toIndex] : nullptr
			, StatesInstances.IsValidIndex(fromIndex) ? StatesInstances[fromIndex] : nullptr);
		OnControllerStateChangedEvent.Broadcast(StatesInstances.IsValidIndex(toIndex) ? StatesInstances[toIndex] : nullptr
			, StatesInstances.IsValidIndex(fromIndex) ? StatesInstances[fromIndex] : nullptr);

		//Notify actions the change of state
		for (int i = 0; i < ActionInstances.Num(); i++)
		{
			ActionInstances[i]->OnControllerStateChanged(this, StatesInstances.IsValidIndex(toIndex) ? StatesInstances[toIndex]->GetDescriptionName() : "", StatesInstances.IsValidIndex(toIndex) ? StatesInstances[toIndex]->GetPriority() : -1);
		}
	}

	return result;
}


FControllerStatus UModularControllerComponent::ProcessControllerState(const FControllerStatus initialState, const float inDelta, bool simulate)
{
	FControllerStatus processMotion = initialState;
	const int index = initialState.ControllerStatus.StateIndex;

	if (StatesInstances.IsValidIndex(index))
	{
		//Handle state snapshot
		if (simulate)
		{
			StatesInstances[index]->SaveStateSnapShot();
		}
		else
		{
			StatesInstances[index]->RestoreStateFromSnapShot();
			TimeOnCurrentState += inDelta;
		}

		processMotion = StatesInstances[index]->ProcessState(this, initialState, inDelta);

		//Handle surface
		processMotion.ControllerSurface = StatesInstances[index]->SurfaceInfos;
	}

	return processMotion;
}


void UModularControllerComponent::OnControllerStateChanged_Implementation(UBaseControllerState* newState, UBaseControllerState* oldState)
{
}


#pragma endregion



#pragma region Actions


bool UModularControllerComponent::CheckActionBehaviourByType(TSubclassOf<UBaseControllerAction> moduleType)
{
	if (ActionInstances.Num() <= 0)
		return false;
	auto index = ActionInstances.IndexOfByPredicate([moduleType](UBaseControllerAction* action) -> bool { return action->GetClass() == moduleType; });
	return ActionInstances.IsValidIndex(index);
}

bool UModularControllerComponent::CheckActionBehaviourByName(FName moduleName)
{
	if (ActionInstances.Num() <= 0)
		return false;
	auto index = ActionInstances.IndexOfByPredicate([moduleName](UBaseControllerAction* action) -> bool { return action->GetDescriptionName() == moduleName; });
	return ActionInstances.IsValidIndex(index);
}

bool UModularControllerComponent::CheckActionBehaviourByPriority(int modulePriority)
{
	if (ActionInstances.Num() <= 0)
		return false;
	auto index = ActionInstances.IndexOfByPredicate([modulePriority](UBaseControllerAction* action) -> bool { return action->GetPriority() == modulePriority; });
	return ActionInstances.IsValidIndex(index);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UModularControllerComponent::AddControllerAction_Implementation(TSubclassOf<UBaseControllerAction> moduleType)
{
	if (moduleType == nullptr)
		return;
	if (CheckActionBehaviourByType(moduleType))
		return;
	UBaseControllerAction* instance = NewObject<UBaseControllerAction>(moduleType, moduleType);
	instance->InitializeAction();
	ActionInstances.Add(instance);
	ActionInstances.Sort([](UBaseControllerAction& a, UBaseControllerAction& b) { return a.GetPriority() > b.GetPriority(); });
}


UBaseControllerAction* UModularControllerComponent::GetActionByType(TSubclassOf<UBaseControllerAction> moduleType)
{
	if (ActionInstances.Num() <= 0)
		return nullptr;
	auto index = ActionInstances.IndexOfByPredicate([moduleType](UBaseControllerAction* action) -> bool { return action->GetClass() == moduleType; });
	if (ActionInstances.IsValidIndex(index))
	{
		return ActionInstances[index];
	}
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UModularControllerComponent::RemoveActionBehaviourByType_Implementation(TSubclassOf<UBaseControllerAction> moduleType)
{
	if (CheckActionBehaviourByType(moduleType))
	{
		auto behaviour = ActionInstances.FindByPredicate([moduleType](UBaseControllerAction* action) -> bool { return action->GetClass() == moduleType->GetClass(); });
		ActionInstances.Remove(*behaviour);
		if (ActionInstances.Num() > 0)
			ActionInstances.Sort([](UBaseControllerAction& a, UBaseControllerAction& b) { return a.GetPriority() > b.GetPriority(); });
		return;
	}
}

void UModularControllerComponent::RemoveActionBehaviourByName_Implementation(FName moduleName)
{
	if (CheckActionBehaviourByName(moduleName))
	{
		auto behaviour = ActionInstances.FindByPredicate([moduleName](UBaseControllerAction* action) -> bool { return action->GetDescriptionName() == moduleName; });
		ActionInstances.Remove(*behaviour);
		if (ActionInstances.Num() > 0)
			ActionInstances.Sort([](UBaseControllerAction& a, UBaseControllerAction& b) { return a.GetPriority() > b.GetPriority(); });
		return;
	}
}

void UModularControllerComponent::RemoveActionBehaviourByPriority_Implementation(int modulePriority)
{
	if (CheckActionBehaviourByPriority(modulePriority))
	{
		auto behaviour = ActionInstances.FindByPredicate([modulePriority](UBaseControllerAction* action) -> bool { return action->GetPriority() == modulePriority; });
		ActionInstances.Remove(*behaviour);
		if (ActionInstances.Num() > 0)
			ActionInstances.Sort([](UBaseControllerAction& a, UBaseControllerAction& b) { return a.GetPriority() > b.GetPriority(); });
		return;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


FControllerStatus UModularControllerComponent::CheckControllerActions(FControllerStatus currentControllerStatus, const float inDelta, FVector simulationDatas)
{
	FControllerStatus endStatus = currentControllerStatus;
	FControllerStatus selectedStatus = endStatus;
	int selectedActionIndex = -1;

	//Check active action still active
	selectedActionIndex = endStatus.ControllerStatus.ActionIndex;
	if (ActionInstances.IsValidIndex(selectedActionIndex))
	{
		if (ActionInstances[selectedActionIndex]->CurrentPhase == ActionPhase_Recovery
			&& ActionInstances[selectedActionIndex]->bCanTransitionToSelf
			&& CheckActionCompatibility(ActionInstances[selectedActionIndex], endStatus.ControllerStatus.StateIndex, endStatus.ControllerStatus.ActionIndex))
		{
			const auto chkResult = ActionInstances[selectedActionIndex]->CheckAction_Internal(this, endStatus, inDelta, true);
			if (chkResult.CheckedCondition)
			{
				selectedStatus = chkResult.ProcessResult;
				selectedStatus.ControllerStatus.PrimaryActionFlag = 1;
			}
		}

		if (ActionInstances[selectedActionIndex]->GetRemainingActivationTime() <= 0)
		{
			selectedActionIndex = -1;
			selectedStatus = endStatus;
		}
	}

	//Check actions
	for (int i = 0; i < ActionInstances.Num(); i++)
	{
		if (selectedActionIndex == i)
			continue;

		if (ActionInstances[i] == nullptr)
			continue;

		if (ActionInstances.IsValidIndex(selectedActionIndex))
		{
			if (ActionInstances[i]->GetPriority() <= ActionInstances[selectedActionIndex]->GetPriority())
			{
				if (ActionInstances[i]->GetPriority() != ActionInstances[selectedActionIndex]->GetPriority())
					continue;
				if (ActionInstances[i]->GetPriority() == ActionInstances[selectedActionIndex]->GetPriority() && ActionInstances[selectedActionIndex]->CurrentPhase != ActionPhase_Recovery)
					continue;
			}
		}

		//Handle state snapshot
		if (simulationDatas.X > 0)
			ActionInstances[i]->SaveActionSnapShot();
		else
			ActionInstances[i]->RestoreActionFromSnapShot();

		if (CheckActionCompatibility(ActionInstances[i], endStatus.ControllerStatus.StateIndex, endStatus.ControllerStatus.ActionIndex))
		{
			const auto chkResult = ActionInstances[i]->CheckAction_Internal(this, endStatus, inDelta, i == endStatus.ControllerStatus.ActionIndex);
			if (chkResult.CheckedCondition)
			{
				selectedActionIndex = i;
				selectedStatus = chkResult.ProcessResult;

				if (simulationDatas.X <= 0 && DebugType == ControllerDebugType_StatusDebug)
				{
					UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Action (%s) was checked as active. Remaining Time: %f"), *ActionInstances[i]->DebugString(), ActionInstances[i]->GetRemainingActivationTime()), true, true, FColor::Silver, 0
						, FName(FString::Printf(TEXT("CheckControllerActions_%s"), *ActionInstances[i]->GetDescriptionName().ToString())));
				}
			}
		}
	}

	if (simulationDatas.X <= 0 && DebugType == ControllerDebugType_StatusDebug)
	{
		UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Check Action Phase: %d"), selectedActionIndex), true, true, FColor::Silver, 0
			, TEXT("CheckControllerActions"));
	}

	endStatus = selectedStatus;
	endStatus.ControllerStatus.ActionIndex = selectedActionIndex;
	return endStatus;
}


bool UModularControllerComponent::CheckActionCompatibility(UBaseControllerAction* actionInstance, int stateIndex, int actionIndex)
{
	if (actionInstance == nullptr)
		return false;

	bool incompatible = false;
	switch (actionInstance->ActionCompatibilityMode)
	{
	default:
		break;
	case ActionCompatibilityMode_WhileCompatibleActionOnly:
	{
		incompatible = true;
		if (actionInstance->CompatibleActions.Num() > 0)
		{
			if (ActionInstances.IsValidIndex(actionIndex) && ActionInstances[actionIndex])
			{
				const auto actionName = ActionInstances[actionIndex]->GetDescriptionName();
				if (actionInstance->CompatibleActions.Contains(actionName))
				{
					incompatible = false;
				}
			}
		}
	}
	break;
	case ActionCompatibilityMode_OnCompatibleStateOnly:
	{
		incompatible = true;
		if (StatesInstances.IsValidIndex(stateIndex) && actionInstance->CompatibleStates.Num() > 0)
		{
			const auto stateName = StatesInstances[stateIndex]->GetDescriptionName();
			if (actionInstance->CompatibleStates.Contains(stateName))
			{
				incompatible = false;
			}
		}
	}
	break;
	case ActionCompatibilityMode_OnBothCompatiblesStateAndAction:
	{
		int compatibilityCount = 0;
		//State
		if (StatesInstances.IsValidIndex(stateIndex) && actionInstance->CompatibleStates.Num() > 0)
		{
			auto stateName = StatesInstances[stateIndex]->GetDescriptionName();
			if (actionInstance->CompatibleStates.Contains(stateName))
			{
				compatibilityCount++;
			}
		}
		//Actions
		if (actionInstance->CompatibleActions.Num() > 0)
		{
			if (ActionInstances.IsValidIndex(actionIndex) && ActionInstances[actionIndex])
			{
				const auto actionName = ActionInstances[actionIndex]->GetDescriptionName();
				if (actionInstance->CompatibleActions.Contains(actionName))
				{
					compatibilityCount++;
				}
			}
		}
		incompatible = compatibilityCount < 2;
	}
	break;
	}

	return !incompatible;
}


FControllerCheckResult UModularControllerComponent::TryChangeControllerAction(FControllerStatus toActionStatus, FControllerStatus fromActionStatus, const float inDelta, bool simulate)
{
	const bool transitionToSelf = toActionStatus.ControllerStatus.PrimaryActionFlag > 0;
	toActionStatus.ControllerStatus.PrimaryActionFlag = 0;

	FControllerCheckResult result = FControllerCheckResult(false, toActionStatus);
	const int fromActionIndex = fromActionStatus.ControllerStatus.ActionIndex;
	const int toActionIndex = toActionStatus.ControllerStatus.ActionIndex;

	if (fromActionIndex == toActionIndex)
	{
		if (!transitionToSelf)
			return FControllerCheckResult(false, fromActionStatus);
	}

	if (!simulate && DebugType == ControllerDebugType_StatusDebug)
	{
		UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Trying to change action from: %d to: %d"), fromActionIndex, toActionIndex), true, true
			, FColor::White, 5, TEXT("TryChangeControllerActions_1"));
	}

	result.CheckedCondition = true;
	result.ProcessResult.ControllerStatus.ActionIndex = toActionIndex;

	//Disable last action
	if (ActionInstances.IsValidIndex(fromActionIndex))
	{
		result.ProcessResult.Kinematics = ActionInstances[fromActionIndex]->OnActionEnds_Internal(this, result.ProcessResult.Kinematics, result.ProcessResult.MoveInput, inDelta);
		if (!simulate && DebugType == ControllerDebugType_StatusDebug)
		{
			UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Action (%s) is Being Disabled. Remaining Time: %f"), *ActionInstances[fromActionIndex]->DebugString(), ActionInstances[fromActionIndex]->GetRemainingActivationTime()), true, true, FColor::Red, 5
				, FName(FString::Printf(TEXT("TryChangeControllerActions_%s"), *ActionInstances[fromActionIndex]->GetDescriptionName().ToString())));

		}
	}

	//Activate action
	if (ActionInstances.IsValidIndex(toActionIndex))
	{
		result.ProcessResult.Kinematics = ActionInstances[toActionIndex]->OnActionBegins_Internal(this, result.ProcessResult.Kinematics, result.ProcessResult.MoveInput, inDelta);
		if (!simulate && DebugType == ControllerDebugType_StatusDebug)
		{
			UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Action (%s) is Being Activated. Remaining Time: %f"), *ActionInstances[toActionIndex]->DebugString(), ActionInstances[toActionIndex]->GetRemainingActivationTime()), true, true, FColor::Green, 5
				, FName(FString::Printf(TEXT("TryChangeControllerActions_%s"), *ActionInstances[toActionIndex]->GetDescriptionName().ToString())));
		}
	}

	//Notify actions and states
	if (!simulate)
	{
		for (int i = 0; i < StatesInstances.Num(); i++)
		{
			StatesInstances[i]->OnControllerActionChanged(this, ActionInstances.IsValidIndex(toActionIndex) ? ActionInstances[toActionIndex] : nullptr
				, ActionInstances.IsValidIndex(fromActionIndex) ? ActionInstances[fromActionIndex] : nullptr);
		}

		for (int i = 0; i < ActionInstances.Num(); i++)
		{
			ActionInstances[i]->OnControllerActionChanged(this, ActionInstances.IsValidIndex(toActionIndex) ? ActionInstances[toActionIndex] : nullptr
				, ActionInstances.IsValidIndex(fromActionIndex) ? ActionInstances[fromActionIndex] : nullptr);
		}

		OnControllerActionChanged(ActionInstances.IsValidIndex(toActionIndex) ? ActionInstances[toActionIndex] : nullptr
			, ActionInstances.IsValidIndex(fromActionIndex) ? ActionInstances[fromActionIndex] : nullptr);
		OnControllerActionChangedEvent.Broadcast(ActionInstances.IsValidIndex(toActionIndex) ? ActionInstances[toActionIndex] : nullptr
			, ActionInstances.IsValidIndex(fromActionIndex) ? ActionInstances[fromActionIndex] : nullptr);

		if (DebugType == ControllerDebugType_StatusDebug)
		{
			UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Changed actions from: %d  to: %d"), fromActionIndex, toActionIndex), true, true
				, FColor::Yellow, 5, TEXT("TryChangeControllerActions_2"));
		}
	}

	return result;
}


FControllerStatus UModularControllerComponent::ProcessControllerAction(const FControllerStatus initialState, const float inDelta, bool simulate)
{
	FControllerStatus processMotion = initialState;
	const int index = initialState.ControllerStatus.ActionIndex;

	if (ActionInstances.IsValidIndex(index))
	{
		processMotion = ProcessSingleAction(ActionInstances[index], processMotion, inDelta, simulate);

		if (DebugType == ControllerDebugType_StatusDebug)
		{
			UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Action (%s) is Being Processed. Remaining Time: %f"), *ActionInstances[index]->DebugString(), ActionInstances[index]->GetRemainingActivationTime()), true, true, FColor::White, 5
				, FName(FString::Printf(TEXT("ProcessControllerActions_%s"), *ActionInstances[index]->GetDescriptionName().ToString())));
		}
	}

	return processMotion;
}


FControllerStatus UModularControllerComponent::ProcessSingleAction(UBaseControllerAction* actionInstance, const FControllerStatus initialState, const float inDelta, bool simulate)
{
	if (actionInstance == nullptr)
		return initialState;

	FControllerStatus processMotion = initialState;

	//Handle state snapshot
	if (simulate)
		actionInstance->SaveActionSnapShot();
	else
		actionInstance->RestoreActionFromSnapShot();

	processMotion = actionInstance->OnActionProcess_Internal(this, processMotion, inDelta);

	return processMotion;
}


void UModularControllerComponent::OnControllerActionChanged_Implementation(UBaseControllerAction* newAction, UBaseControllerAction* lastAction)
{
}


#pragma endregion



#pragma region Animation Component


FVector UModularControllerComponent::GetRootMotionVector(USkeletalMeshComponent* skeletalMeshReference)
{
	USkeletalMeshComponent* component = skeletalMeshReference;
	if (component == nullptr || !_RootMotionParams.Contains(component))
		return FVector(0);
	FVector translation = _RootMotionParams[component].GetTranslation() * RootMotionScale;
	return translation;
}


FQuat UModularControllerComponent::GetRootMotionQuat(USkeletalMeshComponent* skeletalMeshReference)
{
	USkeletalMeshComponent* component = skeletalMeshReference;

	if (component == nullptr || !_RootMotionParams.Contains(component))
		return FQuat::Identity;
	FQuat rotation = _RootMotionParams[component].GetRotation();
	return rotation;
}


USkeletalMeshComponent* UModularControllerComponent::GetSkeletalMesh()
{
	if (!_skeletalMesh.IsValid()) {
		USkeletalMeshComponent* component = MainSkeletal.GetComponent(GetOwner())
			? Cast<USkeletalMeshComponent>(MainSkeletal.GetComponent(GetOwner()))
			: nullptr;
		_skeletalMesh = component;
	}
	return _skeletalMesh.Get();
}

double UModularControllerComponent::PlayAnimationMontage_Internal(FActionMotionMontage Montage, float customAnimStartTime
	, bool useMontageEndCallback, FOnMontageEnded endCallBack)
{
	if (const USkeletalMeshComponent* mesh = GetSkeletalMesh())
	{
		UAnimInstance* animInstance = mesh->GetAnimInstance();
		return PlayAnimMontageSingle(animInstance, Montage, customAnimStartTime, useMontageEndCallback, endCallBack);
	}

	return -1;
}

double UModularControllerComponent::PlayAnimationMontageOnState_Internal(FActionMotionMontage Montage, FName stateName, float customAnimStartTime
	, bool useMontageEndCallback, FOnMontageEnded endCallBack)
{
	if (const USkeletalMeshComponent* mesh = GetSkeletalMesh())
	{
		const UBaseControllerState* state = GetControllerStateByName(stateName);
		if (state == nullptr)
			return -1;
		if (state->StateBlueprintClass == nullptr)
			return -1;
		UAnimInstance* animInstance = mesh->GetLinkedAnimLayerInstanceByClass(state->StateBlueprintClass);
		return PlayAnimMontageSingle(animInstance, Montage, customAnimStartTime, useMontageEndCallback, endCallBack);
	}

	return -1;
}

double UModularControllerComponent::PlayAnimationMontage(FActionMotionMontage Montage, float customAnimStartTime)
{
	return PlayAnimationMontage_Internal(Montage, customAnimStartTime);
}

double UModularControllerComponent::PlayAnimationMontageOnState(FActionMotionMontage Montage, FName stateName, float customAnimStartTime)
{
	return PlayAnimationMontageOnState_Internal(Montage, stateName, customAnimStartTime);
}


void UModularControllerComponent::LinkAnimBlueprint(USkeletalMeshComponent* skeletalMeshReference, FName key, TSubclassOf<UAnimInstance> animClass)
{
	USkeletalMeshComponent* target = skeletalMeshReference;

	if (target == nullptr)
		return;

	FQuat lookDir = target->GetComponentRotation().Quaternion();


	//The mesh is not Listed.
	if (!_linkedAnimClasses.Contains(target))
	{
		TMap <FName, TSoftObjectPtr<UAnimInstance>> meshLinkEntry;

		//Unlink All
		{
			for (auto AnimClass : _linkedAnimClasses)
			{
				if (AnimClass.Key == nullptr)
					continue;
				for (auto Pair : AnimClass.Value)
				{
					if (Pair.Value == nullptr)
						continue;
					auto instance = Pair.Value;
					if (instance == nullptr)
						continue;
				}
			}
			target->LinkAnimClassLayers(nullptr);
		}

		//link
		target->LinkAnimClassLayers(animClass);

		//Register
		meshLinkEntry.Add(key, target->GetLinkedAnimLayerInstanceByClass(animClass));
		_linkedAnimClasses.Add(target, meshLinkEntry);
		target->SetWorldRotation(lookDir);
		return;
	}

	//The mesh links with a new key
	if (!_linkedAnimClasses[target].Contains(key))
	{
		TMap <FName, TSoftObjectPtr<UAnimInstance>> meshLinkEntry;

		//Unlink All
		{
			for (auto AnimClass : _linkedAnimClasses)
			{
				if (AnimClass.Key == nullptr)
					continue;
				for (auto Pair : AnimClass.Value)
				{
					if (Pair.Value == nullptr)
						continue;
					auto instance = Pair.Value;
					if (instance == nullptr)
						continue;
				}
			}
			target->LinkAnimClassLayers(nullptr);
		}

		//link
		target->LinkAnimClassLayers(animClass);

		//Register
		meshLinkEntry.Add(key, target->GetLinkedAnimLayerInstanceByClass(animClass));
		_linkedAnimClasses[target] = meshLinkEntry;
		target->SetWorldRotation(lookDir);
		return;
	}

	if (_linkedAnimClasses[target][key] != nullptr)
	{
		if (_linkedAnimClasses[target][key]->GetClass() == animClass)
			return;
		//Unlink
		target->UnlinkAnimClassLayers(_linkedAnimClasses[target][key]->GetClass());
	}
	if (animClass != nullptr)
	{
		//link
		target->LinkAnimClassLayers(animClass);
		_linkedAnimClasses[target][key] = target->GetLinkedAnimLayerInstanceByClass(animClass);
	}

	target->SetWorldRotation(lookDir);
}


double UModularControllerComponent::PlayAnimMontageSingle(UAnimInstance* animInstance, FActionMotionMontage montage, float customAnimStartTime
	, bool useMontageEndCallback, FOnMontageEnded endCallBack)
{
	if (animInstance == nullptr)
	{
		return -1;
	}

	if (montage.Montage == nullptr)
	{
		return -1;
	}

	const float startTime = customAnimStartTime >= 0 ? customAnimStartTime : 0;
	float duration = animInstance->Montage_Play(montage.Montage, 1, EMontagePlayReturnType::Duration, startTime);
	duration = montage.Montage->GetSectionLength(0);

	if (useMontageEndCallback)
	{
		animInstance->Montage_SetEndDelegate(endCallBack, montage.Montage);
	}

	if (duration <= 0)
	{
		return -1;
	}

	if (!montage.MontageSection.IsNone())
	{
		//Jumps to a section
		animInstance->Montage_JumpToSection(montage.MontageSection, montage.Montage);
		const float newMontagePos = animInstance->Montage_GetPosition(montage.Montage);
		auto sectionID = montage.Montage->GetSectionIndex(montage.MontageSection);
		duration = montage.Montage->GetSectionLength(sectionID);
	}

	return duration;
}


void UModularControllerComponent::ReadRootMotion(FKinematicComponents& kinematics, const ERootMotionType rootMotionMode, const double deltaTime)
{
	if (rootMotionMode != ERootMotionType::RootMotionType_No_RootMotion)
	{
		//Rotation
		kinematics.AngularKinematic.Orientation *= GetRootMotionQuat(GetSkeletalMesh());

		//Translation
		switch (rootMotionMode)
		{
		case RootMotionType_No_RootMotion:
			break;
		default:
		{
			const FVector translation = GetRootMotionTranslation(rootMotionMode, kinematics.LinearKinematic.Velocity, deltaTime);
			kinematics.LinearKinematic.AddCompositeMovement(translation, -1, 0);
		}
		break;
		}
	}
}

FVector UModularControllerComponent::GetRootMotionTranslation(const ERootMotionType rootMotionMode, const FVector currentVelocity, const double deltaTime)
{
	switch (rootMotionMode)
	{
	case RootMotionType_Additive:
	{
		return GetRootMotionVector(GetSkeletalMesh()) / deltaTime + currentVelocity;
	}
	break;
	case RootMotionType_Override:
	{
		return GetRootMotionVector(GetSkeletalMesh()) / deltaTime;
	}
	break;
	default:
		break;
	}

	return currentVelocity;
}


void UModularControllerComponent::EvaluateRootMotions(float delta)
{
	//Add main if not there
	{
		USkeletalMeshComponent* mainSkeletal = GetSkeletalMesh();

		if (mainSkeletal != nullptr && !_RootMotionParams.Contains(mainSkeletal))
		{
			_RootMotionParams.Add(mainSkeletal, FTransform());
		}
	}

	//Extract Root Motion
	for (auto entry : _RootMotionParams)
	{
		USkeletalMeshComponent* skeletal = entry.Key.Get();
		if (skeletal == nullptr)
			continue;
		_RootMotionParams[skeletal] = skeletal->ConvertLocalRootMotionToWorld(skeletal->ConsumeRootMotion().GetRootMotionTransform());
	}
}


FControllerStatus UModularControllerComponent::EvaluateRootMotionOverride(const FControllerStatus inStatus, float inDelta)
{
	FControllerStatus result = inStatus;

	//Handle Root Motion Override
	{
		USkeletalMeshComponent* target = GetSkeletalMesh();

		if (target != nullptr && _overrideRootMotionCommands.Contains(target))
		{
			//Rotation
			if (_overrideRootMotionCommands[target].OverrideRotationRootMotionMode != ERootMotionType::RootMotionType_No_RootMotion)
			{
				//Rotation
				result.Kinematics.AngularKinematic.Orientation *= GetRootMotionQuat(GetSkeletalMesh());
			}

			//Translation
			if (_overrideRootMotionCommands[target].OverrideTranslationRootMotionMode != ERootMotionType::RootMotionType_No_RootMotion)
			{
				switch (_overrideRootMotionCommands[target].OverrideTranslationRootMotionMode)
				{
				case RootMotionType_Additive:
				{
					result.Kinematics.LinearKinematic.Velocity += GetRootMotionVector(GetSkeletalMesh()) / inDelta;
				}break;
				case RootMotionType_Override:
				{
					result.Kinematics.LinearKinematic.Velocity = GetRootMotionVector(GetSkeletalMesh()) / inDelta;
				}break;
				}
			}

			//Auto restore
			if (_overrideRootMotionCommands[target].OverrideRootMotionChrono > 0)
			{
				_overrideRootMotionCommands[target].OverrideRootMotionChrono -= inDelta;
				if (_overrideRootMotionCommands[target].OverrideRootMotionChrono <= 0)
				{
					_overrideRootMotionCommands[target].OverrideRootMotionChrono = ERootMotionType::RootMotionType_No_RootMotion;
					_overrideRootMotionCommands[target].OverrideRootMotionChrono = ERootMotionType::RootMotionType_No_RootMotion;
				}
			}
		}
	}

	return result;
}


#pragma endregion



#pragma region Movement XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

void UModularControllerComponent::Move_Implementation(const FKinematicComponents finalKinematic, float deltaTime)
{
	if (!UpdatedPrimitive)
		return;
	if (UpdatedPrimitive->IsSimulatingPhysics())
	{
		UpdatedPrimitive->SetAllPhysicsPosition(finalKinematic.LinearKinematic.Position);
		UpdatedPrimitive->SetAllPhysicsLinearVelocity(finalKinematic.LinearKinematic.Velocity);
		UpdatedPrimitive->SetAllPhysicsRotation(finalKinematic.AngularKinematic.Orientation);
	}
	else
	{
		UpdatedPrimitive->SetWorldLocationAndRotation(finalKinematic.LinearKinematic.Position, finalKinematic.AngularKinematic.Orientation, false);
	}
}


FKinematicComponents UModularControllerComponent::KinematicMoveEvaluation(FControllerStatus processedMove, bool noCollision, float delta, bool applyForceOnSurfaces)
{
	FKinematicComponents finalKcomp = FKinematicComponents(processedMove.Kinematics);
	FVector initialLocation = processedMove.Kinematics.LinearKinematic.Position;
	const FQuat primaryRotation = processedMove.Kinematics.GetRotation();
	FVector acceleration = processedMove.Kinematics.LinearKinematic.Acceleration;
	FQuat surfaceRotRate = FQuat::Identity;
	const float hull = 0;
	const double drag = processedMove.CustomPhysicProperties.Y >= 0 ? processedMove.CustomPhysicProperties.Y : Drag;

	//Drag
	FVector vel = finalKcomp.LinearKinematic.Velocity * 0.01;
	const double velSqr = vel.SquaredLength();
	if (vel.Normalize())
	{
		acceleration -= vel * ((velSqr * drag) / (2 * GetMass())) * 100;
	}

	//Consume external forces
	acceleration += _collisionAccelerations;
	_collisionAccelerations = FVector(0);


	//Penetration force and velocity
	if (!noCollision)
	{
		FVector penetrationVec = FVector(0);
		FVector contactForce = FVector(0);
		if (CheckPenetrationAt(penetrationVec, contactForce, initialLocation, primaryRotation, nullptr, hull + 0.125, true))
		{
			FVector forceDirection = contactForce;
			initialLocation += penetrationVec;

			if (forceDirection.Normalize() && penetrationVec.Normalize())
			{
				if ((forceDirection | penetrationVec) > 0)
				{
					//acceleration += contactForce / GetMass();
				}
			}
		}
	}

	//Handle surface Operations if any
	if (processedMove.ControllerSurface.GetSurfacePrimitive())
	{
		FVector surfaceVel = processedMove.ControllerSurface.GetSurfaceLinearVelocity(true, true, false);
		const FVector surfaceValues = UStructExtensions::GetSurfacePhysicProperties(processedMove.ControllerSurface.GetHitResult());
		finalKcomp.LinearKinematic.SetReferentialMovement(surfaceVel, delta, processedMove.ControllerSurface.HadChangedSurface() ? 0 : (1 / delta) * surfaceValues.X);
		surfaceRotRate = processedMove.ControllerSurface.GetSurfaceAngularVelocity(true);
	}
	else
	{
		finalKcomp.LinearKinematic.SetReferentialMovement(FVector(0), delta, 0);
	}

	//Kinematic function to evaluate position and velocity from acceleration
	FVector selfVel = finalKcomp.LinearKinematic.Velocity;
	finalKcomp.LinearKinematic.Acceleration = acceleration;
	finalKcomp.LinearKinematic = finalKcomp.LinearKinematic.GetFinalCondition(delta);
	acceleration = finalKcomp.LinearKinematic.Acceleration;
	FVector refMotionVel = finalKcomp.LinearKinematic.refVelocity;

	//Force on surface	
	if (applyForceOnSurfaces)
	{
		if (processedMove.ControllerSurface.GetSurfacePrimitive())
		{
			FVector normalForce = (acceleration).ProjectOnToNormal(processedMove.ControllerSurface.GetHitResult().ImpactNormal) * GetMass();
			const auto surfacePrimo = processedMove.ControllerSurface.GetSurfacePrimitive();
			const FVector impactPt = processedMove.ControllerSurface.GetHitResult().ImpactPoint;
			const FVector impactNormal = processedMove.ControllerSurface.GetHitResult().ImpactNormal;
			const FName impactBoneName = processedMove.ControllerSurface.GetHitResult().BoneName;
			const FVector surfaceValues = UStructExtensions::GetSurfacePhysicProperties(processedMove.ControllerSurface.GetHitResult());

			FVector outSelfVel = FVector(0);
			FVector outSurfaceVel = FVector(0);

			if (surfacePrimo && surfacePrimo->IsSimulatingPhysics())
			{
				FVector atPtVelocity = surfacePrimo->GetPhysicsLinearVelocityAtPoint(impactPt, impactBoneName);
				const double surfaceMass = surfacePrimo->GetMass();
				//Landing
				if (processedMove.ControllerSurface.HadLandedOnSurface())
				{
					if ((selfVel | impactNormal) < 0
						&& UStructExtensions::ComputeCollisionVelocities(selfVel, atPtVelocity, impactNormal, GetMass(), surfaceMass, surfaceValues.Y, outSelfVel, outSurfaceVel))
					{
						const FVector forceOnSurface = (outSurfaceVel / delta) * GetMass();
						surfacePrimo->AddForceAtLocation(forceOnSurface, impactPt, impactBoneName);
					}
				}
				//Continuous
				else
				{
					surfacePrimo->AddForceAtLocation(normalForce * 0.01 + GetGravity() * GetMass(), impactPt, impactBoneName);
				}
			}
		}
	}


	//Primary Movement (momentum movement)
	{
		FHitResult sweepMoveHit = FHitResult(EForceInit::ForceInitToZero);
		FVector priMove = finalKcomp.LinearKinematic.Velocity;
		FVector conservedVelocity = finalKcomp.LinearKinematic.Velocity;

		//Snap displacement
		const FVector noSnapPriMove = priMove;
		priMove += finalKcomp.LinearKinematic.SnapDisplacement / delta;


		bool blockingHit = false;
		FVector endLocation = initialLocation;
		FVector moveDisplacement = FVector(0);

		//Trace to detect hit while moving
		blockingHit = noCollision ? false : ComponentTraceCastSingle(sweepMoveHit, initialLocation, priMove * delta, primaryRotation, hull, bUseComplexCollision);

		//Try to adjust the referential move
		if (blockingHit && refMotionVel.SquaredLength() > 0)
		{
			priMove -= refMotionVel;
			conservedVelocity -= refMotionVel;
			refMotionVel = FVector::VectorPlaneProject(refMotionVel, sweepMoveHit.Normal);
			priMove += refMotionVel;
			conservedVelocity += refMotionVel;
			initialLocation = sweepMoveHit.Location + sweepMoveHit.Normal * 0.01;
			sweepMoveHit = FHitResult(EForceInit::ForceInitToZero);
			blockingHit = noCollision ? false : ComponentTraceCastSingle(sweepMoveHit, initialLocation, priMove * delta, primaryRotation, hull, bUseComplexCollision);
		}
		else
		{
			finalKcomp.AngularKinematic.Orientation *= surfaceRotRate;
		}

		//Handle collision
		if (blockingHit)
		{
			//Properties of the hit surface (X=friction, Y=Bounciness, Z= hit component mass)
			const FVector surfaceProperties = UStructExtensions::GetSurfacePhysicProperties(sweepMoveHit);
			const double surfaceMass = GetHitComponentMass(sweepMoveHit);
			const double friction = processedMove.CustomPhysicProperties.X >= 0 ? processedMove.CustomPhysicProperties.X : surfaceProperties.X;
			const double frictionVelocity = UStructExtensions::GetFrictionAcceleration(sweepMoveHit.Normal, acceleration * GetMass(), GetMass(), friction) * delta;
			const double bounciness = processedMove.CustomPhysicProperties.Z >= 0 ? processedMove.CustomPhysicProperties.Z : surfaceProperties.Y;

			//Momentum and speed conservation
			const FVector conservedMove = (priMove / surfaceMass).GetClampedToMaxSize(priMove.Length());

			//Reaction and Slide on surface
			const FVector pureReaction = -priMove.ProjectOnToNormal(sweepMoveHit.Normal);
			const FVector reactionVelocity = pureReaction * bounciness + conservedMove.ProjectOnToNormal(sweepMoveHit.Normal) * (sweepMoveHit.Time);
			const FVector frictionlessVelocity = FVector::VectorPlaneProject(priMove, sweepMoveHit.Normal);
			FVector withFrictionVelocity = frictionlessVelocity;// -frictionlessVelocity.GetSafeNormal() * frictionVelocity;

			int maxDepth = 1;
			endLocation = SlideAlongSurfaceAt(sweepMoveHit, primaryRotation, (withFrictionVelocity - pureReaction) * delta, maxDepth, delta, hull);

			//Stuck protection
			if (sweepMoveHit.bStartPenetrating && initialLocation.Equals(endLocation, 0.35) && priMove.SquaredLength() > 0)
			{
				if (DebugType == ControllerDebugType_MovementDebug)
				{
					UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("I'm stuck: initial location: (%s). End location: (%s)"), *initialLocation.ToCompactString(), *endLocation.ToCompactString()), true, true, FColor::Magenta, delta * 2, "stuck");
					UKismetSystemLibrary::DrawDebugArrow(this, sweepMoveHit.ImpactPoint, sweepMoveHit.ImpactPoint + sweepMoveHit.Normal * 50, 50, FColor::Magenta, delta * 2, 3);
				}
				endLocation += sweepMoveHit.Normal * (sweepMoveHit.PenetrationDepth > 0 ? sweepMoveHit.PenetrationDepth : 0.125);
			}

			//Debug Movement
			if (DebugType == ControllerDebugType_MovementDebug)
			{
				UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Move Amount Done: (%f) percent. Initial overlap? (%d)"), sweepMoveHit.Time * 100, sweepMoveHit.bStartPenetrating), true, true, FColor::Red, delta, "hitTime");
			}

			//Handle positioning and Velocity
			moveDisplacement = endLocation - initialLocation;
			FVector postCollisionVelocity = (FVector::VectorPlaneProject(moveDisplacement, sweepMoveHit.Normal) / delta) + reactionVelocity;

			if (processedMove.ControllerSurface.GetSurfacePrimitive() == sweepMoveHit.GetComponent())
			{
				conservedVelocity = postCollisionVelocity;
			}
			else
			{
				conservedVelocity = FMath::Lerp(conservedVelocity, postCollisionVelocity, delta);
			}
		}
		else
		{
			//Make the move
			moveDisplacement = priMove * delta;
			endLocation = initialLocation + moveDisplacement;

			//Debug the move
			if (DebugType == ControllerDebugType_MovementDebug)
			{
				UKismetSystemLibrary::DrawDebugArrow(this, initialLocation, initialLocation + priMove * delta, 50, FColor::Green, delta * 2);
			}
		}

		//Compute final position ,velocity and acceleration
		const FVector primaryDelta = moveDisplacement;
		const FVector location = endLocation;
		finalKcomp.LinearKinematic = processedMove.Kinematics.LinearKinematic.GetFinalFromPosition(location, delta, false);
		finalKcomp.LinearKinematic.Acceleration = acceleration;
		finalKcomp.LinearKinematic.Velocity = conservedVelocity;
		finalKcomp.LinearKinematic.refVelocity = refMotionVel;
	}


	//Analytic Debug
	if (DebugType == ControllerDebugType_MovementDebug)
	{
		const FVector relativeVel = finalKcomp.LinearKinematic.Velocity - finalKcomp.LinearKinematic.refVelocity;
		const FVector relativeAcc = finalKcomp.LinearKinematic.Acceleration - finalKcomp.LinearKinematic.refAcceleration;

		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Referential Movement: Vel[Dir:(%s), Lenght:(%f) m/s], Acc[Dir:(%s) Lenght:(%f) m/s2]"), *finalKcomp.LinearKinematic.refVelocity.GetSafeNormal().ToCompactString(), finalKcomp.LinearKinematic.refVelocity.Length() * 0.01, *finalKcomp.LinearKinematic.refAcceleration.GetSafeNormal().ToCompactString(), finalKcomp.LinearKinematic.refAcceleration.Length() * 0.01), true, true, FColor::Magenta, 60, "refInfos");

		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Global Position: (%s)"), *finalKcomp.LinearKinematic.Position.ToCompactString()), true, true, FColor::Blue, 60, "Pos");

		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Linear Velocity [ Global {Dir:(%s), Lenght:(%f) m/s} | Relative {Dir:(%s), Lenght:(%f) m/s}]"), *finalKcomp.LinearKinematic.Velocity.GetSafeNormal().ToCompactString(), (finalKcomp.LinearKinematic.Velocity.Length() * 0.01), *relativeVel.GetSafeNormal().ToCompactString(), relativeVel.Length() * 0.01), true, true, FColor::Cyan, 60, "LineSpd");

		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Linear Acceleration [ Global {Dir:(%s), Lenght:(%f) m/s2} | Relative {Dir:(%s), Lenght:(%f) m/s2}]"), *acceleration.GetSafeNormal().ToCompactString(), (acceleration.Length() * 0.01), *relativeAcc.GetSafeNormal().ToCompactString(), relativeAcc.Length() * 0.01), true, true, FColor::Purple, 60, "lineAcc");

		if (finalKcomp.LinearKinematic.Acceleration.SquaredLength() > 0)
		{
			UKismetSystemLibrary::DrawDebugArrow(this, finalKcomp.LinearKinematic.Position, finalKcomp.LinearKinematic.Position + finalKcomp.LinearKinematic.Acceleration.GetClampedToMaxSize(100) * 0.5, 50, FColor::Purple, delta * 2, 3);
		}

		if (finalKcomp.LinearKinematic.Velocity.SquaredLength() > 0)
		{
			UKismetSystemLibrary::DrawDebugArrow(this, finalKcomp.LinearKinematic.Position, finalKcomp.LinearKinematic.Position + finalKcomp.LinearKinematic.Velocity.GetClampedToMaxSize(100) * 0.5, 50, FColor::Cyan, delta * 2, 3);
		}
	}

	return finalKcomp;
}


FControllerStatus UModularControllerComponent::ConsumeLastKinematicMove(FVector moveInput)
{
	FControllerStatus initialState = ControllerStatus;
	initialState.Kinematics.LinearKinematic.Acceleration = FVector(0);
	initialState.Kinematics.LinearKinematic.CompositeMovements.Empty();
	initialState.Kinematics.LinearKinematic.refAcceleration = FVector(0);
	initialState.Kinematics.LinearKinematic.SnapDisplacement = FVector(0);
	initialState.Kinematics.LinearKinematic.Position = GetLocation();
	initialState.Kinematics.AngularKinematic.AngularAcceleration = FVector(0);
	initialState.Kinematics.AngularKinematic.Orientation = GetRotation();
	initialState.MoveInput = moveInput;
	initialState.CustomPhysicProperties = FVector(-1);

	return initialState;
}


void UModularControllerComponent::KinematicPostMove(FControllerStatus newStatus, const float inDelta)
{
	ControllerStatus = newStatus;
	Velocity = ControllerStatus.Kinematics.LinearKinematic.Velocity;
	UpdateComponentVelocity();
}


FAngularKinematicCondition UModularControllerComponent::HandleKinematicRotation(const FAngularKinematicCondition inRotCondition, const float inDelta) const
{
	FAngularKinematicCondition outputCondition = inRotCondition;

	FVector gravityUp = -GetGravityDirection();

	//Acceleration
	outputCondition.AngularAcceleration = outputCondition.AngularAcceleration.ProjectOnToNormal(gravityUp);

	//Rotation
	outputCondition.RotationSpeed = outputCondition.RotationSpeed.ProjectOnToNormal(gravityUp);

	//Orientation
	{
		FVector virtualFwdDir = FVector::VectorPlaneProject(outputCondition.Orientation.Vector(), gravityUp);
		FVector virtualRightDir = FVector::ZeroVector;
		if (virtualFwdDir.Normalize())
		{
			virtualRightDir = FVector::CrossProduct(gravityUp, virtualFwdDir);
		}
		else
		{
			virtualFwdDir = -virtualFwdDir.Rotation().Quaternion().GetAxisZ();
			FVector::CreateOrthonormalBasis(virtualFwdDir, virtualRightDir, gravityUp);
			virtualFwdDir.Normalize();
		}
		if (!virtualRightDir.Normalize())
		{
			if (DebugType == ControllerDebugType_MovementDebug)
			{
				GEngine->AddOnScreenDebugMessage(152, 1, FColor::Red, FString::Printf(TEXT("Cannot normalize right vector: up = %s, fwd= %s"), *gravityUp.ToCompactString(), *virtualFwdDir.ToCompactString()));
			}
			return outputCondition;
		}
		FRotator desiredRotator = UKismetMathLibrary::MakeRotFromZX(gravityUp, virtualFwdDir);

		virtualFwdDir = desiredRotator.Quaternion().GetAxisX();
		virtualRightDir = desiredRotator.Quaternion().GetAxisY();

		FQuat targetQuat = desiredRotator.Quaternion() * RotationOffset.Quaternion();

		outputCondition.Orientation = targetQuat;
	}

	//Update
	outputCondition = outputCondition.GetFinalCondition(inDelta);

	//Debug
	{
		if (DebugType == ControllerDebugType_MovementDebug)
		{
			auto acc = outputCondition.AngularAcceleration;

			auto spd = outputCondition.GetAngularSpeedQuat();
			FVector spdAxis;
			float spdAngle;
			spd.ToAxisAndAngle(spdAxis, spdAngle);

			auto rot = outputCondition.Orientation;
			FVector rotAxis;
			float rotAngle;
			rot.ToAxisAndAngle(rotAxis, rotAngle);


			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Rotation [ Axis: (%s), Angle: (%f)]"), *rotAxis.ToCompactString(), FMath::RadiansToDegrees(rotAngle)), true, true, FColor::Yellow, inDelta * 2, "Rot");
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Angular Velocity [ Axis: (%s), Angle: (%f)]"), *spdAxis.ToCompactString(), FMath::RadiansToDegrees(spdAngle)), true, true, FColor::Orange, inDelta * 2, "Spd");
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Angular Acceleration [ Axis: (%s), Angle: (%f)]"), *acc.GetSafeNormal().ToCompactString(), acc.Length()), true, true, FColor::Red, inDelta * 2, "Acc");
		}
	}

	return outputCondition;
}


FVector UModularControllerComponent::SlideAlongSurfaceAt(FHitResult& Hit, const FQuat rotation, FVector attemptedMove, int& depth, double deltaTime, float hullInflation)
{
	FVector initialLocation = Hit.Location;
	FVector endLocation = initialLocation;
	FVector originalMove = attemptedMove;
	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredComponent(Hit.GetComponent());

	//Compute slide vector
	FVector slideMove = ComputeSlideVector(originalMove, 1 - Hit.Time, Hit.Normal, Hit);

	if (DebugType == ControllerDebugType_MovementDebug)
	{
		UStructExtensions::DrawDebugCircleOnSurface(Hit, false, 43 + depth * 5, FColor::Green, deltaTime * 2, 1, false);
	}

	if ((slideMove | originalMove) > 0.f)
	{
		FHitResult primaryHit;

		//Check primary
		if (ComponentTraceCastSingle_Internal(primaryHit, initialLocation + Hit.Normal * 0.001, slideMove, rotation, hullInflation, bUseComplexCollision, queryParams))
		{
			queryParams.AddIgnoredComponent(primaryHit.GetComponent());

			// Compute new slide normal when hitting multiple surfaces.
			FVector firstHitLocation = primaryHit.Location - Hit.Normal * 0.001;
			FVector twoWallAdjust = originalMove * (1 - Hit.Time);
			TwoWallAdjust(twoWallAdjust, primaryHit, Hit.ImpactNormal);

			if (DebugType == ControllerDebugType_MovementDebug)
			{
				UStructExtensions::DrawDebugCircleOnSurface(primaryHit, false, 38 + depth * 5, FColor::Orange, deltaTime * 2, 1, false);
			}

			// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
			if ((twoWallAdjust | originalMove) > 0.f)
			{
				FHitResult secondaryMove;
				const FVector newNormal_rht = (Hit.ImpactPoint - primaryHit.ImpactPoint).GetSafeNormal();
				const FVector newNormal = FVector::VectorPlaneProject(Hit.Normal + primaryHit.Normal, newNormal_rht).GetSafeNormal();

				// Perform second move
				if (ComponentTraceCastSingle_Internal(secondaryMove, firstHitLocation + newNormal * 0.001, twoWallAdjust, rotation, hullInflation, bUseComplexCollision, queryParams))
				{
					queryParams.AddIgnoredComponent(secondaryMove.GetComponent());

					if (DebugType == ControllerDebugType_MovementDebug)
					{
						UStructExtensions::DrawDebugCircleOnSurface(secondaryMove, false, 33 + depth * 5, FColor::Black, deltaTime * 2, 1, false);
					}

					if (depth > 0)
					{
						depth--;
						secondaryMove.Location -= newNormal * 0.001;
						endLocation = SlideAlongSurfaceAt(secondaryMove, rotation, twoWallAdjust, depth, deltaTime, hullInflation);
						return endLocation;
					}
					else
					{
						endLocation = secondaryMove.Location - newNormal * 0.001;

						if (DebugType == ControllerDebugType_MovementDebug)
						{
							if (DebugType == ControllerDebugType_MovementDebug)
							{
								FVector midPoint = primaryHit.ImpactPoint + (Hit.ImpactPoint - primaryHit.ImpactPoint) * 0.5;
								midPoint = midPoint + (secondaryMove.ImpactPoint - midPoint) * 0.5;
								UKismetSystemLibrary::DrawDebugArrow(this, midPoint, midPoint + (newNormal * twoWallAdjust.Length()) / deltaTime, 50, FColor::Black, deltaTime * 2);
							}
						}

						return endLocation;
					}
				}
				else
				{
					endLocation = firstHitLocation + twoWallAdjust;
					if (DebugType == ControllerDebugType_MovementDebug)
					{
						FVector midPoint = primaryHit.ImpactPoint + (Hit.ImpactPoint - primaryHit.ImpactPoint) * 0.5;
						UKismetSystemLibrary::DrawDebugArrow(this, midPoint, midPoint + twoWallAdjust / deltaTime, 50, FColor::Orange, deltaTime * 2);
					}
					return endLocation;
				}
			}
			else
			{
				endLocation = firstHitLocation;
				if (DebugType == ControllerDebugType_MovementDebug)
				{
					FVector midPoint = primaryHit.ImpactPoint + (Hit.ImpactPoint - primaryHit.ImpactPoint) * 0.5;
					UKismetSystemLibrary::DrawDebugArrow(this, midPoint, midPoint + twoWallAdjust / deltaTime, 50, FColor::Yellow, deltaTime * 2);
				}
				return endLocation;
			}

		}
		else
		{
			endLocation = initialLocation + slideMove;
			if (DebugType == ControllerDebugType_MovementDebug)
			{
				UKismetSystemLibrary::DrawDebugArrow(this, initialLocation, initialLocation + slideMove / deltaTime, 50, FColor::Green, deltaTime * 2);
			}
			return endLocation;
		}
	}
	else
	{
		if (DebugType == ControllerDebugType_MovementDebug)
		{
			UKismetSystemLibrary::DrawDebugArrow(this, initialLocation, initialLocation + slideMove / deltaTime, 50, FColor::Cyan, deltaTime * 2);
		}
		return initialLocation;
	}
}


#pragma endregion



#pragma region Tools & Utils

bool UModularControllerComponent::ComponentTraceCastSingleUntil(FHitResult& outHit, FVector direction, FVector position,
	FQuat rotation, std::function<bool(FHitResult)> condition, int iterations, double inflation, bool traceComplex)
{
	FCollisionQueryParams queryParams = FCollisionQueryParams::DefaultQueryParam;
	for (int i = 0; i < iterations; i++)
	{
		FHitResult iterationHit;
		if (ComponentTraceCastSingle_Internal(iterationHit, position, direction, rotation, inflation, traceComplex, queryParams))
		{
			if (condition(iterationHit))
			{
				outHit = iterationHit;
				return true;
			}

			queryParams.AddIgnoredComponent(iterationHit.GetComponent());
			continue;
		}

		break;
	}
	return false;
}


bool UModularControllerComponent::ComponentTraceCastMulti_internal(TArray<FHitResult>& outHits, FVector position, FVector direction, FQuat rotation, double inflation, bool traceComplex, FCollisionQueryParams& queryParams)
{
	auto owner = GetOwner();
	if (owner == nullptr)
		return false;

	UPrimitiveComponent* primitive = UpdatedPrimitive;
	if (!primitive)
		return false;

	queryParams.AddIgnoredActor(owner);
	queryParams.bTraceComplex = traceComplex;
	queryParams.bReturnPhysicalMaterial = true;
	float OverlapInflation = inflation;
	auto shape = primitive->GetCollisionShape(OverlapInflation);

	if (GetWorld()->SweepMultiByChannel(outHits, position, position + direction, rotation, primitive->GetCollisionObjectType(), shape, queryParams, FCollisionResponseParams::DefaultResponseParam))
	{
		for (int i = 0; i < outHits.Num(); i++)
		{
			if (!outHits[i].IsValidBlockingHit())
				continue;
			//outHits[i].Location -= direction.GetSafeNormal() * 0.125f;
		}
	}

	queryParams.ClearIgnoredActors();
	return outHits.Num() > 0;
}


bool UModularControllerComponent::ComponentTraceCastSingle_Internal(FHitResult& outHit, FVector position, FVector direction, FQuat rotation, double inflation, bool traceComplex, FCollisionQueryParams& queryParams)
{
	outHit.Location = position;
	auto owner = GetOwner();
	if (owner == nullptr)
		return false;

	UPrimitiveComponent* primitive = UpdatedPrimitive;
	if (!primitive)
		return false;

	queryParams.AddIgnoredActor(owner);
	queryParams.bTraceComplex = traceComplex;
	queryParams.bReturnPhysicalMaterial = true;
	float OverlapInflation = inflation;
	auto shape = primitive->GetCollisionShape(OverlapInflation);

	if (GetWorld()->SweepSingleByChannel(outHit, position, position + direction, rotation, primitive->GetCollisionObjectType(), shape, queryParams))
	{
		//outHit.Location -= direction.GetSafeNormal() * inflation;
		queryParams.ClearIgnoredActors();
		return true;
	}
	else
	{
		queryParams.ClearIgnoredActors();
		return false;
	}

}


void UModularControllerComponent::PathCastComponent_Internal(TArray<FHitResult>& results, FVector start, TArray<FVector> pathPoints, bool stopOnHit, float skinWeight, bool debugRay, bool rotateAlongPath, bool bendOnCollision, bool traceComplex, FCollisionQueryParams& queryParams)
{
	if (pathPoints.Num() <= 0)
		return;


	auto owner = GetOwner();
	if (owner == nullptr)
		return;

	results.Empty();
	queryParams.AddIgnoredActor(owner);
	queryParams.bTraceComplex = traceComplex;
	queryParams.bReturnPhysicalMaterial = true;


	UPrimitiveComponent* primitive = UpdatedPrimitive;
	if (!primitive)
	{
		queryParams.ClearIgnoredActors();
		return;
	}
	auto shape = primitive->GetCollisionShape(skinWeight);

	for (int i = 0; i < pathPoints.Num(); i++)
	{
		FHitResult soloHit;
		FVector in = i <= 0 ? start : pathPoints[i - 1];
		FVector out = pathPoints[i];
		GetWorld()->SweepSingleByChannel(soloHit, in, out, rotateAlongPath ? (out - in).Rotation().Quaternion() : GetRotation()
			, primitive->GetCollisionObjectType(), shape, queryParams, FCollisionResponseParams::DefaultResponseParam);
		if (debugRay)
		{
			UKismetSystemLibrary::DrawDebugArrow(this, in, out, 15, soloHit.Component != nullptr ? FColor::Green : FColor::Silver, 0, 15);
			if (soloHit.Component != nullptr)
			{
				UKismetSystemLibrary::DrawDebugPoint(this, soloHit.ImpactPoint, 30, FColor::Green, 0);
				UKismetSystemLibrary::DrawDebugArrow(this, soloHit.ImpactPoint, soloHit.ImpactPoint + soloHit.ImpactNormal, 15, FColor::Red, 0, 15);
				UKismetSystemLibrary::DrawDebugArrow(this, soloHit.ImpactPoint, soloHit.ImpactPoint + soloHit.Normal, 15, FColor::Orange, 0, 15);
			}
		}
		results.Add(soloHit);
		if (stopOnHit && soloHit.IsValidBlockingHit())
		{
			break;
		}

		if (bendOnCollision && soloHit.IsValidBlockingHit())
		{
			FVector offset = soloHit.Location - out;
			for (int j = i; j < pathPoints.Num(); j++)
			{
				pathPoints[j] += offset + offset.GetSafeNormal();
			}
		}
	}

	queryParams.ClearIgnoredActors();
}


void UModularControllerComponent::PathCastLine(TArray<FHitResult>& results, FVector start, TArray<FVector> pathPoints, ECollisionChannel channel, bool stopOnHit, bool debugRay, bool bendOnCollision, bool traceComplex)
{
	if (pathPoints.Num() <= 0)
		return;


	auto owner = GetOwner();
	if (owner == nullptr)
		return;

	results.Empty();
	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(owner);
	queryParams.bTraceComplex = traceComplex;
	queryParams.bReturnPhysicalMaterial = true;

	for (int i = 0; i < pathPoints.Num(); i++)
	{
		FHitResult soloHit;
		FVector in = i <= 0 ? start : pathPoints[i - 1];
		FVector out = pathPoints[i];
		GetWorld()->LineTraceSingleByChannel(soloHit, in, out, channel, queryParams, FCollisionResponseParams::DefaultResponseParam);
		if (debugRay)
		{
			UKismetSystemLibrary::DrawDebugArrow(this, in, out, 15, soloHit.Component != nullptr ? FColor::Green : FColor::Silver, 0, 15);
			if (soloHit.Component != nullptr)
			{
				UKismetSystemLibrary::DrawDebugPoint(this, soloHit.ImpactPoint, 30, FColor::Green, 0);
				UKismetSystemLibrary::DrawDebugArrow(this, soloHit.ImpactPoint, soloHit.ImpactPoint + soloHit.ImpactNormal, 15, FColor::Red, 0, 15);
			}
		}
		results.Add(soloHit);
		if (stopOnHit && soloHit.IsValidBlockingHit())
		{
			break;
		}

		if (bendOnCollision && soloHit.IsValidBlockingHit())
		{
			FVector offset = soloHit.Location - out;
			for (int j = i; j < pathPoints.Num(); j++)
			{
				pathPoints[j] += offset + offset.GetSafeNormal();
			}
		}
	}
}


bool UModularControllerComponent::CheckPenetrationAt(FVector& separationForce, FVector& contactForce, FVector atPosition, FQuat withOrientation, UPrimitiveComponent* onlyThisComponent, double hullInflation, bool getVelocity)
{
	{
		FVector moveVec = FVector(0);
		FVector velVec = FVector(0);
		auto owner = GetOwner();
		if (owner == nullptr)
			return false;

		UPrimitiveComponent* primitive = UpdatedPrimitive;
		if (!primitive)
			return false;
		bool overlapFound = false;
		TArray<FOverlapResult> _overlaps;
		FComponentQueryParams comQueryParams;
		comQueryParams.AddIgnoredActor(owner);
		if (GetWorld()->OverlapMultiByChannel(_overlaps, atPosition, withOrientation, primitive->GetCollisionObjectType(), primitive->GetCollisionShape(hullInflation), comQueryParams))
		{
			FMTDResult depenetrationInfos;
			for (int i = 0; i < _overlaps.Num(); i++)
			{
				auto& overlap = _overlaps[i];

				if (!overlapFound)
					overlapFound = true;

				if (overlap.Component->GetOwner() == this->GetOwner())
					continue;

				if (DebugType == ControllerDebugType_MovementDebug)
				{
					FVector thisClosestPt;
					FVector compClosestPt;
					overlap.Component->GetClosestPointOnCollision(atPosition, compClosestPt);
					primitive->GetClosestPointOnCollision(compClosestPt, thisClosestPt);
					const FVector separationVector = compClosestPt - thisClosestPt;
					UKismetSystemLibrary::DrawDebugArrow(this, compClosestPt, compClosestPt + separationVector * 10, 1, FColor::Silver, 0, 0.1);
					if (overlap.GetActor())
					{
						UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Overlap Actor: (%s)"), *overlap.GetActor()->GetActorNameOrLabel()), true, true, FColor::White, 0, FName(FString::Printf(TEXT("Overlap_%s"), *overlap.GetActor()->GetActorNameOrLabel())));
					}
				}

				if (overlap.Component->ComputePenetration(depenetrationInfos, primitive->GetCollisionShape(hullInflation), atPosition, withOrientation))
				{
					if (DebugType == ControllerDebugType_MovementDebug)
					{
						if (overlap.GetActor())
						{
							UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Depentrate Actor: (%s)"), *overlap.GetActor()->GetActorNameOrLabel()), true, true, FColor::Silver, 0, FName(FString::Printf(TEXT("OverlapPenetration_%s"), *overlap.GetActor()->GetActorNameOrLabel())));
						}
					}

					const FVector depForce = depenetrationInfos.Direction * depenetrationInfos.Distance;
					FVector hullPt = PointOnShape(-depenetrationInfos.Direction, atPosition);

					if (DebugType == ControllerDebugType_MovementDebug)
					{
						UKismetSystemLibrary::DrawDebugArrow(this, hullPt, hullPt + depForce * 10, 100, FColor::White, 0.018, 0.5);
					}

					FVector overlapObjectForce = FVector(0);
					if (getVelocity)
					{
						UModularControllerComponent* otherModularComponent = nullptr;
						if (overlap.GetActor())
						{
							auto component = overlap.GetActor()->GetComponentByClass(UModularControllerComponent::StaticClass());
							if (component != nullptr)
							{
								otherModularComponent = Cast<UModularControllerComponent>(component);
							}
						}

						bool showDebug = false;

						if (overlap.GetComponent()->IsSimulatingPhysics())
						{
							const double compMass = overlap.GetComponent()->GetMass();
							overlapObjectForce = (overlap.GetComponent()->GetPhysicsLinearVelocityAtPoint(hullPt) * compMass).ProjectOnToNormal(depenetrationInfos.Direction);
							showDebug = true;
						}
						else if (otherModularComponent != nullptr)
						{
							overlapObjectForce = otherModularComponent->ControllerStatus.Kinematics.LinearKinematic.Acceleration.ProjectOnToNormal(depenetrationInfos.Direction);
							showDebug = true;
						}

						if (showDebug && DebugType == ControllerDebugType_MovementDebug)
						{
							UKismetSystemLibrary::DrawDebugArrow(this, hullPt, hullPt + overlapObjectForce, 100, FColor::Silver, 0.018, 1);
						}
					}

					if (onlyThisComponent == overlap.Component)
					{
						separationForce = depForce;
						contactForce = overlapObjectForce;
						return true;
					}
					moveVec += depForce;
					velVec += overlapObjectForce;
				}
			}
		}

		if (onlyThisComponent != nullptr)
		{
			separationForce = FVector(0);
			contactForce = FVector(0);
			return false;
		}

		separationForce = moveVec;
		contactForce = velVec;
		return overlapFound;
	}
}


FVector UModularControllerComponent::PointOnShape(FVector direction, const FVector inLocation, const float hullInflation)
{
	FVector bCenter;
	FVector bExtends;
	direction.Normalize();
	GetOwner()->GetActorBounds(true, bCenter, bExtends);
	FVector outterBoundPt = GetLocation() + direction * bExtends.Length() * 3;
	FVector onColliderPt;
	FVector offset = inLocation - GetLocation();
	if (UpdatedPrimitive != nullptr)
	{
		UpdatedPrimitive->GetClosestPointOnCollision(outterBoundPt, onColliderPt);
	}
	else
	{
		onColliderPt = outterBoundPt;
	}

	return onColliderPt + offset + direction * hullInflation;
}


double UModularControllerComponent::GetHitComponentMass(FHitResult hit)
{
	if (hit.IsValidBlockingHit() && hit.GetActor())
	{
		UModularControllerComponent* otherModularComponent = nullptr;
		const auto component = hit.GetActor()->GetComponentByClass(UModularControllerComponent::StaticClass());
		if (component != nullptr)
		{
			otherModularComponent = Cast<UModularControllerComponent>(component);
		}

		if (hit.Component->IsSimulatingPhysics())
		{
			return hit.Component->GetMass();
		}
		else if (otherModularComponent != nullptr)
		{
			return otherModularComponent->GetMass();
		}
	}

	return TNumericLimits<double>().Max();
}


#pragma endregion
