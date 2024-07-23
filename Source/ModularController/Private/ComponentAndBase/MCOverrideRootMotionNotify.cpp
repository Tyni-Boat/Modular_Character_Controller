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
	customParams.WarpTransform_Start.SetComponents(Controller->ApplyedControllerStatus.Kinematics.AngularKinematic.Orientation
	                             , Controller->ApplyedControllerStatus.Kinematics.LinearKinematic.Position, FVector(1));
	const FTransform localSpaceRootMotion = UFunctionLibrary::ExtractRootMotionFromAnimation(Animation, EventReference.GetNotify()->GetTriggerTime(), EventReference.GetNotify()->GetEndTriggerTime());
	FTransform worldSpaceRootMotion = MeshComp->ConvertLocalRootMotionToWorld(localSpaceRootMotion);
	worldSpaceRootMotion.SetLocation(customParams.WarpTransform_Start.GetLocation() + worldSpaceRootMotion.GetTranslation());
	worldSpaceRootMotion.SetRotation(customParams.WarpTransform_Start.GetRotation() * worldSpaceRootMotion.GetRotation());
	Controller->TryGetMotionWarpTransform(customParams.WarpKey, worldSpaceRootMotion);
	customParams.WarpTransform_End = worldSpaceRootMotion;
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

