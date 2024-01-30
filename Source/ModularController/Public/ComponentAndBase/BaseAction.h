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
#include "BaseAction.generated.h"



///<summary>
/// The abstract basic Action behaviour for a Modular controller.
/// </summary>
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular Action Behaviours", abstract)
class MODULARCONTROLLER_API UBaseAction : public UDataAsset
{
	GENERATED_BODY()

public:

	/// <summary>
	/// The Animation Montages to play
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base Parameters")
		FActionMotionMontage Montage;

	/// <summary>
	/// The Montage should be played in the compatible state's limked anim graph?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base Parameters")
		bool ShouldPlayOnStateAnimGraph = true;

	/// <summary>
	/// The action duration
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base Parameters")
		float Duration = 0.15f;

	/// <summary>
	/// The action cool down delay. the duration the action cannot be done again.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base Parameters")
		float CoolDownDelay;

	/// <summary>
	/// The controller state must be frozen to it current state?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base Parameters")
		bool FreezeCurrentState;

	/// <summary>
	/// The behaviour Root motion Mode
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base Parameters")
		TEnumAsByte<ERootMotionType> RootMotionMode;

	/// <summary>
	/// The Action only execute on compatible state?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base Parameters")
		bool UseCompatibleStatesOnly = true;

	/// <summary>
	/// The list of compatible states
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base Parameters")
		TArray<FName> CompatibleStates;

	/// <summary>
	/// The list of compatible actions
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Action|Base Parameters")
		TArray<FName> CompatibleActions;




	/// <summary>
	/// The action idle function to keep certain things running
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
		void ActionIdle(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// Check if the action is Valid
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
		bool CheckAction(const FKinematicInfos& inDatas, FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When we enters the action behaviour.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
		void OnActionBegins(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// Process action and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
		FVelocity OnActionProcess(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// Post Process state and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
		void OnActionPostProcess(FVelocity& inVelocity, const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When we exit the action.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
		void OnActionEnds(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When we repeat the action.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
		void OnActionRepeat(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// Used to chech if the action can be repeated.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
		bool CheckCanRepeat(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);


	UFUNCTION(BlueprintGetter, Category="Action|Base Events")
	FORCEINLINE bool IsWaitingMontage() const
	{
		return _isWaitingMontage;
	}


	/// <summary>
	/// Notify actions when a state change. whether the action is active or not.
	/// </summary>
	/// <returns></returns>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
		void OnStateChanged(UBaseControllerState* newState, UBaseControllerState* oldState);


	/// <summary>
	/// Debug
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "Action|Base Debug")
		virtual FString DebugString();

	/// <summary>
	/// Trace a Debug arrow
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "Action|Basic Debug")
		void DebugArrow(AActor* owner, FVector start, FVector end, FColor color = FColor::White, float arrowSize = 50, float width = 1);

	/// <summary>
	/// Draw a Debug point
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "Action|Basic Debug")
		void DebugPoint(AActor* owner, FVector point, FColor color = FColor::White, float size = 50);



	/// <summary>
	/// Trace sphere check
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "Action|Physic")
		FHitResult TraceSphere(AActor* owner, FVector start, FVector end, ETraceTypeQuery channel, float width = 1, EDrawDebugTrace::Type debugType = EDrawDebugTrace::None);


	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// <summary>
	/// The priority of this state
	/// </summary>
	UFUNCTION(BlueprintNativeEvent)
		int GetPriority();

	/// <summary>
	/// The description of the particalurity this behaviour is for, if any. it can be used to let say "OnGround" to specify that this behaviour is used for
	/// Ground movements and reactions
	/// </summary>
	UFUNCTION(BlueprintNativeEvent)
		FName GetDescriptionName();



	/// <summary>
	/// The priority of this state
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "Action|Base Events")
		virtual int GetPriority_Implementation();

	/// <summary>
	/// The description of the particalurity this behaviour is for, if any. it can be used to let say "OnGround" to specify that this behaviour is used for
	/// Ground movements and reactions
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "Action|Base Events")
		virtual FName GetDescriptionName_Implementation();



	/// <summary>
	/// The action idle function to keep certain things running
	/// </summary>
	virtual	void ActionIdle_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// Check if the action is Valid
	/// </summary>
	virtual	bool CheckAction_Implementation(const FKinematicInfos& inDatas, FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When we enters the action behaviour.
	/// </summary>
	virtual	void OnActionBegins_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// Process action and return velocity.
	/// </summary>
	virtual	FVelocity OnActionProcess_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// Post Process state and return velocity.
	/// </summary>
	virtual	void OnActionPostProcess_Implementation(FVelocity& inVelocity, const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When we exit the action.
	/// </summary>
	virtual	void OnActionEnds_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When we repeat the action.
	/// </summary>
	virtual	void OnActionRepeat_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When the controller change a behaviour, it call this function to notify nay of it's behaviour the change
	/// </summary>
	virtual	bool CheckCanRepeat_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// Notify the action when a state changes.
	/// </summary>
	virtual void OnStateChanged_Implementation(UBaseControllerState* newState, UBaseControllerState* oldState);

	/// <summary>
	/// Initialize action values and play montage
	/// </summary>
	TSoftObjectPtr<UAnimInstance> OnEnterInner(UModularControllerComponent* controller, bool& opDone, bool asSimulation = false);

	/// <summary>
	/// The second part of enter inner initialisation process
	/// </summary>
	/// <param name="inDelta"></param>
	void OnEnterInner_PartTwo(UAnimInstance* animInstance, bool& success, bool asSimulation = false);

	/// <summary>
	/// Initialize cooldown values
	/// </summary>
	void OnExitInner(bool disposeLater = false);


	/// <summary>
	/// Update the cooldown timmer
	/// </summary>
	void ActiveActionUpdate(float inDelta);

	/// <summary>
	/// Update the cooldown timmer
	/// </summary>
	void PassiveActionUpdate(float inDelta);

	/// <summary>
	/// Check if the action is ended
	/// </summary>
	bool IsActionCompleted(bool asSimulation = false) const;

	/// <summary>
	/// Check if the cooldown is active
	/// </summary>
	bool IsActionCoolingDown() const;

	/// <summary>
	/// Check if the action is active
	/// </summary>
	bool IsActive() const;


	/// <summary>
	/// The end animation delegate.
	/// </summary>
	FOnMontageEnded _EndDelegate;

protected:

	float _CollDownTimer;

	float _actionTimer;

	bool isActionActive;

	bool isWaitingDisposal;

	bool _isWaitingMontage;

};
