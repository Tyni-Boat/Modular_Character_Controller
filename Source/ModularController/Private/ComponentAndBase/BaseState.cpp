// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/BaseState.h"
#include "Animation/AnimMontage.h"




int UBaseState::GetPriority_Implementation()
{
	return 0;
}

FName UBaseState::GetDescriptionName_Implementation()
{
	return "";
}

void UBaseState::StateIdle_Implementation(UModularControllerComponent* controller, const float inDelta)
{

}

bool UBaseState::CheckState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	return false;
}

void UBaseState::OnEnterState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
}

FVelocity UBaseState::ProcessState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	return FVelocity();
}

void UBaseState::PostProcessState_Implementation(FVelocity& inVelocity, const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{

}

void UBaseState::OnExitState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
}

void UBaseState::OnBehaviourChanged_Implementation(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller)
{
}


void UBaseState::CloneState_Implementation(UBaseState* other)
{
}


void UBaseState::Reset_Implementation()
{
}

void UBaseState::ComputeFromFlag_Implementation(int flag)
{
	StateFlag = flag;
}


FString UBaseState::DebugString()
{
	return GetDescriptionName().ToString();
}

void UBaseState::DebugArrow(AActor* owner, FVector start, FVector end, FColor color, float arrowSize, float width)
{
	if(owner == nullptr)
		return;
	UKismetSystemLibrary::DrawDebugArrow(owner, start, end, arrowSize, color, 0, width);
}

void UBaseState::DebugPoint(AActor* owner, FVector point, FColor color, float size)
{
	if (owner == nullptr)
		return;
	UKismetSystemLibrary::DrawDebugPoint(owner, point, size, color, 0);
}

FHitResult UBaseState::TraceSphere(AActor* owner, FVector start, FVector end, ETraceTypeQuery channel, float width, EDrawDebugTrace::Type debugType)
{
	if (owner == nullptr)
		return FHitResult();
	TArray<AActor*> ignore;
	ignore.Add(owner);
	FHitResult result;
	UKismetSystemLibrary::SphereTraceSingle(owner, start, end, width, channel, true, ignore, debugType, result, true);
	return result;
}

bool UBaseState::GetWasTheLastFrameBehaviour()
{
	return _wasTheLastFrameBehaviour;
}

void UBaseState::SetWasTheLastFrameBehaviour(bool value)
{
	_wasTheLastFrameBehaviour = value;
}