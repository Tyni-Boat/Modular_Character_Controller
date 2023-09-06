// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "Enums.h"
#include "Structs.h"
#ifndef MODULAR_CONTROLLER_COMPONENT
#define MODULAR_CONTROLLER_COMPONENT
#include "ModularControllerComponent.h"
#endif
#include "Engine/DataAsset.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include <Kismet/KismetSystemLibrary.h>

#include "BaseState.generated.h"


///<summary>
/// The abstract basic state behaviour for a Modular controller.
/// </summary>
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular State Behaviours", abstract)
class MODULARCONTROLLER_API UBaseState : public UDataAsset
{
	GENERATED_BODY()

public:

	/// <summary>
	/// The linked animation blueprint class
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "State|Basic Parameters")
		TSubclassOf<UAnimInstance> StateBlueprintClass;

	/// <summary>
	/// The behaviour Root motion Mode
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "State|Basic Parameters")
		TEnumAsByte<ERootMotionType> RootMotionMode;

	/// <summary>
	/// This is used to track one surface movements
	/// </summary>
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, category = "State|Basic Parameters")
		FSurfaceInfos SurfaceInfos;

	/// <summary>
	/// The state's flag, often used as binary. to relay this behaviour's state over the network. Can be used for things like behaviour phase.
	/// </summary>
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, category = "State|Basic Parameters")
		int StateFlag;




	/// <summary>
	/// The state idle function to keep certain things running
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
		void StateIdle(UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// Check if the state is Valid
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
		bool CheckState(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When we enters the state.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
		void OnEnterState(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// Process state and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
		FVelocity ProcessState(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// Post Process state and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
		void PostProcessState(FVelocity& inVelocity, const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When we exit the state.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
		void OnExitState(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When the controller change a behaviour, it call this function to notify nay of it's bahaviour the change
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
		void OnBehaviourChanged(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller);

	/// <summary>
	/// Return a copy of this state with discoupled references as much as possible.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
		void CloneState(UBaseState* other);

	/// <summary>
	/// Reset a module state
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
		void Reset();

	/// <summary>
	/// Evaluate behaviour's state base from a flag
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
		void ComputeFromFlag(int flag);


	/// <summary>
	/// Is this behaviour been used by the controller the last frame?
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "State|Basic Events")
		bool GetWasTheLastFrameBehaviour();


	/// <summary>
	/// Is this behaviour been used by the controller the last frame?
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "State|Basic Events")
		void SetWasTheLastFrameBehaviour(bool value);


	/// <summary>
	/// Debug
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "State|Basic Debug")
		virtual FString DebugString();

	/// <summary>
	/// Trace a Debug arrow
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "State|Basic Debug")
		void DebugArrow(AActor* owner, FVector start, FVector end, FColor color = FColor::White, float arrowSize = 50, float width = 1);

	/// <summary>
	/// Draw a Debug point
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "State|Basic Debug")
		void DebugPoint(AActor* owner, FVector point, FColor color = FColor::White, float size = 50);




	/// <summary>
	/// Trace sphere check
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "State|Physic")
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
	UFUNCTION(BlueprintCallable, category = "State|Basic Events")
		virtual int GetPriority_Implementation();

	/// <summary>
	/// The description of the particalurity this behaviour is for, if any. it can be used to let say "OnGround" to specify that this behaviour is used for
	/// Ground movements and reactions
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "State|Basic Events")
		virtual FName GetDescriptionName_Implementation();



	/// <summary>
	/// The state idle function to keep certain things running
	/// </summary>
	virtual void StateIdle_Implementation(UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// Check if the state is Valid
	/// </summary>
	virtual bool CheckState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When we enters the state.
	/// </summary>
	virtual void OnEnterState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// Process state and return velocity.
	/// </summary>
	virtual FVelocity ProcessState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);


	/// <summary>
	/// Post Process state and return velocity.
	/// </summary>
	virtual void PostProcessState_Implementation(FVelocity& inVelocity, const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When we exit the state.
	/// </summary>
	virtual void OnExitState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When the controller change a behaviour, it call this function to notify nay of it's bahaviour the change
	/// </summary>
	virtual	void OnBehaviourChanged_Implementation(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller);

	/// <summary>
	/// Return a copy of this state with discoupled references as much as possible.
	/// </summary>
	virtual void CloneState_Implementation(UBaseState* other);

	/// <summary>
	/// Reset a module state
	/// </summary>
	virtual void Reset_Implementation();

	/// <summary>
	/// Evaluate behaviour's state base from a flag
	/// </summary>
	virtual  void ComputeFromFlag_Implementation(int flag);

protected:

	bool _wasTheLastFrameBehaviour;

};
