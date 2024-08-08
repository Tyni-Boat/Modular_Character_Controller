// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/MCOverrideRootMotionNotify.h"

#include "FunctionLibrary.h"


void UMCOverrideRootMotionNotify::EvaluateMotionWarping(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference,
                                                        UModularControllerComponent* Controller)
{
	if (!Controller)
		return;
	FOverrideRootMotionCommand customParams = OverrideParameters;
	customParams.PlayRate = Animation->RateScale;
	customParams.Duration = TotalDuration;
	customParams.WarpTransform_Path.Empty();
	const FTransform start = FTransform(Controller->ApplyedControllerStatus.Kinematics.AngularKinematic.Orientation
												   , Controller->ApplyedControllerStatus.Kinematics.LinearKinematic.Position, FVector(1));
	FTransform destination = start;
	if(Controller->TryGetMotionWarpTransform(customParams.WarpKey, destination))
	{
		TArray<FTransform> localStepArray;
		customParams.WarpTransform_Path.Add(start);
		const FTransform localSpaceRootMotion = UFunctionLibrary::ExtractRootMotionFromAnimation(Animation, EventReference.GetNotify()->GetTriggerTime(),
																								 EventReference.GetNotify()->GetEndTriggerTime(), &localStepArray);
		if(localStepArray.Num() > 0)
		{
			FTransform worldSpaceDestination = MeshComp->ConvertLocalRootMotionToWorld(localSpaceRootMotion);
			const FVector mainlocationOffset = destination.GetLocation() - worldSpaceDestination.GetLocation();
			const FQuat mainRotationOffset = worldSpaceDestination.GetRotation().Inverse() * destination.GetRotation();
			FVector rotDiffAxis; float rotDiffAngle;
			mainRotationOffset.ToAxisAndAngle(rotDiffAxis, rotDiffAngle);
			const FVector stepOffset = mainlocationOffset / localStepArray.Num();
			const FQuat stepRot = FQuat(rotDiffAxis, rotDiffAngle / localStepArray.Num());
			for(int i = 0; i < localStepArray.Num(); i++)
			{
				FTransform WS_step = MeshComp->ConvertLocalRootMotionToWorld(localStepArray[i]);
				WS_step.SetLocation(WS_step.GetLocation() + stepOffset);
				WS_step.SetRotation(WS_step.GetRotation() * stepRot);
				customParams.WarpTransform_Path.Add(WS_step);
			}
		}
	}
	Controller->SetOverrideRootMotion(customParams, IgnoreCollision);
}


void UMCOverrideRootMotionNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp)
		return;
	if (!MeshComp->GetOwner())
		return;
	const auto ControllerComponent = MeshComp->GetOwner()->GetComponentByClass<UModularControllerComponent>();
	EvaluateMotionWarping(MeshComp, Animation, TotalDuration, EventReference, ControllerComponent);
}
