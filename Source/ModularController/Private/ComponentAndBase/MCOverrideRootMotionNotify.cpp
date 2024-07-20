// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/MCOverrideRootMotionNotify.h"


void UMCOverrideRootMotionNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration);
	if (!MeshComp)
		return;
	ControllerComponent = MeshComp->GetOwner()->GetComponentByClass<UModularControllerComponent>();
}


void UMCOverrideRootMotionNotify::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	if (!MeshComp)
		return;
	if (!ControllerComponent)
		return;
	ControllerComponent->SetOverrideRootMotion(MeshComp, OverrideParameters);
}


void UMCOverrideRootMotionNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::NotifyEnd(MeshComp, Animation);
	if (!MeshComp)
		return;
	if (!ControllerComponent)
		return;
	ControllerComponent->SetOverrideRootMotion(MeshComp, FOverrideRootMotionCommand(ERootMotionType::NoRootMotion, ERootMotionType::NoRootMotion));
}
