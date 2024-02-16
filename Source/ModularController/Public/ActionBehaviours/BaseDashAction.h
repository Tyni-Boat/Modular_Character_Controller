// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "ComponentAndBase/BaseControllerAction.h"
#include "BaseDashAction.generated.h"



UCLASS(BlueprintType, Blueprintable, ClassGroup = "Controller Action Behaviours", abstract)
class MODULARCONTROLLER_API UBaseDashAction : public UBaseControllerAction
{
	GENERATED_BODY()

protected:

	UBaseDashAction();

	/// <summary>
	/// The behaviour key name
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Base")
	FName BehaviourName = "DashAction";

	/// <summary>
	/// The behaviour priority
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Base")
	int BehaviourPriority = 7;

#pragma region Check
protected:

	/// <summary>
	/// The Name of the Dash command
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Inputs")
	FName DashInputCommand;

	/// <summary>
	/// The Name of the Dash location input. this is the location where the controller will try to Dash to. If a value is set and not used, the controller will always try to Dash to zero location.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Inputs")
	FName DashLocationInput;

	//------------------------------------------------------------------------------------------



public:

	/// <summary>
	/// Check for a Dash
	/// </summary>
	/// <param name="controller"></param>
	/// <returns></returns>
	bool CheckDash(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs, const float inDelta, UModularControllerComponent* controller);


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
	
	/// <summary>
	/// The dash distance
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dash Parameters")
	float DashDistance = 1000;

	/// <summary>
	/// The delai to start the dash
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dash Parameters", meta = (ClampMin = "0.0", UIMin = "0.0")) //Limit to only positive values; only inferior to duration
		double DashPropulsionDelay = 0.1;

	/// <summary>
	/// Rotation speed we turn toward the jump direction
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dash Parameters")
	bool bTurnTowardDashDirection = false;

	/// <summary>
	/// Rotation speed we turn toward the jump direction
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dash Parameters", meta=(EditCondition="bTurnTowardDashDirection"))
	bool bUseFourDirectionnalDash = false;

	/// <summary>
	/// The forward dash animation.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dash Parameters|Animation")
	FActionMotionMontage FwdDashMontage;

	/// <summary>
	/// The backward dash animation.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dash Parameters|Animation")
	FActionMotionMontage BackDashMontage;
	
	/// <summary>
	/// The left side dash animation.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dash Parameters|Animation")
	FActionMotionMontage LeftDashMontage;
	
	/// <summary>
	/// The right side dash animation.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dash Parameters|Animation")
	FActionMotionMontage RightDashMontage;

	/// <summary>
	/// The montage should be played on the current state's linked animation graph or on the root graph
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dash Parameters")
	bool bMontageShouldBePlayerOnStateAnimGraph;

	/// <summary>
	/// Should the montage be used as action duration?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dash Parameters")
	bool bUseMontageDuration;


	//------------------------------------------------------------------------------------------
	
	FVector _dashToLocation;
	FVector _dashToLocation_saved;

	FVector _propulsionLocation;
	FVector _propulsionLocation_saved;

	float _dashDelayTimer;
	float _dashDelayTimer_saved;

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

public:
	
	/// <summary>
	/// Called when the first frame the propulsion Start
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "Dash Action|Events")
	void OnPropulsionOccured();

#pragma endregion

#pragma region Functions
public:

	virtual int GetPriority_Implementation() override;

	virtual FName GetDescriptionName_Implementation() override;


	virtual bool CheckAction_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs, UModularControllerComponent* controller, const float inDelta) override;

	virtual FVelocity OnActionProcess_Implementation(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput
		, UModularControllerComponent* controller, const float inDelta) override;


	virtual	void OnStateChanged_Implementation(UBaseControllerState* newState, UBaseControllerState* oldState) override;

	virtual void OnActionEnds_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta) override;

	virtual void OnActionBegins_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta) override;

	virtual void SaveActionSnapShot_Internal() override;

	virtual void RestoreActionFromSnapShot_Internal() override;

#pragma endregion
};
