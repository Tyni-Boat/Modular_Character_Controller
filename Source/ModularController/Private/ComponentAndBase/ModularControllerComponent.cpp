// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#include "ComponentAndBase/ModularControllerComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Pawn.h"
#include "Engine.h"
#include "Engine/World.h"


#pragma region Core and Constructor


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
	for (int i = 0; i < StateClasses.Num(); i++)
	{
		if (StateClasses[i] == nullptr)
			continue;
		UBaseState* instance = NewObject<UBaseState>(StateClasses[i], StateClasses[i]);
		StatesInstances.Add(instance);
	}
	StatesInstances.Sort([](UBaseState& a, UBaseState& b) { return a.GetPriority() > b.GetPriority(); });

	//Action behaviors
	ActionInstances.Empty();
	for (int i = 0; i < ActionClasses.Num(); i++)
	{
		if (ActionClasses[i] == nullptr)
			continue;
		UBaseAction* instance = NewObject<UBaseAction>(ActionClasses[i], ActionClasses[i]);
		ActionInstances.Add(instance);
	}

	//Init last move
	LastMoveMade = FKinematicInfos(GetOwner()->GetActorTransform(), FVelocity(), FSurfaceInfos(), -1, TArray<int>());
	LastMoveMade.FinalTransform = LastMoveMade.InitialTransform;
	_netSyncState = NetSyncState_WaitingFirstSync;
	_serverToClient_CorrectionSync.TimeStamp = -1;
	_serverToClient_AdjustmentSync.Empty();
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
		StandAloneUpdateComponent(movement, _user_inputPool, delta, FVelocity::Null());
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
	EvaluateRootMotions(DeltaTime);

	if (UpdatedPrimitive != nullptr && UpdatedPrimitive->IsSimulatingPhysics())
	{
		UpdatedPrimitive->GetBodyInstance()->AddCustomPhysics(OnCalculateCustomPhysics);
	}
	else
	{
		MainUpdateComponent(DeltaTime);
	}
}


#pragma endregion



#pragma region Input Handling


void UModularControllerComponent::ListenInput(const FName key, const FInputEntry entry)
{
	if (_ownerPawn == nullptr)
		return;
	if (!_ownerPawn->IsLocallyControlled())
		return;
	_user_inputPool.AddOrReplace(key, entry);
}

FInputEntry UModularControllerComponent::ReadInput(const FName key) const
{
	return _user_inputPool.ReadInput(key);
}

#pragma endregion



#pragma region Network Logic

#pragma region Common Logic

void UModularControllerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//DOREPLIFETIME(UModularControllerComponent, StatesInstances);
	//DOREPLIFETIME(UModularControllerComponent, ActionInstances);
}


ENetRole UModularControllerComponent::GetNetRole()
{
	return GetOwner()->GetLocalRole();
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


FKinematicInfos UModularControllerComponent::StandAloneUpdateComponent(FKinematicInfos& movementInfos, FInputEntryPool& usedInputPool, float delta, FVelocity overrideVelocity, bool bOverrideMove, bool bOverrideRotation, bool noInputsUpdate)
{
	movementInfos.ChangeActor(this, BoneName, ShowDebug);
	const FVelocity primaryMotion = EvaluateState(movementInfos, usedInputPool, delta);
	FVelocity alteredMotion = EvaluateAction(primaryMotion, movementInfos, usedInputPool, delta);
	EvaluateRootMotionOverride(alteredMotion, movementInfos, delta);
	FQuat finalRot = HandleRotation(alteredMotion, movementInfos, delta);
	alteredMotion.Rotation = finalRot;
	if (!noInputsUpdate)
		usedInputPool.UpdateInputs(delta);
	if (bOverrideMove)
	{
		alteredMotion.ConstantLinearVelocity = overrideVelocity.ConstantLinearVelocity;
		alteredMotion.InstantLinearVelocity = overrideVelocity.InstantLinearVelocity;
	}
	if (bOverrideRotation)
	{
		alteredMotion.Rotation = overrideVelocity.Rotation;
	}

	FVelocity resultingMove = Move(movementInfos, alteredMotion, delta);
	resultingMove._rooMotionScale = alteredMotion._rooMotionScale;
	if (ShowDebug)
	{
		UKismetSystemLibrary::DrawDebugArrow(this, movementInfos.InitialTransform.GetLocation(), movementInfos.InitialTransform.GetLocation() + alteredMotion.ConstantLinearVelocity, 50, FColor::Silver);
		UKismetSystemLibrary::DrawDebugArrow(this, movementInfos.FinalTransform.GetLocation(), movementInfos.FinalTransform.GetLocation() + movementInfos.FinalVelocities.ConstantLinearVelocity, 50, FColor::Yellow);
	}
	PostMoveUpdate(movementInfos, resultingMove, delta);
	return  movementInfos;
}


#pragma endregion



#pragma region Server Logic


void UModularControllerComponent::MultiCastMoveSync_Implementation(FSyncMoveRequest movementRequest)
{
	FSyncMoveRequest request = movementRequest;

	ENetRole role = GetNetRole();
	switch (role)
	{
	case ENetRole::ROLE_Authority: {
	}break;
	case ENetRole::ROLE_AutonomousProxy: {
		_serverToClient_AdjustmentSync.Enqueue(request);
	}break;
	default: {
		_serverToClient_MoveCommandQueue.Enqueue(request);
	}break;
	}
}


#pragma region Listened


void UModularControllerComponent::ListenServerUpdateComponent(float delta)
{
	FKinematicInfos movement = FKinematicInfos(this, GetGravity(), LastMoveMade, GetMass(), false);
	int currentClientTimeStamp = -1;

	//Execute the move
	{
		movement.bUsePhysic = bUsePhysicAuthority;
		movement = StandAloneUpdateComponent(movement, _user_inputPool, delta, FVelocity::Null());
	}

	//Communication with the network
	{
		currentClientTimeStamp = _timeStamp;
		_timeStamp %= std::numeric_limits<int>::max();
		_timeStamp++;

		//Send move response to clients
		const FSyncMoveRequest syncRequest = FSyncMoveRequest(movement, _user_inputPool, currentClientTimeStamp, delta);
		MultiCastMoveSync(syncRequest);
	}

	LastMoveMade = movement;
}


#pragma endregion

#pragma region Dedicated


void UModularControllerComponent::MultiCastCorrectionSync_Implementation(FSyncMoveRequest movementRequest)
{
	FSyncMoveRequest request = movementRequest;

	ENetRole role = GetNetRole();
	switch (role)
	{
	case ENetRole::ROLE_Authority: {
	}break;
	case ENetRole::ROLE_AutonomousProxy: {
		_serverToClient_CorrectionSync = request;
	}break;
	default: {
		_serverToClient_CorrectionSync = request;
	}break;
	}
}


void UModularControllerComponent::DedicatedServerUpdateComponent(float delta)
{
	_dedicatedServerSyncRequests.Empty();
	FKinematicInfos movement = FKinematicInfos(this, GetGravity(), LastMoveMade, GetMass(), ShowDebug);
	int currentClientTimeStamp = -1;
	int correctionIndex = -1;
	FSyncMoveRequest correctionRequest;

	if (_netSyncState != NetSyncState_WaitingClientCorrectioncknowledge)
	{
		//Make the last client requested move if any
		if (!_clientToServer_MoveRequestList.IsEmpty())
		{
			int lastIndex = _clientToServer_MoveRequestList.Num() - 1;

			if (ShowDebug)
				GEngine->AddOnScreenDebugMessage(114, 0.1f, FColor::White, FString::Printf(TEXT("%d client move requests; %d is the request Time Stamp; %d is the last Time Stamp; %f is the latency; delta time: %f"), _clientToServer_MoveRequestList.Num(), _clientToServer_MoveRequestList[lastIndex].TimeStamp, _lastClientTimeStamp, _clientToServerLatency, _clientToServer_MoveRequestList[lastIndex].DeltaTime));

			if (_clientToServer_MoveRequestList[lastIndex].TimeStamp == _lastClientTimeStamp + 1 || _clientToServer_MoveRequestList[lastIndex].TimeStamp == _lastClientTimeStamp)
			{
				_clientToServer_MoveRequestList[lastIndex].MoveInfos.ToKinematicMove(movement, true);
				_clientToServer_MoveRequestList[lastIndex].Inputs.ToInputs(_user_inputPool);
				currentClientTimeStamp = _clientToServer_MoveRequestList[lastIndex].TimeStamp;
				_lastClientTimeStamp = currentClientTimeStamp;

				//Move
				FHitResult hitWhilePositionning;
				MoveUpdatedComponent(_clientToServer_MoveRequestList[lastIndex].MoveInfos.Location - GetLocation(), movement.InitialTransform.GetRotation(), true, &hitWhilePositionning);
				if (hitWhilePositionning.IsValidBlockingHit())
				{
					correctionIndex = lastIndex;
					correctionRequest = _clientToServer_MoveRequestList[lastIndex];
				}
				movement.InitialTransform = GetOwner()->GetTransform();
				movement.bUsePhysic = bUsePhysicAuthority;
				movement = StandAloneUpdateComponent(movement, _user_inputPool, _clientToServer_MoveRequestList[lastIndex].DeltaTime, movement.FinalVelocities, true, true);
				LastMoveMade = movement;

				//Register
				FSyncMoveRequest request = FSyncMoveRequest(movement, _user_inputPool, currentClientTimeStamp, _clientToServer_MoveRequestList[lastIndex].DeltaTime);
				_dedicatedServerSyncRequests.Add(request);
				_clientToServerLatency = 0;
				_clientToServer_MoveRequestList.RemoveAt(lastIndex);

			}
			else if (_lastClientTimeStamp < _clientToServer_MoveRequestList[lastIndex].TimeStamp)
			{
				int timeStampGap = _clientToServer_MoveRequestList[lastIndex].TimeStamp - _lastClientTimeStamp;

				if (ShowDebug)
					GEngine->AddOnScreenDebugMessage(1133, 0.1f, FColor::Yellow, FString::Printf(TEXT("%d Packets lost. Simulating..."), timeStampGap));

				//Fill the gap
				{
					currentClientTimeStamp = _lastClientTimeStamp + 1;
					_clientToServer_MoveRequestList[lastIndex].MoveInfos.ToKinematicMove(movement);

					//Evaluate parameters
					FVector simulatedPosition = FMath::Lerp(GetLocation(), _clientToServer_MoveRequestList[lastIndex].MoveInfos.Location, UKismetMathLibrary::NormalizeToRange(currentClientTimeStamp, _lastClientTimeStamp, _clientToServer_MoveRequestList[lastIndex].TimeStamp));

					FKinematicInfos annexMove = movement;
					_clientToServer_MoveRequestList[lastIndex].MoveInfos.ToKinematicMove(annexMove);
					FQuat simulatedRotation = FQuat::Slerp(GetRotation(), annexMove.InitialTransform.GetRotation(), UKismetMathLibrary::NormalizeToRange(currentClientTimeStamp, _lastClientTimeStamp, _clientToServer_MoveRequestList[lastIndex].TimeStamp));

					movement.InitialTransform.SetLocation(simulatedPosition);
					movement.InitialTransform.SetRotation(simulatedRotation);

					_lastClientTimeStamp = currentClientTimeStamp;


					//Move
					FVector velocity = simulatedPosition - GetLocation();
					FVelocity targetVel = FVelocity::Null();
					targetVel.ConstantLinearVelocity = velocity / delta;

					targetVel.Rotation = simulatedRotation;

					auto inputs = _user_inputPool;

					movement.bUsePhysic = bUsePhysicAuthority;
					movement = StandAloneUpdateComponent(movement, inputs, delta, targetVel, true, true);
					LastMoveMade = movement;

					//Register
					FSyncMoveRequest request = FSyncMoveRequest(movement, _user_inputPool, currentClientTimeStamp, delta);
					_dedicatedServerSyncRequests.Add(request);
				}
			}
			else
			{
				if (ShowDebug)
					GEngine->AddOnScreenDebugMessage(112, 1, FColor::Red, FString::Printf(TEXT("%d Impossible Time Stamp found. Correction!"), _clientToServer_MoveRequestList[lastIndex].TimeStamp - _lastClientTimeStamp));
				//emergencyCorrection = true;
				_clientToServer_MoveRequestList.Empty();
			}
		}
		else
		{
			if (ShowDebug)
				GEngine->AddOnScreenDebugMessage(1111, 0.1f, FColor::White, FString::Printf(TEXT("no client move requests, simulating move...")));

			movement.bUsePhysic = bUsePhysicAuthority;
			movement = StandAloneUpdateComponent(movement, _user_inputPool, delta, FVelocity::Null(), true);
			LastMoveMade = movement;

			//Register
			FSyncMoveRequest request = FSyncMoveRequest(movement, _user_inputPool, _lastClientTimeStamp, delta);
			_dedicatedServerSyncRequests.Add(request);
		}
	}

	//Communication with the network
	{
		//Is a correction send?
		if ((_netSyncState != NetSyncState_WaitingClientCorrectioncknowledge && correctionIndex > 0))
		{
			FVector positionOffset = correctionRequest.MoveInfos.Location - movement.InitialTransform.GetLocation();
			if ((positionOffset.Length() > CorrectionDistance))
			{
				_clientToServer_MoveRequestList.Empty();
				_netSyncState = NetSyncState_WaitingClientCorrectioncknowledge;
				_lastClientTimeStamp = 0;

				//Send reliably correction to clients once
				FSyncMoveRequest fixRequest = FSyncMoveRequest(movement, _user_inputPool, currentClientTimeStamp, delta);
				MultiCastCorrectionSync(fixRequest);
			}
		}

		if (_netSyncState != NetSyncState_WaitingClientCorrectioncknowledge)
		{
			for (int i = 0; i < _dedicatedServerSyncRequests.Num(); i++)
			{
				//Propagation
				MultiCastMoveSync(_dedicatedServerSyncRequests[i]);
			}
		}
	}

	_clientToServerLatency += delta;
}


#pragma endregion

#pragma endregion



#pragma region Client Logic

bool UModularControllerComponent::ApplyServerMovementCorrection(FKinematicInfos& outCorrected)
{
	if (_serverToClient_CorrectionSync.TimeStamp < 0)
		return false;

	_clientSelf_MoveHistory.Empty();
	_serverToClient_CorrectionSync.MoveInfos.ToKinematicMove(outCorrected);

	//Acknwolwdge that movement
	_serverToClient_CorrectionSync.TimeStamp = -1;
	_serverToClient_AdjustmentSync.Empty();

	return true;
}


#pragma region Automonous Proxy


void UModularControllerComponent::ServerMoveSync_Implementation(FSyncMoveRequest movementRequest)
{
	if (movementRequest.TimeStamp <= 0)
	{
		//Bad infos
		return;
	}

	if (_netSyncState == NetSyncState_WaitingClientCorrectioncknowledge)
	{
		//No more requets until correction
		return;
	}

	APawn* pawn = _ownerPawn.Get();
	if (pawn->IsLocallyControlled())
		return;
	FSyncMoveRequest request = movementRequest;
	_clientToServer_MoveRequestList.Insert(request, 0);
}


void UModularControllerComponent::ServerCorrectionAcknowledge_Implementation(FSyncMoveRequest movementRequest)
{
	FSyncMoveRequest request = movementRequest;

	if (_netSyncState == NetSyncState_WaitingClientCorrectioncknowledge)
	{
		_clientToServer_MoveRequestList.Empty();

		if (request.TimeStamp != -101)
		{
			return;
		}
		_netSyncState = NetSyncState_Synchronisation;
	}
}


void UModularControllerComponent::AutonomousProxyUpdateComponent(float delta)
{
	//Adjustment
	FVector correctionOffsetPosition;
	if (ApplyServerMovementAdjustments(correctionOffsetPosition))
	{
		MoveUpdatedComponent((correctionOffsetPosition - GetLocation()) * (AdjustmentSpeed > 0 ? delta * AdjustmentSpeed : 1), LastMoveMade.InitialTransform.GetRotation(), false);
	}

	FKinematicInfos movement = FKinematicInfos(this, GetGravity(), LastMoveMade, GetMass(), ShowDebug);
	bool correctionOccurs = false;

	//Correction
	{
		//Make correction from server
		correctionOccurs = ApplyServerMovementCorrection(movement);
		if (correctionOccurs)
		{
			if (ShowDebug)
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("[Autonomous] - Correction Occurs")));
			_serverToClient_CorrectionSync.Inputs.ToInputs(_user_inputPool);
			_serverToClient_CorrectionSync.MoveInfos.ToKinematicMove(movement);
			FHitResult hit = FHitResult();
			SafeMoveUpdatedComponent(_serverToClient_CorrectionSync.MoveInfos.Location - GetLocation(), movement.FinalTransform.GetRotation(), false, hit, ETeleportType::TeleportPhysics);
			movement.bUsePhysic = bUsePhysicClients;
			StandAloneUpdateComponent(movement, _user_inputPool, _serverToClient_CorrectionSync.DeltaTime, movement.FinalVelocities);
			_timeStamp = -101;
		}
	}


	//Execute the move
	{
		if (!correctionOccurs)
		{
			movement.bUsePhysic = bUsePhysicClients;
			StandAloneUpdateComponent(movement, _user_inputPool, delta, FVelocity::Null());
		}
	}

	//Communication with the network
	{
		//Send move and inputs to server 
		const FSyncMoveRequest moveRequest = FSyncMoveRequest(movement, _user_inputPool, _timeStamp, delta);

		//We made correction and said the server we made it. now back to nowmal
		if (correctionOccurs)
		{
			ServerCorrectionAcknowledge(moveRequest);
			_timeStamp = 1;
		}
		else
		{
			_clientSelf_MoveHistory.Insert(moveRequest, 0);
			if (_clientSelf_MoveHistory.Num() > MaxClientHistoryBufferSize && _clientSelf_MoveHistory.Num() > 0)
			{
				_clientSelf_MoveHistory.RemoveAt(_clientSelf_MoveHistory.Num() - 1);
			}
			_timeStamp %= std::numeric_limits<int>::max();
			_timeStamp++;
			ServerMoveSync(moveRequest);
		}
	}

	LastMoveMade = movement;
}


bool UModularControllerComponent::ApplyServerMovementAdjustments(FVector& offset)
{
	if (_serverToClient_AdjustmentSync.IsEmpty())
		return false;

	FSyncMoveRequest adjustment;

	if (!_serverToClient_AdjustmentSync.IsEmpty())
	{
		bool adjustmentMade = false;

		while (_serverToClient_AdjustmentSync.Dequeue(adjustment))
		{
			//Find move in move history
			int correctionChkIndex = _clientSelf_MoveHistory.IndexOfByPredicate([this, adjustment](const FSyncMoveRequest item)->bool { return item.TimeStamp == adjustment.TimeStamp; });

			if (_clientSelf_MoveHistory.IsValidIndex(correctionChkIndex))
			{
				adjustmentMade = true;

				//Calculate the offset from that position
				FVector positionOffset = adjustment.MoveInfos.Location - _clientSelf_MoveHistory[correctionChkIndex].MoveInfos.Location;

				//Apply the offset to the history and delete he entry that are no longer valids
				int k = _clientSelf_MoveHistory.Num() - 1;
				for (int i = k; i >= 0; i--)
				{
					if (i > correctionChkIndex)
					{
						_clientSelf_MoveHistory.RemoveAt(i);
						continue;
					}

					_clientSelf_MoveHistory[i].MoveInfos.Location += positionOffset;
				}

				offset = _clientSelf_MoveHistory[0].MoveInfos.Location;

				if (ShowDebug)
					GEngine->AddOnScreenDebugMessage(1444, 0.1f, FColor::Magenta, FString::Printf(TEXT("Apply Server Movement Adjustments: at: %d, moving by: %s"), _clientSelf_MoveHistory[correctionChkIndex].TimeStamp, *positionOffset.ToCompactString()));
			}
			else
			{
				if (ShowDebug)
					GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Purple, FString::Printf(TEXT("Out of bound index: %d in %d; times stamp = %d"), correctionChkIndex, _clientSelf_MoveHistory.Num(), adjustment.TimeStamp));
			}
		}

		return adjustmentMade;
	}
	else
	{
		if (ShowDebug)
			GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Purple, FString::Printf(TEXT("No request")));
	}

	return false;
}


#pragma endregion

#pragma region Simulated Proxy


void UModularControllerComponent::SimulatedProxyUpdateComponent(float delta)
{
	//Gather server move infos
	{
		FKinematicInfos movement = FKinematicInfos(this, GetGravity(), LastMoveMade, GetMass(), ShowDebug);
		float deltaTime = delta;
		FSyncMoveRequest serverMove;
		FVelocity overrideVel = FVelocity::Null();

		if (_serverToClient_MoveCommandQueue.Dequeue(serverMove))
		{
			deltaTime = serverMove.DeltaTime;
			serverMove.Inputs.ToInputs(_user_inputPool);
			serverMove.MoveInfos.ToKinematicMove(movement, true);
			_lastServerResquest = serverMove;

			MoveUpdatedComponent(movement.InitialTransform.GetLocation() - GetLocation(), movement.InitialTransform.GetRotation(), false);
			overrideVel = movement.FinalVelocities;
		}
		else if (_lastServerResquest.TimeStamp > 0)
		{
			_lastServerResquest.MoveInfos.ToKinematicMove(movement, true);
			overrideVel = LastMoveMade.FinalVelocities;
		}

		movement.bUsePhysic = bUsePhysicClients;
		StandAloneUpdateComponent(movement, _user_inputPool, deltaTime, overrideVel, true);
		LastMoveMade = movement;
	}
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


bool UModularControllerComponent::CheckStateBehaviourByType(TSubclassOf<UBaseState> moduleType)
{
	if (StatesInstances.Num() <= 0)
		return false;
	auto index = StatesInstances.IndexOfByPredicate([moduleType](UBaseState* state) -> bool { return state->GetClass() == moduleType; });
	return StatesInstances.IsValidIndex(index);
}

bool UModularControllerComponent::CheckStateBehaviourByName(FName moduleName)
{
	if (StatesInstances.Num() <= 0)
		return false;
	auto index = StatesInstances.IndexOfByPredicate([moduleName](UBaseState* state) -> bool { return state->GetDescriptionName() == moduleName; });
	return StatesInstances.IsValidIndex(index);
}

bool UModularControllerComponent::CheckStateBehaviourByPriority(int modulePriority)
{
	if (StatesInstances.Num() <= 0)
		return false;
	auto index = StatesInstances.IndexOfByPredicate([modulePriority](UBaseState* state) -> bool { return state->GetPriority() == modulePriority; });
	return StatesInstances.IsValidIndex(index);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UModularControllerComponent::AddStateBehaviour_Implementation(TSubclassOf<UBaseState> moduleType)
{
	if (moduleType == nullptr)
		return;
	if (CheckStateBehaviourByType(moduleType))
		return;
	UBaseState* instance = NewObject<UBaseState>(moduleType, moduleType);
	StatesInstances.Add(instance);
	StatesInstances.Sort([](UBaseState& a, UBaseState& b) { return a.GetPriority() > b.GetPriority(); });
}


UBaseState* UModularControllerComponent::GetStateByType(TSubclassOf<UBaseState> moduleType)
{
	if (StatesInstances.Num() <= 0)
		return nullptr;
	auto index = StatesInstances.IndexOfByPredicate([moduleType](UBaseState* state) -> bool { return state->GetClass() == moduleType; });
	if (StatesInstances.IsValidIndex(index))
	{
		return StatesInstances[index];
	}
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UModularControllerComponent::RemoveStateBehaviourByType_Implementation(TSubclassOf<UBaseState> moduleType)
{
	if (CheckStateBehaviourByType(moduleType))
	{
		auto behaviour = StatesInstances.FindByPredicate([moduleType](UBaseState* state) -> bool { return state->GetClass() == moduleType->GetClass(); });
		StatesInstances.Remove(*behaviour);
		StatesInstances.Sort([](UBaseState& a, UBaseState& b) { return a.GetPriority() > b.GetPriority(); });
		return;
	}
}

void UModularControllerComponent::RemoveStateBehaviourByName_Implementation(FName moduleName)
{
	if (CheckStateBehaviourByName(moduleName))
	{
		auto behaviour = StatesInstances.FindByPredicate([moduleName](UBaseState* state) -> bool { return state->GetDescriptionName() == moduleName; });
		StatesInstances.Remove(*behaviour);
		StatesInstances.Sort([](UBaseState& a, UBaseState& b) { return a.GetPriority() > b.GetPriority(); });
		return;
	}
}

void UModularControllerComponent::RemoveStateBehaviourByPriority_Implementation(int modulePriority)
{
	if (CheckStateBehaviourByPriority(modulePriority))
	{
		auto behaviour = StatesInstances.FindByPredicate([modulePriority](UBaseState* state) -> bool { return state->GetPriority() == modulePriority; });
		StatesInstances.Remove(*behaviour);
		StatesInstances.Sort([](UBaseState& a, UBaseState& b) { return a.GetPriority() > b.GetPriority(); });
		return;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FVelocity UModularControllerComponent::EvaluateState(FKinematicInfos& inDatas, const FInputEntryPool inputs, const float inDelta, const bool asSimulation)
{
	FVelocity movement = inDatas.InitialVelocities;
	FVector up = -inDatas.Gravity.GetSafeNormal();
	FVector fwd = FVector::VectorPlaneProject(inDatas.InitialTransform.GetRotation().Vector(), up);
	if (!fwd.Normalize())
	{
		fwd = GetForwardVector();
	}
	FQuat primaryRot = UKismetMathLibrary::MakeRotationFromAxes(fwd, FVector::CrossProduct(up, fwd), up).Quaternion();
	FQuat secondaryRot = FQuat::Identity;
	int behaviourPriority = -1;
	int behaviourIndex = -1;


	//Check if a behaviour's check have success state
	{
		//Check if the state is already defined
		if (behaviourIndex < 0)
		{
			if (StatesInstances.IsValidIndex(inDatas.FinalStateIndex))
			{
				behaviourIndex = inDatas.FinalStateIndex;
				StatesInstances[behaviourIndex]->ComputeFromFlag(inDatas.FinalStateFlag);
			}
		}

		//Check if a valid action freeze the current state
		if (behaviourIndex < 0)
		{
			for (int i = inDatas.InitialActionsIndexes.Num() - 1; i >= 0; i--)
			{
				if (!ActionInstances.IsValidIndex(inDatas.InitialActionsIndexes[i]))
					continue;

				if (ActionInstances[inDatas.InitialActionsIndexes[i]]->FreezeCurrentState)
				{
					behaviourIndex = inDatas.InitialStateIndex;
					break;
				}
			}
		}

		if (behaviourIndex < 0)
		{
			for (int i = 0; i < StatesInstances.Num(); i++)
			{
				if (StatesInstances[i] == nullptr)
					continue;

				if (StatesInstances[i]->CheckState(inDatas, inputs, this, inDelta))
				{
					if (StatesInstances[i]->GetPriority() > behaviourPriority)
					{
						behaviourIndex = i;
						behaviourPriority = StatesInstances[i]->GetPriority();
					}
				}
			}
		}
	}

	//Listen for changes
	{
		inDatas.FinalSurface = StatesInstances.IsValidIndex(behaviourIndex) && (GetNetRole() == ROLE_Authority || GetNetRole() == ROLE_AutonomousProxy) ? StatesInstances[behaviourIndex]->SurfaceInfos : inDatas.InitialSurface;

		if (behaviourIndex != inDatas.InitialStateIndex)
		{
			//_momentum.InstantLinearVelocity = FVector(0);
			if (StatesInstances.IsValidIndex(behaviourIndex))
			{
				//Landing
				StatesInstances[behaviourIndex]->OnEnterState(inDatas, inputs, this, inDelta);
				if (!asSimulation)
					LinkAnimBlueprint(GetSkeletalMesh(), "State", StatesInstances[behaviourIndex]->StateBlueprintClass);
				StatesInstances[behaviourIndex]->SetWasTheLastFrameBehaviour(true);
			}
			else
			{
				if (!asSimulation)
					LinkAnimBlueprint(GetSkeletalMesh(), "State", nullptr);
			}

			if (StatesInstances.IsValidIndex(inDatas.InitialStateIndex))
			{
				//Leaving
				StatesInstances[inDatas.InitialStateIndex]->OnExitState(inDatas, inputs, this, inDelta);
				StatesInstances[inDatas.InitialStateIndex]->SurfaceInfos.Reset();
			}

			for (int i = 0; i < StatesInstances.Num(); i++)
			{
				if (i == behaviourIndex)
					continue;

				StatesInstances[i]->OnBehaviourChanged(StatesInstances.IsValidIndex(behaviourIndex) ? StatesInstances[behaviourIndex]->GetDescriptionName() : "", behaviourPriority, this);
			}

			if (!asSimulation)
			{
				OnBehaviourChanged(StatesInstances.IsValidIndex(behaviourIndex) ? StatesInstances[behaviourIndex] : nullptr
					, StatesInstances.IsValidIndex(inDatas.InitialStateIndex) ? StatesInstances[inDatas.InitialStateIndex] : nullptr);
				OnBehaviourChangedEvent.Broadcast(StatesInstances.IsValidIndex(behaviourIndex) ? StatesInstances[behaviourIndex] : nullptr
					, StatesInstances.IsValidIndex(inDatas.InitialStateIndex) ? StatesInstances[inDatas.InitialStateIndex] : nullptr);
			}

			//Notify actions the change of state
			for (int i = 0; i < ActionInstances.Num(); i++)
			{
				ActionInstances[i]->OnStateChanged(StatesInstances.IsValidIndex(behaviourIndex) ? StatesInstances[behaviourIndex] : nullptr
					, StatesInstances.IsValidIndex(inDatas.InitialStateIndex) ? StatesInstances[inDatas.InitialStateIndex] : nullptr);
			}
		}
		else
		{
			if (StatesInstances.IsValidIndex(behaviourIndex))
			{
				StatesInstances[behaviourIndex]->SetWasTheLastFrameBehaviour(true);
			}
		}

		inDatas.FinalStateIndex = behaviourIndex;
		inDatas._currentStateName = StatesInstances.IsValidIndex(behaviourIndex) ? StatesInstances[behaviourIndex]->GetDescriptionName() : "";
		inDatas.FinalStateFlag = StatesInstances.IsValidIndex(behaviourIndex) ? StatesInstances[behaviourIndex]->StateFlag : 0;

		for (int i = 0; i < StatesInstances.Num(); i++)
		{
			if (i == behaviourIndex)
				continue;
			StatesInstances[i]->SetWasTheLastFrameBehaviour(false);
		}
	}

	//Get velocity and rotation from valid behaviour or the master one
	{
		if (StatesInstances.IsValidIndex(behaviourIndex))
		{
			FVelocity processMotion = movement;
			processMotion = StatesInstances[behaviourIndex]->ProcessState(inDatas, inputs, this, inDelta);

			if (StatesInstances[behaviourIndex]->RootMotionMode != ERootMotionType::RootMotionType_No_RootMotion)
			{
				//Rotation
				processMotion.Rotation *= GetRootMotionQuat(GetSkeletalMesh());

				FVelocity rootMotion = processMotion;
				float rootMotionScale = processMotion._rooMotionScale;
				float processMotionScale = 1 - processMotion._rooMotionScale;

				//Translation
				switch (StatesInstances[behaviourIndex]->RootMotionMode)
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

			//Fuse and post process
			movement = processMotion;
			StatesInstances[behaviourIndex]->PostProcessState(movement, inDatas, inputs, this, inDelta);
		}
		else
		{
			movement.ConstantLinearVelocity = inDatas.GetInitialMomentum() + _gravityVector * inDelta;
		}
	}


	// Update Behaviours Idles
	{
		for (int i = 0; i < StatesInstances.Num(); i++)
		{
			StatesInstances[i]->StateIdle(this, inDelta);
		}
	}

	return movement;
}


void UModularControllerComponent::OnBehaviourChanged_Implementation(UBaseState* OldOne, UBaseState* NewOne)
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




FVelocity UModularControllerComponent::EvaluateAction(const FVelocity inVelocities, FKinematicInfos& inDatas, const FInputEntryPool inputs, const float inDelta, const bool asSimulation)
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
			if (actions[i]->Montage.Montage == nullptr || asSimulation)
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

	UBaseState* state = StatesInstances.IsValidIndex(inDatas.FinalStateIndex) ? StatesInstances[inDatas.FinalStateIndex] : nullptr;
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
		FInputEntryPool usedInputs = actionsIndexToActivate.Num() <= 0 ? inputs : FInputEntryPool();

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



#pragma region Movement

FVelocity UModularControllerComponent::Move_Implementation(const FKinematicInfos& inDatas, FVelocity movement, float delta)
{
	auto owner = GetOwner();
	FVelocity result = FVelocity::Null();

	FVector priMove = movement.ConstantLinearVelocity;
	FVector secMove = movement.InstantLinearVelocity;

	FVector initialLocation = owner->GetActorLocation();
	FVector location = initialLocation;
	FQuat primaryRotation = movement.Rotation;
	FVector primaryDelta = FVector(0);
	FVector secondaryDelta = FVector(0);
	FVector pushObjectForce = priMove;

	//Make the move
	FVector normalDebug = FVector(0);
	FVector impactPointDebug = location;

	////get Pushed by objects
	if (inDatas.bUsePhysic && _collisionForces.Length() > 0)
	{
		priMove += (_collisionForces / inDatas.GetMass()) * (priMove.Length() > 0 ? FMath::Clamp(FVector::DotProduct(priMove.GetSafeNormal(), _collisionForces.GetSafeNormal()), 0, 1) : 1);
		if (ShowDebug)
			GEngine->AddOnScreenDebugMessage((int32)GetOwner()->GetUniqueID() + 10, 1, FColor::Green, FString::Printf(TEXT("Applying collision force: %s"), *_collisionForces.ToString()));
		_collisionForces = FVector(0);
	}

	//Primary Movement (momentum movement)
	{
		FHitResult sweepMoveHit;

		SafeMoveUpdatedComponent(priMove * delta, primaryRotation, true, sweepMoveHit, ETeleportType::None);
		if (sweepMoveHit.IsValidBlockingHit())
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
			normalDebug = sweepMoveHit.Normal;
			impactPointDebug = sweepMoveHit.ImpactPoint;
			float percent = SlideAlongSurface(priMove * delta, 1 - sweepMoveHit.Time, sweepMoveHit.Normal, sweepMoveHit);
		}

		//delta
		primaryDelta = ((sweepMoveHit.IsValidBlockingHit() ? sweepMoveHit.Location : sweepMoveHit.TraceEnd) - location);

		location = (sweepMoveHit.IsValidBlockingHit() ? sweepMoveHit.Location : sweepMoveHit.TraceEnd);
	}

	//Secondary Movement (Adjustement movement)
	{
		FHitResult sweepMoveHit;

		owner->SetActorLocation(location + secMove, true, &sweepMoveHit, ETeleportType::None);

		//delta
		secondaryDelta = ((sweepMoveHit.IsValidBlockingHit() ? sweepMoveHit.Location : sweepMoveHit.TraceEnd) - location);

		location = (sweepMoveHit.IsValidBlockingHit() ? sweepMoveHit.Location : sweepMoveHit.TraceEnd);
	}

	result.ConstantLinearVelocity = primaryDelta / delta;
	result.InstantLinearVelocity = secondaryDelta;

	return result;
}


void UModularControllerComponent::PostMoveUpdate(FKinematicInfos& inDatas, const FVelocity moveMade, const float inDelta)
{
	//Final velocities
	inDatas.FinalVelocities.ConstantLinearVelocity = moveMade.ConstantLinearVelocity;
	inDatas.FinalVelocities.InstantLinearVelocity = FVector(0);
	inDatas.FinalVelocities.Rotation = GetRotation();

	//Position
	inDatas.FinalTransform = inDatas.InitialTransform;
	inDatas.FinalTransform.SetLocation(GetLocation());
	inDatas.FinalTransform.SetRotation(GetRotation());

	//Root Motion
	inDatas.FinalVelocities._rooMotionScale = moveMade._rooMotionScale;

	Velocity = inDatas.GetFinalMomentum();

	UpdateComponentVelocity();
	if (UpdatedPrimitive != nullptr && UpdatedPrimitive->IsSimulatingPhysics())
	{
		UpdatedPrimitive->SetAllPhysicsLinearVelocity(Velocity);
	}
}


FQuat UModularControllerComponent::HandleRotation(const FVelocity inVelocities, const FKinematicInfos inDatas, const float inDelta) const
{
	//Get the good upright vector
	FVector desiredUpVector = -inDatas.Gravity.GetSafeNormal();
	desiredUpVector.Normalize();

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


FVector UModularControllerComponent::SlideAlongSurfaceAt(const FVector& Position, const FQuat& Rotation, const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit)
{
	const FVector OldHitNormal = Normal;

	//Compute slide vector
	FVector SlideDelta = ComputeSlideVector(Delta, Time, Normal, Hit);
	FVector endLocation = Position + SlideDelta;

	if ((SlideDelta | Delta) > 0.f)
	{
		if (ComponentTraceCastSingle(Hit, Position, SlideDelta, Rotation))
		{
			endLocation = Hit.Location;
			// Compute new slide normal when hitting multiple surfaces.
			TwoWallAdjust(SlideDelta, Hit, OldHitNormal);

			// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
			if (!SlideDelta.IsNearlyZero(1e-3f) && (SlideDelta | Delta) > 0.f)
			{
				// Perform second move
				ComponentTraceCastSingle(Hit, endLocation, SlideDelta, Rotation);
				endLocation = Hit.Location;
			}
		}
	}
	return endLocation;
}


#pragma endregion



#pragma region Tools & Utils

bool UModularControllerComponent::ComponentTraceCastMulti(TArray<FHitResult>& outHits, FVector position, FVector direction, FQuat rotation, ECollisionChannel channel)
{
	auto owner = GetOwner();
	if (owner == nullptr)
		return false;

	UPrimitiveComponent* primitive = UpdatedPrimitive;
	if (!primitive)
		return false;

	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(owner);
	float OverlapInflation = 0.100f;
	auto shape = primitive->GetCollisionShape(OverlapInflation);

	if (GetWorld()->SweepMultiByChannel(outHits, position, position + direction, rotation, channel, shape, queryParams, FCollisionResponseParams::DefaultResponseParam))
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


bool UModularControllerComponent::ComponentTraceCastMultiByInflation(TArray<FHitResult>& outHits, FVector position, FVector direction, FQuat rotation, double inflation, ECollisionChannel channel)
{
	auto owner = GetOwner();
	if (owner == nullptr)
		return false;

	UPrimitiveComponent* primitive = UpdatedPrimitive;
	if (!primitive)
		return false;

	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(owner);
	float OverlapInflation = inflation;
	auto shape = primitive->GetCollisionShape(OverlapInflation);

	if (GetWorld()->SweepMultiByChannel(outHits, position, position + direction, rotation, channel, shape, queryParams, FCollisionResponseParams::DefaultResponseParam))
	{
		for (int i = 0; i < outHits.Num(); i++)
		{
			if (!outHits[i].IsValidBlockingHit())
				continue;
			outHits[i].Location -= direction.GetSafeNormal() * (inflation + 0.025f);
		}
	}

	return outHits.Num() > 0;
}


bool UModularControllerComponent::ComponentTraceCastSingle(FHitResult& outHit, FVector position, FVector direction, FQuat rotation, ECollisionChannel channel)
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
	float OverlapInflation = 0.100f;
	auto shape = primitive->GetCollisionShape(OverlapInflation);

	if (GetWorld()->SweepSingleByChannel(outHit, position, position + direction, rotation, channel, UpdatedPrimitive->GetCollisionShape(OverlapInflation), queryParams))
	{
		outHit.Location -= direction.GetSafeNormal() * 0.125f;
	}

	return outHit.IsValidBlockingHit();
}


bool UModularControllerComponent::ComponentTraceCastSingleByInflation(FHitResult& outHit, FVector position, FVector direction, FQuat rotation, double inflation, ECollisionChannel channel)
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
	float OverlapInflation = inflation;
	auto shape = primitive->GetCollisionShape(OverlapInflation);

	if (GetWorld()->SweepSingleByChannel(outHit, position, position + direction, rotation, channel, UpdatedPrimitive->GetCollisionShape(OverlapInflation), queryParams))
	{
		outHit.Location -= direction.GetSafeNormal() * (inflation + 0.025f);
	}

	return outHit.IsValidBlockingHit();
}


void UModularControllerComponent::PathCastComponent(TArray<FHitResult>& results, FVector start, TArray<FVector> pathPoints, ECollisionChannel channel, bool stopOnHit, float skinWeight, bool debugRay, bool rotateAlongPath, bool bendOnCollision)
{
	if (pathPoints.Num() <= 0)
		return;


	auto owner = GetOwner();
	if (owner == nullptr)
		return;

	results.Empty();
	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(owner);


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
			, channel, shape, queryParams, FCollisionResponseParams::DefaultResponseParam);
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


void UModularControllerComponent::PathCastLine(TArray<FHitResult>& results, FVector start, TArray<FVector> pathPoints, ECollisionChannel channel, bool stopOnHit, bool debugRay, bool bendOnCollision)
{
	if (pathPoints.Num() <= 0)
		return;


	auto owner = GetOwner();
	if (owner == nullptr)
		return;

	results.Empty();
	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(owner);

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


bool UModularControllerComponent::CheckPenetrationAt(FVector& force, FVector position, FQuat NewRotationQuat, UPrimitiveComponent* onlyThisComponent, ECollisionChannel channel, bool debugHit)
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
		FMTDResult depenatrationInfos;
		if (GetWorld()->OverlapMultiByChannel(_overlaps, position, NewRotationQuat, channel, primitive->GetCollisionShape(), comQueryParams))
		{
			for (auto& overlap : _overlaps)
			{
				if (!overlapFound)
					overlapFound = true;

				if (debugHit)
				{
					UKismetSystemLibrary::DrawDebugArrow(this, position, overlap.GetActor()->GetActorLocation(), 50, FColor::Emerald, 0, 15);
				}

				if (overlap.Component->ComputePenetration(depenatrationInfos, primitive->GetCollisionShape(), position, NewRotationQuat))
				{
					FVector depForce = depenatrationInfos.Direction * (depenatrationInfos.Distance + 0.125f);
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

	{
		//FVector endLocation = position;

		//// See if we can fit at the adjusted location without overlapping anything.
		//AActor* ActorOwner = UpdatedComponent->GetOwner();
		//if (!ActorOwner)
		//{
		//	return false;
		//}

		//// We really want to make sure that precision differences or differences between the overlap test and sweep tests don't put us into another overlap,
		//// so make the overlap test a bit more restrictive.
		//float OverlapInflation = 0.100f;
		//bool bEncroached = OverlapTest(position, NewRotationQuat, UpdatedPrimitive->GetCollisionObjectType(), UpdatedPrimitive->GetCollisionShape(OverlapInflation), ActorOwner);
		//if (!bEncroached)
		//{
		//	// Move without sweeping.
		//	DrawDebugPoint(GetWorld(), endLocation, 20, FColor::White, false, -1, 30);
		//	return true;
		//}
		//else
		//{
		//	// Disable MOVECOMP_NeverIgnoreBlockingOverlaps if it is enabled, otherwise we wouldn't be able to sweep out of the object to fix the penetration.
		//	TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags, EMoveComponentFlags(MoveComponentFlags & (~MOVECOMP_NeverIgnoreBlockingOverlaps)));

		//	// Try sweeping as far as possible...
		//	FHitResult SweepOutHit(1.f);
		//	FCollisionQueryParams queryParams;
		//	queryParams.AddIgnoredActor(ActorOwner);
		//	queryParams.bDebugQuery = true;

		//	DrawDebugPoint(GetWorld(), endLocation, 20, FColor::Black, false, -1, 30);
		//	bool bMoved = !GetWorld()->SweepSingleByChannel(SweepOutHit, endLocation, endLocation, NewRotationQuat, channel, UpdatedPrimitive->GetCollisionShape(OverlapInflation), queryParams);
		//	endLocation = SweepOutHit.FinalTransform;

		//	// Still stuck?
		//	if (!bMoved && SweepOutHit.bStartPenetrating)
		//	{
		//		// Combine two MTD results to get a new direction that gets out of multiple surfaces.
		//		const FVector SecondMTD = GetPenetrationAdjustment(SweepOutHit);
		//		const FVector CombinedMTD = SecondMTD;
		//		DrawDebugPoint(GetWorld(), endLocation, 20, FColor::Red, false, -1, 30);
		//		if (!CombinedMTD.IsZero())
		//		{
		//			bMoved = !GetWorld()->SweepSingleByChannel(SweepOutHit, endLocation, endLocation + CombinedMTD, NewRotationQuat, channel, UpdatedPrimitive->GetCollisionShape(OverlapInflation), queryParams);
		//			endLocation = SweepOutHit.FinalTransform;
		//		}
		//	}

		//	force = endLocation - position;
		//	return bMoved;

		//}

		//return false;
	}
}


FVector UModularControllerComponent::PointOnShape(FVector direction, const FVector inLocation, bool debugPoint)
{
	FVector bCenter;
	FVector bExtends;
	direction.Normalize();
	GetOwner()->GetActorBounds(true, bCenter, bExtends);
	FVector outterBoundPt = GetLocation() + direction * bExtends.Length();
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

	if (debugPoint)
	{
		UKismetSystemLibrary::DrawDebugPoint(this, onColliderPt + offset, 30, FColor::Yellow, 0);
	}

	return onColliderPt + offset;
}


#pragma endregion
