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
	SetTickGroup(ETickingGroup::TG_PrePhysics);
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

	//Init collider
	if (UpdatedPrimitive)
	{
		UpdatedPrimitive->SetGenerateOverlapEvents(true);
	}

	//Inputs
	_user_inputPool = NewObject<UInputEntryPool>(UInputEntryPool::StaticClass(), UInputEntryPool::StaticClass());

	//State behaviors
	StatesInstances.Empty();
	//if (GetNetRole() == ROLE_Authority)
	{
		for (int i = StateClasses.Num() - 1; i >= 0; i--)
		{
			if (StateClasses[i] == nullptr)
				continue;
			UBaseControllerState* instance = NewObject<UBaseControllerState>(StateClasses[i], StateClasses[i]);
			StatesInstances.Add(instance);
			//AddControllerState(StateClasses[i]);
		}
		StatesInstances.Sort([](UBaseControllerState& a, UBaseControllerState& b) { return a.GetPriority() > b.GetPriority(); });
	}
	//else
	//{
	//	ServerRequestStates(this);
	//}

	//Action behaviors
	ActionInstances.Empty();
	//if (GetNetRole() == ROLE_Authority)
	{
		for (int i = ActionClasses.Num() - 1; i >= 0; i--)
		{
			if (ActionClasses[i] == nullptr)
				continue;
			UBaseControllerAction* instance = NewObject<UBaseControllerAction>(ActionClasses[i], ActionClasses[i]);
			instance->InitializeAction();
			ActionInstances.Add(instance);
			//AddControllerAction(ActionClasses[i]);
		}
		//ActionInstances.Sort([](UBaseControllerAction& a, UBaseControllerAction& b) { return a.GetPriority() > b.GetPriority(); });
	}
	//else
	//{
	//	ServerRequestActions(this);
	//}

	//Init last move
	LastMoveMade = FKinematicInfos(GetOwner()->GetActorTransform(), FVelocity(), FSurfaceInfos());
	LastMoveMade.FinalTransform = LastMoveMade.InitialTransform;
	LastMoveMade.FinalVelocities = LastMoveMade.InitialVelocities;
	if (GetNetRole() == ROLE_Authority)
	{
		_lastCmdReceived.ToLocation = LastMoveMade.InitialTransform.GetLocation();
		_lastCmdReceived.ToRotation = LastMoveMade.InitialTransform.GetRotation().Rotator();
	}


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
		FKinematicInfos movement = FKinematicInfos(moveInp, GetGravity(), LastMoveMade, GetMass());
		movement.bUsePhysic = bUsePhysicAuthority;
		StandAloneUpdateComponent(moveInp, movement, _user_inputPool, delta);
		LastMoveMade = movement;
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



FKinematicInfos UModularControllerComponent::StandAloneUpdateComponent(FVector movementInput, FKinematicInfos& movementInfos, UInputEntryPool* usedInputPool, float delta, bool noCollision)
{
	auto controllerStatus = EvaluateControllerStatus(movementInfos, movementInput, usedInputPool, delta);
	FVelocity alteredMotion = ProcessStatus(controllerStatus, movementInfos, movementInput, usedInputPool, delta);

	EvaluateRootMotionOverride(alteredMotion, movementInfos, delta);
	const FQuat finalRot = HandleRotation(alteredMotion, movementInfos, delta);
	alteredMotion.Rotation = finalRot;
	if (usedInputPool)
		usedInputPool->UpdateInputs(delta);

	FVelocity resultingMove = EvaluateMove(movementInfos, alteredMotion, delta, noCollision);
	resultingMove._rooMotionScale = alteredMotion._rooMotionScale;
	PostMoveUpdate(movementInfos, resultingMove, CurrentStateIndex, delta);
	Move(movementInfos.FinalTransform.GetLocation(), movementInfos.FinalTransform.GetRotation(), delta);

	movementInfos.FinalTransform.SetComponents(UpdatedPrimitive->GetComponentRotation().Quaternion(), UpdatedPrimitive->GetComponentLocation(), UpdatedPrimitive->GetComponentScale());
	if (DebugType)
	{
		UKismetSystemLibrary::DrawDebugArrow(this, movementInfos.InitialTransform.GetLocation(), movementInfos.InitialTransform.GetLocation() + alteredMotion.ConstantLinearVelocity * 0.1f, 50, FColor::Magenta);
		DrawCircle(GetWorld(), movementInfos.FinalTransform.GetLocation(), alteredMotion.Rotation.GetAxisX(), alteredMotion.Rotation.GetAxisY(), FColor::Magenta, 35, 32, false, -1, 0, 2);
	}
	return  movementInfos;
}


FClientNetMoveCommand UModularControllerComponent::SimulateMoveCommand(FClientNetMoveCommand moveCmd, const FKinematicInfos fromKinematic, UInputEntryPool* usedInputPool, bool shouldSweep, FHitResult* hitResult, int customInitialStateIndex, int customInitialActionIndexes)
{
	FClientNetMoveCommand result = moveCmd;

	//Sweep chk
	if (shouldSweep)
	{
		FHitResult hit;
		auto currentLocation = UpdatedPrimitive->GetComponentLocation();
		ComponentTraceCastSingle(hit, currentLocation, moveCmd.FromLocation - currentLocation, moveCmd.FromRotation.Quaternion(), 0.1, bUseComplexCollision);
		if (hit.IsValidBlockingHit())
		{
			result.FromLocation = hit.Location;
			result.FromRotation = fromKinematic.FinalVelocities.Rotation.Rotator();
		}

		hitResult = &hit;
	}

	//move
	const FVector moveInp = result.userMoveInput;
	FKinematicInfos movement = FKinematicInfos(moveInp, GetGravity(), fromKinematic, GetMass());
	movement.InitialTransform.SetRotation(result.FromRotation.Quaternion());
	movement.InitialTransform.SetLocation(result.FromLocation);
	movement.InitialVelocities.ConstantLinearVelocity = result.WithVelocity;

	auto controllerStatus = EvaluateControllerStatus(movement, moveInp, usedInputPool, result.DeltaTime, result.ControllerStatus, true, customInitialStateIndex, customInitialActionIndexes);
	FVelocity alteredMotion = ProcessStatus(controllerStatus, movement, moveInp, usedInputPool, result.DeltaTime, controllerStatus.StateIndex, controllerStatus.ActionIndex);

	const FQuat finalRot = HandleRotation(alteredMotion, movement, result.DeltaTime);
	alteredMotion.Rotation = finalRot;

	const FVelocity resultingMove = EvaluateMove(movement, alteredMotion, result.DeltaTime);

	//post move
	{
		//Final velocities
		movement.FinalVelocities.ConstantLinearVelocity = resultingMove.ConstantLinearVelocity;
		movement.FinalVelocities.InstantLinearVelocity = resultingMove.InstantLinearVelocity;
		movement.FinalVelocities.Rotation = resultingMove.Rotation;

		//Position
		movement.FinalTransform = movement.InitialTransform;
		const FVector mov = (resultingMove.ConstantLinearVelocity * result.DeltaTime + resultingMove.InstantLinearVelocity);
		movement.FinalTransform.SetLocation(movement.InitialTransform.GetLocation() + mov);
		movement.FinalTransform.SetRotation(resultingMove.Rotation);

		//Root Motion
		movement.FinalVelocities._rooMotionScale = resultingMove._rooMotionScale;
	}

	result.FromLocation = movement.InitialTransform.GetLocation();
	result.ToLocation = movement.FinalTransform.GetLocation();

	result.FromRotation = movement.InitialTransform.GetRotation().Rotator();
	result.ToRotation = movement.FinalTransform.GetRotation().Rotator();

	result.ControllerStatus = controllerStatus;
	result.ToVelocity = movement.FinalVelocities.ConstantLinearVelocity;

	return result;
}


FStatusParameters UModularControllerComponent::EvaluateControllerStatus(FKinematicInfos kinematicInfos, FVector moveInput, UInputEntryPool* usedInputPool, float delta, FStatusParameters statusOverride, bool simulate, int simulatedInitialStateIndex, int simulatedInitialActionIndexes)
{
	//State
	auto statusInfos = statusOverride;
	int initialState = simulatedInitialStateIndex >= 0 ? simulatedInitialStateIndex : CurrentStateIndex;
	const auto stateIndex = CheckControllerStates(kinematicInfos, moveInput, usedInputPool, delta, simulate);
	int targetState = statusInfos.StateIndex < 0 ? stateIndex : statusInfos.StateIndex;
	if (TryChangeControllerState(initialState, targetState, kinematicInfos, moveInput, delta, simulate))
	{
		statusInfos.StateModifiers.Empty();
		statusInfos.StateIndex = targetState;
	}
	else
	{
		statusInfos.StateIndex = initialState;
	}

	//Actions
	const int initialActionIndex = simulatedInitialActionIndexes >= 0 ? simulatedInitialActionIndexes : CurrentActionIndex;
	bool actionSelfTransition = false;
	const auto actionIndex = CheckControllerActions(kinematicInfos, moveInput, usedInputPool, statusInfos.StateIndex, initialActionIndex, delta, actionSelfTransition, simulate);
	const int targetActionIndex = statusInfos.ActionIndex < 0 ? actionIndex : statusInfos.ActionIndex;
	if (TryChangeControllerAction(initialActionIndex, targetActionIndex, kinematicInfos, moveInput, delta, actionSelfTransition, simulate))
	{
		statusInfos.ActionsModifiers.Empty();
		statusInfos.ActionIndex = targetActionIndex;
	}
	else
	{
		statusInfos.ActionIndex = initialActionIndex;
	}

	return statusInfos;
}


FVelocity UModularControllerComponent::ProcessStatus(FStatusParameters& inStatus,
	FKinematicInfos kinematicInfos, FVector moveInput, UInputEntryPool* usedInputPool, float delta, int simulatedStateIndex, int simulatedActionIndexes)
{
	const FVelocity primaryMotion = ProcessControllerState(inStatus, kinematicInfos, moveInput, delta, simulatedStateIndex);
	FVelocity alteredMotion = ProcessControllerAction(inStatus, kinematicInfos, primaryMotion, moveInput, delta, simulatedStateIndex, simulatedActionIndexes);
	return  alteredMotion;
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

void UModularControllerComponent::ListenInput(const FName key, const FInputEntry entry)
{
	if (_ownerPawn == nullptr)
		return;
	if (!_ownerPawn->IsLocallyControlled())
		return;
	if (_user_inputPool)
		_user_inputPool->AddOrReplace(key, entry);
}

void UModularControllerComponent::ListenButtonInput(const FName key, const float buttonBufferTime)
{
	if (!key.IsValid())
		return;
	FInputEntry entry;
	entry.Nature = EInputEntryNature::InputEntryNature_Button;
	entry.Type = buttonBufferTime > 0 ? EInputEntryType::InputEntryType_Buffered : EInputEntryType::InputEntryType_Simple;
	entry.InputBuffer = buttonBufferTime;
	ListenInput(key, entry);
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
	if (DebugType == ControllerDebugType_InputDebug)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Consumed Move Input: %s"), *move.ToCompactString()), true, true, FColor::Silver, 0, "MoveInput_");
	}
	return move;
}

FInputEntry UModularControllerComponent::ReadInput(const FName key, bool consume, bool debug, UObject* worldContext)
{
	if (debug && !worldContext)
		worldContext = GetWorld();
	if (!_user_inputPool)
		return {};
	if (consume)
		return _user_inputPool->ConsumeInput(key, debug && DebugType == ControllerDebugType_InputDebug, worldContext);
	return _user_inputPool->ReadInput(key, debug && DebugType == ControllerDebugType_InputDebug, worldContext);
}

bool UModularControllerComponent::ReadButtonInput(const FName key, bool consume, bool debug, UObject* worldContext)
{
	const FInputEntry entry = ReadInput(key, consume, debug && DebugType == ControllerDebugType_InputDebug, worldContext);
	return entry.Phase == EInputEntryPhase::InputEntryPhase_Held || entry.Phase == EInputEntryPhase::InputEntryPhase_Pressed;
}

float UModularControllerComponent::ReadValueInput(const FName key, bool consume, bool debug, UObject* worldContext)
{
	const FInputEntry entry = ReadInput(key, consume, debug && DebugType == ControllerDebugType_InputDebug, worldContext);
	return entry.Axis.X;
}

FVector UModularControllerComponent::ReadAxisInput(const FName key, bool consume, bool debug, UObject* worldContext)
{
	const FInputEntry entry = ReadInput(key, consume, debug && DebugType == ControllerDebugType_InputDebug, worldContext);
	return entry.Axis;
}

#pragma endregion



#pragma region Network Logic XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

#pragma region Common Logic

void UModularControllerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//DOREPLIFETIME(UModularControllerComponent, StatesInstances);
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


void UModularControllerComponent::MultiCastMoveCommand_Implementation(FClientNetMoveCommand command, FServerNetCorrectionData Correction, bool asCorrection)
{
	const ENetRole role = GetNetRole();
	switch (role)
	{
	case ENetRole::ROLE_Authority:
	{
	} break;
	case ENetRole::ROLE_AutonomousProxy:
	{
		if (asCorrection)
		{
			_lastCorrectionReceived = Correction;
			if (DebugType == ControllerDebugType_NetworkDebug)
			{
				UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Autonomous Proxy Received Correction Stamped: %f"), Correction.TimeStamp), true, true, FColor::Orange, 5, TEXT("MultiCastMoveCommand_1"));
			}
		}

		_lastCmdReceived = command;
	}
	break;

	default:
	{
		_lastCmdReceived = command;
		if (DebugType == ControllerDebugType_NetworkDebug)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Simulated Proxy Received Command Stamped: %f"), command.TimeStamp), true, true, FColor::Cyan, 1, TEXT("MultiCastMoveCommand_2"));
		}
	}
	break;
	}
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
	const FVector moveInp = ConsumeMovementInput();
	FKinematicInfos movement = FKinematicInfos(moveInp, GetGravity(), LastMoveMade, GetMass());

	//Execute the move
	movement.bUsePhysic = bUsePhysicAuthority;
	auto statusInfos = EvaluateControllerStatus(movement, moveInp, _user_inputPool, delta);
	//
	{
		FVelocity alteredMotion = ProcessStatus(statusInfos, movement, moveInp, _user_inputPool, delta);

		EvaluateRootMotionOverride(alteredMotion, movement, delta);
		alteredMotion.Rotation = HandleRotation(alteredMotion, movement, delta);
		if (_user_inputPool)
			_user_inputPool->UpdateInputs(delta);

		FVelocity resultingMove = EvaluateMove(movement, alteredMotion, delta);
		resultingMove._rooMotionScale = alteredMotion._rooMotionScale;
		PostMoveUpdate(movement, resultingMove, CurrentStateIndex, delta);
		Move(movement.FinalTransform.GetLocation(), movement.FinalTransform.GetRotation(), delta);

		movement.FinalTransform.SetComponents(UpdatedPrimitive->GetComponentRotation().Quaternion(), UpdatedPrimitive->GetComponentLocation(), UpdatedPrimitive->GetComponentScale());
	}
	LastMoveMade = movement;
	auto moveCmd = FClientNetMoveCommand(_timeElapsed, delta, moveInp, LastMoveMade, statusInfos);

	if (_lastCmdReceived.HasChanged(moveCmd, 1, 5) || !_startPositionSet)
	{
		_startPositionSet = true;
		_lastCmdReceived = moveCmd;
		MultiCastMoveCommand(moveCmd);
		if (DebugType == ControllerDebugType_NetworkDebug)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Listen Send Command Stamped: %f"), moveCmd.TimeStamp), true, true, FColor::White, 1, TEXT("ListenServerUpdateComponent"));
		}
	}
}


#pragma endregion

#pragma region Dedicated OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO


void UModularControllerComponent::DedicatedServerUpdateComponent(float delta)
{
	FHitResult initialChk;
	bool madeCorrection = false;
	bool ackCorrection = false;

	//Verification
	{
		if (_servercmdCheckPool.Num() > 0)
		{
			if (bUseClientAuthorative)
			{
				_lastCmdReceived = _servercmdCheckPool[0];
				_servercmdCheckPool.RemoveAt(0);
			}
			else
			{
				_lastCmdReceived = _servercmdCheckPool[0];
				if (ComponentTraceCastSingle(initialChk, UpdatedComponent->GetComponentLocation(), _lastCmdReceived.FromLocation - UpdatedComponent->GetComponentLocation(), _lastCmdReceived.FromRotation.Quaternion()))
				{
					madeCorrection = true;
				}
				else if (ComponentTraceCastSingle(initialChk, _lastCmdReceived.FromLocation, _lastCmdReceived.ToLocation - _lastCmdReceived.FromLocation, _lastCmdReceived.ToRotation.Quaternion()))
				{
					madeCorrection = true;
				}

				if (madeCorrection)
				{
					_lastCmdReceived.FromLocation = initialChk.TraceStart;
					_lastCmdReceived.ToLocation = initialChk.Location;
				}
				else
				{
					ackCorrection = _lastCmdReceived.CorrectionAckowledgement;
				}

				_servercmdCheckPool.RemoveAt(0);
			}
		}
	}


	FKinematicInfos movement = FKinematicInfos(_lastCmdReceived.userMoveInput, GetGravity(), LastMoveMade, GetMass());

	//Move
	FVector currentLocation = UpdatedPrimitive->GetComponentLocation();
	FVector targetLocation = _lastCmdReceived.ToLocation;
	FVector lerpLocation = FMath::Lerp(currentLocation, targetLocation, delta * AdjustmentSpeed);
	FHitResult sweepHit;
	UpdatedPrimitive->SetWorldLocation(lerpLocation, true, &sweepHit);
	movement.InitialTransform.SetLocation(UpdatedPrimitive->GetComponentLocation());

	//Rotate
	FQuat currentRotation = UpdatedPrimitive->GetComponentRotation().Quaternion();
	FQuat targetRotation = _lastCmdReceived.ToRotation.Quaternion();
	FQuat slerpRot = FQuat::Slerp(currentRotation, targetRotation, delta * AdjustmentSpeed);
	UpdatedPrimitive->SetWorldRotation(slerpRot.Rotator());
	movement.InitialTransform.SetRotation(currentRotation);
	movement.FinalVelocities.Rotation = targetRotation;

	//Velocity
	movement.FinalVelocities.ConstantLinearVelocity = _lastCmdReceived.WithVelocity;

	//Status
	EvaluateControllerStatus(movement, _lastCmdReceived.userMoveInput, _user_inputPool, delta, _lastCmdReceived.ControllerStatus);
	auto copyOfStatus = _lastCmdReceived.ControllerStatus;
	ProcessStatus(copyOfStatus, movement, _lastCmdReceived.userMoveInput, _user_inputPool, delta);

	PostMoveUpdate(movement, movement.FinalVelocities, _lastCmdReceived.ControllerStatus.StateIndex, delta);
	LastMoveMade = movement;

	//Network
	if (_lastCmdExecuted.HasChanged(_lastCmdReceived, 1, 5) || !_startPositionSet)
	{
		//Set a time stamp to be able to initialize client
		if (!_startPositionSet)
			_lastCmdReceived.TimeStamp = _timeElapsed;

		_lastCmdExecuted = _lastCmdReceived;
		if (madeCorrection || ackCorrection)
		{
			auto hitResult = initialChk.IsValidBlockingHit() ? initialChk : sweepHit;
			FServerNetCorrectionData correction = FServerNetCorrectionData(ackCorrection ? 0 : _lastCmdReceived.TimeStamp, LastMoveMade, &hitResult);
			MultiCastMoveCommand(_lastCmdReceived, correction, true);
		}
		else
		{
			MultiCastMoveCommand(_lastCmdReceived);
		}

		if (DebugType == ControllerDebugType_NetworkDebug)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Dedicated Send Command Stamped: %f as correction? %d"), _lastCmdReceived.TimeStamp, madeCorrection), true, true, FColor::White, 1, TEXT("DedicatedServerUpdateComponent"));
		}
	}
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


void UModularControllerComponent::ServerCastMoveCommand_Implementation(FClientNetMoveCommand command)
{
	//Stop trying to initialize when receiving the first move request.
	_startPositionSet = true;

	_servercmdCheckPool.Add(command);
	if (bUseClientAuthorative)
	{
		MultiCastMoveCommand(command);
	}

	if (DebugType == ControllerDebugType_NetworkDebug)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Dedicated received command Stamped: %f"), command.TimeStamp), true, true, FColor::Black, 1, TEXT("ServerCastMoveCommand"));
	}
}


void UModularControllerComponent::AutonomousProxyUpdateComponent(float delta)
{
	//Handle Starting Location
	{
		if (_lastCmdReceived.TimeStamp == 0 && !_startPositionSet)
			return;
		if (!_startPositionSet)
		{
			UpdatedComponent->SetWorldLocationAndRotation(_lastCmdReceived.ToLocation, _lastCmdReceived.ToRotation);
			LastMoveMade.FinalTransform.SetComponents(_lastCmdReceived.ToRotation.Quaternion(), _lastCmdReceived.ToLocation, LastMoveMade.FinalTransform.GetScale3D());
			_startPositionSet = true;
		}
	}


	////Correction
	bool corrected = false;
	if (!bUseClientAuthorative)
	{
		FClientNetMoveCommand cmdBefore;
		if (_clientcmdHistory.Num() > 0)
		{
			cmdBefore = _clientcmdHistory[_clientcmdHistory.Num() - 1];
		}
		FClientNetMoveCommand correctionCmd = cmdBefore;
		if (_lastCorrectionReceived.ApplyCorrectionRecursive(_clientcmdHistory, correctionCmd))
		{
			if (cmdBefore.HasChanged(correctionCmd))
			{
				corrected = true;

				LastMoveMade.FinalTransform.SetComponents(correctionCmd.ToRotation.Quaternion(), correctionCmd.ToLocation, FVector::OneVector);
				LastMoveMade.FinalVelocities.ConstantLinearVelocity = correctionCmd.ToVelocity;
				LastMoveMade.FinalVelocities.Rotation = LastMoveMade.InitialTransform.GetRotation();

				if (DebugType == ControllerDebugType_NetworkDebug)
				{
					UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Autonomous Set Correction to Stamped: %f"), _lastCorrectionReceived.TimeStamp), true, true, FColor::Orange, 1, TEXT("AutonomousProxyUpdateComponent_correction_1"));
				}
			}
		}

		if (DebugType == ControllerDebugType_NetworkDebug)
		{
			DrawDebugCapsule(GetWorld(), correctionCmd.FromLocation, 90, 40, correctionCmd.FromRotation.Quaternion(), FColor::Orange, false, 1);
			DrawDebugDirectionalArrow(GetWorld(), correctionCmd.FromLocation, correctionCmd.FromLocation + correctionCmd.FromRotation.Vector() * 40, 20, FColor::Red, false, -1);

			auto acceleration = correctionCmd.ToVelocity.Length() - correctionCmd.WithVelocity.Length();
			FLinearColor accColor = FLinearColor(-acceleration, acceleration, 0);
			DrawDebugLine(GetWorld(), correctionCmd.FromLocation + FVector::UpVector * 15, correctionCmd.ToLocation + FVector::UpVector * 15, accColor.ToFColor(true), false, -1);
		}
	}


	const FVector moveInp = ConsumeMovementInput();
	FKinematicInfos movement = FKinematicInfos(moveInp, GetGravity(), LastMoveMade, GetMass());

	//Execute the move
	movement.bUsePhysic = bUsePhysicAuthority;
	auto statusInfos = EvaluateControllerStatus(movement, moveInp, _user_inputPool, delta);
	//
	{
		FVelocity alteredMotion = ProcessStatus(statusInfos, movement, moveInp, _user_inputPool, delta);

		EvaluateRootMotionOverride(alteredMotion, movement, delta);
		alteredMotion.Rotation = HandleRotation(alteredMotion, movement, delta);
		if (_user_inputPool)
			_user_inputPool->UpdateInputs(delta);

		FVelocity resultingMove = EvaluateMove(movement, alteredMotion, delta);
		if (_lastCorrectionReceived.CollisionOccured)
		{
			const bool tryMoveThroughObstacle_Linear = FVector::DotProduct(resultingMove.ConstantLinearVelocity, _lastCorrectionReceived.CollisionNormal) <= 0;
			if (tryMoveThroughObstacle_Linear)
				resultingMove.ConstantLinearVelocity = FVector::VectorPlaneProject(resultingMove.ConstantLinearVelocity, _lastCorrectionReceived.CollisionNormal);

			const bool tryMoveThroughObstacle_Instant = FVector::DotProduct(resultingMove.InstantLinearVelocity, _lastCorrectionReceived.CollisionNormal) <= 0;
			if (tryMoveThroughObstacle_Instant)
				resultingMove.InstantLinearVelocity = FVector::VectorPlaneProject(resultingMove.InstantLinearVelocity, _lastCorrectionReceived.CollisionNormal);
		}
		resultingMove._rooMotionScale = alteredMotion._rooMotionScale;
		PostMoveUpdate(movement, resultingMove, CurrentStateIndex, delta);

		if (bUseClientAuthorative)
		{
			Move(movement.FinalTransform.GetLocation(), movement.FinalTransform.GetRotation(), delta);
			movement.FinalTransform.SetComponents(UpdatedPrimitive->GetComponentRotation().Quaternion(), UpdatedPrimitive->GetComponentLocation()
				, UpdatedPrimitive->GetComponentScale());
		}
		else
		{
			const FVector lerpPos = FMath::Lerp(UpdatedComponent->GetComponentLocation(), movement.FinalTransform.GetLocation(), delta * AdjustmentSpeed);
			const FQuat slerpRot = FQuat::Slerp(UpdatedComponent->GetComponentQuat(), movement.FinalTransform.GetRotation(), delta * AdjustmentSpeed);
			UpdatedComponent->SetWorldLocationAndRotation(lerpPos, slerpRot);
		}
	}
	LastMoveMade = movement;
	auto moveCmd = FClientNetMoveCommand(_timeElapsed, delta, moveInp, LastMoveMade, statusInfos);

	//Changes and Network
	if (_lastCmdExecuted.HasChanged(moveCmd, 1, 5))
	{
		_lastCmdExecuted = moveCmd;
		_clientcmdHistory.Add(moveCmd);
		if (!corrected)
		{
			moveCmd.CorrectionAckowledgement = _lastCorrectionReceived.TimeStamp != 0;
			ServerCastMoveCommand(moveCmd);
			if (DebugType == ControllerDebugType_NetworkDebug)
			{
				UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Autonomous Send Command Stamped: %f"), moveCmd.TimeStamp), true, true, FColor::Orange, 1, TEXT("AutonomousProxyUpdateComponent"));
			}
		}
	}
}

#pragma endregion

#pragma region Simulated Proxy OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO


void UModularControllerComponent::SimulatedProxyUpdateComponent(float delta)
{
	FKinematicInfos movement = FKinematicInfos(_lastCmdReceived.userMoveInput, GetGravity(), LastMoveMade, GetMass());

	//Move
	FVector currentLocation = UpdatedPrimitive->GetComponentLocation();
	FVector targetLocation = _lastCmdReceived.ToLocation;
	FVector lerpLocation = FMath::Lerp(currentLocation, targetLocation, delta * AdjustmentSpeed);
	UpdatedPrimitive->SetWorldLocation(lerpLocation);
	movement.InitialTransform.SetLocation(currentLocation);

	//Rotate
	FQuat currentRotation = UpdatedPrimitive->GetComponentRotation().Quaternion();
	FQuat targetRotation = _lastCmdReceived.ToRotation.Quaternion();
	FQuat slerpRot = FQuat::Slerp(currentRotation, targetRotation, delta * AdjustmentSpeed);
	UpdatedPrimitive->SetWorldRotation(slerpRot.Rotator());
	movement.InitialTransform.SetRotation(currentRotation);
	movement.FinalVelocities.Rotation = targetRotation;

	//Velocity
	movement.FinalVelocities.ConstantLinearVelocity = _lastCmdReceived.WithVelocity;

	//Status
	EvaluateControllerStatus(movement, _lastCmdReceived.userMoveInput, _user_inputPool, delta, _lastCmdReceived.ControllerStatus);
	auto copyOfStatus = _lastCmdReceived.ControllerStatus;
	ProcessStatus(copyOfStatus, movement, _lastCmdReceived.userMoveInput, _user_inputPool, delta);

	PostMoveUpdate(movement, movement.FinalVelocities, _lastCmdReceived.ControllerStatus.StateIndex, delta);
	LastMoveMade = movement;
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
		if (DebugType)
			GEngine->AddOnScreenDebugMessage((int32)GetOwner()->GetUniqueID() + 9, 1, FColor::Green, FString::Printf(TEXT("Overlaped With: %s"), *OtherActor->GetActorNameOrLabel()));
	}
}


void UModularControllerComponent::BeginCollision(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor != nullptr && DebugType)
		GEngine->AddOnScreenDebugMessage((int32)GetOwner()->GetUniqueID() + 10, 1, FColor::Green, FString::Printf(TEXT("Collision With: %s"), *OtherActor->GetActorNameOrLabel()));

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
			_collisionForces += OtherComp->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);
		}
		else if (otherModularComponent != nullptr)
		{
			_collisionForces += otherModularComponent->Velocity;
		}
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


int UModularControllerComponent::CheckControllerStates(FKinematicInfos& inDatas, FVector moveInput, UInputEntryPool* inputs, const float inDelta, bool simulation
	, int simulatedCurrentStateIndex, int simulatedActiveActionIndex)
{
	int maxStatePriority = -1;
	int selectedStateIndex = -1;
	bool disableStateWasLastFrameStateStatus = false;


	//Check if a State's check have success state
	{
		//Check if a valid action freeze the current state
		if (selectedStateIndex < 0)
		{
			int activeActionIndex = CurrentActionIndex;
			if (simulatedActiveActionIndex >= 0)
			{
				activeActionIndex = simulatedActiveActionIndex;
			}

			if (ActionInstances.IsValidIndex(activeActionIndex) && ActionInstances[activeActionIndex])
			{
				//Find state freeze
				if (ActionInstances[activeActionIndex]->bFreezeCurrentState)
				{
					selectedStateIndex = simulatedCurrentStateIndex < 0 ? CurrentStateIndex : simulatedCurrentStateIndex;
				}

				//Find last frame status voider
				if (ActionInstances[activeActionIndex]->bShouldControllerStateCheckOverride)
				{
					disableStateWasLastFrameStateStatus = true;
				}
			}
		}

		if (selectedStateIndex < 0)
		{
			for (int i = 0; i < StatesInstances.Num(); i++)
			{
				if (StatesInstances[i] == nullptr)
					continue;

				//Don't event check lower priorities
				if (StatesInstances[i]->GetPriority() < maxStatePriority)
				{
					continue;
				}

				//Handle state snapshot
				if (simulation)
					StatesInstances[i]->SaveStateSnapShot();
				else
					StatesInstances[i]->RestoreStateFromSnapShot();

				if (StatesInstances[i]->CheckState(inDatas, moveInput, inputs, this, inDelta, disableStateWasLastFrameStateStatus ? 0 : -1))
				{
					selectedStateIndex = i;
					maxStatePriority = StatesInstances[i]->GetPriority();
				}
			}
		}

	}

	return selectedStateIndex;
}


bool UModularControllerComponent::TryChangeControllerState(int fromStateIndex, int toStateIndex, FKinematicInfos& inDatas, FVector moveInput, const float inDelta, bool simulate)
{
	if (fromStateIndex == toStateIndex)
		return false;

	if (!StatesInstances.IsValidIndex(toStateIndex))
		return false;

	//Landing
	StatesInstances[toStateIndex]->OnEnterState(inDatas, moveInput, this, inDelta);
	if (!simulate)
		LinkAnimBlueprint(GetSkeletalMesh(), "State", StatesInstances[toStateIndex]->StateBlueprintClass);
	StatesInstances[toStateIndex]->SetWasTheLastFrameControllerState(true);

	if (StatesInstances.IsValidIndex(fromStateIndex))
	{
		//Leaving
		StatesInstances[fromStateIndex]->OnExitState(inDatas, moveInput, this, inDelta);
		StatesInstances[fromStateIndex]->SurfaceInfos.Reset();
	}

	for (int i = 0; i < StatesInstances.Num(); i++)
	{
		if (i == toStateIndex)
			continue;

		StatesInstances[i]->SetWasTheLastFrameControllerState(false);
		if (!simulate)
		{
			StatesInstances[i]->OnControllerStateChanged(StatesInstances.IsValidIndex(toStateIndex) ? StatesInstances[toStateIndex]->GetDescriptionName() : "", StatesInstances.IsValidIndex(toStateIndex) ? StatesInstances[toStateIndex]->GetPriority() : -1, this);
		}
	}

	if (!simulate)
	{
		OnControllerStateChanged(StatesInstances.IsValidIndex(toStateIndex) ? StatesInstances[toStateIndex] : nullptr
			, StatesInstances.IsValidIndex(fromStateIndex) ? StatesInstances[fromStateIndex] : nullptr);
		OnControllerStateChangedEvent.Broadcast(StatesInstances.IsValidIndex(toStateIndex) ? StatesInstances[toStateIndex] : nullptr
			, StatesInstances.IsValidIndex(fromStateIndex) ? StatesInstances[fromStateIndex] : nullptr);
	}

	if (!simulate)
	{
		//Notify actions the change of state
		for (int i = 0; i < ActionInstances.Num(); i++)
		{
			ActionInstances[i]->OnStateChanged(StatesInstances.IsValidIndex(toStateIndex) ? StatesInstances[toStateIndex] : nullptr
				, StatesInstances.IsValidIndex(fromStateIndex) ? StatesInstances[fromStateIndex] : nullptr);
		}
	}

	if (!simulate)
	{
		CurrentStateIndex = toStateIndex;
	}

	return true;
}


FVelocity UModularControllerComponent::ProcessControllerState(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVector moveInput, const float inDelta, int simulatedStateIndex)
{
	FVelocity movement = inDatas.InitialVelocities;
	int index = simulatedStateIndex >= 0 ? simulatedStateIndex : CurrentStateIndex;

	if (StatesInstances.IsValidIndex(index))
	{
		//Handle state snapshot
		if (simulatedStateIndex >= 0)
			StatesInstances[index]->SaveStateSnapShot();
		else
			StatesInstances[index]->RestoreStateFromSnapShot();

		FVelocity processMotion = movement;
		processMotion = StatesInstances[index]->ProcessState(controllerStatus, inDatas, moveInput, this, inDelta);

		if (StatesInstances[index]->RootMotionMode != ERootMotionType::RootMotionType_No_RootMotion)
		{
			//Rotation
			processMotion.Rotation *= GetRootMotionQuat(GetSkeletalMesh());

			FVelocity rootMotion = processMotion;
			float rootMotionScale = processMotion._rooMotionScale;
			float processMotionScale = 1 - processMotion._rooMotionScale;

			//Translation
			switch (StatesInstances[index]->RootMotionMode)
			{
			case RootMotionType_AdditivePrimary:
			{
				rootMotion.ConstantLinearVelocity += GetRootMotionVector(GetSkeletalMesh()) * rootMotionScale;
			}
			break;
			case RootMotionType_AdditiveSecondary:
			{
				rootMotion.InstantLinearVelocity += GetRootMotionVector(GetSkeletalMesh()) * rootMotionScale
					* inDelta;
			}
			break;
			case RootMotionType_OverridePrimary:
			{
				rootMotion.ConstantLinearVelocity = (GetRootMotionVector(GetSkeletalMesh()) * rootMotionScale
					* 1 / (inDelta)) + processMotion.ConstantLinearVelocity * processMotionScale;
			}
			break;
			case RootMotionType_OverrideSecondary:
			{
				rootMotion.InstantLinearVelocity = GetRootMotionVector(GetSkeletalMesh()) * rootMotionScale *
					inDelta + processMotion.InstantLinearVelocity * processMotionScale;
			}
			break;
			case RootMotionType_OverrideAll:
			{
				rootMotion.ConstantLinearVelocity = (GetRootMotionVector(GetSkeletalMesh()) * rootMotionScale
					* 1 / (inDelta)) + processMotion.ConstantLinearVelocity * processMotionScale;
				processMotion.InstantLinearVelocity = FVector::ZeroVector;
			}
			break;
			}

			//Fuse RM
			processMotion = rootMotion;
		}

		movement = processMotion;
	}
	else
	{
		movement.ConstantLinearVelocity = inDatas.GetInitialMomentum();
	}

	return movement;
}


void UModularControllerComponent::OnControllerStateChanged_Implementation(UBaseControllerState* OldOne, UBaseControllerState* NewOne)
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


int UModularControllerComponent::CheckControllerActions(FKinematicInfos& inDatas, FVector moveInput,
	UInputEntryPool* inputs, const int controllerStateIndex, const int controllerActionIndex, const float inDelta, bool& transitionToSelf, bool simulation)
{
	int activeActionIndex = -1;

	////Check active action still active
	activeActionIndex = controllerActionIndex;
	if (ActionInstances.IsValidIndex(activeActionIndex))
	{
		if (ActionInstances[activeActionIndex]->CurrentPhase == ActionPhase_Recovery
			&& ActionInstances[activeActionIndex]->bCanTransitionToSelf
			&& CheckActionCompatibility(ActionInstances[activeActionIndex], controllerStateIndex, controllerActionIndex)
			&& ActionInstances[activeActionIndex]->CheckAction_Internal(inDatas, moveInput, inputs, this, inDelta))
		{
			transitionToSelf = true;
		}

		if (ActionInstances[activeActionIndex]->GetRemainingActivationTime() <= 0)
		{
			activeActionIndex = -1;
		}
	}
	else
	{
		activeActionIndex = -1;
	}

	//Check actions
	for (int i = 0; i < ActionInstances.Num(); i++)
	{
		if (activeActionIndex == i)
			continue;

		if (ActionInstances[i] == nullptr)
			continue;

		if (ActionInstances.IsValidIndex(activeActionIndex))
		{
			if (ActionInstances[i]->GetPriority() <= ActionInstances[activeActionIndex]->GetPriority())
			{
				if (ActionInstances[i]->GetPriority() != ActionInstances[activeActionIndex]->GetPriority())
					continue;
				if (ActionInstances[i]->GetPriority() == ActionInstances[activeActionIndex]->GetPriority() && ActionInstances[activeActionIndex]->CurrentPhase != ActionPhase_Recovery)
					continue;
			}
		}

		//Handle state snapshot
		if (simulation)
			ActionInstances[i]->SaveActionSnapShot();
		else
			ActionInstances[i]->RestoreActionFromSnapShot();

		if (CheckActionCompatibility(ActionInstances[i], controllerStateIndex, controllerActionIndex)
			&& ActionInstances[i]->CheckAction_Internal(inDatas, moveInput, inputs, this, inDelta))
		{
			activeActionIndex = i;

			if (!simulation && DebugType == ControllerDebugType_StatusDebug)
			{
				UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Action (%s) was checked as active. Remaining Time: %f"), *ActionInstances[i]->DebugString(), ActionInstances[i]->GetRemainingActivationTime()), true, true, FColor::Silver, 0
					, FName(FString::Printf(TEXT("CheckControllerActions_%s"), *ActionInstances[i]->GetDescriptionName().ToString())));
			}
		}

	}

	if (!simulation && DebugType == ControllerDebugType_StatusDebug)
	{
		UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Check Action Phase: %d"), activeActionIndex), true, true, FColor::Silver, 0
			, TEXT("CheckControllerActions"));
	}

	return activeActionIndex;
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



bool UModularControllerComponent::TryChangeControllerAction(int fromActionIndex, int toActionIndex,
	FKinematicInfos& inDatas, FVector moveInput, const float inDelta, const bool transitionToSelf, bool simulate)
{
	if (fromActionIndex == toActionIndex)
	{
		if (!transitionToSelf)
			return false;
	}

	if (!simulate && DebugType == ControllerDebugType_StatusDebug)
	{
		UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Trying to change action from: %d to: %d"), fromActionIndex, toActionIndex), true, true
			, FColor::White, 5, TEXT("TryChangeControllerActions_1"));
	}

	//Disable last action
	if (ActionInstances.IsValidIndex(fromActionIndex))
	{
		ActionInstances[fromActionIndex]->SetActivatedLastFrame(false);
		ActionInstances[fromActionIndex]->OnActionEnds_Internal(inDatas, moveInput, this, inDelta);
		if (!simulate)
		{
			if (DebugType == ControllerDebugType_StatusDebug)
			{
				UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Action (%s) is Being Disabled. Remaining Time: %f"), *ActionInstances[fromActionIndex]->DebugString(), ActionInstances[fromActionIndex]->GetRemainingActivationTime()), true, true, FColor::Red, 5
					, FName(FString::Printf(TEXT("TryChangeControllerActions_%s"), *ActionInstances[fromActionIndex]->GetDescriptionName().ToString())));
			}
		}
	}

	//Activate action
	if (ActionInstances.IsValidIndex(toActionIndex))
	{
		ActionInstances[toActionIndex]->OnActionBegins_Internal(inDatas, moveInput, this, inDelta);
		ActionInstances[toActionIndex]->SetActivatedLastFrame(true);
		if (!simulate)
		{
			if (DebugType == ControllerDebugType_StatusDebug)
			{
				UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Action (%s) is Being Activated. Remaining Time: %f"), *ActionInstances[toActionIndex]->DebugString(), ActionInstances[toActionIndex]->GetRemainingActivationTime()), true, true, FColor::Green, 5
					, FName(FString::Printf(TEXT("TryChangeControllerActions_%s"), *ActionInstances[toActionIndex]->GetDescriptionName().ToString())));
			}
		}
	}

	//Notify actions and states
	if (!simulate)
	{
		for (int i = 0; i < StatesInstances.Num(); i++)
		{
			StatesInstances[i]->OnActionChanged(ActionInstances.IsValidIndex(toActionIndex) ? ActionInstances[toActionIndex] : nullptr
				, ActionInstances.IsValidIndex(fromActionIndex) ? ActionInstances[fromActionIndex] : nullptr);
		}

		for (int i = 0; i < ActionInstances.Num(); i++)
		{
			ActionInstances[i]->OnActionChanged(ActionInstances.IsValidIndex(toActionIndex) ? ActionInstances[toActionIndex] : nullptr
				, ActionInstances.IsValidIndex(fromActionIndex) ? ActionInstances[fromActionIndex] : nullptr);
		}
	}

	if (!simulate)
	{
		CurrentActionIndex = toActionIndex;

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
	return true;
}



FVelocity UModularControllerComponent::ProcessControllerAction(FStatusParameters& controllerStatus,
	const FKinematicInfos& inDatas, FVelocity fromStateVelocity, const FVector moveInput, const float inDelta, int simulatedStateIndex,
	int simulatedActionIndex)
{
	FVelocity actionVelocity = fromStateVelocity;
	const FQuat initialRotation = fromStateVelocity.Rotation;
	int activeActionIndex = simulatedActionIndex >= 0 ? simulatedActionIndex : CurrentActionIndex;

	if (ActionInstances.IsValidIndex(activeActionIndex))
	{
		actionVelocity = ProcessSingleAction(ActionInstances[activeActionIndex], controllerStatus, inDatas, fromStateVelocity, moveInput
			, inDelta, simulatedStateIndex, simulatedActionIndex);

		if (DebugType == ControllerDebugType_StatusDebug)
		{
			UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Action (%s) is Being Processed. Remaining Time: %f"), *ActionInstances[activeActionIndex]->DebugString(), ActionInstances[activeActionIndex]->GetRemainingActivationTime()), true, true, FColor::White, 5
				, FName(FString::Printf(TEXT("ProcessControllerActions_%s"), *ActionInstances[activeActionIndex]->GetDescriptionName().ToString())));
		}
	}

	return actionVelocity;
}


FVelocity UModularControllerComponent::ProcessSingleAction(UBaseControllerAction* actionInstance,
	FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, FVelocity previousVelocity,
	const FVector moveInput, const float inDelta, int simulatedStateIndex, int simulatedActionIndex)
{
	if (actionInstance == nullptr)
		return previousVelocity;

	FVelocity movement = previousVelocity;
	int stateIndex = simulatedStateIndex >= 0 ? simulatedStateIndex : CurrentStateIndex;
	int activeActionIndex = simulatedActionIndex >= 0 ? simulatedActionIndex : CurrentActionIndex;

	//Handle state snapshot
	if (simulatedStateIndex >= 0 || simulatedActionIndex >= 0)
		actionInstance->SaveActionSnapShot();
	else
		actionInstance->RestoreActionFromSnapShot();

	FVelocity processMotion = movement;
	processMotion = actionInstance->OnActionProcess_Internal(controllerStatus, inDatas, previousVelocity, moveInput, this, inDelta);

	if (actionInstance->RootMotionMode != ERootMotionType::RootMotionType_No_RootMotion)
	{
		//Rotation
		processMotion.Rotation *= GetRootMotionQuat(GetSkeletalMesh());

		FVelocity rootMotion = processMotion;
		float rootMotionScale = processMotion._rooMotionScale;
		float processMotionScale = 1 - processMotion._rooMotionScale;

		//Translation
		switch (actionInstance->RootMotionMode)
		{
		case RootMotionType_AdditivePrimary:
		{
			rootMotion.ConstantLinearVelocity += GetRootMotionVector(GetSkeletalMesh()) * rootMotionScale;
		}
		break;
		case RootMotionType_AdditiveSecondary:
		{
			rootMotion.InstantLinearVelocity += GetRootMotionVector(GetSkeletalMesh()) * rootMotionScale
				* inDelta;
		}
		break;
		case RootMotionType_OverridePrimary:
		{
			rootMotion.ConstantLinearVelocity = (GetRootMotionVector(GetSkeletalMesh()) * rootMotionScale
				* 1 / (inDelta)) + processMotion.ConstantLinearVelocity * processMotionScale;
		}
		break;
		case RootMotionType_OverrideSecondary:
		{
			rootMotion.InstantLinearVelocity = GetRootMotionVector(GetSkeletalMesh()) * rootMotionScale *
				inDelta + processMotion.InstantLinearVelocity * processMotionScale;
		}
		break;
		case RootMotionType_OverrideAll:
		{
			rootMotion.ConstantLinearVelocity = (GetRootMotionVector(GetSkeletalMesh()) * rootMotionScale
				* 1 / (inDelta)) + processMotion.ConstantLinearVelocity * processMotionScale;
			processMotion.InstantLinearVelocity = FVector::ZeroVector;
		}
		break;
		}

		//Fuse RM
		processMotion = rootMotion;
	}

	movement = processMotion;

	return movement;
}



void UModularControllerComponent::OnControllerActionChanged_Implementation(UBaseControllerAction* newAction,
	UBaseControllerAction* lastAction)
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


void UModularControllerComponent::EvaluateRootMotionOverride(FVelocity& movement, const FKinematicInfos inDatas, float inDelta)
{
	//Handle Root Motion Override
	{
		USkeletalMeshComponent* target = GetSkeletalMesh();

		if (target != nullptr && _overrideRootMotionCommands.Contains(target))
		{
			//Rotation
			if (_overrideRootMotionCommands[target].OverrideRotationRootMotionMode != ERootMotionType::RootMotionType_No_RootMotion)
			{
				//Rotation
				movement.Rotation *= GetRootMotionQuat(GetSkeletalMesh());
			}

			//Translation
			if (_overrideRootMotionCommands[target].OverrideTranslationRootMotionMode != ERootMotionType::RootMotionType_No_RootMotion)
			{
				switch (_overrideRootMotionCommands[target].OverrideTranslationRootMotionMode)
				{
				case RootMotionType_AdditivePrimary:
				{
					movement.ConstantLinearVelocity += GetRootMotionVector(GetSkeletalMesh());
				}break;
				case RootMotionType_AdditiveSecondary:
				{
					movement.InstantLinearVelocity += GetRootMotionVector(GetSkeletalMesh()) * inDelta;
				}break;
				case RootMotionType_OverridePrimary:
				{
					movement.ConstantLinearVelocity = GetRootMotionVector(GetSkeletalMesh()) * 1 / (inDelta);
				}break;
				case RootMotionType_OverrideSecondary:
				{
					movement.InstantLinearVelocity = GetRootMotionVector(GetSkeletalMesh()) * inDelta;
				}break;
				case RootMotionType_OverrideAll:
				{
					movement.ConstantLinearVelocity = GetRootMotionVector(GetSkeletalMesh()) * 1 / inDelta;
					movement.InstantLinearVelocity = FVector::ZeroVector;
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
}


#pragma endregion



#pragma region Movement XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

void UModularControllerComponent::Move_Implementation(const FVector endLocation, const FQuat endRotation, float deltaTime)
{
	if (!UpdatedPrimitive)
		return;
	if (UpdatedPrimitive->IsSimulatingPhysics())
	{
		UpdatedPrimitive->SetAllPhysicsLinearVelocity(Velocity);
	}
	else
	{
		float fps = (1 / deltaTime);
		FVector lerpPos = FMath::Lerp(UpdatedPrimitive->GetComponentLocation(), endLocation, deltaTime * (fps * 0.5f));
		UpdatedPrimitive->SetWorldLocationAndRotation(endLocation, endRotation, false);
	}
}


FVelocity UModularControllerComponent::EvaluateMove(const FKinematicInfos& inDatas, FVelocity movement, float delta, bool noCollision)
{
	auto owner = GetOwner();
	FVelocity result = FVelocity::Null();

	FVector priMove = movement.ConstantLinearVelocity;
	FVector secMove = movement.InstantLinearVelocity;

	FVector initialLocation = inDatas.InitialTransform.GetLocation();
	FVector location = initialLocation;
	FQuat primaryRotation = movement.Rotation;
	FVector primaryDelta = FVector(0);
	FVector secondaryDelta = FVector(0);
	FVector pushObjectForce = priMove;

	////get Pushed by objects
	if (inDatas.bUsePhysic && _collisionForces.Length() > 0 && !noCollision)
	{
		priMove += (_collisionForces / inDatas.GetMass()) * (priMove.Length() > 0 ? FMath::Clamp(FVector::DotProduct(priMove.GetSafeNormal(), _collisionForces.GetSafeNormal()), 0, 1) : 1);
		if (DebugType)
		{
			GEngine->AddOnScreenDebugMessage((int32)GetOwner()->GetUniqueID() + 10, 1, FColor::Green, FString::Printf(TEXT("Applying collision force: %s"), *_collisionForces.ToString()));
		}
		_collisionForces = FVector(0);
	}

	//Primary Movement (momentum movement)
	{
		FHitResult sweepMoveHit = FHitResult(EForceInit::ForceInitToZero);
		bool blockingHit = false;

		blockingHit = noCollision ? false : ComponentTraceCastSingle(sweepMoveHit, initialLocation, priMove * delta, primaryRotation, 0.100, bUseComplexCollision);
		if (blockingHit)
		{
			//Push objects around
			if (inDatas.bUsePhysic && pushObjectForce.Length() > 0 && sweepMoveHit.GetComponent())
			{
				float dotProduct = FVector::DotProduct(pushObjectForce.GetSafeNormal(), sweepMoveHit.ImpactNormal.GetSafeNormal());
				if (sweepMoveHit.GetComponent()->IsSimulatingPhysics())
				{
					sweepMoveHit.GetComponent()->AddForceAtLocation(pushObjectForce * inDatas.GetMass() * FMath::Clamp(-dotProduct, 0, 1), sweepMoveHit.ImpactPoint, sweepMoveHit.BoneName);
				}
			}

			int maxDepth = 4;
			FVector endLocation = SlideAlongSurfaceAt(sweepMoveHit.Location, primaryRotation, sweepMoveHit.TraceEnd - sweepMoveHit.TraceStart, 1 - sweepMoveHit.Time, sweepMoveHit.Normal, sweepMoveHit, maxDepth);
			sweepMoveHit.Location = endLocation;
		}

		//delta
		primaryDelta = noCollision ? priMove * delta : ((blockingHit ? sweepMoveHit.Location : sweepMoveHit.TraceEnd) - location);
		location = noCollision ? location + primaryDelta : (blockingHit ? sweepMoveHit.Location : sweepMoveHit.TraceEnd);
	}

	//Secondary Movement (Adjustement movement)
	{
		FHitResult sweepMoveHit;

		if (!noCollision)
			ComponentTraceCastSingle(sweepMoveHit, location, secMove, primaryRotation, 0.100, bUseComplexCollision);

		FVector newLocation = noCollision ? location + secMove : (sweepMoveHit.IsValidBlockingHit() ? sweepMoveHit.Location : sweepMoveHit.TraceEnd);
		if (!noCollision)
		{
			FVector depenetrationForce = FVector(0);
			if (CheckPenetrationAt(depenetrationForce, newLocation, primaryRotation))
			{
				newLocation += depenetrationForce;
				if (DebugType == ControllerDebugType_MovementDebug)
				{
					UKismetSystemLibrary::DrawDebugArrow(this, newLocation, newLocation + depenetrationForce, 50, FColor::Red, 0, 3);
				}
			}
		}

		secondaryDelta = (newLocation - location);
		location = newLocation;
	}

	result.ConstantLinearVelocity = primaryDelta.IsNearlyZero() ? FVector::ZeroVector : primaryDelta / delta;
	result.InstantLinearVelocity = secondaryDelta.IsNearlyZero() ? FVector::ZeroVector : secondaryDelta;
	result.Rotation = primaryRotation;

	return result;
}


void UModularControllerComponent::PostMoveUpdate(FKinematicInfos& inDatas, const FVelocity moveMade, int stateIndex, const float inDelta)
{
	//Final velocities
	inDatas.FinalVelocities.ConstantLinearVelocity = moveMade.ConstantLinearVelocity;
	inDatas.FinalVelocities.InstantLinearVelocity = moveMade.InstantLinearVelocity;
	inDatas.FinalVelocities.Rotation = moveMade.Rotation;

	//Position
	inDatas.FinalTransform = inDatas.InitialTransform;
	const FVector movement = (moveMade.ConstantLinearVelocity * inDelta + moveMade.InstantLinearVelocity);
	inDatas.FinalTransform.SetLocation(inDatas.InitialTransform.GetLocation() + movement);
	inDatas.FinalTransform.SetRotation(moveMade.Rotation);

	//Root Motion
	inDatas.FinalVelocities._rooMotionScale = moveMade._rooMotionScale;

	//State and actions
	//inDatas.FinalStateIndex = stateIndex;

	Velocity = inDatas.GetFinalMomentum();

	UpdateComponentVelocity();
}


FQuat UModularControllerComponent::HandleRotation(const FVelocity inVelocities, const FKinematicInfos inDatas, const float inDelta) const
{
	//Get the good upright vector
	FVector desiredUpVector = -inDatas.Gravity.GetSafeNormal();
	if (!desiredUpVector.Normalize())
	{
		desiredUpVector = FVector::UpVector;
	}

	//Get quaternions
	FVector virtualFwdDir = FVector::VectorPlaneProject(inVelocities.Rotation.Vector(), desiredUpVector);
	FVector virtualRightDir = FVector::ZeroVector;
	if (virtualFwdDir.Normalize())
	{
		virtualRightDir = FVector::CrossProduct(desiredUpVector, virtualFwdDir);
	}
	else
	{
		virtualFwdDir = -virtualFwdDir.Rotation().Quaternion().GetAxisZ();
		FVector::CreateOrthonormalBasis(virtualFwdDir, virtualRightDir, desiredUpVector);
		virtualFwdDir.Normalize();
	}
	if (!virtualRightDir.Normalize())
	{
		if (DebugType == ControllerDebugType_MovementDebug)
		{
			GEngine->AddOnScreenDebugMessage(152, 1, FColor::Red, FString::Printf(TEXT("Cannot normalize right vector: up = %s, fwd= %s"), *desiredUpVector.ToCompactString(), *virtualFwdDir.ToCompactString()));
		}
		return inVelocities.Rotation;
	}
	FRotator desiredRotator = UKismetMathLibrary::MakeRotFromZX(desiredUpVector, virtualFwdDir);

	virtualFwdDir = desiredRotator.Quaternion().GetAxisX();
	virtualRightDir = desiredRotator.Quaternion().GetAxisY();

	FQuat targetQuat = desiredRotator.Quaternion() * RotationOffset.Quaternion();
	//FQuat currentQuat = inDatas.InitialTransform.GetRotation();

	////Get Angular speed
	//currentQuat.EnforceShortestArcWith(targetQuat);
	//auto quatDiff = targetQuat * currentQuat.Inverse();
	return targetQuat;
}


FVector UModularControllerComponent::SlideAlongSurfaceAt(const FVector& Position, const FQuat& Rotation, const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, int& depth)
{
	const FVector OldHitNormal = Normal;

	//Compute slide vector
	FVector SlideDelta = ComputeSlideVector(Delta, Time, Normal, Hit);
	FVector endLocation = Position + SlideDelta;

	if (DebugType) {
		UKismetSystemLibrary::DrawDebugArrow(this, Position, Position + Normal.GetSafeNormal() * 30, 50, FColor::Blue);
		UKismetSystemLibrary::DrawDebugArrow(this, Position, Position + SlideDelta, 50, FColor::Cyan);
	}

	if ((SlideDelta | Delta) > 0.f)
	{
		if (ComponentTraceCastSingle(Hit, Position, SlideDelta, Rotation, 0.100, bUseComplexCollision))
		{
			// Compute new slide normal when hitting multiple surfaces.
			FVector move = Hit.TraceEnd - Hit.TraceStart;
			TwoWallAdjust(move, Hit, OldHitNormal);

			if (DebugType) {
				UKismetSystemLibrary::DrawDebugArrow(this, Hit.Location, Hit.Location + move, 50, FColor::Purple);
				DrawCircle(GetWorld(), Hit.Location, GetRotation().GetAxisX(), GetRotation().GetAxisY(), FColor::Purple, 25, 32, false, -1, 0, 3);
			}

			endLocation = Hit.Location;

			// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
			if (!move.IsNearlyZero(1e-3f) && (move | SlideDelta) > 0.f)
			{
				FHitResult secondaryMove;
				// Perform second move
				if (ComponentTraceCastSingle(secondaryMove, Hit.Location, move, Rotation, 0.100, bUseComplexCollision))
				{
					if (depth > 0) {
						depth--;
						endLocation = SlideAlongSurfaceAt(secondaryMove.Location, Rotation, secondaryMove.TraceEnd - secondaryMove.TraceStart, 1 - secondaryMove.Time, secondaryMove.Normal, secondaryMove, depth);
						secondaryMove.Location = endLocation;
						if (DebugType) {
							DrawCircle(GetWorld(), endLocation, GetRotation().GetAxisX(), GetRotation().GetAxisY(), FColor::Yellow, 25, 32, false, -1, 0, 3);
						}
					}
					else {
						endLocation = secondaryMove.Location;
						if (DebugType) {
							DrawCircle(GetWorld(), endLocation, GetRotation().GetAxisX(), GetRotation().GetAxisY(), FColor::Orange, 25, 32, false, -1, 0, 3);
						}
					}
				}
				else
				{
					endLocation = secondaryMove.TraceEnd;

					if (DebugType) {
						DrawCircle(GetWorld(), endLocation, GetRotation().GetAxisX(), GetRotation().GetAxisY(), FColor::Red, 25, 32, false, -1, 0, 3);
					}
				}
			}

		}
		else
		{
			endLocation = Hit.TraceEnd;
		}
	}
	return endLocation;
}


#pragma endregion



#pragma region Tools & Utils

bool UModularControllerComponent::ComponentTraceCastMulti(TArray<FHitResult>& outHits, FVector position, FVector direction, FQuat rotation, double inflation, bool traceComplex)
{
	auto owner = GetOwner();
	if (owner == nullptr)
		return false;

	UPrimitiveComponent* primitive = UpdatedPrimitive;
	if (!primitive)
		return false;

	FCollisionQueryParams queryParams;
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
			outHits[i].Location -= direction.GetSafeNormal() * 0.125f;
		}
	}

	return outHits.Num() > 0;
}


bool UModularControllerComponent::ComponentTraceCastSingle(FHitResult& outHit, FVector position, FVector direction, FQuat rotation, double inflation, bool traceComplex)
{
	outHit.Location = position;
	auto owner = GetOwner();
	if (owner == nullptr)
		return false;

	UPrimitiveComponent* primitive = UpdatedPrimitive;
	if (!primitive)
		return false;

	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(owner);
	queryParams.bTraceComplex = traceComplex;
	queryParams.bReturnPhysicalMaterial = true;
	float OverlapInflation = inflation;
	auto shape = primitive->GetCollisionShape(OverlapInflation);

	if (GetWorld()->SweepSingleByChannel(outHit, position, position + direction, rotation, primitive->GetCollisionObjectType(), shape, queryParams))
	{
		outHit.Location -= direction.GetSafeNormal() * 0.125f;
		return true;
	}
	else
	{
		return false;
	}

}


void UModularControllerComponent::PathCastComponent(TArray<FHitResult>& results, FVector start, TArray<FVector> pathPoints, bool stopOnHit, float skinWeight, bool debugRay, bool rotateAlongPath, bool bendOnCollision, bool traceComplex)
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


	UPrimitiveComponent* primitive = UpdatedPrimitive;
	if (!primitive)
		return;
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


bool UModularControllerComponent::CheckPenetrationAt(FVector& force, FVector position, FQuat NewRotationQuat, UPrimitiveComponent* onlyThisComponent)
{
	{
		FVector moveVec = FVector(0);
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
		if (GetWorld()->OverlapMultiByChannel(_overlaps, position, NewRotationQuat, primitive->GetCollisionObjectType(), primitive->GetCollisionShape(0.125f), comQueryParams))
		{
			FMTDResult depenetrationInfos;
			for (auto& overlap : _overlaps)
			{
				UKismetSystemLibrary::DrawDebugPoint(this, position, 10, FColor::Blue);
				if (!overlapFound)
					overlapFound = true;

				if (overlap.Component->ComputePenetration(depenetrationInfos, primitive->GetCollisionShape(0.125f), position, NewRotationQuat))
				{
					const FVector depForce = depenetrationInfos.Direction * (depenetrationInfos.Distance + 0.125f);
					if (onlyThisComponent == overlap.Component)
					{
						force = depForce;
						return true;
					}
					moveVec += depForce;
				}
			}
		}

		if (onlyThisComponent != nullptr)
		{
			force = FVector(0);
			return false;
		}

		force = moveVec;
		return overlapFound;
	}
}


FVector UModularControllerComponent::PointOnShape(FVector direction, const FVector inLocation)
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

	return onColliderPt + offset;
}


#pragma endregion
