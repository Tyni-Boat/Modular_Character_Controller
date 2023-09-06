// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/BaseAction.h"
#include "Animation/AnimMontage.h"
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ComponentAndBase/BaseState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"


int UBaseAction::GetPriority_Implementation()
{
	return 0;
}

FName UBaseAction::GetDescriptionName_Implementation()
{
	return "";
}

void UBaseAction::ActionIdle_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{

}

bool UBaseAction::CheckAction_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	return false;
}

void UBaseAction::OnActionBegins_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
}

FVelocity UBaseAction::OnActionProcess_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	return inDatas.InitialVelocities;
}

void UBaseAction::OnActionPostProcess_Implementation(FVelocity& inVelocity, const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{

}

void UBaseAction::OnActionEnds_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
}

void UBaseAction::OnActionRepeat_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
}

bool UBaseAction::CheckCanRepeat_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	return false;
}

void UBaseAction::OnStateChanged_Implementation(UBaseState* newState, UBaseState* oldState)
{

}


TSoftObjectPtr<UAnimInstance> UBaseAction::OnEnterInner(UModularControllerComponent* controller, bool& opDone, bool asSimulation)
{
	if (controller == nullptr)
		return nullptr;

	if (Montage.Montage != nullptr)
	{
		const USkeletalMeshComponent* mesh = controller->GetSkeletalMesh();
		if (mesh == nullptr)
			return nullptr;

		if (ShouldPlayOnStateAnimGraph)
		{
			const UBaseState* state = controller->GetCurrentStateBehaviour();
			if (state == nullptr)
				return nullptr;
			if (state->StateBlueprintClass == nullptr)
				return nullptr;
			UAnimInstance* animInstance = mesh->GetLinkedAnimLayerInstanceByClass(state->StateBlueprintClass);
			OnEnterInner_PartTwo(animInstance, opDone, asSimulation);
			return  animInstance;
		}
		else
		{
			UAnimInstance* animInstance = mesh->GetAnimInstance();
			OnEnterInner_PartTwo(animInstance, opDone, asSimulation);
			return  animInstance;
		}

	}

	_actionTimer = Duration;
	opDone = true;
	isActionActive = true;
	return nullptr;
}


void UBaseAction::OnEnterInner_PartTwo(UAnimInstance* animInstance, bool& success, bool asSimulation)
{
	if (animInstance == nullptr)
		return;
	const float duration = animInstance->Montage_Play(Montage.Montage.Get(), 1, EMontagePlayReturnType::Duration);
	if (duration <= 0)
		return;
	if (!Montage.MontageSection.IsNone())
	{
		//Jumps to a section
		animInstance->Montage_JumpToSection(Montage.MontageSection, Montage.Montage.Get());
	}

	_actionTimer = duration;
	success = true;
	isActionActive = true;
	if (asSimulation)
		animInstance->Montage_Stop(0, Montage.Montage.Get());
}


void UBaseAction::OnExitInner(bool disposeLater)
{
	_CollDownTimer = CoolDownDelay;
	_actionTimer = 0;
	isActionActive = false;
	isWaitingDisposal = disposeLater;
}


void UBaseAction::ActiveActionUpdate(float inDelta)
{
	if (Montage.Montage == nullptr && _actionTimer > 0)
	{
		_actionTimer -= inDelta;
	}
}


void UBaseAction::PassiveActionUpdate(float inDelta)
{
	if (_CollDownTimer > 0)
	{
		_CollDownTimer -= inDelta;
	}
}


bool UBaseAction::IsActionCompleted(bool asSimulation) const
{
	if (Montage.Montage != nullptr && _actionTimer > 0 && !asSimulation)
		return false;
	return _actionTimer <= 0;
}


bool UBaseAction::IsActionCoolingDown() const
{
	return _CollDownTimer > 0;
}


bool UBaseAction::IsActive() const
{
	return isActionActive || isWaitingDisposal;
}


FString UBaseAction::DebugString()
{
	return GetDescriptionName().ToString();
}


void UBaseAction::DebugArrow(AActor* owner, FVector start, FVector end, FColor color, float arrowSize, float width)
{
	if (owner == nullptr)
		return;
	UKismetSystemLibrary::DrawDebugArrow(owner, start, end, arrowSize, color, 0, width);
}

void UBaseAction::DebugPoint(AActor* owner, FVector point, FColor color, float size)
{
	if (owner == nullptr)
		return;
	UKismetSystemLibrary::DrawDebugPoint(owner, point, size, color, 0);
}

FHitResult UBaseAction::TraceSphere(AActor* owner, FVector start, FVector end, ETraceTypeQuery channel, float width, EDrawDebugTrace::Type debugType)
{
	if (owner == nullptr)
		return FHitResult();
	TArray<AActor*> ignore;
	ignore.Add(owner);
	FHitResult result;
	UKismetSystemLibrary::SphereTraceSingle(owner, start, end, width, channel, true, ignore, debugType, result, true);
	return result;
}
