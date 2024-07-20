// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "Animation/AnimMontage.h"
#include "ComponentAndBase/BaseControllerAction.h"
#include "Curves/CurveFloat.h"
#include "JumpActionBase.generated.h"


UCLASS(BlueprintType, Blueprintable, ClassGroup = "Controller Action Behaviours", abstract)
class MODULARCONTROLLER_API UJumpActionBase : public UBaseControllerAction
{
	GENERATED_BODY()

public:
	UJumpActionBase();

#pragma region Check
protected:
	//[Button] The Name of the jump Button Input
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Inputs")
	FName JumpInputCommand;

	//[Axis] The Name of the jump location Axis input. this is the location where the controller will try to land. If a value is set and not used, the controller will always try to jump at zero location.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Inputs")
	FName JumpLocationInput;

	//The map of mantling and vaulting surface params. will be scanned in the order and trigger a path event with the key at the first valid surface check. event fire only once during jump life time.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Inputs")
	TMap<FName, FSurfaceCheckParams> MantlingAndVaultingMap;


#pragma endregion

#pragma region Jump
protected:
	// The normalized Jump curve [0-1]
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "JumpTo Parameters")
	FAlphaBlend JumpCurve;

	// The Jump force scale
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "JumpTo Parameters")
	float JumpForce = 4450;


	// Rotation speed to turn toward the jump direction
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "JumpTo Parameters")
	float TurnTowardDirectionSpeed = 15;

	// Should the controller speed go back to zero during anticipation phase?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "JumpTo Parameters")
	bool bStopOnAnticipation = false;


public:
	/// <summary>
	/// Get the initial velocity required to jump to a location.
	/// </summary>
	/// <returns></returns>
	FVector VelocityJumpTo(const FControllerStatus startingConditions, const FVector gravity, const float inDelta, const FVector Location) const;


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
