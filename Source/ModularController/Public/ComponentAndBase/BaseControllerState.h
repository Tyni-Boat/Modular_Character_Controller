// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "Enums.h"
#include "Structs.h"
#ifndef MODULAR_CONTROLLER_COMPONENT
#define MODULAR_CONTROLLER_COMPONENT
#include "ModularControllerComponent.h"
#endif
#ifndef BASE_ACTION
#define BASE_ACTION
#include "BaseControllerAction.h"
#endif
#include "Engine/DataAsset.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include <Kismet/KismetSystemLibrary.h>

#include "BaseControllerState.generated.h"


class UModularControllerComponent;


///<summary>
/// The abstract basic state behaviour for a Modular controller.
/// </summary>
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular State Behaviours", abstract)
class MODULARCONTROLLER_API UBaseControllerState : public UDataAsset
{
	GENERATED_BODY()

public:

	// The linked animation blueprint class that will be used while this state is active.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "State|Basic Parameters")
	TSubclassOf<UAnimInstance> StateBlueprintClass;

	// The State's Root motion Mode
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "State|Basic Parameters")
	TEnumAsByte<ERootMotionType> RootMotionMode;

	// The informations on the current surface. This is used to track one surface movements
	UPROPERTY(BlueprintReadOnly, category = "State|Basic Parameters")
	FSurfaceInfos SurfaceInfos;

	// The state's flag, often used as binary. to relay this State's state over the network.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "State|Basic Parameters")
	int StateFlag;

	// Enable or disable debug for this state
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "State|Basic Parameters")
	bool bDebugState;






	/// <summary>
	/// When we enters the state.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
	void OnEnterState(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When we exit the state.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
	void OnExitState(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// Check if the state is Valid
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
	bool CheckState(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs, UModularControllerComponent* controller, const float inDelta
		, int overrideWasLastStateStatus = -1);


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// Process state and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
	FVelocity ProcessState(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);



	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	
	/// <summary>
	/// When the controller change a State, it call this function to notify all of it's states the change
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
	void OnControllerStateChanged(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller);

	/// <summary>
	/// Get Notify actions the active action change. whether the action is active or not.
	/// </summary>
	/// <returns></returns>
	UFUNCTION(BlueprintNativeEvent, Category = "Action|Base Events")
	void OnActionChanged(UBaseControllerAction* newAction, UBaseControllerAction* lastAction);


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// Is this behaviour been used by the controller the last frame?
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "State|Basic Events")
	bool GetWasTheLastFrameControllerState();


	/// <summary>
	/// Is this behaviour been used by the controller the last frame?
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "State|Basic Events")
	void SetWasTheLastFrameControllerState(bool value);
	

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// Debug
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "State|Basic Debug")
	virtual FString DebugString();


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
	/// Save a snap shot of the state.
	/// </summary>
	void SaveStateSnapShot();

	/// <summary>
	/// Restore a state from it's previous snapShot if exist.
	/// </summary>
	void RestoreStateFromSnapShot();


	/// <summary>
	/// The priority of this state
	/// </summary>
	virtual int GetPriority_Implementation();

	/// <summary>
	/// The description of the particalurity this behaviour is for, if any. it can be used to let say "OnGround" to specify that this behaviour is used for
	/// Ground movements and reactions
	/// </summary>
	virtual FName GetDescriptionName_Implementation();
	



	/// <summary>
	/// When we enters the state.
	/// </summary>

	UFUNCTION(BlueprintCallable, Category = "State|Base Events|C++ Implementation")
	virtual void OnEnterState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);

	/// <summary>
	/// When we exit the state.
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "State|Base Events|C++ Implementation")
	virtual void OnExitState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);



	/// <summary>
	/// Check if the state is Valid
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "State|Base Events|C++ Implementation")
	virtual bool CheckState_Implementation(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs, UModularControllerComponent* controller, const float inDelta
		, int overrideWasLastStateStatus = -1);


	/// <summary>
	/// Process state and return velocity.
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "State|Base Events|C++ Implementation")
	virtual FVelocity ProcessState_Implementation(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVector moveInput, UModularControllerComponent* controller, const float inDelta);




	/// <summary>
	/// When the controller change a behaviour, it call this function to notify nay of it's bahaviour the change
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "State|Base Events|C++ Implementation")
	virtual	void OnControllerStateChanged_Implementation(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller);


	///Check if this is running as a part of a simulation
	UFUNCTION(BlueprintCallable, Category = "State|Others")
	FORCEINLINE bool IsSimulated() { return _snapShotSaved; }

protected:

	bool _wasTheLastFrameBehaviour;

	bool _wasTheLastFrameBehaviour_saved;
	
	bool _snapShotSaved;
		
	
	virtual void SaveStateSnapShot_Internal();
	
	virtual void RestoreStateFromSnapShot_Internal();
};
