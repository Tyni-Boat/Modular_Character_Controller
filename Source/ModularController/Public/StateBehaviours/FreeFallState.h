// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "ComponentAndBase/BaseState.h"
#include "FreeFallState.generated.h"



/**
 A Free fall behaviour for the Modular controller component
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular State Behaviours", abstract)
class MODULARCONTROLLER_API UFreeFallState : public UBaseState
{
	GENERATED_BODY()

#pragma region Parameters
protected:

	/// <summary>
	/// The behaviour key name
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Base")
		FName BehaviourName = "InAir";

	/// <summary>
	/// The behaviour priority
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Base")
		int BehaviourPriority = 0;

	/// <summary>
	/// The maximum distance to check for ground distance
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Base")
		float MaxGroundDistDetection = 9999;

	/// <summary>
	/// The in air control max speed
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Air Control")
		float AirControlSpeed = 100;

	/// <summary>
	/// The in air control rotation speed
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Air Control")
		float AirControlRotationSpeed = 1;

	/// <summary>
	/// The in air control acceleration
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Air Control")
		float AirControlAcceleration = 3;

	/// <summary>
	/// The in air rotation speed
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Air Control")
		float AirControlRotationControl = 0.5f;

	/// <summary>
	/// The name of the movement input
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Air Control")
		FName MovementInputName = "Move";

	/// <summary>
	/// The Gravity
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Gravity")
		FVector Gravity = FVector(0, 0, -981);

#pragma endregion

#pragma region LiveAttributes
private:
	//The time spend in the air
	float _airTime;

	//The distance to the ground
	float _groundDistance;

	//the gravity vector
	FVector _gravity;

#pragma endregion


#pragma region Air Velocity and Checks
public:


	/// <summary>
	/// To control the controller in the air
	/// </summary>
	/// <param name="delta"></param>
	/// <returns></returns>
	virtual FVector AirControl(FVector inputAxis, FVector horizontalVelocity, float delta);

	/// <summary>
	/// Apply Gravity force
	/// </summary>
	/// <param name="delta"></param>
	/// <returns></returns>
	virtual FVector AddGravity(FVector verticalVelocity, float delta);


	/**
	 * @brief Check for the ground distance.
	 * @param controller
	 * @param delta
	 * @return
	 */
	void CheckGroundDistance(UModularControllerComponent* controller, const FVector inLocation, const FQuat inQuat);



	/// <summary>
	/// Get the time spend in the air.
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "Properties")
		FORCEINLINE float GetAirTime() { return _airTime; }

	/// <summary>
	/// Get The ground Distance
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "Properties")
		FORCEINLINE float GetGroundDistance() { return _groundDistance; }

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


#pragma endregion

};
