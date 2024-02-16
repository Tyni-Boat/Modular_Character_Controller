// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "Animation/AnimMontage.h"
#include "ComponentAndBase/BaseControllerAction.h"
#include "JumpActionBase.generated.h"



UCLASS(BlueprintType, Blueprintable, ClassGroup = "Controller Action Behaviours", abstract)
class MODULARCONTROLLER_API UJumpActionBase : public UBaseControllerAction
{
	GENERATED_BODY()

protected:

	UJumpActionBase();

	/// <summary>
	/// The behaviour key name
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Base")
	FName BehaviourName = "JumpingAction";

	/// <summary>
	/// The behaviour priority
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Base")
	int BehaviourPriority = 6;

#pragma region Check
protected:

	/// <summary>
	/// The Name of the jump command
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Inputs")
	FName JumpInputCommand;

	/// <summary>
	/// The Name of the jump location input. this is the location where the controller will try to land. If a value is set and not used, the controller will always try to jump at zero location.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Inputs")
	FName JumpLocationInput;


	/// <summary>
	/// Should the controller apply force below when jumping?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Jump Parameters")
	bool UsePhysicOnInteractions = true;

	//------------------------------------------------------------------------------------------



public:

	/// <summary>
	/// Check for a jump input or jump state
	/// </summary>
	/// <param name="controller"></param>
	/// <returns></returns>
	bool CheckJump(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs, const float inDelta, UModularControllerComponent* controller);
	
#pragma endregion

#pragma region Jump
protected:

	/// <summary>
	/// The maximum Jump height
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Jump Parameters")
	float MaxJumpHeight = 200;

	/// <summary>
	/// The minimum distance from the ceiling for the jump to happen
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Jump Parameters")
	float MinJumpHeight = 50;

	/// <summary>
	/// The maximum Jump Distance
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Jump Parameters")
	float MaxJumpDistance = 150;
	
	/// <summary>
	/// The delai of the propulsion of the jump.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Jump Parameters", meta = (ClampMin = "0.0", UIMin = "0.0")) //Limit to only positive values; only inferior to duration
	double JumpPropulsionDelay = 0.1;

	/// <summary>
	/// Rotation speed we turn toward the jump direction
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Jump Parameters")
	float TurnTowardDirectionSpeed = 15;

	/// <summary>
	/// The array of animations to play per jump count.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Jump Parameters")
	FActionMotionMontage JumpMontage;

	/// <summary>
	/// The montage should be played on the current state's linked animation graph or on the root graph
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Jump Parameters")
	bool bMontageShouldBePlayerOnStateAnimGraph;
	
	/// <summary>
	/// Should the montage be used as action duration?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Jump Parameters")
	bool bUseMontageDuration;

	/// <summary>
	/// Should we use the momentum instead of jump distance?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Jump Parameters")
	bool UseMomentum;

	/// <summary>
	/// Should we use the surface normal when no directional input is given?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Jump Parameters")
	bool UseSurfaceNormalOnNoDirection;


	//------------------------------------------------------------------------------------------

	//The remaining time before the jump force occurs.
	float _jumpDelayTimer;
	float _jumpDelayTimer_saved;
	
	//the normal of the surface we jumped from
	FVector _jumpSurfaceNormal;
	FVector _jumpSurfaceNormal_saved;

	//the momentum when entered action
	FVelocity _startMomentum;
	FVelocity _startMomentum_saved;


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
	/// <param name="controller"></param>
	/// <returns></returns>
	FVector Jump(const FKinematicInfos inDatas, FVector moveInput, const FVelocity momentum, const float inDelta, FVector customJumpLocation = FVector(NAN));


	/// <summary>
	/// Called when the propulsion occured
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "Jump Action|Events")
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

