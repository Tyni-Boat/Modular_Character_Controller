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

	/// <summary>
	/// The behaviour key name.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Base")
	FName BehaviourName = "OnGround";

	/// <summary>
	/// The behaviour priority.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Base")
	int BehaviourPriority = 5;


#pragma region Check XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:
	
	/// <summary>
	/// The distance the compoenet will be floationg above the ground
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main")
	float FloatingGroundDistance = 10;

	/// <summary>
	/// The inflation of the component while checking
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main")
	float HullInflation = -10;

	/// <summary>
	/// The maximum distance to check for the ground if the last state was on ground. it prevent controller from leaving the ground when moving down stairs.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main")
	float MaxCheckDistance = 10;

	
	/// <summary>
	/// Should we hit complex colliders?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main")
	bool bCanTraceComplex = false;

	/// <summary>
	/// The collision Channel.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main")
	TEnumAsByte<ECollisionChannel> ChannelGround;
	

	//------------------------------------------------------------------------------------------
	

public:

	/// <summary>
	/// Check for a valid surface
	/// </summary>
	/// <param name="controller"></param>
	/// <returns></returns>
	virtual bool CheckSurface(const FKinematicInfos& inDatas, UModularControllerComponent* controller, const float inDelta, bool forceMaxDistance = false);

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
	FVector ComputeSnappingForce(const FKinematicInfos& inDatas) const;

#pragma endregion


#pragma region General Movement XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:

	/// <summary>
	/// The name of the movement input
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement")
	FName MovementInputName = "Move";

	/// <summary>
	/// The name of the lock-on input direction.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement")
	FName LockOnDirection = "LockOnDirection";

	/// <summary>
	/// Prevent the controller from falling
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement")
	bool IsPreventingFalling = false;

	/// <summary>
	/// the speed at wich the controller absorb landing impacts. the higher the speed the shorter the time the controller get stuck on the ground.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement")
	float LandingImpactAbsorbtionSpeed = 2682;

	/// <summary>
	/// the force threshold for the controller be able to move despite still absorbing landing impact
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement")
	float LandingImpactMoveThreshold = 981;

	/// <summary>
	/// The current ground state mode
	/// </summary>
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Movement")
	TEnumAsByte<EGroundLocomotionMode> CurrentLocomotionMode;

	/// <summary>
	/// The landing impact force remaining. it decrease over time at Landing Impact Absorbtion Speed
	/// </summary>
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
	virtual FVector MoveOnTheGround(const FKinematicInfos& inDatas, FVector desiredMovement, const float acceleration, const float decceleration, const float inDelta);

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


#pragma region Slope And Sliding XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:

	/// <summary>
	/// The maximum surface inclination angle in degree
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Slope and Slide")
	float MaxSlopeAngle = 30;


	/// <summary>
	/// The maximum speed of the controller on the surface while sliding
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Slope and Slide")
	float MaxSlidingSpeed = 981;

	/// <summary>
	/// The movement acceleration while sliding
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Slope and Slide")
	float SlidingAcceleration = 35;

	/// <summary>
	/// Should the slope increase or decrease speed when we are ascending and descending.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Slope and Slide")
	bool bSlopeAffectSpeed = false;

#pragma endregion


#pragma region Walk Jog XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:
	
	/// <summary>
	/// The maximum speed of the controller on the surface
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Walk Jog")
	float MaxJogSpeed = 350;

	/// <summary>
	/// The movement acceleration
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Walk Jog")
	float JogAcceleration = 27;

	/// <summary>
	/// The movement deceleration
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Walk Jog")
	float JogDeceleration = 9;

	/// <summary>
	/// The speed at wich the target component be rotated with movement direction
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Walk Jog")
	float JoggingTurnSpeed = 20;
	
#pragma endregion


#pragma region Sprint XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:
	
	/// <summary>
	/// The name of the sprint input value
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Sprinting")
	FName SprintInputName = "Sprinting";
	
	/// <summary>
	/// The minimum speed of the controller on the surface to start sprinting
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Sprinting")
	float MinSprintSpeed = 250;
	
	/// <summary>
	/// The maximum speed of the controller on the surface
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Sprinting")
	float MaxSprintSpeed = 750;

	/// <summary>
	/// The movement acceleration
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Sprinting")
	float SprintAcceleration = 27;

	/// <summary>
	/// The movement deceleration
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Sprinting")
	float SprintDeceleration = 9;

	/// <summary>
	/// The speed at wich the target component be rotated with movement direction
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Sprinting")
	float SprintingTurnSpeed = 12;
	
#pragma endregion


#pragma region Crouch XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:
	
	/// <summary>
	/// The name of the crouch input value
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Crouch")
	FName CrouchInputName = "Crouching";
	
	/// <summary>
	/// The maximum speed of the controller on the surface
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Crouch")
	float MaxCrouchSpeed = 180;

	/// <summary>
	/// The movement acceleration
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Crouch")
	float CrouchAcceleration = 27;

	/// <summary>
	/// The movement deceleration
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Crouch")
	float CrouchDeceleration = 9;

	/// <summary>
	/// The speed at wich the target component be rotated with movement direction
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Crouch")
	float CrouchTurnSpeed = 25;
	
#pragma endregion


#pragma region Crawl XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:
	
	/// <summary>
	/// The name of the Crawl input value
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Crawl")
	FName CrawlInputName = "Crawling";
	
	/// <summary>
	/// The maximum speed of the controller on the surface
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Crawl")
	float MaxCrawlSpeed = 90;

	/// <summary>
	/// The movement acceleration
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Crawl")
	float CrawlAcceleration = 27;

	/// <summary>
	/// The movement deceleration
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Crawl")
	float CrawlDeceleration = 9;

	/// <summary>
	/// The speed at wich the target component be rotated with movement direction
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement|Crawl")
	float CrawlTurnSpeed = 8;
	
#pragma endregion


#pragma region Functions XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
public:

	virtual int GetPriority_Implementation() override;

	virtual FName GetDescriptionName_Implementation() override;


	
	virtual bool CheckState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta) override;

	virtual void OnEnterState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta) override;

	virtual FMovePreprocessParams PreProcessState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta) override;

	virtual FVelocity ProcessState_Implementation(const FKinematicInfos& inDatas, const FMovePreprocessParams params, UModularControllerComponent* controller, const float inDelta) override;
	
	virtual void OnExitState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta) override;

	virtual	void OnControllerStateChanged_Implementation(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller) override;

	virtual FString DebugString() override;
	

#pragma endregion
};
