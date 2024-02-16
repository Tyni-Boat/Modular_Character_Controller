// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "Animation/AnimMontage.h"
#ifndef MODULAR_CONTROLLER_COMPONENT
#define MODULAR_CONTROLLER_COMPONENT
#include "ModularControllerComponent.h"
#endif
#ifndef BASE_STATE
#define BASE_STATE
#include "BaseControllerState.h"
#endif
#include "Structs.h"
#include "Engine/DataAsset.h"
#include "BaseControllerAction.generated.h"



///<summary>
/// The abstract basic Action behaviour for a Modular controller.
/// </summary>
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular Action Behaviours", abstract)
class MODULARCONTROLLER_API UBaseControllerAction : public UDataAsset
{
	GENERATED_BODY()

public:

	/// <summary>
	/// The action duration
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|Timing")
	float Duration = 0.15f;

	/// <summary>
	/// The action cool down delay. the duration the action cannot be done again.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|Timing")
	float CoolDownDelay = 0.25f;

	/// <summary>
	/// The behaviour Root motion Mode
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|Motion")
	TEnumAsByte<ERootMotionType> RootMotionMode;

	/// <summary>
	/// The action's flag, often used as binary. to relay this actions's state over the network.
	/// </summary>
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, category = "Action|Base|Motion")
	int ActionFlag;

	/// <summary>
	/// The controller state must be frozen to it current state?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|State & Compatibility")
	bool FreezeCurrentState;

	/// <summary>
	/// The operation of checking states on the controller should be overriden during this action?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|State & Compatibility")
	bool bShouldControllerStateCheckOverride;

	/// <summary>
	/// The Action only execute on compatible state?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|State & Compatibility")
	TEnumAsByte<EActionCompatibilityMode> ActionCompatibilityMode;

	/// <summary>
	/// The list of compatible states
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|State & Compatibility"
		, meta = (EditCondition = "ActionCompatibilityMode == EActionCompatibilityMode::OnCompatibleStateOnly || ActionCompatibilityMode == EActionCompatibilityMode::OnBothCompatiblesStateAndAction"))
	TArray<FName> CompatibleStates;

	/// <summary>
	/// The list of compatible actions
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|State & Compatibility"
		, meta = (EditCondition = "ActionCompatibilityMode == EActionCompatibilityMode::WhileCompatibleActionOnly || ActionCompatibilityMode == EActionCompatibilityMode::OnBothCompatiblesStateAndAction"))
	TArray<FName> CompatibleActions;
	
	/// <summary>
	/// Enable or disable debug
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Action|Base|Debug")
	bool bDebugAction;





	/// <summary>
	/// When we enters the action behaviour.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
	void OnActionBegins(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);


	UFUNCTION(BlueprintCallable, Category = "Action|Base Events|C++ Implementation")
	virtual void OnActionBegins_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);


	/// <summary>
	/// When we exit the action.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
	void OnActionEnds(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);

	UFUNCTION(BlueprintCallable, Category = "Action|Base Events|C++ Implementation")
	virtual void OnActionEnds_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// <summary>
	/// Check if the action is Valid
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
	bool CheckAction(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs, UModularControllerComponent* controller, const float inDelta);


	UFUNCTION(BlueprintCallable, Category = "Action|Base Events|C++ Implementation")
	virtual bool CheckAction_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs, UModularControllerComponent* controller, const float inDelta);


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	
	/// <summary>
	/// Process action and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
	FVelocity OnActionProcess(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);


	UFUNCTION(BlueprintCallable, Category = "Action|Base Events|C++ Implementation")
	virtual FVelocity OnActionProcess_Implementation(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput
		, UModularControllerComponent* controller, const float inDelta);
	

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	

	/// <summary>
	/// Notify actions when a state change. whether the action is active or not.
	/// </summary>
	/// <returns></returns>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
	void OnStateChanged(UBaseControllerState* newState, UBaseControllerState* oldState);


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// Is this Action been active in the controller the last frame?
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "State|Basic Events")
	bool GetActivatedLastFrame();


	/// <summary>
	/// Is this Action been active in the controller the last frame?
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "State|Basic Events")
	void SetActivatedLastFrame(bool value);


	/// <summary>
	/// Get the time left to the action to still be active.
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "State|Basic Events")
	double GetRemainingActivationTime();

	/// <summary>
	/// Get the time left to the action to still be cooling down.
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "State|Basic Events")
	double GetRemainingCoolDownTime();


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// Debug
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "Action|Base Debug")
	virtual FString DebugString();
	

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// The priority of this state
	/// </summary>
	UFUNCTION(BlueprintNativeEvent)
	int GetPriority();

	/// <summary>
	/// The description of the particularity this Action is for, if any. it can be used to let say "Jump" to specify that this action is used for
	/// jumping
	/// </summary>
	UFUNCTION(BlueprintNativeEvent)
	FName GetDescriptionName();


	/// <summary>
	/// Save a snap shot of the action.
	/// </summary>
	void SaveActionSnapShot();

	/// <summary>
	/// Restore an action from it's previous snapShot if exist.
	/// </summary>
	void RestoreActionFromSnapShot();



	/// <summary>
	/// The priority of this state
	/// </summary>
	virtual int GetPriority_Implementation();

	/// <summary>
	/// The description of the particalurity this behaviour is for, if any. it can be used to let say "OnGround" to specify that this behaviour is used for
	/// Ground movements and reactions
	/// </summary>
	virtual FName GetDescriptionName_Implementation();



	UFUNCTION(BlueprintCallable, Category = "Action|Base Events|C++ Implementation")
	virtual void OnStateChanged_Implementation(UBaseControllerState* newState, UBaseControllerState* oldState);


	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





	/// <summary>
	/// When we enters the action behaviour.
	/// </summary>
	void OnActionBegins_Internal(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When we exit the action.
	/// </summary>
	void OnActionEnds_Internal(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// Check if the action is Valid
	/// </summary>
	bool CheckAction_Internal(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// Process action and return velocity.
	/// </summary>
	FVelocity OnActionProcess_Internal(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);



	///Check if this is running as a part of a simulation
	UFUNCTION(BlueprintCallable)
	FORCEINLINE bool IsSimulated() const { return _snapShotSaved; }

protected:

	double _cooldownTimer;

	double _cooldownTimer_saved;

	double _remainingActivationTimer;

	double _remainingActivationTimer_saved;

	int _repeatCount;

	int _repeatCount_saved;

	bool _wasActiveFrame;

	bool _wasActiveFrame_saved;

	bool _snapShotSaved;


	virtual void SaveActionSnapShot_Internal();

	virtual void RestoreActionFromSnapShot_Internal();

};
