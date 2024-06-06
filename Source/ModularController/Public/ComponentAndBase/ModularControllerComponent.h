// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include <functional>

#include "Animation/AnimMontage.h"
#include "CoreMinimal.h"
#include "Structs.h"
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


// Modular pawn controller, handle the logic to move the pawn based on any movement state.
UCLASS(ClassGroup = "Controllers", hidecategories = (Object, LOD, Lighting, TextureStreaming, Velocity, PlanarMovement, MovementComponent, Tags, Activation, Cooking, AssetUserData, Collision), meta = (DisplayName = "Modular Controller", BlueprintSpawnableComponent))
class MODULARCONTROLLER_API UModularControllerComponent : public UNavMovementComponent
{
	GENERATED_BODY()

#pragma region Core and Constructor

public:

	// Sets default values for this component's properties
	UModularControllerComponent();



protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// Called to initialize the component
	void Initialize();

	// Called to Update the component logics
	void MainUpdateComponent(float delta);

	//Use this to offset rotation. useful when using skeletal mesh without as root primitive.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Core")
	FRotator RotationOffset;

	//When using a skeletal mesh, This is the name of the bone used as primitive for the movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Core")
	FName BoneName;

	//The component owner class, casted to pawn
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, category = "Controllers|Core")
	TSoftObjectPtr<APawn> _ownerPawn;

public:

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;



	// Get the controller's actor custom Transform
	UFUNCTION(BlueprintGetter, Category = "Controllers|Core")
	FORCEINLINE FTransform GetTransfrom() { return FTransform(GetRotation(), GetLocation(), GetScale()); }


	// Get the controller's actor position
	UFUNCTION(BlueprintGetter, Category = "Controllers|Core")
	FORCEINLINE FVector GetLocation() { return GetOwner()->GetActorLocation(); }


	// Get the controller's actor Rotation.
	UFUNCTION(BlueprintGetter, Category = "Controllers|Core")
	FORCEINLINE FQuat GetRotation() { return GetOwner()->GetActorQuat() * RotationOffset.Quaternion().Inverse(); }


	// Get the controller's actor Scale.
	UFUNCTION(BlueprintGetter, Category = "Controllers|Core")
	FORCEINLINE FVector GetScale() { return GetOwner()->GetActorScale(); }


	// Get the component's actor Forward vector
	UFUNCTION(BlueprintCallable, Category = "Controllers|Core")
	FORCEINLINE FVector GetForwardVector() { return GetRotation().GetForwardVector(); }


	// Get the component's actor Up vector
	UFUNCTION(BlueprintCallable, Category = "Controllers|Core")
	FORCEINLINE FVector GetUpVector() { return GetRotation().GetUpVector(); }


	// Get the component's actor Right vector
	UFUNCTION(BlueprintCallable, Category = "Controllers|Core")
	FORCEINLINE FVector GetRightVector() { return GetRotation().GetRightVector(); }


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

	// Lister to user input and Add input to the inputs pool
	void ListenInput(const FName key, const FInputEntry entry, const bool hold = false);

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

	// Read an input from the pool
	UFUNCTION(BlueprintCallable, Category = "Controllers|Inputs")
	FInputEntry ReadInput(const FName key);

	// Read an input from the pool.
	UFUNCTION(BlueprintCallable, Category = "Controllers|Inputs")
	bool ReadButtonInput(const FName key);

	// Read an input from the pool
	UFUNCTION(BlueprintCallable, Category = "Controllers|Inputs")
	float ReadValueInput(const FName key);

	// Read an input from the pool
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

	// The should the client have authority over the movement?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Network")
	bool bUseClientAuthorative = false;

	// The speed the client adjust his position to match the server's. negative values instantly match position.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Network")
	float AdjustmentSpeed = 10;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Network")
	int MaxSimulationCount = 500;


	// Used to replicate some properties.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


	// Get the network local role
	UFUNCTION(BlueprintCallable, Category = "Controllers|Network")
	ENetRole GetNetRole();


	// Get the network local role, as debug FName
	UFUNCTION(BlueprintCallable, Category = "Controllers|Network")
	FName GetNetRoleDebug(ENetRole role);


protected:

	// Called to Update the component logic in standAlone Mode. it's also used in other mode with parameters.
	void StandAloneUpdateComponent(FControllerStatus initialState, UInputEntryPool* usedInputPool, float delta, bool noCollision = false);


	/**
	 * @brief
	 * @param initialStatus Initial status of the controller
	 * @param moveInput The direction the user is willing to move
	 * @param usedInputPool the input pool used
	 * @param delta the delta time from the last frame
	 * @param simulationDatas for simulation: X= Simulation active? Y= Simulated state index, Z= Simulated action index
	 * @return the status after state processing.
	 */
	FControllerStatus EvaluateControllerStatus(const FControllerStatus initialStatus, UInputEntryPool* usedInputPool, const float delta, const FVector simulationDatas = FVector(-1));


	/**
	 * @brief Process velocity based on input status infos
	 * @param inStatus the input status parameter to process
	 * @param kinematicInfos the input kinematic infos
	 * @param usedInputPool the user input pool
	 * @param delta the delta time
	 * @return The resulting velocity
	 */
	FControllerStatus ProcessStatus(const FControllerStatus initialState, const float inDelta);


#pragma endregion


#pragma region Server Logic

public:

	/// Replicate server's user move to clients
	UFUNCTION(NetMulticast, Unreliable, Category = "Controllers|Network|Server To CLient|RPC")
	void MultiCastMoveCommand();

	/// Replicate server's statesClasses to clients on request
	UFUNCTION(NetMulticast, Reliable, Category = "Controllers|Network|Server To CLient|RPC")
	void MultiCastStates(const TArray<TSubclassOf<UBaseControllerState>>& statesClasses, UModularControllerComponent* caller);

	/// Replicate server's actionsClasses to clients on request
	UFUNCTION(NetMulticast, Reliable, Category = "Controllers|Network|Server To CLient|RPC")
	void MultiCastActions(const TArray<TSubclassOf<UBaseControllerAction>>& actionsClasses, UModularControllerComponent* caller);


#pragma region Listened

protected:

	// Called to Update the component logic in Listened Server Mode
	void ListenServerUpdateComponent(float delta);


#pragma endregion


#pragma region Dedicated


protected:

	// Called to Update the component logic in Dedicated Server Mode
	void DedicatedServerUpdateComponent(float delta);

#pragma endregion



#pragma endregion


#pragma region Client Logic

	/// Replicate server's states to clients on request
	UFUNCTION(Server, Reliable, Category = "Controllers|Network|Client To Server|RPC")
	void ServerRequestStates(UModularControllerComponent* caller);

	/// Replicate server's actions to clients on request
	UFUNCTION(Server, Reliable, Category = "Controllers|Network|Client To Server|RPC")
	void ServerRequestActions(UModularControllerComponent* caller);

#pragma region Automonous Proxy

public:

	/// Replicate client's movement infos to server
	UFUNCTION(Server, Unreliable, Category = "Controllers|Network|Client To Server|RPC")
	void ServerCastMoveCommand();

protected:

	// Called to Update the component logic in Autonomous Proxy Mode
	void AutonomousProxyUpdateComponent(float delta);

#pragma endregion


#pragma region Simulated Proxy

protected:

	// Called to Update the component logic in Simulated Proxy Mode
	void SimulatedProxyUpdateComponent(float delta);

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

	// Use physic sub-stepping?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	bool bUseSubStepping = false;

	// Use complex collision for movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	bool bUseComplexCollision = false;

	// Use physic interractions on server and stand alone?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	bool bUsePhysicAuthority = true;

	// Use physic interractions on clients
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	bool bUsePhysicClients = false;


	// The physic substep tick function delegate.
	FCalculateCustomPhysics OnCalculateCustomPhysics;

	// The component's current gravity vector
	FVector _gravityVector = -FVector::UpVector;

	// The collision velocity vector.
	FVector _collisionAccelerations;

	//The gravity state currently active.
	UPROPERTY()
	UBaseControllerState* _currentActiveGravityState;




	// The Substep physic tick function.
	void SubstepTick(float DeltaTime, FBodyInstance* BodyInstance);


	// Get the controller Gravity
	UFUNCTION(BlueprintGetter, Category = "Controllers|Physic")
	FORCEINLINE FVector GetGravity() const
	{
		return _gravityVector;
	}


	// Get the controller Gravity
	UFUNCTION(BlueprintCallable, Category = "Controllers|Physic")
	FORCEINLINE void SetGravity(FVector gravity, UBaseControllerState* gravityStateOverride = nullptr)
	{
		_gravityVector = gravity;
		if (gravityStateOverride)
			_currentActiveGravityState = gravityStateOverride;
	}

	// Get the controller's active Gravity state
	UFUNCTION(BlueprintGetter, Category = "Controllers|Physic")
	FORCEINLINE UBaseControllerState* GetCurrentGravityState() const
	{
		return _currentActiveGravityState;
	}

	// Get the controller Gravity's Direction
	UFUNCTION(BlueprintCallable, Category = "Controllers|Physic")
	FORCEINLINE FVector GetGravityDirection() const
	{
		return _gravityVector.GetSafeNormal();
	}


	// Get the controller Gravity's Scale
	UFUNCTION(BlueprintCallable, Category = "Controllers|Physic")
	FORCEINLINE float GetGravityScale() const
	{
		return _gravityVector.Length();
	}



	// Get the controller Mass
	UFUNCTION(BlueprintCallable, Category = "Controllers|Physic")
	FORCEINLINE float GetMass() const
	{
		return (Mass < 0 && UpdatedPrimitive != nullptr && UpdatedPrimitive->IsSimulatingPhysics()) ? UpdatedPrimitive->GetMass() : FMath::Clamp(Mass, 1, 9999999);
	}



	// Get the controller's Current State Surface
	UFUNCTION(BlueprintCallable, Category = "Controllers|Physic")
	FSurfaceInfos GetCurrentSurface() const;



	// Add a force to the controller
	UFUNCTION(BlueprintCallable, Category = "Controllers|Physic")
	void AddForce(FVector force, bool massIndependant = false);


	//Add force to surface at location. 
	UFUNCTION(BlueprintCallable, Category = "Surface|Physic")
	void AddMomentumOnHit(const FHitResult hit, FVector momentum);


	// called When overlap occurs.
	UFUNCTION()
	void BeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// called When Collision occurs.
	UFUNCTION()
	void BeginCollision(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);


#pragma endregion



#pragma region All Behaviours

protected:

	// The map of override root motion commands per simulation tag
	TMap<USkeletalMeshComponent*, FOverrideRootMotionCommand> _overrideRootMotionCommands;


public:

	/// <summary>
	/// Set an override of motion.
	/// </summary>
	/// <param name="translationMode">Override in translation Mode</param>
	/// <param name="rotationMode">Override in Rotation Mode</param>
	/// <returns></returns>
	UFUNCTION(BlueprintCallable, Category = "Controllers|Behaviours")
	void SetOverrideRootMotionMode(USkeletalMeshComponent* caller, const ERootMotionType translationMode, const ERootMotionType rotationMode);


#pragma endregion



#pragma region States

public:

	// The State types used on this controller by default
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Controller State")
	TArray<TSubclassOf<UBaseControllerState>> StateClasses;

	// The states instances used on this controller.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Controllers|Controller State")
	TArray<UBaseControllerState*> StatesInstances;

	// The time spend on the current state
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, category = "Controllers|Controller State")
	double TimeOnCurrentState;

	// The state Behaviour changed event
	UPROPERTY(BlueprintAssignable, Category = "Controllers|Controller State|Events")
	FControllerStateChangedSignature OnControllerStateChangedEvent;

public:

	// Get the current state behaviour instance
	UFUNCTION(BlueprintGetter, Category = "Controllers|Controller State")
	FORCEINLINE UBaseControllerState* GetCurrentControllerState() const
	{
		if (StatesInstances.IsValidIndex(ControllerStatus.ControllerStatus.StateIndex))
			return StatesInstances[ControllerStatus.ControllerStatus.StateIndex];
		return nullptr;
	}


	/// Check if we have a state behaviour by type
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller State")
	bool CheckControllerStateByType(TSubclassOf<UBaseControllerState> moduleType);


	/// Check if we have a state behaviour. by name
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller State")
	bool CheckControllerStateByName(FName moduleName);

	/// Check if we have a state behaviour. by priority
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller State")
	bool CheckControllerStateByPriority(int modulePriority);

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


	/**
	 * @brief Check controller states
	 * @param currentControllerStatus initial controller status.
	 * @param moveInput the user input pool
	 * @param inDelta the delta time
	 * @param simulationDatas for simulation: X= Simulation active? Y= Simulated state index, Z= Simulated action index
	 * @return returns a new status
	 */
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller State|Events")
	FControllerStatus CheckControllerStates(FControllerStatus currentControllerStatus, const float inDelta, FVector simulationDatas = FVector(-1));

	/// Change from state 1 to 2
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller State|Events")
	FControllerCheckResult TryChangeControllerState(FControllerStatus ToStateStatus, FControllerStatus fromStateStatus, const float inDelta, bool simulate = false);

	/// Evaluate the component state
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller State|Events")
	FControllerStatus ProcessControllerState(const FControllerStatus initialState, const float inDelta, bool simulate = false);


	/// <summary>
	/// When the controller change a behaviour, it call this function
	/// </summary>
	virtual void OnControllerStateChanged_Implementation(UBaseControllerState* newState, UBaseControllerState* oldState);


#pragma endregion



#pragma region Actions

public:


	/// The actions types used on this controller.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Controller Action")
	TArray<TSubclassOf<UBaseControllerAction>> ActionClasses;

	/// The actions instances used on this controller.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Controllers|Controller Action")
	TArray<UBaseControllerAction*> ActionInstances;

	// The action changed event
	UPROPERTY(BlueprintAssignable, Category = "Controllers|Controller Action|Events")
	FControllerActionChangedSignature OnControllerActionChangedEvent;


public:


	/// Get the current action behaviour instance
	UFUNCTION(BlueprintGetter, Category = "Controllers|Controller Action")
	FORCEINLINE UBaseControllerAction* GetCurrentControllerAction() const
	{
		if (ActionInstances.IsValidIndex(ControllerStatus.ControllerStatus.ActionIndex))
		{
			return ActionInstances[ControllerStatus.ControllerStatus.ActionIndex];
		}
		return nullptr;
	}



	/// Check if we have an action behaviour by type
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller Action")
	bool CheckActionBehaviourByType(TSubclassOf<UBaseControllerAction> moduleType);

	/// Check if we have an action behaviour by name
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller Action")
	bool CheckActionBehaviourByName(FName moduleName);

	/// Check if we have an action behaviour by priority
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller Action")
	bool CheckActionBehaviourByPriority(int modulePriority);

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

protected:


	/// Check controller Actions and returns the index of the active one.
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller Action|Events")
	FControllerStatus CheckControllerActions(FControllerStatus currentControllerStatus, const float inDelta, FVector simulationDatas = FVector(-1));

	/**
	 * @brief Check if an action is compatible with this state and those actions
	 * @param actionInstance The action to verify
	 * @param stateIndex the controller state index used
	 * @param actionIndex the action array used
	 * @return true if it's compatible
	 */
	bool CheckActionCompatibility(UBaseControllerAction* actionInstance, int stateIndex, int actionIndex);

	/// Change actions from action index 1 to 2
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller Action|Events")
	FControllerCheckResult TryChangeControllerAction(FControllerStatus toActionStatus, FControllerStatus fromActionStatus, const float inDelta, bool simulate = false);

	/// Evaluate the component state
	UFUNCTION(BlueprintCallable, Category = "Controllers|Controller Action|Events")
	FControllerStatus ProcessControllerAction(const FControllerStatus initialState, const float inDelta, bool simulate = false);


	// Process single action's velocity.
	FControllerStatus ProcessSingleAction(UBaseControllerAction* actionInstance, const FControllerStatus initialState, const float inDelta, bool simulate = false);


#pragma endregion



#pragma region Animation Component
public:

	/// The Skinned mesh component reference. used to play montages and switch anim linked instances based on current state.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Controllers|Animation Component", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SkeletalMeshComponent"))
	FComponentReference MainSkeletal;


	/// The Root motion scale. some times the root motion doesn't macth the actual movement and movement. this scale the movement ot match the animation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Animation Component")
	float RootMotionScale = 1;




	/// Get Root motion vector. the motion is not consumed after this
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	FVector GetRootMotionVector(USkeletalMeshComponent* skeletalMeshReference);

	/// Get Root motion rotation. the motion is not consumed after this
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	FQuat GetRootMotionQuat(USkeletalMeshComponent* skeletalMeshReference);


	/// Get the controller's Skeletal Mesh
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	USkeletalMeshComponent* GetSkeletalMesh();

	///Play an animation montage on the controller globally. returns the duration
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	double PlayAnimationMontage(FActionMotionMontage Montage, float customAnimStartTime = -1);

	///Play an animation montage on the specified controller state linked anim graph. returns the duration
	UFUNCTION(BlueprintCallable, Category = "Controllers|Animation Component")
	double PlayAnimationMontageOnState(FActionMotionMontage Montage, FName stateName, float customAnimStartTime = -1);


	///Play an animation montage on the controller globally. returns the duration
	double PlayAnimationMontage_Internal(FActionMotionMontage Montage, float customAnimStartTime = -1
		, bool useMontageEndCallback = false, FOnMontageEnded endCallBack = {});

	///Play an animation montage on the specified controller state linked anim graph. returns the duration
	double PlayAnimationMontageOnState_Internal(FActionMotionMontage Montage, FName stateName, float customAnimStartTime = -1
		, bool useMontageEndCallback = false, FOnMontageEnded endCallBack = {});


	//Read the skeletal mesh root motion.
	void ReadRootMotion(FKinematicComponents& kinematics, const ERootMotionType rootMotionMode, const double deltaTime);

	//Read the root motion translation vector
	FVector GetRootMotionTranslation(const ERootMotionType rootMotionMode, const FVector currentVelocity, const double deltaTime);

private:

	// The root motion transform.
	TMap<TSoftObjectPtr<USkeletalMeshComponent>, FTransform> _RootMotionParams;

	// The linked anim Map
	TMap<TSoftObjectPtr<USkeletalMeshComponent>, TMap<FName, TSoftObjectPtr<UAnimInstance>>> _linkedAnimClasses;

	//the cached skeletal mesh reference
	TSoftObjectPtr<USkeletalMeshComponent> _skeletalMesh;

protected:

	/// Link anim blueprint on a skeletal mesh, with a key. the use of different key result in the link of several anim blueprints.
	virtual void LinkAnimBlueprint(USkeletalMeshComponent* skeletalMeshReference, FName key, TSubclassOf<UAnimInstance> animClass);

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

	/// The kinematic components of the last frame.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, category = "Controllers|Movement")
	FControllerStatus ControllerStatus;


	/// Move the owner in a direction. return the displacement actually made. it can be different from the input movement if collision occured.
	UFUNCTION(BlueprintNativeEvent, Category = "Controllers|Movement|Events")
	void Move(const FKinematicComponents finalKinematic, float deltaTime);

protected:

	/// Move the owner in a direction. return the displacement actually made. it can be different from the input movement if collision occured.
	virtual void Move_Implementation(const FKinematicComponents finalKinematic, float deltaTime);


	/// Evaluate the movement and return the new kinematic component.
	virtual FKinematicComponents KinematicMoveEvaluation(FControllerStatus processedMove, bool noCollision, float delta, bool applyForceOnSurfaces = true);


	/// Handle premove logic. Always call at the top of the Update.
	FControllerStatus ConsumeLastKinematicMove(FVector moveInput);


	/// Operation to Conserve momentum.
	void KinematicPostMove(FControllerStatus newStatus, const float inDelta);


	/// Handle the controller rotation to always stay Up aligned with gravity
	FAngularKinematicCondition HandleKinematicRotation(const FAngularKinematicCondition inRotCondition, const float inDelta) const;


	/// Simulate A slide along a surface at a position with a rotation. Returns the position after slide.
	FVector SlideAlongSurfaceAt(FHitResult& Hit, const FQuat rotation, FVector attemptedMove, int& depth, double deltaTime, float hullInflation = 0.100);


#pragma endregion



#pragma region Tools & Utils

public:


	/// Show the debugs traces and logs
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Debug")
	TEnumAsByte<EControllerDebugType> DebugType;
	


	//Tracecast a number of times until the condition is meet
	bool ComponentTraceCastSingleUntil(FHitResult& outHit, FVector direction, FVector position, FQuat rotation, std::function<bool(FHitResult)> condition, int iterations = 3, double inflation = 0.100, bool traceComplex = false);


	/// Check for collision at a position and rotation in a direction. return true if collision occurs
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	FORCEINLINE bool ComponentTraceCastMulti(TArray<FHitResult>& outHits, FVector position, FVector direction, FQuat rotation, double inflation = 0.100, bool traceComplex = false)
	{
		return ComponentTraceCastMulti_internal(outHits, position, direction, rotation, inflation, traceComplex, FCollisionQueryParams::DefaultQueryParam);
	}


	bool ComponentTraceCastMulti_internal(TArray<FHitResult>& outHits, FVector position, FVector direction, FQuat rotation, double inflation = 0.100, bool traceComplex = false, FCollisionQueryParams& queryParams = FCollisionQueryParams::DefaultQueryParam);



	/// Check for collision at a position and rotation in a direction. return true if collision occurs
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	FORCEINLINE bool ComponentTraceCastSingle(FHitResult& outHit, FVector position, FVector direction, FQuat rotation, double inflation = 0.100, bool traceComplex = false)
	{
		return ComponentTraceCastSingle_Internal(outHit, position, direction, rotation, inflation, traceComplex, FCollisionQueryParams::DefaultQueryParam);
	}


	bool ComponentTraceCastSingle_Internal(FHitResult& outHit, FVector position, FVector direction, FQuat rotation, double inflation = 0.100, bool traceComplex = false, FCollisionQueryParams& queryParams = FCollisionQueryParams::DefaultQueryParam);



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
	FORCEINLINE void PathCastComponent(TArray<FHitResult>& results, FVector start, TArray<FVector> pathPoints, bool stopOnHit = true, float skinWeight = 0, bool debugRay = false, bool rotateAlongPath = false, bool bendOnCollision = false, bool traceComplex = false)
	{
		PathCastComponent_Internal(results, start, pathPoints, stopOnHit, skinWeight, debugRay, rotateAlongPath, bendOnCollision, traceComplex, FCollisionQueryParams::DefaultQueryParam);
	}

	void PathCastComponent_Internal(TArray<FHitResult>& results, FVector start, TArray<FVector> pathPoints, bool stopOnHit = true, float skinWeight = 0, bool debugRay = false, bool rotateAlongPath = false, bool bendOnCollision = false, bool traceComplex = false, FCollisionQueryParams& queryParams = FCollisionQueryParams::DefaultQueryParam);


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
	void PathCastLine(TArray<FHitResult>& results, FVector start, TArray<FVector> pathPoints, ECollisionChannel channel, bool stopOnHit = true, bool debugRay = false, bool bendOnCollision = false, bool traceComplex = false);


	/// Check for Overlap at a atPosition and rotation and return the separationForce needed for depenetration. work best with simple collisions
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	bool CheckPenetrationAt(FVector& separationForce, FVector& contactForce, FVector atPosition, FQuat withOrientation, UPrimitiveComponent* onlyThisComponent = nullptr, double hullInflation = 0.125, bool getVelocity = false);


	/// Return a point on the surface of the collider.
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	FVector PointOnShape(FVector direction, const FVector inLocation, const float hullInflation = 0.126);

	/// Get the mass of a component upon hit
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	double GetHitComponentMass(FHitResult hit);


#pragma endregion

};

