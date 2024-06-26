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
			if (!StateClasses[i].IsValid())
				continue;
			StatesInstances.Add(StateClasses[i]->GetDefaultObject());
		}
		SortStates();
	}

	//Action behaviors
	ActionInstances.Empty();
	{
		for (int i = ActionClasses.Num() - 1; i >= 0; i--)
		{
			if (!ActionClasses[i].IsValid())
				continue;
			ActionInstances.Add(ActionClasses[i]->GetDefaultObject());
		}
		SortActions();
	}

	//Init last move
	_lastLocation = GetLocation();
	ComputedControllerStatus.Kinematics.LinearKinematic.Position = GetLocation();
	ApplyedControllerStatus.Kinematics.LinearKinematic.Position = GetLocation();
	ComputedControllerStatus.Kinematics.AngularKinematic.Orientation = UpdatedPrimitive->GetComponentQuat();
	ApplyedControllerStatus.Kinematics.AngularKinematic.Orientation = UpdatedPrimitive->GetComponentQuat();

	//Set time elapsed
	const auto timePassedSince = FDateTime::UtcNow() - FDateTime(2024, 06, 01, 0, 0, 0, 0);
	_timeElapsed = timePassedSince.GetTotalSeconds();
}


// Called every frame
void UModularControllerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
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
	}
}


void UModularControllerComponent::MovementTickComponent(float delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MovementTickComponent");

	//Update Inputs
	if (_inputPool.IsValid())
		_inputPool->UpdateInputs(delta, DebugType == ControllerDebugType_InputDebug, this);

	//Update Action infos
	for (auto& infos : ActionInfos)
	{
		if (!infos.Key.IsValid())
			continue;
		auto phase = infos.Value.CurrentPhase;
		ActionInfos[infos.Key].Update(delta);
		if (ActionInfos[infos.Key].CurrentPhase != phase)
		{
			OnControllerActionPhaseChangedEvent.Broadcast(ActionInfos[infos.Key].CurrentPhase, phase);
		}
	}

	//In StandAlone Mode, don't bother with net logic at all
	if (GetNetMode() == NM_Standalone)
	{
		AuthorityMoveComponent(delta);
		_lastLocation = FVector(0);
		return;
	}
}


void UModularControllerComponent::ComputeTickComponent(float delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ComputeTickComponent");
	//if (!OwnerPawn.IsValid())
	//	return;

	//Extract Root motion
	EvaluateRootMotions(delta);

	//Count time elapsed
	_timeElapsed += delta;

	//Solve collisions
	int maxDepth = 64;
	OverlapSolver(maxDepth, delta, &_contactComponents);

	//Handle tracked surfaces
	HandleTrackedSurface(delta);

	//In StandAlone Mode, don't bother with net logic at all
	if (GetNetMode() == NM_Standalone)
	{
		//AuthorityComputeComponent(delta);

		const FVector moveInp = ConsumeMovementInput();
		const FControllerStatus initialState = ConsumeLastKinematicMove(moveInp);
		ComputedControllerStatus = initialState;
		//auto status = StandAloneEvaluateStatus(initialState, delta);
		//ComputedControllerStatus = status;

		_lastLocation = GetLocation();
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
	}
}


FControllerStatus UModularControllerComponent::StandAloneEvaluateStatus(FControllerStatus initialState, float delta, bool noCollision, bool physicImpact)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("StandAloneEvaluateStatus");
	FControllerStatus processState = initialState;
	processState = EvaluateStatusParams(initialState, delta);
	processState = ProcessStatus(processState, delta);
	processState = EvaluateRootMotionOverride(processState, delta);
	processState.Kinematics.AngularKinematic = HandleKinematicRotation(processState.Kinematics.AngularKinematic, delta);

	//Evaluate
	FKinematicComponents finalKcomp = KinematicMoveEvaluation(processState, noCollision || bDisableCollision, delta, physicImpact);
	processState.Kinematics = finalKcomp;
	return processState;
}


FControllerStatus UModularControllerComponent::StandAloneApplyStatus(FControllerStatus state, float delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("StandAloneApplyStatus");
	ApplyStatusParams(state, delta);
	Move(state.Kinematics, delta);
	state.ComputeDiffManifest(ApplyedControllerStatus);
	KinematicPostMove(state, delta);
	return state;
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
	//stateStatus.ControllerSurface = swapCheckedStatus.ProcessResult.ControllerSurface;
	UFunctionLibrary::DrawDebugCircleOnSurface(stateControllerStatus.ControllerSurface.GetHitResult(), true, 100);

	//Actions
	FControllerStatus actionStatus = stateStatus;
	const int initialActionIndex = actionStatus.StatusParams.ActionIndex;
	const auto actionControllerStatus = CheckControllerActions(actionStatus, delta);
	actionStatus.StatusParams.ActionIndex = initialActionIndex;

	const auto actionCheckedStatus = TryChangeControllerAction(actionControllerStatus, actionStatus);
	actionStatus = actionCheckedStatus.ProcessResult;

	return actionStatus;
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

