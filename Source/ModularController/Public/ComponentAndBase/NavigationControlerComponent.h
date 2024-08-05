// Copyright � 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "GameFramework/NavMovementComponent.h"
#include "NavigationSystem.h"
#include "NavLinkCustomInterface.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UObject/WeakObjectPtr.h"
#include "NavigationControlerComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPathReached, FAIRequestID, moveUID);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPathStarted, FAIRequestID, moveUID);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPathFailed, FAIRequestID, moveUID);


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
	/// activate the debug mode.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Controllers|Debug")
	bool IsDebug;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;


public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// get the current status of path following
	UFUNCTION(BlueprintPure, Category = "AI Controllers|Core")
	FORCEINLINE EPathFollowingStatus::Type GetPathFollowingStatus() const { return Status; }

	// Pause Path following
	UFUNCTION(BlueprintCallable, Category = "AI Controllers|Core")
	FORCEINLINE void PausePathFollowing()
	{
		PauseMove(GetCurrentRequestId());
		_explicitPathPause = true;
	}

	// Resume Path following
	UFUNCTION(BlueprintCallable, Category = "AI Controllers|Core")
	FORCEINLINE void ResumePathFollowing()
	{
		ResumeMove(GetCurrentRequestId());
		_explicitPathPause = false;
	}

#pragma endregion


#pragma region Search XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


	// Find Path to an actor, a location or the closest navigable location within a max distance.
	int SearchPath(AActor* target, FVector location, float maxOffNavDistance, TSubclassOf<UNavigationQueryFilter> filter);


	// Cancel any path finding
	UFUNCTION(BlueprintCallable, Category = "AI Controllers|Core")
	void CancelPath();


	/// <summary>
	/// Execute a la fin d'un path.
	/// </summary>
	/// <param name="EPathFollowingResult"></param>
	void OnPathEnds(FAIRequestID requestID, uint32 EPathFollowingResult);


#pragma endregion


#pragma region Path Requests and Follow XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:
	//the maximum projection distance to navMesh
	float MaxPointProjection = 10000;

	bool _explicitPathPause = false;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Controllers")
	float AgentHeight = 180;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Controllers")
	float AgentRadius = 50;

	// When offset from the path, the minimum segment lenght to try to return to the path
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Controllers")
	float MinimumBackToPathSegmentLeght = 300;

	// the distance from the end of a segment to begin smoothing directions
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Controllers")
	float SmoothDirectionThreshold = 50;

	// the minimum angle to smooth path.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Controllers", meta=(UIMin=0, UIMax = 90, ClampMin = 0, ClampMax = 90))
	float SmoothAngleThreshold = 5;

	// the smoothing density
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Controllers", meta=(UIMin=0, UIMax = 1, ClampMin = 0, ClampMax = 1))
	float SmoothStep = 0.1;

	// How much should we reduce the speed when cornering?
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Controllers", meta=(UIMin=0, UIMax = 0.99, ClampMin = 0, ClampMax = 0.99))
	float CorneringSpeedReduction = 0.5;

	// the smoothing curve
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Controllers")
	EAlphaBlendOption SmoothCurve = EAlphaBlendOption::Linear;


	// The direction of the movement to follow the path
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AI Controllers")
	FVector PathVelocity;

	// the offset of the actor location from it actual location in the navigation
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AI Controllers")
	FVector NavigationOffset;

	// Active if we have a valid path and our status is "moving"
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI Controllers")
	bool IsFollowingAPath;

	// Total lenght of the path
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI Controllers")
	float PathRemainingLenght = 0;

	// Remaining lenght of a path
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI Controllers")
	float PathTotalLenght = 0;

	// The Lenght of the current path segment
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI Controllers")
	float PathCurrentSegmentLenght = 0;

	// Remaining lenght the current segment
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI Controllers")
	float PathCurrentSegmentRemainingLenght = 0;


	UPROPERTY(BlueprintAssignable)
	FOnPathReached OnPathReachedEvent;

	UPROPERTY(BlueprintAssignable)
	FOnPathFailed OnPathFailedEvent;

	UPROPERTY(BlueprintAssignable)
	FOnPathStarted OnPathStartedEvent;

protected:
	//the request queue
	TQueue<FAIRequestID> _activePathQueue;

	//The queue of path request async waiting to the processed.
	TQueue<TTuple<uint32, TSoftObjectPtr<AActor>>> _asyncPathRequestQueue;

	//The queue of path processed async waiting to be start on the main thread.
	TQueue<TTuple<uint32, TTuple<FAIMoveRequest, FNavPathSharedPtr>>> _asyncPathResponseQueue;

	//The array of path curves, containing starts index as X and end index as Y and Z the angle in degrees of the corner
	TArray<FVector> _curvesMap;


public:
	/// <summary>
	/// Request an AI path to a destination.
	/// </summary>
	/// <param name="location"></param>
	/// <param name="filter"></param>
	/// <returns></returns>
	int AIRequestPathTo(FVector location, float maxOffNavDistance = 1000, TSubclassOf<UNavigationQueryFilter> filter = nullptr);

	/// <summary>
	/// Request an AI path to an actor.
	/// </summary>
	/// <param name="target"></param>
	/// <param name="filter"></param>
	/// <returns></returns>
	int AIRequestPathToActor(AActor* target, float maxOffNavDistance = 1000, TSubclassOf<UNavigationQueryFilter> filter = nullptr);


	virtual void OnPathFinished(const FPathFollowingResult& Result) override;


	virtual FAIRequestID RequestMove(const FAIMoveRequest& RequestData, FNavPathSharedPtr InPath) override;


protected:
	//Called when an async path calculation ends.
	void OnAsyncPathEvaluated(uint32 aPathId, ENavigationQueryResult::Type aResultType, FNavPathSharedPtr aNavPointer);

	// Look for computed paths and start them on the main thread.
	void UpdateStartPath();

	// Calculate the current path remaining distance.
	void CalculatePathRemainingLenght();

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


	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;


	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "3"), Category = "AI|Modular AI Movement")
	static UPathFollowEvent* ModularAIMoveTo(const UObject* WorldContextObject, UNavigationControlerComponent* controller, FVector location, float maxOffNavDistance = 1000
	                                         , TSubclassOf<UNavigationQueryFilter> filter = nullptr);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "3"), Category = "AI|Modular AI Movement")
	static UPathFollowEvent* ModularAIFollow(const UObject* WorldContextObject, UNavigationControlerComponent* controller, AActor* target, float maxOffNavDistance = 1000
	                                         , TSubclassOf<UNavigationQueryFilter> filter = nullptr);


private:
	UFUNCTION()
	void _OnPathReached(FAIRequestID requestID);
	UFUNCTION()
	void _OnPathFailed(FAIRequestID requestID);

	void CleanUp();


	const UObject* WorldContextObject;

	UPROPERTY()
	UNavigationControlerComponent* _controller;
	FVector _destination;
	float _offNavDistance;
	TSubclassOf<UNavigationQueryFilter> _navFilter = nullptr;

	UPROPERTY()
	AActor* _target;
	bool _targetMode;

	FTimerHandle TimerHandle_OnInstantFinish;
	FAIRequestID pathID = FAIRequestID::InvalidRequest;
};

#pragma endregion
