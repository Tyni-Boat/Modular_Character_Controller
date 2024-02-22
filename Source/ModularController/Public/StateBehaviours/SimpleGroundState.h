// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentAndBase/BaseControllerState.h"
#include "SimpleGroundState.generated.h"

/**
 * The SImple ground base movement state using component shape.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular State Behaviours", abstract)
class MODULARCONTROLLER_API USimpleGroundState : public UBaseControllerState
{
	GENERATED_BODY()

protected:


	// The State's unique name
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base")
	FName StateName = "OnGround";

	// The State's priority.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base")
	int StatePriority = 5;


#pragma region Check XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:
	
	// The distance the component will be floating above the ground
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	float FloatingGroundDistance = 10;

	// The inflation of the component while checking for ground
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	float HullInflation = -10;

	// The maximum distance to check for the ground. it prevent controller from leaving the ground when moving down stairs.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	float MaxCheckDistance = 10;
		
	// Ground check should check against complex collision?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	bool bCanTraceComplex = false;

	// The ground collision Channel.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Main")
	TEnumAsByte<ECollisionChannel> ChannelGround;
	

	//------------------------------------------------------------------------------------------
	

public:

	/// <summary>
	/// Check for a valid surface
	/// </summary>
	/// <param name="controller"></param>
	/// <returns></returns>
	virtual bool CheckSurface(const FTransform spacialInfos, const FVector gravityDir, UModularControllerComponent* controller, const float inDelta, bool useMaxDistance = false);

	/// <summary>
	/// Called when we land on a surface
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Ground Events")
	void OnLanding(FSurfaceInfos landingSurface, const FKinematicInfos& inDatas, const float delta);

	/// <summary>
	/// Called when we land on a surface
	/// </summary>
	virtual void OnLanding_Implementation(FSurfaceInfos landingSurface, const FKinematicInfos& inDatas, const float delta);

	/// <summary>
	/// Called when we take off from a surface
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Ground Events")
	void OnTakeOff(FSurfaceInfos landingSurface, const FKinematicInfos& inDatas);

	/// <summary>
	/// Called when we take off from a surface
	/// </summary>
	virtual void OnTakeOff_Implementation(FSurfaceInfos landingSurface, const FKinematicInfos& inDatas);

#pragma endregion

#pragma region Surface and Snapping XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

protected:

	//The current surface's infos.
	FHitResult t_currentSurfaceInfos;
	
public:

	/**
	 * @brief Compute snapping and return the required force.
	 * @param inDatas The input datas
	 * @return The instant force needed to snap the controller on the suarface at FloatingGroundDistance
	 */
	FVector ComputeSnappingForce(const FKinematicInfos& inDatas, UObject* debugObject = NULL) const;

#pragma endregion


#pragma region General Movement XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:

	//[Axis] The name of the lock-on input direction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement")
	FName LockOnDirection = "LockOnDirection";

	// Prevent the controller from falling off ledges
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement")
	bool IsPreventingFalling = false;

	// the speed at wich the controller absorb landing impacts. the higher the speed the shorter the time the controller get stuck on the ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement")
	float LandingImpactAbsorbtionSpeed = 2682;

	// the force threshold for the controller be able to move despite still absorbing landing impact
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement")
	float LandingImpactMoveThreshold = 981;

	// The landing impact force remaining. it decrease over time at Landing Impact Absorption Speed
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Movement")
	float LandingImpactRemainingForce;


public:

	/**
	 * @brief Move the controller on the ground
	 * @param inDatas inputs movement data
	 * @param inputs controller inputs
	 * @param inDelta delta time to process
	 * @param modRotation
	 * @param speedAndAcceleration
	 * @param turnSpeed
	 * @param changedState
	 * @return vector corresponding to the linear movement
	 */
	virtual FVector MoveOnTheGround(const FKinematicInfos& inDatas, FVector desiredMovement, const float acceleration, const float deceleration, const float inDelta);

	/**
	 * @brief Correct movement to prevent falling.
	 * @param controller
	 * @param inDatas input datas
	 * @param attemptedMove the move the controller will attempt to do. after MoveOnGround
	 * @param inDelta delta time
	 * @return the corrected move
	 */
	virtual FVector MoveToPreventFalling(UModularControllerComponent* controller, const FKinematicInfos& inDatas, const FVector attemptedMove, const float inDelta, FVector& adjusmentMove);

#pragma endregion

#pragma region SnapShot
private:

	float _landingImpactRemainingForce_saved;

	FVector_NetQuantize _lastControlledPosition;
	FVector_NetQuantize _lastControlledPosition_saved;

#pragma endregion


#pragma region Slope And Sliding XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:

	// The maximum surface inclination angle in degree
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Slope and Slide")
	float MaxSlopeAngle = 30;


	// The maximum speed of the controller on the surface while sliding
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Slope and Slide")
	float MaxSlidingSpeed = 981;

	// The movement acceleration while sliding
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Slope and Slide")
	float SlidingAcceleration = 35;

	// Should the slope increase or decrease speed when we are ascending and descending?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Slope and Slide")
	bool bSlopeAffectSpeed = false;

#pragma endregion


#pragma region Move XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:
	
	// The maximum speed of the controller on the surface
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Move Parameters")
	float MaxSpeed = 350;

	// The movement acceleration
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Move Parameters")
	float Acceleration = 27;

	// The movement deceleration
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Move Parameters")
	float Deceleration = 9;

	// The speed used to rotate toward movement direction
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Movement|Move Parameters")
	float TurnSpeed = 20;
	
#pragma endregion
	

#pragma region Functions XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
public:

	virtual int GetPriority_Implementation() override;

	virtual FName GetDescriptionName_Implementation() override;


	
	virtual bool CheckState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs, UModularControllerComponent* controller
		, const float inDelta, int overrideWasLastStateStatus) override;

	virtual void OnEnterState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta) override;


	virtual FVelocity ProcessState_Implementation(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta) override;
	
	virtual void OnExitState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta) override;

	virtual	void OnControllerStateChanged_Implementation(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller) override;

	virtual FString DebugString() override;


	virtual void SaveStateSnapShot_Internal() override;
	virtual void RestoreStateFromSnapShot_Internal() override;
	

#pragma endregion
};
