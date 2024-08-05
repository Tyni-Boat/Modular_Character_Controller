// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonTypes.h"
#ifndef MODULAR_CONTROLLER_COMPONENT
#define MODULAR_CONTROLLER_COMPONENT
#include "ModularControllerComponent.h"
#endif
#include "BaseTraversalWatcher.generated.h"

/**
 * Traversal watcher, triggering event when condition meets
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular Traversal Watcher", abstract)
class MODULARCONTROLLER_API UBaseTraversalWatcher : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// The Watcher's unique name. Events will be triggerred with this name.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base")
	FName WatcherName = "[Set Watcher Unique Name]";

	// The State's priority.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base")
	int WatcherPriority = 0;

	//The map of named conditions to be checked, from first to last. the event will be trigger with the format "WatcherName_Key"
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Inputs")
	TMap<FName, FSurfaceCheckParams> TraversalMap;

	//If active, all traversal will be verified and triggered. by default, only the first Traversal checked true will be triggerred.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Inputs")
	bool bMultiTraversalTrigger = false;


	// The Action only execute modes.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base|State & Compatibility")
	EActionCompatibilityMode WatcherCompatibilityMode;

	// The list of compatible states names.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base|State & Compatibility"
		, meta = (EditCondition =
			"WatcherCompatibilityMode == EActionCompatibilityMode::OnCompatibleStateOnly || WatcherCompatibilityMode == EActionCompatibilityMode::OnBothCompatiblesStateAndAction"
		))
	TArray<FName> CompatibleStates;

	// The list of compatible actions names
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base|State & Compatibility"
		, meta = (EditCondition =
			"WatcherCompatibilityMode == EActionCompatibilityMode::WhileCompatibleActionOnly || WatcherCompatibilityMode == EActionCompatibilityMode::OnBothCompatiblesStateAndAction"
		))
	TArray<FName> CompatibleActions;

	// Enable or disable debug for this action
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Base|Debug")
	bool bDebugWatcher;


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// <summary>
	/// Check all Watcher's traversals
	/// </summary>
	bool CheckWatcher(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta, TMap<FName, TArray<bool>>* TraversalDebugMap = nullptr) const;

	/// <summary>
	/// Trigger a traversal event with a combined key "WatcherName_Key"
	/// </summary>
	UFUNCTION(BlueprintNativeEvent, Category = "Watcher|Events")
	void TriggerTraversalEvent(UModularControllerComponent* controller, const FControllerStatus startingConditions, FName combinedKey, FSurfaceCheckParams TraversalParam,
	                           FSurfaceCheckResponse response) const;


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	UFUNCTION(BlueprintCallable, Category = "Watcher|Events|C++ Implementation")
	void TriggerTraversalEvent_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FName combinedKey,
	                                                                 FSurfaceCheckParams TraversalParam, FSurfaceCheckResponse response) const;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// Debug
	/// </summary>
	UFUNCTION(BlueprintCallable, Category = "Watcher|Debug")
	virtual FString DebugString();


	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	/// <summary>
	/// The priority of this Watcher
	/// </summary>
	UFUNCTION(BlueprintGetter)
	FORCEINLINE int GetPriority() const { return WatcherPriority; }

	/// <summary>
	/// The description of the Watcher.
	/// </summary>
	UFUNCTION(BlueprintGetter)
	FORCEINLINE FName GetDescriptionName() const { return WatcherName; }
};
