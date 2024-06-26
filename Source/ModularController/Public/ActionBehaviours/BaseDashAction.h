// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "ComponentAndBase/BaseControllerAction.h"
#include "BaseDashAction.generated.h"



UCLASS(BlueprintType, Blueprintable, ClassGroup = "Controller Action Behaviours", abstract)
class MODULARCONTROLLER_API UBaseDashAction : public UBaseControllerAction
{
	GENERATED_BODY()

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
	bool CheckDash(UModularControllerComponent* controller) const;


	/**
	 * @brief Get the closest direction on transform to the desired direction.
	 * @param bodyTransform The transform from wich calculate the four directionnal
	 * @param desiredDir the desired direction
	 * @param directionIndex the index of the direction: 0-noDir, 1-fwd, 2-back, 3-left, 4-right
	 * @return the closest direction to the desired dir.
	 */
	FVector GetFourDirectionnalVector(FTransform bodyTransform, FVector desiredDir, int& directionIndex) const;

#pragma endregion

#pragma region Dash
protected:

	// The State's Root motion Mode
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Dash Parameters")
	TEnumAsByte<ERootMotionType> RootMotionMode;

	// The dash distance
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters")
	float DashDistance = 1000;


	// Rotation speed we turn toward the Dash direction
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters")
	bool bTurnTowardDashDirection = false;

	// Should the dash conserve the current controller rotation?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters", meta = (EditCondition = "!bTurnTowardDashDirection"))
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

#pragma endregion

#pragma region Functions
public:

	virtual FControllerCheckResult CheckAction_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta, bool asLastActiveAction) const override;

	virtual FVector OnActionBegins_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const override;

	virtual void OnActionEnds_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const override;

	virtual FControllerStatus OnActionProcessAnticipationPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const override;

	virtual FControllerStatus OnActionProcessActivePhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const override;

	virtual FControllerStatus OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const override;
	

#pragma endregion
};
