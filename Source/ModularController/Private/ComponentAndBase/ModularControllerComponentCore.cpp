// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#include "ComponentAndBase/ModularControllerComponent.h"

#include <functional>
#include "GameFramework/Pawn.h"
#include "Engine.h"
#include "FunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Engine/EngineTypes.h"


#pragma region Core and Constructor XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


// Sets default values for this component's properties
UModularControllerComponent::UModularControllerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bCanEverTick = true;
	SecondaryComponentTick.TickGroup = TG_PrePhysics;
	SecondaryComponentTick.bCanEverTick = true;

	SetIsReplicatedByDefault(true);
}


void UModularControllerComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);

	if (bRegister)
	{
		if (SetupActorComponentTickFunction(&SecondaryComponentTick))
		{
			SecondaryComponentTick.Target = this;
		}
	}
	else
	{
		if (SecondaryComponentTick.IsTickFunctionRegistered())
		{
			SecondaryComponentTick.UnRegisterTickFunction();
		}
	}
}

// Called when the game starts
void UModularControllerComponent::BeginPlay()
{
	GetOwner()->SetReplicateMovement(false);
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	SecondaryComponentTick.TickGroup = TG_PrePhysics;
	Super::BeginPlay();
	if (UpdatedPrimitive != nullptr)
	{
		//Init collider
		UpdatedPrimitive->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
		UpdatedPrimitive->SetGenerateOverlapEvents(true);
		UpdatedPrimitive->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		UpdatedPrimitive->OnComponentBeginOverlap.AddDynamic(this, &UModularControllerComponent::BeginOverlap);
	}
	Initialize();
}


void UModularControllerComponent::Initialize()
{
	Velocity = FVector(0);
	OwnerPawn = Cast<APawn>(GetOwner());
	SetGravity(FVector::DownVector * FMath::Abs(GetGravityZ()));

	//Inputs
	_inputPool = NewObject<UInputEntryPool>(UInputEntryPool::StaticClass(), UInputEntryPool::StaticClass());

	//Handle Skeletal mesh operations.
	{
		//Precaching skeletal mesh
		GetSkeletalMesh();
	}

	//State behaviors
	StatesInstances.Empty();
	{
		for (int i = StateClasses.Num() - 1; i >= 0; i--)
		{
			if (!StateClasses[i])
				continue;
			StatesInstances.Add(StateClasses[i]->GetDefaultObject());
		}
		SortStates();
	}

	//Action behaviors
	_onActionMontageEndedCallBack.Unbind();
	_onActionMontageEndedCallBack.BindUObject(this, &UModularControllerComponent::OnActionMontageEnds);
	ActionInstances.Empty();
	{
		ActionMontageInstance = NewObject<UActionMontage>();
		ActionInstances.Add(ActionMontageInstance);
		for (int i = ActionClasses.Num() - 1; i >= 0; i--)
		{
			if (!ActionClasses[i])
				continue;
			ActionInstances.Add(ActionClasses[i]->GetDefaultObject());
		}
		SortActions();
	}

	//Physic Inits
	EvaluateCardinalPoints();

	//Init last move
	_lastLocation = GetLocation();
	_lastRotation = GetRotation();
	ComputedControllerStatus.Kinematics.LinearKinematic.Position = GetLocation();
	ApplyedControllerStatus.Kinematics.LinearKinematic.Position = GetLocation();
	ComputedControllerStatus.Kinematics.AngularKinematic.Orientation = UpdatedPrimitive ? UpdatedPrimitive->GetComponentQuat() : GetRotation();
	ApplyedControllerStatus.Kinematics.AngularKinematic.Orientation = UpdatedPrimitive ? UpdatedPrimitive->GetComponentQuat() : GetRotation();

	//Set time elapsed
	const auto timePassedSince = FDateTime::UtcNow() - FDateTime(2024, 06, 01, 0, 0, 0, 0);
	_timeElapsed = timePassedSince.GetTotalSeconds();
}


// Called every frame
void UModularControllerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (!IsComponentTickEnabled())
		return;

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ThisTickFunction->TickGroup == TG_PrePhysics)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("PrimaryTick_PrePhysic_ModularController");
		// Apply movements here
		MovementTickComponent(DeltaTime);
	}
	else if (ThisTickFunction->TickGroup == TG_DuringPhysics)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("SecondaryTick_PostPhysic_ModularController");
		// Evaluate next frame movements here
		ComputeTickComponent(DeltaTime);

		//Reset external forces
		_externalForces = FVector(0);
	}
}


void UModularControllerComponent::MovementTickComponent(float delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MovementTickComponent");

	//Update Inputs
	if (_inputPool)
		_inputPool->UpdateInputs(delta, DebugType == EControllerDebugType::InputDebug, this);

	//Update Action infos
	for (auto& infos : ActionInfos)
	{
		if (!infos.Key.IsValid())
			continue;
		auto phase = infos.Value.CurrentPhase;
		const int stateIndex = ApplyedControllerStatus.StatusParams.StateIndex;
		const int actionIndex = ApplyedControllerStatus.StatusParams.ActionIndex;
		ActionInfos[infos.Key].Update(delta, CheckActionCompatibility(infos.Key, stateIndex, actionIndex));
		if (ActionInfos[infos.Key].CurrentPhase != phase)
		{
			OnControllerActionPhaseChangedEvent.Broadcast(ActionInfos[infos.Key].CurrentPhase, phase);
		}
	}

	//Apply movement computed
	//if (GetNetMode() == NM_Standalone)
	{
		AuthorityMoveComponent(delta);
	}
}


void UModularControllerComponent::ComputeTickComponent(float delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ComputeTickComponent");

	//Check for any change on the shape
	TrackShapeChanges();

	//Extract Root motion
	ExtractRootMotions(delta);

	//Count time elapsed
	_timeElapsed += delta;

	//Solve collisions
	int maxDepth = 64;
	OverlapSolver(maxDepth, delta, &_contactHits, ApplyedControllerStatus.CustomSolverCheckParameters);

	//Handle tracked surfaces
	HandleTrackedSurface(ApplyedControllerStatus, delta);

	//In StandAlone Mode, don't bother with net logic at all
	if (GetNetMode() == NM_Standalone)
	{
		AuthorityComputeComponent(delta);
		_lastLocation = GetLocation();
		_lastRotation = GetRotation();
		return;
	}

	//Compute depending on net role
	switch (GetNetRole(OwnerPawn))
	{
		case ROLE_SimulatedProxy:
			SimulatedProxyComputeComponent(delta);
			break;
		case ROLE_AutonomousProxy:
			AutonomousProxyUpdateComponent(delta);
			break;
		default:
			{
				if (OwnerPawn.IsValid() && !OwnerPawn->IsLocallyControlled())
				{
					DedicatedServerUpdateComponent(delta);
				}
				else
				{
					AuthorityComputeComponent(delta, true);
				}
			};
			break;
	}

	_lastLocation = GetLocation();
	_lastRotation = GetRotation();
}


FControllerStatus UModularControllerComponent::StandAloneEvaluateStatus(FControllerStatus initialState, float delta, bool noCollision)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("StandAloneEvaluateStatus");
	FControllerStatus processState = initialState;
	processState = EvaluateStatusParams(processState, delta);
	processState = ProcessStatus(processState, delta);
	processState = EvaluateRootMotionOverride(processState, delta, noCollision);
	processState.Kinematics.AngularKinematic = HandleKinematicRotation(processState.Kinematics, delta);

	//Evaluate
	processState.Kinematics = KinematicMoveEvaluation(processState, noCollision || IsIgnoringCollision(), delta);
	return processState;
}


FControllerStatus UModularControllerComponent::StandAloneApplyStatus(FControllerStatus state, float delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("StandAloneApplyStatus");
	ApplyStatusParams(state, delta);
	Move(state.Kinematics, delta);
	KinematicPostMove(state, delta);
	return state;
}

FControllerStatus UModularControllerComponent::StandAloneCosmeticStatus(FControllerStatus state, float delta)
{
	FControllerStatus endState = state;
	FControllerStatus processState = CosmeticUpdateStatusParams(state, delta);
	processState = ProcessStatus(processState, delta);

	endState.CustomSolverCheckParameters = processState.CustomSolverCheckParameters;
	endState.StatusParams.StatusCosmeticVariables = processState.StatusParams.StatusCosmeticVariables;
	endState.Kinematics.SurfaceBinaryFlag = processState.Kinematics.SurfaceBinaryFlag;

	return endState;
}


FControllerStatus UModularControllerComponent::EvaluateStatusParams(const FControllerStatus initialStatus, const float delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("EvaluateStatusParams");
	//State
	FControllerStatus stateStatus = initialStatus;
	const int initialState = stateStatus.StatusParams.StateIndex;
	const auto stateControllerStatus = CheckControllerStates(initialStatus, delta);
	stateStatus.StatusParams.StateIndex = initialState;

	const auto swapCheckedStatus = TryChangeControllerState(stateControllerStatus, stateStatus);
	stateStatus = swapCheckedStatus.ProcessResult;

	//Actions
	FControllerStatus actionStatus = stateStatus;
	const int initialActionIndex = actionStatus.StatusParams.ActionIndex;
	const auto actionControllerStatus = CheckControllerActions(actionStatus, delta);
	actionStatus.StatusParams.ActionIndex = initialActionIndex;

	const auto actionCheckedStatus = TryChangeControllerAction(actionControllerStatus, actionStatus);
	actionStatus = actionCheckedStatus.ProcessResult;

	return actionStatus;
}

FControllerStatus UModularControllerComponent::CosmeticUpdateStatusParams(const FControllerStatus initialStatus, const float delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CosmeticUpdateStatusParams");
	//State
	const auto stateControllerStatus = CosmeticCheckState(initialStatus, delta);
	//Actions
	const auto actionControllerStatus = CosmeticCheckActions(stateControllerStatus, delta);

	return actionControllerStatus;
}


void UModularControllerComponent::ApplyStatusParams(const FControllerStatus status, const float delta)
{
	ChangeControllerState(status, delta);
	ChangeControllerAction(status, delta);
}


FControllerStatus UModularControllerComponent::ProcessStatus(const FControllerStatus initialState, const float inDelta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ProcessStatus");
	const FControllerStatus primaryMotion = ProcessControllerState(initialState, inDelta);
	const FControllerStatus alteredMotion = ProcessControllerAction(primaryMotion, inDelta);
	return alteredMotion;
}


#pragma endregion



#pragma region Action Montage XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


void UActionMontageEvent::Activate()
{
	if (!_controller)
	{
		FFrame::KismetExecutionMessage(TEXT("Invalid Modular Controller. Cannot execute Play Action Montage."), ELogVerbosity::Error);
		_OnActionMontageFailed();
		return;
	}

	if(!_controller->PlayActionMontage(MontageToPlay, Priority))
	{
		_OnActionMontageFailed();
		return;
	}

	_controller->OnActionMontageCompleted.AddDynamic(this, &UActionMontageEvent::_OnActionMontageCompleted);
}


UActionMontageEvent* UActionMontageEvent::PlayActionMontage(const UObject* WorldContextObject, UModularControllerComponent* controller, FActionMotionMontage Montage, int priority)
{
	UActionMontageEvent* Node = NewObject<UActionMontageEvent>();
	Node->WorldContextObject = WorldContextObject;
	Node->_controller = controller;
	Node->MontageToPlay = Montage;
	Node->Priority = priority;
	Node->RegisterWithGameInstance(controller);
	return Node;
}


void UActionMontageEvent::CleanUp()
{
	if (!_controller)
	{
		return;
	}
	_controller->OnActionMontageCompleted.RemoveDynamic(this, &UActionMontageEvent::_OnActionMontageCompleted);
}


void UActionMontageEvent::_OnActionMontageCompleted()
{
	OnActionMontageCompleted.Broadcast();
	CleanUp();
	SetReadyToDestroy();
}

void UActionMontageEvent::_OnActionMontageFailed()
{
	OnActionMontageFailed.Broadcast();
	CleanUp();
	SetReadyToDestroy();
}


#pragma endregion
