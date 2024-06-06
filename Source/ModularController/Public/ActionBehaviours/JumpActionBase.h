// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "Animation/AnimMontage.h"
#include "ComponentAndBase/BaseControllerAction.h"
#include "JumpActionBase.generated.h"



UCLASS(BlueprintType, Blueprintable, ClassGroup = "Controller Action Behaviours", abstract)
class MODULARCONTROLLER_API UJumpActionBase : public UBaseControllerAction
{
	GENERATED_BODY()
	
#pragma region Check
protected:

	//[Button] The Name of the jump Button Input
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Inputs")
	FName JumpInputCommand;

	//[Axis] The Name of the jump location Axis input. this is the location where the controller will try to land. If a value is set and not used, the controller will always try to jump at zero location.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Inputs")
	FName JumpLocationInput;


	// Should the controller apply force on current surface when jumping?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Jump Parameters")
	bool UsePhysicOnInteractions = true;

	//------------------------------------------------------------------------------------------



public:

	/// <summary>
	/// Check for a jump input or jump state
	/// </summary>
	/// <param name="controller"></param>
	/// <returns></returns>
	bool CheckJump(UModularControllerComponent* controller, const FVector currentPosition, const FQuat currentRotation, const FVector gravityDir) const;
	
#pragma endregion

#pragma region Jump
protected:

	// The maximum Jump height
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Jump Parameters")
	float MaxJumpHeight = 200;

	// The minimum distance from the ceiling for the jump to happen
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Jump Parameters")
	float MinJumpHeight = 50;

	// The maximum Jump Distance
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Jump Parameters")
	float MaxJumpDistance = 150;
	


	// Rotation speed to turn toward the jump direction
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Jump Parameters")
	float TurnTowardDirectionSpeed = 15;

	// The Montage to play when jumping
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Jump Parameters")
	FActionMotionMontage JumpMontage;

	// The montage should be played on the current state's linked animation blueprint or on the root skeletal mesh anim blueprint
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Jump Parameters")
	bool bMontageShouldBePlayerOnStateAnimGraph;
	
	// Should the montage lenght be used as action duration?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Jump Parameters")
	bool bUseMontageDuration;

	// Should we use the momentum instead of jump distance?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Jump Parameters")
	bool UseMomentum;

	// Should we use the surface normal when no directional input is given?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Jump Parameters")
	bool UseSurfaceNormalOnNoDirection;


	//------------------------------------------------------------------------------------------
		
	//the normal of the surface we jumped from
	FVector _jumpSurfaceNormal;
	FVector _jumpSurfaceNormal_saved;

	//the jump propulsion just occured
	bool _jumped;
	bool _jumped_saved;


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
	/// Execute a jump Move
	/// </summary>
	/// <returns></returns>
	FVector Jump(const FControllerStatus startingConditions, const FVector gravity, const float inDelta, const FVector customJumpLocation = FVector(NAN)) const;
	

#pragma endregion

#pragma region Functions
public:

	virtual FControllerCheckResult CheckAction_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta, bool asLastActiveAction) override;

	virtual FKinematicComponents OnActionBegins_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) override;

	virtual FKinematicComponents OnActionEnds_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) override;

	virtual FControllerStatus OnActionProcessAnticipationPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta) override;

	virtual FControllerStatus OnActionProcessActivePhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta) override;

	virtual FControllerStatus OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta) override;


	virtual void SaveActionSnapShot_Internal() override;

	virtual void RestoreActionFromSnapShot_Internal() override;

#pragma endregion


};

