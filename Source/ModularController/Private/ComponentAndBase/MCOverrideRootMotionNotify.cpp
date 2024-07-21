// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/MCOverrideRootMotionNotify.h"

#include "FunctionLibrary.h"


void UMCOverrideRootMotionNotify::EvaluateMotionWarping(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference,
                                                   UModularControllerComponent* Controller)
{
	if(!Controller)
		return;
	durationRange.X = EventReference.GetNotify()->GetTriggerTime();
	durationRange.Y = durationRange.X + TotalDuration;
	durationRange.Z = durationRange.X;
	// StartTransform.SetComponents(MeshComp->GetOwner()->GetActorQuat(), MeshComp->GetOwner()->GetActorLocation(), FVector(1));
	StartTransform.SetComponents(Controller->ApplyedControllerStatus.Kinematics.AngularKinematic.Orientation
		, Controller->ApplyedControllerStatus.Kinematics.LinearKinematic.Position, FVector(1));
	const FTransform localSpaceRootMotion = UFunctionLibrary::ExtractRootMotionFromAnimation(Animation, durationRange.X, durationRange.Y);
	FTransform worldSpaceRootMotion = MeshComp->ConvertLocalRootMotionToWorld(localSpaceRootMotion);
	worldSpaceRootMotion.SetLocation(StartTransform.GetLocation() + worldSpaceRootMotion.GetTranslation());
	worldSpaceRootMotion.SetRotation(StartTransform.GetRotation() * worldSpaceRootMotion.GetRotation());
	if(ControllerComponent->TryGetMotionWarpTransform(WarpKey, worldSpaceRootMotion))
		bMustWarp = true;
	EndTransform = worldSpaceRootMotion;
}


void UMCOverrideRootMotionNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	bMustWarp = false;
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp)
		return;
	if (!MeshComp->GetOwner())
		return;
	ControllerComponent = MeshComp->GetOwner()->GetComponentByClass<UModularControllerComponent>();
	EvaluateMotionWarping(MeshComp, Animation, TotalDuration, EventReference, ControllerComponent);
}


void UMCOverrideRootMotionNotify::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	if (!MeshComp)
		return;
	if (!ControllerComponent)
		return;
	const float normalizedTime = (durationRange.Z - durationRange.X) / (durationRange.Y - durationRange.X);
	const float blendAlpha = FAlphaBlend::AlphaToBlendOption(normalizedTime, WarpCurve);
	const FTransform matchTransform = FTransform(FQuat::Slerp(StartTransform.GetRotation(), EndTransform.GetRotation(), blendAlpha),
	                                             FMath::Lerp(StartTransform.GetLocation(), EndTransform.GetLocation(), blendAlpha),
	                                             FVector(1));
	FOverrideRootMotionCommand cloneParams = OverrideParameters;
	cloneParams.WarpTransform = matchTransform;
	cloneParams.bIsMotionWarping = bMustWarp;
	ControllerComponent->SetOverrideRootMotion(MeshComp, cloneParams);
	durationRange.Z += FrameDeltaTime;
	durationRange.Z = FMath::Clamp(durationRange.Z, durationRange.X, durationRange.Y);
}


void UMCOverrideRootMotionNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (!MeshComp)
		return;
	if (!ControllerComponent)
		return;
	ControllerComponent->RemoveMotionWarp(WarpKey);
}
