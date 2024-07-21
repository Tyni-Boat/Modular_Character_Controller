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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Root Motion Parameters")
	FOverrideRootMotionCommand OverrideParameters;

	// Wrap transform key
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Root Motion Parameters")
	FName WarpKey;

	// Wrap transform curve.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Root Motion Parameters")
	EAlphaBlendOption WarpCurve;

private:
	UPROPERTY()
	UModularControllerComponent* ControllerComponent;

	UPROPERTY()
	FVector durationRange = FVector(0);

	UPROPERTY()
	FTransform StartTransform = FTransform::Identity;

	UPROPERTY()
	FTransform EndTransform = FTransform::Identity;

	UPROPERTY()
	bool bMustWarp = false;


	void EvaluateMotionWarping(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference,
	                      UModularControllerComponent* Controller);


public:
	void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
