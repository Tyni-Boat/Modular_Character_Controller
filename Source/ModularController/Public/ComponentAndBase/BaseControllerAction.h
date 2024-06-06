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

	// The State's unique name
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base")
	FName ActionName = "[Set Action Unique Name]";

	// The State's priority.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base")
	int ActionPriority = 0;

	
	// The action actual phase.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Base|Timing|Phasing")
	TEnumAsByte<EActionPhase> CurrentPhase;

	// The action anticipation phase duration
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base|Timing|Phasing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AnticipationPhaseDuration = 0;

	// The action active phase duration
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base|Timing|Phasing", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float ActivePhaseDuration = 0.15f;

	// The action recovery phase duration
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base|Timing|Phasing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RecoveryPhaseDuration = 0;

	// The action can transition to self on recovery phase?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base|Timing|Phasing")
	bool bCanTransitionToSelf;



	// The action cool down delay. the duration the action cannot be done again.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base|Timing")
	float CoolDownDelay = 0.25f;


	// The action's flag, often used as binary. to relay this actions's state over the network.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Base|Motion")
	int ActionFlag;

	// The current controller state must be frozen while this action is running?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base|State & Compatibility")
	bool bFreezeCurrentState;

	// Special tag that affect how the current controller state check phase will behave.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base|State & Compatibility")
	bool bShouldControllerStateCheckOverride;

	// The Action only execute modes.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base|State & Compatibility")
	TEnumAsByte<EActionCompatibilityMode> ActionCompatibilityMode;

	// The list of compatible states names.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base|State & Compatibility"
		, meta = (EditCondition = "ActionCompatibilityMode == EActionCompatibilityMode::ActionCompatibilityMode_OnCompatibleStateOnly || ActionCompatibilityMode == EActionCompatibilityMode::ActionCompatibilityMode_OnBothCompatiblesStateAndAction"))
	TArray<FName> CompatibleStates;

	// The list of compatible actions names
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base|State & Compatibility"
		, meta = (EditCondition = "ActionCompatibilityMode == EActionCompatibilityMode::ActionCompatibilityMode_WhileCompatibleActionOnly || ActionCompatibilityMode == EActionCompatibilityMode::ActionCompatibilityMode_OnBothCompatiblesStateAndAction"))
	TArray<FName> CompatibleActions;

	// Enable or disable debug for this action
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base|Debug")
	bool bDebugAction;





	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	


	/// <summary>
	/// When we enters the action behaviour.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Events")
	FKinematicComponents OnActionBegins(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta);

	
	/// <summary>
	/// When we exit the action.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Events")
	FKinematicComponents OnActionEnds(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta);


	/// <summary>
	/// Check if the action is Valid
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Events")
	FControllerCheckResult CheckAction(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta, bool asLastActiveAction);



	/// <summary>
	/// Process action's anticipation phase and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Events")
	FControllerStatus OnActionProcessAnticipationPhase(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta);


	/// <summary>
	/// Process action's active phase and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Events")
	FControllerStatus OnActionProcessActivePhase(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta);


	/// <summary>
	/// Process action's recovery phase and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Events")
	FControllerStatus OnActionProcessRecoveryPhase(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta);



	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	UFUNCTION(BlueprintCallable, Category = "Action|Events|C++ Implementation")
	virtual FKinematicComponents OnActionBegins_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta);


	UFUNCTION(BlueprintCallable, Category = "Action|Events|C++ Implementation")
	virtual FKinematicComponents OnActionEnds_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta);


	UFUNCTION(BlueprintCallable, Category = "Action|Events|C++ Implementation")
	virtual FControllerCheckResult CheckAction_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta, bool asLastActiveAction);
	


	UFUNCTION(BlueprintCallable, Category = "Action|Events|C++ Implementation")
	virtual FControllerStatus OnActionProcessAnticipationPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta);

	
	UFUNCTION(BlueprintCallable, Category = "Action|Events|C++ Implementation")
	virtual FControllerStatus OnActionProcessActivePhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta);


	UFUNCTION(BlueprintCallable, Category = "Action|Events|C++ Implementation")
	virtual FControllerStatus OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta);


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// When the controller change a State, it call this function to notify all of it's states the change
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "Action|Events")
	void OnControllerStateChanged(UModularControllerComponent* onController, FName newBehaviourDescName, int newPriority);

	/// <summary>
	/// Get Notify actions the active action change. whether the action is active or not.
	/// </summary>
	/// <returns></returns>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Events")
	void OnControllerActionChanged(UModularControllerComponent* onController, UBaseControllerAction* newAction, UBaseControllerAction* lastAction);


	// Called when the action phase changed while not simulated.
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Events")
	void OnActionPhaseChanged(EActionPhase newPhase, EActionPhase lastPhase);


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	

	/// <summary>
	/// Get the time left to the action to still be active.
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "Action|Timing")
	double GetRemainingActivationTime();

	/// <summary>
	/// Get the time left to the action to still be cooling down.
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "Action|Timing")
	double GetRemainingCoolDownTime();


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// Debug
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "Action|Debug")
	virtual FString DebugString();


	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// The priority of this state
	/// </summary>
	UFUNCTION(BlueprintGetter)
	FORCEINLINE int GetPriority() const { return ActionPriority; }

	/// <summary>
	/// The description of the particularity this Action is for, if any. it can be used to let say "Jump" to specify that this action is used for
	/// jumping
	/// </summary>
	UFUNCTION(BlueprintGetter)
	FORCEINLINE FName GetDescriptionName() const { return  ActionName; }


	/// <summary>
	/// Save a snap shot of the action.
	/// </summary>
	void SaveActionSnapShot();

	/// <summary>
	/// Restore an action from it's previous snapShot if exist.
	/// </summary>
	void RestoreActionFromSnapShot();
	

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





	/// <summary>
	/// When we enters the action behaviour.
	/// </summary>
	FKinematicComponents OnActionBegins_Internal(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta);

	/// <summary>
	/// When we exit the action.
	/// </summary>
	FKinematicComponents OnActionEnds_Internal(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta);

	/// <summary>
	/// Check if the action is Valid
	/// </summary>
	FControllerCheckResult CheckAction_Internal(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta, bool asLastActiveAction);

	/// <summary>
	/// Process action and return velocity.
	/// </summary>
	FControllerStatus OnActionProcess_Internal(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta);



	///Check if this is running as a part of a simulation
	UFUNCTION(BlueprintCallable, Category = "Action|Others")
	FORCEINLINE bool IsSimulated() const { return _snapShotSaved; }

	//Remap the durations
	UFUNCTION(BlueprintCallable, Category = "Action|Timing")
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

	bool _snapShotSaved;

	FVector _startingDurations;


	virtual void SaveActionSnapShot_Internal();

	virtual void RestoreActionFromSnapShot_Internal();

};
