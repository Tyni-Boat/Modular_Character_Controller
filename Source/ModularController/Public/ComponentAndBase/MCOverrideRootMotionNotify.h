// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonTypes.h"
#include "ModularControllerComponent.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Override Rott Motion")
	FOverrideRootMotionCommand OverrideParameters;

private:

	UPROPERTY()
	UModularControllerComponent* ControllerComponent;


public:

	void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration) override;

	void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
	
};
