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

	void InitializeAction();
	
	// The action actual phase.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Action|Base|Timing|Phasing")
	TEnumAsByte<EActionPhase> CurrentPhase;

	// The action anticipation phase duration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|Timing|Phasing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AnticipationPhaseDuration = 0;

	// The action active phase duration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|Timing|Phasing", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float ActivePhaseDuration = 0.15f;

	// The action recovery phase duration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|Timing|Phasing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RecoveryPhaseDuration = 0;

	// The action can transition to self on recovery phase?
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|Timing|Phasing")
	bool bCanTransitionToSelf;



	// The action cool down delay. the duration the action cannot be done again.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|Timing")
	float CoolDownDelay = 0.25f;

	// The action's Root motion Mode.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|Motion")
	TEnumAsByte<ERootMotionType> RootMotionMode;

	// The action's flag, often used as binary. to relay this actions's state over the network.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Action|Base|Motion")
	int ActionFlag;

	// The current controller state must be frozen while this action is running?
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|State & Compatibility")
	bool bFreezeCurrentState;

	// Special tag that affect how the current controller state check phase will behave.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|State & Compatibility")
	bool bShouldControllerStateCheckOverride;

	// The Action only execute modes.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|State & Compatibility")
	TEnumAsByte<EActionCompatibilityMode> ActionCompatibilityMode;

	// The list of compatible states names.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|State & Compatibility"
		, meta = (EditCondition = "ActionCompatibilityMode == EActionCompatibilityMode::ActionCompatibilityMode_OnCompatibleStateOnly || ActionCompatibilityMode == EActionCompatibilityMode::ActionCompatibilityMode_OnBothCompatiblesStateAndAction"))
	TArray<FName> CompatibleStates;

	// The list of compatible actions names
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base|State & Compatibility"
		, meta = (EditCondition = "ActionCompatibilityMode == EActionCompatibilityMode::ActionCompatibilityMode_WhileCompatibleActionOnly || ActionCompatibilityMode == EActionCompatibilityMode::ActionCompatibilityMode_OnBothCompatiblesStateAndAction"))
	TArray<FName> CompatibleActions;

	// Enable or disable debug for this action
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
	/// Process action's anticipation phase and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
	FVelocity OnActionProcessAnticipationPhase(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);


	UFUNCTION(BlueprintCallable, Category = "Action|Base Events|C++ Implementation")
	virtual FVelocity OnActionProcessAnticipationPhase_Implementation(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput
		, UModularControllerComponent* controller, const float inDelta);


	/// <summary>
	/// Process action's active phase and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
	FVelocity OnActionProcessActivePhase(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);


	UFUNCTION(BlueprintCallable, Category = "Action|Base Events|C++ Implementation")
	virtual FVelocity OnActionProcessActivePhase_Implementation(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput
		, UModularControllerComponent* controller, const float inDelta);


	/// <summary>
	/// Process action's recovery phase and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
	FVelocity OnActionProcessRecoveryPhase(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);


	UFUNCTION(BlueprintCallable, Category = "Action|Base Events|C++ Implementation")
	virtual FVelocity OnActionProcessRecoveryPhase_Implementation(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput
		, UModularControllerComponent* controller, const float inDelta);


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// Get Notify when a state change. whether the action is active or not.
	/// </summary>
	/// <returns></returns>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
	void OnStateChanged(UBaseControllerState* newState, UBaseControllerState* oldState);

	/// <summary>
	/// Get Notify actions the active action change. whether the action is active or not.
	/// </summary>
	/// <returns></returns>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
	void OnActionChanged(UBaseControllerAction* newAction, UBaseControllerAction* lastAction);


	// Called when the action phase changed while not simulated.
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
	void OnActionPhaseChanged(EActionPhase newPhase, EActionPhase lastPhase);


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
	UFUNCTION(BlueprintCallable, Category = "Action|Others")
	FORCEINLINE bool IsSimulated() const { return _snapShotSaved; }

	//Remap the durations
	UFUNCTION(BlueprintCallable, Category = "Action|Others")
	FORCEINLINE void RemapDuration(float duration, bool tryDontMapAnticipation = false, bool tryDontMapRecovery = false)
	{
		const float anticipationScale = _startingDurations.X / (_startingDurations.X + _startingDurations.Y + _startingDurations.Z);
		const float recoveryScale = _startingDurations.Z / (_startingDurations.X + _startingDurations.Y + _startingDurations.Z);
		//Anticipation
		float newAnticipation = duration * anticipationScale;
		if (tryDontMapAnticipation)
		{
			newAnticipation = FMath::Clamp(_startingDurations.X, 0, FMath::Clamp((duration * 0.5f) - 0.05, 0, TNumericLimits<float>().Max()));
		}

		//Recovery
		float newRecovery = duration * recoveryScale;
		if (tryDontMapRecovery)
		{
			newRecovery = FMath::Clamp(_startingDurations.Z, 0, FMath::Clamp((duration * 0.5f) - 0.05, 0, TNumericLimits<float>().Max()));
		}

		const float newDuration = duration - (newAnticipation + newRecovery);

		AnticipationPhaseDuration = newAnticipation;
		ActivePhaseDuration = newDuration;
		RecoveryPhaseDuration = newRecovery;


		if (bDebugAction)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Remap from (%s) to (%s)"), *_startingDurations.ToCompactString(), *FVector(AnticipationPhaseDuration, ActivePhaseDuration, RecoveryPhaseDuration).ToCompactString()), true, true, FColor::Orange, 5, "remapingDuration");
		}
	}

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

	FVector _startingDurations;


	virtual void SaveActionSnapShot_Internal();

	virtual void RestoreActionFromSnapShot_Internal();

};
