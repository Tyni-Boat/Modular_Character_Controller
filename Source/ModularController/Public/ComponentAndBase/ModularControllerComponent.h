// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "Animation/AnimMontage.h"
#include "CoreMinimal.h"
#include "InputTranscoderConfig.h"
#include "Structs.h"
#include "Containers/Queue.h"
#include "GameFramework/MovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/GameStateBase.h"

#ifndef BASE_ACTION
#define BASE_ACTION
#include "BaseAction.h"
#endif

#ifndef BASE_STATE
#define BASE_STATE
#include "BaseState.h"
#endif

#include "ModularControllerComponent.generated.h"





/// <summary>
/// Declare a multicast for when an action begins
/// </summary>
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FActionBeginsSignature, UModularControllerComponent, OnActionBeginsEvent, UBaseAction*, Action);

/// <summary>
/// Declare a multicast for when an action Ends
/// </summary>
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FActionEndsSignature, UModularControllerComponent, OnActionEndsEvent, UBaseAction*, Action);

/// <summary>
/// Declare a multicast for when an action Cancelled
/// </summary>
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FActionCancelledSignature, UModularControllerComponent, OnActionCancelledEvent, UBaseAction*, Action);

/// <summary>
/// Declare a multicast for when a behaviour changed
/// </summary>
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FBehaviourChangedSignature, UModularControllerComponent, OnBehaviourChangedEvent, UBaseState*, LastOne, UBaseState*, NewOne);


// Modular pawn controller, handle the logic to move the pawn based on any movement state.
UCLASS(ClassGroup = "Controllers", hidecategories = (Object, LOD, Lighting, TextureStreaming, Velocity, PlanarMovement, MovementComponent, Tags, Activation, Cooking, AssetUserData, Collision), meta = (DisplayName = "Modular Controller", BlueprintSpawnableComponent))
class MODULARCONTROLLER_API UModularControllerComponent : public UMovementComponent
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Core")
	FRotator RotationOffset;

	//When using a skeletal mesh, This is the name of the bone used as primitive for the movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Core")
	FName BoneName;

	//The component owner class, casted to pawn
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, category = "Core")
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
	FInputEntryPool _user_inputPool;

public:

	//Use this to offset rotation. useful when using skeletal mesh without as root primitive.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Core")
	TSubclassOf<UInputTranscoderConfig> InputTranscoder;
	
	// Lister to user input and Add input to the inputs pool
	UFUNCTION(BlueprintCallable, Category = "Controllers|Inputs")
	void ListenInput(const FName key, const FInputEntry entry);


	// Read an input from the pool
	UFUNCTION(BlueprintCallable, Category = "Controllers|Inputs")
	FInputEntry ReadInput(const FName key) const;


#pragma endregion



#pragma region Network Logic

#pragma region Common Logic

private:

	//Describe the current synchronisation state of the controller. useful to enforce synchronisation.
	TEnumAsByte<ENetSyncState> _netSyncState = NetSyncState_WaitingFirstSync;


public:

	// The distance of the client from the server position to make a correction on client
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Network")
	float CorrectionDistance = 50;

	// The speed the client adjust his position to match the server's. negative values instantly match position.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Network")
	float AdjustmentSpeed = 1;

	// The maximum size of client's move history buffer
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Network")
	int MaxClientHistoryBufferSize = 500;


	// Used to replicate some properties.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


	// Get the network local role
	UFUNCTION(BlueprintCallable, Category = "Controllers|Network")
	ENetRole GetNetRole();


	// Get the network local role, as debug FName
	UFUNCTION(BlueprintCallable, Category = "Controllers|Network")
	FName GetNetRoleDebug(ENetRole role);


	// Get the component time clock. It's the server time + offset
	UFUNCTION(BlueprintGetter, Category = "Controllers|Network")
	FORCEINLINE double GetNetworkTime() const
	{
		return GetWorld()->GetGameState()->GetServerWorldTimeSeconds() + _serverTimeOffset;
	}


protected:

	// Called to Update the component logic in standAlone Mode. it's also used in other mode with parameters.
	FKinematicInfos StandAloneUpdateComponent(FKinematicInfos& movementInfos, FInputEntryPool& usedInputPool, float delta, FVelocity overrideVelocity, bool bOverrideMove = false, bool bOverrideRotation = false, bool noInputsUpdate = false);


#pragma endregion


#pragma region Server Logic

public:


	/// Replicate server's movement to all clients
	UFUNCTION(NetMulticast, Unreliable, Category = "Controllers|Network|Server To CLient|RPC")
	void MultiCastMoveSync(FSyncMoveRequest movementRequest);

	/// Replicate server's movement to all clients
	void MultiCastMoveSync_Implementation(FSyncMoveRequest movementRequest);



#pragma region Listened

protected:

	// Called to Update the component logic in Listened Server Mode
	void ListenServerUpdateComponent(float delta);


#pragma endregion


#pragma region Dedicated
private:

	// The list of client's movements received by the dedicated server. the server will check and try them one after the other and remove from the list each time.
	TArray<FSyncMoveRequest> _clientToServer_MoveRequestList;

	// The last valid client movement's time stamp. used to simulated movement on packet loss.
	int _lastClientTimeStamp = 0;

	// The dedicated server requets temp array. to batch several movement commands at once.
	TArray<FSyncMoveRequest> _dedicatedServerSyncRequests;

	// The latency, time elapsed from the last received move command reception
	float _clientToServerLatency;


public:

	// Return server time to client id who ask for it.
	UFUNCTION(NetMulticast, Reliable, Category = "Network|Server To CLient|RPC")
	void MultiCastTimeSync(int clientID, double serverTime);

	// Replicate Correction to all clients.
	void MultiCastTimeSync_Implementation(int clientID, double serverTime);

	// Replicate Correction to all clients.
	UFUNCTION(NetMulticast, Reliable, Category = "Network|Server To CLient|RPC")
	void MultiCastCorrectionSync(FSyncMoveRequest movementRequest);

	// Replicate Correction to all clients.
	void MultiCastCorrectionSync_Implementation(FSyncMoveRequest movementRequest);

protected:

	// Called to Update the component logic in Dedicated Server Mode
	void DedicatedServerUpdateComponent(float delta);

#pragma endregion



#pragma endregion


#pragma region Client Logic
private:

	//The offset from the server time for simulation.
	double _serverTimeOffset = 0;

	//The time at wich the client strted syncing time with the server
	double _startSyncTimeDate = 0;

	// The Correction comming from the server to a client.
	FSyncMoveRequest _serverToClient_CorrectionSync;

	// The movement comming from the server to a client for a  lite ajustment
	TQueue<FSyncMoveRequest> _serverToClient_AdjustmentSync;

protected:

	// Apply server correction to clients
	bool ApplyServerMovementCorrection(FKinematicInfos& outCorrected);


#pragma region Automonous Proxy
private:

	// The history of client's movements. it's size is controlled by MaxClientHistoryBufferSize
	TArray<FSyncMoveRequest> _clientSelf_MoveHistory;


public:

	// Request the Time from the server
	UFUNCTION(Server, Unreliable, Category = "Controllers|Network|Client To Server|RPC")
	void ServerTimeSyncRequest(int clientID, double startTime);

	// Request the Time from the server implementation
	void ServerTimeSyncRequest_Implementation(int clientID, double startTime);

	// Send movement to the dedicated server.
	UFUNCTION(Server, Unreliable, Category = "Controllers|Network|Client To Server|RPC")
	void ServerMoveSync(FSyncMoveRequest movementRequest);

	// Send movement to the dedicated server.
	void ServerMoveSync_Implementation(FSyncMoveRequest movementRequest);

	// Acknowledge the last server correction.
	UFUNCTION(Server, Reliable, Category = "Controllers|Network|Client To Server|RPC")
	void ServerCorrectionAcknowledge(FSyncMoveRequest movementRequest);


	// Acknowledge the last server correction.
	void ServerCorrectionAcknowledge_Implementation(FSyncMoveRequest movementRequest);

protected:

	// Called to Update the component logic in Autonomous Proxy Mode
	void AutonomousProxyUpdateComponent(float delta);


	// Apply server correction to clients
	bool ApplyServerMovementAdjustments(FVector& offset);

#pragma endregion


#pragma region Simulated Proxy
private:

	// The queue of server's movements the simulted proxy client have to execute.
	TQueue<FSyncMoveRequest> _serverToClient_MoveCommandQueue;

	// The last server movement made.
	FSyncMoveRequest _lastServerResquest;

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

	// Use physic sub-stepping?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	bool bUseSubStepping = false;

	// Use physic interractions on server and stand alone?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	bool bUsePhysicAuthority = true;

	// Use physic interractions on clients
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Physic")
	bool bUsePhysicClients = false;


	// The physic substep tick function delegate.
	FCalculateCustomPhysics OnCalculateCustomPhysics;

	// The component's current gravity vector
	FVector _gravityVector = FVector::UpVector;

	// The collision velocity vector.
	FVector _collisionForces;




	// The Substep physic tick function.
	void SubstepTick(float DeltaTime, FBodyInstance* BodyInstance);


	// Get the controller Gravity
	FORCEINLINE FVector GetGravity() const
	{
		return _gravityVector;
	}


	// Get the controller Gravity
	FORCEINLINE void SetGravity(FVector gravity)
	{
		_gravityVector = gravity;
	}

	// Get the controller Gravity's Direction
	FORCEINLINE FVector GetGravityDirection() const
	{
		return _gravityVector.GetSafeNormal();
	}


	// Get the controller Gravity's Scale
	FORCEINLINE float GetGravityScale() const
	{
		return _gravityVector.Length();
	}



	// Get the controller Mass
	FORCEINLINE float GetMass() const
	{
		return (Mass < 0 && UpdatedPrimitive != nullptr && UpdatedPrimitive->IsSimulatingPhysics()) ? UpdatedPrimitive->GetMass() : FMath::Clamp(Mass, 1, 9999999);
	}


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
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, category = "Controllers|StateBehaviours")
	TArray<TSubclassOf<UBaseState>> StateClasses;

	// The states instances used on this controller.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Controllers|StateBehaviours")
	TArray<UBaseState*> StatesInstances;

	// The state Behaviour changed event
	UPROPERTY(BlueprintAssignable, Category = "Controllers|StateBehaviours|Events")
	FBehaviourChangedSignature OnBehaviourChangedEvent;

public:

	// Get the current state behaviour instance
	UFUNCTION(BlueprintCallable, Category = "Controllers|StateBehaviours")
	FORCEINLINE UBaseState* GetCurrentStateBehaviour()
	{
		if (StatesInstances.IsValidIndex(LastMoveMade.FinalStateIndex))
			return StatesInstances[LastMoveMade.FinalStateIndex];
		return nullptr;
	}


	/// Check if we have a state behaviour by type
	UFUNCTION(BlueprintCallable, Category = "Controllers|StateBehaviours")
	bool CheckStateBehaviourByType(TSubclassOf<UBaseState> moduleType);


	/// Check if we have a state behaviour. by name
	UFUNCTION(BlueprintCallable, Category = "Controllers|StateBehaviours")
	bool CheckStateBehaviourByName(FName moduleName);

	/// Check if we have a state behaviour. by priority
	UFUNCTION(BlueprintCallable, Category = "Controllers|StateBehaviours")
	bool CheckStateBehaviourByPriority(int modulePriority);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Add a state behaviour
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|StateBehaviours")
	void AddStateBehaviour(TSubclassOf<UBaseState> moduleType);

	/// Add a state behaviour
	void AddStateBehaviour_Implementation(TSubclassOf<UBaseState> moduleType);

	/// Get reference to a state by type
	UFUNCTION(BlueprintCallable, Category = "Controllers|StateBehaviours")
	UBaseState* GetStateByType(TSubclassOf<UBaseState> moduleType);


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Remove a state behaviour by type
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|StateBehaviours")
	void RemoveStateBehaviourByType(TSubclassOf<UBaseState> moduleType);

	/// Remove a state behaviour by type
	void RemoveStateBehaviourByType_Implementation(TSubclassOf<UBaseState> moduleType);

	/// Remove a state behaviour by name
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|StateBehaviours")
	void RemoveStateBehaviourByName(FName moduleName);

	/// Remove a state behaviour by name
	void RemoveStateBehaviourByName_Implementation(FName moduleName);


	/// Remove a state behaviour by priority
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|StateBehaviours")
	void RemoveStateBehaviourByPriority(int modulePriority);

	/// Remove a state behaviour by priority
	void RemoveStateBehaviourByPriority_Implementation(int modulePriority);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// When the controller change a state, it call this function
	UFUNCTION(BlueprintNativeEvent, Category = "Controllers|StateBehaviours|Events")
	void OnBehaviourChanged(UBaseState* OldOne, UBaseState* NewOne);

protected:

	/// Evaluate the component state
	UFUNCTION(BlueprintCallable, Category = "Controllers|StateBehaviours|Events")
	FVelocity EvaluateState(FKinematicInfos& inDatas, const FInputEntryPool inputs, const float inDelta, const bool asSimulation = false);


	/// <summary>
	/// When the controller change a behaviour, it call this function
	/// </summary>
	virtual void OnBehaviourChanged_Implementation(UBaseState* OldOne, UBaseState* NewOne);


#pragma endregion



#pragma region Actions

public:


	/// The actions types used on this controller.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, category = "Controllers|ActionBehaviours")
	TArray<TSubclassOf<UBaseAction>> ActionClasses;

	/// The actions instances used on this controller.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Controllers|ActionBehaviours")
	TArray<UBaseAction*> ActionInstances;

	/// The action begins event
	UPROPERTY(BlueprintAssignable, Category = "Controllers|ActionBehaviours|Events")
	FActionBeginsSignature OnActionBeginsEvent;


	/// The action Ends event
	UPROPERTY(BlueprintAssignable, Category = "Controllers|ActionBehaviours|Events")
	FActionEndsSignature OnActionEndsEvent;

	/// The action cancelled event
	UPROPERTY(BlueprintAssignable, Category = "Controllers|ActionBehaviours|Events")
	FActionCancelledSignature OnActionCancelledEvent;


public:


	/// Get the current action behaviour instance
	UFUNCTION(BlueprintCallable, Category = "Controllers|ActionBehaviours")
	FORCEINLINE TArray<UBaseAction*> GetCurrentActions()
	{
		TArray<UBaseAction*> actions;
		for (int i = 0; i < LastMoveMade.FinalActionsIndexes.Num(); i++)
		{
			if (ActionInstances.IsValidIndex(LastMoveMade.FinalActionsIndexes[i]))
			{
				actions.Add(ActionInstances[LastMoveMade.FinalActionsIndexes[i]]);
			}
		}
		return actions;
	}



	/// Check if we have an action behaviour by type
	UFUNCTION(BlueprintCallable, Category = "Controllers|ActionBehaviours")
	bool CheckActionBehaviourByType(TSubclassOf<UBaseAction> moduleType);

	/// Check if we have an action behaviour by name
	UFUNCTION(BlueprintCallable, Category = "Controllers|ActionBehaviours")
	bool CheckActionBehaviourByName(FName moduleName);

	/// Check if we have an action behaviour by priority
	UFUNCTION(BlueprintCallable, Category = "Controllers|ActionBehaviours")
	bool CheckActionBehaviourByPriority(int modulePriority);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// Add an action behaviour
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|ActionBehaviours")
	void AddActionBehaviour(TSubclassOf<UBaseAction> moduleType);


	/// Add an action behaviour
	void AddActionBehaviour_Implementation(TSubclassOf<UBaseAction> moduleType);

	/// Get an action behaviour by type
	UFUNCTION(BlueprintCallable, Category = "Controllers|ActionBehaviours")
	UBaseAction* GetActionByType(TSubclassOf<UBaseAction> moduleType);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// Remove an action behaviour by type
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|ActionBehaviours")
	void RemoveActionBehaviourByType(TSubclassOf<UBaseAction> moduleType);

	/// Remove an action behaviour by type
	void RemoveActionBehaviourByType_Implementation(TSubclassOf<UBaseAction> moduleType);

	/// Remove an action behaviour by name
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|ActionBehaviours")
	void RemoveActionBehaviourByName(FName moduleName);

	/// Remove an action behaviour by name
	void RemoveActionBehaviourByName_Implementation(FName moduleName);


	/// Remove an action behaviour by priority
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Controllers|ActionBehaviours")
	void RemoveActionBehaviourByPriority(int modulePriority);

	/// Remove an action behaviour by priority
	void RemoveActionBehaviourByPriority_Implementation(int modulePriority);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// When an action begins this controller, it call this function
	UFUNCTION(BlueprintNativeEvent, Category = "Controllers|Controllers|ActionBehaviours|Events")
	void OnActionBegins(UBaseAction* action);


	/// When an action Ends this controller, it call this function
	UFUNCTION(BlueprintNativeEvent, Category = "Controllers|ActionBehaviours|Events")
	void OnActionEnds(UBaseAction* action);


	/// When an action is Cancelled or repeated this controller, it call this function
	UFUNCTION(BlueprintNativeEvent, Category = "Controllers|ActionBehaviours|Events")
	void OnActionCancelled(UBaseAction* action);

protected:


	/// Evaluate the component movement through it's beheviours
	UFUNCTION(BlueprintCallable, Category = "Controllers|ActionBehaviours|Events")
	FVelocity EvaluateAction(const FVelocity inVelocities, FKinematicInfos& inDatas, const FInputEntryPool inputs, const float inDelta, const bool asSimulation = false);


	/// When an action begins this controller, it call this function
	void OnActionBegins_Implementation(UBaseAction* action);


	/// When an action Ends this controller, it call this function
	void OnActionEnds_Implementation(UBaseAction* action);

	/// When an action is Cancelled or repeated this controller, it call this function
	void OnActionCancelled_Implementation(UBaseAction* action);

	/// <summary>
	/// Called at the end of an action's montage.
	/// </summary>
	/// <param name="Montage"></param>
	/// <param name="bInterrupted"></param>
	void OnAnimationEnded(UAnimMontage* Montage, bool bInterrupted);


#pragma endregion



#pragma region Animation Component
public:

	/// The Skinned mesh component reference. used to play montages and switch anim linked instances based on current state.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Controllers|Animation Component", meta = (UseComponentPicker, AllowedClasses = "SkeletalMeshComponent"))
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


	/// Evaluate component's from it's skeletal mesh Root motions
	void EvaluateRootMotions(float delta);


	/// Evaluate root motion override parameters
	void EvaluateRootMotionOverride(FVelocity& movement, const FKinematicInfos inDatas, float inDelta);


#pragma endregion



#pragma region Movement

public:

	/// The last frame's movement informations. Can easilly provide a way to compute values like acceleration.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, category = "Controllers|Movement")
	FKinematicInfos LastMoveMade;


public:


	/// Move the owner in a direction. return the displacement actually made. it can be different from the input movement if collision occured.
	UFUNCTION(BlueprintNativeEvent, Category = "Controllers|Movement|Events")
	FVelocity Move(const FKinematicInfos& inDatas, FVelocity movement, float delta);


protected:

	/// Move the owner in a direction. return the displacement actually made. it can be different from the input movement if collision occured.
	virtual FVelocity Move_Implementation(const FKinematicInfos& inDatas, FVelocity movement, float delta);


	/// Operation to update the last movement made
	void PostMoveUpdate(FKinematicInfos& inDatas, const FVelocity moveMade, const float inDelta);


	/// Hadle the controller rotation to always stay Up aligned with gravity
	FQuat HandleRotation(const FVelocity inVelocities, const FKinematicInfos inDatas, const float inDelta) const;


	/// Simulate A slide along a surface at a position with a rotation. Returns the position after slide.
	FVector SlideAlongSurfaceAt(const FVector& Position, const FQuat& Rotation, const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit);


#pragma endregion



#pragma region Tools & Utils

public:


	/// Show the debugs traces and logs
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Controllers|Debug")
	bool ShowDebug;


	/// Check for collision at a position and rotation in a direction. return true if collision occurs
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	bool ComponentTraceCastMulti(TArray<FHitResult>& outHits, FVector position, FVector direction, FQuat rotation, ECollisionChannel channel = ECC_Visibility);


	/// Check for collision at a position and rotation in a direction. return true if collision occurs.
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	bool ComponentTraceCastMultiByInflation(TArray<FHitResult>& outHits, FVector position, FVector direction, FQuat rotation, double inflation = 0.100, ECollisionChannel channel = ECC_Visibility);


	/// Check for collision at a position and rotation in a direction. return true if collision occurs
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	bool ComponentTraceCastSingle(FHitResult& outHit, FVector position, FVector direction, FQuat rotation, ECollisionChannel channel = ECC_Visibility);


	/// Check for collision at a position and rotation in a direction. return true if collision occurs
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	bool ComponentTraceCastSingleByInflation(FHitResult& outHit, FVector position, FVector direction, FQuat rotation, double inflation = 0.100, ECollisionChannel channel = ECC_Visibility);


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
	void PathCastComponent(TArray<FHitResult>& results, FVector start, TArray<FVector> pathPoints, ECollisionChannel channel, bool stopOnHit = true, float skinWeight = 0, bool debugRay = false, bool rotateAlongPath = false, bool bendOnCollision = false);


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
	void PathCastLine(TArray<FHitResult>& results, FVector start, TArray<FVector> pathPoints, ECollisionChannel channel, bool stopOnHit = true, bool debugRay = false, bool bendOnCollision = false);


	/// Check for Overlap at a position and rotation and return the force needed for depenetration.
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	bool CheckPenetrationAt(FVector& force, FVector position, FQuat NewRotationQuat, UPrimitiveComponent* onlyThisComponent = nullptr, ECollisionChannel channel = ECC_Camera, bool debugHit = false);


	/// Return a point on the surface of the collider.
	UFUNCTION(BlueprintCallable, Category = "Controllers|Tools & Utils")
	FVector PointOnShape(FVector direction, const FVector inLocation, bool debugPoint = false);


#pragma endregion

};
