// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/BaseControllerAction.h"
#include "Animation/AnimMontage.h"
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ComponentAndBase/BaseControllerState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"



int UBaseControllerAction::GetPriority_Implementation()
{
	return 0;
}

FName UBaseControllerAction::GetDescriptionName_Implementation()
{
	return "";
}



void UBaseControllerAction::OnStateChanged_Implementation(UBaseControllerState* newState, UBaseControllerState* oldState)
{

}



void UBaseControllerAction::SaveActionSnapShot()
{
	if (_snapShotSaved)
		return;
	SaveActionSnapShot_Internal();
	_remainingActivationTimer_saved = _remainingActivationTimer;
	_repeatCount_saved = _repeatCount;
	_wasActiveFrame_saved = _wasActiveFrame;
	_cooldownTimer_saved = _cooldownTimer;
	_snapShotSaved = true;
}

void UBaseControllerAction::RestoreActionFromSnapShot()
{
	if (!_snapShotSaved)
		return;
	_remainingActivationTimer = _remainingActivationTimer_saved;
	_cooldownTimer = _cooldownTimer_saved;
	_repeatCount = _repeatCount_saved;
	_wasActiveFrame = _wasActiveFrame_saved;
	RestoreActionFromSnapShot_Internal();
	_snapShotSaved = false;
}

void UBaseControllerAction::SaveActionSnapShot_Internal()
{
}

void UBaseControllerAction::RestoreActionFromSnapShot_Internal()
{
}




bool UBaseControllerAction::GetActivatedLastFrame()
{
	return _wasActiveFrame;
}

void UBaseControllerAction::SetActivatedLastFrame(bool value)
{
	_wasActiveFrame = value;
}

double UBaseControllerAction::GetRemainingActivationTime()
{
	if (_remainingActivationTimer < 0)
		_remainingActivationTimer = 0;
	return _remainingActivationTimer;
}

double UBaseControllerAction::GetRemainingCoolDownTime()
{
	if (_cooldownTimer < 0)
		_cooldownTimer = 0;
	return _cooldownTimer;
}




void UBaseControllerAction::OnActionBegins_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
}

void UBaseControllerAction::OnActionEnds_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
}

bool UBaseControllerAction::CheckAction_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UInputEntryPool* inputs, UModularControllerComponent* controller, const float inDelta)
{
	return false;
}

FVelocity UBaseControllerAction::OnActionProcess_Implementation(FStatusParameters& controllerStatus,
	const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput, UModularControllerComponent* controller,
	const float inDelta)
{
	return {};
}



void UBaseControllerAction::OnActionBegins_Internal(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	//Set timers
	_remainingActivationTimer = Duration;
	_cooldownTimer = 0;

	OnActionBegins(inDatas, moveInput, controller, inDelta);
}

void UBaseControllerAction::OnActionEnds_Internal(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	//Reset timers
	_remainingActivationTimer = 0;
	_cooldownTimer = CoolDownDelay;

	OnActionEnds(inDatas, moveInput, controller, inDelta);
}

bool UBaseControllerAction::CheckAction_Internal(const FKinematicInfos& inDatas, const FVector moveInput,
	UInputEntryPool* inputs, UModularControllerComponent* controller, const float inDelta)
{
	//Update cooldown timer
	if (_cooldownTimer > 0)
	{
		_cooldownTimer -= inDelta;
		return false;
	}

	return CheckAction(inDatas, moveInput, inputs, controller, inDelta);
}

FVelocity UBaseControllerAction::OnActionProcess_Internal(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVelocity fromVelocity,
	const FVector moveInput, UModularControllerComponent* controller, const float inDelta)
{
	//Update activation timer
	if (_remainingActivationTimer > 0)
	{
		_remainingActivationTimer -= inDelta;
		return OnActionProcess(controllerStatus, inDatas, fromVelocity, moveInput, controller, inDelta);
	}

	return fromVelocity;
}




FString UBaseControllerAction::DebugString()
{
	return GetDescriptionName().ToString();
}

