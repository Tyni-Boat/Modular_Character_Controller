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

	// The State's unique name
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base")
	FName StateName = "InAir";

	// The State's priority
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base")
	int StatePriority = 0;

	// The maximun velocity the user can move planar to the gravity force
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Air Control")
	float AirControlSpeed = 300;

	// The Acceleration toward air control speed. Also serve as air drag for horizontal moves
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Air Control")
	float AirControlAcceleration = 5;

	// In air controlled user rotation speed
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Air Control")
	float AirControlRotationSpeed = 3;

	// The Gravity force
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Gravity")
	FVector Gravity = FVector(0, 0, -981);

	// The maximum fall speed, in cm/s
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Gravity")
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

	//Set new gravity force.
	UFUNCTION(BlueprintCallable, Category="Gravity Control")
	void SetGravityForce(FVector newGravity);
	

#pragma endregion

#pragma region Functions
public:

	virtual int GetPriority_Implementation() override;

	virtual FName GetDescriptionName_Implementation() override;


	virtual  bool CheckState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs
		, UModularControllerComponent* controller, FStatusParameters controllerStatusParam, FStatusParameters& currentStatus, const float inDelta, int overrideWasLastStateStatus) override;

	virtual void OnEnterState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta) override;
	
	virtual FVelocity ProcessState_Implementation(FStatusParameters controllerStatusParam, FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta) override;

	virtual void OnExitState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta) override;
	
	virtual FString DebugString() override;


	virtual	void OnControllerStateChanged_Implementation(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller) override;


	virtual void SaveStateSnapShot_Internal() override;
	virtual void RestoreStateFromSnapShot_Internal() override;

#pragma endregion

};
