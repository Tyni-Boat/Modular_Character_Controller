// Copyright � 2023 by Tyni Boat. All Rights Reserved.

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
#include "CommonTypes.h"
#include "Engine/DataAsset.h"
#include "Kismet/KismetSystemLibrary.h"
#include "BaseControllerAction.generated.h"



///<summary>
/// The abstract basic Action behaviour for a Modular controller.
/// </summary>
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular Action Behaviours", abstract)
class MODULARCONTROLLER_API UBaseControllerAction : public UObject
{
	GENERATED_BODY()

public:

	// The State's unique name
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base")
	FName ActionName = "[Set Action Unique Name]";

	// The State's priority.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base")
	int ActionPriority = 0;

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
	/// When we enters the action behaviour. Return the action timings Vector: X=Anticipation duration, Y=Active phase duration, Z=Recovery Phase duration
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Events")
	FVector OnActionBegins(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const;


	/// <summary>
	/// When we exit the action.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Events")
	void OnActionEnds(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const;


	/// <summary>
	/// Check if the action is Valid
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Events")
	FControllerCheckResult CheckAction(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta, bool asLastActiveAction) const;



	/// <summary>
	/// Process action's anticipation phase and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Events")
	FControllerStatus OnActionProcessAnticipationPhase(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const;


	/// <summary>
	/// Process action's active phase and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Events")
	FControllerStatus OnActionProcessActivePhase(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const;


	/// <summary>
	/// Process action's recovery phase and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Events")
	FControllerStatus OnActionProcessRecoveryPhase(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const;



	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	UFUNCTION(BlueprintCallable, Category = "Action|Events|C++ Implementation")
	virtual FVector OnActionBegins_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const;


	UFUNCTION(BlueprintCallable, Category = "Action|Events|C++ Implementation")
	virtual void OnActionEnds_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const;


	UFUNCTION(BlueprintCallable, Category = "Action|Events|C++ Implementation")
	virtual FControllerCheckResult CheckAction_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta, bool asLastActiveAction) const;



	UFUNCTION(BlueprintCallable, Category = "Action|Events|C++ Implementation")
	virtual FControllerStatus OnActionProcessAnticipationPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const;


	UFUNCTION(BlueprintCallable, Category = "Action|Events|C++ Implementation")
	virtual FControllerStatus OnActionProcessActivePhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const;


	UFUNCTION(BlueprintCallable, Category = "Action|Events|C++ Implementation")
	virtual FControllerStatus OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const;


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
	/// The description of the particularity this Action is for, if any. it can be used to let say "JumpTo" to specify that this action is used for
	/// jumping
	/// </summary>
	UFUNCTION(BlueprintGetter)
	FORCEINLINE FName GetDescriptionName() const { return  ActionName; }



	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	//Remap the durations
	UFUNCTION(BlueprintCallable, Category = "Action|Timing")
	FVector RemapDuration(float duration, bool tryDontMapAnticipation = false, bool tryDontMapRecovery = false) const;

};
