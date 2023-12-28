// Copyright � 2023 by Tyni Boat. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentAndBase/BaseState.h"
#include "SimpleGroundState.generated.h"

/**
 * The SImple ground base movement state using component shape.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular State Behaviours", abstract)
class MODULARCONTROLLER_API USimpleGroundState : public UBaseState
{
	GENERATED_BODY()

protected:

	/// <summary>
	/// The behaviour key name.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Base")
	FName BehaviourName = "OnGround_Simple";

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
	float FloatingGroundDistance = 1;

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
	bool CanTraceComplex = false;

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
	virtual bool CheckSurface(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

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


#pragma region Move XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
protected:

	/// <summary>
	/// The name of the movement input
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement")
	FName MovementInputName = "Move";

	/// <summary>
	/// The maximum speed of the controller on the surface
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement")
	float MaxMoveSpeed = 260;

	/// <summary>
	/// The movement acceleration
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement")
	float Acceleration = 3;

	/// <summary>
	/// The movement deceleration
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement")
	float Deceleration = 0.01f;

	/// <summary>
	/// The speed at wich the target component be rotated with movement direction
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement")
	float TurnSpeed = 8;

public:

	/**
	 * @brief Move the controller on the ground
	 * @param inDatas inputs movement data
	 * @param inputs controller inputs
	 * @param inDelta delta time to process
	 * @return vector corresponding to the linear movement
	 */
	virtual FVector MoveOnTheGround(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, const float inDelta, FQuat& modRotation);

#pragma endregion


#pragma region Functions XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
public:

	virtual int GetPriority_Implementation() override;

	virtual FName GetDescriptionName_Implementation() override;


	virtual void StateIdle_Implementation(UModularControllerComponent* controller, const float inDelta) override;

	virtual bool CheckState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta) override;

	virtual void OnEnterState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta) override;

	virtual FVelocity ProcessState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta) override;

	virtual void OnExitState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta) override;

	virtual	void OnBehaviourChanged_Implementation(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller) override;

	virtual FString DebugString() override;

	void ComputeFromFlag_Implementation(int flag) override;

#pragma endregion
};
