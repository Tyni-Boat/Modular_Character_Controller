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
	 * @return the closest direction to the desired dir.
	 */
	FVector GetFourDirectionnalVector(FTransform bodyTransform, FVector desiredDir, ESixAxisDirectionType& directionEnum) const;

	// Get the direction from index. 1-fwd, 2-back, 3-left, 4-right.
	FVector GetFourDirectionnalVectorFromIndex(FTransform bodyTransform, const ESixAxisDirectionType directionEnum) const;

#pragma endregion

#pragma region Dash
protected:
	// The State's Root motion Mode
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Dash Parameters")
	ERootMotionType RootMotionMode;
	
	// The maximum surface angle from plane defined by gravity vector if any
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters")
	float MaxSurfaceAngle = 40;

	// The dash speed
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters")
	float DashSpeed = 500;

	// The normalized Dash curve [0-1]
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters")
	FAlphaBlend DashCurve;

	// Should the dash conserve the current controller rotation?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Dash Parameters", meta = (EditCondition = "!bTurnTowardDashDirection"))
	bool bUseFourDirectionnalDash = false;

#pragma endregion

#pragma region Functions
public:
	virtual FControllerCheckResult CheckAction_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta,
	                                                          bool asLastActiveAction) const override;

	virtual FVector4 OnActionBegins_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput,
	                                              const float delta) const override;

	virtual void OnActionEnds_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput,
	                                         const float delta) const override;

	virtual FControllerStatus OnActionProcessAnticipationPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos,
	                                                                          const float delta) const override;

	virtual FControllerStatus OnActionProcessActivePhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos,
	                                                                    const float delta) const override;

	virtual FControllerStatus OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos,
	                                                                      const float delta) const override;


#pragma endregion
};
