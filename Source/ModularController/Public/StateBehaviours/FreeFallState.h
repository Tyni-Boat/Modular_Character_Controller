// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "ComponentAndBase/BaseControllerState.h"
#include "FreeFallState.generated.h"



/**
 A Free fall behaviour for the Modular controller component
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular State Behaviours", abstract)
class MODULARCONTROLLER_API UFreeFallState : public UBaseControllerState
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
	/// The in air control max speed
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Air Control")
	float AirControlSpeed = 300;

	/// <summary>
	/// The in air control rotation speed
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Air Control")
	float AirControlRotationSpeed = 3;
	
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

	/// <summary>
	/// The maximum fall speed, in cm/s
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Gravity")
	float TerminalVelocity = 5364;

#pragma endregion

#pragma region LiveAttributes
private:
	//The time spend in the air
	float _airTime;

	//the gravity vector
	FVector _gravity;

#pragma endregion

#pragma region SnapShot
private:
	float _airTime_saved;
	FVector _gravity_saved;

#pragma endregion

#pragma region Air Velocity and Checks
public:


	/// <summary>
	/// To control the controller in the air
	/// </summary>
	/// <param name="delta"></param>
	/// <returns></returns>
	virtual FVector AirControl(FVector desiredMove, FVector horizontalVelocity, float delta);

	/// <summary>
	/// Apply Gravity force
	/// </summary>
	/// <param name="delta"></param>
	/// <returns></returns>
	virtual FVector AddGravity(FVector verticalVelocity, float delta);
	

	/// <summary>
	/// Get the time spend in the air.
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "Properties")
	FORCEINLINE float GetAirTime() { return _airTime; }
	

#pragma endregion

#pragma region Functions
public:

	virtual int GetPriority_Implementation() override;

	virtual FName GetDescriptionName_Implementation() override;


	virtual  bool CheckState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs
		, UModularControllerComponent* controller, const float inDelta, int overrideWasLastStateStatus) override;

	virtual void OnEnterState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta) override;
	
	virtual FVelocity ProcessState_Implementation(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta) override;

	virtual void OnExitState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta) override;
	
	virtual	void OnControllerStateChanged_Implementation(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller) override;


	virtual void SaveStateSnapShot_Internal() override;
	virtual void RestoreStateFromSnapShot_Internal() override;

#pragma endregion

};
