// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "Enums.h"
#include "CommonTypes.h"
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
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular Controller States", abstract)
class MODULARCONTROLLER_API UBaseControllerState : public UObject
{
	GENERATED_BODY()

public:

	// The State's unique name
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base")
	FName StateName = "[Set State Unique Name]";

	// The State's priority.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base")
	int StatePriority = 0;


	// The linked animation blueprint class that will be used while this state is active.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Base|Basic State Parameters")
	TSubclassOf<UAnimInstance> StateBlueprintClass;


	// The state's flag, often used as binary. to relay this State's state over the network.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Base|Basic State Parameters")
	int StateFlag;

	// Enable or disable debug for this state
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Base|Basic State Parameters")
	bool bDebugState;






	/// <summary>
	/// When we enters the state.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
	void OnEnterState(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const;

	/// <summary>
	/// When we exit the state.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
	void OnExitState(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const;


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// Check if the state is Valid
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
	FControllerCheckResult CheckState(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta, bool asLastActiveState) const;


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// Process state and return velocity.
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, category = "State|Basic Events")
	FControllerStatus ProcessState(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta) const;

	

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// Debug
	/// </summary>
	UFUNCTION(BlueprintCallable, category = "State|Basic Debug")
	virtual FString DebugString() const;


	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// <summary>
	/// The priority of this state
	/// </summary>
	UFUNCTION(BlueprintGetter)
	FORCEINLINE int GetPriority() const { return  StatePriority; }

	/// <summary>
	/// The description of the particalurity this behaviour is for, if any. it can be used to let say "OnGround" to specify that this behaviour is used for
	/// Ground movements and reactions
	/// </summary>
	UFUNCTION(BlueprintGetter)
	FORCEINLINE FName GetDescriptionName() const { return  StateName; };

	


	/// <summary>
	/// When we enters the state.
	/// </summary>

	UFUNCTION(BlueprintCallable, Category = "State|Base Events|C++ Implementation")
	virtual void OnEnterState_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const;

	/// <summary>
	/// When we exit the state.
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "State|Base Events|C++ Implementation")
	virtual void OnExitState_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const;



	/// <summary>
	/// Check if the state is Valid
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "State|Base Events|C++ Implementation")
	virtual FControllerCheckResult CheckState_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float inDelta, bool asLastActiveState = false) const;


	/// <summary>
	/// Process state and return velocity.
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "State|Base Events|C++ Implementation")
	virtual FControllerStatus ProcessState_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta) const;
		
};
