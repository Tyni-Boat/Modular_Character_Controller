// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "ComponentAndBase/BaseControllerAction.h"
#include "BaseDashAction.generated.h"



UCLASS(BlueprintType, Blueprintable, ClassGroup = "Controller Action Behaviours", abstract)
class MODULARCONTROLLER_API UBaseDashAction : public UBaseControllerAction
{
	GENERATED_BODY()

protected:
	
	// The Action unique name
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base")
	FName ActionName = "DashAction";

	// The action's priority
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base")
	int ActionPriority = 7;

#pragma region Check
protected:

	//[Button] The Name of the button Dash command.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Inputs")
	FName DashInputCommand;

	//[Axis] The Name of the Axis Dash location input. this is the location where the controller will try to Dash to. If a value is set and not used, the controller will always try to Dash to zero location.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Inputs")
	FName DashLocationInput;

	//------------------------------------------------------------------------------------------



public:

	/// <summary>
	/// Check for a Dash
	/// </summary>
	/// <param name="controller"></param>
	/// <returns></returns>
	bool CheckDash(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs, FStatusParameters controllerStatusParam,
		FStatusParameters& currentStatus, const float inDelta, UModularControllerComponent* controller);


	/**
	 * @brief Get the closest direction on transform to the desired direction.
	 * @param bodyTransform The transform from wich calculate the four directionnal
	 * @param desiredDir the desired direction
	 * @param directionIndex the index of the direction: 0-noDir, 1-fwd, 2-back, 3-left, 4-right
	 * @return the closest direction to the desired dir.
	 */
	FVector GetFourDirectionnalVector(FTransform bodyTransform, FVector desiredDir, int& directionIndex);

#pragma endregion

#pragma region Dash
protected:
	
	// The dash distance
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters")
	float DashDistance = 1000;
	

	// Rotation speed we turn toward the Dash direction
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters")
	bool bTurnTowardDashDirection = false;

	// Should the dash conserve the current controller rotation?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters", meta=(EditCondition="bTurnTowardDashDirection"))
	bool bUseFourDirectionnalDash = false;

	// The forward dash animation.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters|Animation")
	FActionMotionMontage FwdDashMontage;

	// The backward dash animation.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters|Animation")
	FActionMotionMontage BackDashMontage;
	
	// The left side dash animation.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters|Animation")
	FActionMotionMontage LeftDashMontage;
	
	// The right side dash animation.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters|Animation")
	FActionMotionMontage RightDashMontage;

	// The montage should be played on the current state's linked animation blueprint or on the root skeletal mesh anim blueprint
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters")
	bool bMontageShouldBePlayerOnStateAnimGraph;

	// Should the montage lenght be used as action duration?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters")
	bool bUseMontageDuration;


	//------------------------------------------------------------------------------------------
	
	FVector _dashToLocation;
	FVector _dashToLocation_saved;

	FVector _propulsionLocation;
	FVector _propulsionLocation_saved;

	bool _dashed;
	bool _dashed_saved;

	FQuat _initialRot;
	FQuat _initialRot_saved;


	/// <summary>
	/// The end montage delegate.
	/// </summary>
	FOnMontageEnded _EndDelegate;


	/// <summary>
	/// Called at the end of the montage.
	/// </summary>
	/// <param name="Montage"></param>
	/// <param name="bInterrupted"></param>
	void OnAnimationEnded(UAnimMontage* Montage, bool bInterrupted);


#pragma endregion

#pragma region Functions
public:

	virtual int GetPriority_Implementation() override;

	virtual FName GetDescriptionName_Implementation() override;


	virtual bool CheckAction_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs, UModularControllerComponent* controller
		, FStatusParameters controllerStatusParam, FStatusParameters& currentStatus, const float inDelta) override;


	virtual FVelocity OnActionProcessAnticipationPhase_Implementation(FStatusParameters controllerStatusParam, FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput, UModularControllerComponent* controller, const float inDelta) override;

	virtual FVelocity OnActionProcessActivePhase_Implementation(FStatusParameters controllerStatusParam, FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput
		, UModularControllerComponent* controller, const float inDelta) override;

	virtual FVelocity OnActionProcessRecoveryPhase_Implementation(FStatusParameters controllerStatusParam, FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput, UModularControllerComponent* controller, const float inDelta) override;


	virtual	void OnStateChanged_Implementation(UBaseControllerState* newState, UBaseControllerState* oldState) override;

	virtual void OnActionEnds_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller,
		FStatusParameters controllerStatusParam, FStatusParameters& currentStatus, const float inDelta) override;

	virtual void OnActionBegins_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller,
		FStatusParameters controllerStatusParam, FStatusParameters& currentStatus, const float inDelta) override;

	virtual void SaveActionSnapShot_Internal() override;

	virtual void RestoreActionFromSnapShot_Internal() override;

#pragma endregion
};
