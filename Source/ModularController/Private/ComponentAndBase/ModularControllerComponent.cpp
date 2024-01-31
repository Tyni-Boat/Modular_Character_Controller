// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#include "ComponentAndBase/ModularControllerComponent.h"
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

	//State behaviors
	StatesInstances.Empty();
	for (int i = StateClasses.Num() - 1; i >= 0; i--)
	{
		if (StateClasses[i] == nullptr)
			continue;
		UBaseControllerState* instance = NewObject<UBaseControllerState>(StateClasses[i], StateClasses[i]);
		StatesInstances.Add(instance);
	}
	StatesInstances.Sort([](UBaseControllerState& a, UBaseControllerState& b) { return a.GetPriority() > b.GetPriority(); });

	//Action behaviors
	ActionInstances.Empty();
	for (int i = ActionClasses.Num() - 1; i >= 0; i--)
	{
		if (ActionClasses[i] == nullptr)
			continue;
		UBaseAction* instance = NewObject<UBaseAction>(ActionClasses[i], ActionClasses[i]);
		ActionInstances.Add(instance);
	}

	//Init last move
	LastMoveMade = FKinematicInfos(GetOwner()->GetActorTransform(), FVelocity(), FSurfaceInfos(), -1, TArray<int>());
	LastMoveMade.FinalTransform = LastMoveMade.InitialTransform;


	//Set time elapsed
	auto timePassedSince = FDateTime::UtcNow() - FDateTime(2023, 11, 26, 0, 0, 0, 0);
	_timeElapsed = timePassedSince.GetTotalSeconds();
}


void UModularControllerComponent::MainUpdateComponent(float delta)
{
	APawn* pawn = _ownerPawn.Get();
	if (pawn == nullptr)
		return;

	if (GetNetMode() == ENetMode::NM_Standalone)
	{
		FKinematicInfos movement = FKinematicInfos(this, GetGravity(), LastMoveMade, GetMass(), ShowDebug);
		movement.bUsePhysic = bUsePhysicAuthority;
		const FVector moveInp = ConsumeMovementInput();
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



FKinematicInfos UModularControllerComponent::StandAloneUpdateComponent(FVector movementInput, FKinematicInfos& movementInfos, FInputEntryPool& usedInputPool, float delta)
{
	movementInfos.ChangeActor(this, BoneName, ShowDebug);

	//auto stateIndex = CheckControllerStates(movementInfos, usedInputPool, delta);
	//TryChangeControllerState(CurrentStateIndex, stateIndex, movementInfos, usedInputPool, delta);
	//preprocessUsed = PreProcessCurrentControllerState(movementInfos, usedInputPool, delta);
	//const FVelocity primaryMotion = ProcessCurrentControllerState(movementInfos, preprocessUsed, delta);

	//FVelocity alteredMotion = EvaluateAction(primaryMotion, movementInfos, usedInputPool, delta);
	FVelocity alteredMotion;
	alteredMotion.ConstantLinearVelocity = movementInput * 1000;

	EvaluateRootMotionOverride(alteredMotion, movementInfos, delta);
	const FQuat finalRot = HandleRotation(alteredMotion, movementInfos, delta);
	alteredMotion.Rotation = finalRot;
	usedInputPool.UpdateInputs(delta, movementInfos.IsDebugMode);

	FVelocity resultingMove = EvaluateMove(movementInfos, alteredMotion, delta);
	resultingMove._rooMotionScale = alteredMotion._rooMotionScale;
	PostMoveUpdate(movementInfos, resultingMove, CurrentStateIndex, delta);
	Move(movementInfos.FinalTransform.GetLocation(), movementInfos.FinalTransform.GetRotation(), delta);
	movementInfos.FinalTransform.SetComponents(UpdatedPrimitive->GetComponentRotation().Quaternion(), UpdatedPrimitive->GetComponentLocation(), UpdatedPrimitive->GetComponentScale());
	if (ShowDebug)
	{
		UKismetSystemLibrary::DrawDebugArrow(this, movementInfos.InitialTransform.GetLocation(), movementInfos.InitialTransform.GetLocation() + alteredMotion.ConstantLinearVelocity * 0.1f, 50, FColor::Magenta);
		DrawCircle(GetWorld(), movementInfos.FinalTransform.GetLocation(), alteredMotion.Rotation.GetAxisX(), alteredMotion.Rotation.GetAxisY(), FColor::Magenta, 35, 32, false, -1, 0, 2);
	}
	return  movementInfos;
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
	_user_inputPool.AddOrReplace(key, entry);
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
	return move;
}

FInputEntry UModularControllerComponent::ReadInput(const FName key, bool consume)
{
	if (consume)
		return _user_inputPool.ConsumeInput(key);
	return _user_inputPool.ReadInput(key);
}

bool UModularControllerComponent::ReadButtonInput(const FName key, bool consume)
{
	const FInputEntry entry = ReadInput(key, consume);
	return entry.Phase == EInputEntryPhase::InputEntryPhase_Held || entry.Phase == EInputEntryPhase::InputEntryPhase_Pressed;
}

float UModularControllerComponent::ReadValueInput(const FName key, bool consume)
{
	const FInputEntry entry = ReadInput(key, consume);
	return entry.Axis.X;
}

FVector UModularControllerComponent::ReadAxisInput(const FName key, bool consume)
{
	const FInputEntry entry = ReadInput(key, consume);
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



void UModularControllerComponent::MultiCastMoveInput_Implementation(FVector_NetQuantize10 userMoveInput, FVector_NetQuantize atLocation, FVector_NetQuantize withVelocity, double timeStamp)
{
	_moveInputCommands.Add(TTuple<double, FVector, FVector, FVector>(timeStamp, userMoveInput, atLocation, withVelocity));
}

void UModularControllerComponent::MultiCastPosition_Implementation(FVector_NetQuantize10 newPos, double timeStamp)
{
	const ENetRole role = GetNetRole();
	switch (role)
	{
	case ENetRole::ROLE_Authority:
	{
	} break;
	case ENetRole::ROLE_AutonomousProxy:
	{
		_netPosition.Key = timeStamp;
		_netPosition.Value = newPos;
	}
	break;

	default:
	{
		_netPosition.Key = timeStamp;
		_netPosition.Value = newPos;
	}
	break;
	}
}

void UModularControllerComponent::MultiCastVelocity_Implementation(FVector_NetQuantize10 newVelocity, double timeStamp)
{
	const ENetRole role = GetNetRole();
	switch (role)
	{
	case ENetRole::ROLE_Authority:
	{
	} break;
	case ENetRole::ROLE_AutonomousProxy:
	{
		_netVelocity.Key = timeStamp;
		_netVelocity.Value = newVelocity;
	}
	break;

	default:
	{
		_netVelocity.Key = timeStamp;
		_netVelocity.Value = newVelocity;
	}
	break;
	}
}

void UModularControllerComponent::MultiCastRotation_Implementation(FRotator newRot, double timeStamp)
{
	const ENetRole role = GetNetRole();
	switch (role)
	{
	case ENetRole::ROLE_Authority:
	{
	} break;
	case ENetRole::ROLE_AutonomousProxy:
	{
		_netRotation.Key = timeStamp;
		_netRotation.Value = newRot.Quaternion();
	}
	break;

	default:
	{
		_netRotation.Key = timeStamp;
		_netRotation.Value = newRot.Quaternion();
	}
	break;
	}
}


void UModularControllerComponent::MultiCastState_Implementation(uint32 stateIndex, double timeStamp)
{
	const ENetRole role = GetNetRole();
	switch (role)
	{
	case ENetRole::ROLE_Authority:
	{
	} break;
	case ENetRole::ROLE_AutonomousProxy:
	{
		_netMoveState.Key = timeStamp;
		_netMoveState.Value = stateIndex;
	}
	break;

	default:
	{
		_netMoveState.Key = timeStamp;
		_netMoveState.Value = stateIndex;
	}
	break;
	}
}

void UModularControllerComponent::MultiCastMoveParams_Implementation(FMovePreprocessParams params, double timeStamp)
{
	const ENetRole role = GetNetRole();
	switch (role)
	{
	case ENetRole::ROLE_Authority:
	{
	} break;
	case ENetRole::ROLE_AutonomousProxy:
	{
		_netStateMoveParams.Key = timeStamp;
		_netStateMoveParams.Value = params;
	}
	break;

	default:
	{
		_netStateMoveParams.Key = timeStamp;
		_netStateMoveParams.Value = params;
	}
	break;
	}
}


bool UModularControllerComponent::MulticastMovement(FKinematicInfos movement, FMovePreprocessParams movementParams, double timeStamp)
{
	bool somethingChanged = false;

	if (movementParams.ParamChanged(_netStateMoveParams.Value))
	{
		movementParams.Serialize();
		_netStateMoveParams.Key = timeStamp;
		_netStateMoveParams.Value = movementParams;
		MultiCastMoveParams(movementParams, timeStamp);
		somethingChanged = true;
	}

	if (FVector_NetQuantize(_netPosition.Value) != FVector_NetQuantize(movement.FinalTransform.GetLocation()))
	{
		_netPosition.Value = movement.FinalTransform.GetLocation();
		_netPosition.Key = timeStamp;
		MultiCastPosition(_netPosition.Value, timeStamp);
		somethingChanged = true;
	}

	if (FVector_NetQuantize(_netVelocity.Value) != FVector_NetQuantize(movement.FinalVelocities.ConstantLinearVelocity))
	{
		_netVelocity.Key = timeStamp;
		_netVelocity.Value = movement.FinalVelocities.ConstantLinearVelocity;
		MultiCastVelocity(movement.FinalVelocities.ConstantLinearVelocity, timeStamp);
		somethingChanged = true;
	}

	if (FMath::RadiansToDegrees(movement.FinalTransform.GetRotation().AngularDistance(_netRotation.Value)) > 4)
	{
		_netRotation.Key = timeStamp;
		_netRotation.Value = movement.FinalTransform.GetRotation();
		MultiCastRotation(_netRotation.Value.Rotator(), timeStamp);
		somethingChanged = true;
	}

	if (_netMoveState.Value != CurrentStateIndex)
	{
		_netMoveState.Key = timeStamp;
		_netMoveState.Value = CurrentStateIndex;
		MultiCastState(_netMoveState.Value, timeStamp);
		somethingChanged = true;
	}

	return somethingChanged;
}


#pragma region Listened OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO


void UModularControllerComponent::ListenServerUpdateComponent(float delta)
{
	FKinematicInfos movement = FKinematicInfos(this, GetGravity(), LastMoveMade, GetMass(), false);
	const FVector moveInp = ConsumeMovementInput();

	//Execute the move
	{
		movement.bUsePhysic = bUsePhysicAuthority;
		movement = StandAloneUpdateComponent(moveInp, movement, _user_inputPool, delta);
		LastMoveMade = movement;
	}

	if (_lastMoveCommand.Get<1>() != moveInp || _lastMoveCommand.Get<2>() != LastMoveMade.FinalTransform.GetLocation() || _lastMoveCommand.Get<3>() != LastMoveMade.FinalVelocities.ConstantLinearVelocity)
	{
		MultiCastMoveInput(moveInp, LastMoveMade.FinalTransform.GetLocation(), LastMoveMade.FinalVelocities.ConstantLinearVelocity, _timeElapsed);
		_lastMoveCommand = TTuple<double, FVector, FVector, FVector>(_timeElapsed, moveInp, LastMoveMade.FinalTransform.GetLocation(), LastMoveMade.FinalVelocities.ConstantLinearVelocity);
	}
	//MulticastMovement(LastMoveMade, movementParams, _timeElapsed);
}


#pragma endregion

#pragma region Dedicated OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO


void UModularControllerComponent::DedicatedServerUpdateComponent(float delta)
{
	if (bUseClientAuthorative)
	{
		SimulatedProxyUpdateComponent(delta);
	}
	else
	{
	}
}


#pragma endregion

#pragma endregion



#pragma region Client Logic ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma region Automonous Proxy OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO

void UModularControllerComponent::ServerCastPosition_Implementation(FVector_NetQuantize10 newPos, double timeStamp)
{
	if (bUseClientAuthorative)
	{
		MultiCastPosition(newPos, timeStamp);
	}
}

void UModularControllerComponent::ServerCastRotation_Implementation(FRotator newRot, double timeStamp)
{
	if (bUseClientAuthorative)
	{
		MultiCastRotation(newRot, timeStamp);
	}
}

void UModularControllerComponent::ServerCastVelocity_Implementation(FVector_NetQuantize10 newVelocity, double timeStamp)
{
	if (bUseClientAuthorative)
	{
		MultiCastVelocity(newVelocity, timeStamp);
	}
}

void UModularControllerComponent::ServerCastState_Implementation(uint32 stateIndex, double timeStamp)
{
	if (bUseClientAuthorative)
	{
		MultiCastState(stateIndex, timeStamp);
	}
}

void UModularControllerComponent::ServerCastMoveParams_Implementation(FMovePreprocessParams params, double timeStamp)
{
	if (bUseClientAuthorative)
	{
		MultiCastMoveParams(params, timeStamp);
	}
}

void UModularControllerComponent::ServerCastClientRequest_Implementation(FVector_NetQuantize10 startLocation, FTranscodedInput inputs, double timeStamp)
{
	_serverPendingRequests.Add(TKeyValuePair<double, TKeyValuePair<FVector, FTranscodedInput>>(timeStamp, TKeyValuePair<FVector, FTranscodedInput>(startLocation, inputs)));
}

int UModularControllerComponent::ServerCastAllMovement(FKinematicInfos lastMove, FKinematicInfos currentMove, FMovePreprocessParams movementParams, double timeStamp)
{
	int result = 0;

	if (FVector_NetQuantize(lastMove.FinalTransform.GetLocation()) != FVector_NetQuantize(currentMove.FinalTransform.GetLocation()))
	{
		ServerCastPosition(currentMove.FinalTransform.GetLocation(), timeStamp);
		result += 10;
	}

	if (FVector_NetQuantize(lastMove.FinalVelocities.ConstantLinearVelocity) != FVector_NetQuantize(currentMove.FinalVelocities.ConstantLinearVelocity))
	{
		ServerCastVelocity(currentMove.FinalVelocities.ConstantLinearVelocity, timeStamp);
		result += 100;
	}

	if (FMath::RadiansToDegrees(currentMove.FinalTransform.GetRotation().AngularDistance(lastMove.FinalTransform.GetRotation())) > 4)
	{
		ServerCastRotation(lastMove.FinalTransform.GetRotation().Rotator(), timeStamp);
		result += 1000;
	}

	if (lastMove.FinalStateIndex != CurrentStateIndex)
	{
		ServerCastState(CurrentStateIndex, timeStamp);
		result += 10000;
	}

	return result;
}


void UModularControllerComponent::AutonomousProxyUpdateComponent(float delta)
{
	if (bUseClientAuthorative)
	{
		FKinematicInfos movement = FKinematicInfos(this, GetGravity(), LastMoveMade, GetMass(), ShowDebug);
		movement.bUsePhysic = bUsePhysicAuthority;
		const FVector moveInp = ConsumeMovementInput();
		StandAloneUpdateComponent(moveInp, movement, _user_inputPool, delta);
		LastMoveMade = movement;

		//ServerCastAllMovement();
	}
	else
	{

	}
}

#pragma endregion

#pragma region Simulated Proxy OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO


void UModularControllerComponent::SimulatedProxyUpdateComponent(float delta)
{
	if (_moveInputCommands.Num() > 0)
	{
		_lastMoveCommand = _moveInputCommands[0];
		GEngine->AddOnScreenDebugMessage(1, 1, FColor::Black, FString::Printf(TEXT("Move commands Remaining: %d"), _moveInputCommands.Num()));
		_moveInputCommands.RemoveAt(0);
	}

	auto moveCmd = _lastMoveCommand;

	if (moveCmd.Get<0>() != 0)
	{
		const FVector lerpLocation = FMath::Lerp(LastMoveMade.FinalTransform.GetLocation(), moveCmd.Get<2>(), delta * AdjustmentSpeed);
		UpdatedPrimitive->SetWorldLocation(lerpLocation);
		LastMoveMade.FinalTransform.SetLocation(lerpLocation);
		LastMoveMade.FinalVelocities.ConstantLinearVelocity = moveCmd.Get<3>();
		MovementInput(moveCmd.Get<1>());
	}

	FKinematicInfos movement = FKinematicInfos(this, GetGravity(), LastMoveMade, GetMass(), ShowDebug);
	movement.bUsePhysic = bUsePhysicAuthority;
	FVector moveInp = ConsumeMovementInput();
	StandAloneUpdateComponent(moveInp, movement, _user_inputPool, delta);
	LastMoveMade = movement;

	//FKinematicInfos movement = FKinematicInfos(this, GetGravity(), LastMoveMade, GetMass(), false);
	//movement.InitialTransform.SetComponents(UpdatedPrimitive->GetComponentRotation().Quaternion(), UpdatedPrimitive->GetComponentLocation(), UpdatedPrimitive->GetComponentScale());
	//movement.ChangeActor(this, BoneName, ShowDebug);

	////States Handling
	//{
	//	auto copyOfInputs = _user_inputPool;
	//	TryChangeControllerState(CurrentStateIndex, _netMoveState.Value, movement, copyOfInputs, delta);
	//	FMovePreprocessParams stateParams = _netStateMoveParams.Value;
	//	stateParams.Deserialize();
	//	ProcessCurrentControllerState(movement, stateParams, delta);
	//}

	////Move and calculate velocity
	//{
	//	FVelocity primaryMotion;

	//	//Velocity deduction
	//	primaryMotion.Rotation = _netRotation.Value;
	//	primaryMotion.ConstantLinearVelocity = _netVelocity.Value;

	//	//Lerp initials
	//	UpdatedPrimitive->SetWorldLocationAndRotation(FMath::Lerp(UpdatedPrimitive->GetComponentLocation(), _netPosition.Value, delta * AdjustmentSpeed * 2), FQuat::Slerp(UpdatedPrimitive->GetComponentQuat(), _netRotation.Value, delta * AdjustmentSpeed * 2));

	//	PostMoveUpdate(movement, primaryMotion, CurrentStateIndex, delta);
	//	LastMoveMade = movement;
	//}
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
		if (ShowDebug)
			GEngine->AddOnScreenDebugMessage((int32)GetOwner()->GetUniqueID() + 9, 1, FColor::Green, FString::Printf(TEXT("Overlaped With: %s"), *OtherActor->GetActorNameOrLabel()));
	}
}


void UModularControllerComponent::BeginCollision(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor != nullptr && ShowDebug)
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UModularControllerComponent::RemoveControllerStateByType_Implementation(TSubclassOf<UBaseControllerState> moduleType)
{
	if (CheckControllerStateByType(moduleType))
	{
		auto behaviour = StatesInstances.FindByPredicate([moduleType](UBaseControllerState* state) -> bool { return state->GetClass() == moduleType->GetClass(); });
		StatesInstances.Remove(*behaviour);
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
		StatesInstances.Sort([](UBaseControllerState& a, UBaseControllerState& b) { return a.GetPriority() > b.GetPriority(); });
		return;
	}
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int UModularControllerComponent::CheckControllerStates(FKinematicInfos& inDatas, FInputEntryPool& inputs,
	const float inDelta)
{
	int maxStatePriority = -1;
	int selectedStateIndex = -1;


	//Check if a behaviour's check have success state
	{
		//Check if a valid action freeze the current state
		if (selectedStateIndex < 0)
		{
			for (int i = inDatas.InitialActionsIndexes.Num() - 1; i >= 0; i--)
			{
				if (!ActionInstances.IsValidIndex(inDatas.InitialActionsIndexes[i]))
					continue;

				if (ActionInstances[inDatas.InitialActionsIndexes[i]]->FreezeCurrentState)
				{
					selectedStateIndex = inDatas.InitialStateIndex;
					break;
				}
			}
		}

		if (selectedStateIndex < 0)
		{
			for (int i = 0; i < StatesInstances.Num(); i++)
			{
				if (StatesInstances[i] == nullptr)
					continue;

				if (StatesInstances[i]->CheckState(inDatas, inputs, this, inDelta))
				{
					if (StatesInstances[i]->GetPriority() > maxStatePriority)
					{
						selectedStateIndex = i;
						maxStatePriority = StatesInstances[i]->GetPriority();
					}
				}
			}
		}
	}

	return selectedStateIndex;
}


bool UModularControllerComponent::TryChangeControllerState(int fromStateIndex, int toStateIndex, FKinematicInfos& inDatas, FInputEntryPool& inputs, const float inDelta)
{
	if (fromStateIndex == toStateIndex)
		return false;

	if (!StatesInstances.IsValidIndex(toStateIndex))
		return false;

	//Landing
	StatesInstances[toStateIndex]->OnEnterState(inDatas, inputs, this, inDelta);
	LinkAnimBlueprint(GetSkeletalMesh(), "State", StatesInstances[toStateIndex]->StateBlueprintClass);
	StatesInstances[toStateIndex]->SetWasTheLastFrameBehaviour(true);

	if (StatesInstances.IsValidIndex(fromStateIndex))
	{
		//Leaving
		StatesInstances[fromStateIndex]->OnExitState(inDatas, inputs, this, inDelta);
		StatesInstances[fromStateIndex]->SurfaceInfos.Reset();
	}

	for (int i = 0; i < StatesInstances.Num(); i++)
	{
		if (i == toStateIndex)
			continue;

		StatesInstances[i]->SetWasTheLastFrameBehaviour(false);
		StatesInstances[i]->OnControllerStateChanged(StatesInstances.IsValidIndex(toStateIndex) ? StatesInstances[toStateIndex]->GetDescriptionName() : "", StatesInstances.IsValidIndex(toStateIndex) ? StatesInstances[toStateIndex]->GetPriority() : -1, this);
	}


	OnControllerStateChanged(StatesInstances.IsValidIndex(toStateIndex) ? StatesInstances[toStateIndex] : nullptr
		, StatesInstances.IsValidIndex(fromStateIndex) ? StatesInstances[fromStateIndex] : nullptr);
	OnControllerStateChangedEvent.Broadcast(StatesInstances.IsValidIndex(toStateIndex) ? StatesInstances[toStateIndex] : nullptr
		, StatesInstances.IsValidIndex(fromStateIndex) ? StatesInstances[fromStateIndex] : nullptr);


	//Notify actions the change of state
	for (int i = 0; i < ActionInstances.Num(); i++)
	{
		ActionInstances[i]->OnStateChanged(StatesInstances.IsValidIndex(toStateIndex) ? StatesInstances[toStateIndex] : nullptr
			, StatesInstances.IsValidIndex(fromStateIndex) ? StatesInstances[fromStateIndex] : nullptr);
	}

	CurrentStateIndex = toStateIndex;
	return true;
}


FMovePreprocessParams UModularControllerComponent::PreProcessCurrentControllerState(FKinematicInfos& inDatas,
	FInputEntryPool& inputs, const float inDelta)
{
	FMovePreprocessParams stateParams;

	if (StatesInstances.IsValidIndex(CurrentStateIndex))
	{
		stateParams = StatesInstances[CurrentStateIndex]->PreProcessState(inDatas, inputs, this, inDelta);
	}

	return stateParams;
}



FVelocity UModularControllerComponent::ProcessCurrentControllerState(FKinematicInfos& inDatas,
	FMovePreprocessParams preProcessParams, const float inDelta)
{
	FVelocity movement = inDatas.InitialVelocities;
	if (StatesInstances.IsValidIndex(CurrentStateIndex))
	{
		FVelocity processMotion = movement;
		processMotion = StatesInstances[CurrentStateIndex]->ProcessState(inDatas, preProcessParams, this, inDelta);

		if (StatesInstances[CurrentStateIndex]->RootMotionMode != ERootMotionType::RootMotionType_No_RootMotion)
		{
			//Rotation
			processMotion.Rotation *= GetRootMotionQuat(GetSkeletalMesh());

			FVelocity rootMotion = processMotion;
			float rootMotionScale = processMotion._rooMotionScale;
			float processMotionScale = 1 - processMotion._rooMotionScale;

			//Translation
			switch (StatesInstances[CurrentStateIndex]->RootMotionMode)
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


bool UModularControllerComponent::CheckActionBehaviourByType(TSubclassOf<UBaseAction> moduleType)
{
	if (ActionInstances.Num() <= 0)
		return false;
	auto index = ActionInstances.IndexOfByPredicate([moduleType](UBaseAction* action) -> bool { return action->GetClass() == moduleType; });
	return ActionInstances.IsValidIndex(index);
}

bool UModularControllerComponent::CheckActionBehaviourByName(FName moduleName)
{
	if (ActionInstances.Num() <= 0)
		return false;
	auto index = ActionInstances.IndexOfByPredicate([moduleName](UBaseAction* action) -> bool { return action->GetDescriptionName() == moduleName; });
	return ActionInstances.IsValidIndex(index);
}

bool UModularControllerComponent::CheckActionBehaviourByPriority(int modulePriority)
{
	if (ActionInstances.Num() <= 0)
		return false;
	auto index = ActionInstances.IndexOfByPredicate([modulePriority](UBaseAction* action) -> bool { return action->GetPriority() == modulePriority; });
	return ActionInstances.IsValidIndex(index);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UModularControllerComponent::AddActionBehaviour_Implementation(TSubclassOf<UBaseAction> moduleType)
{
	if (moduleType == nullptr)
		return;
	if (CheckActionBehaviourByType(moduleType))
		return;
	UBaseAction* instance = NewObject<UBaseAction>(moduleType, moduleType);
	ActionInstances.Add(instance);
}


UBaseAction* UModularControllerComponent::GetActionByType(TSubclassOf<UBaseAction> moduleType)
{
	if (ActionInstances.Num() <= 0)
		return nullptr;
	auto index = ActionInstances.IndexOfByPredicate([moduleType](UBaseAction* action) -> bool { return action->GetClass() == moduleType; });
	if (ActionInstances.IsValidIndex(index))
	{
		return ActionInstances[index];
	}
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UModularControllerComponent::RemoveActionBehaviourByType_Implementation(TSubclassOf<UBaseAction> moduleType)
{
	if (CheckActionBehaviourByType(moduleType))
	{
		auto behaviour = ActionInstances.FindByPredicate([moduleType](UBaseAction* action) -> bool { return action->GetClass() == moduleType->GetClass(); });
		ActionInstances.Remove(*behaviour);
		return;
	}
}

void UModularControllerComponent::RemoveActionBehaviourByName_Implementation(FName moduleName)
{
	if (CheckActionBehaviourByName(moduleName))
	{
		auto behaviour = ActionInstances.FindByPredicate([moduleName](UBaseAction* action) -> bool { return action->GetDescriptionName() == moduleName; });
		ActionInstances.Remove(*behaviour);
		return;
	}
}

void UModularControllerComponent::RemoveActionBehaviourByPriority_Implementation(int modulePriority)
{
	if (CheckActionBehaviourByPriority(modulePriority))
	{
		auto behaviour = ActionInstances.FindByPredicate([modulePriority](UBaseAction* action) -> bool { return action->GetPriority() == modulePriority; });
		ActionInstances.Remove(*behaviour);
		return;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




FVelocity UModularControllerComponent::EvaluateAction(const FVelocity inVelocities, FKinematicInfos& inDatas, FInputEntryPool& inputs, const float inDelta, const bool asSimulation)
{
	//Fill current actions
	TArray<UBaseAction*> actions;
	TArray<int> actionsIndexToActivate;

	if (inDatas.FinalActionsIndexes.Num() <= 0)
	{
		for (int i = 0; i < inDatas.InitialActionsIndexes.Num(); i++)
		{
			if (ActionInstances.IsValidIndex(inDatas.InitialActionsIndexes[i]))
			{
				if (ActionInstances[inDatas.InitialActionsIndexes[i]]->IsActive())
				{
					actions.Add(ActionInstances[inDatas.InitialActionsIndexes[i]]);
				}
			}
		}
	}

	for (int i = 0; i < inDatas.FinalActionsIndexes.Num(); i++)
	{
		if (ActionInstances.IsValidIndex(inDatas.FinalActionsIndexes[i]))
		{
			if (ActionInstances[inDatas.FinalActionsIndexes[i]]->IsActive())
			{
				if (!actions.Contains(ActionInstances[inDatas.FinalActionsIndexes[i]]))
					actions.Add(ActionInstances[inDatas.FinalActionsIndexes[i]]);
			}
			else
			{
				actionsIndexToActivate.Add(inDatas.FinalActionsIndexes[i]);
			}
		}
	}

	//Inner actions update
	for (int i = actions.Num() - 1; i >= 0; i--)
	{
		if (actions[i]->IsActionCompleted(asSimulation))
		{
			if (actions[i]->Montage.Montage == nullptr || !actions[i]->IsWaitingMontage() || asSimulation)
			{
				if (!asSimulation)
					OnActionEndsEvent.Broadcast(actions[i]);
				actions[i]->OnExitInner();
			}
			actions[i]->OnActionEnds(inDatas, inputs, this, inDelta);
			actions.RemoveAt(i);
			continue;
		}

		actions[i]->ActiveActionUpdate(inDelta);
	}

	UBaseControllerState* state = StatesInstances.IsValidIndex(inDatas.FinalStateIndex) ? StatesInstances[inDatas.FinalStateIndex] : nullptr;
	if (state == nullptr)
	{
		inDatas.FinalActionsIndexes.Empty();
		for (int i = 0; i < inDatas.InitialActionsIndexes.Num(); i++)
			inDatas.FinalActionsIndexes.Add(inDatas.InitialActionsIndexes[i]);

		return inVelocities;
	}


	FVelocity move = inVelocities;

	//Check if an action can be launched
	{
		FInputEntryPool emptyInputPool;
		FInputEntryPool& usedInputs = actionsIndexToActivate.Num() <= 0 ? inputs : emptyInputPool;

		for (int i = 0; i < ActionInstances.Num(); i++)
		{
			if (ActionInstances[i] == nullptr)
				continue;

			bool forceActivation = actionsIndexToActivate.Num() > 0 && actionsIndexToActivate.Contains(i);

			//Run action idle.
			ActionInstances[i]->ActionIdle(inDatas, usedInputs, this, inDelta);

			//if action were not passed as already activated
			if (!forceActivation)
			{

				//Check state validity
				{
					if (ActionInstances[i]->CompatibleStates.Num() > 0 && state == nullptr)
					{
						continue;
					}

					if (state != nullptr
						&& ActionInstances[i]->UseCompatibleStatesOnly
						&& ActionInstances[i]->CompatibleStates.Num() > 0
						&& !ActionInstances[i]->CompatibleStates.Contains(state->GetDescriptionName()))
					{
						continue;
					}

					if (state != nullptr && ActionInstances[i]->CompatibleStates.Num() <= 0)
					{
						continue;
					}
				}
			}

			//Check if we can repeat the action now
			if (actions.Contains(ActionInstances[i]))
			{
				if (!ActionInstances[i]->CheckCanRepeat(inDatas, usedInputs, this, inDelta))
					continue;
			}
			//Check cooldown
			else
			{
				ActionInstances[i]->PassiveActionUpdate(inDelta);
				if (ActionInstances[i]->IsActionCoolingDown())
					continue;
			}


			if (ActionInstances[i]->CheckAction(inDatas, usedInputs, this, inDelta) || forceActivation)
			{
				//if (ShowDebug)
				//	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Blue, FString::Printf(TEXT("[%s] - Activating Action %s. forced? %d"), *GetNetRoleDebug(inDatas.GetActor()->GetLocalRole()).ToString(), *ActionInstances[i]->GetDescriptionName().ToString(), forceActivation));

				//There is an action constraint? find if a compatible action found. if not, continue
				if (!forceActivation)
				{
					if (ActionInstances[i]->CompatibleActions.Num() > 0 && ActionInstances[i]->CompatibleActions.IndexOfByPredicate([actions](FName compActions)-> bool { for (int j = 0; j < actions.Num(); j++)
					{
						if (actions[j]->GetDescriptionName() == compActions)
							return true;
					}
					return false; }) == INDEX_NONE)
					{
						continue;
					}
				}

				bool okAction = false;
				TSoftObjectPtr<UAnimInstance> animInstance = ActionInstances[i]->OnEnterInner(this, okAction, asSimulation);

				if (okAction)
				{
					//Action Begins
					{
						if (actions.Contains(ActionInstances[i]))
						{
							ActionInstances[i]->OnActionRepeat(inDatas, usedInputs, this, inDelta);
						}
						else
						{
							ActionInstances[i]->OnActionBegins(inDatas, usedInputs, this, inDelta);
						}
					}

					//Register montage call backs
					if (ActionInstances[i]->Montage.Montage != nullptr && animInstance != nullptr && !asSimulation)
					{
						ActionInstances[i]->_EndDelegate.BindUObject(this, &UModularControllerComponent::OnAnimationEnded);
						animInstance->Montage_SetEndDelegate(ActionInstances[i]->_EndDelegate);
					}
					else if (actions.Contains(ActionInstances[i]) && !asSimulation)
					{
						OnActionCancelledEvent.Broadcast(ActionInstances[i]);
					}

					if (!actions.Contains(ActionInstances[i]))
					{
						actions.Add(ActionInstances[i]);
					}
				}
				else
				{
					ActionInstances[i]->OnActionEnds(inDatas, usedInputs, this, inDelta);
				}
			}
		}
	}

	//Get velocity and rotation from valid behaviour or the master one
	{
		if (actions.Num() > 0)
		{
			int rmActionIndex = -1;
			int RMpriority = -1;
			for (int i = 0; i < actions.Num(); i++)
			{
				if (actions[i]->RootMotionMode != ERootMotionType::RootMotionType_No_RootMotion && actions[i]->GetPriority() > RMpriority)
				{
					rmActionIndex = i;
					RMpriority = actions[i]->GetPriority();
				}
			}

			//Is there an action using Root Motion?
			if (actions.IsValidIndex(rmActionIndex))
			{
				move = actions[rmActionIndex]->OnActionProcess(inDatas, inputs, this, inDelta);

				//Rotation
				if (actions[rmActionIndex]->RootMotionMode != ERootMotionType::RootMotionType_No_RootMotion)
				{
					move.Rotation *= GetRootMotionQuat(GetSkeletalMesh());
				}

				//Translation
				switch (actions[rmActionIndex]->RootMotionMode)
				{
				case RootMotionType_AdditivePrimary:
				{
					move.ConstantLinearVelocity += GetRootMotionVector(GetSkeletalMesh()) * move._rooMotionScale;
				}break;
				case RootMotionType_AdditiveSecondary:
				{
					move.InstantLinearVelocity += GetRootMotionVector(GetSkeletalMesh()) * move._rooMotionScale * inDelta;
				}break;
				case RootMotionType_OverridePrimary:
				{
					move.ConstantLinearVelocity = GetRootMotionVector(GetSkeletalMesh()) * move._rooMotionScale * 1 / (inDelta);
				}break;
				case RootMotionType_OverrideSecondary:
				{
					move.InstantLinearVelocity = GetRootMotionVector(GetSkeletalMesh()) * move._rooMotionScale * inDelta;
				}break;
				case RootMotionType_OverrideAll:
				{
					move.ConstantLinearVelocity = GetRootMotionVector(GetSkeletalMesh()) * move._rooMotionScale * 1 / inDelta;
					move.InstantLinearVelocity = FVector::ZeroVector;
				}break;
				}

				actions[rmActionIndex]->OnActionPostProcess(move, inDatas, inputs, this, inDelta);
			}
			else
			{
				//Find the highest priority action
				int priority = -1;
				int indexOfItem = -1;
				for (int i = 0; i < actions.Num(); i++)
				{
					if (actions[i]->GetPriority() > priority)
					{
						indexOfItem = i;
						priority = actions[i]->GetPriority();
					}
				}

				if (!actions.IsValidIndex(indexOfItem))
					indexOfItem = 0;

				move = actions[indexOfItem]->OnActionProcess(inDatas, inputs, this, inDelta);
				actions[indexOfItem]->OnActionPostProcess(move, inDatas, inputs, this, inDelta);
			}
		}
	}

	inDatas.FinalActionsIndexes.Empty();
	for (int i = 0; i < ActionInstances.Num(); i++)
	{
		if (actions.Contains(ActionInstances[i]))
		{
			inDatas.FinalActionsIndexes.Add(i);
		}
	}

	return move;
}

void UModularControllerComponent::OnActionBegins_Implementation(UBaseAction* action)
{
}

void UModularControllerComponent::OnActionEnds_Implementation(UBaseAction* action)
{
}

void UModularControllerComponent::OnActionCancelled_Implementation(UBaseAction* action)
{
}

void UModularControllerComponent::OnAnimationEnded(UAnimMontage* Montage, bool bInterrupted)
{
	TArray<UBaseAction*> actions = ActionInstances;
	if (actions.Num() <= 0)
		return;

	int index = actions.IndexOfByPredicate([Montage](UBaseAction* action)->bool { return action->Montage.Montage == Montage; });

	if (index == INDEX_NONE)
		return;

	if (bInterrupted)
		OnActionCancelledEvent.Broadcast(actions[index]);
	else
		OnActionEndsEvent.Broadcast(actions[index]);

	actions[index]->_EndDelegate.Unbind();
	actions[index]->OnExitInner(true);
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
					for (int i = 0; i < instance->MontageInstances.Num(); i++)
						OnAnimationEnded(instance->MontageInstances[i]->Montage, true);
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
					for (int i = 0; i < instance->MontageInstances.Num(); i++)
						OnAnimationEnded(instance->MontageInstances[i]->Montage, true);
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
		//Unlink
		auto instance = _linkedAnimClasses[target][key];
		if (instance != nullptr)
		{
			for (int i = 0; i < instance->MontageInstances.Num(); i++)
				OnAnimationEnded(instance->MontageInstances[i]->Montage, true);
		}
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


FVelocity UModularControllerComponent::EvaluateMove(const FKinematicInfos& inDatas, FVelocity movement, float delta)
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
	if (inDatas.bUsePhysic && _collisionForces.Length() > 0)
	{
		priMove += (_collisionForces / inDatas.GetMass()) * (priMove.Length() > 0 ? FMath::Clamp(FVector::DotProduct(priMove.GetSafeNormal(), _collisionForces.GetSafeNormal()), 0, 1) : 1);
		if (ShowDebug)
		{
			GEngine->AddOnScreenDebugMessage((int32)GetOwner()->GetUniqueID() + 10, 1, FColor::Green, FString::Printf(TEXT("Applying collision force: %s"), *_collisionForces.ToString()));
		}
		_collisionForces = FVector(0);
	}

	//Primary Movement (momentum movement)
	{
		FHitResult sweepMoveHit = FHitResult(EForceInit::ForceInitToZero);
		bool blockingHit = false;

		blockingHit = ComponentTraceCastSingle(sweepMoveHit, initialLocation, priMove * delta, primaryRotation, 0.100, bUseComplexCollision);
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
		primaryDelta = ((blockingHit ? sweepMoveHit.Location : sweepMoveHit.TraceEnd) - location);
		location = (blockingHit ? sweepMoveHit.Location : sweepMoveHit.TraceEnd);
	}

	//Secondary Movement (Adjustement movement)
	{
		FHitResult sweepMoveHit;

		ComponentTraceCastSingle(sweepMoveHit, location, secMove, primaryRotation, 0.100, bUseComplexCollision);

		FVector newLocation = (sweepMoveHit.IsValidBlockingHit() ? sweepMoveHit.Location : sweepMoveHit.TraceEnd);
		FVector depenetrationForce = FVector(0);
		if (CheckPenetrationAt(depenetrationForce, newLocation, primaryRotation))
		{
			newLocation += depenetrationForce;
			if (inDatas.IsDebugMode)
			{
				UKismetSystemLibrary::DrawDebugArrow(this, newLocation, newLocation + depenetrationForce, 50, FColor::Red, 0, 3);
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
	inDatas.FinalStateIndex = stateIndex;

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
		if (inDatas.IsDebugMode)
		{
			GEngine->AddOnScreenDebugMessage(152, 1, FColor::Red, FString::Printf(TEXT("Cannot normalize right vector: up = %s, fwd= %s"), *desiredUpVector.ToCompactString(), *virtualFwdDir.ToCompactString()));
		}
		return GetOwner()->GetActorQuat();
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

	if (ShowDebug) {
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

			if (ShowDebug) {
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
						if (ShowDebug) {
							DrawCircle(GetWorld(), endLocation, GetRotation().GetAxisX(), GetRotation().GetAxisY(), FColor::Yellow, 25, 32, false, -1, 0, 3);
						}
					}
					else {
						endLocation = secondaryMove.Location;
						if (ShowDebug) {
							DrawCircle(GetWorld(), endLocation, GetRotation().GetAxisX(), GetRotation().GetAxisY(), FColor::Orange, 25, 32, false, -1, 0, 3);
						}
					}
				}
				else
				{
					endLocation = secondaryMove.TraceEnd;

					if (ShowDebug) {
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
