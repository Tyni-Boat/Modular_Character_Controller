// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "ComponentAndBase/BaseState.h"

#include "GroundState.generated.h"




///<summary>
 /// The ability to move on the ground for the kinematic controller component
 ///</summary>
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular State Behaviours", abstract)
class MODULARCONTROLLER_API UGroundState : public UBaseState
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

#pragma region Check
protected:

	/// <summary>
	/// The check shape
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main")
		TEnumAsByte<EShapeMode> CheckShape = EShapeMode::ShapeMode_Sphere;

	/// <summary>
	/// The check offset
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main")
		FVector CheckOffset = FVector(0);

	/// <summary>
	/// The check radius
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main")
		float CheckRadius = 30;

	/// <summary>
	/// The maximum distance to check for the ground.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main")
		float MaxCheckDistance = 10;

	/// <summary>
	/// After leaving a surface, the delay in seconds before the controller will be able to land again. 
	/// to prevent rapid takeOff-Landing repetition scenarios.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main")
		float CheckDelay = 0.1f;

	/// <summary>
	/// Should we hit complex colliders?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main")
		bool CanTraceComplex = false;

	/// <summary>
	/// The collision Channel.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main")
		TEnumAsByte<ETraceTypeQuery> ChannelGround;

	/// <summary>
	/// The ground state type.
	/// </summary>
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, category = "Main")
		TEnumAsByte<EGroundStateMode> GroundState = EGroundStateMode::GroundStateMode_No_Ground;

	//------------------------------------------------------------------------------------------

	//the check delay chrono.
	float _checkDelayChrono;

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



#pragma region Surface
protected:

	/// <summary>
	/// The default surface grip ratio
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Surface") 
		float SurfaceGripRatio = 0.75f;

	/// <summary>
	/// The maximum angle allowed for a surface
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Surface") 
		float MaxSurfaceAngle = 44;

	/// <summary>
	/// The offset from center allowed for a surface. use [0-0.99] for ratio and 1+ for values
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Surface") 
		float MaxSurfaceOffsetRatio = 1;

	/// <summary>
	/// The surface cone angle, to keep the normal calculus inside of a cone
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Surface") 
		float NormalConeAngle = 5;


#pragma endregion



#pragma region Movement
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
	/// The movement decceleration
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement") 
		float Decceleration = 0.01f;

	/// <summary>
	/// The speed at wich the target component be rotated with movement direction
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Movement") 
		float TurnSpeed = 8;

	/// <summary>
	/// The current speed ratio. current speed / Max speed
	/// </summary>
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, category = "Movement")
		float CurrentSpeedRatio;

	//the direction the user want to move
	FVector _userMoveDirection;

	//the vertical momentum when landing
	FVector _landingVelocity;


public:

	/// <summary>
	/// Move the controller on the ground
	/// </summary>
	/// <param name="controller"></param>
	/// <param name="delta"></param>
	/// <param name="outUserInputVector">The user input vector direction</param>
	/// <returns></returns>
	virtual FVector MoveOnTheGround(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, const float inDelta);

#pragma endregion



#pragma region Snapping
protected:

	/// <summary>
	/// The snap to ground height
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Snapping")
		float SnapToSurfaceDistance = 130;

	/// <summary>
	/// The snap up to ground multiplier. Should be [0-1].
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Snapping")
		float SnapToSurfaceUpSpeed = 0.25f;

	/// <summary>
	/// The snap down to ground multiplier. Should be [0-1].
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Snapping")
		float SnapToSurfaceDownSpeed = 0.25f;


	//active when the snap up occurs at least one time
	bool _touchedGroundReal = false;

	//The snap hit point
	FVector _snapVector;



	/// <summary>
	/// Snap the controller to the ground
	/// </summary>
	FVector SnapToGround(const FVector hitPoint, const FKinematicInfos& inDatas, UModularControllerComponent* controller);

#pragma endregion



#pragma region Slide
protected:

	/// <summary>
	/// The surface slide speed multiplier
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Slide") float SlidingSpeed = 400;

	/// <summary>
	/// The surface slide acceleration
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Slide") float SlidingAcceleration = 0.5f;
	

public:

	/// <summary>
	/// Slide on a slope
	/// </summary>
	/// <param name="controller"></param>
	/// <param name="delta"></param>
	/// <returns></returns>
	virtual FVector SlideOnTheGround(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, const float inDelta);

#pragma endregion



#pragma region Steping
protected:

	/// <summary>
	/// The maximum height of a step.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Stepping")
		float MaxStepHeight = 50;

	/// <summary>
	/// The maximum step up distance
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Stepping")
		float MaxStepUpDistance = 5;

#pragma endregion



#pragma region Functions
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
