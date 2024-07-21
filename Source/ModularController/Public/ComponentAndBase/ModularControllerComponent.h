// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include <functional>

#include "Animation/AnimMontage.h"
#include "CoreMinimal.h"
#include "CommonTypes.h"
#include "Containers/Queue.h"
#include "CollisionQueryParams.h"
#include "GameFramework/MovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/NavMovementComponent.h"

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

class UBaseControllerState;
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
                                                    OnControllerTriggerPathEvent, FName, LaunchID, TArray<FTransform>, Path);

/// <summary>
/// Declare a multicast for event with a unique name identifier
/// </summary>
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FControllerUniqueEventSignature, UModularControllerComponent,
                                                   OnControllerUniqueEvent, FName, LaunchID);

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

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// Called to initialize the component
	void Initialize();

	// Called to Update the Movement logic
	void MovementTickComponent(float delta);

	// Called to Compute the next frame's Movement logic
	void ComputeTickComponent(float delta);

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


	// The component's current gravity vector
	FVector _gravityVector = -FVector::UpVector;

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


	UFUNCTION(BlueprintPure, Category="Controllers|Physic")
	bool IsIgnoringCollision() const;


	// Get the controller Gravity
	UFUNCTION(BlueprintPure, Category = "Controllers|Physic")
	FORCEINLINE FVector GetGravity() const
	{
		return _gravityVector;
	}


	// Get the controller Gravity
	UFUNCTION(BlueprintCallable, Category = "Controllers|Physic")
	FORCEINLINE void SetGravity(FVector gravity)
	{
		_gravityVector = gravity;
	}


	// Get the controller Gravity's Direction
	UFUNCTION(BlueprintPure, Category = "Controllers|Physic")
	FORCEINLINE FVector GetGravityDirection() const
	{
		return _gravityVector.GetSafeNormal();
	}


	// Get the controller Gravity's Scale
	UFUNCTION(BlueprintPure, Category = "Controllers|Physic")
	FORCEINLINE float GetGravityScale() const
	{
		return _gravityVector.Length();
	}


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
	void OverlapSolver(int& maxDepth, float DeltaTime, TArray<FHitResultExpanded>* touchedHits = nullptr, const FVector4 scanParameters = FVector4(0));


	// Handle tracked surfaces.
	void HandleTrackedSurface(FControllerStatus& fromStatus, float delta);


#pragma endregion


#pragma region All Behaviours

protected:
	// The queue of override root motion commands
	TQueue<FOverrideRootMotionCommand> _overrideRootMotionCommands;

	// The queue of override root motion commands with no collision
	TQueue<FOverrideRootMotionCommand> _noCollisionOverrideRootMotionCommands;


public:
	// Trigger a path event.
	UPROPERTY(BlueprintAssignable, Category = "Controllers|Events")
	FControllerTriggerPathEventSignature OnControllerTriggerPathEvent;


	// Trigger a unique event.
	UPROPERTY(BlueprintAssignable, Category = "Controllers|Events")
	FControllerUniqueEventSignature OnControllerTriggerUniqueEvent;


	/// <summary>
	/// Set an override of motion.
	/// </summary>
	/// <param name="translationMode">Override in translation Mode</param>
	/// <param name="rotationMode">Override in Rotation Mode</param>
	/// <returns></returns>
	UFUNCTION(BlueprintCallable, Category = "Controllers|Behaviours")
	void SetOverrideRootMotionMode(USkeletalMeshComponent* caller, const ERootMotionType translationMode, const ERootMotionType rotationMode);

	/// <summary>
	/// Set an override of motion.
	/// </summary>
	/// <returns></returns>
	UFUNCTION(BlueprintCallable, Category = "Controllers|Behaviours")
	void SetOverrideRootMotion(USkeletalMeshComponent* caller, const FOverrideRootMotionCommand rootMotionParams);


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

	/// The HashMap of Controller action infos
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Controllers|Controller Action")
	TMap<TSoftObjectPtr<UBaseControllerAction>, FActionInfos> ActionInfos;

	// The action changed event
	UPROPERTY(BlueprintAssignable, Category = "Controllers|Controller Action|Events")
	FControllerActionChangedSignature OnControllerActionChangedEvent;

	// The action phase changed event
	UPROPERTY(BlueprintAssignable, Category = "Controllers|Controller Action|Events")
	FControllerActionPhaseChangedSignature OnControllerActionPhaseChangedEvent;


public:
	/// Get the current action behaviour instance
	UFUNCTION(BlueprintPure, Category = "Controllers|Controller Action", meta = (CompactNodeTitle = "CurrentAction"))
	UBaseControllerAction* GetCurrentControllerAction() const;

	/// Get the current action Infos
	UFUNCTION(BlueprintPure, Category = "Controllers|Controller Action", meta = (CompactNodeTitle = "CurrentActionInfos"))
	FActionInfos GetCurrentControllerActionInfos() const;


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
	                                     , bool useMontageEndCallback = false, FOnMontageEnded endCallBack = {});

	///Play an animation montage on the specified controller state linked anim graph. returns the duration
	double PlayAnimationMontageOnState_Internal(FActionMotionMontage Montage, FName stateName, float customAnimStartTime = -1
	                                            , bool useMontageEndCallback = false, FOnMontageEnded endCallBack = {});


	//Read the skeletal mesh root motion.
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	void ReadRootMotion(FKinematicComponents& kinematics, const FVector fallbackVelocity, const ERootMotionType rootMotionMode, float surfaceFriction = 1) const;

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

protected:
	/// Link anim blueprint on a skeletal mesh, with a key. the use of different key result in the link of several anim blueprints.
	virtual void LinkAnimBlueprint(TSoftObjectPtr<USkeletalMeshComponent> skeletalMeshReference, FName key, TSubclassOf<UAnimInstance> animClass);


	// Play a montage on an animation instance and return the duration
	double PlayAnimMontageSingle(UAnimInstance* animInstance, FActionMotionMontage montage, float customAnimStartTime = -1
	                             , bool useMontageEndCallback = false, FOnMontageEnded endCallBack = FOnMontageEnded());


	/// Evaluate component's from it's skeletal mesh Root motions
	void EvaluateRootMotions(float delta);


	/// Evaluate root motion override parameters
	FControllerStatus EvaluateRootMotionOverride(const FControllerStatus inStatus, float inDelta);


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
	bool ComponentTraceCastSingleUntil(FHitResult& outHit, FVector direction, FVector position, FQuat rotation, std::function<bool(FHitResult)> condition, int iterations = 3,
	                                   double inflation = 0.100, bool traceComplex = false);


	/// Check for all collisions at a position and rotation in a direction as overlaps. return true if any collision occurs
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	FORCEINLINE bool ComponentTraceCastMulti(TArray<FHitResultExpanded>& outHits, FVector position, FVector direction, FQuat rotation, double inflation = 0.100, bool traceComplex = false)
	{
		return ComponentTraceCastMulti_internal(outHits, position, direction, rotation, inflation, traceComplex, FCollisionQueryParams::DefaultQueryParam);
	}


	bool ComponentTraceCastMulti_internal(TArray<FHitResultExpanded>& outHits, FVector position, FVector direction, FQuat rotation, double inflation = 0.100, bool traceComplex = false,
	                                      FCollisionQueryParams& queryParams = FCollisionQueryParams::DefaultQueryParam,
	                                      double counterDirectionMaxOffset = TNumericLimits<float>::Max()) const;


	/// Check for collision at a position and rotation in a direction. return true if collision occurs
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	FORCEINLINE bool ComponentTraceCastSingle(FHitResult& outHit, FVector position, FVector direction, FQuat rotation, double inflation = 0.100, bool traceComplex = false)
	{
		return ComponentTraceCastSingle_Internal(outHit, position, direction, rotation, inflation, traceComplex, FCollisionQueryParams::DefaultQueryParam);
	}


	bool ComponentTraceCastSingle_Internal(FHitResult& outHit, FVector position, FVector direction, FQuat rotation, double inflation = 0.100, bool traceComplex = false,
	                                       FCollisionQueryParams& queryParams = FCollisionQueryParams::DefaultQueryParam) const;


	/// <summary>
	/// Trace component along a path
	/// </summary>
	/// <param name="results">the result of the trace</param>
	/// <param name="start">the start point</param>
	/// <param name="pathPoints">the point the trace wil follow</param>
	/// <param name="channel">the collision channel of the path</param>
	/// <param name="stopOnHit">should the trace stop when hit obstacle?</param>
	/// <param name="skinWeight">the skim weight of the component</param>
	/// <param name="debugRay">visulize the path?</param>
	/// <param name="rotateAlongPath">should the component rotation follow the path?</param>
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	FORCEINLINE void PathCastComponent(TArray<FHitResult>& results, FVector start, TArray<FVector> pathPoints, bool stopOnHit = true, float skinWeight = 0, bool debugRay = false,
	                                   bool rotateAlongPath = false, bool bendOnCollision = false, bool traceComplex = false)
	{
		PathCastComponent_Internal(results, start, pathPoints, stopOnHit, skinWeight, debugRay, rotateAlongPath, bendOnCollision, traceComplex, FCollisionQueryParams::DefaultQueryParam);
	}

	void PathCastComponent_Internal(TArray<FHitResult>& results, FVector start, TArray<FVector> pathPoints, bool stopOnHit = true, float skinWeight = 0, bool debugRay = false,
	                                bool rotateAlongPath = false, bool bendOnCollision = false, bool traceComplex = false,
	                                FCollisionQueryParams& queryParams = FCollisionQueryParams::DefaultQueryParam);


	/// <summary>
	/// Trace component along a path
	/// </summary>
	/// <param name="results">the result of the trace</param>
	/// <param name="start">the start point</param>
	/// <param name="pathPoints">the point the trace wil follow</param>
	/// <param name="channel">the collision channel of the path</param>
	/// <param name="stopOnHit">should the trace stop when hit obstacle?</param>
	/// <param name="debugRay">visulize the path?</param>
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	void PathCastLine(TArray<FHitResult>& results, FVector start, TArray<FVector> pathPoints, ECollisionChannel channel, bool stopOnHit = true, bool debugRay = false,
	                  bool bendOnCollision = false, bool traceComplex = false);


	/// Check for Overlap at a atPosition and rotation and return the separationForce needed for depenetration. work best with simple collisions
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	bool CheckPenetrationAt(FVector& separationForce, FVector& contactForce, FVector atPosition, FQuat withOrientation, UPrimitiveComponent* onlyThisComponent = nullptr,
	                        double hullInflation = 0.125, bool getVelocity = false);


	/// Return a point on the surface of the collider.
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	FVector PointOnShape(FVector direction, const FVector inLocation, const float hullInflation = 0.126);


	// Evaluate all conditions of a surface against this controller
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	bool EvaluateSurfaceConditions(FSurfaceCheckParams conditions, FSurfaceCheckResponse& response, FSurface surface, FControllerStatus status, FVector customLocation = FVector(0),
	                               FVector customOrientation = FVector(0),
	                               FVector customDirection = FVector(0));


#pragma endregion


#pragma region Debug

public:
#pragma endregion
};
