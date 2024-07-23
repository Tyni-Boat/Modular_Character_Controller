// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonTypes.h"
#include "ModularControllerComponent.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimNotifyLibrary.h"
#include "MCOverrideRootMotionNotify.generated.h"

/**
 * Override root motion logic on Modular controller.
 */
UCLASS()
class MODULARCONTROLLER_API UMCOverrideRootMotionNotify : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// Override the root motion parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Root Motion Parameters")
	FOverrideRootMotionCommand OverrideParameters;
	
	// Should the controller collision be ignored during this time?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Root Motion Parameters")
	bool IgnoreCollision = false;

private:
	
	void EvaluateMotionWarping(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference,
	                      UModularControllerComponent* Controller);


public:
	void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
};
