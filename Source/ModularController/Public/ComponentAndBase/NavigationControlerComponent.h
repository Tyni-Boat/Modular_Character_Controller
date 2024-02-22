// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "GameFramework/NavMovementComponent.h"
#include "NavigationSystem.h"
#include "NavLinkCustomInterface.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UObject/WeakObjectPtr.h"
#include "NavigationControlerComponent.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPathReached);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPathUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPathFailed);



#pragma region Network path type XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


/// <summary>
/// The representation of a path point on the network
/// </summary>
UCLASS()
class MODULARCONTROLLER_API UNetPathPoint : public UObject
{
	GENERATED_BODY()

public:

	UNetPathPoint();

	UNetPathPoint(FVector location, int index, INavLinkCustomInterface* navLinkInterface);

	virtual bool IsSupportedForNetworking() const override;

public:

	//the index of the point on the path.
	UPROPERTY(Replicated)
	int PointIndex;

	//the point location
	UPROPERTY(Replicated)
	FVector_NetQuantize10 Location;

	////the point location
	UPROPERTY(Replicated)
	UObject* NavLinkInterface;

	//the navlink at this point.
	INavLinkCustomInterface* GetNavLinkInterface() const;
};

#pragma endregion



UCLASS(ClassGroup = "Controllers", meta = (BlueprintSpawnableComponent))
class MODULARCONTROLLER_API UNavigationControlerComponent : public UPathFollowingComponent
{
	GENERATED_BODY()

#pragma region Core XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

public:
	// Sets default values for this component's properties
	UNavigationControlerComponent();

	/// <summary>
	/// The ai agent height
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Controllers|Core")
	float AgentHeight = 180;

	/// <summary>
	/// The ai agent radius
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Controllers|Core")
	float AgentRadius = 40;

	/// <summary>
	/// activate the debug mode.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Controllers|Core")
	bool IsDebug;

protected:

	//The owner actor's pawn
	UPROPERTY()
	APawn* t_ownerPawn;

	//The move request done earlier
	FAIMoveRequest _moveRequest;

	//The move request ID
	FAIRequestID _currentMoveRequestID;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;


public:

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#pragma endregion


#pragma region Network XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

	/// <summary>
	/// Get the network role of the owning actor
	/// </summary>
	/// <returns></returns>
	UFUNCTION(BlueprintGetter)
	FORCEINLINE ENetRole GetNetRole() const { return GetOwner()->GetLocalRole(); }


	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;


	/// <summary>
	/// Find path to location on server
	/// </summary>
	/// <param name="location"></param>
	/// <param name="filter"></param>
	UFUNCTION(Server, Reliable)
	void ServerFindPathToLocation(FVector location, TSubclassOf<UNavigationQueryFilter> filter);

	void ServerFindPathToLocation_Implementation(FVector location, TSubclassOf<UNavigationQueryFilter> filter);

	/// <summary>
	/// Find path to actor on server
	/// </summary>
	/// <param name="target"></param>
	/// <param name="filter"></param>
	UFUNCTION(Server, Reliable)
	void ServerFindPathToActor(AActor* target, TSubclassOf<UNavigationQueryFilter> filter);

	void ServerFindPathToActor_Implementation(AActor* target, TSubclassOf<UNavigationQueryFilter> filter);

	/// <summary>
	/// Update path on clients
	/// </summary>
	UFUNCTION(NetMulticast, Reliable)
	void MulticastUpdatePath();

	void MulticastUpdatePath_Implementation();


	/// <summary>
	/// Execute a la fin d'un path.
	/// </summary>
	/// <param name="EPathFollowingResult"></param>
	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnPathFinnished(uint32 EPathFollowingResult);

	void MulticastOnPathFinnished_Implementation(uint32 EPathFollowingResult);



#pragma endregion


#pragma region Path Requests and Follow XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
public:

	//the maximum distance away from the path that trigger path recalculation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Controllers|Path Requests and Follow")
	float MaxMoveAwayDistanceThreshold = 1000;

	//the replicated path points.
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "AI Controllers|Path Requests and Follow")
	TArray<UNetPathPoint*> CustomPathPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AI Controllers|Path Requests and Follow")
	FVector PathVelocity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI Controllers|Path Requests and Follow")
	float PathRemainingDistance;

	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly, Category = "AI Controllers|Path Requests and Follow")
	bool IsFollowingAPath;


	UPROPERTY(BlueprintAssignable)
	FOnPathReached OnPathReachedEvent;

	UPROPERTY(BlueprintAssignable)
	FOnPathFailed OnPathFailedEvent;

	UPROPERTY(BlueprintAssignable)
	FOnPathUpdated OnPathUpdatedEvent;

protected:

	//the client navigation next index
	uint32 _clientNextPathIndex;

	//The last distance. used to recalculate path.
	float t_lastDistance = -1;


public:

	/// <summary>
	/// Request an AI path to a destination.
	/// </summary>
	/// <param name="location"></param>
	/// <param name="filter"></param>
	/// <returns></returns>
	bool AIRequestPathTo(FVector location, TSubclassOf<UNavigationQueryFilter> filter = nullptr);

	/// <summary>
	/// Request an AI path to an actor.
	/// </summary>
	/// <param name="target"></param>
	/// <param name="filter"></param>
	/// <returns></returns>
	bool AIRequestPathToActor(AActor* target, TSubclassOf<UNavigationQueryFilter> filter = nullptr);

	virtual void OnPathUpdated() override;

	virtual void OnPathFinished(const FPathFollowingResult& Result) override;

	/**
	 * @brief Check if a point is containes in the agent. Execute only on server. on client i returns false
	 * @param point The point to check
	 * @return true if the point is contained inside of the capsule's shape
	 */
	bool AgentCapsuleContainsPoint(const FVector point);

protected:

	/**
	 * @brief Update Path logic
	 * @param delta delta time
	 */
	void UpdatePath(float delta);

	/**
	 * @brief Follow the path if any.
	 * @param delta delta time
	 */
	void FollowPath(float delta);


#pragma endregion

};


#pragma region Nav Events XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX



UCLASS(BlueprintType)
class MODULARCONTROLLER_API UPathFollowEvent : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:

	UPROPERTY(BlueprintAssignable)
	FOnPathReached OnPathReached;

	UPROPERTY(BlueprintAssignable)
	FOnPathFailed OnPathFailed;

	UPROPERTY(BlueprintAssignable)
	FOnPathUpdated OnPathUpdated;


	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;


	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "AI|Modular AI Movement")
	static UPathFollowEvent* ModularAIMoveTo(const UObject* WorldContextObject, UNavigationControlerComponent* controller, FVector location, TSubclassOf<UNavigationQueryFilter> filter = nullptr);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "AI|Modular AI Movement")
	static UPathFollowEvent* ModularAIFollow(const UObject* WorldContextObject, UNavigationControlerComponent* controller, AActor* target, TSubclassOf<UNavigationQueryFilter> filter = nullptr);


private:

	UFUNCTION()
	void _OnPathReached();
	UFUNCTION()
	void _OnPathFailed();
	UFUNCTION()
	void _OnPathUpdated();

	void CleanUp();

	const UObject* WorldContextObject;

	UPROPERTY()
	UNavigationControlerComponent* _controller;
	FVector _destination;

	UPROPERTY()
	AActor* _target;
	bool _targetMode;

	FTimerHandle TimerHandle_OnInstantFinish;
};

#pragma endregion