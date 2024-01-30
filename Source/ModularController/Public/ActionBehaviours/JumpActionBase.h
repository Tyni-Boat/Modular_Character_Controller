// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "Animation/AnimMontage.h"
#include "ComponentAndBase/BaseAction.h"
#include "JumpActionBase.generated.h"



///<summary>
/// The abstract basic state behaviour for a Modular controller.
/// </summary>
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Controller Action Behaviours", abstract)
class MODULARCONTROLLER_API UJumpActionBase : public UBaseAction
{
	GENERATED_BODY()

protected:

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main") 
		FName JumpInputCommand;

	/// <summary>
	/// Consume the jump input?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main") 
		bool bConsumeJumpInput = true;

	/// <summary>
	/// The number of jumps it's possible to do
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main")
		int MaxJumpCount;

	/// <summary>
	/// The maximum Jump height
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main") 
		float MaxJumpHeight = 200;

	/// <summary>
	/// The maximum Jump Distance
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main") 
		float MaxJumpDistance = 150;


	/// <summary>
	/// Should the controller apply force below when jumping?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Main") 
		bool UsePhysicOnInterractions = true;

	//------------------------------------------------------------------------------------------



public:

	/// <summary>
	/// Check for a jump input or jump state
	/// </summary>
	/// <param name="controller"></param>
	/// <returns></returns>
	virtual bool CheckJump(const FKinematicInfos& inDatas, FInputEntryPool& inputs, const float inDelta);

	/// <summary>
	/// Check if we are jumping
	/// </summary>
	/// <param name="controller"></param>
	/// <returns></returns>
	FORCEINLINE bool IsJUmping() { return _jumpChrono > 0; }

	/// <summary>
	/// Check if we can jump
	/// </summary>
	/// <returns></returns>
	FORCEINLINE bool CanJump() { return !IsJUmping() && _jumpCount < MaxJumpCount; }

#pragma endregion

#pragma region Steering

	/// <summary>
	/// The name of the movement input
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Steering")
		FName DirectionInputName;

	/// <summary>
	/// Rotation speed we turn toward the jump direction
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Steering") 
		float TurnTowardDirectionSpeed = 15;

#pragma endregion

#pragma region Jump
protected:

	/// <summary>
	/// The time we should rest on a surface before jumps again.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Jump Params")
		TArray<FActionMotionMontage> Montages;

	/// <summary>
	/// The time we should rest on a surface before jumps again.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Jump Params") 
		float LandCoolDownTime = 1;

	/// <summary>
	/// Should we use the momentum instead of jump distance?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Jump Params") 
		bool UseMomentum;

	/// <summary>
	/// Should we use the surface normal when no directionnal input is given?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Jump Params") 
		bool UseSurfaceNormalOnNoDirection;


	//------------------------------------------------------------------------------------------

	//The chrono since the last jump
	float _jumpChrono;

	//The chrono since we landed
	float _landChrono;

	//the total number of jump in a row
	int _jumpCount;

	//the normal of the surface we jumped from
	FVector _jumpSurfaceNormal;

	//the jump force
	FVector _jumpForce;

	//the last jump force
	FVector _lastJumpVector;

	//Detects surface switch during this jump
	bool _haveSwitchedSurfaceDuringJump;

	//only active the frame the jump occurs
	bool _justJumps;


public:


	/// <summary>
	/// Execute a jump Move
	/// </summary>
	/// <param name="controller"></param>
	/// <returns></returns>
	virtual FVector Jump(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, const float inDelta);

	/// <summary>
	/// Start a jump
	/// </summary>
	virtual void JumpStart();

	/// <summary>
	/// Called When Jumping
	/// </summary>
	virtual void OnJump(const FKinematicInfos& inDatas, FVector jumpForce);

	/// <summary>
	/// Ends a jump
	/// </summary>
	virtual void JumpEnd(FName surfaceName);

#pragma endregion

#pragma region Functions
public:

	virtual int GetPriority_Implementation() override;

	virtual FName GetDescriptionName_Implementation() override;


	void ActionIdle_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta) override;

	virtual bool CheckAction_Implementation(const FKinematicInfos& inDatas, FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta) override;

	virtual FVelocity OnActionProcess_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta) override;

	virtual	void OnActionRepeat_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta) override;

	virtual	void OnStateChanged_Implementation(UBaseControllerState* newState, UBaseControllerState* oldState) override;

	virtual void OnActionEnds_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta) override;


#pragma endregion

	
};
