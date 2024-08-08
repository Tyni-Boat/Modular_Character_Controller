// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include <functional>
#include <Kismet/BlueprintAsyncActionBase.h>

#include "Animation/AnimMontage.h"
#include "CoreMinimal.h"
#include "CommonTypes.h"
#include "Containers/Queue.h"
#include "CollisionQueryParams.h"
#include "GameFramework/MovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/NavMovementComponent.h"

#ifndef BASE_WATCHER
#define BASE_WATCHER
#include "BaseTraversalWatcher.h"
#endif

#ifndef BASE_ACTION
#define BASE_ACTION
#include "BaseControllerAction.h"
#endif

#ifndef BASE_STATE
#define BASE_STATE
#include "BaseControllerState.h"
#endif

#include "ModularControllerComponent.generated.h"
#define MODULAR_CONTROLLER_COMPONENT

class UActionMontage;
class UBaseControllerState;
class UBaseControllerAction;
struct FCollisionQueryParams;


/// <summary>
/// Declare a multicast for when a State changed
/// </summary>
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FControllerStateChangedSignature, UModularControllerComponent,
                                                    OnControllerStateChangedEvent, UBaseControllerState*, NewState, UBaseControllerState*, OldState);

/// <summary>
/// Declare a multicast for when a action changed
/// </summary>
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FControllerActionChangedSignature, UModularControllerComponent,
                                                    OnControllerActionChangedEvent, UBaseControllerAction*, NewAction, UBaseControllerAction*, OldAction);

/// <summary>
/// Declare a multicast for when a action phase change
/// </summary>
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FControllerActionPhaseChangedSignature, UModularControllerComponent,
                                                    OnControllerActionPhaseChangedEvent, EActionPhase, NewPhase, EActionPhase, OldPhase);

/// <summary>
/// Declare a multicast for event with a path of point array
/// </summary>
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FControllerTriggerPathEventSignature, UModularControllerComponent,
                                                    OnControllerTriggerTraversalEvent, FName, LaunchID, TArray<FTransform>, Path);

/// <summary>
/// Declare a multicast for event with a unique name identifier
/// </summary>
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FControllerUniqueEventSignature, UModularControllerComponent,
                                                   OnControllerUniqueEvent, FName, LaunchID);


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActionMontageCompleted);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActionMontageBlendingOut);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActionMontageFailed);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActionMontageLauched);


//The inflation used when detecting main surfaces
#define OVERLAP_INFLATION 5


// Modular pawn controller, handle the logic to move the pawn based on any movement state.
UCLASS(ClassGroup = "Controllers",
	hidecategories = (Object, LOD, Lighting, TextureStreaming, Velocity, PlanarMovement, MovementComponent, Tags, Activation, Cooking, AssetUserData, Collision),
	meta = (DisplayName = "Modular Controller", BlueprintSpawnableComponent))
class MODULARCONTROLLER_API UModularControllerComponent : public UNavMovementComponent
{
	GENERATED_BODY()

#pragma region Core and Constructor

public:
	// Sets default values for this component's properties
	UModularControllerComponent();

	virtual void RegisterComponentTickFunctions(bool bRegister) override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FActorComponentTickFunction SecondaryComponentTick;

	FVector _lastLocation = FVector(0);

	FQuat _lastRotation = FQuat::Identity;

	float _mainTickChrono;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// Called to initialize the component
	void Initialize();

	// Called to Update the Movement logic
	void MovementTickComponent(float delta);

	// Called to Compute the next frame's Movement logic
	void ComputeTickComponent(float delta);

	//The performance profile in use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Core")
	FControllerPerformanceLOD CurrentPerformanceProfile;

	//Use this to offset rotation. useful when using skeletal mesh without as root primitive.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Core")
	FRotator RotationOffset = FRotator(0);

	//The component owner class, casted to pawn
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, category = "Controllers|Core")
	TSoftObjectPtr<APawn> OwnerPawn;

public:
	// Get the controller's actor custom Transform
	UFUNCTION(BlueprintGetter, Category = "Controllers|Core")
	FORCEINLINE FTransform GetTransform() const { return FTransform(GetRotation(), GetLocation(), GetScale()); }


	// Get the controller's actor position
	UFUNCTION(BlueprintGetter, Category = "Controllers|Core")
	FORCEINLINE FVector GetLocation() const { return UpdatedPrimitive ? UpdatedPrimitive->GetComponentLocation() : GetOwner()->GetActorLocation(); }


	// Get the controller's actor Rotation.
	UFUNCTION(BlueprintGetter, Category = "Controllers|Core")
	FORCEINLINE FQuat GetRotation() const { return (UpdatedPrimitive ? UpdatedPrimitive->GetComponentQuat() : GetOwner()->GetActorQuat()) * RotationOffset.Quaternion().Inverse(); }


	// Get the controller's actor Scale.
	UFUNCTION(BlueprintGetter, Category = "Controllers|Core")
	FORCEINLINE FVector GetScale() const { return GetOwner()->GetActorScale(); }


	// Get the component's actor Forward vector
	UFUNCTION(BlueprintCallable, Category = "Controllers|Core")
	FORCEINLINE FVector GetForwardVector() const { return GetRotation().GetForwardVector(); }


	// Get the component's actor Up vector
	UFUNCTION(BlueprintCallable, Category = "Controllers|Core")
	FORCEINLINE FVector GetUpVector() const { return GetRotation().GetUpVector(); }


	// Get the component's actor Right vector
	UFUNCTION(BlueprintCallable, Category = "Controllers|Core")
	FORCEINLINE FVector GetRightVector() const { return GetRotation().GetRightVector(); }


#pragma endregion


#pragma region Input Handling

private:
	//The user input pool, store and compute all user-made inputs
	UPROPERTY()
	UInputEntryPool* _inputPool;

	//The history of direction the user is willing to move.
	TArray<FVector_NetQuantize10> _userMoveDirectionHistory;

public:
	// Input a direction in wich to move the controller
	UFUNCTION(BlueprintCallable, Category = "Controllers|Inputs")
	void MovementInput(FVector movement);

	//Get the move vector from an input w/o root motion
	UFUNCTION(BlueprintCallable, Category = "Controllers|Inputs")
	virtual FVector GetMoveVector(const FVector inputVector, const float MaxSpeed, const float moveScale = 1, const ERootMotionType RootMotionType = ERootMotionType::NoRootMotion);

	// Lister to user input and Add input to the inputs pool
	void ListenInput(const FName key, const FInputEntry entry, const bool hold = false) const;

	// Lister to user input button and Add input to the inputs pool
	UFUNCTION(BlueprintCallable, Category = "Controllers|Inputs")
	void ListenButtonInput(const FName key, const float buttonBufferTime = 0, const bool hold = false);

	// Lister to user input value and Add input to the inputs pool
	UFUNCTION(BlueprintCallable, Category = "Controllers|Inputs")
	void ListenValueInput(const FName key, const float value);

	// Lister to user input axis and Add input to the inputs pool
	UFUNCTION(BlueprintCallable, Category = "Controllers|Inputs")
	void ListenAxisInput(const FName key, const FVector axis);


	// Consume the movement input. Movement input history will be consumed if it has 2 or more items.
	FVector ConsumeMovementInput();

	// Read an input from the pool 1 frame after it is registered
	UFUNCTION(BlueprintCallable, Category = "Controllers|Inputs")
	FInputEntry ReadInput(const FName key, bool consume = false);

	// Read a button from the pool 1 frame after it is registered
	UFUNCTION(BlueprintCallable, Category = "Controllers|Inputs")
	bool ReadButtonInput(const FName key, bool consume = false);

	// Read a value from the pool 1 frame after it is registered
	UFUNCTION(BlueprintCallable, Category = "Controllers|Inputs")
	float ReadValueInput(const FName key);

	// Read an axis from the pool 1 frame after it is registered
	UFUNCTION(BlueprintCallable, Category = "Controllers|Inputs")
	FVector ReadAxisInput(const FName key);


#pragma endregion


#pragma region Network Logic

#pragma region Common Logic

private:
	//The time elapsed since the object is active in scene.
	double _timeElapsed = 0;

	//The average network latency
	double _timeNetLatency = 0;

public:
	// Try to compensate this amount of the latency
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Network", meta=(ClampMin=0, ClampMax = 1, UIMin = 0, UIMax = 1))
	float LatencyCompensationScale = 0.1;

	// Evaluate cosmetic variables on dedicated server?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Network")
	bool bServerSideCosmetic = false;


	// Used to replicate some properties.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


	// Get the network local role
	UFUNCTION(BlueprintCallable, Category = "Controllers|Network")
	ENetRole GetNetRole(TSoftObjectPtr<APawn> pawn) const;


	// Get the network local role, as debug FName
	UFUNCTION(BlueprintCallable, Category = "Controllers|Network")
	FName GetNetRoleDebug(ENetRole role) const;


	// Get the last evaluated network latency in seconds
	UFUNCTION(BlueprintPure, Category = "Controllers|Network", meta = (CompactNodeTitle = "Net Latency"))
	double GetNetLatency() const;


protected:
	// Evaluate controller status and return the result.
	FControllerStatus StandAloneEvaluateStatus(FControllerStatus initialState, float delta, bool noCollision = false);

	// Apply controller status and return the changed diff.
	FControllerStatus StandAloneApplyStatus(FControllerStatus state, float delta);

	// Evaluate cosmetic value from a defined state 
	FControllerStatus StandAloneCosmeticStatus(FControllerStatus state, float delta);


	//Evaluate the status params of the controller
	FControllerStatus EvaluateStatusParams(const FControllerStatus initialStatus, const float delta);

	//Force the Update of status params based on a state and an action
	FControllerStatus CosmeticUpdateStatusParams(const FControllerStatus initialStatus, const float delta);

	// Apply status params on the controller.
	void ApplyStatusParams(const FControllerStatus status, const float delta);


	// Process velocity based on input status infos
	FControllerStatus ProcessStatus(const FControllerStatus initialState, const float inDelta);


#pragma endregion


#pragma region Server Logic
protected:
	TArray<TTuple<double, FControllerStatus>> _clientRequestReceptionQueue;

	double waitingClientCorrectionACK = -1;

public:
	/// Replicate server's time to clients
	UFUNCTION(NetMulticast, Unreliable, Category = "Controllers|Network|Server To CLient|RPC")
	void MultiCastTime(double timeStamp);

	/// Replicate server's user move to clients
	UFUNCTION(NetMulticast, Unreliable, Category = "Controllers|Network|Server To CLient|RPC")
	void MultiCastKinematics(FNetKinematic netKinematic);

	/// Replicate server's user Status params to clients
	UFUNCTION(NetMulticast, Unreliable, Category = "Controllers|Network|Server To CLient|RPC")
	void MultiCastStatusParams(FNetStatusParam netStatusParam);


	/// Replicate server's statesClasses to clients on request
	UFUNCTION(NetMulticast, Reliable, Category = "Controllers|Network|Server To CLient|RPC")
	void MultiCastStates(const TArray<TSubclassOf<UBaseControllerState>>& statesClasses, UModularControllerComponent* caller);

	/// Replicate server's actionsClasses to clients on request
	UFUNCTION(NetMulticast, Reliable, Category = "Controllers|Network|Server To CLient|RPC")
	void MultiCastActions(const TArray<TSubclassOf<UBaseControllerAction>>& actionsClasses, UModularControllerComponent* caller);


#pragma region Listened/StandAlone

protected:
	// Called to evaluate the next movement of the component
	void AuthorityComputeComponent(float delta, bool asServer = false);

	//Called to make the component moves
	void AuthorityMoveComponent(float delta);


#pragma endregion


#pragma region Dedicated


protected:
	// Called to Update the component logic in Dedicated Server Mode
	void DedicatedServerUpdateComponent(float delta);

#pragma endregion


#pragma endregion


#pragma region Client Logic

	/// Replicate client's user move to server
	UFUNCTION(Server, Unreliable, Category = "Controllers|Network|Cleint To Server|RPC")
	void ServerControllerStatus(double timeStamp, FNetKinematic netkinematic, FNetStatusParam netStatusParam);

	/// Replicate server's states to clients on request
	UFUNCTION(Server, Reliable, Category = "Controllers|Network|Client To Server|RPC")
	void ServerRequestStates(UModularControllerComponent* caller);

	/// Replicate server's actions to clients on request
	UFUNCTION(Server, Reliable, Category = "Controllers|Network|Client To Server|RPC")
	void ServerRequestActions(UModularControllerComponent* caller);

#pragma region Automonous Proxy

protected:
	// Called to Update the component logic in Autonomous Proxy Mode
	void AutonomousProxyUpdateComponent(float delta);

#pragma endregion


#pragma region Simulated Proxy

protected:
	FControllerStatus lastUpdatedControllerStatus;


	// Called to Update the component logic in Simulated Proxy Mode
	void SimulatedProxyComputeComponent(float delta);


#pragma endregion


#pragma endregion


#pragma endregion


#pragma region Physic
public:
	// The Mass of the object. use negative values to auto calculate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	float Mass = 80;

	// The Current Frag of the fluid / air. it's divided by the mass
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	float Drag = 0.564;

	// The Bounciness coefficient of this component on non-surface hits
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	float Bounciness = 0;

	// The number of point in fibonacci sphere, used as cardinal points on the surface.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	int CardinalPointsNumber = 64;

	// The scale of the force applyed to other controller upon collision
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	float InterCollisionForceScale = 0.5;

	// Disable collision with the world and other characters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	bool bDisableCollision = false;

	// The list of components to ignore when moving.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	TArray<UPrimitiveComponent*> IgnoredCollisionComponents;

	// Use complex collision for movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	bool bUseComplexCollision = false;

	// The Maximum sample in the history
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	int MaxHistorySamples = 10;

	// The History time interval
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	float HistoryTimeStep = 0.1;

	// The History Samples array. Youngest at index 0
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, category = "Controllers|Physic")
	TArray<FKinematicPredictionSample> MovementHistorySamples;


	// The cached array of component in contact
	TArray<FHitResultExpanded> _contactHits;

	//The cached array of hits during OverlapSolving
	TArray<FHitResultExpanded> _tempOverlapSolverHits;

	// The list of local space cardinal points;
	TArray<FVector> _localSpaceCardinalPoints;

	// The external forces applied to the controller
	FVector _externalForces;

	// The current collision shape datas.
	FCollisionShape _shapeDatas;

	// The history save chrono.
	float _historyCounterChrono;


	UFUNCTION(BlueprintPure, Category="Controllers|Physic")
	bool IsIgnoringCollision() const;


	// Get the controller Mass
	UFUNCTION(BlueprintPure, Category = "Controllers|Physic")
	FORCEINLINE float GetMass() const
	{
		return (Mass < 0 && UpdatedPrimitive != nullptr && UpdatedPrimitive->IsSimulatingPhysics()) ? UpdatedPrimitive->GetMass() : FMath::Clamp(Mass, 1, TNumericLimits<double>().Max());
	}


	//Add force on component. 
	UFUNCTION(BlueprintCallable, Category = "Controllers|Physic")
	void AddForce(const FVector force);


	// Evaluates the local points on the shape in each directions. Call it after modify the primitive shape.
	UFUNCTION(BlueprintCallable, Category = "Controllers|Physic")
	void EvaluateCardinalPoints();


	// Get the closest cardinal point on the shape matching the worldSpaceDirection. return NAN if an error happened.
	UFUNCTION(BlueprintPure, Category = "Controllers|Physic")
	FVector GetWorldSpaceCardinalPoint(const FVector worldSpaceDirection) const;


	// called When overlap occurs.
	UFUNCTION()
	void BeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);


	// track shape changes and update variables like cardinal points accordingly
	void TrackShapeChanges();


	//Solve overlap problems recursively.
	void OverlapSolver(TArray<FHitResultExpanded> &tempOverlapSolverHits, int& maxDepth, float DeltaTime, TArray<FHitResultExpanded>* touchedHits = nullptr, const FVector4 scanParameters = FVector4(0), FTransform* customTransform = nullptr,
	                   std::function<void(FVector)> OnLocationSet = {}) const;


	// Handle tracked surfaces.
	void HandleTrackedSurface(FControllerStatus& fromStatus, TArray<FHitResultExpanded> incomingCollisions, float delta) const;


	// Update the movement history
	void UpdateMovementHistory(FControllerStatus& status, const float delta);


#pragma endregion


#pragma region All Behaviours

protected:
	// The queue of override root motion commands
	FOverrideRootMotionCommand _overrideRootMotionCommand;

	// The queue of override root motion commands with no collision
	FOverrideRootMotionCommand _noCollisionOverrideRootMotionCommand;


public:
	// Trigger a path event.
	UPROPERTY(BlueprintAssignable, Category = "Controllers|Events")
	FControllerTriggerPathEventSignature OnControllerTriggerTraversalEvent;


	// Trigger a unique event.
	UPROPERTY(BlueprintAssignable, Category = "Controllers|Events")
	FControllerUniqueEventSignature OnControllerTriggerUniqueEvent;

	/// <summary>
	/// Set an override of motion.
	/// </summary>
	/// <returns></returns>
	UFUNCTION(BlueprintCallable, Category = "Controllers|Behaviours")
	void SetOverrideRootMotion(const FOverrideRootMotionCommand rootMotionParams, bool IgnoreCollision = false);


#pragma endregion


#pragma region States

public:
	// The State types used on this controller by default
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, category = "Controllers|Controller State")
	TArray<TSubclassOf<UBaseControllerState>> StateClasses;

	// The states instances used on this controller.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Controllers|Controller State")
	TArray<TSoftObjectPtr<UBaseControllerState>> StatesInstances;

	// The override map of linked anim blueprints per state name.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Controllers|Controller State")
	TMap<FName, TSubclassOf<UAnimInstance>> StatesOverrideAnimInstances;


	// The time spend on the current state
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Controllers|Controller State")
	double TimeOnCurrentState;

	// The state Behaviour changed event
	UPROPERTY(BlueprintAssignable, Category = "Controllers|Controller State|Events")
	FControllerStateChangedSignature OnControllerStateChangedEvent;

public:
	// Get the current state behaviour instance
	UFUNCTION(BlueprintPure, Category = "Controllers|Controller State", meta=(CompactNodeTitle="CurrentState"))
	UBaseControllerState* GetCurrentControllerState() const;


	/// Check if we have a state behaviour by type
	UFUNCTION(BlueprintPure, Category = "Controllers|Controller State")
	bool CheckControllerStateByType(TSubclassOf<UBaseControllerState> moduleType) const;


	/// Check if we have a state behaviour. by name
	UFUNCTION(BlueprintPure, Category = "Controllers|Controller State")
	bool CheckControllerStateByName(FName moduleName) const;

	/// Check if we have a state behaviour. by priority
	UFUNCTION(BlueprintPure, Category = "Controllers|Controller State")
	bool CheckControllerStateByPriority(int modulePriority) const;

	// Sort states array.
	void SortStates();

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Add a state behaviour
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|Controller State")
	void AddControllerState(TSubclassOf<UBaseControllerState> moduleType);

	/// Add a state behaviour
	void AddControllerState_Implementation(TSubclassOf<UBaseControllerState> moduleType);

	/// Get reference to a state by type
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller State")
	UBaseControllerState* GetControllerStateByType(TSubclassOf<UBaseControllerState> moduleType);

	/// Get reference to a state by name
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller State")
	UBaseControllerState* GetControllerStateByName(FName moduleName);


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Remove a state behaviour by type
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|Controller State")
	void RemoveControllerStateByType(TSubclassOf<UBaseControllerState> moduleType);

	/// Remove a state behaviour by type
	void RemoveControllerStateByType_Implementation(TSubclassOf<UBaseControllerState> moduleType);

	/// Remove a state behaviour by name
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|Controller State")
	void RemoveControllerStateByName(FName moduleName);

	/// Remove a state behaviour by name
	void RemoveControllerStateByName_Implementation(FName moduleName);


	/// Remove a state behaviour by priority
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|Controller State")
	void RemoveControllerStateByPriority(int modulePriority);

	/// Remove a state behaviour by priority
	void RemoveControllerStateByPriority_Implementation(int modulePriority);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// When the controller change a state, it call this function
	UFUNCTION(BlueprintNativeEvent, Category = "Controllers|Controller State|Events")
	void OnControllerStateChanged(UBaseControllerState* newState, UBaseControllerState* oldState);

protected:
	//Check state
	FControllerStatus CheckControllerStates(FControllerStatus currentControllerStatus, const float inDelta);

	//Execute check states juste to arouse cosmetic variables
	FControllerStatus CosmeticCheckState(FControllerStatus currentControllerStatus, const float inDelta);

	/// try Change from state 1 to 2
	FControllerCheckResult TryChangeControllerState(FControllerStatus ToStateStatus, FControllerStatus fromStateStatus) const;

	//Change state to state index
	void ChangeControllerState(FControllerStatus ToStateStatus, const float inDelta);

	/// Evaluate the component state
	FControllerStatus ProcessControllerState(const FControllerStatus initialState, const float inDelta);


	/// When the controller change a behaviour, it call this function
	virtual void OnControllerStateChanged_Implementation(UBaseControllerState* newState, UBaseControllerState* oldState);


#pragma endregion


#pragma region Actions

public:
	/// The actions types used on this controller.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, category = "Controllers|Controller Action")
	TArray<TSubclassOf<UBaseControllerAction>> ActionClasses;

	/// The actions montage library map, to play montage on action start.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Controller Action")
	TMap<FName, FActionMontageLibrary> ActionMontageLibraryMap;

	/// The actions instances used on this controller.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Controllers|Controller Action")
	TArray<TSoftObjectPtr<UBaseControllerAction>> ActionInstances;

	/// The action Montage instance used on this controller.
	UPROPERTY()
	UActionMontage* ActionMontageInstance;

	/// The HashMap of Controller action infos
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Controllers|Controller Action")
	TMap<TSoftObjectPtr<UBaseControllerAction>, FActionInfos> ActionInfos;

	// The action changed event
	UPROPERTY(BlueprintAssignable, Category = "Controllers|Controller Action|Events")
	FControllerActionChangedSignature OnControllerActionChangedEvent;

	// The action phase changed event
	UPROPERTY(BlueprintAssignable, Category = "Controllers|Controller Action|Events")
	FControllerActionPhaseChangedSignature OnControllerActionPhaseChangedEvent;

	// Triggers when The action montage complete, interrupted or not.
	UPROPERTY(BlueprintAssignable, Category = "Controllers|Controller Action|Events")
	FOnActionMontageCompleted OnActionMontageCompleted;

	// Triggers when The action montage starting blending out.
	UPROPERTY(BlueprintAssignable, Category = "Controllers|Controller Action|Events")
	FOnActionMontageBlendingOut OnActionMontageBlendingOut;

public:
	/// Get the current action behaviour instance
	UFUNCTION(BlueprintPure, Category = "Controllers|Controller Action", meta = (CompactNodeTitle = "CurrentAction"))
	UBaseControllerAction* GetCurrentControllerAction() const;

	/// Get the current action Infos
	UFUNCTION(BlueprintPure, Category = "Controllers|Controller Action", meta = (CompactNodeTitle = "CurrentActionInfos"))
	FActionInfos GetCurrentControllerActionInfos() const;

	/// Return true if the current action is the action montage
	UFUNCTION(BlueprintPure, Category = "Controllers|Controller Action", meta = (CompactNodeTitle = "IsActionMontage"))
	bool IsActionMontagePlaying() const;

	/// Check if we have an action behaviour by type
	UFUNCTION(BlueprintPure, Category = "Controllers|Controller Action")
	bool CheckActionBehaviourByType(TSubclassOf<UBaseControllerAction> moduleType) const;

	/// Check if we have an action behaviour by name
	UFUNCTION(BlueprintPure, Category = "Controllers|Controller Action")
	bool CheckActionBehaviourByName(FName moduleName) const;

	/// Check if we have an action behaviour by priority
	UFUNCTION(BlueprintPure, Category = "Controllers|Controller Action")
	bool CheckActionBehaviourByPriority(int modulePriority) const;

	// Sort Actions instances Array
	void SortActions();


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// Add an action behaviour
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|Controller Action")
	void AddControllerAction(TSubclassOf<UBaseControllerAction> moduleType);


	/// Add an action behaviour
	void AddControllerAction_Implementation(TSubclassOf<UBaseControllerAction> moduleType);

	/// Get an action behaviour by type
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller Action")
	UBaseControllerAction* GetActionByType(TSubclassOf<UBaseControllerAction> moduleType);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// Remove an action behaviour by type
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|Controller Action")
	void RemoveActionBehaviourByType(TSubclassOf<UBaseControllerAction> moduleType);

	/// Remove an action behaviour by type
	void RemoveActionBehaviourByType_Implementation(TSubclassOf<UBaseControllerAction> moduleType);

	/// Remove an action behaviour by name
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|Controller Action")
	void RemoveActionBehaviourByName(FName moduleName);

	/// Remove an action behaviour by name
	void RemoveActionBehaviourByName_Implementation(FName moduleName);


	/// Remove an action behaviour by priority
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|Controller Action")
	void RemoveActionBehaviourByPriority(int modulePriority);

	/// Remove an action behaviour by priority
	void RemoveActionBehaviourByPriority_Implementation(int modulePriority);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Triggered when the active action is changed.
	UFUNCTION(BlueprintNativeEvent, Category = "Controllers|Controllers|Controller Action|Events")
	void OnControllerActionChanged(UBaseControllerAction* newAction, UBaseControllerAction* lastAction);

	/**
	 * @brief Check if an action is compatible with this state and those actions
	 * @param actionInstance The action to verify
	 * @param stateIndex the controller state index used
	 * @param actionIndex the action array used
	 * @return true if it's compatible
	 */
	bool CheckActionCompatibility(const TSoftObjectPtr<UBaseControllerAction> actionInstance, int stateIndex, int actionIndex) const;

	// Play the action montage with the specified priority.
	bool PlayActionMontage(FActionMotionMontage Montage, int priority = 0);

	/// Get an action's current motion montage
	UFUNCTION(BlueprintGetter, Category = "Controllers|Controller Action|Utils")
	FActionMotionMontage GetActionCurrentMotionMontage(const UBaseControllerAction* actionInst) const;


protected:
	/// Check controller Actions and returns the index of the active one.
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller Action|Events")
	FControllerStatus CheckControllerActions(FControllerStatus currentControllerStatus, const float inDelta);

	//Execute check actions juste to arouse cosmetic variables
	FControllerStatus CosmeticCheckActions(FControllerStatus currentControllerStatus, const float inDelta);

	/// Try Change actions from action index 1 to 2
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller Action|Events")
	FControllerCheckResult TryChangeControllerAction(FControllerStatus toActionStatus, FControllerStatus fromActionStatus);

	/// Change actions from action index 1 to 2
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller Action|Events")
	void ChangeControllerAction(FControllerStatus toActionStatus, const float inDelta);

	/// Evaluate the component state
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller Action|Events")
	FControllerStatus ProcessControllerAction(const FControllerStatus initialState, const float inDelta);

	// Process single action's velocity.
	FControllerStatus ProcessSingleAction(TSoftObjectPtr<UBaseControllerAction> actionInstance, const FControllerStatus initialState, const float inDelta);


#pragma endregion

#pragma region Watchers

public:
	/// The watchers types used on this controller.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, category = "Controllers|Controller Watcher")
	TArray<TSubclassOf<UBaseTraversalWatcher>> WatcherClasses;

	/// The watcher instances used on this controller.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Controllers|Controller Watcher")
	TArray<TSoftObjectPtr<UBaseTraversalWatcher>> WatcherInstances;

public:
	/// Check if we have a watcher by type
	UFUNCTION(BlueprintPure, Category = "Controllers|Controller Watcher")
	bool CheckTraversalWatcherByType(TSubclassOf<UBaseTraversalWatcher> moduleType) const;

	/// Check if we have a watcher by name
	UFUNCTION(BlueprintPure, Category = "Controllers|Controller Watcher")
	bool CheckTraversalWatcherByName(FName moduleName) const;

	/// Check if we have a Watcher by priority
	UFUNCTION(BlueprintPure, Category = "Controllers|Controller Watcher")
	bool CheckTraversalWatcherByPriority(int modulePriority) const;

	// Sort Watcher instances Array
	void SortTraversalWatchers();


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// Add a traversal watcher
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|Controller Watcher")
	void AddTraversalWatcher(TSubclassOf<UBaseTraversalWatcher> moduleType);


	/// Get an traversal watcher by type
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller Watcher")
	UBaseTraversalWatcher* GetTraversalWatcherByType(TSubclassOf<UBaseTraversalWatcher> moduleType);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// Remove a traversal watcher by type
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|Controller Watcher")
	void RemoveTraversalWatcherByType(TSubclassOf<UBaseTraversalWatcher> moduleType);

	/// Remove a traversal watcher by name
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|Controller Watcher")
	void RemoveTraversalWatcherByName(FName moduleName);


	/// Remove a traversal watcher by priority
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|Controller Watcher")
	void RemoveTraversalWatcherByPriority(int modulePriority);

protected:
	// The tick chrono for watchers
	float _watcherTickChrono = 0;

	/// Check controller Watchers.
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller Watcher|Events")
	void CheckControllerTraversalWatcher(FControllerStatus currentControllerStatus, const float inDelta);


	/**
	 * @brief Check if a traversal watcher is compatible with this state and those actions
	 * @param watcherInstance The action to verify
	 * @param stateIndex the controller state index used
	 * @param actionIndex the action array used
	 * @return true if it's compatible
	 */
	bool CheckTraversalWatcherCompatibility(const TSoftObjectPtr<UBaseTraversalWatcher> watcherInstance, int stateIndex, int actionIndex) const;


#pragma endregion


#pragma region Animation Component
public:
	/// The Skinned mesh component reference. used to play montages and switch anim linked instances based on current state.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Controllers|Animation Component", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SkeletalMeshComponent"))
	FComponentReference MainSkeletal;


	/// The Root motion scale. some times the root motion does not match the actual movement and movement. this scale the movement ot match the animation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Animation Component")
	float RootMotionScale = 1;


	/// Get Root motion vector. the motion is not consumed after this
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	FVector GetRootMotionVector() const;

	/// Get Root motion rotation. the motion is not consumed after this
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	FQuat GetRootMotionQuat() const;


	/// Get the controller's Skeletal Mesh
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	TSoftObjectPtr<USkeletalMeshComponent> GetSkeletalMesh();

	///Play an animation montage on the controller globally. returns the duration
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	double PlayAnimationMontage(FActionMotionMontage Montage, float customAnimStartTime = -1);


	// Stop a playing montage
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	void StopMontage(FActionMotionMontage montage, bool isPlayingOnState = false);

	///Play an animation montage on the specified controller state linked anim graph. returns the duration
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	double PlayAnimationMontageOnState(FActionMotionMontage Montage, FName stateName, float customAnimStartTime = -1);

	//Get the base anim  instance or the specific anim instance of a state
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	UAnimInstance* GetAnimInstance(FName stateName = NAME_None);

	//Try get the motion warp transform at key
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	void AddOrUpdateMotionWarp(FName warpKey, const FTransform inTransform);

	//Remove Warp transform
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	void RemoveMotionWarp(FName warpKey);

	//Try get the motion warp transform at key
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	bool TryGetMotionWarpTransform(FName warpKey, FTransform& outTransform);

	///Play an animation montage on the controller globally. returns the duration
	double PlayAnimationMontage_Internal(FActionMotionMontage Montage, float customAnimStartTime = -1
	                                     , bool useMontageEndCallback = false, FOnMontageEnded endCallBack = {}, FOnMontageBlendingOutStarted blendOutCallBack = {});

	///Play an animation montage on the specified controller state linked anim graph. returns the duration
	double PlayAnimationMontageOnState_Internal(FActionMotionMontage Montage, FName stateName, float customAnimStartTime = -1
	                                            , bool useMontageEndCallback = false, FOnMontageEnded endCallBack = {}, FOnMontageBlendingOutStarted blendOutCallBack = {});


	//Read the skeletal mesh root motion.
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	void ReadRootMotion(FKinematicComponents& kinematics, const FVector fallbackVelocity, const ERootMotionType rootMotionMode, float surfaceFriction = 1, float weight = 1) const;

	//Read the root motion translation vector
	FVector GetRootMotionTranslation(const ERootMotionType rootMotionMode, const FVector currentVelocity) const;

private:
	// The root motion transform.
	FTransform _RootMotionParams;

	// The linked anim Map
	TMap<TSoftObjectPtr<USkeletalMeshComponent>, TMap<FName, TWeakObjectPtr<UAnimInstance>>> _linkedAnimClasses;

	//the cached skeletal mesh reference
	TSoftObjectPtr<USkeletalMeshComponent> _skeletalMesh;

	// Motion warp transform registered
	TMap<FName, FTransform> _motionWarpTransforms;

	// The call back on action's montage end
	FOnMontageEnded _onActionMontageEndedCallBack;

	// The call back on action's montage end
	FOnMontageBlendingOutStarted _OnActionMontageBlendingOutStartedCallBack;

	// Link a montage to one or several actions. used to stop action when montage interrupts
	TMap<TSoftObjectPtr<UAnimMontage>, TArray<TSoftObjectPtr<UBaseControllerAction>>> _montageOnActionBound;

protected:
	/// Link anim blueprint on a skeletal mesh, with a key. the use of different key result in the link of several anim blueprints.
	virtual void LinkAnimBlueprint(TSoftObjectPtr<USkeletalMeshComponent> skeletalMeshReference, FName key, TSubclassOf<UAnimInstance> animClass);


	// Play a montage on an animation instance and return the duration
	double PlayAnimMontageSingle(UAnimInstance* animInstance, FActionMotionMontage montage, float customAnimStartTime = -1
	                             , bool useMontageEndCallback = false, FOnMontageEnded endCallBack = FOnMontageEnded(),
	                             FOnMontageBlendingOutStarted blendOutCallBack = FOnMontageBlendingOutStarted());


	// Called when an action's montage ends.
	UFUNCTION()
	void OnActionMontageEnds(UAnimMontage* Montage, bool Interrupted);

	// Called when an action's montage starting to blend out.
	UFUNCTION()
	void OnActionMontageBlendOutStarted(UAnimMontage* Montage, bool Interrupted);


	/// Evaluate component's from it's skeletal mesh Root motions
	void ExtractRootMotions(float delta);


	/// Evaluate root motion override parameters
	FControllerStatus EvaluateRootMotionOverride(const FControllerStatus inStatus, float inDelta, bool& ignoredCollision);


#pragma endregion


#pragma region Movement

public:
	/// The kinematic components to be apply on the next frame.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, category = "Controllers|Movement")
	FControllerStatus ComputedControllerStatus;

	/// The kinematic components that been apply this frame.
	UPROPERTY(BlueprintReadOnly, category = "Controllers|Movement")
	FControllerStatus ApplyedControllerStatus;


	/// Move the owner in a direction. return the displacement actually made. it can be different from the input movement if collision occured.
	UFUNCTION(BlueprintNativeEvent, Category = "Controllers|Movement|Events")
	void Move(const FKinematicComponents finalKinematic, float deltaTime);

protected:
	/// Move the owner in a direction. return the displacement actually made. it can be different from the input movement if collision occured.
	virtual void Move_Implementation(const FKinematicComponents finalKinematic, float deltaTime);


	/// Evaluate the movement and return the new kinematic component.
	virtual FKinematicComponents KinematicMoveEvaluation(FControllerStatus processedMove, bool noCollision, float delta);


	/// Handle premove logic. Always call at the top of the Update.
	FControllerStatus ConsumeLastKinematicMove(FVector moveInput, float delta) const;


	/// Operation to Conserve momentum.
	void KinematicPostMove(FControllerStatus newStatus, const float inDelta);


	/// Handle the controller rotation to always stay Up aligned with gravity
	FAngularKinematicCondition HandleKinematicRotation(const FKinematicComponents inKinematic, const float inDelta) const;


	/// Simulate A slide along a surface at a position with a rotation. Returns the position after slide.
	FVector SlideAlongSurfaceAt(FHitResult& Hit, const FQuat rotation, FVector attemptedMove, int& depth, double deltaTime, float hullInflation = 0.100);


#pragma endregion


#pragma region Tools & Utils

public:
	/// Show the debugs traces and logs
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Debug")
	EControllerDebugType DebugType;


	//Tracecast a number of times until the condition is meet
	bool ComponentTraceSingleUntil(FHitResult& outHit, FVector direction, FVector position, FQuat rotation, std::function<bool(FHitResult)> condition, int iterations = 3,
	                               double inflation = 0.100, bool traceComplex = false) const;


	/// Check for all collisions at a position and rotation in a direction as overlaps. return true if any collision occurs
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	FORCEINLINE bool ComponentTraceMulti(TArray<FHitResultExpanded>& outHits, FVector position, FVector direction, FQuat rotation, double inflation = 0.100, bool traceComplex = false)
	{
		return ComponentTraceMulti_internal(outHits, position, direction, rotation, inflation, traceComplex, FCollisionQueryParams::DefaultQueryParam);
	}

	bool ComponentTraceMulti_internal(TArray<FHitResultExpanded>& outHits, FVector position, FVector direction, FQuat rotation, double inflation = 0.100, bool traceComplex = false,
	                                  FCollisionQueryParams& queryParams = FCollisionQueryParams::DefaultQueryParam,
	                                  double counterDirectionMaxOffset = TNumericLimits<float>::Max()) const;


	/// Check for collision at a position and rotation in a direction. return true if collision occurs
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	FORCEINLINE bool ComponentTraceSingle(FHitResult& outHit, FVector position, FVector direction, FQuat rotation, double inflation = 0.100, bool traceComplex = false) const
	{
		return ComponentTraceSingle_Internal(outHit, position, direction, rotation, inflation, traceComplex, FCollisionQueryParams::DefaultQueryParam);
	}

	bool ComponentTraceSingle_Internal(FHitResult& outHit, FVector position, FVector direction, FQuat rotation, double inflation = 0.100, bool traceComplex = false,
	                                   FCollisionQueryParams& queryParams = FCollisionQueryParams::DefaultQueryParam) const;


	// Trace single along a path
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	FORCEINLINE bool ComponentPathTraceSingle(FHitResult& result, int& pathPtIndex, TArray<FTransform> pathPoints, float inflation = 0, bool traceComplex = false) const
	{
		return ComponentPathTraceSingle_Internal(result, pathPtIndex, pathPoints, inflation, traceComplex, FCollisionQueryParams::DefaultQueryParam);
	}

	bool ComponentPathTraceSingle_Internal(FHitResult& result, int& pathPtIndex, TArray<FTransform> pathPoints, float inflation = 0, bool traceComplex = false,
	                                       FCollisionQueryParams& queryParams = FCollisionQueryParams::DefaultQueryParam) const;


	// Trace Multi along a path
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	FORCEINLINE bool ComponentPathTraceMulti(TArray<FHitResultExpanded>& results, TArray<int>& pathPtIndexes, TArray<FTransform> pathPoints, float inflation = 0, bool traceComplex = false)
	{
		return ComponentPathTraceMulti_Internal(results, pathPtIndexes, pathPoints, inflation, traceComplex, FCollisionQueryParams::DefaultQueryParam);
	}

	bool ComponentPathTraceMulti_Internal(TArray<FHitResultExpanded>& results, TArray<int>& pathPtIndexes, TArray<FTransform> pathPoints, float inflation = 0, bool traceComplex = false,
	                                      FCollisionQueryParams& queryParams = FCollisionQueryParams::DefaultQueryParam) const;


	/// Check for Overlap at a atPosition and rotation and return the separationForce needed for depenetration. work best with simple collisions
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	bool CheckPenetrationAt(FVector& separationForce, FVector& contactForce, FVector atPosition, FQuat withOrientation, UPrimitiveComponent* onlyThisComponent = nullptr,
	                        double hullInflation = 0.125, bool getVelocity = false) const;


	/// Return a point on the surface of the collider.
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils", meta=(AdvancedDisplay="2"))
	FVector PointOnShape(FVector direction, const FVector inLocation, const float hullInflation = 0.126) const;


	// Evaluate all conditions of a surface against this controller
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils", meta=(AdvancedDisplay="3"))
	bool EvaluateSurfaceConditions(FSurfaceCheckParams conditions, FSurfaceCheckResponse& response, FControllerStatus inStatus, FVector locationOffset = FVector(0),
	                               FVector orientationOffset = FVector(0), FVector solverChkParam = FVector(0), FVector customDirection = FVector(0));


	// Evaluate all conditions of a surface against this controller
	bool EvaluateSurfaceConditionsInternal(TArray<FHitResultExpanded>& tmpSolverHits, FSurfaceCheckParams conditions, FSurfaceCheckResponse& response, FControllerStatus inStatus, FVector locationOffset = FVector(0),
	                                       FVector orientationOffset = FVector(0), FVector solverChkParam = FVector(0), FVector customDirection = FVector(0),
	                                       TArray<bool>* checkDones = nullptr) const;


#pragma endregion


#pragma region Debug

public:
#pragma endregion
};


UCLASS(BlueprintType)
class MODULARCONTROLLER_API UActionMontageEvent : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FOnActionMontageLauched OnActionMontageLaunched;
	
	UPROPERTY(BlueprintAssignable)
	FOnActionMontageCompleted OnActionMontageCompleted;
	
	UPROPERTY(BlueprintAssignable)
	FOnActionMontageBlendingOut OnActionMontageBlendingOut;

	UPROPERTY(BlueprintAssignable)
	FOnActionMontageFailed OnActionMontageFailed;

	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "AI|Modular AI Movement")
	static UActionMontageEvent* PlayActionMontage(const UObject* WorldContextObject, UModularControllerComponent* controller, FActionMotionMontage Montage, int priority = 0);


private:
	UFUNCTION()
	void _OnActionMontageCompleted();
	
	UFUNCTION()
	void _OnActionMontageBlendOutStart();

	UFUNCTION()
	void _OnActionMontageFailed();
	
	UFUNCTION()
	void _OnActionMontageLaunched();

	void CleanUp();

	UPROPERTY()
	FActionMotionMontage MontageToPlay;

	int Priority = -1;

	const UObject* WorldContextObject;

	UPROPERTY()
	UModularControllerComponent* _controller;
};
